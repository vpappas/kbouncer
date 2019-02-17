#include <windows.h>
#include <stdio.h>


DWORD main(DWORD argc, CHAR *argv[])
{
	PROCESS_INFORMATION pi;
	STARTUPINFO si;
	FILETIME created, exited, sys, user;
	ULARGE_INTEGER a, b;

	if (argc != 2) {
		printf("usage: %s command\n", argv[0]);
		return 1;
	}

	// set up members of the PROCESS_INFORMATION structure. 
	ZeroMemory(&pi, sizeof(PROCESS_INFORMATION));

	// set up members of the STARTUPINFO structure. 
	ZeroMemory(&si, sizeof(STARTUPINFO));
	si.cb = sizeof(STARTUPINFO);

	// create the child process. 
	if (!CreateProcess(NULL, argv[1], NULL, NULL, TRUE,
				0, NULL, NULL, &si, &pi)) {
		printf("CreateProcess failed (%d)\n", GetLastError());
		return 1;
	}

	// wait until child process exits.
	if (WaitForSingleObject(pi.hProcess, INFINITE) == WAIT_FAILED) {
		printf("WaitForSingleObject failed (%d)\n", GetLastError());
		return 1;
	}

	// get the usage times
	if (!GetProcessTimes(pi.hProcess, &created, &exited, &sys, &user)) {
		printf("GetProcessTime failed (%d)\n", GetLastError());
		return 1;
	}

	// real time (wallclock)
	a.LowPart = created.dwLowDateTime;
	a.HighPart = created.dwHighDateTime;
	b.LowPart = exited.dwLowDateTime;
	b.HighPart = exited.dwHighDateTime;

	printf("real %u.%02u\n", (b.QuadPart-a.QuadPart)/10000000, 
				((b.QuadPart-a.QuadPart)/100000)%100);

	// user CPU time
	a.LowPart = user.dwLowDateTime;
	a.HighPart = user.dwHighDateTime;

	printf("user %u.%02u\n", a.QuadPart/10000000, (a.QuadPart/100000)%100);

	// kernel CPU time
	a.LowPart = sys.dwLowDateTime;
	a.HighPart = sys.dwHighDateTime;
	
	printf("sys %u.%02u\n", a.QuadPart/10000000, (a.QuadPart/100000)%100);

	CloseHandle(pi.hProcess);

	CloseHandle(pi.hThread);

	return 0;
}
