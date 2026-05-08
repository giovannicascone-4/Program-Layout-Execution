/*
 * assembler.c --- Two-pass assembler core.
 *
 * Pass 1: lex + parse every line, assign an address to each instruction
 *         / .word / .ascii, and record label → address mappings in the
 *         symbol table. `.equ` constants also go in the symbol table.
 *
 * Pass 2: resolve any label operands to their addresses, encode each
 *         instruction, and emit 16-bit words into a 64 KB memory image.
 *
 * Output format (.bin):
 *   Raw little-endian 16-bit words forming a memory image starting at
 *   address 0. Addresses below the first emitted word are zero-filled.
 *   The file contains `(highest_written_addr + 1) * 2` bytes.
 *
 *   This is a plain memory image, which Nikhil's mem_load() can fread
 *   directly into RAM. Coordinate if a different format is needed.
 *
 * Errors are reported to stderr as they are encountered; the assembler
 * continues scanning so multiple problems can be reported per run.
 * After pass 2 if `n_errors > 0` no output file is written.
 */
#include "assembler.h"
#include "encoder.h"
#include "errors.h"
#include "isa.h"
#include "lexer.h"
#include "parser.h"
#include "symtable.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Local strdup so we don't depend on POSIX/GNU feature flags. */
static char *asm_strdup(const char *s) {
    size_t n = strlen(s) + 1;
    char *p = malloc(n);
    if (p) memcpy(p, s, n);
    return p;
}

/* --- Error reporting (defined once for the whole assembler) ---------- */

const char *asm_error_name(AsmError e) {
    switch (e) {
    case ERR_OK:                   return "ok";
    case ERR_UNDEFINED_LABEL:      return "undefined-label";
    case ERR_DUPLICATE_LABEL:      return "duplicate-label";
    case ERR_IMM_OVERFLOW:         return "immediate-overflow";
    case ERR_JUMP_OVERFLOW:        return "jump-overflow";
    case ERR_UNKNOWN_MNEMONIC:     return "unknown-mnemonic";
    case ERR_WRONG_OPERAND_COUNT:  return "wrong-operand-count";
    case ERR_INVALID_REGISTER:     return "invalid-register";
    case ERR_INVALID_NUMBER:       return "invalid-number";
    case ERR_INVALID_DIRECTIVE:    return "invalid-directive";
    case ERR_IO:                   return "io";
    case ERR_INTERNAL:             return "internal";
    }
    return "unknown";
}

void asm_report(const char *file, int line, AsmError code,
                const char *fmt, ...) {
    if (file) fprintf(stderr, "%s:", file);
    if (line > 0) fprintf(stderr, "%d: ", line);
    fprintf(stderr, "error[%s]: ", asm_error_name(code));
    va_list ap;
    va_start(ap, fmt);
    vfprintf(stderr, fmt, ap);
    va_end(ap);
    fputc('\n', stderr);
}

/* --- Input buffering ------------------------------------------------- */

typedef struct {
    char   **lines;      /* array of heap-allocated \0-terminated lines */
    int      count;
    int      capacity;
} SourceFile;

static void source_free(SourceFile *sf) {
    for (int i = 0; i < sf->count; i++) free(sf->lines[i]);
    free(sf->lines);
    sf->lines = NULL;
    sf->count = sf->capacity = 0;
}

static bool source_read(const char *path, SourceFile *sf) {
    FILE *f = fopen(path, "r");
    if (!f) {
        asm_report(path, 0, ERR_IO, "could not open for reading");
        return false;
    }
    sf->count = 0;
    sf->capacity = 0;
    sf->lines = NULL;

    char buf[4096];
    while (fgets(buf, sizeof(buf), f)) {
        /* strip trailing newline */
        size_t n = strlen(buf);
        while (n > 0 && (buf[n - 1] == '\n' || buf[n - 1] == '\r')) buf[--n] = '\0';

        if (sf->count == sf->capacity) {
            int newcap = sf->capacity ? sf->capacity * 2 : 64;
            char **nl = realloc(sf->lines, sizeof(char *) * newcap);
            if (!nl) { fclose(f); source_free(sf); return false; }
            sf->lines = nl;
            sf->capacity = newcap;
        }
        sf->lines[sf->count++] = asm_strdup(buf);
    }
    fclose(f);
    return true;
}

/* --- Passes ---------------------------------------------------------- */

/*
 * Compute the number of output words produced by a parsed line so we
 * can advance the address counter during pass 1.
 */
static uint16_t line_size(const ParsedLine *pl) {
    switch (pl->kind) {
    case PL_INSTR:
        return 1;
    case PL_DIRECTIVE:
        switch (pl->dir.kind) {
        case DIR_WORD:  return 1;
        case DIR_ASCII: return (uint16_t)((pl->dir.str_len + 1 + 1) / 2);
            /* +1 for zero terminator, packed 2 bytes/word, rounded up */
        case DIR_ORG:
        case DIR_EQU:
            return 0;
        }
        return 0;
    case PL_LABEL:
    case PL_EMPTY:
    default:
        return 0;
    }
}

/*
 * Try to resolve an operand that carries a label to its numeric address.
 * Leaves imm/reg operands untouched. Returns true if the operand is now
 * immediate-valued (or never was a label in the first place).
 */
static bool resolve_operand(Operand *o, const char *file, int line_no) {
    if (o->kind != OPD_LABEL) return true;
    uint32_t v = symtable_lookup(o->label);
    if (v == SYM_NOT_FOUND) {
        asm_report(file, line_no, ERR_UNDEFINED_LABEL,
                   "label '%s' is not defined", o->label);
        return false;
    }
    o->kind = OPD_IMM;
    o->imm  = (int32_t)v;
    return true;
}

AsmError assemble_file(const char *src_path, const char *out_path,
                       const AsmOptions *opt, AsmResult *result) {
    AsmOptions opt_default = {0};
    if (!opt) opt = &opt_default;
    if (result) memset(result, 0, sizeof(*result));

    SourceFile sf;
    if (!source_read(src_path, &sf)) return ERR_IO;

    /* Allocate the parsed-line array (one per source line). */
    ParsedLine *parsed = calloc(sf.count, sizeof(ParsedLine));
    if (!parsed) { source_free(&sf); return ERR_INTERNAL; }

    symtable_reset();

    int n_errors = 0;

    /* ------------------- Pass 1: labels & addresses ------------------ */
    uint16_t addr = 0;
    bool saw_origin = false;
    uint16_t origin = 0;

    for (int i = 0; i < sf.count; i++) {
        TokenLine tl = {0};
        int lex_err = 0;
        if (!lex_line(sf.lines[i], i + 1, &tl, src_path, &lex_err)) {
            n_errors++;
            continue;
        }

        int parse_err = 0;
        if (!parse_line(&tl, &parsed[i], src_path, &parse_err)) {
            n_errors++;
            continue;
        }

        ParsedLine *pl = &parsed[i];
        pl->address = addr;

        /* Any label attached to this line is defined at `addr`. */
        if (pl->label[0]) {
            int rc = symtable_insert(pl->label, addr);
            if (rc != ERR_OK) {
                asm_report(src_path, pl->line_no, (AsmError)rc,
                           "label '%s'", pl->label);
                n_errors++;
            }
        }

        /* .org: move the address counter; do NOT emit bytes here. */
        if (pl->kind == PL_DIRECTIVE && pl->dir.kind == DIR_ORG) {
            int32_t v = pl->dir.value.imm;
            if (v < 0 || v > 0xFFFF) {
                asm_report(src_path, pl->line_no, ERR_IMM_OVERFLOW,
                           ".org address 0x%X out of range", v);
                n_errors++;
            } else {
                addr = (uint16_t)v;
                if (!saw_origin) { origin = addr; saw_origin = true; }
            }
            continue;
        }

        /* .equ: define a symbol, no address advance. */
        if (pl->kind == PL_DIRECTIVE && pl->dir.kind == DIR_EQU) {
            int32_t v = pl->dir.value.imm;
            int rc = symtable_insert(pl->dir.name, (uint16_t)v);
            if (rc != ERR_OK) {
                asm_report(src_path, pl->line_no, (AsmError)rc,
                           "constant '%s'", pl->dir.name);
                n_errors++;
            }
            continue;
        }

        addr += line_size(pl);
    }

    if (!saw_origin) origin = 0;
    uint16_t highest = (addr == 0) ? 0 : (uint16_t)(addr - 1);

    /* ------------------- Pass 2: encode & emit ----------------------- */
    /* 64 KB word-addressable image; each word is 2 bytes little-endian. */
    uint16_t *image = calloc(MEM_SIZE, sizeof(uint16_t));
    if (!image) { free(parsed); source_free(&sf); return ERR_INTERNAL; }

    uint16_t n_words = 0;

    for (int i = 0; i < sf.count; i++) {
        ParsedLine *pl = &parsed[i];

        if (pl->kind == PL_INSTR) {
            /* Resolve any label operands to numeric addresses. */
            bool ok = true;
            for (int k = 0; k < pl->instr.nops; k++) {
                ok &= resolve_operand(&pl->instr.ops[k], src_path, pl->line_no);
            }
            if (!ok) { n_errors++; continue; }

            uint16_t word = 0;
            int rc = encode_instr(&pl->instr, pl->address, &word);
            if (rc != ERR_OK) {
                asm_report(src_path, pl->line_no, (AsmError)rc,
                           "while encoding '%s'", pl->instr.mnemonic);
                n_errors++;
                continue;
            }

            image[pl->address] = word;
            n_words++;

            if (opt->listing) {
                printf("  %04X  %04X   ", pl->address, word);
                if (i < sf.count && sf.lines[i]) {
                    printf("%s\n", sf.lines[i]);
                } else {
                    printf("\n");
                }
            }
        } else if (pl->kind == PL_DIRECTIVE) {
            switch (pl->dir.kind) {
            case DIR_WORD: {
                int32_t v = 0;
                if (pl->dir.value.kind == OPD_LABEL) {
                    if (!resolve_operand(&pl->dir.value, src_path, pl->line_no)) {
                        n_errors++;
                        break;
                    }
                }
                v = pl->dir.value.imm;
                image[pl->address] = (uint16_t)(v & 0xFFFF);
                n_words++;
                if (opt->listing) {
                    printf("  %04X  %04X   ; .word %d\n",
                           pl->address, (uint16_t)v, v);
                }
                break;
            }
            case DIR_ASCII: {
                /* Pack 2 bytes/word little-endian; write a trailing NUL. */
                const char *s = pl->dir.str;
                int len = pl->dir.str_len;
                uint16_t a = pl->address;
                for (int b = 0; b <= len; b += 2) {
                    uint16_t lo = (b     <= len) ? (uint8_t)s[b]     : 0;
                    uint16_t hi = (b + 1 <= len) ? (uint8_t)s[b + 1] : 0;
                    uint16_t w = (uint16_t)(lo | (hi << 8));
                    image[a++] = w;
                    n_words++;
                    if (opt->listing) {
                        printf("  %04X  %04X   ; .ascii\n", (uint16_t)(a - 1), w);
                    }
                }
                break;
            }
            case DIR_ORG:
            case DIR_EQU:
                /* no emission */
                break;
            }
        }
        /* PL_LABEL / PL_EMPTY: nothing to emit. */
    }

    /* ------------------- Finalise ------------------------------------ */
    AsmError rc = ERR_OK;

    if (n_errors == 0) {
        /* Write memory image from 0 up to (highest + 1) words, little-endian. */
        FILE *of = fopen(out_path, "wb");
        if (!of) {
            asm_report(out_path, 0, ERR_IO, "could not open for writing");
            rc = ERR_IO;
        } else {
            uint16_t count = (uint16_t)(highest + 1);
            for (uint32_t w = 0; w < count; w++) {
                uint8_t bytes[2] = {
                    (uint8_t)(image[w] & 0xFF),
                    (uint8_t)((image[w] >> 8) & 0xFF)
                };
                if (fwrite(bytes, 1, 2, of) != 2) {
                    asm_report(out_path, 0, ERR_IO, "write failed");
                    rc = ERR_IO;
                    break;
                }
            }
            fclose(of);
        }
    } else {
        rc = ERR_INTERNAL; /* generic: see reported messages */
    }

    if (opt->dump_symbols) symtable_dump();

    if (result) {
        result->origin   = origin;
        result->n_words  = n_words;
        result->n_errors = n_errors;
    }

    free(image);
    free(parsed);
    source_free(&sf);
    return rc;
}
