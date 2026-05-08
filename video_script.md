# Video Script — Function Calls and Recursion on the educpu16

**Course:** CMPE 220 — Computer Architecture  
**Project:** Program Layout and Execution  
**Duration:** ~12–15 minutes

---

## [INTRO — 0:00]

Hello, and welcome to this walkthrough of function calls and recursion on the
educpu16, our custom 16-bit software CPU.

In this video we'll cover three things:

1. How a running program is **laid out in memory** — both on a real OS and on
   the educpu16's flat 64K address space.
2. How the **CALL and RET instructions** implement function calls, including
   what happens to the stack pointer and program counter at each step.
3. How **recursion** reuses the same function body while keeping each call's
   data isolated on the stack — and how it unwinds cleanly.

We'll start with the C source code, connect every concept to the educpu16 ISA,
and finish with a live trace through the stack.

---

## [SECTION 1 — Program Memory Layout — 1:00]

### The native picture (C on x86-64 / ARM64)

Open `src/main.c`.  At the top you'll see a comment block showing the virtual
address space of our process.

When the OS loads our `factorial_demo` executable it sets up several **segments**
in memory:

```
High address 0xFFFF...FFFF
┌─────────────────────────────────────────────┐
│  Kernel space  (OS — inaccessible)          │
├─────────────────────────────────────────────┤
│  Stack         grows ↓  (each CALL adds)    │
├─────────────────────────────────────────────┤
│  (guard page — stack overflow → crash here) │
├─────────────────────────────────────────────┤
│  Heap          grows ↑  (malloc/free)       │
├─────────────────────────────────────────────┤
│  .bss          zero-initialised globals     │
├─────────────────────────────────────────────┤
│  .data         initialised globals          │
├─────────────────────────────────────────────┤
│  .rodata       string literals (printf fmt) │
├─────────────────────────────────────────────┤
│  .text         machine code: main, factorial│
└─────────────────────────────────────────────┘
Low address 0x0000...0000
```

The key insight: **code lives at the low end, the stack at the high end, growing
toward each other.**  The OS ensures they never collide (guard page).

### The educpu16 picture

Now open `asm/sum_recursive.asm` and look at the memory map at the top.

The educpu16 has **no MMU** — there are no separate segments, no virtual memory,
no protection.  Everything lives in a single flat 65 536-word space:

```
Address     Contents
─────────   ────────────────────────────────────────────────────
0x0000      First instruction — main begins here
0x0008      First instruction of the sum function
0x0200      Output slot — we store sum(5) here after HALT
0xFEFF      STACK_BASE — SP (R7) starts here; stack grows ↓
0xFF00      IO_STDOUT — MMIO write-only port
0xFF01      IO_STDIN  — MMIO read-only port
```

**Notice the same pattern:**  code at the low end (0x0000), stack at the high
end (0xFEFF), data in between (0x0200).  The educpu16 just lacks the hardware
and OS support to enforce boundaries.

---

## [SECTION 2 — The Function Call Mechanism — 3:00]

### What CALL actually does

On educpu16 the CALL instruction does two things atomically:

```
CALL target:
  1.  mem[SP] = PC          ; push the return address (PC is already
                             ; incremented past the CALL instruction)
  2.  SP = SP - 1           ; move SP to the next free slot
  3.  PC = PC + offset      ; jump to the target label
```

And RET is the mirror image:

```
RET:
  1.  SP = SP + 1           ; pop: move SP back to the return-address slot
  2.  PC = mem[SP]          ; load the return address into PC
```

This is exactly what a hardware CALL/RET pair does on x86-64 — the only
difference is that x86-64 uses a byte-addressed 64-bit stack whereas educpu16
uses a word-addressed 16-bit stack.

### Stepping through `main → sum(5)`

Let's trace the very first call.  Before CALL:

```
PC = 0x0005  (the CALL sum instruction)
SP = R7 = 0xFEFF  (STACK_BASE, the initial value)
R1 = 5            (argument)
```

The fetch-decode-execute cycle fetches the instruction at 0x0005 and
increments PC to 0x0006, so the return address is 0x0006 — the SW
instruction that stores our result.

CALL executes:
```
mem[0xFEFF] = 0x0006   ; push return address
SP = 0xFEFE            ; SP decremented
PC = 0x0006 + offset   ; jumps to 'sum' (0x0008)
```

Stack immediately after the call:
```
0xFEFF: 0x0006   ← return address to main's SW instruction
         SP = 0xFEFE  ← next free slot
```

Control transfers to address 0x0008 — the first instruction of `sum`.

---

## [SECTION 3 — The Recursive Function — 5:30]

### C source (factorial.c)

Open `src/factorial.c`.  The C function is:

```c
long factorial(int n)
{
    if (n <= 1)
        return 1L;                    // base case
    return (long)n * factorial(n-1); // recursive case
}
```

Two parts:
- **Base case** — when n ≤ 1, return immediately.  No further calls, no
  additional stack growth.
- **Recursive case** — save the current n, call factorial(n-1), then
  multiply the returned result by n and return that.

The key constraint: **n must be saved before the recursive call** because
the call will overwrite the register holding n with the result.

### educpu16 assembly (sum_recursive.asm)

The educpu16 ISA has no MUL, so our assembly computes `sum(n) = n + (n-1) + … + 0`
instead of factorial — the recursive structure is identical; ADD replaces MUL.

Look at the `sum` label.  The function body is:

```asm
sum:
    CMP  R1, R0          ; is n == 0?
    JEQ  sum_base        ; yes → base case

    ; ── Save n before the recursive call ──
    SW   R1, R7, 0       ; mem[SP] = n
    ADDI R7, R7, -1      ; SP -= 1

    ADDI R1, R1, -1      ; argument = n-1
    CALL sum             ; recursive call

    ; ── Restore n after returning ──
    ADDI R7, R7, 1       ; SP += 1  (point back to saved n)
    LW   R2, R7, 0       ; R2 = saved n

    ADD  R1, R1, R2      ; R1 = sum(n-1) + n
    RET

sum_base:
    MOV  R1, 0           ; return 0
    RET
```

Let me walk through this instruction by instruction.

**CMP R1, R0** — subtracts R0 (always 0) from R1 and sets the Zero Flag if the
result is zero.  If n == 0, ZF = 1.

**JEQ sum_base** — if ZF is set, jump to the base case.  Otherwise fall through.

**SW R1, R7, 0** — store word: writes the value of R1 (= n) to the memory
address R7 + 0, which is the current stack pointer.  This **pushes n onto the
stack** — same as what a compiler does when it spills a register.

**ADDI R7, R7, -1** — decrements the stack pointer, opening up the next free
slot for future pushes (or for CALL's return address).

**ADDI R1, R1, -1** — sets up the argument for the recursive call: n-1.

**CALL sum** — pushes the return address (address of the ADDI below it) and
jumps back to the beginning of sum.  This is the recursive call.

After returning:

**ADDI R7, R7, 1** — moves SP up one slot, pointing it back at the saved n.

**LW R2, R7, 0** — load word: reads from mem[SP], which contains the n we
saved before.  This is the **pop** operation.

**ADD R1, R1, R2** — combines: R1 = sum(n-1) [returned result] + R2 [saved n].

**RET** — pops the return address and jumps back to the caller.

---

## [SECTION 4 — Stack Trace: Winding Down — 8:00]

Now let's watch the stack grow as recursion digs deeper into sum(5).

I'll list the stack slot address, what is stored there, and what SP equals
at each significant moment.

```
Event                       SP      What was just written
──────────────────────────────────────────────────────────────────────
Program starts              0xFEFF  (stack empty)
main: CALL sum(5)           0xFEFE  0xFEFF ← 0x0006  (ret to main)
sum(5): push n=5            0xFEFE  0xFEFE ← 5
        ADDI SP, -1         0xFEFD
        CALL sum(4)         0xFEFC  0xFEFD ← 0x000E  (ret to sum5)
sum(4): push n=4            0xFEFC  0xFEFC ← 4
        ADDI SP, -1         0xFEFB
        CALL sum(3)         0xFEFA  0xFEFB ← 0x000E
sum(3): push n=3            0xFEFA  0xFEFA ← 3
        ADDI SP, -1         0xFEF9
        CALL sum(2)         0xFEF8  0xFEF9 ← 0x000E
sum(2): push n=2            0xFEF8  0xFEF8 ← 2
        ADDI SP, -1         0xFEF7
        CALL sum(1)         0xFEF6  0xFEF7 ← 0x000E
sum(1): push n=1            0xFEF6  0xFEF6 ← 1
        ADDI SP, -1         0xFEF5
        CALL sum(0)         0xFEF4  0xFEF5 ← 0x000E  ← PEAK DEPTH
```

At peak depth, SP = **0xFEF4** and the stack occupies 11 words from 0xFEF5 to
0xFEFF.  Each of the five recursive frames used two slots — one for the
return address (pushed by CALL) and one for the saved n (pushed manually).
The initial call from main used one additional slot (return to main).

Visually, the stack at peak depth:

```
Address   Value     Meaning
────────  ────────  ─────────────────────────────────────────────────
0xFEFF    0x0006    return address → main's SW instruction
0xFEFE    0x0005    saved n=5  (sum(5)'s frame)
0xFEFD    0x000E    return address → sum(5)'s continuation
0xFEFC    0x0004    saved n=4  (sum(4)'s frame)
0xFEFB    0x000E    return address → sum(4)'s continuation
0xFEFA    0x0003    saved n=3  (sum(3)'s frame)
0xFEF9    0x000E    return address → sum(3)'s continuation
0xFEF8    0x0002    saved n=2  (sum(2)'s frame)
0xFEF7    0x000E    return address → sum(2)'s continuation
0xFEF6    0x0001    saved n=1  (sum(1)'s frame)
0xFEF5    0x000E    return address → sum(1)'s continuation
0xFEF4    ───       SP (next free — sum(0) reaches base case here)
```

---

## [SECTION 5 — Stack Trace: Unwinding — 10:00]

sum(0) is the base case.  CMP sees n=0, ZF=1, JEQ branches to sum_base.
MOV R1, 0 sets the return value to 0, then RET executes:

```
RET (sum(0)):
  SP = SP + 1 = 0xFEF5
  PC = mem[0xFEF5] = 0x000E   (sum(1)'s continuation)
  R1 = 0
```

We're now back in sum(1)'s continuation at address 0x000E:

```
ADDI R7, R7, 1    ; SP = 0xFEF5 + 1 = 0xFEF6
LW R2, R7, 0      ; R2 = mem[0xFEF6] = 1     (sum(1)'s saved n)
ADD R1, R1, R2    ; R1 = 0 + 1 = 1            (sum(1) = 1)
RET               ; SP = 0xFEF7, PC = 0x000E (sum(2)'s continuation)
```

Continuing the unwind:

```
sum(2) continuation:
  ADDI R7,+1 → SP=0xFEF8 ;  LW R2 → R2=2 ;  ADD R1=1+2=3 ;  RET → SP=0xFEF9

sum(3) continuation:
  ADDI R7,+1 → SP=0xFEFA ;  LW R2 → R2=3 ;  ADD R1=3+3=6 ;  RET → SP=0xFEFB

sum(4) continuation:
  ADDI R7,+1 → SP=0xFEFC ;  LW R2 → R2=4 ;  ADD R1=6+4=10 ;  RET → SP=0xFEFD

sum(5) continuation:
  ADDI R7,+1 → SP=0xFEFE ;  LW R2 → R2=5 ;  ADD R1=10+5=15 ; RET → SP=0xFEFF
```

RET from sum(5) pops the return address at 0xFEFF = 0x0006 and jumps back to
main's SW instruction.  SP is restored to 0xFEFF — exactly where it started.

**The stack is clean.**  Every push was matched by a pop; every CALL was
matched by a RET.

Main then executes:

```
SW R1, R3, 0  ; mem[0x0200] = 15   (R3 holds 0x0200, built with SHL earlier)
HALT
```

---

## [SECTION 6 — Running It — 12:00]

### Native C demo

```bash
make run
```

You'll see a table of factorials from 0! to 12!, followed by an annotated
call-chain trace showing how 5! = 120 is reached step by step.

### educpu16 assembly demo

First make sure the educpu16 tools are built:

```bash
cd ../educpu16
make -f Makefile_assembler
make
cd ../Program-Layout-Execution
```

Then assemble and run:

```bash
make assemble    # produces sum_recursive.bin with a listing
make emulate     # runs on the emulator; dumps memory at 0x0200
```

Expected output:

```
0200: 000F ...
Expected: 0200: 000F ...  (sum(5) = 15)
```

For a full instruction-by-instruction trace (shows every register and flag):

```bash
make trace
```

Use `make step` to advance one instruction at a time and watch the stack
pointer R7 and register R1 change with each CALL and RET.

---

## [SUMMARY — 13:30]

Let's recap what we've seen:

| Concept               | Native C / x86-64               | educpu16                          |
|-----------------------|---------------------------------|-----------------------------------|
| Code lives at         | Low .text segment               | 0x0000 onward                     |
| Stack lives at        | High virtual address, grows ↓   | 0xFEFF (STACK_BASE), grows ↓      |
| CALL instruction      | Pushes ret addr, jumps          | Pushes ret addr to mem[SP], SP--, PC+=offset |
| RET  instruction      | Pops ret addr, jumps            | SP++, PC = mem[SP]                |
| Save n before recurse | Compiler spills to stack        | Explicit SW + ADDI SP,-1          |
| Restore n after ret   | Compiler reloads from stack     | Explicit ADDI SP,+1 + LW         |
| Stack depth for sum(5)| 6 frames (varies by ABI)        | 11 words (0xFEF5–0xFEFF)         |
| Result location       | Return register (rax / x0)      | R1                                |

The educpu16 makes every step **visible and explicit** — there is no hidden
compiler magic, no ABI complexity, no hardware register windows.  What you
write in assembly is exactly what the CPU executes.

Thanks for watching.

---

*Files referenced in this script:*
- [src/factorial.c](src/factorial.c) — recursive C function with call-mechanic annotations
- [src/main.c](src/main.c) — driver with full memory-layout diagrams
- [asm/sum_recursive.asm](asm/sum_recursive.asm) — educpu16 assembly with embedded stack trace
- [Makefile](Makefile) — build, assemble, emulate, trace targets
