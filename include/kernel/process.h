#ifndef PROCESS_H
#define PROCESS_H

#include <stdint.h>

#define NPROC 16
#define STACK_SIZE 4096

enum proc_state {
  PROC_UNUSED = 0,
  PROC_RUNNABLE,
  PROC_RUNNING,
  PROC_ZOMBIE
};

struct context {
  uint64_t ra;  // Return Address
  uint64_t sp;  // Stack Pointer
  // Callee-saved registers
  uint64_t s0;
  uint64_t s1;
  uint64_t s2;
  uint64_t s3;
  uint64_t s4;
  uint64_t s5;
  uint64_t s6;
  uint64_t s7;
  uint64_t s8;
  uint64_t s9;
  uint64_t s10;
  uint64_t s11;
};

struct process {
  int pid;
  enum proc_state state;
  void *stack;                  // Pointer to the base of stack allocated
  struct context context;       // Saved context
  void (*entry)(void);          // Entry point function
  char name[16];                // Process name
};

// Extern declarations
extern struct process procs[NPROC];
extern struct process *current_process;

// Function declarations
void swtch(struct context *old, struct context *new);
void proc_init(void);
struct process *proc_create(void (*entry)(void), const char *name);
void scheduler(void);
void yield(void);

#endif // PROCESS_H