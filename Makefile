# =============================================================================
# Makefile  —  Program-Layout-Execution
#
# Targets
#   all        Build the native C demo (default)
#   run        Build and run the native C demo
#   assemble   Assemble sum_recursive.asm using the educpu16 assembler
#   emulate    Run the assembled binary on the educpu16 emulator
#   trace      Run with --trace (prints CPU state after every instruction)
#   step       Run in interactive single-step mode (press Enter to advance)
#   clean      Remove all generated files
#
# Prerequisites
#   C compiler (cc / clang / gcc) for the native demo
#   educpu16 assembler and emulator must be built first:
#     cd ../educpu16 && make -f Makefile_assembler && make
# =============================================================================

# ── Native C demo ─────────────────────────────────────────────────────────────

CC      := cc
CFLAGS  := -std=c11 -Wall -Wextra -g

BIN     := factorial_demo
SRCS    := src/main.c src/factorial.c
OBJS    := $(SRCS:.c=.o)

.PHONY: all run clean assemble emulate trace step

all: $(BIN)

$(BIN): $(OBJS)
	$(CC) $(CFLAGS) -o $@ $^

src/%.o: src/%.c src/factorial.h
	$(CC) $(CFLAGS) -c -o $@ $<

run: $(BIN)
	./$(BIN)

# ── educpu16 assembly demo ────────────────────────────────────────────────────

EDUCPU      := ../educpu16
ASM_BIN     := $(EDUCPU)/assembler_bin
EMU_BIN     := $(EDUCPU)/emulator_bin
ASM_SRC     := asm/sum_recursive.asm
ASM_OUT     := sum_recursive.bin

assemble: $(ASM_OUT)

$(ASM_OUT): $(ASM_SRC) $(ASM_BIN)
	$(ASM_BIN) $(ASM_SRC) -o $(ASM_OUT) --listing --symbols

emulate: $(ASM_OUT) $(EMU_BIN)
	$(EMU_BIN) $(ASM_OUT) --dump --addr 0200
	@echo ""
	@echo "Expected: 0200: 000F ...  (sum(5) = 15)"

trace: $(ASM_OUT) $(EMU_BIN)
	$(EMU_BIN) $(ASM_OUT) --trace --dump --addr 0200

step: $(ASM_OUT) $(EMU_BIN)
	$(EMU_BIN) $(ASM_OUT) --step --dump --addr 0200

# ── Cleanup ───────────────────────────────────────────────────────────────────

clean:
	rm -f $(OBJS) $(BIN) $(ASM_OUT)
