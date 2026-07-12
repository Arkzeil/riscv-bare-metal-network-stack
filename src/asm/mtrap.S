# Assembly entry point for machine-mode traps.
# Handles S-mode ecalls and machine timer interrupts.

.section .text
.globl mtrap_vector
.align 4
mtrap_vector:
    # Swap t0 and mscratch.
    # Now t0 points to the physical top of our M-mode stack (m_stack_top).
    # mscratch contains the original t0.
    csrrw t0, mscratch, t0

    # Save registers on the M-mode stack (t0 points to the top, so we grow downwards)
    addi t0, t0, -40
    sd t1, 0(t0)
    sd t2, 8(t0)
    sd a0, 16(t0)
    sd a7, 24(t0)

    # Now read the original t0 from mscratch and save it
    csrr t1, mscratch
    sd t1, 32(t0)

    # Read mcause into t1
    csrr t1, mcause

    # Check if it's an interrupt (bit 63 is set)
    blt t1, zero, handle_m_interrupt

    # It's an exception. Check if it is an ecall from S-mode (mcause = 9)
    li t2, 9
    bne t1, t2, m_panic

    # It's ecall from S-mode. Check if a7 is 0 (sbi_set_timer)
    li t2, 0
    bne a7, t2, m_panic

    # Set mtimecmp = a0
    # clint base is 0x2000000, mtimecmp for hart 0 is at 0x2004000
    li t1, 0x2004000
    sd a0, 0(t1)

    # Clear pending supervisor timer interrupt (clear STIP in mip)
    li t1, 0x20 # MIP_STIP (1 << 5)
    csrc mip, t1

    # Enable machine timer interrupts (set MTIE in mie)
    li t1, 0x80 # MIE_MTIE (1 << 7)
    csrs mie, t1

    # Advance mepc by 4 so we return to the instruction after ecall
    csrr t1, mepc
    addi t1, t1, 4
    csrw mepc, t1

    # Restore registers and return
    ld t2, 8(t0)
    ld a0, 16(t0)
    ld a7, 24(t0)

    # Restore mscratch to m_stack_top (which is t0 + 40)
    addi t1, t0, 40
    csrw mscratch, t1

    # Restore t1 and t0
    ld t1, 0(t0)
    ld t0, 32(t0)
    mret

handle_m_interrupt:
    # Get exception code (clear bit 63)
    slli t1, t1, 1
    srli t1, t1, 1
    li t2, 7       # Machine Timer Interrupt is 7
    bne t1, t2, m_panic

    # It's a Machine Timer Interrupt
    # Disable machine timer interrupts (clear MTIE in mie)
    li t1, 0x80 # MIE_MTIE (1 << 7)
    csrc mie, t1

    # Set supervisor timer interrupt pending (set STIP in mip)
    li t1, 0x20 # MIP_STIP (1 << 5)
    csrs mip, t1

    # Restore registers and return
    ld t2, 8(t0)
    ld a0, 16(t0)
    ld a7, 24(t0)

    # Restore mscratch to m_stack_top (which is t0 + 40)
    addi t1, t0, 40
    csrw mscratch, t1

    # Restore t1 and t0
    ld t1, 0(t0)
    ld t0, 32(t0)
    mret

m_panic:
    # Print "MPanic\n" directly to UART at 0x10000000
    li t1, 0x10000000
    li t2, 77  # 'M'
    sb t2, 0(t1)
    li t2, 80  # 'P'
    sb t2, 0(t1)
    li t2, 97  # 'a'
    sb t2, 0(t1)
    li t2, 110 # 'n'
    sb t2, 0(t1)
    li t2, 105 # 'i'
    sb t2, 0(t1)
    li t2, 99  # 'c'
    sb t2, 0(t1)
    li t2, 10  # '\n'
    sb t2, 0(t1)

m_panic_loop:
    j m_panic_loop

.section .data
.align 8
.globl m_stack
.globl m_stack_top
m_stack:
    .space 256
m_stack_top:
