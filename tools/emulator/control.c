#include "control.h"

#include "alu.h"
#include "memory.h"
#include <stddef.h>
#include <stdio.h>

void cpu_step(CPU *cpu) {
    uint8_t op;
    uint8_t rd;
    uint8_t rs1;
    uint8_t rs2;
    int16_t imm;
    int16_t off;
    uint16_t addr;

    if (cpu == NULL || cpu->halted) {
        return;
    }

    /* FETCH */
    cpu->ir = mem_read(cpu->pc);
    cpu->pc = (uint16_t)(cpu->pc + 1u);
    cpu->cycle_count += 1u;

    /* DECODE */
    op = (uint8_t)INSTR_OPCODE(cpu->ir);
    rd = (uint8_t)INSTR_RD(cpu->ir);
    rs1 = (uint8_t)INSTR_RS1(cpu->ir);
    rs2 = (uint8_t)INSTR_RS2(cpu->ir);
    imm = INSTR_IMM5(cpu->ir);
    off = INSTR_OFFSET(cpu->ir);

    /* EXECUTE */
    switch (op) {
    case OP_ADD:
    case OP_SUB:
    case OP_AND:
    case OP_OR:
    case OP_XOR:
    case OP_SHL:
    case OP_SHR:
        cpu->regs[rd] = alu_execute(cpu, op, cpu->regs[rs1], cpu->regs[rs2]);
        break;

    case OP_NOT:
        cpu->regs[rd] = alu_execute(cpu, op, cpu->regs[rs1], 0u);
        break;

    case OP_ADDI:
        cpu->regs[rd] = alu_execute(cpu, op, cpu->regs[rs1], (uint16_t)imm);
        break;

    case OP_LW:
        addr = (uint16_t)(cpu->regs[rs1] + (uint16_t)imm);
        cpu->regs[rd] = mem_read(addr);
        break;

    case OP_SW:
        addr = (uint16_t)(cpu->regs[rs1] + (uint16_t)imm);
        mem_write(addr, cpu->regs[rd]);
        break;

    case OP_MOV:
        cpu->regs[rd] = alu_execute(cpu, OP_ADD, 0u, (uint16_t)imm);
        break;

    case OP_CMP:
        (void)alu_execute(cpu, op, cpu->regs[rs1], cpu->regs[rs2]);
        break;

    case OP_JMP:
        cpu->pc = (uint16_t)(cpu->pc + off);
        break;

    case OP_JEQ:
        if ((cpu->flags & FLAG_ZF) != 0u) {
            cpu->pc = (uint16_t)(cpu->pc + off);
        }
        break;

    case OP_JNE:
        if ((cpu->flags & FLAG_ZF) == 0u) {
            cpu->pc = (uint16_t)(cpu->pc + off);
        }
        break;

    case OP_JLT:
        if ((cpu->flags & FLAG_NF) != 0u) {
            cpu->pc = (uint16_t)(cpu->pc + off);
        }
        break;

    case OP_JGT:
        if ((cpu->flags & FLAG_ZF) == 0u && (cpu->flags & FLAG_NF) == 0u) {
            cpu->pc = (uint16_t)(cpu->pc + off);
        }
        break;

    case OP_CALL:
        mem_write(cpu->sp, cpu->pc);
        cpu->sp = (uint16_t)(cpu->sp - 1u);
        cpu->regs[7] = cpu->sp;
        cpu->pc = (uint16_t)(cpu->pc + off);
        break;

    case OP_RET:
        cpu->sp = (uint16_t)(cpu->sp + 1u);
        cpu->regs[7] = cpu->sp;
        cpu->pc = mem_read(cpu->sp);
        break;

    case OP_HALT:
        cpu->halted = true;
        break;

    default:
        fprintf(stderr, "illegal instruction: opcode=0x%02X at PC=0x%04X\n",
                op, (uint16_t)(cpu->pc - 1u));
        cpu->halted = true;
        break;
    }
}
