/*
 * encoder.c --- Format-specific and mnemonic-driven instruction encoding.
 *
 * Instruction shapes (all 16-bit):
 *   R: [op5][rd3][rs1_3][rs2_3][func2]
 *   I: [op5][rd3][rs1_3][imm5]
 *   J: [op5][offset11]            (PC-relative, signed)
 *
 * Mnemonic → operand shape mapping lives in `op_defs[]` below.
 */
#include "encoder.h"
#include "errors.h"
#include "isa.h"

#include <stdbool.h>
#include <stddef.h>
#include <string.h>
#include <strings.h>

/* --- Low-level encoders ---------------------------------------------- */

int encode_R(uint8_t op, uint8_t rd, uint8_t rs1, uint8_t rs2,
             uint16_t *out) {
    if (op  > 0x1F) return ERR_INTERNAL;
    if (rd  >= NUM_REGS || rs1 >= NUM_REGS || rs2 >= NUM_REGS)
        return ERR_INVALID_REGISTER;
    uint16_t w = (uint16_t)(((uint16_t)op  & 0x1F) << 11);
    w |=        (uint16_t)(((uint16_t)rd  & 0x07) << 8);
    w |=        (uint16_t)(((uint16_t)rs1 & 0x07) << 5);
    w |=        (uint16_t)(((uint16_t)rs2 & 0x07) << 2);
    /* func2 = 0 for all current ops. */
    *out = w;
    return ERR_OK;
}

int encode_I(uint8_t op, uint8_t rd, uint8_t rs1, int32_t imm,
             uint16_t *out) {
    if (op  > 0x1F) return ERR_INTERNAL;
    if (rd  >= NUM_REGS || rs1 >= NUM_REGS) return ERR_INVALID_REGISTER;
    if (imm < IMM5_MIN || imm > IMM5_MAX) return ERR_IMM_OVERFLOW;
    uint16_t w = (uint16_t)(((uint16_t)op  & 0x1F) << 11);
    w |=        (uint16_t)(((uint16_t)rd  & 0x07) << 8);
    w |=        (uint16_t)(((uint16_t)rs1 & 0x07) << 5);
    w |=        (uint16_t)((uint16_t)imm & 0x1F);
    *out = w;
    return ERR_OK;
}

int encode_J(uint8_t op, int32_t offset, uint16_t *out) {
    if (op > 0x1F) return ERR_INTERNAL;
    if (offset < JUMP_MIN || offset > JUMP_MAX) return ERR_JUMP_OVERFLOW;
    uint16_t w = (uint16_t)(((uint16_t)op & 0x1F) << 11);
    w |=        (uint16_t)((uint16_t)offset & 0x7FF);
    *out = w;
    return ERR_OK;
}

/* --- Mnemonic table -------------------------------------------------- */

typedef enum {
    FMT_R3,     /* Rd, Rs1, Rs2   (ADD, SUB, AND, OR, XOR, SHL, SHR) */
    FMT_R2,     /* Rd, Rs1        (NOT, CMP-as-R variant) */
    FMT_R_CMP,  /* Rs1, Rs2       (CMP) */
    FMT_I3,     /* Rd, Rs1, imm5  (ADDI, LW, SW) */
    FMT_I_MOV,  /* Rd, imm5       (MOV)  — rs1 forced to 0 */
    FMT_J,      /* label/imm      (JMP, JEQ, JNE, JLT, JGT, CALL) */
    FMT_NONE    /* no operands    (RET, HALT) */
} Fmt;

typedef struct {
    const char *name;
    uint8_t     opcode;
    Fmt         fmt;
} OpDef;

static const OpDef op_defs[] = {
    { "ADD",  OP_ADD,  FMT_R3   },
    { "SUB",  OP_SUB,  FMT_R3   },
    { "AND",  OP_AND,  FMT_R3   },
    { "OR",   OP_OR,   FMT_R3   },
    { "XOR",  OP_XOR,  FMT_R3   },
    { "NOT",  OP_NOT,  FMT_R2   },
    { "SHL",  OP_SHL,  FMT_R3   },
    { "SHR",  OP_SHR,  FMT_R3   },
    { "ADDI", OP_ADDI, FMT_I3   },
    { "LW",   OP_LW,   FMT_I3   },
    { "SW",   OP_SW,   FMT_I3   },
    { "MOV",  OP_MOV,  FMT_I_MOV},
    { "CMP",  OP_CMP,  FMT_R_CMP},
    { "JMP",  OP_JMP,  FMT_J    },
    { "JEQ",  OP_JEQ,  FMT_J    },
    { "JNE",  OP_JNE,  FMT_J    },
    { "JLT",  OP_JLT,  FMT_J    },
    { "JGT",  OP_JGT,  FMT_J    },
    { "CALL", OP_CALL, FMT_J    },
    { "RET",  OP_RET,  FMT_NONE },
    { "HALT", OP_HALT, FMT_NONE },
};
static const size_t NUM_OPS = sizeof(op_defs) / sizeof(op_defs[0]);

static const OpDef *find_op(const char *mnemonic) {
    for (size_t i = 0; i < NUM_OPS; i++) {
        if (strcasecmp(op_defs[i].name, mnemonic) == 0) return &op_defs[i];
    }
    return NULL;
}

bool is_known_mnemonic(const char *mnemonic) {
    return find_op(mnemonic) != NULL;
}

/* --- Operand accessors ---------------------------------------------- */

static int want_reg(const Operand *o, uint8_t *out) {
    if (o->kind != OPD_REG) return ERR_INVALID_REGISTER;
    *out = o->reg;
    return ERR_OK;
}

static int want_imm(const Operand *o, int32_t *out) {
    if (o->kind != OPD_IMM) return ERR_INVALID_NUMBER;
    *out = o->imm;
    return ERR_OK;
}

/* --- High-level encode ---------------------------------------------- */

int encode_instr(const Instr *in, uint16_t pc, uint16_t *out) {
    const OpDef *def = find_op(in->mnemonic);
    if (!def) return ERR_UNKNOWN_MNEMONIC;

    int rc;
    uint8_t rd, rs1, rs2;
    int32_t imm;

    switch (def->fmt) {
    case FMT_R3:
        if (in->nops != 3) return ERR_WRONG_OPERAND_COUNT;
        if ((rc = want_reg(&in->ops[0], &rd))  != ERR_OK) return rc;
        if ((rc = want_reg(&in->ops[1], &rs1)) != ERR_OK) return rc;
        if ((rc = want_reg(&in->ops[2], &rs2)) != ERR_OK) return rc;
        return encode_R(def->opcode, rd, rs1, rs2, out);

    case FMT_R2:
        if (in->nops != 2) return ERR_WRONG_OPERAND_COUNT;
        if ((rc = want_reg(&in->ops[0], &rd))  != ERR_OK) return rc;
        if ((rc = want_reg(&in->ops[1], &rs1)) != ERR_OK) return rc;
        return encode_R(def->opcode, rd, rs1, 0, out);

    case FMT_R_CMP:
        if (in->nops != 2) return ERR_WRONG_OPERAND_COUNT;
        if ((rc = want_reg(&in->ops[0], &rs1)) != ERR_OK) return rc;
        if ((rc = want_reg(&in->ops[1], &rs2)) != ERR_OK) return rc;
        /* CMP has no destination; rd field = 0. */
        return encode_R(def->opcode, 0, rs1, rs2, out);

    case FMT_I3:
        if (in->nops != 3) return ERR_WRONG_OPERAND_COUNT;
        if ((rc = want_reg(&in->ops[0], &rd))  != ERR_OK) return rc;
        if ((rc = want_reg(&in->ops[1], &rs1)) != ERR_OK) return rc;
        if ((rc = want_imm(&in->ops[2], &imm)) != ERR_OK) return rc;
        return encode_I(def->opcode, rd, rs1, imm, out);

    case FMT_I_MOV:
        if (in->nops != 2) return ERR_WRONG_OPERAND_COUNT;
        if ((rc = want_reg(&in->ops[0], &rd))  != ERR_OK) return rc;
        if ((rc = want_imm(&in->ops[1], &imm)) != ERR_OK) return rc;
        /* MOV: rd = imm (zero-extended). Encoded as I-format with rs1=0. */
        return encode_I(def->opcode, rd, 0, imm, out);

    case FMT_J: {
        if (in->nops != 1) return ERR_WRONG_OPERAND_COUNT;
        int32_t target_or_offset;
        if (in->ops[0].kind == OPD_IMM) {
            /* Bare number: treat as absolute target; compute PC-relative. */
            target_or_offset = in->ops[0].imm - (int32_t)(pc + 1);
        } else if (in->ops[0].kind == OPD_LABEL) {
            /* Label should have been resolved to OPD_IMM already. */
            return ERR_UNDEFINED_LABEL;
        } else {
            return ERR_WRONG_OPERAND_COUNT;
        }
        return encode_J(def->opcode, target_or_offset, out);
    }

    case FMT_NONE:
        if (in->nops != 0) return ERR_WRONG_OPERAND_COUNT;
        /* RET and HALT: R-format with all register fields 0. */
        return encode_R(def->opcode, 0, 0, 0, out);
    }

    return ERR_INTERNAL;
}
