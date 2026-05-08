#pragma once

#include <stdint.h>

#include "../assembler/isa.h"

uint16_t mem_read(uint16_t addr);
void mem_write(uint16_t addr, uint16_t val);
void mem_load(const uint16_t *program, uint16_t len, uint16_t base);
void mem_dump(uint16_t start, uint16_t end);
