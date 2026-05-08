/*
 * lexer.c --- Tokeniser implementation.
 */
#include "lexer.h"
#include "errors.h"

#include <ctype.h>
#include <stdbool.h>
#include <string.h>

static bool is_ident_start(int c) {
    return isalpha(c) || c == '_' || c == '.';
}
static bool is_ident_cont(int c) {
    return isalnum(c) || c == '_';
}

/*
 * Append a token; returns false if the line already has the maximum
 * number of tokens.
 */
static bool push_token(TokenLine *tl, TokenKind kind, const char *text) {
    if (tl->count >= MAX_TOKENS_PER_LINE) return false;
    Token *t = &tl->tokens[tl->count++];
    t->kind = kind;
    size_t n = strlen(text);
    if (n > MAX_TOKEN_LEN) n = MAX_TOKEN_LEN;
    memcpy(t->text, text, n);
    t->text[n] = '\0';
    return true;
}

bool lex_line(const char *src, int line_no, TokenLine *out,
              const char *file, int *had_error) {
    out->count = 0;
    out->line_no = line_no;

    const char *p = src;
    char buf[MAX_TOKEN_LEN + 2];

    while (*p) {
        /* Skip whitespace and commas (commas are separators, not operators). */
        while (*p && (isspace((unsigned char)*p) || *p == ',')) p++;
        if (!*p) break;

        /* Comment: rest of line. */
        if (*p == ';') break;

        /* String literal: ".." with simple backslash escapes. */
        if (*p == '"') {
            p++;
            size_t n = 0;
            while (*p && *p != '"') {
                char c = *p++;
                if (c == '\\' && *p) {
                    char esc = *p++;
                    switch (esc) {
                        case 'n': c = '\n'; break;
                        case 't': c = '\t'; break;
                        case 'r': c = '\r'; break;
                        case '0': c = '\0'; break;
                        case '\\': c = '\\'; break;
                        case '"': c = '"';  break;
                        default:  c = esc;  break;
                    }
                }
                if (n >= MAX_TOKEN_LEN) {
                    asm_report(file, line_no, ERR_INTERNAL,
                               "string literal too long");
                    *had_error = 1;
                    return false;
                }
                buf[n++] = c;
            }
            if (*p != '"') {
                asm_report(file, line_no, ERR_INTERNAL,
                           "unterminated string literal");
                *had_error = 1;
                return false;
            }
            p++; /* consume closing quote */
            buf[n] = '\0';
            if (!push_token(out, TOK_STRING, buf)) {
                asm_report(file, line_no, ERR_INTERNAL, "too many tokens");
                *had_error = 1;
                return false;
            }
            continue;
        }

        /* Number (signed): optional -, then digits / 0x / 0b. */
        if (isdigit((unsigned char)*p) ||
            (*p == '-' && isdigit((unsigned char)p[1])) ||
            (*p == '+' && isdigit((unsigned char)p[1]))) {
            size_t n = 0;
            if (*p == '-' || *p == '+') buf[n++] = *p++;
            /* 0x.., 0b.., 0..  */
            if (*p == '0' && (p[1] == 'x' || p[1] == 'X' ||
                              p[1] == 'b' || p[1] == 'B')) {
                buf[n++] = *p++; /* 0 */
                buf[n++] = *p++; /* x or b */
            }
            while (*p && (isalnum((unsigned char)*p))) {
                if (n >= MAX_TOKEN_LEN) break;
                buf[n++] = *p++;
            }
            buf[n] = '\0';
            if (!push_token(out, TOK_NUMBER, buf)) {
                asm_report(file, line_no, ERR_INTERNAL, "too many tokens");
                *had_error = 1;
                return false;
            }
            continue;
        }

        /* Identifier / directive / label. */
        if (is_ident_start((unsigned char)*p)) {
            size_t n = 0;
            bool is_directive = (*p == '.');
            while (*p && (is_ident_cont((unsigned char)*p) ||
                          (n == 0 && *p == '.'))) {
                if (n >= MAX_TOKEN_LEN) break;
                buf[n++] = *p++;
            }
            buf[n] = '\0';

            if (*p == ':') {
                /* Label definition. */
                p++;
                if (!push_token(out, TOK_LABEL, buf)) {
                    asm_report(file, line_no, ERR_INTERNAL, "too many tokens");
                    *had_error = 1;
                    return false;
                }
            } else {
                TokenKind kind = is_directive ? TOK_DIRECTIVE : TOK_IDENT;
                if (!push_token(out, kind, buf)) {
                    asm_report(file, line_no, ERR_INTERNAL, "too many tokens");
                    *had_error = 1;
                    return false;
                }
            }
            continue;
        }

        /* Unknown character. */
        asm_report(file, line_no, ERR_INTERNAL,
                   "unexpected character '%c' (0x%02X)", *p, (unsigned char)*p);
        *had_error = 1;
        return false;
    }

    return true;
}
