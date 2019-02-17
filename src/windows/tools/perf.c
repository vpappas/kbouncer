#include <windows.h>
#include <stdio.h>

#include "..\drv\exe\drvctl.h"
#include "..\..\common\decode.h"

#pragma comment(lib, "user32.lib")

#define LOG(x, ...)	fprintf(stderr, x, __VA_ARGS__)


DWORD TimeSysCheck(DWORD num)
{
	DWORD i;

	if (!DriverInit()) {
		LOG("failed to init driver\n");
		return 1;
	}

	if (!EnableLBR()) {
		LOG("failed to enable LBR\n");
		return 1;
	}

	for (i=0; i < num; i++) {
		if (!CheckLBR()) {
			LOG("failed to check LBR\n");
			return 1;
		}
	}

	if (!DisableLBR()) {
		LOG("failed to disable LBR\n");
		return 1;
	}

	if (!DriverFini()) {
		LOG("failed to fini driver\n");
		return 1;
	}

	return 0;
}

DWORD TimeUserCheck(DWORD num)
{
	MEMORY_BASIC_INFORMATION mi;
	PUCHAR base, addr;
	SIZE_T size;
	DWORD i;

	// VirualQuery is in kernel32.dll
	if (VirtualQuery(VirtualQuery, &mi, sizeof(mi)) == 0) {
		LOG("failed to vquery %d\n", GetLastError());
		return 1;
	}

	base = mi.AllocationBase;
	size = mi.RegionSize;

	for (i=0; i < num; i++) {
		addr = base + i % size;
		check_branch(addr, addr);
	}

	return 0;
}

DWORD main(DWORD argc, CHAR *argv[])
{
	ULARGE_INTEGER start, end;
	FILETIME ft;

	GetSystemTimeAsFileTime(&ft);
	start.LowPart = ft.dwLowDateTime;
	start.HighPart = ft.dwHighDateTime;

	//TimeUserCheck(1000000000);
	
	TimeSysCheck   (10000000);

	GetSystemTimeAsFileTime(&ft);
	end.LowPart = ft.dwLowDateTime;
	end.HighPart = ft.dwHighDateTime;


	printf("took %lld ms\n", (end.QuadPart-start.QuadPart)/10000);

	return 0;
}
