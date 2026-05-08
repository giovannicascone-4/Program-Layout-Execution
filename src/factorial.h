/*
 * factorial.h -- Public interface for the recursive factorial function.
 *
 * The function computes n! = n * (n-1) * ... * 2 * 1, with 0! = 1! = 1.
 * Valid input range: 0 <= n <= 20  (21! overflows a 64-bit signed integer).
 */
#ifndef FACTORIAL_H
#define FACTORIAL_H

/*
 * factorial(n) -- returns n! as a long (64-bit on LP64 platforms).
 *
 * Each call pushes an activation record on the native stack:
 *   - the return address   (pushed automatically by the CALL instruction)
 *   - the saved base pointer rbp
 *   - the local copy of n  (compiler places it in the frame)
 *
 * On educpu16 the same three elements exist, but the ISA is explicit:
 * CALL pushes only the return address; everything else must be done
 * manually with SW/LW and ADDI on R7 (the stack pointer).
 * See ../asm/sum_recursive.asm for the direct assembly analogue.
 */
long factorial(int n);

#endif /* FACTORIAL_H */
