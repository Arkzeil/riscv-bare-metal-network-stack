/* ISA document:                                       */
/* https://msyksphinz-self.github.io/riscv-isadoc/#_ld */
    
    .section .text.entry
    .globl _start

_start:
    /* Set the global pointer (optional but good practice if using small data) */
    /* -mcmodel=tiny, optimize for memory footprint and performance in embedded systems 
    by keeping data sections within a small address space */
    /* la      gp, __global_pointer$ */

    /* Set up stack pointer */

    /* la      t0, _start */
    /* mv      sp, t0 */
    la      sp, __stack_top
    /* Pass the dtb address inside a1 (x11) to _dtb_addr */
    la      t0, _dtb_addr
    sd      a1, 0(t0)
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

// _dtb_addr is in data section
.section .data
    .global _dtb_addr
// make it 64bits
_dtb_addr: 
  .quad 0x0	