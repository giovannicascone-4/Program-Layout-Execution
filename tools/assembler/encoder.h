/*
 * encoder.h --- Encode parsed instructions into 16-bit words.
 *
 * Each encode_X function enforces the relevant field-range check and
 * returns an AsmError via int. The produced 16-bit word is written to
 * `*out` on success.
 *
 * The high-level encode_instr() is driven by a mnemonic table.
 */
#ifndef ENCODER_H
#define ENCODER_H

#include <stdbool.h>
#include <stdint.h>

#include "parser.h"

/* Low-level format encoders. */
int encode_R(uint8_t op, uint8_t rd, uint8_t rs1, uint8_t rs2,
             uint16_t *out);
int encode_I(uint8_t op, uint8_t rd, uint8_t rs1, int32_t imm,
             uint16_t *out);
int encode_J(uint8_t op, int32_t offset, uint16_t *out);

/*
 * Is `mnemonic` a known instruction? Case-insensitive.
 */
bool is_known_mnemonic(const char *mnemonic);

/*
 * Encode a single parsed instruction. `pc` is the address of this
 * instruction (used for PC-relative jumps: offset = target - (pc + 1)).
 * Labels in operands must already have been resolved to imm values by
 * the caller (assembler.c) before calling this.
 *
 * Returns ERR_OK on success, or an AsmError code on failure.
 */
int encode_instr(const Instr *in, uint16_t pc, uint16_t *out);

#endif /* ENCODER_H */
