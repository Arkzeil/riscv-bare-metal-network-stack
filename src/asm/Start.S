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
    call    relocate_kernel

    /* Return to S-mode */

    /* Disable interrupt */
    csrw mie, zero

    /* Configure PMP (Grant S-mode full access to all memory) */
    /* Range: 0 to (1 << 54)-1 (covers all possible addresses) */
    li t0, -1
    csrw pmpaddr0, t0
    li t1, 0x8f         /* NAPOT | R | W | X */
    csrw pmpcfg0, t1

    /* Delegate interrupts and exceptions to S-mode */
    /* This allows S-mode to handle its own Page Faults and Timer interrupts */
    li t0, 0xffff
    csrw medeleg, t0
    csrw mideleg, t0

    /* Set mstatus.MPP to 1 (Supervisor Mode) */
    /* Clear both MPP bits first, then set MPP=S. */
    li t0, (3 << 11)
    csrc mstatus, t0
    li t0, (1 << 11)
    csrs mstatus, t0

    /* Set mepc to the VIRTUAL address of kernel_main */
    /* Because of the linker script, 'la' will load the 0xFFFFFFC0... address */
    la t0, kernel_entry
    csrw mepc, t0

    /* Drop to S-mode and jump to mepc */
    mret

kernel_entry:
    la      sp, __stack_top
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