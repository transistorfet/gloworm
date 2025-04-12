
#ifndef _ASM_M68K_EXCEPTIONS_H
#define _ASM_M68K_EXCEPTIONS_H

#include <stdint.h>

struct exception_frame {
	uint16_t status;
	uint32_t pc;

	#if !defined(CONFIG_MC68000)
	unsigned format: 4;
	unsigned vector: 12;
	union {
		struct {
			uint32_t instruction;
		} format2;
		struct {
			uint32_t instruction;
			uint16_t internal[4];
		} format9;
		struct {
			uint16_t internal0;
			uint16_t special;
			uint16_t pipe_c;
			uint16_t pipe_b;
			uint32_t fault_addr;
			uint16_t internal1[2];
			uint32_t data_out;
			uint16_t internal2[2];
		} formata;
		struct {
			uint16_t internal0;
			uint16_t special;
			uint16_t pipe_c;
			uint16_t pipe_b;
			uint32_t fault_addr;
			uint16_t internal3[2];
			uint32_t data_out;
			uint16_t internal4[4];
			uint32_t stage_b_addr;
			uint16_t internal5[2];
			uint32_t data_in;
			uint16_t internal6[4];
			uint16_t internal7[18];
		} formatb;
	};
	#endif
};

#endif

