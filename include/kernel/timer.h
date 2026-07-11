#ifndef TIMER_H
#define TIMER_H

#include <stdint.h>

static inline void set_timer(uint64_t interval) {
    uint64_t current_time;
    __asm__ volatile("csrr %0, time" : "=r"(current_time));
    uint64_t next_time = current_time + interval;
    __asm__ volatile("csrw stimecmp, %0" :: "r"(next_time));
}

static inline uint64_t get_time(void) {
    uint64_t time;
    __asm__ volatile("csrr %0, time" : "=r"(time));
    return time;
}

static inline void enable_timer_interrupt(void) {
    __asm__ volatile("csrs sie, %0" :: "r"(1ULL << 5)); // Set STIE bit in SIE
}

static inline void disable_timer_interrupt(void) {
    __asm__ volatile("csrc sie, %0" :: "r"(1ULL << 5)); // Clear STIE bit in SIE
}

#endif