#include <cstdio>
#include <cstring>
#include "disassembler.h"
#include "utils.h"
#include "opecode.h"

#ifdef __x86_64__

#define INSTR_2BYTE_0F 0x0f

//
// type definitions
//
typedef void (*code_parser_t)(opecode *op, uint8_t *code);

struct mod_rm_info_t {
	const bool sib;
	const opecode_disp_t disp_type;
};

struct instr_info {
	int length;
	code_parser_t parser;
	int prefix;
	int instr_2byte;
};

//
// static const instances definitions
//
static const mod_rm_info_t mod_rm_sib = {
	true,
	DISP_NONE,
};

static const mod_rm_info_t mod_rm_disp32 = {
	false,
	DISP32,
};

static const mod_rm_info_t mod_rm_ecx_disp8 = {
	false,
	DISP8,
};

static const mod_rm_info_t mod_rm_sib_disp8 = {
	true,
	DISP8,
};

static const mod_rm_info_t mod_rm_esi_disp8 = {
	false,
	DISP8,
};

static const mod_rm_info_t mod_rm_eax = {
	false,
	DISP_NONE,
};

static const mod_rm_info_t mod_rm_ecx = {
	false,
	DISP_NONE,
};

static const mod_rm_info_t mod_rm_edx = {
	false,
	DISP_NONE,
};

static const mod_rm_info_t mod_rm_ebx = {
	false,
	DISP_NONE,
};

static const mod_rm_info_t mod_rm_esp = {
	false,
	DISP_NONE,
};

static const mod_rm_info_t mod_rm_ebp = {
	false,
	DISP_NONE,
};

static const mod_rm_info_t mod_rm_esi = {
	false,
	DISP_NONE,
};

static const mod_rm_info_t mod_rm_edi = {
	false,
	DISP_NONE,
};

static const mod_rm_info_t *mod_rm_matrix[4][8] = 
{
	{NULL, NULL, NULL, NULL,
	 &mod_rm_sib, &mod_rm_disp32, NULL, NULL},

	{NULL, &mod_rm_ecx_disp8, NULL, NULL,
	 &mod_rm_sib_disp8, NULL, &mod_rm_esi_disp8, NULL},

	{NULL, NULL, NULL, NULL,
	 NULL, NULL, NULL, NULL},

	{&mod_rm_eax, &mod_rm_ecx, &mod_rm_edx, &mod_rm_ebx,
	 &mod_rm_esp, &mod_rm_ebp, &mod_rm_esi, &mod_rm_edi},
};

//
// private functions
//
static uint8_t parse_immediate8(uint8_t *code, opecode *op)
{
	op->inc_length(1);
	return *code;
}

static uint32_t parse_immediate32(uint8_t *code, opecode *op)
{
	op->inc_length(4);
	return *((uint32_t *)code);
}


static void parse_sib(uint8_t sib, opecode *op)
{
	int ss    = (sib & 0xc0) >> 6;
	int index = (sib & 0x38) >> 3;
	int base  = (sib & 0x07);
	op->set_sib_param(ss, index, base);
	op->inc_length();
}

static int parse_disp(opecode_disp_t disp_type, uint8_t *addr, opecode *op)
{
	int length = 0;
	if (disp_type == DISP8) {
		op->set_disp(disp_type, *addr, addr);
		length = 1;
	} else if (disp_type == DISP32) {
		op->set_disp(disp_type, *((uint32_t*)addr), addr);
		length = 4;
	}
	else
		ROACH_BUG("Unknown disp type: %d\n", disp_type);
	op->inc_length(length);
	return length;
}

static const mod_rm_info_t *parse_mod_rm(uint8_t mod_rm, opecode *op)
{
	int mod = (mod_rm & 0xc0) >> 6;
	int reg = (mod_rm & 0x38) >> 3;
	int r_m = (mod_rm & 0x07);
	const mod_rm_info_t *mod_rm_info = mod_rm_matrix[mod][r_m];
	if (mod_rm_info == NULL)
		ROACH_BUG("mod_rm: mod: %d, r_m: %d, NULL (not implemented)\n",
		          mod, r_m);
	op->set_mod_rm(mod, reg, r_m);
	op->inc_length();
	return mod_rm_info;
}

static uint8_t *parse_operand(opecode *op, uint8_t *code)
{
	const mod_rm_info_t *mod_rm_info = parse_mod_rm(*code, op);
	code++;
	if (mod_rm_info->sib) {
		parse_sib(*code, op);
		code++;
	}
	if (mod_rm_info->disp_type != DISP_NONE)
		code += parse_disp(mod_rm_info->disp_type, code, op);
	return code;
}

// 0x01 ADD (MR)
static void parser_add_Ev_Gv(opecode *op, uint8_t *code)
{
	parse_operand(op, code);
}

static const instr_info instr_info_add_Ev_Gv = {
	1,
	parser_add_Ev_Gv,
};

// 0x0f 2byte escape
static const instr_info instr_info_2byte_escape = {
	1,
	NULL,
	0,
	INSTR_2BYTE_0F,
};

// 0x31 XOR (MR)
static void parser_xor_Eb_Gb(opecode *op, uint8_t *code)
{
	parse_operand(op, code);
}

static const instr_info instr_info_xor_Eb_Gb = {
	1,
	parser_xor_Eb_Gb,
};

// 0x41 REX.B Preifx
static const instr_info instr_info_rex_b = {
	1,
	NULL,
	PREFIX_REX_B,
};

// 0x48 REX.W Prefix
static const instr_info instr_info_rex_w = {
	1,
	NULL,
	PREFIX_REX_W,
};

// 0x50 PUSH
static void parser_push_rAXr8(opecode *op, uint8_t *code)
{
	// no operand
}

static const instr_info instr_info_push_rAXr8 = {
	1,
	parser_push_rAXr8,
};

// 0x53 PUSH
static void parser_push_rBXr11(opecode *op, uint8_t *code)
{
	// no operand
}

static const instr_info instr_info_push_rBXr11 = {
	1,
	parser_push_rBXr11,
};

// 0x54 PUSH
static void parser_push_rSPr12(opecode *op, uint8_t *code)
{
	// no operand
}

static const instr_info instr_info_push_rSPr12 = {
	1,
	parser_push_rSPr12,
};

// 0x55 PUSH
static void parser_push_rBPr13(opecode *op, uint8_t *code)
{
	// no operand
}

static const instr_info instr_info_push_rBPr13 = {
	1,
	parser_push_rBPr13,
};

// 0x56 PUSH
static void parser_push_rSIr14(opecode *op, uint8_t *code)
{
	// no operand
}

static const instr_info instr_info_push_rSIr14 = {
	1,
	parser_push_rSIr14,
};

// 0x58 POP
static void parser_pop_rAXr8(opecode *op, uint8_t *code)
{
	// no operand
}

static const instr_info instr_info_pop_rAXr8 = {
	1,
	parser_pop_rAXr8,
};

// 0x7e Jcc, jb- Short displacement jump on condition
static void parser_jcc_LE_NG(opecode *op, uint8_t *code)
{
	op->set_rel_jump_addr(REL8, *code);
	op->inc_length(1);
}

static const instr_info instr_info_jcc_LE_NG  = {
	1,
	parser_jcc_LE_NG,
};

// 0x80 Immediate Group 1 (MI): sub, cmp
static void parser_imm_grp1_Eb_Ib(opecode *op, uint8_t *code)
{
	code = parse_operand(op, code);
	uint8_t imm = parse_immediate8(code, op);
	op->set_immediate(IMM8, imm);
	code++;
}

static const instr_info instr_info_imm_grp1_Eb_Ib = {
	1,
	parser_imm_grp1_Eb_Ib,
};

// 0x81 Immediate Group 1 (MI): sub, cmp
static void parser_imm_grp1_Ev_Iz(opecode *op, uint8_t *code)
{
	code = parse_operand(op, code);
	uint8_t imm = parse_immediate32(code, op);
	op->set_immediate(IMM32, imm);
	code += 4;
}

static const instr_info instr_info_imm_grp1_Ev_Iz = {
	1,
	parser_imm_grp1_Ev_Iz,
};

// 0x83 Immediate Group 1 (MI): sub, cmp
static void parser_imm_grp1_Ev_Ib(opecode *op, uint8_t *code)
{
	code = parse_operand(op, code);
	uint8_t imm = parse_immediate8(code, op);
	op->set_immediate(IMM8, imm);
	code++;
}

static const instr_info instr_info_imm_grp1_Ev_Ib = {
	1,
	parser_imm_grp1_Ev_Ib,
};

// 0x85 TEST (MR)
static void parser_test_Ev_Gv(opecode *op, uint8_t *code)
{
	parse_operand(op, code);
}

static const instr_info instr_info_test_Ev_Gv = {
	1,
	parser_test_Ev_Gv,
};

// 0x89 MOV (MR)
static void parser_mov_Ev_Gv(opecode *op, uint8_t *code)
{
	parse_operand(op, code);
}

static const instr_info instr_info_mov_Ev_Gv = {
	1,
	parser_mov_Ev_Gv,
};

// 0x8b MOV (RM)
static void parser_mov_Gv_Ev(opecode *op, uint8_t *code)
{
	parse_operand(op, code);
}

static const instr_info instr_info_mov_Gv_Ev = {
	1,
	parser_mov_Gv_Ev,
};

// 0x8d LEA (RM)
static void parser_lea_Gv_M(opecode *op, uint8_t *code)
{
	code = parse_operand(op, code);
}

static const instr_info instr_info_lea_Gv_M = {
	1,
	parser_lea_Gv_M,
};

// 0xbe MOV
static void parser_mov_rSIr14_Iv(opecode *op, uint8_t *code)
{
	uint32_t imm = parse_immediate32(code, op);
	op->set_immediate(IMM32, imm);
}

static const instr_info instr_info_mov_rSIr14_Iv = {
	1,
	parser_mov_rSIr14_Iv,
};

// 0xc3 RET
static void parser_not_impl(opecode *op, uint8_t *code)
{
	// nothing to do
}

static const instr_info instr_info_ret = {
	1,
	parser_not_impl,
};

static const instr_info *first_byte_instr_array[0x100] = 
{
	NULL,                         // 0x00
	&instr_info_add_Ev_Gv,        // 0x01
	NULL,                         // 0x02
	NULL,                         // 0x03
	NULL,                         // 0x04
	NULL,                         // 0x05
	NULL,                         // 0x06
	NULL,                         // 0x07
	NULL,                         // 0x08
	NULL,                         // 0x09
	NULL,                         // 0x0a
	NULL,                         // 0x0b
	NULL,                         // 0x0c
	NULL,                         // 0x0d
	NULL,                         // 0x0e
	&instr_info_2byte_escape,     // 0x0f

	NULL,                         // 0x10
	NULL,                         // 0x11
	NULL,                         // 0x12
	NULL,                         // 0x13
	NULL,                         // 0x14
	NULL,                         // 0x15
	NULL,                         // 0x16
	NULL,                         // 0x17
	NULL,                         // 0x18
	NULL,                         // 0x19
	NULL,                         // 0x1a
	NULL,                         // 0x1b
	NULL,                         // 0x1c
	NULL,                         // 0x1d
	NULL,                         // 0x1e
	NULL,                         // 0x1f

	NULL,                         // 0x20
	NULL,                         // 0x21
	NULL,                         // 0x22
	NULL,                         // 0x23
	NULL,                         // 0x24
	NULL,                         // 0x25
	NULL,                         // 0x26
	NULL,                         // 0x27
	NULL,                         // 0x28
	NULL,                         // 0x29
	NULL,                         // 0x2a
	NULL,                         // 0x2b
	NULL,                         // 0x2c
	NULL,                         // 0x2d
	NULL,                         // 0x2e
	NULL,                         // 0x2f

	NULL,                         // 0x30
	&instr_info_xor_Eb_Gb,        // 0x31
	NULL,                         // 0x32
	NULL,                         // 0x33
	NULL,                         // 0x34
	NULL,                         // 0x35
	NULL,                         // 0x36
	NULL,                         // 0x37
	NULL,                         // 0x38
	NULL,                         // 0x39
	NULL,                         // 0x3a
	NULL,                         // 0x3b
	NULL,                         // 0x3c
	NULL,                         // 0x3d
	NULL,                         // 0x3e
	NULL,                         // 0x3f

	NULL,                         // 0x40
	&instr_info_rex_b,            // 0x41
	NULL,                         // 0x42
	NULL,                         // 0x43
	NULL,                         // 0x44
	NULL,                         // 0x45
	NULL,                         // 0x46
	NULL,                         // 0x47
	&instr_info_rex_w,            // 0x48
	NULL,                         // 0x49
	NULL,                         // 0x4a
	NULL,                         // 0x4b
	NULL,                         // 0x4c
	NULL,                         // 0x4d
	NULL,                         // 0x4e
	NULL,                         // 0x4f

	&instr_info_push_rAXr8,       // 0x50
	NULL,                         // 0x51
	NULL,                         // 0x52
	&instr_info_push_rBXr11,      // 0x53
	&instr_info_push_rSPr12,      // 0x54
	&instr_info_push_rBPr13,      // 0x55
	&instr_info_push_rSIr14,      // 0x56
	NULL,                         // 0x57
	&instr_info_pop_rAXr8,        // 0x58
	NULL,                         // 0x59
	NULL,                         // 0x5a
	NULL,                         // 0x5b
	NULL,                         // 0x5c
	NULL,                         // 0x5d
	NULL,                         // 0x5e
	NULL,                         // 0x5f

	NULL,                         // 0x60
	NULL,                         // 0x61
	NULL,                         // 0x62
	NULL,                         // 0x63
	NULL,                         // 0x64
	NULL,                         // 0x65
	NULL,                         // 0x66
	NULL,                         // 0x67
	NULL,                         // 0x68
	NULL,                         // 0x69
	NULL,                         // 0x6a
	NULL,                         // 0x6b
	NULL,                         // 0x6c
	NULL,                         // 0x6d
	NULL,                         // 0x6e
	NULL,                         // 0x6f

	NULL,                         // 0x70
	NULL,                         // 0x71
	NULL,                         // 0x72
	NULL,                         // 0x73
	NULL,                         // 0x74
	NULL,                         // 0x75
	NULL,                         // 0x76
	NULL,                         // 0x77
	NULL,                         // 0x78
	NULL,                         // 0x79
	NULL,                         // 0x7a
	NULL,                         // 0x7b
	NULL,                         // 0x7c
	NULL,                         // 0x7d
	&instr_info_jcc_LE_NG,        // 0x7e
	NULL,                         // 0x7f

	&instr_info_imm_grp1_Eb_Ib,   // 0x80
	&instr_info_imm_grp1_Ev_Iz,   // 0x81
	NULL,                         // 0x82
	&instr_info_imm_grp1_Ev_Ib,   // 0x83
	NULL,                         // 0x84
	&instr_info_test_Ev_Gv,       // 0x85
	NULL,                         // 0x86
	NULL,                         // 0x87
	NULL,                         // 0x88
	&instr_info_mov_Ev_Gv,        // 0x89
	NULL,                         // 0x8a
	&instr_info_mov_Gv_Ev,        // 0x8b
	NULL,                         // 0x8c
	&instr_info_lea_Gv_M,         // 0x8d
	NULL,                         // 0x8e
	NULL,                         // 0x8f

	NULL,                         // 0x90
	NULL,                         // 0x91
	NULL,                         // 0x92
	NULL,                         // 0x93
	NULL,                         // 0x94
	NULL,                         // 0x95
	NULL,                         // 0x96
	NULL,                         // 0x97
	NULL,                         // 0x98
	NULL,                         // 0x99
	NULL,                         // 0x9a
	NULL,                         // 0x9b
	NULL,                         // 0x9c
	NULL,                         // 0x9d
	NULL,                         // 0x9e
	NULL,                         // 0x9f
/*
	&instr_info_pushf,            // 0x9c
	&instr_info_popf,             // 0x9d
*/
	NULL,                         // 0xa0
	NULL,                         // 0xa1
	NULL,                         // 0xa2
	NULL,                         // 0xa3
	NULL,                         // 0xa4
	NULL,                         // 0xa5
	NULL,                         // 0xa6
	NULL,                         // 0xa7
	NULL,                         // 0xa8
	NULL,                         // 0xa9
	NULL,                         // 0xaa
	NULL,                         // 0xab
	NULL,                         // 0xac
	NULL,                         // 0xad
	NULL,                         // 0xae
	NULL,                         // 0xaf

	NULL,                         // 0xb0
	NULL,                         // 0xb1
	NULL,                         // 0xb2
	NULL,                         // 0xb3
	NULL,                         // 0xb4
	NULL,                         // 0xb5
	NULL,                         // 0xb6
	NULL,                         // 0xb7
	NULL,                         // 0xb8
	NULL,                         // 0xb9
	NULL,                         // 0xba
	NULL,                         // 0xbb
	NULL,                         // 0xbc
	NULL,                         // 0xbd
	&instr_info_mov_rSIr14_Iv,    // 0xbe
	NULL,                         // 0xbf

	NULL,                         // 0xc0
	NULL,                         // 0xc1
	NULL,                         // 0xc2
	&instr_info_ret,              // 0xc3
	NULL,                         // 0xc4
	NULL,                         // 0xc5
	NULL,                         // 0xc6
	NULL,                         // 0xc7
	NULL,                         // 0xc8
	NULL,                         // 0xc9
	NULL,                         // 0xca
	NULL,                         // 0xcb
	NULL,                         // 0xcc
	NULL,                         // 0xcd
	NULL,                         // 0xce
	NULL,                         // 0xcf

	NULL,                         // 0xd1
	NULL,                         // 0xd2
	NULL,                         // 0xd3
	NULL,                         // 0xd4
	NULL,                         // 0xd5
	NULL,                         // 0xd6
	NULL,                         // 0xd7
	NULL,                         // 0xd8
	NULL,                         // 0xd9
	NULL,                         // 0xda
	NULL,                         // 0xdb
	NULL,                         // 0xdc
	NULL,                         // 0xdd
	NULL,                         // 0xde
	NULL,                         // 0xdf

	NULL,                         // 0xe0
	NULL,                         // 0xe0
	NULL,                         // 0xe0
	NULL,                         // 0xe1
	NULL,                         // 0xe2
	NULL,                         // 0xe3
	NULL,                         // 0xe4
	NULL,                         // 0xe5
	NULL,                         // 0xe6
	NULL,                         // 0xe7
	NULL,                         // 0xe8
	NULL,                         // 0xe9
	NULL,                         // 0xea
	NULL,                         // 0xeb
	NULL,                         // 0xec
	NULL,                         // 0xed
	NULL,                         // 0xee
	NULL,                         // 0xef

	NULL,                         // 0xf1
	NULL,                         // 0xf2
	NULL,                         // 0xf3
	NULL,                         // 0xf4
	NULL,                         // 0xf5
	NULL,                         // 0xf6
	NULL,                         // 0xf7
	NULL,                         // 0xf8
	NULL,                         // 0xf9
	NULL,                         // 0xfa
	NULL,                         // 0xfb
	NULL,                         // 0xfc
	NULL,                         // 0xfd
	NULL,                         // 0xfe
	NULL,                         // 0xff
};

//
// 2byte instructions
//

// 0xaf IMUL
static void parser_imul_Gv_Ev(opecode *op, uint8_t *code)
{
	code = parse_operand(op, code);
}

static const instr_info instr_info_imul_Gv_Ev = {
	1,
	parser_imul_Gv_Ev,
};

static const instr_info *second_byte_instr_array_0f[0x100] = 
{
	NULL,                         // 0x00
	NULL,                         // 0x01
	NULL,                         // 0x02
	NULL,                         // 0x03
	NULL,                         // 0x04
	NULL,                         // 0x05
	NULL,                         // 0x06
	NULL,                         // 0x07
	NULL,                         // 0x08
	NULL,                         // 0x09
	NULL,                         // 0x0a
	NULL,                         // 0x0b
	NULL,                         // 0x0c
	NULL,                         // 0x0d
	NULL,                         // 0x0e
	NULL,                         // 0x0f

	NULL,                         // 0x10
	NULL,                         // 0x11
	NULL,                         // 0x12
	NULL,                         // 0x13
	NULL,                         // 0x14
	NULL,                         // 0x15
	NULL,                         // 0x16
	NULL,                         // 0x17
	NULL,                         // 0x18
	NULL,                         // 0x19
	NULL,                         // 0x1a
	NULL,                         // 0x1b
	NULL,                         // 0x1c
	NULL,                         // 0x1d
	NULL,                         // 0x1e
	NULL,                         // 0x1f

	NULL,                         // 0x20
	NULL,                         // 0x21
	NULL,                         // 0x22
	NULL,                         // 0x23
	NULL,                         // 0x24
	NULL,                         // 0x25
	NULL,                         // 0x26
	NULL,                         // 0x27
	NULL,                         // 0x28
	NULL,                         // 0x29
	NULL,                         // 0x2a
	NULL,                         // 0x2b
	NULL,                         // 0x2c
	NULL,                         // 0x2d
	NULL,                         // 0x2e
	NULL,                         // 0x2f

	NULL,                         // 0x30
	NULL,                         // 0x31
	NULL,                         // 0x32
	NULL,                         // 0x33
	NULL,                         // 0x34
	NULL,                         // 0x35
	NULL,                         // 0x36
	NULL,                         // 0x37
	NULL,                         // 0x38
	NULL,                         // 0x39
	NULL,                         // 0x3a
	NULL,                         // 0x3b
	NULL,                         // 0x3c
	NULL,                         // 0x3d
	NULL,                         // 0x3e
	NULL,                         // 0x3f

	NULL,                         // 0x40
	NULL,                         // 0x41
	NULL,                         // 0x42
	NULL,                         // 0x43
	NULL,                         // 0x44
	NULL,                         // 0x45
	NULL,                         // 0x46
	NULL,                         // 0x47
	NULL,                         // 0x48
	NULL,                         // 0x49
	NULL,                         // 0x4a
	NULL,                         // 0x4b
	NULL,                         // 0x4c
	NULL,                         // 0x4d
	NULL,                         // 0x4e
	NULL,                         // 0x4f

	NULL,                         // 0x50
	NULL,                         // 0x51
	NULL,                         // 0x52
	NULL,                         // 0x53
	NULL,                         // 0x54
	NULL,                         // 0x55
	NULL,                         // 0x56
	NULL,                         // 0x57
	NULL,                         // 0x58
	NULL,                         // 0x59
	NULL,                         // 0x5a
	NULL,                         // 0x5b
	NULL,                         // 0x5c
	NULL,                         // 0x5d
	NULL,                         // 0x5e
	NULL,                         // 0x5f

	NULL,                         // 0x60
	NULL,                         // 0x61
	NULL,                         // 0x62
	NULL,                         // 0x63
	NULL,                         // 0x64
	NULL,                         // 0x65
	NULL,                         // 0x66
	NULL,                         // 0x67
	NULL,                         // 0x68
	NULL,                         // 0x69
	NULL,                         // 0x6a
	NULL,                         // 0x6b
	NULL,                         // 0x6c
	NULL,                         // 0x6d
	NULL,                         // 0x6e
	NULL,                         // 0x6f

	NULL,                         // 0x70
	NULL,                         // 0x71
	NULL,                         // 0x72
	NULL,                         // 0x73
	NULL,                         // 0x74
	NULL,                         // 0x75
	NULL,                         // 0x76
	NULL,                         // 0x77
	NULL,                         // 0x78
	NULL,                         // 0x79
	NULL,                         // 0x7a
	NULL,                         // 0x7b
	NULL,                         // 0x7c
	NULL,                         // 0x7d
	NULL,                         // 0x7e
	NULL,                         // 0x7f

	NULL,                         // 0x80
	NULL,                         // 0x81
	NULL,                         // 0x82
	NULL,                         // 0x83
	NULL,                         // 0x84
	NULL,                         // 0x85
	NULL,                         // 0x86
	NULL,                         // 0x87
	NULL,                         // 0x88
	NULL,                         // 0x89
	NULL,                         // 0x8a
	NULL,                         // 0x8b
	NULL,                         // 0x8c
	NULL,                         // 0x8d
	NULL,                         // 0x8e
	NULL,                         // 0x8f

	NULL,                         // 0x90
	NULL,                         // 0x91
	NULL,                         // 0x92
	NULL,                         // 0x93
	NULL,                         // 0x94
	NULL,                         // 0x95
	NULL,                         // 0x96
	NULL,                         // 0x97
	NULL,                         // 0x98
	NULL,                         // 0x99
	NULL,                         // 0x9a
	NULL,                         // 0x9b
	NULL,                         // 0x9c
	NULL,                         // 0x9d
	NULL,                         // 0x9e
	NULL,                         // 0x9f

	NULL,                         // 0xa0
	NULL,                         // 0xa1
	NULL,                         // 0xa2
	NULL,                         // 0xa3
	NULL,                         // 0xa4
	NULL,                         // 0xa5
	NULL,                         // 0xa6
	NULL,                         // 0xa7
	NULL,                         // 0xa8
	NULL,                         // 0xa9
	NULL,                         // 0xaa
	NULL,                         // 0xab
	NULL,                         // 0xac
	NULL,                         // 0xad
	NULL,                         // 0xae
	&instr_info_imul_Gv_Ev,       // 0xaf

	NULL,                         // 0xb0
	NULL,                         // 0xb1
	NULL,                         // 0xb2
	NULL,                         // 0xb3
	NULL,                         // 0xb4
	NULL,                         // 0xb5
	NULL,                         // 0xb6
	NULL,                         // 0xb7
	NULL,                         // 0xb8
	NULL,                         // 0xb9
	NULL,                         // 0xba
	NULL,                         // 0xbb
	NULL,                         // 0xbc
	NULL,                         // 0xbd
	NULL,                         // 0xbe
	NULL,                         // 0xbf

	NULL,                         // 0xc0
	NULL,                         // 0xc1
	NULL,                         // 0xc2
	NULL,                         // 0xc3
	NULL,                         // 0xc4
	NULL,                         // 0xc5
	NULL,                         // 0xc6
	NULL,                         // 0xc7
	NULL,                         // 0xc8
	NULL,                         // 0xc9
	NULL,                         // 0xca
	NULL,                         // 0xcb
	NULL,                         // 0xcc
	NULL,                         // 0xcd
	NULL,                         // 0xce
	NULL,                         // 0xcf

	NULL,                         // 0xd1
	NULL,                         // 0xd2
	NULL,                         // 0xd3
	NULL,                         // 0xd4
	NULL,                         // 0xd5
	NULL,                         // 0xd6
	NULL,                         // 0xd7
	NULL,                         // 0xd8
	NULL,                         // 0xd9
	NULL,                         // 0xda
	NULL,                         // 0xdb
	NULL,                         // 0xdc
	NULL,                         // 0xdd
	NULL,                         // 0xde
	NULL,                         // 0xdf

	NULL,                         // 0xe0
	NULL,                         // 0xe0
	NULL,                         // 0xe0
	NULL,                         // 0xe1
	NULL,                         // 0xe2
	NULL,                         // 0xe3
	NULL,                         // 0xe4
	NULL,                         // 0xe5
	NULL,                         // 0xe6
	NULL,                         // 0xe7
	NULL,                         // 0xe8
	NULL,                         // 0xe9
	NULL,                         // 0xea
	NULL,                         // 0xeb
	NULL,                         // 0xec
	NULL,                         // 0xed
	NULL,                         // 0xee
	NULL,                         // 0xef

	NULL,                         // 0xf1
	NULL,                         // 0xf2
	NULL,                         // 0xf3
	NULL,                         // 0xf4
	NULL,                         // 0xf5
	NULL,                         // 0xf6
	NULL,                         // 0xf7
	NULL,                         // 0xf8
	NULL,                         // 0xf9
	NULL,                         // 0xfa
	NULL,                         // 0xfb
	NULL,                         // 0xfc
	NULL,                         // 0xfd
	NULL,                         // 0xfe
	NULL,                         // 0xff
};

#endif // __x86_64__

// --------------------------------------------------------------------------
// private functions
// --------------------------------------------------------------------------

// --------------------------------------------------------------------------
// public functions
// --------------------------------------------------------------------------
opecode *disassembler::parse(uint8_t *code_start)
{
	uint8_t *code = code_start;
	ROACH_DBG("BEGIN: %p\n", code_start);
	opecode *op = new opecode(code_start);
	int instr_2byte = 0;
	while (true) {
		const instr_info *instr = first_byte_instr_array[*code];
		if (instr_2byte == INSTR_2BYTE_0F)
			instr = second_byte_instr_array_0f[*code];
		if (instr == NULL) {
			ROACH_ERR("Failed to parse code byte: %p: %02x, "
			          "instr_2byte: %02x\n",
			          code, *code, instr_2byte);
			ROACH_ABORT();
		}
		instr_2byte = 0;
		code += instr->length;
		op->inc_length(instr->length);

		if (instr->parser)
			(*instr->parser)(op, code);
		if (instr->instr_2byte) {
			instr_2byte = instr->instr_2byte;
			continue;
		}

		// If the first byte is prefix, we parse again
		if (instr->prefix) {
			op->add_prefix(instr->prefix);
			continue;
		}
		break;
	}
	op->copy_code(code_start);
	ROACH_DBG("END: length: %d\n", op->get_length());
	return op;
}

