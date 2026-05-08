#include "cpu.h"

#include <inttypes.h>
#include <stdio.h>
#include <string.h>

void cpu_init(CPU *cpu) {
    if (cpu == NULL) {
        return;
    }

    memset(cpu, 0, sizeof(*cpu));
    cpu->sp = (uint16_t)STACK_BASE;
    cpu->regs[7] = cpu->sp;
}

void cpu_reset(CPU *cpu) {
    if (cpu == NULL) {
        return;
    }

    memset(cpu->regs, 0, sizeof(cpu->regs));
    cpu->pc = 0u;
    cpu->ir = 0u;
    cpu->sp = (uint16_t)STACK_BASE;
    cpu->regs[7] = cpu->sp;
    cpu->flags = 0u;
    cpu->cycle_count = 0u;
    cpu->halted = false;
}

void cpu_run(CPU *cpu) {
    if (cpu == NULL) {
        return;
    }

    while (!cpu->halted) {
        cpu_step(cpu);
    }
}

void cpu_dump(const CPU *cpu) {
    uint32_t i;

    if (cpu == NULL) {
        return;
    }

    puts("=== CPU State ===");
    for (i = 0u; i < NUM_REGS; ++i) {
        printf("R%u: 0x%04X (%5u)\n", i, cpu->regs[i], cpu->regs[i]);
    }
    printf("PC: 0x%04X\n", cpu->pc);
    printf("IR: 0x%04X\n", cpu->ir);
    printf("SP: 0x%04X\n", cpu->sp);
    printf("FLAGS: [Z=%u N=%u C=%u O=%u] (0x%02X)\n",
           (cpu->flags & FLAG_ZF) != 0u,
           (cpu->flags & FLAG_NF) != 0u,
           (cpu->flags & FLAG_CF) != 0u,
           (cpu->flags & FLAG_OF) != 0u,
           cpu->flags);
    printf("CYCLES: %" PRIu64 "\n", cpu->cycle_count);
    printf("HALTED: %s\n", cpu->halted ? "yes" : "no");
}
