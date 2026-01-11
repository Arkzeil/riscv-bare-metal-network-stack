#ifndef MEM_H
#define MEM_H

#include <stdint.h>

#ifndef NULL
#define NULL ((void*)0)
#endif

#define MAX_HEAP_SIZE 0x1000000 // 16 MB
#define PAGE_SIZE 4096

void *mem_alloc(uint32_t size);
void *mem_alloc_page();
void mem_free_page(void* page);
void mem_set(void* dest, uint8_t value, uint32_t size);
void mem_copy(void* dest, const void* src, uint32_t size);
int mem_compare(const void* buf1, const void* buf2, uint32_t size);

#endif // MEM_H