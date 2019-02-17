// http://msdn.microsoft.com/en-us/library/aa366763%28VS.85%29.aspx
// http://msdn.microsoft.com/en-us/library/aa366551(v=vs.85).aspx
// http://msdn.microsoft.com/en-us/library/ms686958(v=vs.85).aspx
#define UNICODE
#define _UNICODE
#include <windows.h>
#include <winternl.h>
#include <dbghelp.h>
#include <tchar.h>
#include <stdio.h>

#pragma comment(lib, "dbghelp.lib")

#define printf(fmt, ...) _ftprintf(stderr, TEXT(fmt), __VA_ARGS__)

typedef NTSTATUS (NTAPI *pNtQueryInformationProcess)(HANDLE ProcessHandle,
	PROCESSINFOCLASS ProcessInformationClass, PVOID ProcessInformation,
	ULONG ProcessInformationLength, PULONG ReturnLength);

static HANDLE hProcess;    // child process

HANDLE CreateChildProcess(LPTSTR szCmdline) 
{
	PROCESS_INFORMATION piProcInfo;
	STARTUPINFO siStartInfo;
	BOOL bFuncRetn = FALSE;

	// set up members of the PROCESS_INFORMATION structure. 
	ZeroMemory(&piProcInfo, sizeof(PROCESS_INFORMATION));

	// set up members of the STARTUPINFO structure. 
	ZeroMemory(&siStartInfo, sizeof(STARTUPINFO));
	siStartInfo.cb = sizeof(STARTUPINFO); 

	// create the child process. 
	bFuncRetn = CreateProcess(NULL, 
			szCmdline,	// command line 
			NULL,		// process security attributes 
			NULL,		// primary thread security attributes 
			TRUE,		// handles are inherited 
			DEBUG_PROCESS,	// creation flags 
			NULL,		// use parent's environment 
			NULL,		// use parent's current directory 
			&siStartInfo,	// STARTUPINFO pointer 
			&piProcInfo);	// receives PROCESS_INFORMATION 

	if (bFuncRetn == 0) {
		printf("CreateProcess failed (%d)\n", GetLastError());
		return INVALID_HANDLE_VALUE;
	}
	
	CloseHandle(piProcInfo.hThread);

	return piProcInfo.hProcess;
}

BOOL AcroRd32exeFix(DEBUG_EVENT *e)
{
	LPVOID pthree;
	CHAR three, four='4';
	DWORD old, old2;

#if 0 // XXX: the lpImageName is always NULL .. 
	LPVOID name = NULL;
	TCHAR dllName[MAX_PATH];
	LPVOID apip, api;
	ULONG len;

	printf("fix AcroRd.exe\n");

	if (e->u.CreateProcessInfo.lpImageName == NULL)
		return FALSE;

	if (!ReadProcessMemory(hProcess, e->u.LoadDll.lpImageName,
				&name, sizeof(LPVOID), NULL)) {
		printf("cant read mem (%d)!!\n", GetLastError());
		return FALSE;
	}

	if (name == NULL) {
		printf("name is null\n");
		return FALSE;
	}

	if (!ReadProcessMemory(hProcess, name, dllName, MAX_PATH, NULL)) {
		printf("cant read mem (%d)\n!!", GetLastError());
		return FALSE;
	}

	// at this point we have read the new module's name
	printf("loaded %s\n", dllName);

	if (_tcsstr(dllName, TEXT("AcroRd32.exe")) == NULL) {
		printf("not AcroRd32.exe\n");
		return FALSE;
	}
#endif
	// assume we only load AcroRd.exe ..
	
	printf("image base %lx\n", e->u.CreateProcessInfo.lpBaseOfImage);

	pthree = &((PCHAR)e->u.CreateProcessInfo.lpBaseOfImage)[0x717a];

	if (!ReadProcessMemory(hProcess, pthree, &three, sizeof(three), NULL)) {
		printf("cant read mem (%d)\n!!", GetLastError());
		return FALSE;
	}

	if (three != '3') {
		printf("read %x from %lx\n", three, pthree);
		return FALSE;
	}

	if (!VirtualProtectEx(hProcess, pthree, sizeof(four),
				PAGE_READWRITE, &old)) {
		printf("cant change perms (%d)\n!!", GetLastError());
		return FALSE;
	}

	if (!WriteProcessMemory(hProcess, pthree, &four, sizeof(four), NULL)) {
		printf("cant write mem (%d)\n!!", GetLastError());
		return FALSE;
	}
	
	if (!VirtualProtectEx(hProcess, pthree, sizeof(four), old, &old2)) {
		printf("cant change perms (%d)\n!!", GetLastError());
		return FALSE;
	}

	printf("changed 3 to 4\n");

	return TRUE;
}

BOOL ProcessLoadKernel32Event(DEBUG_EVENT *e)
{
	LPVOID name = NULL;
	TCHAR dllName[MAX_PATH];
	HMODULE hNtDll;
	pNtQueryInformationProcess qip;
	PROCESS_BASIC_INFORMATION pbi={0};
	LPVOID apip, api;
	ULONG len;


	if (e->u.LoadDll.lpImageName == NULL)
		return FALSE;

	if (!ReadProcessMemory(hProcess, e->u.LoadDll.lpImageName,
				&name, sizeof(LPVOID), NULL)) {
		printf("cant read mem (%d)!!\n", GetLastError());
		return FALSE;
	}

	if (name == NULL) {
		//printf("name is null\n");
		return FALSE;
	}

	if (!ReadProcessMemory(hProcess, name, dllName, MAX_PATH, NULL)) {
		printf("cant read mem (%d)\n!!", GetLastError());
		return FALSE;
	}

	// at this point we have read the new module's name

	if (_tcsstr(dllName, TEXT("kernel32.dll")) == NULL) {
		//printf("not kernel32.dll\n");
		return FALSE;
	}

	if ((hNtDll=LoadLibrary(TEXT("ntdll.dll"))) == NULL) {
		printf("failed to load ntdll\n");
		return FALSE;
	}

	qip = (pNtQueryInformationProcess) GetProcAddress(hNtDll,
				"NtQueryInformationProcess");
	if (qip == NULL) {
		printf("failed to get qip\n");
		FreeLibrary(hNtDll);
		return FALSE;
	}

	if (qip(hProcess, ProcessBasicInformation, &pbi, sizeof(pbi), &len)) {
		printf("failed to get process info\n");
		return FALSE;
	}

	printf("peb at %p\n", pbi.PebBaseAddress);

	// XXX for 64-bit: apip = &((DWORDLONG*)pbi.PebBaseAddress)[13];
	apip = &((DWORD*)pbi.PebBaseAddress)[14];

	if (!ReadProcessMemory(hProcess, apip, &api, sizeof(LPVOID), NULL)) {
		printf("cant read mem (%d)!!\n", GetLastError());
		return FALSE;
	}

	printf("apiset at %p\n", api);

	// XXX hardcoded value .. should dynamically find the export symbol:
	// api_set_schema in the proxy kernel32.dll
	api = (PBYTE)e->u.LoadDll.lpBaseOfDll + 0x16000;

#if 0 // FIXME: does not work .. 
	if (SymInitialize(hProcess, NULL, TRUE) == FALSE) {
		printf("cannot init sym (%d)\n", GetLastError());
		return FALSE;
	}

	sym->SizeOfStruct = sizeof(SYMBOL_INFO);
	sym->MaxNameLen = MAX_SYM_NAME;
	_tcscpy_s(snam, MAX_SYM_NAME, TEXT("WinExec"));

	if (!SymFromName(hProcess, (PCSTR)snam, sym)) {
		printf("could not find api_set_schema (%d)\n", GetLastError());
		return FALSE;
	}
#endif

	if (!WriteProcessMemory(hProcess, apip, &api, sizeof(LPVOID), NULL)) {
		printf("cant write mem (%d)!!\n", GetLastError());
		return FALSE;
	}
	
	printf("changed apiset to %p\n", api);

	return TRUE;
}


VOID PrintException(DEBUG_EVENT *e)
{
	PEXCEPTION_RECORD x = &e->u.Exception.ExceptionRecord;
	int i;

	printf("%x %x %lx %d\n", x->ExceptionCode, x->ExceptionFlags,
			x->ExceptionAddress, x->NumberParameters);

	for (i=0; i < x->NumberParameters; i++)
		printf("\t%lx\n", x->ExceptionInformation[i]);
}


void DebugLoop()
{
	BOOL completed = FALSE; 
	BOOL attached = FALSE;
	DEBUG_EVENT DebugEvent;

	while (!completed) {

		if (!WaitForDebugEvent(&DebugEvent, INFINITE)) {
			printf("Debug loop aborted\n");
			ExitProcess(1);
		}

		switch (DebugEvent.dwDebugEventCode) {
		
			case CREATE_PROCESS_DEBUG_EVENT:
				AcroRd32exeFix(&DebugEvent);
				printf("CREATE_PROCESS_DEBUG_EVENT\n");
				break;

			case EXIT_PROCESS_DEBUG_EVENT:
			case RIP_EVENT:
				printf("EXIT_PROCESS_DEBUG_EVENT\n");
				//completed = TRUE;
				break;

			case CREATE_THREAD_DEBUG_EVENT:
				printf("CREATE_THREAD_DEBUG_EVENT\n");
				break;

			case EXIT_THREAD_DEBUG_EVENT:
				printf("EXIT_THREAD_DEBUG_EVENT\n");
				break;

			case LOAD_DLL_DEBUG_EVENT:
				//printf("LOAD_DLL_DEBUG_EVENT\n");
				if (ProcessLoadKernel32Event(&DebugEvent))
					; //completed = TRUE;
				break;

			case EXCEPTION_DEBUG_EVENT:
				if (!attached) {
					// First exception is special
					printf("EXCEPTION_DEBUG_EVENT first\n");
					attached = TRUE;
				}
				else if (DebugEvent.u.Exception.dwFirstChance) {
					printf("EXCEPTION_DEBUG_EVENT chanc\n");
					PrintException(&DebugEvent);
				}
				else {
					printf("EXCEPTION_DEBUG_EVENT\n");
					PrintException(&DebugEvent);
					ExitProcess(1);
				}
				break;
		}


		if (completed)
			DebugSetProcessKillOnExit(FALSE);

		// XXX nothing is handled (last flag bellow)
		if (!ContinueDebugEvent(DebugEvent.dwProcessId,
					DebugEvent.dwThreadId,
					DBG_EXCEPTION_NOT_HANDLED)) {
			printf("Error continuing debug event\n");
			ExitProcess(1);
		}

		if (completed) {
			DebugActiveProcessStop(DebugEvent.dwProcessId);
		}
	}
}


int _tmain(int argc, TCHAR *argv[])
{
	BOOL res;

	printf("%x\n", EXCEPTION_ACCESS_VIOLATION);

	if (argc == 1) {
		printf("usage: %s PROGRAM\n", argv[0]);
		ExitProcess(1);
	}

	// Start the child process that will read the memory
	hProcess = CreateChildProcess(argv[1]);

	// Ensure this process is around until the child process terminates
	if (hProcess == INVALID_HANDLE_VALUE) {
		printf("failed to create the child process..\n");
		ExitProcess(1);
	}

	printf("successfully created the child process, waiting..\n");

	//WaitForSingleObject(hProcess, INFINITE);
	DebugLoop();

	CloseHandle(hProcess);

	return 0;
}
