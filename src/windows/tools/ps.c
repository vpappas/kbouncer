#include <windows.h>
#include <stdio.h>
#include <psapi.h>

#pragma comment(lib, "psapi.lib")
#pragma comment(lib, "advapi32.lib")

// based on http://msdn.microsoft.com/en-us/library/windows/desktop/ms682621(v=vs.85).aspx and 
// simply compile with cl.exe .. 

#define MAX_MODS	1024
#define MAX_PROCS	1024

// globals
HMODULE modules[MAX_MODS];
DWORD processes[MAX_PROCS]; 

void DisplayError(LPTSTR func)
{
	LPTSTR buf;
	DWORD len;
	
	fprintf(stderr,"%s() error!\n", func);
	
	len = FormatMessage(
		FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM,
		NULL, GetLastError(),
		GetSystemDefaultLangID(),
		(LPTSTR) &buf, 0, NULL);
	
	if (len == 0)
		fprintf(stderr, "(could not get error message for %d)\n",
	    		    GetLastError());
	else {
		fprintf(stderr, "%s", buf);
		LocalFree(buf);
	}
}

int PrintModules(DWORD pid)
{
	CHAR name[MAX_PATH];
	HANDLE proc;
	DWORD needed, status;
	MODULEINFO info;
	unsigned int i;
	
	printf("\nProcess ID: %u ", pid);

	// Get a handle to the process.
	proc = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ,
				FALSE, pid);
	if (proc == NULL) {
		DisplayError("OpenProcess");
		return 1;
	}

	// Get the filename of the process
	if (GetProcessImageFileName(proc, name, MAX_PATH) == 0) {
		DisplayError("GetProcessImageFileName");
        	printf("(error getting name %d):\n", GetLastError());
	}
	else
		printf("%s:\n", name);

	// Get a list of all the modules in this process.
	if (EnumProcessModulesEx(proc, modules, sizeof(modules),
				&needed, LIST_MODULES_ALL) == 0) {
		DisplayError("EnumProcessModulesEx");
		CloseHandle(proc);
		return 1;
	}
	
	// check if maximum number of modules was exceeded
	if (needed > sizeof(modules)) {
		fprintf(stderr, "raise MAX_MODULES!!\n");
		exit(1);
	}

	for (i=0; i < (needed / sizeof(HMODULE)); i++) {
		// Get the base name of the module
		if (GetModuleBaseName(proc, modules[i], name, MAX_PATH) == 0) {
			DisplayError("GetModuleBaseName");
			printf("\tunknown", modules[i]);
		}
		else
			printf("\t%s", name, modules[i]);

		// Get the base and size for the module
		if (GetModuleInformation(proc, modules[i],
				&info, sizeof(info)) == 0) {
			DisplayError("GetModuleInformation");
			printf(" N/A\n", modules[i]);
		}
		else
			printf(" %p %d\n", info.lpBaseOfDll, info.SizeOfImage);
	}
    

	// Release the handle to the process.
	CloseHandle(proc);

	return 0;
}

BOOL EnableDebugPrivilege() 
{
	HANDLE token;
	TOKEN_PRIVILEGES tp = { 0 }; 
	LUID luid; 
	DWORD cb = sizeof(TOKEN_PRIVILEGES); 

	// look up the priviledge
	if (LookupPrivilegeValue(NULL, SE_DEBUG_NAME, &luid) == 0) {
		DisplayError("LookupPrivilegeValue");
		return FALSE;
	}

	// open our token
	if (OpenThreadToken(GetCurrentThread(),
				TOKEN_ADJUST_PRIVILEGES | TOKEN_QUERY,
				FALSE, &token) == 0) {

		if (GetLastError() == ERROR_NO_TOKEN) {
			if (!ImpersonateSelf(SecurityImpersonation))
				return FALSE;

			if (OpenThreadToken(GetCurrentThread(),
					TOKEN_ADJUST_PRIVILEGES | TOKEN_QUERY,
				       	FALSE, &token) == 0) {
				DisplayError("OpenThreadToken");
				return FALSE;
			}
		}
		else {
			DisplayError("OpenThreadToken");
			return FALSE;
		}
	}

	// setup the priviledge token
	tp.PrivilegeCount = 1;
	tp.Privileges[0].Luid = luid;
	tp.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED; 
	
	// adjust the priviledge
	if (AdjustTokenPrivileges(token, FALSE, &tp, cb, NULL, NULL) == 0) { 
		DisplayError("AdjustTokenPrivileges");
		CloseHandle(token);
		return FALSE; 
	}

	CloseHandle(token);

	return TRUE;
}

int main(int argc, char *argv[])
{
	DWORD needed;
	unsigned int i;

	// enable SeDebugPrivilege
	if (EnableDebugPrivilege() == FALSE)
		return 1;

	// Get the list of process identifiers
	if (EnumProcesses(processes, sizeof(processes), &needed) == 0) {
		DisplayError("EnumProcesses");
		return 1;
	}

	if (needed > sizeof(processes)) {
		fprintf(stderr, "raise the maximum number of processes!\n");
		return 1;
	}

	// Print the names of the modules for each process.
	for (i=0; i < (needed / sizeof(DWORD)); i++)
		PrintModules(processes[i]);

	return 0;
}
