#include "decode.h"

/*
 * decode a return instruction, optionally prefixed by rep.
 * return the length of the instruction, or 0 if no ret can
 * be decoded.
 */
int decode_ret(unsigned char *insn)
{
	unsigned int idx=0, ilen=0;

	//skip rep prefix
	if (insn[idx] == 0xf3)
		idx++;

	switch (insn[idx]) {
		case 0xc3: // near ret
		case 0xcb: // far ret
			ilen = idx + 1;
			break;
		case 0xc2: // near ret imm16
		case 0xca: // far ret imm16
			ilen = idx + 1 + 2;
			break;
	}

	return ilen;
}


/*
 * decode a call instruction, optionally prefixed.
 * return the length of the instruction, or 0 if no
 * call can be decoded.
 */
int decode_call(unsigned char *insn)
{
	unsigned char word, reg, rm, mod, base;
	unsigned int idx=0, ilen=0;

	// parse prefix

	word = 4; // or 2 if in 16-bit mode

	switch (insn[idx]) {

		/* call near relative */
		case 0xe8:
			// rel16/32 (16/32/64-bit)
			ilen = idx + 1 + word;
			break;

		/* reg == 2 -> call near absolute indirect r/m16/32/64
		 * reg == 3 -> call far absolute indirect m16:16/32/64 */
		case 0xff:
			idx++; //there's a modrm byte
			reg = (insn[idx] >> 3) & 0x7; //opcode extension
			rm = insn[idx] & 0x7;
			mod = insn[idx] >> 6;

			// disp16 (special case in 16-bit)
			if (mod == 0 && word == 2 && rm == 6)
				ilen = idx + 1 + word;
			// ([ip]+)disp32 (special case in 16/32-bit)
			else if (mod == 0 && word == 4 && rm == 5)
				ilen = idx + 1 + word;
			// [rm] (16/32/64-bit)
			else if (mod == 0)
				ilen = idx + 1;
			// [rm]+disp8 (16/32/64-bit)
			else if (mod == 1)
				ilen = idx + 1 + 1;
			// [rm]+disp16/32 (16/32/64-bit)
			else if (mod == 2)
				ilen = idx + 1 + word;
			// rm (16/32/64-bit only for call near
			else if (mod == 3 && reg == 2) 
				ilen = idx + 1;
			// sim byte (32/64-bit)
			if (ilen && rm == 4 && word == 4 && mod != 3) {
				ilen += 1;
				// check sib special cases
				base = insn[idx+1] & 0x7;
				// base is disp32 (0)
				if (base == 5 && mod == 0)// || mod == 2))
					ilen += word;
			}

			break;

		/* call far absolute static -- invalid in 64-bit! */
		case 0x9a:
			// ptr16:16/32 (16/32-bit)
			ilen = idx + 1 + 2 + word;
			break;
	}

	return ilen;
}

int check_branch(unsigned char *from, unsigned char *to)
{
	int i, matches;

	if (decode_ret(from) == 0)
		return 0; //TODO: log!

	for (i=8, matches=0; i < MAX_INSN - 1; i++)
		if (decode_call(to - MAX_INSN + i) == MAX_INSN - i)
			matches++;

	return (matches == 0);
}
