#include "kernel/mem.h"
#include "kernel/utils.h"

// Get the symbol _end from the linker script
// these two extern symbols from linker script can be used in 
// following codes
extern void* __heap_start;
extern uint8_t __heap_end[];

// get the heap address
/* 
    > Linker scripts symbol declarations, by contrast, create an entry in the symbol table 
    but do not assign any memory to them. Thus "they are an address without a value". 
    This means that you cannot access the value of a linker script defined symbol - it has no value - 
    all you can do is use the address of a linker script defined symbol.
*/
static uint8_t *allocated = (uint8_t*)&__heap_start;
static uint32_t allocated_pages __attribute__((unused)) = 0;

void *mem_alloc(uint32_t size) {
    size = (size + 7) & ~7; // 8-byte align

    if(allocated + size > __heap_end)
        return NULL; // Out of memory

    uint8_t* mem = allocated;
    allocated += size;

    return mem;
}

void *mem_alloc_page() {
    // Placeholder implementation
    return NULL;
}

void mem_free_page(void* page) {
    // Placeholder implementation
}

void mem_set(void* dest, uint8_t value, uint32_t size) {
    uint8_t* ptr = (uint8_t*)dest;
    for(uint32_t i = 0; i < size; i++) {
        ptr[i] = value;
    }
}

void mem_copy(void* dest, const void* src, uint32_t size) {
    uint8_t* d = (uint8_t*)dest;
    const uint8_t* s = (const uint8_t*)src;
    for(uint32_t i = 0; i < size; i++) {
        d[i] = s[i];
    }
}

int mem_compare(const void* buf1, const void* buf2, uint32_t size) {
    const uint8_t* b1 = (const uint8_t*)buf1;
    const uint8_t* b2 = (const uint8_t*)buf2;
    for(uint32_t i = 0; i < size; i++) {
        if(b1[i] != b2[i]) {
            return (b1[i] < b2[i]) ? -1 : 1;
        }
    }
    return 0;
}