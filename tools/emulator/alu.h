#pragma once

#include <stdint.h>

#include "cpu.h"

uint16_t alu_execute(CPU *cpu, uint8_t op, uint16_t a, uint16_t b);
