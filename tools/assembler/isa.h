/*
 * isa.h --- Shared KunalISA constants and decode helpers.
 *
 * This header is the compatibility contract between assembler and emulator.
 * Keep definitions stable once downstream components depend on them.
 */
#ifndef ISA_H
#define ISA_H

#include <stdint.h>

/* Register file layout. */
#define NUM_REGS 8u

/* Opcodes (5-bit, stored in bits 15:11). */
#define OP_ADD  0x01u
#define OP_SUB  0x02u
#define OP_AND  0x03u
#define OP_OR   0x04u
#define OP_XOR  0x05u
#define OP_NOT  0x06u
#define OP_SHL  0x07u
#define OP_SHR  0x08u
#define OP_ADDI 0x09u
#define OP_LW   0x0Au
#define OP_SW   0x0Bu
#define OP_MOV  0x0Cu
#define OP_CMP  0x0Du
#define OP_JMP  0x0Eu
#define OP_JEQ  0x0Fu
#define OP_JNE  0x10u
#define OP_JLT  0x11u
#define OP_JGT  0x12u
#define OP_CALL 0x13u
#define OP_RET  0x14u
#define OP_HALT 0x1Fu

/* Immediate bounds for I-format imm5. */
#define IMM5_MIN (-16)
#define IMM5_MAX 15

/*
 * Jump bounds used by the current assembler encoder implementation.
 * The existing code emits J as op5 + signed offset11.
 */
#define JUMP_MIN (-1024)
#define JUMP_MAX 1023

/* Memory map (word-addressable 64K space). */
#define MEM_SIZE   0x10000u
#define IO_BASE    0xFF00u
#define IO_STDOUT  0xFF00u
#define IO_STDIN   0xFF01u
#define IO_TIMER   0xFF02u
#define IO_STATUS  0xFF03u
#define STACK_BASE 0xFEFFu

/* Instruction field extraction helpers. */
#define INSTR_OPCODE(w) (((uint16_t)(w) >> 11) & 0x1Fu)
#define INSTR_RD(w)     (((uint16_t)(w) >> 8)  & 0x07u)
#define INSTR_RS1(w)    (((uint16_t)(w) >> 5)  & 0x07u)
#define INSTR_RS2(w)    (((uint16_t)(w) >> 2)  & 0x07u)

/* Sign-extend low 5 bits to int16_t. */
#define INSTR_IMM5(w)   ((int16_t)(((int16_t)((uint16_t)(w) << 11)) >> 11))

/* Sign-extend low 11 bits (current encoded jump offset) to int16_t. */
#define INSTR_OFFSET(w) ((int16_t)(((int16_t)((uint16_t)(w) << 5)) >> 5))

/*
 * Optional helper for future full-spec J format (signed imm15).
 * Kept here to align with Kunal's ISA document while preserving compatibility.
 */
#define INSTR_IMM15(w)  ((int16_t)(((int16_t)((uint16_t)(w) << 1)) >> 1))

#endif /* ISA_H */
