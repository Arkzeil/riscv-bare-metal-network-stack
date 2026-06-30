# Assembly entry point for supervisor-mode traps (interrupts/exceptions).
# The address of `trap_vector` is loaded into the `stvec` register.
#
# When a trap occurs, the hardware:
# 1. Saves PC into `sepc`
# 2. Sets `scause` and `stval`
# 3. Disables interrupts (`sstatus.SIE` is copied to `sstatus.SPIE`, and `SIE` is set to 0)
# 4. Jumps to the address in `stvec` (which is `trap_vector`).

.section .text
.globl trap_vector
.align 4
trap_vector:
    # 1. Allocate space on the stack for the trap context (36 * 8 = 288 bytes)
    addi sp, sp, -288

    # 2. Save general-purpose registers (except x0, which is always 0)
    sd ra,   8(sp)   # x1 = return address
    
    # Save the original stack pointer (before the trap) in regs[2] slot (sp)
    addi t0, sp, 288
    sd t0,   16(sp)  # x2 = sp
    
    sd gp,   24(sp)  # x3 = gp
    sd tp,   32(sp)  # x4 = tp
    
    # Save t0 (x5) which we just used as temporary
    sd t0,   40(sp)
    
    sd t1,   48(sp)  # x6 = t1
    sd t2,   56(sp)  # x7 = t2
    sd s0,   64(sp)  # x8 = s0/fp
    sd s1,   72(sp)  # x9 = s1
    sd a0,   80(sp)  # x10 = a0
    sd a1,   88(sp)  # x11 = a1
    sd a2,   96(sp)  # x12 = a2
    sd a3,   104(sp) # x13 = a3
    sd a4,   112(sp) # x14 = a4
    sd a5,   120(sp) # x15 = a5
    sd a6,   128(sp) # x16 = a6
    sd a7,   136(sp) # x17 = a7
    sd s2,   144(sp) # x18 = s2
    sd s3,   152(sp) # x19 = s3
    sd s4,   160(sp) # x20 = s4
    sd s5,   168(sp) # x21 = s5
    sd s6,   176(sp) # x22 = s6
    sd s7,   184(sp) # x23 = s7
    sd s8,   192(sp) # x24 = s8
    sd s9,   200(sp) # x25 = s9
    sd s10,  208(sp) # x26 = s10
    sd s11,  216(sp) # x27 = s11
    sd t3,   224(sp) # x28 = t3
    sd t4,   232(sp) # x29 = t4
    sd t5,   240(sp) # x30 = t5
    sd t6,   248(sp) # x31 = t6

    # 3. Save supervisor CSRs
    csrr t0, sstatus
    sd t0,   256(sp)
    csrr t0, sepc
    sd t0,   264(sp)
    csrr t0, scause
    sd t0,   272(sp)
    csrr t0, stval
    sd t0,   280(sp)

    # 4. Call the C trap handler, passing the pointer to the saved context in a0
    mv a0, sp
    call trap_handler

    # 5. Restore supervisor CSRs (especially sstatus and sepc)
    ld t0,   256(sp)
    csrw sstatus, t0
    ld t0,   264(sp)
    csrw sepc, t0

    # 6. Restore general-purpose registers
    ld ra,   8(sp)
    ld gp,   24(sp)
    ld tp,   32(sp)
    ld t0,   40(sp)
    ld t1,   48(sp)
    ld t2,   56(sp)
    ld s0,   64(sp)
    ld s1,   72(sp)
    ld a0,   80(sp)
    ld a1,   88(sp)
    ld a2,   96(sp)
    ld a3,   104(sp)
    ld a4,   112(sp)
    ld a5,   120(sp)
    ld a6,   128(sp)
    ld a7,   136(sp)
    ld s2,   144(sp)
    ld s3,   152(sp)
    ld s4,   160(sp)
    ld s5,   168(sp)
    ld s6,   176(sp)
    ld s7,   184(sp)
    ld s8,   192(sp)
    ld s9,   200(sp)
    ld s10,  208(sp)
    ld s11,  216(sp)
    ld t3,   224(sp)
    ld t4,   232(sp)
    ld t5,   240(sp)
    ld t6,   248(sp)

    # 7. Restore the original stack pointer (from context)
    ld sp,   16(sp)

    # 8. Return from supervisor trap
    sret
