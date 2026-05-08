; =============================================================================
; sum_recursive.asm  —  Program-Layout-Execution / educpu16 demonstration
;
; PURPOSE
;   Demonstrates recursive function calls, function-call conventions, and
;   the run-time stack on the educpu16 16-bit CPU.
;
;   The program computes:
;       sum(5)  =  5 + 4 + 3 + 2 + 1 + 0  =  15  (0x000F)
;   by calling a recursive function that mirrors the structure of the C
;   factorial function in ../src/factorial.c.
;
;   NOTE: The educpu16 ISA has no MUL instruction, so sum(n) is used instead
;   of factorial(n).  Both have the same recursive shape:
;       sum(0)     = 0                   (base case)
;       sum(n)     = n + sum(n-1)        (recursive case, uses ADD)
;       factorial(0) = 1                 (base case)
;       factorial(n) = n * factorial(n-1)(recursive case, uses MUL)
;   Every CALL, RET, and stack-frame detail shown here applies equally to
;   the factorial version.
;
; ─────────────────────────────────────────────────────────────────────────────
; PROGRAM MEMORY MAP  (word-addressable 64K flat space)
; ─────────────────────────────────────────────────────────────────────────────
;
;  Address    Contents
;  ─────────  ───────────────────────────────────────────────────────────────
;  0x0000     MOV  R0, 0              ; main: initialise zero register
;  0x0001     MOV  R3, 2              ; main: start building output addr
;  0x0002     MOV  R4, 8              ; main: shift amount
;  0x0003     SHL  R3, R3, R4         ; main: R3 = 0x0200 (output address)
;  0x0004     MOV  R1, 5              ; main: argument n = 5
;  0x0005     CALL sum                ; main: call sum(5), offset = +2
;  0x0006     SW   R1, R3, 0          ; main: mem[0x0200] = result (15)
;  0x0007     HALT                    ; main: end of program
;
;  0x0008     CMP  R1, R0             ; sum: test n == 0?
;  0x0009     JEQ  sum_base           ; sum: if n==0 jump to base case (+9)
;  0x000A     SW   R1, R7, 0          ; sum: push n  (mem[SP] = n)
;  0x000B     ADDI R7, R7, -1         ; sum: SP -= 1
;  0x000C     ADDI R1, R1, -1         ; sum: arg = n-1
;  0x000D     CALL sum                ; sum: recursive call, offset = -6
;  0x000E     ADDI R7, R7, 1          ; sum: SP += 1  (point to saved n)
;  0x000F     LW   R2, R7, 0          ; sum: R2 = saved n (pop)
;  0x0010     ADD  R1, R1, R2         ; sum: R1 = sum(n-1) + n
;  0x0011     RET                     ; sum: return
;  0x0012     MOV  R1, 0              ; sum_base: return 0
;  0x0013     RET                     ; sum_base: return
;
;  0x0014–    (unused; available for future functions)
;  0x01FF
;
;  0x0200     result of sum(5) = 15 (0x000F) — written by SW at 0x0006
;
;  0xFEF4     SP at peak depth (sum(0) just reached)  ← 11 slots used
;  0xFEF5     return address pushed by sum(1)'s CALL sum
;  0xFEF6     saved n = 1   (pushed manually by sum(1))
;  0xFEF7     return address pushed by sum(2)'s CALL sum
;  0xFEF8     saved n = 2
;  0xFEF9     return address pushed by sum(3)'s CALL sum
;  0xFEFA     saved n = 3
;  0xFEFB     return address pushed by sum(4)'s CALL sum
;  0xFEFC     saved n = 4
;  0xFEFD     return address pushed by sum(5)'s CALL sum
;  0xFEFE     saved n = 5
;  0xFEFF     STACK_BASE — return address pushed by main's CALL sum
;
;  0xFF00     IO_STDOUT   MMIO
;  0xFF01     IO_STDIN    MMIO
;  0xFF02     IO_TIMER    MMIO
;  0xFF03     IO_STATUS   MMIO
;
; ─────────────────────────────────────────────────────────────────────────────
; CALLING CONVENTION  (educpu16 convention used in this file)
; ─────────────────────────────────────────────────────────────────────────────
;
;  Argument   : R1  — caller places argument here before CALL
;  Return val : R1  — callee places result here before RET
;  Callee     : must save and restore any register it clobbers
;               (this function saves R1 = n before the recursive CALL)
;  Stack      : SP (R7) points to the NEXT FREE slot; grows downward
;
;  Push idiom (mirrors CALL behaviour):
;      SW   Rx, R7, 0      ; write to current SP
;      ADDI R7, R7, -1     ; SP -= 1 (advance past the written slot)
;
;  Pop idiom (mirrors RET behaviour):
;      ADDI R7, R7, 1      ; SP += 1 (back to the written slot)
;      LW   Rx, R7, 0      ; read from the slot
;
; ─────────────────────────────────────────────────────────────────────────────
; STACK FRAME  for one call to sum(n > 0)
; ─────────────────────────────────────────────────────────────────────────────
;
;  Higher addresses ↑
;  ┌──────────────────────────────────────────────────┐
;  │  ... (frames of callers above this one) ...      │
;  ├──────────────────────────────────────────────────┤
;  │  return address  ← pushed by CALL in the caller │  ← SP+2  on entry
;  ├──────────────────────────────────────────────────┤
;  │  saved n         ← pushed manually by this sum  │  ← SP+1  after push
;  ├──────────────────────────────────────────────────┤
;  │  (next free slot)                                │  ← SP    after push
;  └──────────────────────────────────────────────────┘
;  Lower addresses ↓
;
;  Two words per frame × 6 active frames (sum 5..0) = 12 words used.
;  Return address for the initial call from main occupies 0xFEFF (1 extra),
;  giving 11 slots total (0xFEFF down to 0xFEF5, SP = 0xFEF4 at peak depth).
;
; ─────────────────────────────────────────────────────────────────────────────
; REGISTER MAP
; ─────────────────────────────────────────────────────────────────────────────
;
;  R0  permanent zero  — MOV R0,0 once at startup; never overwritten
;  R1  argument / return value
;  R2  scratch         — used to retrieve saved n during unwinding
;  R3  output address  — 0x0200, built in main with SHL
;  R4  shift amount    — 8, used once to build R3, then unused
;  R7  SP (stack pointer) — managed by CALL, RET, and explicit ADDI
;
; =============================================================================


; ─── main ─────────────────────────────────────────────────────────────────────
;
; Entry point.  Sets up constants, calls sum(5), stores the result at 0x0200,
; and halts.  The emulator loads instructions starting at address 0x0000.

        ; R0 is used as the "permanent zero" constant throughout the program.
        ; Because the ISA has no register-to-register MOV, we rely on ADD Rd, Rs, R0
        ; (which copies Rs) to move values.  R0 = 0 enables this idiom safely.
        MOV  R0, 0          ; R0 = 0  (never overwritten after this point)

        ; Build output address 0x0200 = 2 << 8.
        ; MOV carries only a signed 5-bit immediate (-16 to 15), so 0x0200 = 512
        ; cannot be loaded directly.  Shift a small constant into place instead.
        MOV  R3, 2          ; R3 = 2
        MOV  R4, 8          ; R4 = 8  (shift amount)
        SHL  R3, R3, R4     ; R3 = 2 << 8 = 0x0200  (output destination)

        ; Call sum(5).
        ; CALL pushes the address of the NEXT instruction (0x0006, the SW below)
        ; onto the stack at mem[SP=0xFEFF], decrements SP to 0xFEFE, then
        ; transfers control to the 'sum' label.
        MOV  R1, 5          ; R1 = 5  (argument n)
        CALL sum            ; sum(5) → R1 on return contains 15

        ; Store the result for emulator verification.
        ; Run with:  ./emulator_bin sum_recursive.bin --dump --addr 0200
        ; Expected:  0200: 000F ...
        SW   R1, R3, 0      ; mem[0x0200] = sum(5) = 15
        HALT


; ─── sum ──────────────────────────────────────────────────────────────────────
;
; Computes sum(n) = 0 + 1 + ... + n  by linear recursion.
;
;   In:  R1 = n  (non-negative integer, caller-provided)
;   Out: R1 = sum(n)
;   Clobbers: R2 (scratch for popped n during unwind)
;
; Call graph for sum(5):
;   main ──CALL──► sum(5) ──CALL──► sum(4) ──CALL──► sum(3)
;          ◄──RET──        ◄──RET──          ◄──RET──
;                                    ──CALL──► sum(2) ──CALL──► sum(1) ──CALL──► sum(0)
;                                    ◄──RET──          ◄──RET──          ◄──RET──
;
; Values returned at each level (unwinding):
;   sum(0)=0, sum(1)=0+1=1, sum(2)=1+2=3, sum(3)=3+3=6, sum(4)=6+4=10, sum(5)=10+5=15

sum:
        ; ── Base case ──────────────────────────────────────────────────────────
        ; If n == 0, sum(0) = 0.  Return immediately without touching the stack
        ; beyond the return address that CALL already pushed.
        ;
        ; CMP sets the Zero Flag (ZF) when R1 - R0 == 0, i.e. when R1 == 0.
        CMP  R1, R0         ; flags ← R1 - 0;  ZF=1 iff n == 0
        JEQ  sum_base       ; if ZF=1 (n == 0), jump to base-case epilogue

        ; ── Recursive case ─────────────────────────────────────────────────────
        ;
        ; Before making the recursive call we must preserve the current value of
        ; n (in R1).  The recursive call will overwrite R1 with sum(n-1), and we
        ; need n to compute the final result  sum(n-1) + n  after returning.
        ;
        ; Step 1 — Push n onto the stack.
        ;   SP (R7) points to the NEXT FREE slot.  We write n there, then move
        ;   SP one slot lower, exactly mirroring what CALL does with the return
        ;   address.
        SW   R1, R7, 0      ; mem[SP] = n  (write current n to the free slot)
        ADDI R7, R7, -1     ; SP -= 1      (SP now points to the slot below n)

        ; Step 2 — Prepare argument and make the recursive call.
        ;   CALL will push the return address (address of the ADDI at 0x000E) to
        ;   mem[SP], decrement SP again, then jump to 'sum'.
        ADDI R1, R1, -1     ; R1 = n - 1  (argument for the recursive call)
        CALL sum            ; ── recursive call ── pushes ret addr, jumps to sum

        ; ── Post-call (continuation) ───────────────────────────────────────────
        ; Execution resumes here (address 0x000E) once the recursive call returns.
        ; At this point:
        ;   R1 = sum(n-1)   (result from the deeper call)
        ;   SP = the slot that CALL used for its return address  (already consumed
        ;        by RET; the slot holds a stale address, but SP now points to it)
        ;
        ; Stack layout relative to this continuation (SP → stale ret slot):
        ;
        ;   Higher addresses ↑
        ;   ... (callers' frames) ...
        ;   [ return address to our own caller ]  ← SP+2
        ;   [ saved n = our argument           ]  ← SP+1   ← we want this
        ;   [ stale ret addr (just popped)     ]  ← SP     ← SP currently here
        ;   Lower addresses ↓
        ;
        ; Step 3 — Recover saved n from the stack.
        ADDI R7, R7, 1      ; SP += 1  (move SP up to the saved-n slot)
        LW   R2, R7, 0      ; R2 = saved n  (read and "pop" it)

        ; Step 4 — Combine and return.
        ADD  R1, R1, R2     ; R1 = sum(n-1) + n  =  sum(n)

        ; RET pops the return address: SP += 1, then PC = mem[SP].
        ; This transfers control back to whoever called sum(n).
        RET

sum_base:
        ; Base case: sum(0) = 0.
        ; R1 is already 0 when we reach here (CMP found R1==0 above), but we
        ; set it explicitly for clarity and in case the base case is reached via
        ; a future code path.
        MOV  R1, 0          ; return value = 0
        RET                 ; return to caller

; =============================================================================
; STEP-BY-STEP STACK TRACE  (for sum(5), annotated)
; =============================================================================
;
; Notation:  SP = current stack pointer value
;            mem[A] = word stored at address A
;            ► = CALL enters sum  /  ◄ = RET exits sum
;
; ─── WINDING DOWN (pushing frames) ───────────────────────────────────────────
;
;  Event                         SP       mem written
;  ─────────────────────────────────────────────────────────────────────────
;  initial state                 0xFEFF
;  main: CALL sum(5)             0xFEFE   0xFEFF ← 0x0006 (ret to main's SW)
;  sum(5): push n=5              0xFEFE   0xFEFE ← 5
;          ADDI SP,-1            0xFEFD
;          CALL sum(4)           0xFEFC   0xFEFD ← 0x000E (ret to sum5 cont.)
;  sum(4): push n=4              0xFEFC   0xFEFC ← 4
;          ADDI SP,-1            0xFEFB
;          CALL sum(3)           0xFEFA   0xFEFB ← 0x000E
;  sum(3): push n=3              0xFEFA   0xFEFA ← 3
;          ADDI SP,-1            0xFEF9
;          CALL sum(2)           0xFEF8   0xFEF9 ← 0x000E
;  sum(2): push n=2              0xFEF8   0xFEF8 ← 2
;          ADDI SP,-1            0xFEF7
;          CALL sum(1)           0xFEF6   0xFEF7 ← 0x000E
;  sum(1): push n=1              0xFEF6   0xFEF6 ← 1
;          ADDI SP,-1            0xFEF5
;          CALL sum(0)           0xFEF4   0xFEF5 ← 0x000E
;  sum(0): CMP R1(=0), R0 → ZF=1
;          JEQ sum_base  →  MOV R1,0 ; RET         ← PEAK DEPTH: SP=0xFEF4
;
; ─── UNWINDING (popping frames, computing results) ───────────────────────────
;
;  Event                         SP       R1 (running result)
;  ─────────────────────────────────────────────────────────────────────────
;  sum(0): RET                   0xFEF5   0        (sum(0) = 0)
;  sum(1): ADDI SP,+1            0xFEF6
;          LW R2, SP(0xFEF6) → R2=1
;          ADD R1, R1, R2        —        0+1 = 1  (sum(1) = 1)
;          RET                   0xFEF7
;  sum(2): ADDI SP,+1            0xFEF8
;          LW R2, SP(0xFEF8) → R2=2
;          ADD R1, R1, R2        —        1+2 = 3  (sum(2) = 3)
;          RET                   0xFEF9
;  sum(3): ADDI SP,+1            0xFEFA
;          LW R2, SP(0xFEFA) → R2=3
;          ADD R1, R1, R2        —        3+3 = 6  (sum(3) = 6)
;          RET                   0xFEFB
;  sum(4): ADDI SP,+1            0xFEFC
;          LW R2, SP(0xFEFC) → R2=4
;          ADD R1, R1, R2        —        6+4 = 10 (sum(4) = 10)
;          RET                   0xFEFD
;  sum(5): ADDI SP,+1            0xFEFE
;          LW R2, SP(0xFEFE) → R2=5
;          ADD R1, R1, R2        —        10+5 = 15 (sum(5) = 15)
;          RET                   0xFEFF
;  main:   SW R1(=15), R3(0x0200), 0  → mem[0x0200] = 0x000F
;          HALT
;
; VERIFY with emulator:
;   ../educpu16/assembler_bin  asm/sum_recursive.asm  -o sum_recursive.bin
;   ../educpu16/emulator_bin   sum_recursive.bin --dump --addr 0200
;   Expected output:  0200: 000F ...
; =============================================================================
