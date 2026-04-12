# void swtch(struct context *old, struct context *new);
# a0 = old_context pointer, a1 = new_context pointer

.globl swtch
swtch:
    # save old context
    sd ra, 0(a1)    #save return address (x1)
    sd sp, 8(a1)    #save stack pointer (x2)
    sd s0, 16(a1)   # Func arg / ret val
    sd s1, 24(a1)   # Func arg / ret val
    sd s2, 32(a1)   # Saved registers
    sd s3, 40(a1)
    sd s4, 48(a1)
    sd s5, 56(a1)
    sd s6, 64(a1)
    sd s7, 72(a1)
    sd s8, 80(a1)
    sd s9, 88(a1)
    sd s10, 96(a1)
    sd s11, 104(a1)
    # switch to new context
    ld ra, 0(a1)
    ld sp, 8(a1)
    ld s0, 16(a1)
    ld s1, 24(a1)
    ld s2, 32(a1)
    ld s3, 40(a1)
    ld s4, 48(a1)
    ld s5, 56(a1)
    ld s6, 64(a1)
    ld s7, 72(a1)
    ld s8, 80(a1)
    ld s9, 88(a1)
    ld s10, 96(a1)
    ld s11, 104(a1)

    ret