#ifndef PROCESS_H
#define PROCESS_H

#include <stdint.h>

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

#endif // PROCESS_H