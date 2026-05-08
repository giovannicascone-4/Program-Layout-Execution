/*
 * errors.h --- Error codes and reporting helpers for the assembler.
 */
#ifndef ERRORS_H
#define ERRORS_H

typedef enum {
    ERR_OK = 0,
    ERR_UNDEFINED_LABEL,      /* label referenced but never defined */
    ERR_DUPLICATE_LABEL,      /* same label defined twice */
    ERR_IMM_OVERFLOW,         /* immediate value out of encodable range */
    ERR_JUMP_OVERFLOW,        /* jump offset too large for signed field */
    ERR_UNKNOWN_MNEMONIC,     /* unrecognised instruction mnemonic */
    ERR_WRONG_OPERAND_COUNT,  /* wrong number of operands for mnemonic */
    ERR_INVALID_REGISTER,     /* bad register name (e.g. R9, RX) */
    ERR_INVALID_NUMBER,       /* malformed numeric literal */
    ERR_INVALID_DIRECTIVE,    /* unknown .directive */
    ERR_IO,                   /* file I/O failure */
    ERR_INTERNAL              /* assembler bug / assertion */
} AsmError;

const char *asm_error_name(AsmError e);

/*
 * asm_report() prints a formatted error prefixed with "<file>:<line>: error: "
 * to stderr. Pass line = 0 to omit the line number.
 */
void asm_report(const char *file, int line, AsmError code,
                const char *fmt, ...);

#endif /* ERRORS_H */
