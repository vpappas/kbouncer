#include <iostream>
#include <iomanip>
#include <fstream>
#include <stdlib.h>

#include "pin.H"
#include "portability.H"

using namespace std;

/*
 * The ID of the buffer
 */
BUFFER_ID bufId;

/*
 * The lock for I/O.
 */
PIN_LOCK fileLock;

/*
 * There is an isolation bug in the Pin windows support that prevents
 * the pin tool from opening files ina callback routine.  If a tool
 * does this, deadlock occurs. Instead, open one file in main, and
 * write the thread id along with the data.
 */
FILE *brf, *imgf, *funcf;

/*
 * Number of OS pages for the buffer
 */
#define NUM_BUF_PAGES	1

/*
 * Indirect branch instruction types
 */
#define RETURN		1
#define JUMP		2
#define CALL		3
#define SYSENTER	4

/*
 * Maximum length of an x86 instruction
 */
#define INSN_LEN	16

/*
 * Name of the file where the branch trace is stored
 */
#define TRACE_FILE	"branches.log"

/*
 * Name of the file where the loaded images are stored
 */
#define IMAGES_FILE	"images.log"

/*
 * Name of the file where the functions are stored
 */
#define FUNCTIONS_FILE	"functions.log"

/*
 * Full path of the windows ntdll.dll
 */
//FIXME: this only works for 64-bit .. fix it for wow64
#define NTDLL	"C:\\Windows\\System32\\ntdll.dll"

/*
 * Record of branches.
 */
typedef struct
{
	THREADID tid;
	ADDRINT from;
	ADDRINT to;
	UINT32 type;
} br_t;


VOID Image(IMG img, VOID *v)
{
	GetLock(&fileLock, 1);

	fprintf(imgf, "%lx %lx %s\n", IMG_LowAddress(img), IMG_HighAddress(img),
			IMG_Name(img).c_str());

	ReleaseLock(&fileLock);
}


VOID Routine(RTN rtn, VOID *v)
{
	GetLock(&fileLock, 1);

	string name = IMG_Name(SEC_Img(RTN_Sec(rtn)));

	if (_stricmp(name.c_str(), NTDLL) == 0)
		fprintf(funcf, "%lx %s %s\n", RTN_Address(rtn),
				RTN_Name(rtn).c_str(), name.c_str());

	ReleaseLock(&fileLock);
}


VOID Instruction(INS ins, VOID *v)
{
	if (INS_IsRet(ins))

		INS_InsertFillBuffer(ins, IPOINT_BEFORE, bufId,
			IARG_THREAD_ID, offsetof(br_t, tid),
			IARG_INST_PTR, offsetof(br_t, from),
			IARG_RETURN_IP, offsetof(br_t, to),
			IARG_UINT32, RETURN, offsetof(br_t, type),
			IARG_END);
	
	else if (INS_IsSyscall(ins))

		INS_InsertFillBuffer(ins, IPOINT_BEFORE, bufId,
			IARG_THREAD_ID, offsetof(br_t, tid),
			IARG_INST_PTR, offsetof(br_t, from),
			IARG_SYSCALL_NUMBER, offsetof(br_t, to),
			IARG_UINT32, SYSENTER, offsetof(br_t, type),
			IARG_END);
	
	else if (INS_IsCall(ins))

		INS_InsertFillBuffer(ins, IPOINT_BEFORE, bufId,
			IARG_THREAD_ID, offsetof(br_t, tid),
			IARG_INST_PTR, offsetof(br_t, from),
			IARG_BRANCH_TARGET_ADDR, offsetof(br_t, to),
			IARG_UINT32, CALL, offsetof(br_t, type),
			IARG_END);

	//XXX: for now keep only returns
#if 0
	else if (INS_IsIndirectBranchOrCall(ins)) {

		if (INS_IsCall(ins))

			INS_InsertFillBuffer(ins, IPOINT_BEFORE, bufId,
				IARG_THREAD_ID, offsetof(br_t, tid),
				IARG_INST_PTR, offsetof(br_t, from),
				IARG_BRANCH_TARGET_ADDR, offsetof(br_t, to),
				IARG_UINT32, CALL, offsetof(br_t, type),
				IARG_END);

		else

			INS_InsertFillBuffer(ins, IPOINT_BEFORE, bufId,
				IARG_THREAD_ID, offsetof(br_t, tid),
				IARG_INST_PTR, offsetof(br_t, from),
				IARG_BRANCH_TARGET_ADDR, offsetof(br_t, to),
				IARG_UINT32, JUMP, offsetof(br_t, type),
				IARG_END);
	}
#endif
}


VOID *BufferFull(BUFFER_ID id, THREADID tid, const CONTEXT *ctxt, VOID *buf,
			UINT64 numElements, VOID *v)
{
	GetLock(&fileLock, 1);

	br_t *br = (br_t*) buf;
	unsigned char src[INSN_LEN], dst[INSN_LEN];
	char src_s[INSN_LEN*3+1], dst_s[INSN_LEN*3+1];
	size_t slen, dlen, i, j;

	for (i=0; i < numElements; i++, br++) {

		if (br->type == SYSENTER) {
			fprintf(brf, "sys %d %lx %lx {d3ad} {d3ad}\n",
					br->tid, br->from, br->to);
			continue;
		}
		else if (br->type == CALL) {
			fprintf(brf, "call %d %lx %lx {d3ad} {d3ad}\n",
					br->tid, br->from, br->to);
			continue;
		}

		slen = PIN_SafeCopy(src, (VOID*)br->from, INSN_LEN);
		dlen = PIN_SafeCopy(dst, (VOID*)(br->to-INSN_LEN), INSN_LEN);

		// it happens rarely for weird reasons .. skip it
		// most probably it's due to the fact that we copy the
		// memory whenever the buf is full and not when the br occurs
		if (slen != INSN_LEN || dlen != INSN_LEN)
			continue;

		for (j=0; j < INSN_LEN; j++) {
			sprintf(src_s + j*3, "%02x ", src[j]);
			sprintf(dst_s + j*3, "%02x ", dst[j]);
		}

		src_s[j*3] = dst_s[j*3] = '\0';

		// FIXME: 64-bit addresses are truncated!
		fprintf(brf, "ret %d %lx %lx {%s} {%s}\n",
				br->tid, br->from, br->to, src_s, dst_s);
	}

	ReleaseLock(&fileLock);

	return buf;
}


VOID Fini(INT32 code, VOID *v)
{
	GetLock(&fileLock, 1);

	fclose(brf);
	fclose(imgf);
	fclose(funcf);

	ReleaseLock(&fileLock);
}


int main(int argc, char *argv[])
{
	PIN_InitSymbols();

	if (PIN_Init(argc, argv))
		return 1;
	
	bufId = PIN_DefineTraceBuffer(sizeof(br_t), NUM_BUF_PAGES,
						BufferFull, 0);

	if (bufId == BUFFER_ID_INVALID) {
		fprintf(stderr, "Error: could not allocate initial buffer\n");
		return 1;
	}

	InitLock(&fileLock);

	brf = fopen(TRACE_FILE, "w");
	imgf = fopen(IMAGES_FILE, "w");
	funcf = fopen(FUNCTIONS_FILE, "w");

	if (brf == NULL || imgf == NULL || funcf == NULL) {
		fprintf(stderr, "Error: could not open output files\n");
		return 1;
	}

	IMG_AddInstrumentFunction(Image, 0);
	
	RTN_AddInstrumentFunction(Routine, 0);

	INS_AddInstrumentFunction(Instruction, 0);

	PIN_AddFiniFunction(Fini, 0);
	
	PIN_StartProgram();
	
	return 0;
}
