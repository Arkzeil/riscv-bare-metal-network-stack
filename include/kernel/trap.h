#ifndef TRAP_H
#define TRAP_H

#include <stdint.h>

/* RISC-V S-mode Cause Codes */
#define SCAUSE_INTR_FLAG      (1ULL << 63)

/* Interrupts */
#define IRQ_S_SOFT            1
#define IRQ_S_TIMER           5
#define IRQ_S_EXT             9

/* Exceptions */
#define EXC_INST_MISALIGNED   0
#define EXC_INST_FAULT        1
#define EXC_ILLEGAL_INST      2
#define EXC_BREAKPOINT        3
#define EXC_LOAD_MISALIGNED   4
#define EXC_LOAD_FAULT        5
#define EXC_STORE_MISALIGNED  6
#define EXC_STORE_FAULT       7
#define EXC_ENV_CALL_U        8
#define EXC_ENV_CALL_S        9
#define EXC_INST_PAGE_FAULT   12
#define EXC_LOAD_PAGE_FAULT   13
#define EXC_STORE_PAGE_FAULT  15

typedef void (*trap_handler_t)(struct trap_context *ctx);

/* Trap context structure saved on the stack */
struct trap_context {
    uint64_t regs[32];    /* General-purpose registers: regs[1]=ra, regs[2]=sp, etc. */
    uint64_t sstatus;     /* Supervisor Status register */
    uint64_t sepc;        /* Supervisor Exception Program Counter */
    uint64_t scause;      /* Supervisor Cause register */
    uint64_t stval;       /* Supervisor Trap Value register */
};

/* Initialize trap handling (sets stvec to the assembly vector) */
void trap_init(void);

/* The C-mode trap handler, called from assembly vector */
void trap_handler(struct trap_context *ctx);

/* Forward declarations of weak/custom interrupt and exception handlers */
void handle_software_interrupt(struct trap_context *ctx);
void handle_timer_interrupt(struct trap_context *ctx);
void handle_external_interrupt(struct trap_context *ctx);
void handle_exception(struct trap_context *ctx);

/* Helper inline functions to enable/disable interrupts */
static inline void intr_on(void) {
    /* Set SIE (Supervisor Interrupt Enable) bit in sstatus */
    __asm__ volatile("csrs sstatus, %0" :: "r"(1ULL << 1));
}

static inline void intr_off(void) {
    /* Clear SIE bit in sstatus */
    __asm__ volatile("csrc sstatus, %0" :: "r"(1ULL << 1));
}

static inline uint64_t r_sstatus(void) {
    uint64_t x;
    __asm__ volatile("csrr %0, sstatus" : "=r"(x));
    return x;
}

static inline void w_sstatus(uint64_t x) {
    __asm__ volatile("csrw sstatus, %0" :: "r"(x));
}

static inline uint64_t r_stvec(void) {
    uint64_t x;
    __asm__ volatile("csrr %0, stvec" : "=r"(x));
    return x;
}

static inline void w_stvec(uint64_t x) {
    __asm__ volatile("csrw stvec, %0" :: "r"(x));
}

static inline uint64_t r_sepc(void) {
    uint64_t x;
    __asm__ volatile("csrr %0, sepc" : "=r"(x));
    return x;
}

static inline void w_sepc(uint64_t x) {
    __asm__ volatile("csrw sepc, %0" :: "r"(x));
}

static inline uint64_t r_scause(void) {
    uint64_t x;
    __asm__ volatile("csrr %0, scause" : "=r"(x));
    return x;
}

static inline uint64_t r_stval(void) {
    uint64_t x;
    __asm__ volatile("csrr %0, stval" : "=r"(x));
    return x;
}

static inline void sie_enable_software(void) {
    __asm__ volatile("csrs sie, %0" :: "r"(1ULL << 1));
}

static inline void sie_enable_timer(void) {
    __asm__ volatile("csrs sie, %0" :: "r"(1ULL << 5));
}

static inline void sie_enable_external(void) {
    __asm__ volatile("csrs sie, %0" :: "r"(1ULL << 9));
}

#endif /* TRAP_H */
