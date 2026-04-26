#ifndef MEM_H
#define MEM_H

#include <stdint.h>
#include "kernel/utils.h"

#ifndef NULL
#define NULL ((void*)0)
#endif

#define BASE_ADDR       0x80000000

#define MAX_HEAP_SIZE   0x1000000 // 16 MB
#define PAGE_SIZE       4096

typedef uint64_t        pte_t;
typedef pte_t*          pagetable_t;

#define PTE_V           BIT_MASK(0) // Valid
#define PTE_R           BIT_MASK(1) // Read
#define PTE_W           BIT_MASK(2) // Write
#define PTE_X           BIT_MASK(3) // Execute

void memset(void* dest, uint8_t value, uint32_t size);
uint8_t mem_init(void);
void map_page(pagetable_t root, uint64_t va, uint64_t pa, uint64_t flags);
void *mem_alloc(uint32_t size);
void *mem_alloc_page();
void mem_free_page(void* page);
void mem_set(void* dest, uint8_t value, uint32_t size);
void mem_copy(void* dest, const void* src, uint32_t size);
int mem_compare(const void* buf1, const void* buf2, uint32_t size);

#endif // MEM_H