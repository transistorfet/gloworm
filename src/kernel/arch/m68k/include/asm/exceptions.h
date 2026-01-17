
#ifndef _ASM_M68K_EXCEPTIONS_H
#define _ASM_M68K_EXCEPTIONS_H

#include <stdint.h>

#define SSW_FAULT_STAGE_C	0x8000
#define SSW_FAULT_STAGE_B	0x4000
#define SSW_RERUN_STAGE_C	0x2000
#define SSW_RERUN_STAGE_B	0x1000
#define SSW_DATA_FAULT		0x0100
#define SSW_READ_MODIFY_WRITE	0x0080
#define SSW_READ_WRITE		0x0040
#define SSW_SIZE		0x0030
#define SSW_FUNCTION_CODE	0x0007

#define SSW_RW_READ		SSW_READ_WRITE
#define SSW_RW_WRITE		0

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
			uint16_t ssw;
			uint16_t pipe_c;
			uint16_t pipe_b;
			uint32_t fault_addr;
			uint16_t internal1[2];
			uint32_t data_out;
			uint16_t internal2[2];
		} formata;
		struct {
			uint16_t internal0;
			uint16_t ssw;
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

