#ifndef MEM_H
#define MEM_H

#include <stdint.h>
#include "kernel/utils.h"

#ifndef NULL
#define NULL ((void*)0)
#endif

#define BASE_ADDR       0x80000000
#define VIRT_OFFSET     0xFFFFFFC000000000UL
#define KERNEL_BASE_VA  (BASE_ADDR + VIRT_OFFSET)

#define MAX_HEAP_SIZE   0x1000000 // 16 MB
#define PGSHIFT         12
#define PAGE_SIZE       (1UL << PGSHIFT)

// from xv6-------------
// shift a physical address to the right place for a PTE.
#define PA2PTE(pa)      ((((uint64_t)(pa)) >> PGSHIFT) << 10)
#define PTE2PA(pte)     ((((uint64_t)(pte)) >> 10) << PGSHIFT)
// extract the three 9-bit page table indices from a virtual address.
#define PXMASK         0x1FFUL // 9 bits
#define PXSHIFT(level) (PGSHIFT + (9 * (level)))
#define PX(level, va)  ((((uint64_t)(va)) >> PXSHIFT(level)) & PXMASK)
//-----------------------

typedef uint64_t        pte_t;
typedef pte_t*          pagetable_t;

#define PTE_V           BIT_MASK(0) // Valid
#define PTE_R           BIT_MASK(1) // Read
#define PTE_W           BIT_MASK(2) // Write
#define PTE_X           BIT_MASK(3) // Execute

// A free page stores its link in its first word; allocated pages own all 4 KiB.
struct free_page {
    struct free_page *next;
};

void memset(void* dest, uint8_t value, uint32_t size);
uint8_t mem_init(void);
void map_page(pagetable_t root, uint64_t va, uint64_t pa, uint64_t flags);
// Allocate contiguous, page-aligned memory, rounding size up to whole pages.
// Returns NULL for size zero or when no suitable free pages remain.
void *mem_alloc(uint32_t size);
// Return one allocated page to the head of the free list. For a multi-page
// allocation, free each page separately. NULL is ignored; do not double-free.
void mem_free(void* page);
void *mem_alloc_page(void);
void mem_free_page(void* page);
void mem_set(void* dest, uint8_t value, uint32_t size);
void mem_copy(void* dest, const void* src, uint32_t size);
int mem_compare(const void* buf1, const void* buf2, uint32_t size);

#endif // MEM_H
