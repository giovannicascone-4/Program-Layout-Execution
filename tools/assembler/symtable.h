/*
 * symtable.h --- Label/constant symbol table for the assembler.
 *
 * A simple open-addressing hash table keyed by symbol name. Capacity is
 * fixed at compile time; the assembler aborts if the table fills up.
 */
#ifndef SYMTABLE_H
#define SYMTABLE_H

#include <stdbool.h>
#include <stdint.h>

#define SYM_CAPACITY 256
#define SYM_NAME_MAX 63           /* max identifier length, +1 for NUL */
#define SYM_NOT_FOUND ((uint32_t)0xFFFFFFFFu)

typedef struct {
    char     name[SYM_NAME_MAX + 1];
    uint16_t addr;
    bool     used;   /* slot is occupied */
} Symbol;

/* Reset the symbol table (call once per assembly run). */
void symtable_reset(void);

/*
 * Insert a symbol. Returns ERR_OK on success, ERR_DUPLICATE_LABEL if the
 * name was already present, or ERR_INTERNAL if the table is full.
 *
 * Uses int return (matches AsmError enum) to avoid pulling in errors.h here.
 */
int symtable_insert(const char *name, uint16_t addr);

/*
 * Look up a symbol. Returns the address, or SYM_NOT_FOUND if missing.
 * We use a 32-bit return so we can signal "not found" distinctly from
 * any legal 16-bit address.
 */
uint32_t symtable_lookup(const char *name);

/* Number of defined symbols (for diagnostics). */
int symtable_count(void);

/* Print all symbols to stdout as "NAME = 0xHHHH". */
void symtable_dump(void);

#endif /* SYMTABLE_H */
