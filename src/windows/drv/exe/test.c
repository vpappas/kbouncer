#include <windows.h>
#include <stdio.h>

#include "drvctl.h"

#define LOG(x, ...) fprintf(stderr, x, __VA_ARGS__)

__declspec(noinline)
VOID RopFunc()
{
	void *ret = _AddressOfReturnAddress();
	char *val = *(char**) ret;

	*(void**)ret = val + 11; // skips the increment
}

struct {
	int model;
	char *name;
} models[] = {
	{0x206E6, "Nehalem - Beckton"},
	{0x106A4, "Nehalem - Bloomfield or Gainestown"},
	{0x106A5, "Nehalem - Bloomfield or Gainestown"},
	{0x106E4, "Nehalem - Clarksfield or Lynnfield or Jasper Forest"},
	{0x106E5, "Nehalem - Clarksfield or Lynnfield or Jasper Forest"},
	{0x206F2, "Nehalem - Westmere-EX"},
	{0x206C2, "Nehalem - Gulftown or Westmere-EP"},
	{0x20652, "Nehalem - Arrandale or Clarkdale"},
	{0x20655, "Nehalem - Arrandale or Clarkdale"},
	{0x206A7, "Sandy Bridge - HE-4 or H-2 or M-2"},
	{0x206D6, "Sandy Bridge - EP-8 or EP-4"},
	{0x206D7, "Sandy Bridge - EP-8 or EP-4"},
	{0, NULL}
};


DWORD main()
{
	int num=69;
	int info[4], i;

	// check the CPU's microarchitecture
	__cpuid(info, 1);

	for (i=0; models[i].model != 0 && info[0] != models[i].model; i++)
		;
	if (models[i].model != 0)
		LOG("CPU: '%s' should support selective LBR\n", models[i].name);
	else
		LOG("could not identify the CPU model (%x) .. not sure if"
			"it supports the selective LBR feature\n", info[0]);

	if (!DriverInit()) {
		LOG("failed to init driver\n");
		return 1;
	}

	if (!EnableLBR()) {
		LOG("failed to enable LBR\n");
		return 1;
	}
	
	RopFunc();
	num++;

	if (!CheckLBR()) {
		LOG("failed to check LBR\n");
		return 1;
	}

	LOG("num is %d\n", num);

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
