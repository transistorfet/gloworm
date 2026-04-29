
#ifndef _ASM_M68K_SETJMP_H
#define _ASM_M68K_SETJMP_H

#include <stdint.h>

#define JMP_BUF_LENGTH		2
#define JMP_BUF_RETURN_ADDR	0

 struct jmp_buf_data {
	uint32_t pc;
	//uint32_t d0;
	//uint32_t d1;
	//uint32_t a0;
	//uint32_t a1;
	uint32_t sp;
};

typedef uintptr_t jmp_buf[JMP_BUF_LENGTH];

int setjmp(jmp_buf env);
void longjmp(jmp_buf env, int val);

#endif

