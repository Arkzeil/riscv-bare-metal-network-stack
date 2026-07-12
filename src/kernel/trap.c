#include "kernel/trap.h"
#include "kernel/uart.h"
#include "kernel/timer.h"
#include "kernel/process.h"

static const char * const exception_names[] = {
    [EXC_INST_MISALIGNED]   = "Instruction Address Misaligned",
    [EXC_INST_FAULT]        = "Instruction Access Fault",
    [EXC_ILLEGAL_INST]      = "Illegal Instruction",
    [EXC_BREAKPOINT]        = "Breakpoint",
    [EXC_LOAD_MISALIGNED]   = "Load Address Misaligned",
    [EXC_LOAD_FAULT]        = "Load Access Fault",
    [EXC_STORE_MISALIGNED]  = "Store/AMO Address Misaligned",
    [EXC_STORE_FAULT]       = "Store/AMO Access Fault",
    [EXC_ENV_CALL_U]        = "Environment Call from U-mode",
    [EXC_ENV_CALL_S]        = "Environment Call from S-mode",
    [EXC_INST_PAGE_FAULT]   = "Instruction Page Fault",
    [EXC_LOAD_PAGE_FAULT]   = "Load Page Fault",
    [EXC_STORE_PAGE_FAULT]  = "Store/AMO Page Fault"
};

static const char * const interrupt_names[] = {
    [IRQ_S_SOFT]   = "Supervisor Software Interrupt",
    [IRQ_S_TIMER]  = "Supervisor Timer Interrupt",
    [IRQ_S_EXT]    = "Supervisor External Interrupt"
};

static void dump_registers(struct trap_context *ctx) {
    uart_puts("--- Register Dump ---\n");
    uart_puts("sepc:   "); print_hex64(ctx->sepc);   uart_puts("  sstatus: "); print_hex64(ctx->sstatus); uart_puts("\n");
    uart_puts("scause: "); print_hex64(ctx->scause); uart_puts("  stval:   "); print_hex64(ctx->stval);   uart_puts("\n");
    
    static const char * const reg_names[32] = {
        "zero", "ra", "sp", "gp", "tp", "t0", "t1", "t2",
        "s0", "s1", "a0", "a1", "a2", "a3", "a4", "a5",
        "a6", "a7", "s2", "s3", "s4", "s5", "s6", "s7",
        "s8", "s9", "s10", "s11", "t3", "t4", "t5", "t6"
    };

    for (int i = 0; i < 32; i++) {
        uart_puts(reg_names[i]);
        // pad register name to 5 chars
        int len = 0;
        for (const char* p = reg_names[i]; *p; p++) len++;
        for (int j = len; j < 5; j++) uart_putc(' ');
        uart_puts(": ");
        print_hex64(ctx->regs[i]);
        if (i % 2 == 1) {
            uart_puts("\n");
        } else {
            uart_puts("  ");
        }
    }
    uart_puts("\n---------------------\n");
}

extern void trap_vector(void);

void trap_init(void) {
    // Write the address of our assembly vector (trap_vector) to stvec.
    // The mode is Direct (lowest 2 bits are 00).
    w_stvec((uint64_t)trap_vector);
}

void trap_handler(struct trap_context *ctx) {
    uint64_t scause = ctx->scause;
    
    if (scause & SCAUSE_INTR_FLAG) {
        // Interrupt
        uint64_t irq_code = scause & ~SCAUSE_INTR_FLAG;
        uart_puts("Interrupt received: scause = ");
        print_hex64(scause);
        uart_puts(", irq_code = ");
        print_hex64(irq_code);
        uart_puts("\n");
        switch (irq_code) {
            case IRQ_S_SOFT:
                handle_software_interrupt(ctx);
                break;
            case IRQ_S_TIMER:
                handle_timer_interrupt(ctx);
                break;
            case IRQ_S_EXT:
                handle_external_interrupt(ctx);
                break;
            default:
                uart_puts("Unhandled S-mode interrupt: ");
                print_hex64(irq_code);
                uart_puts("\n");
                while(1);
        }
    } else {
        // Exception
        handle_exception(ctx);
    }
}

/* Weak implementations that the user can override elsewhere */

void __attribute__((weak)) handle_software_interrupt(struct trap_context *ctx) {
    uart_puts("Software interrupt received\n");
    // Clear the pending software interrupt (SSIP bit in sip)
    __asm__ volatile("csrc sip, %0" :: "r"(1ULL << 1));
}

void __attribute__((weak)) handle_timer_interrupt(struct trap_context *ctx) {
    uart_puts("Current time: ");
    uart_dec64(get_time());
    uart_puts("\n");
    // Set timer for 1,000,000 cycles (which clears the pending supervisor timer interrupt)
    set_timer(10000000);
    // Yield the CPU to the next process
    yield();
}

void __attribute__((weak)) handle_external_interrupt(struct trap_context *ctx) {
    uart_puts("External interrupt received\n");
    // PLIC handling needs to claim/complete the interrupt.
    // The user will implement the driver-level details.
}

void __attribute__((weak)) handle_load_page_fault(struct trap_context *ctx) {
    
}

void __attribute__((weak)) handle_exception(struct trap_context *ctx) {
    uart_puts("Kernel exception occurred: scause = ");
    print_hex64(ctx->scause);
    uart_puts(", sepc = ");
    print_hex64(ctx->sepc);
    uart_puts(", stval = ");
    print_hex64(ctx->stval);
    uart_puts("\n");
    
    dump_registers(ctx);

    uart_puts("Exception: ");
    uart_puts(exception_names[ctx->scause]);
    uart_puts("\n");

    switch(ctx->scause) {
        case EXC_INST_MISALIGNED:
        case EXC_INST_FAULT:
        case EXC_ILLEGAL_INST:
        case EXC_BREAKPOINT:
        case EXC_LOAD_MISALIGNED:
        case EXC_LOAD_FAULT:
        case EXC_STORE_MISALIGNED:
        case EXC_STORE_FAULT:
        case EXC_ENV_CALL_U:
        case EXC_ENV_CALL_S:
        case EXC_INST_PAGE_FAULT:
        case EXC_LOAD_PAGE_FAULT:
        case EXC_STORE_PAGE_FAULT:
            break;
        default:
            uart_puts("Unknown exception code: ");
            print_hex64(ctx->scause);
            uart_puts("\n");
    }
    
    uart_puts("Kernel panic: S-mode exception unhandled. Halted.\n");
    while (1);
}
