#include <windows.h>
#include <stdio.h>
#include <imagehlp.h>

#pragma comment(lib, "imagehlp.lib")

int main(int argc, char *argv[])
{
	LOADED_IMAGE img;
	PIMAGE_EXPORT_DIRECTORY exp;
	DWORD *nams, *rvas, exp_size;
	WORD *ords;
	LPVOID func;
	char module[MAX_PATH], *tmp, *name;
	int i;

	if (argc != 3) {
		fprintf(stderr, "usage: %s image dest\n", argv[0]);
		return 1;
	}

	if (MapAndLoad(argv[1], NULL, &img, TRUE, TRUE) == FALSE) {
		fprintf(stderr, "load failed %d\n", GetLastError());
		return 1;
	}

	exp = (PIMAGE_EXPORT_DIRECTORY) ImageRvaToVa(
			img.FileHeader, img.MappedAddress, 
			img.FileHeader->OptionalHeader
			.DataDirectory[0].VirtualAddress,
			NULL);

	exp_size = img.FileHeader->OptionalHeader.DataDirectory[0].Size;

	rvas = (DWORD*) ImageRvaToVa(img.FileHeader, img.MappedAddress,
			exp->AddressOfFunctions, NULL);

	nams = (DWORD*) ImageRvaToVa(img.FileHeader, img.MappedAddress,
			exp->AddressOfNames, NULL);

	ords = (WORD*) ImageRvaToVa(img.FileHeader, img.MappedAddress,
			exp->AddressOfNameOrdinals, NULL);

	for (i=0; i < exp->NumberOfFunctions; i++) {

		if (i < exp->NumberOfNames)
			name = ImageRvaToVa(img.FileHeader,
					img.MappedAddress, nams[i], NULL);
		else
			break; //FIXME: unamed export!

		printf("#pragma comment (linker, \"/export:%s=", name);

		func = ImageRvaToVa(img.FileHeader,
				img.MappedAddress, rvas[i], NULL);

		//if (func >= exp && func < (PBYTE)exp + exp_size) //fwd
		//	printf("%s", func);
		//else
			printf("%s.%s", argv[2], name);

		printf(",@%d\")\n", ords[i] + exp->Base);
	}

	if (UnMapAndLoad(&img) == FALSE) {
		fprintf(stderr, "unload failed %d\n", GetLastError());
		return 1;
	}

	return 0;
}
