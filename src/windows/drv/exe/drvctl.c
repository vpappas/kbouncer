#include <windows.h>
#include <winioctl.h>
#include <tchar.h>
#include <strsafe.h>

#include "..\sys\kb.h"
#include "..\..\..\common\decode.h"

//XXX: SOURCES file does not accept sources from other directories .. that's
//why the .c is inlcuded here
#include "..\..\..\common\decode.c"

#ifdef DETOUR
extern VOID Log(LPCTSTR fmt, ...);
#define LOG Log
#else
#include <stdio.h>
#define LOG(x, ...) fprintf(stderr, x, __VA_ARGS__)
#endif

static HANDLE dev;

BOOL DriverInit()
{
	dev = CreateFile( "\\\\.\\kb", GENERIC_READ | GENERIC_WRITE, 0,
			NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);

	if (dev == INVALID_HANDLE_VALUE) {
		LOG("createfile failed: %d\n", GetLastError());
		return FALSE;
	}

	return TRUE;
}

BOOL DriverFini()
{
	if (!CloseHandle(dev)) {
		LOG("closehandle failed: %d\n", GetLastError());
		return FALSE;
	}

	return TRUE;
}

BOOL EnableLBR()
{
	ULONG bytesReturned;

	if (!DeviceIoControl(dev, (DWORD) IOCTL_KB_ENABLE_LBR,
			NULL, 0, NULL, 0, &bytesReturned, NULL)) {

		LOG("enable ioctl error: %d\n", GetLastError());
		return FALSE;
	}

	return TRUE;
}

BOOL DisableLBR()
{
	ULONG bytesReturned;
	int faults;

	if (!DeviceIoControl(dev, (DWORD) IOCTL_KB_DISABLE_LBR, NULL, 0,
			&faults, sizeof(faults), &bytesReturned, NULL)) {

		LOG("disable ioctl error: %d\n", GetLastError());
		return FALSE;
	}

	LOG("faults: %d (bret=%lu)\n", faults, bytesReturned);

	return TRUE;
}

#define MAX_FN	128
#define MAX_LN	256
#define MAX_MSG	1024

VOID Alert(__int64 from, __int64 to)
{
	MEMORY_BASIC_INFORMATION mi;
	TCHAR name[MAX_FN], line[MAX_LN];
	TCHAR msg[MAX_MSG]="kBouncer detected a bad branch!\r\r";
	PDWORD addr;

	StringCchCopy(name, MAX_FN, TEXT("NULL"));

	if (VirtualQuery((LPCVOID)from, &mi, sizeof(mi))) {
		GetModuleFileName((HMODULE)mi.AllocationBase, name, MAX_FN);
		StringCchCopy(name, MAX_FN, _tcsrchr(name, '\\') + 1);
	}

	StringCchPrintf(line, MAX_LN, "From: %llx [%s:%d]\r",
		       	from, name, (PUCHAR)from - (PUCHAR)mi.AllocationBase);
	StringCchCat(msg, MAX_MSG, line);

	StringCchCopy(name, MAX_FN, TEXT("NULL"));

	if (VirtualQuery((LPCVOID)from, &mi, sizeof(mi))) {
		GetModuleFileName((HMODULE)mi.AllocationBase, name, MAX_FN);
		StringCchCopy(name, MAX_FN, _tcsrchr(name, '\\') + 1);
	}

	StringCchPrintf(line, MAX_LN, "To: %llx [%s:%d]\r",
		       	to, name, (PUCHAR)to - (PUCHAR)mi.AllocationBase);
	StringCchCat(msg, MAX_MSG, line);

	StringCchCat(msg, MAX_MSG, TEXT("Dump of destination bytes:\r"));
	
	addr = ((PDWORD) (to & 0xFFFFFFF0)) - 4;

	while (addr <= ((PDWORD) (to & 0xFFFFFFF0)) + 4) {
		StringCchPrintf(line, MAX_LN, "%p: %08lx %08lx %08lx %08lx\r",
				addr, addr[0], addr[1], addr[2], addr[3]);
		StringCchCat(msg, MAX_MSG, line);
		addr += 4;
	}

	MessageBox(NULL, msg, NULL, MB_OK | MB_ICONHAND | MB_TASKMODAL);
}

BOOL CheckLBR()
{
	ULONG bytesReturned;
	BR br;

	if (!DeviceIoControl(dev, (DWORD) IOCTL_KB_CHECK_LBR,
			NULL, 0, &br, sizeof(br), &bytesReturned, NULL)) {

		LOG("check ioctl error: %d\n", GetLastError());
		return FALSE;
	}

	if (bytesReturned == sizeof(br)) {
		LOG("bad br %llx -> %llx\n", br.from, br.to);
		Alert(br.from, br.to);
		ExitProcess(0x42);
	}

	return TRUE;
}

BOOL CheckLBRDump()
{
	ULONG bytesReturned, i;
	BR lbr[LBR_SZ];

	if (!DeviceIoControl(dev, (DWORD) IOCTL_KB_DUMP_LBR,
			NULL, 0, lbr, sizeof(lbr), &bytesReturned, NULL)) {

		LOG("dump ioctl error: %d\n", GetLastError());
		return FALSE;
	}

	for (i=0; i < LBR_SZ; i++) {

		LOG("%2d: %llx -> %llx\n", i, lbr[i].from, lbr[i].to);

		// TODO: add exception handling for EXCEPTION_ACCESS_VIOLATION
		if (check_branch((PUCHAR)lbr[i].from, (PUCHAR)lbr[i].to)) {

			MessageBox(NULL, TEXT("got you!"), NULL,
					MB_OK | MB_ICONHAND | MB_TASKMODAL);
			ExitProcess(0x42);
		}
	}

	return TRUE;
}

