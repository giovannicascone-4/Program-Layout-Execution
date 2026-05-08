# Program Layout and Execution

This repository contains the code for the CMPE 220 Program Layout and Execution assignment. It demonstrates:

- a recursive C program using `factorial`
- a matching recursion walkthrough in native output
- an `educpu16` assembly program that shows function calls, recursion, stack growth, and stack unwinding

The assembly version uses recursive summation instead of factorial because the `educpu16` ISA does not include a multiply instruction. The recursion structure is the same, so it still demonstrates program layout, function calls, and recursion on the software CPU.

## Repository Layout

- `src/main.c`  
  Driver program that prints a factorial table, a call-chain trace, and a stack-depth illustration.
- `src/factorial.c`  
  Recursive factorial implementation with comments about call/return behavior.
- `src/factorial.h`  
  Public interface for the factorial function.
- `asm/sum_recursive.asm`  
  Recursive `educpu16` assembly demo showing stack-based function calls and recursion.
- `tools/assembler/`  
  Local assembler source used to build `assembler_bin`.
- `tools/emulator/`  
  Local emulator source used to build `emulator_bin`.
- `Makefile`  
  Main build and run commands for the assignment.

## Requirements

You need:

- a C compiler such as `cc`, `clang`, or `gcc`
- `make`

No external `educpu16` repository is required for this repo. The assembler and emulator are built from the local `tools/` directory.

## How to Build and Run

### 1. Run the native C recursion demo

```bash
make run
```

This builds `factorial_demo` and runs it.

What it shows:

- factorial values from `0!` through `12!`
- the call chain for `factorial(5)`
- a stack-frame illustration at peak recursion depth

### 2. Assemble the `educpu16` recursion program

```bash
make assemble
```

This builds the local assembler if needed and produces:

- `sum_recursive.bin`

### 3. Run the assembled program in the emulator

```bash
make emulate
```

Expected verification:

```text
0200: 000F
```

This means the program correctly computed:

```text
sum(5) = 15
```

and stored the result at memory address `0x0200`.

### 4. View a full execution trace

```bash
make trace
```

This prints CPU state after every instruction, which is useful for showing:

- `CALL` pushing return addresses
- `RET` popping return addresses
- stack pointer changes in `R7`
- recursion winding down and unwinding

### 5. Step through interactively

```bash
make step
```

This runs the emulator in single-step mode so you can advance one instruction at a time.

## What the Assembly Program Demonstrates

The assembly program in `asm/sum_recursive.asm` is the main CPU-level demonstration for this assignment.

It shows:

- where code, data, output, and stack live in memory
- how `main` sets up input and calls a recursive function
- how `CALL` and `RET` use the stack
- how each recursive invocation keeps its own saved state
- how the result is returned and written to memory

## Clean Up Generated Files

```bash
make clean
```

This removes:

- object files in `src/`
- `factorial_demo`
- `sum_recursive.bin`

If needed, local tool binaries can also be removed with:

```bash
make -C tools clean
```

## Notes for Submission

This repository is organized so that:

- the native C demo is separate from the `educpu16` assembly demo
- build commands are short and reproducible
- the software CPU tools are included locally for easier grading and verification

For the video/demo portion, the recommended focus is the assembly program, since it directly shows the executable layout in memory, function call handling, and recursion behavior on the software CPU.
