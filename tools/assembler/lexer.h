/*
 * lexer.h --- Line-oriented tokeniser.
 *
 * The source language is line-oriented: each line yields a list of tokens.
 * ';' begins a comment that runs to end-of-line. Commas are treated as
 * whitespace separators. Identifiers are [A-Za-z_.][A-Za-z0-9_]*, and a
 * trailing ':' marks a label definition.
 */
#ifndef LEXER_H
#define LEXER_H

#include <stdbool.h>

#define MAX_TOKENS_PER_LINE 16
#define MAX_TOKEN_LEN       127

typedef enum {
    TOK_NONE = 0,
    TOK_IDENT,     /* a bare identifier, e.g. MOV, R3, foo */
    TOK_LABEL,     /* identifier terminated by ':' (colon is stripped) */
    TOK_NUMBER,    /* decimal, 0x hex, 0b binary, signed */
    TOK_STRING,    /* "..."  (quotes stripped) */
    TOK_DIRECTIVE  /* .org, .word, .ascii, .equ */
} TokenKind;

typedef struct {
    TokenKind kind;
    char      text[MAX_TOKEN_LEN + 1];
} Token;

typedef struct {
    Token tokens[MAX_TOKENS_PER_LINE];
    int   count;
    int   line_no;   /* 1-based */
} TokenLine;

/*
 * Tokenise one line of source. Returns true on success, false on a lexer
 * error (unterminated string, token too long, too many tokens, ...).
 * `line_no` is stored on the TokenLine for error messages.
 *
 * The input string need not be null-trimmed; trailing newlines are ignored.
 */
bool lex_line(const char *src, int line_no, TokenLine *out,
              const char *file, int *had_error);

#endif /* LEXER_H */
