/*
 * parser.c --- Parse a single TokenLine into a ParsedLine.
 */
#include "parser.h"
#include "errors.h"
#include "isa.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

/* Bounded string copy: guarantees NUL termination and avoids the
 * -Wstringop-truncation warning produced by strncpy(dst, src, SIZE-1). */
#define COPY_TOK(dst, src) snprintf((dst), sizeof(dst), "%s", (src))

int parse_register(const char *tok) {
    if (!tok) return -1;
    if ((tok[0] != 'R' && tok[0] != 'r')) return -1;
    /* Accept R0..R7 exactly. */
    if (!isdigit((unsigned char)tok[1]) || tok[2] != '\0') return -1;
    int n = tok[1] - '0';
    if (n < 0 || n >= NUM_REGS) return -1;
    return n;
}

bool parse_number(const char *tok, int32_t *out) {
    if (!tok || !*tok) return false;

    const char *p = tok;
    int sign = 1;
    if (*p == '+') p++;
    else if (*p == '-') { sign = -1; p++; }

    if (!*p) return false;

    int base = 10;
    if (p[0] == '0' && (p[1] == 'x' || p[1] == 'X')) { base = 16; p += 2; }
    else if (p[0] == '0' && (p[1] == 'b' || p[1] == 'B')) { base = 2; p += 2; }

    char *end = NULL;
    long v = strtol(p, &end, base);
    if (end == p || (end && *end != '\0')) return false;

    *out = (int32_t)(sign * v);
    return true;
}

static bool fill_value_operand(const Token *t, Operand *op) {
    if (t->kind == TOK_NUMBER) {
        int32_t v;
        if (!parse_number(t->text, &v)) return false;
        op->kind = OPD_IMM;
        op->imm  = v;
        return true;
    }
    if (t->kind == TOK_IDENT) {
        /* Could be a register or a label reference. Decide by R0..R7 check. */
        int r = parse_register(t->text);
        if (r >= 0) {
            op->kind = OPD_REG;
            op->reg  = (uint8_t)r;
        } else {
            op->kind = OPD_LABEL;
            COPY_TOK(op->label, t->text);
        }
        return true;
    }
    return false;
}

static bool parse_directive(const TokenLine *tl, int start, ParsedLine *out,
                            const char *file, int *had_error) {
    const Token *d = &tl->tokens[start];
    const char *name = d->text; /* includes leading '.' */

    out->kind = PL_DIRECTIVE;

    if (strcasecmp(name, ".org") == 0) {
        if (start + 1 >= tl->count) {
            asm_report(file, tl->line_no, ERR_WRONG_OPERAND_COUNT,
                       ".org requires one numeric argument");
            *had_error = 1;
            return false;
        }
        out->dir.kind = DIR_ORG;
        if (!fill_value_operand(&tl->tokens[start + 1], &out->dir.value) ||
            out->dir.value.kind != OPD_IMM) {
            asm_report(file, tl->line_no, ERR_INVALID_NUMBER,
                       ".org argument must be a numeric literal");
            *had_error = 1;
            return false;
        }
        return true;
    }
    if (strcasecmp(name, ".word") == 0) {
        if (start + 1 >= tl->count) {
            asm_report(file, tl->line_no, ERR_WRONG_OPERAND_COUNT,
                       ".word requires one argument");
            *had_error = 1;
            return false;
        }
        out->dir.kind = DIR_WORD;
        if (!fill_value_operand(&tl->tokens[start + 1], &out->dir.value)) {
            asm_report(file, tl->line_no, ERR_INVALID_NUMBER,
                       ".word argument must be a number or label");
            *had_error = 1;
            return false;
        }
        return true;
    }
    if (strcasecmp(name, ".ascii") == 0) {
        if (start + 1 >= tl->count || tl->tokens[start + 1].kind != TOK_STRING) {
            asm_report(file, tl->line_no, ERR_WRONG_OPERAND_COUNT,
                       ".ascii requires a string literal");
            *had_error = 1;
            return false;
        }
        out->dir.kind = DIR_ASCII;
        const char *s = tl->tokens[start + 1].text;
        size_t n = strlen(s);
        if (n > MAX_TOKEN_LEN) n = MAX_TOKEN_LEN;
        memcpy(out->dir.str, s, n);
        out->dir.str[n] = '\0';
        out->dir.str_len = (int)n;
        return true;
    }
    if (strcasecmp(name, ".equ") == 0) {
        if (start + 2 >= tl->count ||
            tl->tokens[start + 1].kind != TOK_IDENT ||
            tl->tokens[start + 2].kind != TOK_NUMBER) {
            asm_report(file, tl->line_no, ERR_WRONG_OPERAND_COUNT,
                       ".equ requires <name> <numeric-value>");
            *had_error = 1;
            return false;
        }
        out->dir.kind = DIR_EQU;
        COPY_TOK(out->dir.name, tl->tokens[start + 1].text);
        int32_t v;
        if (!parse_number(tl->tokens[start + 2].text, &v)) {
            asm_report(file, tl->line_no, ERR_INVALID_NUMBER,
                       ".equ value is not a valid number");
            *had_error = 1;
            return false;
        }
        out->dir.value.kind = OPD_IMM;
        out->dir.value.imm  = v;
        return true;
    }

    asm_report(file, tl->line_no, ERR_INVALID_DIRECTIVE,
               "unknown directive '%s'", name);
    *had_error = 1;
    return false;
}

static bool parse_instruction(const TokenLine *tl, int start, ParsedLine *out,
                              const char *file, int *had_error) {
    out->kind = PL_INSTR;
    COPY_TOK(out->instr.mnemonic, tl->tokens[start].text);

    int n = 0;
    for (int i = start + 1; i < tl->count; i++) {
        if (n >= 3) {
            asm_report(file, tl->line_no, ERR_WRONG_OPERAND_COUNT,
                       "too many operands for %s", out->instr.mnemonic);
            *had_error = 1;
            return false;
        }
        const Token *t = &tl->tokens[i];
        if (!fill_value_operand(t, &out->instr.ops[n])) {
            asm_report(file, tl->line_no, ERR_WRONG_OPERAND_COUNT,
                       "unexpected operand '%s'", t->text);
            *had_error = 1;
            return false;
        }
        n++;
    }
    out->instr.nops = n;
    return true;
}

bool parse_line(const TokenLine *tl, ParsedLine *out,
                const char *file, int *had_error) {
    memset(out, 0, sizeof(*out));
    out->line_no = tl->line_no;

    if (tl->count == 0) {
        out->kind = PL_EMPTY;
        return true;
    }

    int idx = 0;

    /* Optional label definition at the head of the line. */
    if (tl->tokens[0].kind == TOK_LABEL) {
        COPY_TOK(out->label, tl->tokens[0].text);
        idx = 1;
        if (idx == tl->count) {
            out->kind = PL_LABEL;
            return true;
        }
    }

    const Token *head = &tl->tokens[idx];
    if (head->kind == TOK_DIRECTIVE) {
        return parse_directive(tl, idx, out, file, had_error);
    }
    if (head->kind == TOK_IDENT) {
        return parse_instruction(tl, idx, out, file, had_error);
    }

    asm_report(file, tl->line_no, ERR_UNKNOWN_MNEMONIC,
               "expected mnemonic or directive, got '%s'", head->text);
    *had_error = 1;
    return false;
}
