#ifndef __KB_DECODE_H__
#define __KB_DECODE_H__

#define MAX_INSN	16 //bytes in x86

int decode_ret(unsigned char *insn);

int decode_call(unsigned char *insn);

int check_branch(unsigned char *from, unsigned char *to);

#endif
