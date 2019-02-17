#define _WIN32_WINNT        0x0400
#define WIN32
#define NT

#define DBG_TRACE   0

#include <windows.h>
#include <stdio.h>

#include "detours.h"

extern "C" {
#include "drvctl.h"
}

#define PULONG_PTR          PVOID
#define PLONG_PTR           PVOID
#define ULONG_PTR           PVOID
#define ENUMRESNAMEPROCA    PVOID
#define ENUMRESNAMEPROCW    PVOID
#define ENUMRESLANGPROCA    PVOID
#define ENUMRESLANGPROCW    PVOID
#define ENUMRESTYPEPROCA    PVOID
#define ENUMRESTYPEPROCW    PVOID
#define STGOPTIONS          PVOID

//////////////////////////////////////////////////////////////////////////////

#pragma warning(disable:4127)   // Many of our asserts are constants.

#define ASSERT_ALWAYS(x)   \
    do {                                                        \
    if (!(x)) {                                                 \
	    Log(TEXT("ASSERT(%s) failed in %s, line %d.\n"),	\
                TEXT(#x), __FILE__, __LINE__);			\
            DebugBreak();                                       \
    }                                                           \
    } while (0)

#ifndef NDEBUG
#define ASSERT(x)           ASSERT_ALWAYS(x)
#else
#define ASSERT(x)
#endif

#define UNUSED(c)    (c) = (c)


//////////////////////////////////////////////////////////////////////////////
static HMODULE s_hInst = NULL;
static CHAR s_szDllPath[MAX_PATH];

extern "C" VOID Log(LPCTSTR fmt, ...);

//////////////////////////////////////////////////////////////////////////////
// Trampolines
//
extern "C" {

    HANDLE (WINAPI *
            Real_CreateFileW)(LPCWSTR a0,
                              DWORD a1,
                              DWORD a2,
                              LPSECURITY_ATTRIBUTES a3,
                              DWORD a4,
                              DWORD a5,
                              HANDLE a6)
        = CreateFileW;

    BOOL (WINAPI *
          Real_WriteFile)(HANDLE hFile,
                          LPCVOID lpBuffer,
                          DWORD nNumberOfBytesToWrite,
                          LPDWORD lpNumberOfBytesWritten,
                          LPOVERLAPPED lpOverlapped)
        = WriteFile;
    BOOL (WINAPI *
          Real_FlushFileBuffers)(HANDLE hFile)
        = FlushFileBuffers;
    BOOL (WINAPI *
          Real_CloseHandle)(HANDLE hObject)
        = CloseHandle;

    BOOL (WINAPI *
          Real_WaitNamedPipeW)(LPCWSTR lpNamedPipeName, DWORD nTimeOut)
        = WaitNamedPipeW;
    BOOL (WINAPI *
          Real_SetNamedPipeHandleState)(HANDLE hNamedPipe,
                                        LPDWORD lpMode,
                                        LPDWORD lpMaxCollectionCount,
                                        LPDWORD lpCollectDataTimeout)
        = SetNamedPipeHandleState;

    DWORD (WINAPI *
           Real_GetCurrentProcessId)(VOID)
        = GetCurrentProcessId;
    VOID (WINAPI *
          Real_GetSystemTimeAsFileTime)(LPFILETIME lpSystemTimeAsFileTime)
        = GetSystemTimeAsFileTime;

    VOID (WINAPI *
          Real_InitializeCriticalSection)(LPCRITICAL_SECTION lpSection)
        = InitializeCriticalSection;
    VOID (WINAPI *
          Real_EnterCriticalSection)(LPCRITICAL_SECTION lpSection)
        = EnterCriticalSection;
    VOID (WINAPI *
          Real_LeaveCriticalSection)(LPCRITICAL_SECTION lpSection)
        = LeaveCriticalSection;
}

#if _MSC_VER < 1300
LPVOID (WINAPI *
        Real_HeapAlloc)(HANDLE hHeap, DWORD dwFlags, DWORD dwBytes)
    = HeapAlloc;
#else
LPVOID (WINAPI *
        Real_HeapAlloc)(HANDLE hHeap, DWORD dwFlags, DWORD_PTR dwBytes)
    = HeapAlloc;
#endif

DWORD (WINAPI * Real_GetModuleFileNameW)(HMODULE a0,
                                         LPWSTR a1,
                                         DWORD a2)
    = GetModuleFileNameW;

DWORD (WINAPI * Real_GetModuleFileNameA)(HMODULE a0,
                                         LPSTR a1,
                                         DWORD a2)
    = GetModuleFileNameA;

BOOL (WINAPI * Real_CreateProcessW)(LPCWSTR a0,
                                    LPWSTR a1,
                                    LPSECURITY_ATTRIBUTES a2,
                                    LPSECURITY_ATTRIBUTES a3,
                                    BOOL a4,
                                    DWORD a5,
                                    LPVOID a6,
                                    LPCWSTR a7,
                                    struct _STARTUPINFOW* a8,
                                    LPPROCESS_INFORMATION a9)
    = CreateProcessW;

HANDLE (WINAPI * Real_CreateFileA)(LPCSTR a0,
                              DWORD a1,
                              DWORD a2,
                              LPSECURITY_ATTRIBUTES a3,
                              DWORD a4,
                              DWORD a5,
                              HANDLE a6)
        = CreateFileA;

BOOL (WINAPI * Real_VirtualProtect)(LPVOID a1,
				SIZE_T a2,
				DWORD a3,
				PDWORD a4)
	= VirtualProtect;

//////////////////////////////////////////////////////////////////////////////
// Detours
//

BOOL WINAPI Mine_VirtualProtect(
	LPVOID lpAddress,
	SIZE_T dwSize,
	DWORD flNewProtect,
	PDWORD lpflOldProtect)
{
	
	if (!CheckLBR())
		Log("dump LBR failed %d\n", GetLastError());
	else
		Log("dumped LBR!\n");

	Log("VirtualProtect(%p, %ld, %x, %p)\n", 
		lpAddress, dwSize, flNewProtect, lpflOldProtect);


    BOOL b = FALSE;
    __try {
        b = Real_VirtualProtect(
		lpAddress, dwSize, flNewProtect, lpflOldProtect);
    } __finally {
        Log("CreateFileA() -> %d\n", b);
    };
    return b;
}

HANDLE WINAPI Mine_CreateFileA(
	LPCTSTR lpFileName,
	DWORD dwDesiredAccess,
	DWORD dwShareMode,
	LPSECURITY_ATTRIBUTES lpSecurityAttributes,
	DWORD dwCreationDisposition,
	DWORD dwFlagsAndAttributes,
	HANDLE hTemplateFile)		
{

	if (!CheckLBR())
		Log("dump LBR failed %d\n", GetLastError());
	else
		Log("dumped LBR!\n");

	Log("CreateFileA(%s, %x, %x, %p, %x, %x, %d)\n", 
		lpFileName, dwDesiredAccess, dwShareMode,
		lpSecurityAttributes, dwCreationDisposition,
		dwFlagsAndAttributes, hTemplateFile);


    HANDLE h = 0;
    __try {
        h = Real_CreateFileA(lpFileName, dwDesiredAccess, dwShareMode,
		lpSecurityAttributes, dwCreationDisposition,
		dwFlagsAndAttributes, hTemplateFile);
    } __finally {
        Log("CreateFileA() -> %x\n", h);
    };
    return h;
}

BOOL WINAPI Mine_CreateProcessW(LPCWSTR lpApplicationName,
                                LPWSTR lpCommandLine,
                                LPSECURITY_ATTRIBUTES lpProcessAttributes,
                                LPSECURITY_ATTRIBUTES lpThreadAttributes,
                                BOOL bInheritHandles,
                                DWORD dwCreationFlags,
                                LPVOID lpEnvironment,
                                LPCWSTR lpCurrentDirectory,
                                LPSTARTUPINFOW lpStartupInfo,
                                LPPROCESS_INFORMATION lpProcessInformation)
{
    Log("CreateProcessW(%ls,%ls,%p,%p,%x,%x,%p,%ls,%p,%p)\n",
                lpApplicationName,
                lpCommandLine,
                lpProcessAttributes,
                lpThreadAttributes,
                bInheritHandles,
                dwCreationFlags,
                lpEnvironment,
                lpCurrentDirectory,
                lpStartupInfo,
                lpProcessInformation);

	Log("Calling DetourCreateProcessWithDllW(,%hs)\n", s_szDllPath);

    BOOL rv = 0;
    __try {
        rv = DetourCreateProcessWithDllW(lpApplicationName,
                                         lpCommandLine,
                                         lpProcessAttributes,
                                         lpThreadAttributes,
                                         bInheritHandles,
                                         dwCreationFlags,
                                         lpEnvironment,
                                         lpCurrentDirectory,
                                         lpStartupInfo,
                                         lpProcessInformation,
                                         s_szDllPath,
                                         Real_CreateProcessW);
    } __finally {
        Log("CreateProcessW(,,,,,,,,,) -> %x\n", rv);
    };
    return rv;
}

//////////////////////////////////////////////////////////////////////////////
// AttachDetours
//
VOID DetAttach(PVOID *ppbReal, PVOID pbMine, PCHAR psz)
{
    LONG l = DetourAttach(ppbReal, pbMine);
    if (l != 0) {
        Log("Attach failed: `%s': error %d\n", psz, l);
    }
}

VOID DetDetach(PVOID *ppbReal, PVOID pbMine, PCHAR psz)
{
    LONG l = DetourDetach(ppbReal, pbMine);
    if (l != 0) {
        Log("Detach failed: `%s': error %d\n", psz, l);
    }
}

#define ATTACH(x)       DetAttach(&(PVOID&)Real_##x,Mine_##x,#x)

LONG AttachDetours(VOID)
{
    DetourTransactionBegin();
    DetourUpdateThread(GetCurrentThread());

    ATTACH(CreateProcessW);
    ATTACH(CreateFileA);
    ATTACH(VirtualProtect);

    return DetourTransactionCommit();
}

#define DETACH(x)       DetDetach(&(PVOID&)Real_##x,Mine_##x,#x)

LONG DetachDetours(VOID)
{
    DetourTransactionBegin();
    DetourUpdateThread(GetCurrentThread());

    DETACH(CreateProcessW);
    DETACH(CreateFileA);
    DETACH(VirtualProtect);

    return DetourTransactionCommit();
}

////////////////////////////////////////////////////////////// Logging System.
//

#include <strsafe.h>
#define MAX_MSG	1024
#define S_DOTS	TEXT("...\n")
#define S_ERR	TEXT("failed to sprintf\n")

VOID Log(LPCTSTR fmt, ...)
{
	HANDLE h;
	TCHAR msg[MAX_MSG];
	HRESULT hr;
	DWORD written;
	va_list args;
	size_t len;
	
	h = Real_CreateFileA("kb.log", FILE_APPEND_DATA,
			FILE_SHARE_WRITE, NULL, OPEN_ALWAYS,
			FILE_ATTRIBUTE_NORMAL, NULL);

	if (h == INVALID_HANDLE_VALUE)
		return;

	va_start(args, fmt);

	hr = StringCchVPrintf(msg, MAX_MSG, fmt, args);

	va_end(args);

	if (hr == STRSAFE_E_INSUFFICIENT_BUFFER) {
		StringCbCopy(&msg[MAX_MSG-5], sizeof(S_DOTS), S_DOTS);
		len = MAX_MSG;
	}
	else if (hr == STRSAFE_E_INVALID_PARAMETER) {
		StringCbCopy(msg, sizeof(S_ERR), S_ERR);
		len = sizeof(S_ERR);
	}
	else {
		StringCchLength(msg, MAX_MSG, &len);
		len = (len + 1) * sizeof(TCHAR); // null byte ..
	}

	//XXX no checks ..
	WriteFile(h, msg, len, &written, NULL);

	CloseHandle(h);
}


VOID NullExport()
{
}

//////////////////////////////////////////////////////////////////////////////
//
// DLL module information
//
BOOL ThreadAttach(HMODULE hDll)
{
    (void)hDll;

    return TRUE;
}

BOOL ThreadDetach(HMODULE hDll)
{
    (void)hDll;

    return TRUE;
}

BOOL ProcessAttach(HMODULE hDll)
{

    WCHAR wzExePath[MAX_PATH];

    s_hInst = hDll;
    Real_GetModuleFileNameA(s_hInst, s_szDllPath, ARRAYSIZE(s_szDllPath));
    Real_GetModuleFileNameW(NULL, wzExePath, ARRAYSIZE(wzExePath));

    Log("##########################################\n");
    Log("### %ls\n", wzExePath);

    LONG error = AttachDetours();
    if (error != NO_ERROR) {
        Log("### Error attaching detours: %d\n", error);
    }

    ThreadAttach(hDll);

    if (!DriverInit())
        Log("### Error initializing driver: %d\n", GetLastError());
    else if (!EnableLBR())
        Log("### Error enabling: %d\n", GetLastError());
    else
	Log("### driver initialized and LBR is enabled!\n");

    CheckLBR();

    return TRUE;
}

BOOL ProcessDetach(HMODULE hDll)
{
    ThreadDetach(hDll);

    LONG error = DetachDetours();
    if (error != NO_ERROR) {
        Log("### Error detaching detours: %d\n", error);
    }

    Log("### Closing.\n");

    return TRUE;
}

BOOL APIENTRY DllMain(HINSTANCE hModule, DWORD dwReason, PVOID lpReserved)
{
    (void)hModule;
    (void)lpReserved;

    switch (dwReason) {
      case DLL_PROCESS_ATTACH:
        DetourRestoreAfterWith();
        return ProcessAttach(hModule);
      case DLL_PROCESS_DETACH:
        return ProcessDetach(hModule);
      case DLL_THREAD_ATTACH:
        return ThreadAttach(hModule);
      case DLL_THREAD_DETACH:
        return ThreadDetach(hModule);
    }
    return TRUE;
}
//
///////////////////////////////////////////////////////////////// End of File.
