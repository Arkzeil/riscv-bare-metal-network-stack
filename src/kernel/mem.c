#include "kernel/mem.h"
#include "kernel/utils.h"
#include "driver/virt_mmio.h"

// Linker symbols denote addresses, not stored pointer values.
extern uint8_t __heap_start[];
extern uint8_t __stack_limit[];

static struct free_page *free_list;
static uintptr_t heap_start;
static uintptr_t heap_end;
static uint32_t free_page_count;

static inline uint64_t virt_to_phys(uint64_t addr) {
    if (addr >= KERNEL_BASE_VA) {
        return addr - VIRT_OFFSET;
    }
    return addr;
}

static void page_allocator_init(void) {
    // Use identity-mapped addresses in both M-mode boot and S-mode execution.
    heap_start = (virt_to_phys((uintptr_t)__heap_start) + PAGE_SIZE - 1)
                 & ~(PAGE_SIZE - 1);
    heap_end = heap_start + MAX_HEAP_SIZE;
    uintptr_t stack_limit = virt_to_phys((uintptr_t)__stack_limit);
    if (heap_end > stack_limit) {
        heap_end = stack_limit;
    }
    heap_end &= ~(PAGE_SIZE - 1);

    free_list = NULL;
    free_page_count = 0;
    // Build an ascending list so initial multi-page allocations are contiguous.
    for (uintptr_t addr = heap_end; addr > heap_start; ) {
        addr -= PAGE_SIZE;
        struct free_page *page = (struct free_page *)addr;
        page->next = free_list;
        free_list = page;
        free_page_count++;
    }
}

// The kernel currently uses one hart. Mask S-mode interrupts so a timer-driven
// context switch cannot interrupt a free-list update, including during boot.
static uint64_t allocator_intr_off(void) {
    uint64_t status;
    __asm__ volatile("csrrc %0, sstatus, %1"
                     : "=r"(status) : "r"(1UL << 1) : "memory");
    return status;
}

static void allocator_intr_restore(uint64_t status) {
    if (status & (1UL << 1)) {
        __asm__ volatile("csrs sstatus, %0" :: "r"(1UL << 1) : "memory");
    }
}

void memset(void* dest, uint8_t value, uint32_t size) {
    uint8_t* ptr = (uint8_t*)dest;
    for(uint32_t i = 0; i < size; i++) {
        ptr[i] = value;
    }
}

// tmp page table for booting, it will be identity mapped to va 0x80000000 and also mapped to high half va 0xFFFFFFC080000000
pte_t boot_page_table[512] __attribute__((aligned(PAGE_SIZE)));

static void setup_uart_mapping(pagetable_t root) {
    // Map UART0 at 0x10000000 to a high-half virtual address, e.g., 0xFFFFFFC100000000
    // uint64_t uart_va = 0xFFFFFFC100000000;
    uint64_t uart_va = 0x10000000;
    uint64_t uart_pa = 0x10000000;
    map_page(root, uart_va, uart_pa, PTE_R | PTE_W | PTE_V | PTE_X);
    // boot_page_table[257] = PA2PTE(uart_pa) | PTE_V | PTE_R | PTE_W | PTE_X;
}

static void setup_virtio_mappings(pagetable_t root) {
    // Identity-map every QEMU virt VirtIO MMIO slot for the driver's scan.
    for (uint32_t slot = 1; slot <= VIRTIO_SLOTS; slot++) {
        uint64_t addr = VIRT_MMIO(slot * VIRTIO_SIZE);
        map_page(root, addr, addr, PTE_R | PTE_W);
    }
}

static void setup_boot_page_table(void) {
    // Identity Mapping(Virtual Address = Physical Address): VA 0x80000000 -> PA 0x80000000
    // so that we can continue to execute code after enabling paging
    boot_page_table[PX(2, BASE_ADDR)] = PA2PTE(BASE_ADDR) | PTE_V | PTE_R | PTE_W | PTE_X;

    // High-half Mapping: VA 0xFFFFFFC080000000 -> PA 0x80000000
    // 0xFFFFFFC080000000[38:30] = 258
    // This allows the kernel to access its own code and data using high-half addresses after paging is enabled.
    boot_page_table[PX(2, KERNEL_BASE_VA)] = PA2PTE(BASE_ADDR) | PTE_V | PTE_R | PTE_W | PTE_X;
}

void relocate_kernel(void) {
    // Boot MMIO mappings allocate page-table pages before mem_init() runs.
    page_allocator_init();
    // 1. Setup the boot page table with identity and high-half mappings
    setup_boot_page_table();
    setup_uart_mapping(boot_page_table);
    setup_virtio_mappings(boot_page_table);
    // 2. Load the new page table into satp and enable paging
    // Mode = 8 for Sv39, ASID = 0 (not used), PPN = boot_page_table >> 12
    uint64_t satp_value = (8ULL << 60) | (virt_to_phys((uint64_t)boot_page_table) >> PGSHIFT);
    __asm__ volatile(
        "sfence.vma\n"    // Update TLB. Ensure all page table updates are visible before enabling paging
        "csrw satp, %0\n" // Load the new page table into satp
        "sfence.vma\n"    // Update TLB. Ensure the new page table is used immediately
        :: "r"(satp_value) : "memory"
    );

    // After this point, the kernel is running with paging enabled and can access memory using virtual addresses.
}

uint8_t mem_init(void) {
    pagetable_t root_page_table = (pagetable_t)mem_alloc(PAGE_SIZE);
    if(root_page_table == NULL) {
        // Handle memory allocation failure (e.g., log an error, halt the system, etc.)
        return -1;
    }
    memset(root_page_table, 0, PAGE_SIZE);

    // Map the first 1GB both identity and high-half kernel. The high-half mapping must
    // survive the satp switch because the kernel is executing there.
    uint64_t pa = BASE_ADDR;
    root_page_table[PX(2, BASE_ADDR)] = PA2PTE(pa) | PTE_R | PTE_W | PTE_X | PTE_V;
    // 258 is the index for 0xFFFFFFC080000000 in the root page table for Sv39
    // 258 x 0x40000000 -> 0xFFFFFFC080000000 (38:0) = 0x4080000000
    root_page_table[PX(2, KERNEL_BASE_VA)] = PA2PTE(pa) | PTE_R | PTE_W | PTE_X | PTE_V;

    setup_uart_mapping(root_page_table);
    setup_virtio_mappings(root_page_table);

    // Set the satp register to point to the root page table and enable paging
    // Mode = 8 for Sv39, ASID = 0 (not used), PPN = root_page_table >> 12
    uint64_t satp_value = (8ULL << 60) | (virt_to_phys((uint64_t)root_page_table) >> PGSHIFT);
    __asm__ volatile(
        "sfence.vma\n"    // Update TLB. Ensure all page table updates are visible before enabling paging
        "csrw satp, %0\n" // Load the new page table into satp
        "sfence.vma\n"    // Update TLB. Ensure the new page table is used immediately
        :: "r"(satp_value) : "memory"
    );

    //TODO: free the boot page table

    return 0;
}

void map_page(pagetable_t root, uint64_t va, uint64_t pa, uint64_t flags) {
    pagetable_t pt = root;
    for (int level = 2; level > 0; level--) {
        // Calculate the VPN for the current level
        uint64_t vpn = PX(level, va);
        pte_t *pte = &pt[vpn];
        
        if (*pte & PTE_V) {
            // next level page table already exists, get the PPN of it
            pt = (pagetable_t)PTE2PA(*pte);
        }
        else {
            // allocate a new page for the next level page table
            pagetable_t next_pt = (pagetable_t)mem_alloc(PAGE_SIZE);
            memset(next_pt, 0, PAGE_SIZE);
            // point to next level page table, set valid bit
            *pte = PA2PTE(virt_to_phys((uint64_t)next_pt)) | PTE_V;
            pt = next_pt;
        }
    }
    // Set the final PTE to point to the physical address with the given flags
    uint64_t vpn0 = PX(0, va);
    pt[vpn0] = PA2PTE(pa) | flags | PTE_V;
}

void va2pa(pagetable_t root, uint64_t va, uint64_t *pa) {
    pagetable_t pt = root;
    for (int level = 2; level > 0; level--) {
        uint64_t vpn = PX(level, va);
        pte_t pte = pt[vpn];
        
        if (!(pte & PTE_V)) {
            *pa = 0; // Not mapped
            return;
        }
        pt = (pagetable_t)(PTE2PA(pte));
    }
    uint64_t vpn0 = PX(0, va);
    pte_t final_pte = pt[vpn0];
    
    if (!(final_pte & PTE_V)) {
        *pa = 0; // Not mapped
        return;
    }
    *pa = PTE2PA(final_pte) | (va & (PAGE_SIZE - 1)); // Combine with page offset
}

void *mem_alloc(uint32_t size) {
    if (size == 0) {
        return NULL;
    }

    uint32_t count = ((uint64_t)size + PAGE_SIZE - 1) >> PGSHIFT;
    uint64_t status = allocator_intr_off();
    if (count > free_page_count) {
        allocator_intr_restore(status);
        return NULL;
    }

    if (count == 1) {
        struct free_page *page = free_list;
        free_list = page->next;
        free_page_count--;
        allocator_intr_restore(status);
        return page;
    }

    // Free-list order changes on free. Check address ranges, rather than
    // assuming that neighboring list entries are neighboring physical pages.
    // Note: ideally the kernel should use buddy system or slab allocator for multi-page allocations,
    // but this is a simple implementation.
    uintptr_t bytes = (uintptr_t)count * PAGE_SIZE;
    for (struct free_page *first = free_list; first; first = first->next) {
        uintptr_t start = (uintptr_t)first;
        uintptr_t end = start + bytes;
        if (end > heap_end) {
            continue;
        }
        uint32_t found = 0;
        for (struct free_page *page = free_list; page; page = page->next) {
            if ((uintptr_t)page >= start && (uintptr_t)page < end) {
                found++;
            }
        }
        if (found != count) {
            continue;
        }

        // Remove the contiguous pages from the free list and return the starting address.
        struct free_page **link = &free_list;
        while (*link) {
            uintptr_t addr = (uintptr_t)*link;
            if (addr >= start && addr < end) {
                *link = (*link)->next;
            }
            // if the current page is not in the range, move to the next link
            else {
                link = &(*link)->next;
            }
        }
        free_page_count -= count;
        allocator_intr_restore(status);
        return (void *)start;
    }

    allocator_intr_restore(status);
    return NULL;
}

void mem_free(void* page) {
    uintptr_t addr = virt_to_phys((uintptr_t)page);
    if (page == NULL || addr < heap_start || addr >= heap_end
        || (addr & (PAGE_SIZE - 1))) {
        return;
    }

    uint64_t status = allocator_intr_off();
    struct free_page *node = (struct free_page *)addr;
    node->next = free_list;
    free_list = node;
    free_page_count++;
    allocator_intr_restore(status);
}

void *mem_alloc_page(void) {
    return mem_alloc(PAGE_SIZE);
}

void mem_free_page(void* page) {
    mem_free(page);
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
