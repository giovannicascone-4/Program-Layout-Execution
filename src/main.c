/*
 * main.c -- Driver for the factorial recursion demonstration.
 *
 * ═══════════════════════════════════════════════════════════════════════════
 * PART 1: PROCESS MEMORY LAYOUT  (native x86-64 / ARM64, Linux or macOS)
 * ═══════════════════════════════════════════════════════════════════════════
 *
 * When the OS loads this executable, the virtual address space looks like:
 *
 *  High virtual address  0xFFFF_FFFF_FFFF_FFFF
 *  ┌─────────────────────────────────────────────────────────────────────┐
 *  │  Kernel space  (OS code, page tables — inaccessible to user code)   │
 *  ├─────────────────────────────────────────────────────────────────────┤
 *  │  Stack segment                                                       │
 *  │    Holds activation records (frames) for every active function call │
 *  │    Grows DOWNWARD (toward lower addresses) on each CALL             │
 *  │    Shrinks UPWARD  (toward higher addresses) on each RET            │
 *  │                                                                      │
 *  │    At program entry (deepest stack state for factorial(5)):         │
 *  │      frame: main()                        ← oldest frame, deepest  │
 *  │      frame: factorial(5)  n=5, ret→main                            │
 *  │      frame: factorial(4)  n=4, ret→fact5                           │
 *  │      frame: factorial(3)  n=3, ret→fact4                           │
 *  │      frame: factorial(2)  n=2, ret→fact3                           │
 *  │      frame: factorial(1)  n=1, ret→fact2  ← newest frame (top)    │
 *  │                                                          rsp ↑      │
 *  ├─────────────────────────────────────────────────────────────────────┤
 *  │  (unmapped guard page — stack overflow causes SIGSEGV here)         │
 *  ├─────────────────────────────────────────────────────────────────────┤
 *  │  Heap segment  (grows UPWARD; managed by malloc/free)               │
 *  │  (not used by this program, but present in every process)           │
 *  ├─────────────────────────────────────────────────────────────────────┤
 *  │  .bss segment   — uninitialised globals / statics (zeroed by OS)    │
 *  ├─────────────────────────────────────────────────────────────────────┤
 *  │  .data segment  — initialised globals / statics                     │
 *  │    (none in this program)                                           │
 *  ├─────────────────────────────────────────────────────────────────────┤
 *  │  .rodata segment — read-only data (string literals for printf)      │
 *  │    "=== Factorial Recursion Demo ===\n"  lives here                 │
 *  │    "%2d! = %ld\n"  lives here                                       │
 *  ├─────────────────────────────────────────────────────────────────────┤
 *  │  .text segment  — executable machine code (READ + EXEC, not WRITE)  │
 *  │    main()         function body                                     │
 *  │    factorial()    function body                                     │
 *  │    C runtime startup (_start / __main)                              │
 *  └─────────────────────────────────────────────────────────────────────┘
 *  Low virtual address  0x0000_0000_0000_0000
 *
 * ═══════════════════════════════════════════════════════════════════════════
 * PART 2: EDUCPU16 MEMORY LAYOUT  (word-addressable 64K flat space)
 * ═══════════════════════════════════════════════════════════════════════════
 *
 * The educpu16 has a single, flat 65 536-word address space.  Every address
 * holds one 16-bit word.  There is no MMU and no virtual memory.
 *
 *  Address       Contents
 *  ──────────────────────────────────────────────────────────────────────
 *  0x0000–0x0007  .text (main)    8 instructions: setup, CALL sum, store, HALT
 *  0x0008–0x0013  .text (sum)    12 instructions: the recursive function
 *  0x0014–0x01FF  (unused code space)
 *  0x0200         .data (output)  sum(5) result written here after HALT
 *  0x0201–0xFEFE  (unused data / heap area — free for future use)
 *  0xFEFF         STACK_BASE      initial value of SP (R7); stack grows ↓
 *  0xFF00         IO_STDOUT       MMIO: write a character to the terminal
 *  0xFF01         IO_STDIN        MMIO: read a character from the terminal
 *  0xFF02         IO_TIMER        MMIO: read the current timer tick
 *  0xFF03         IO_STATUS       MMIO: status register
 *  ──────────────────────────────────────────────────────────────────────
 *
 *  Stack at peak recursion depth (sum(5) → sum(0), 6 frames active):
 *
 *  Address  Contents
 *  ───────  ─────────────────────────────────────────────────────────────
 *  0xFEFF   return address → main (written by the initial CALL from main)
 *  0xFEFE   saved n = 5    (pushed manually by sum before recursing)
 *  0xFEFD   return address → sum(5)'s continuation
 *  0xFEFC   saved n = 4
 *  0xFEFB   return address → sum(4)'s continuation
 *  0xFEFA   saved n = 3
 *  0xFEF9   return address → sum(3)'s continuation
 *  0xFEF8   saved n = 2
 *  0xFEF7   return address → sum(2)'s continuation
 *  0xFEF6   saved n = 1
 *  0xFEF5   return address → sum(1)'s continuation
 *  0xFEF4   ← SP (next free slot; sum(0) base case reached here)
 *  ───────  ─────────────────────────────────────────────────────────────
 *  10 words consumed (0xFEFF down to 0xFEF5; SP = 0xFEF4)
 *
 * See ../asm/sum_recursive.asm for the full step-by-step assembly.
 */

#include <stdio.h>
#include "factorial.h"

/* ─── helpers ────────────────────────────────────────────────────────────── */

/* Print a header banner. */
static void banner(const char *title)
{
    printf("\n=== %s ===\n", title);
}

/* ─── main ───────────────────────────────────────────────────────────────── */

int main(void)
{
    int  n;
    long result;

    banner("Factorial Recursion Demo");

    /* ── Table of n! for n = 0 .. 12 ───────────────────────────────────── */
    printf("\n  n     n!\n");
    printf("  ─────────────\n");
    for (n = 0; n <= 12; ++n) {
        result = factorial(n);
        printf("  %2d    %ld\n", n, result);
    }

    /* ── Annotated call-chain trace for factorial(5) ───────────────────── */
    banner("Call-Chain Trace: factorial(5)");
    printf("\n");
    printf("  CALL  factorial(5)\n");
    printf("    CALL  factorial(4)           ; push ret_addr, jump\n");
    printf("      CALL  factorial(3)         ; push ret_addr, jump\n");
    printf("        CALL  factorial(2)       ; push ret_addr, jump\n");
    printf("          CALL  factorial(1)     ; push ret_addr, jump\n");
    printf("          RET   1               ; base case: n<=1, return 1\n");
    printf("        RET   2 * 1 = 2         ; unwind factorial(2)\n");
    printf("      RET   3 * 2 = 6           ; unwind factorial(3)\n");
    printf("    RET   4 * 6 = 24            ; unwind factorial(4)\n");
    printf("  RET   5 * 24 = 120            ; unwind factorial(5)\n");
    printf("\n  Result: factorial(5) = %ld\n", factorial(5));

    /* ── Stack-depth illustration ───────────────────────────────────────── */
    banner("Stack Depth at Peak (inside factorial(1))");
    printf("\n");
    printf("  [main]          <- oldest frame (bottom of call stack)\n");
    printf("  [factorial(5)]  saved n=5,  ret-addr -> main\n");
    printf("  [factorial(4)]  saved n=4,  ret-addr -> factorial(5)\n");
    printf("  [factorial(3)]  saved n=3,  ret-addr -> factorial(4)\n");
    printf("  [factorial(2)]  saved n=2,  ret-addr -> factorial(3)\n");
    printf("  [factorial(1)]  saved n=1,  ret-addr -> factorial(2) <- top (rsp)\n");

    /* ── Point to the assembly equivalent ──────────────────────────────── */
    banner("educpu16 Assembly Equivalent");
    printf("\n");
    printf("  See  asm/sum_recursive.asm  for the educpu16 version.\n");
    printf("  sum(n) = n + (n-1) + ... + 1  (same structure; ADD replaces MUL)\n");
    printf("  Assemble and run:\n");
    printf("    make assemble\n");
    printf("    make emulate\n");
    printf("  Expected result at 0x0200:  sum(5) = 15  (0x000F)\n\n");

    return 0;
}
