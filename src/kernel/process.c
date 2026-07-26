#include "kernel/process.h"
#include "kernel/mem.h"
#include "kernel/uart.h"

struct process procs[NPROC];
struct process *current_process = NULL;
static struct context sched_context;

// Entry wrapper for all processes.
static void forkret(void) {
    // A process starts its execution here on its own stack.
    if (current_process && current_process->entry) {
        current_process->entry();
    }
    
    // When the process returns, mark it as zombie and yield
    if (current_process) {
        uart_puts("Process '");
        uart_puts(current_process->name);
        uart_puts("' finished execution. Exiting...\n");
        current_process->state = PROC_ZOMBIE;
    }
    yield();
    
    // We should never reach here since yield switches to the scheduler
    while (1) {}
}

void proc_init(void) {
    for (int i = 0; i < NPROC; i++) {
        procs[i].pid = 0;
        procs[i].state = PROC_UNUSED;
        procs[i].stack = NULL;
        procs[i].entry = NULL;
        procs[i].name[0] = '\0';
        mem_set(&procs[i].context, 0, sizeof(struct context));
    }
    current_process = NULL;
    mem_set(&sched_context, 0, sizeof(struct context));
}

struct process *proc_create(void (*entry)(void), const char *name) {
    struct process *p = NULL;
    
    // Find an unused process slot
    for (int i = 0; i < NPROC; i++) {
        if (procs[i].state == PROC_UNUSED) {
            p = &procs[i];
            break;
        }
    }
    
    if (p == NULL) {
        uart_puts("proc_create: no free process slots.\n");
        return NULL;
    }
    
    // Allocate stack memory
    void *stack = mem_alloc(STACK_SIZE);
    if (stack == NULL) {
        uart_puts("proc_create: failed to allocate stack.\n");
        return NULL;
    }
    
    // Setup process info
    p->pid = (int)(p - procs) + 1;
    p->state = PROC_RUNNABLE;
    p->stack = stack;
    p->entry = entry;
    
    // Copy name securely
    int len = 0;
    if (name) {
        while (name[len] != '\0' && len < 15) {
            p->name[len] = name[len];
            len++;
        }
    }
    p->name[len] = '\0';
    
    // Initialize context
    mem_set(&p->context, 0, sizeof(p->context));
    
    // RISC-V stack grows downward, so stack pointer points to top of stack
    p->context.sp = (uint64_t)stack + STACK_SIZE;
    
    // Set return address to forkret wrapper
    p->context.ra = (uint64_t)forkret;
    
    return p;
}

void scheduler(void) {
    while (1) {
        int found = 0;
        for (int i = 0; i < NPROC; i++) {
            struct process *p = &procs[i];
            if (p->state == PROC_RUNNABLE) {
                p->state = PROC_RUNNING;
                current_process = p;
                
                // Context switch to the selected process
                swtch(&sched_context, &p->context);
                
                // We returned back to the scheduler
                current_process = NULL;
                found = 1;
            }
        }
        
        if (!found) {
            // No runnable processes. Check if any processes are active.
            int active = 0;
            for (int i = 0; i < NPROC; i++) {
                if (procs[i].state != PROC_UNUSED && procs[i].state != PROC_ZOMBIE) {
                    active = 1;
                    break;
                }
            }
            if (!active) {
                // If there are no active processes left, exit the scheduler loop.
                uart_puts("scheduler: no active processes remaining. Exiting scheduler.\n");
                break;
            }
            
            // Otherwise, active processes exist but are waiting (e.g. for timer/interrupt)
            __asm__ volatile("wfi");
        }
    }
}

void yield(void) {
    struct process *p = current_process;
    if (p) {
        if (p->state == PROC_RUNNING) {
            p->state = PROC_RUNNABLE;
        }
        current_process = NULL;
        
        // Switch context from the current process back to the scheduler
        swtch(&p->context, &sched_context);
    }
}
