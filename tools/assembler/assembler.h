/*
 * assembler.h --- Public API for the two-pass assembler.
 *
 * Typical usage:
 *   AsmOptions opt = { .listing = false, .dump_symbols = false };
 *   AsmResult  res = {0};
 *   AsmError   rc  = assemble_file("prog.asm", "prog.bin", &opt, &res);
 *
 * The .bin output is a little-endian stream of 16-bit words starting at
 * the origin address set by the first (or default) .org directive.
 */
#ifndef ASSEMBLER_H
#define ASSEMBLER_H

#include <stdbool.h>
#include <stdint.h>

#include "errors.h"
#include "isa.h"

typedef struct {
    bool listing;       /* --listing : print address, hex, source line */
    bool dump_symbols;  /* --symbols : dump the symbol table after asm */
} AsmOptions;

typedef struct {
    uint16_t origin;       /* first address emitted */
    uint16_t n_words;      /* number of 16-bit words emitted */
    int      n_errors;     /* count of reported errors */
} AsmResult;

/*
 * Assemble `src_path` and write binary output to `out_path`.
 * Returns ERR_OK on success, or an AsmError on failure. On failure the
 * partial .bin file is not written.
 */
AsmError assemble_file(const char *src_path, const char *out_path,
                       const AsmOptions *opt, AsmResult *result);

#endif /* ASSEMBLER_H */
