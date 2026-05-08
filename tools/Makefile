CC     := cc
CFLAGS := -std=c11 -Wall -Wextra -O2 -Iassembler

ASM_SRCS := assembler/main.c assembler/assembler.c assembler/lexer.c \
            assembler/parser.c assembler/symtable.c assembler/encoder.c
EMU_SRCS := $(wildcard emulator/*.c)

.PHONY: all clean

all: assembler_bin emulator_bin

assembler_bin: $(ASM_SRCS)
	$(CC) $(CFLAGS) $^ -o $@

emulator_bin: $(EMU_SRCS)
	$(CC) $(CFLAGS) $^ -o $@

clean:
	rm -f assembler_bin emulator_bin
