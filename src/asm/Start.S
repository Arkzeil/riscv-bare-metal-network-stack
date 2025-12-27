    .section .text.entry
    .globl _start

_start:
    /* Set the global pointer (optional but good practice if you use small data) */
    la      gp, __global_pointer$

    /* Set up stack pointer */
    la      sp, _stack_top

    /* Zero out .bss section: for (p = _sbss; p < _ebss; p++) *p = 0; */
    la      t0, _sbss
    la      t1, _ebss
1:
    bgeu    t0, t1, 2f       /* if t0 >= t1, done */
    sd      zero, 0(t0)      /* store 0 (64-bit) at *t0 */
    addi    t0, t0, 8        /* advance by 8 bytes (for RV64) */
    j       1b
2:
    /* Call main() */
    call    main

    /* If main returns, just hang */
3:
    j       3b
