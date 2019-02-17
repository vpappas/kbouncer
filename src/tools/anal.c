#ifdef _WIN32
#include <windows.h>
#else
#include <limits.h>
#define MAX_PATH	PATH_MAX
#endif

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>

#include "decode.h"

/*
 * Maximum lengh of an x86 instruction
 */
#define MAX_INSN	16

/*
 * Maximum number of images loaded in a process
 */
#define MAX_IMG		1024

/*
 * Maximum number of system call wrapper functions
 */
#define MAX_FUNC	2048


struct image {
	size_t start;
	size_t end;
	char *path;
} images[MAX_IMG];


struct function {
	size_t address;
	char *name;
	char *img_path;
} functions[MAX_FUNC];


int img_cnt = 0;

int func_cnt = 0;


char *lookup_addr(size_t addr);

char *lookup_image(size_t addr)
{
	int i;

	for (i=0; i < img_cnt; i++)
		if (addr >= images[i].start && addr <= images[i].end)
			return images[i].path;

	return NULL;
}

char *lookup_function(size_t addr)
{
	int i;

	for (i=0; i < func_cnt; i++)
		if (functions[i].address == addr)
			return functions[i].name;

	return NULL;
}


int load_images_cb(FILE *fp)
{
	char path[MAX_PATH];
	size_t start, end;

	if (fscanf(fp, "%lx %lx %[^\n\r]\n", &start, &end, path) != 3) {
		fprintf(stderr, "cannot fscanf!\n");
		return 1;
	}

	images[img_cnt].start = start;
	images[img_cnt].end = end;
	images[img_cnt].path = strdup(path);

	if (images[img_cnt].path == NULL) {
		fprintf(stderr, "cannot strdup!\n");
		return 1;
	}

	img_cnt += 1;

	if (img_cnt == MAX_IMG) {
		fprintf(stderr, "raise MAX_IMG!\n");
		return 1;
	}

	return 0;
}


int load_functions_cb(FILE *fp)
{
	char path[MAX_PATH], name[MAX_PATH];
	size_t address;

	if (fscanf(fp, "%lx %s %[^\n]\n", &address, name, path) != 3) {
		fprintf(stderr, "cannot fscanf!\n");
		return 1;
	}

	//printf("%lx %s %s\n", address, name, path);

	functions[func_cnt].address = address;
	functions[func_cnt].name = strdup(name);
	functions[func_cnt].img_path = strdup(path);

	if (functions[func_cnt].name == NULL ||
			functions[func_cnt].img_path == NULL) {
		fprintf(stderr, "cannot strdup!\n");
		return 1;
	}

	func_cnt += 1;

	if (func_cnt == MAX_FUNC) {
		fprintf(stderr, "raise MAX_FUNC!\n");
		return 1;
	}

	return 0;
}

int no_matches = 0;
int more_matches = 0;
int calls = 0;
int returns = 0;
int syscalls = 0;
char *last_func=NULL;

int process_branches_cb(FILE *fp)
{
	size_t from, to;
	char src_s[4*MAX_INSN], dst_s[4*MAX_INSN];
	char *sptr, *dptr, type[16];
	unsigned char src[MAX_INSN], dst[MAX_INSN];
	int tid, i, matches;

	if (fscanf(fp, "%s %d %lx %lx {%[^}]} {%[^}]}\n",
			type, &tid, &from, &to, src_s, dst_s) != 6) {
		fprintf(stderr, "cannot fscanf!\n");
		return 1;
	}

	if (strcmp(type, "ret") == 0) {
		
		returns += 1;

		// 1. check that the instruction in the source can
		// be decoded to a return and that there was a call
		// instruction right above the return address
		for (i=0, sptr=src_s, dptr=dst_s; i < MAX_INSN; i++) {
			src[i] = strtoul(sptr, &sptr, 16);
			dst[i] = strtoul(dptr, &dptr, 16);
		}

		if (decode_ret(src) == 0) {
			fprintf(stderr, "ret src not a ret '%s'!\n", src_s);
			// return 0 and continue; that could be an iret..
			return 0;
		}

		for (i=0, matches=0; i < MAX_INSN-1; i++)
			if (decode_call(&dst[i]) + i == MAX_INSN)
				matches++;

		if (matches == 0) { 
			no_matches += 1;
			fprintf(stderr, "no call match %lx %s > %lx %s '%s'!\n",
					from, lookup_image(from), 
					to, lookup_image(to), dst_s);
		}
		else if (matches > 1)
			more_matches += 1;

		// 2. 

	}
	else if (strcmp(type, "call") == 0) {

		calls += 1;

		// 2. when there is a call to a function from ntdll.dll
		// start counting the number of returns
		//last_func = lookup_function(to);

		//if (last_func != NULL)
		//	printf("call %s from %s\n", last_func, lookup_image(from));

	}
	else if (strcmp(type, "sys") == 0) {
		
		syscalls += 1;

		// 2. print which ntdll call caused this system call
		//printf("syscall %ld from %s %s\n", to,
		//		(last_func)?last_func:"unknown",
		//		lookup_image(from));
	}
	else {
		fprintf(stderr, "unknown type '%s'\n", type);
		return 1;
	}

	return 0;
}


int process_file(char *fname, int (*cb)(FILE *fp))
{
	FILE *fp;

	fp = fopen(fname, "r");

	if (fp == NULL) {
		perror("fopen");
		return 1;
	}

	for (;;) {

		if (cb(fp) != 0)
			return 1;

		if (ferror(fp)) {
			perror("fscanf");
			return 1;
		}
		else if (feof(fp))
			break;
	}

	fclose(fp);

	return 0;
}


int main(int argc, char *argv[])
{
	if (argc != 4) {
		fprintf(stderr, "usage: %s imgs funcs brs\n", argv[0]);
		return 1;
	}

	if (process_file(argv[1], load_images_cb) != 0) {
		fprintf(stderr, "failed to load %s\n", argv[1]);
		return 1;
	}

	printf("loaded %d images\n", img_cnt);

	if (process_file(argv[2], load_functions_cb) != 0) {
		fprintf(stderr, "failed to load %s\n", argv[2]);
		return 1;
	}

	printf("loaded %d functions\n", func_cnt);

	if (process_file(argv[3], process_branches_cb) != 0) {
		fprintf(stderr, "failed to proces %s\n", argv[3]);
		return 1;
	}

	printf("%d returns %d calls %d syscalls\n", returns, calls, syscalls);

	printf("call matches: %d not, %d more\n", no_matches, more_matches);

	return 0;
}
