#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "../assembler/isa.h"

/* Flag bit positions in CPU flags register. */
#define FLAG_ZF (1u << 0) /* Zero */
#define FLAG_NF (1u << 1) /* Negative */
#define FLAG_CF (1u << 2) /* Carry */
#define FLAG_OF (1u << 3) /* Overflow */

typedef struct CPU {
    uint16_t regs[NUM_REGS]; /* R0-R7 */
    uint16_t pc;             /* Program Counter */
    uint16_t ir;             /* Instruction Register */
    uint16_t sp;             /* Stack Pointer (R7 alias) */
    uint8_t flags;           /* ZF | NF | CF | OF */
    uint64_t cycle_count;    /* Total cycles executed */
    bool halted;             /* True after HALT */
} CPU;

void cpu_init(CPU *cpu);
void cpu_reset(CPU *cpu);
void cpu_step(CPU *cpu);
void cpu_run(CPU *cpu);
void cpu_dump(const CPU *cpu);
