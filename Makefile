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
#   The assembler and emulator are built automatically from tools/ when needed
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

TOOLS       := tools
ASM_BIN     := $(TOOLS)/assembler_bin
EMU_BIN     := $(TOOLS)/emulator_bin
ASM_SRC     := asm/sum_recursive.asm
ASM_OUT     := sum_recursive.bin

$(ASM_BIN):
	$(MAKE) -C $(TOOLS) assembler_bin

$(EMU_BIN):
	$(MAKE) -C $(TOOLS) emulator_bin

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
