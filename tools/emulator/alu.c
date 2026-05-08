#include "alu.h"

static void set_flag(CPU *cpu, uint8_t mask, int value) {
    if (value) {
        cpu->flags |= mask;
    } else {
        cpu->flags &= (uint8_t)(~mask);
    }
}

static void update_zn_flags(CPU *cpu, uint16_t result) {
    set_flag(cpu, FLAG_ZF, result == 0u);
    set_flag(cpu, FLAG_NF, (result & 0x8000u) != 0u);
}

uint16_t alu_execute(CPU *cpu, uint8_t op, uint16_t a, uint16_t b) {
    uint32_t result32 = 0u;
    uint16_t result = 0u;
    int is_add_like = 0;

    if (cpu == 0) {
        return 0u;
    }

    switch (op) {
    case OP_ADD:
    case OP_ADDI:
        result32 = (uint32_t)a + (uint32_t)b;
        is_add_like = 1;
        break;

    case OP_SUB:
    case OP_CMP:
        result32 = (uint32_t)a - (uint32_t)b;
        break;

    case OP_AND:
        result = (uint16_t)(a & b);
        goto no_carry;
    case OP_OR:
        result = (uint16_t)(a | b);
        goto no_carry;
    case OP_XOR:
        result = (uint16_t)(a ^ b);
        goto no_carry;
    case OP_NOT:
        result = (uint16_t)(~a);
        goto no_carry;
    case OP_SHL:
        result32 = (uint32_t)a << (b & 0x0Fu);
        break;
    case OP_SHR:
        result = (uint16_t)(a >> (b & 0x0Fu));
        goto no_carry;
    default:
        return a;
    }

    result = (uint16_t)result32;

    /* Carry flag (for ADD/SUB/CMP/SHL paths only). */
    set_flag(cpu, FLAG_CF, result32 > 0xFFFFu);

    /* Overflow flag (signed), per team reference contract. */
    if (is_add_like) {
        uint16_t sign_a = (uint16_t)((a >> 15) & 1u);
        uint16_t sign_b = (uint16_t)((b >> 15) & 1u);
        uint16_t sign_r = (uint16_t)((result >> 15) & 1u);
        set_flag(cpu, FLAG_OF, (sign_a == sign_b) && (sign_r != sign_a));
    } else {
        set_flag(cpu, FLAG_OF, 0);
    }

no_carry:
    /* Zero and Negative flags always reflect ALU result. */
    update_zn_flags(cpu, result);

    return result;
}
