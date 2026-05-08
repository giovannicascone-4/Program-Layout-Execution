/*
 * parser.h --- Turn a TokenLine into a parsed Instruction or Directive.
 *
 * The parser is intentionally simple: one line → one ParsedLine. Label
 * definitions on their own line produce a PL_LABEL record. Lines that
 * combine a label with an instruction are split by the caller (assembler.c)
 * into two ParsedLine records.
 */
#ifndef PARSER_H
#define PARSER_H

#include <stdbool.h>
#include <stdint.h>

#include "lexer.h"

typedef enum {
    PL_EMPTY = 0,    /* nothing on this line */
    PL_LABEL,        /* label-only line */
    PL_INSTR,        /* instruction */
    PL_DIRECTIVE     /* .org / .word / .ascii / .equ */
} ParsedKind;

typedef enum {
    OPD_NONE = 0,
    OPD_REG,       /* R0..R7 */
    OPD_IMM,       /* numeric literal (already evaluated) */
    OPD_LABEL      /* identifier to be resolved later */
} OperandKind;

typedef struct {
    OperandKind kind;
    uint8_t     reg;         /* valid if kind == OPD_REG */
    int32_t     imm;         /* valid if kind == OPD_IMM (wide enough for .word) */
    char        label[MAX_TOKEN_LEN + 1]; /* valid if kind == OPD_LABEL */
} Operand;

typedef struct {
    char     mnemonic[MAX_TOKEN_LEN + 1];
    Operand  ops[3];
    int      nops;
} Instr;

typedef enum {
    DIR_ORG,
    DIR_WORD,
    DIR_ASCII,
    DIR_EQU
} DirectiveKind;

typedef struct {
    DirectiveKind kind;
    /* For .org / .word: a numeric operand (imm) or label reference. */
    Operand       value;
    /* For .ascii: the string contents. */
    char          str[MAX_TOKEN_LEN + 1];
    int           str_len;
    /* For .equ: the name being defined. */
    char          name[MAX_TOKEN_LEN + 1];
} Directive;

typedef struct {
    ParsedKind   kind;
    int          line_no;
    uint16_t     address;   /* assigned in pass 1 */
    char         label[MAX_TOKEN_LEN + 1]; /* non-empty if this line had a label */
    Instr        instr;
    Directive    dir;
} ParsedLine;

/*
 * Parse a single TokenLine into one ParsedLine. If the line carries both a
 * label and an instruction/directive, the label is attached to the same
 * ParsedLine (on `.label`) and the kind is set to PL_INSTR / PL_DIRECTIVE.
 * An isolated label becomes PL_LABEL.
 *
 * Returns true on success; on failure the error has already been reported
 * and *had_error is set.
 */
bool parse_line(const TokenLine *tl, ParsedLine *out,
                const char *file, int *had_error);

/* Map "R0".."R7" to 0..7. Returns -1 on bad input. */
int parse_register(const char *tok);

/*
 * Parse a numeric literal token (decimal, 0x.., 0b..). Returns true on
 * success. Accepts a leading sign.
 */
bool parse_number(const char *tok, int32_t *out);

#endif /* PARSER_H */
