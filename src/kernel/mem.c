#include "kernel/mem.h"
#include "kernel/utils.h"

// Get the symbol _end from the linker script
// these two extern symbols from linker script can be used in 
// following codes
extern void* __heap_start;
extern uint8_t __stack_limit[];

// get the heap address
/* 
    > Linker scripts symbol declarations, by contrast, create an entry in the symbol table 
    but do not assign any memory to them. Thus "they are an address without a value". 
    This means that you cannot access the value of a linker script defined symbol - it has no value - 
    all you can do is use the address of a linker script defined symbol.
*/
static uint8_t *allocated = (uint8_t*)&__heap_start;
static uint32_t allocated_pages __attribute__((unused)) = 0;

void memset(void* dest, uint8_t value, uint32_t size) {
    uint8_t* ptr = (uint8_t*)dest;
    for(uint32_t i = 0; i < size; i++) {
        ptr[i] = value;
    }
}

// tmp page table for booting, it will be identity mapped to va 0x80000000 and also mapped to high half va 0xFFFFFFC080000000
pte_t boot_page_table[512] __attribute__((aligned(4096)));

static void setup_boot_page_table(void) {
    // Identity Mapping: VA 0x80000000 -> PA 0x80000000
    boot_page_table[2] = (BASE_ADDR >> 12 << 10) | PTE_V | PTE_R | PTE_W | PTE_X;

    // High-half Mapping: VA 0xFFFFFFC080000000 -> PA 0x80000000
    // 0xFFFFFFC080000000[38:30] = 258
    boot_page_table[258] = (BASE_ADDR >> 12 << 10) | PTE_V | PTE_R | PTE_W | PTE_X;
}

void relocate_kernel(void) {
    // 1. Setup the boot page table with identity and high-half mappings
    setup_boot_page_table();

    // 2. Load the new page table into satp and enable paging
    uint64_t satp_value = (8ULL << 60) | ((uint64_t)boot_page_table >> 12);
    __asm__ volatile(
        "sfence.vma\n"    // Update TLB. Ensure all page table updates are visible before enabling paging
        "csrw satp, %0\n" // Load the new page table into satp
        "sfence.vma\n"    // Update TLB. Ensure the new page table is used immediately
        :: "r"(satp_value) : "memory"
    );

    // After this point, the kernel is running with paging enabled and can access memory using virtual addresses.
}

uint8_t mem_init(void) {
    pagetable_t root_page_table = (pagetable_t)mem_alloc(4096); 
    memset(root_page_table, 0, 4096);

    if(root_page_table == NULL) {
        // Handle memory allocation failure (e.g., log an error, halt the system, etc.)
        return -1;
    }

    // Identity map the first 1GB of physical memory(for kernel) with RWX permissions
    uint64_t pa = 0x80000000;
    root_page_table[2] = ((pa >> 12) << 10) | PTE_R | PTE_W | PTE_X | PTE_V;

    // Set the satp register to point to the root page table and enable paging
    // Mode = 8 for Sv39, ASID = 0 (not used), PPN = root_page_table >> 12
    uint64_t satp_value = (8ULL << 60) | ((uint64_t)root_page_table >> 12);
    __asm__ volatile(
        "sfence.vma\n"    // Update TLB. Ensure all page table updates are visible before enabling paging
        "csrw satp, %0\n" // Load the new page table into satp
        "sfence.vma\n"    // Update TLB. Ensure the new page table is used immediately
        :: "r"(satp_value) : "memory"
    );

    return 0;
}

void map_page(pagetable_t root, uint64_t va, uint64_t pa, uint64_t flags) {
    pagetable_t pt = root;
    for (int level = 2; level > 0; level--) {
        // Calculate the VPN for the current level
        uint64_t vpn = (va >> (12 + level * 9)) & 0x1FF;
        pte_t *pte = &pt[vpn];
        
        if (*pte & PTE_V) {
            // next level page table already exists, get the PPN of it
            pt = (pagetable_t)((*pte >> 10) << 12);
        } else {
            // allocate a new page for the next level page table
            pagetable_t next_pt = (pagetable_t)mem_alloc(4096);
            memset(next_pt, 0, 4096);
            // point to next level page table, set valid bit
            *pte = (((uint64_t)next_pt >> 12) << 10) | PTE_V;
            pt = next_pt;
        }
    }
    // Set the final PTE to point to the physical address with the given flags
    uint64_t vpn0 = (va >> 12) & 0x1FF;
    pt[vpn0] = ((pa >> 12) << 10) | flags | PTE_V;
}

void va2pa(pagetable_t root, uint64_t va, uint64_t *pa) {
    pagetable_t pt = root;
    for (int level = 2; level > 0; level--) {
        uint64_t vpn = (va >> (12 + level * 9)) & 0x1FF;
        pte_t pte = pt[vpn];
        
        if (!(pte & PTE_V)) {
            *pa = 0; // Not mapped
            return;
        }
        pt = (pagetable_t)((pte >> 10) << 12);
    }
    uint64_t vpn0 = (va >> 12) & 0x1FF;
    pte_t final_pte = pt[vpn0];
    
    if (!(final_pte & PTE_V)) {
        *pa = 0; // Not mapped
        return;
    }
    *pa = ((final_pte >> 10) << 12) | (va & 0xFFF); // Combine with page offset
}

void *mem_alloc(uint32_t size) {
    // Default to 8-byte alignment
    uint32_t alignment = 8;

    // A basic heuristic to request page alignment for larger allocations,
    // which are likely for DMA buffers or page-sized structures.
    if (size >= 4096) {
        alignment = 4096;
    }

    // Ensure alignment is a power of two
    if ((alignment & (alignment - 1)) != 0) {
        // Alignment is not a power of two, cannot proceed
        return NULL;
    }

    uintptr_t current_ptr = (uintptr_t)allocated;
    // Calculate the next aligned address
    uintptr_t aligned_ptr = (current_ptr + alignment - 1) & ~(alignment - 1);
    
    uintptr_t new_allocated_ptr = aligned_ptr + size;

    if ((uint8_t*)new_allocated_ptr > __stack_limit) {
        return NULL; // Out of memory
    }

    allocated = (uint8_t*)new_allocated_ptr;

    return (void*)aligned_ptr;
}

void *_sbrk(int increment) {
    char *prev_heap_ptr = allocated;
    char *next_heap_ptr = allocated + increment;

    // 1. Get the current Stack Pointer value
    register char *stack_ptr __asm__("sp");

    // 2. CHECK: Is the new heap address crossing into the stack?
    // We check against the current sp to prevent hitting the LIVE stack,
    // and against _stack_limit to ensure we don't steal reserved stack space.
    if (next_heap_ptr > stack_ptr || next_heap_ptr > __stack_limit) {
        // Out of memory! 
        return (void *)-1; 
    }

    allocated = next_heap_ptr;
    return (void *)prev_heap_ptr;
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