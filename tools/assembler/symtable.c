/*
 * symtable.c --- Fixed-size open-addressing hash table.
 *
 * Names are compared case-sensitively. The hash is FNV-1a over the
 * name bytes, which is simple and has decent dispersion for short
 * identifiers.
 */
#include "symtable.h"
#include "errors.h"

#include <stdio.h>
#include <string.h>

static Symbol table[SYM_CAPACITY];
static int    count;

/* FNV-1a 32-bit hash. */
static uint32_t hash_name(const char *s) {
    uint32_t h = 2166136261u;
    while (*s) {
        h ^= (unsigned char)*s++;
        h *= 16777619u;
    }
    return h;
}

void symtable_reset(void) {
    memset(table, 0, sizeof(table));
    count = 0;
}

int symtable_insert(const char *name, uint16_t addr) {
    if (name == NULL || *name == '\0') {
        return ERR_INTERNAL;
    }
    if (count >= SYM_CAPACITY) {
        return ERR_INTERNAL; /* table full */
    }

    uint32_t h = hash_name(name);
    for (int probe = 0; probe < SYM_CAPACITY; probe++) {
        int idx = (int)((h + (uint32_t)probe) % SYM_CAPACITY);
        Symbol *s = &table[idx];
        if (!s->used) {
            strncpy(s->name, name, SYM_NAME_MAX);
            s->name[SYM_NAME_MAX] = '\0';
            s->addr = addr;
            s->used = true;
            count++;
            return ERR_OK;
        }
        if (strcmp(s->name, name) == 0) {
            return ERR_DUPLICATE_LABEL;
        }
    }
    return ERR_INTERNAL;
}

uint32_t symtable_lookup(const char *name) {
    if (name == NULL || *name == '\0') return SYM_NOT_FOUND;

    uint32_t h = hash_name(name);
    for (int probe = 0; probe < SYM_CAPACITY; probe++) {
        int idx = (int)((h + (uint32_t)probe) % SYM_CAPACITY);
        const Symbol *s = &table[idx];
        if (!s->used) return SYM_NOT_FOUND;
        if (strcmp(s->name, name) == 0) return s->addr;
    }
    return SYM_NOT_FOUND;
}

int symtable_count(void) { return count; }

void symtable_dump(void) {
    printf("-- symbol table (%d entries) --\n", count);
    for (int i = 0; i < SYM_CAPACITY; i++) {
        if (table[i].used) {
            printf("  %-32s = 0x%04X\n", table[i].name, table[i].addr);
        }
    }
}
