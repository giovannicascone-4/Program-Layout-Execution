/*
 * factorial.c -- Recursive factorial with annotated call mechanics.
 *
 * ═══════════════════════════════════════════════════════════════════════════
 * HOW A RECURSIVE CALL WORKS ON A REAL CPU (x86-64 / ARM64)
 * ═══════════════════════════════════════════════════════════════════════════
 *
 * When C executes  return (long)n * factorial(n - 1);  the compiler emits
 * roughly these steps for the CALL to factorial(n-1):
 *
 *   1. SAVE CALLER REGISTERS  — any register the callee might clobber that
 *      the caller still needs is spilled to the stack (here: the value of n).
 *
 *   2. SET UP ARGUMENT        — n-1 is placed in the argument register
 *      (rdi on x86-64 / w0 on ARM64).
 *
 *   3. CALL INSTRUCTION       — the CPU automatically pushes the return
 *      address (the address of the multiply instruction) onto the stack and
 *      transfers control to the first instruction of factorial().
 *
 *   4. EXECUTE CALLEE         — factorial(n-1) runs, recurses further if
 *      needed, and eventually reaches the base case and returns.
 *
 *   5. RET INSTRUCTION        — pops the saved return address and resumes
 *      execution in the caller at the multiply instruction.
 *
 *   6. RESTORE + COMBINE      — the saved n is restored from the stack, and
 *      n * return_value is computed to produce this frame's result.
 *
 * Stack snapshot while computing factorial(4):
 *
 *   High address ↑
 *   ┌─────────────────────────┐
 *   │  main()  frame          │  n=5, return addr to OS startup
 *   ├─────────────────────────┤
 *   │  factorial(5)  frame    │  saved n=5, return addr → main
 *   ├─────────────────────────┤
 *   │  factorial(4)  frame    │  saved n=4, return addr → fact(5)
 *   ├─────────────────────────┤
 *   │  factorial(3)  frame    │  saved n=3, return addr → fact(4)
 *   ├─────────────────────────┤
 *   │  factorial(2)  frame    │  ← currently executing
 *   └─────────────────────────┘
 *   Low address (rsp) ↓
 *
 * ═══════════════════════════════════════════════════════════════════════════
 * EDUCPU16 EQUIVALENT
 * ═══════════════════════════════════════════════════════════════════════════
 *
 * The educpu16 uses the same CALL/RET + stack mechanism but the ISA makes
 * every step explicit — there is no hardware frame pointer, no compiler
 * register allocator, and no implicit spilling.  Argument passing, saving
 * registers, and restoring them are all hand-written SW/LW instructions.
 *
 * Because educpu16 has no MUL instruction, the assembly demo (../asm/
 * sum_recursive.asm) computes sum(n) = n + (n-1) + ... + 1, which has
 * identical recursive structure but uses only ADD.  The stack mechanics
 * are exactly the same.
 *
 * Cross-reference map:
 *   C                           educpu16 assembly
 *   ──────────────────────────  ─────────────────────────────────────────
 *   n <= 1 → return 1           CMP R1, R0  /  JEQ sum_base
 *   CALL factorial(n-1)         ADDI R1,R1,-1  /  CALL sum
 *   compiler spills n           SW R1, R7, 0  /  ADDI R7, R7, -1
 *   RET (hardware pops addr)    RET  (SP++; PC = mem[SP])
 *   restore n from stack        ADDI R7,R7,1  /  LW R2, R7, 0
 *   n * result                  ADD R1, R1, R2  (sum uses ADD, not MUL)
 */

#include <stdio.h>
#include "factorial.h"

long factorial(int n)
{
    /* ── Base case ────────────────────────────────────────────────────────
     * 0! = 1  and  1! = 1.
     * On educpu16: CMP R1, R0 sets ZF; JEQ sum_base branches here.
     * No recursive call is made, so nothing is pushed to the stack beyond
     * the return address that CALL already put there.
     */
    if (n <= 1)
        return 1L;

    /* ── Recursive case ───────────────────────────────────────────────────
     * The compiler will:
     *   (a) save the current value of n on the stack (spill)
     *   (b) place n-1 in the argument register
     *   (c) emit CALL factorial  → pushes return addr, jumps
     * On educpu16 (a) and (b) are explicit SW and ADDI instructions;
     * (c) is the CALL instruction which only saves the return address.
     */
    return (long)n * factorial(n - 1);
}
