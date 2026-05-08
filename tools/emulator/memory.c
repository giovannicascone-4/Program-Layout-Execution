#include "memory.h"

#include <stdio.h>
#include <time.h>

static uint16_t mem[MEM_SIZE];

uint16_t mem_read(uint16_t addr) {
    if (addr == IO_STDIN) {
        int ch = getchar();
        if (ch == EOF) {
            return 0u;
        }
        return (uint16_t)(uint8_t)ch;
    }

    if (addr == IO_TIMER) {
        return (uint16_t)(clock() & 0xFFFFu);
    }

    return mem[addr];
}

void mem_write(uint16_t addr, uint16_t val) {
    if (addr == IO_STDOUT) {
        putchar((char)(val & 0x00FFu));
        return;
    }

    if (addr >= IO_BASE) {
        return; /* other memory-mapped I/O writes are ignored */
    }

    mem[addr] = val;
}

void mem_load(const uint16_t *program, uint16_t len, uint16_t base) {
    uint16_t i;

    if (program == NULL) {
        return;
    }

    for (i = 0u; i < len; ++i) {
        mem[(uint16_t)(base + i)] = program[i];
    }
}

void mem_dump(uint16_t start, uint16_t end) {
    uint16_t addr;
    uint16_t col;

    if (start >= end) {
        return;
    }

    for (addr = start; addr < end; addr = (uint16_t)(addr + 8u)) {
        printf("%04X: ", addr);
        for (col = 0u; col < 8u && (uint16_t)(addr + col) < end; ++col) {
            printf("%04X ", mem[(uint16_t)(addr + col)]);
        }
        putchar('\n');
    }
}
