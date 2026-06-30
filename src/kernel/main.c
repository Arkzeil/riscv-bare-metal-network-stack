#include <stdint.h>
#include "kernel/mem.h"
#include "kernel/uart.h"
#include "kernel/dtb.h"
#include "kernel/trap.h"
#include "driver/virt_mmio.h"
/*
void taskA(void){
    while(1){
        uart_puts("Task A is running.\n");
        for (int i = 0; i < 1000000; i++); // Simple delay loop
    }
}

void taskB(void){
    while(1){
        uart_puts("Task B is running.\n");
        for (int i = 0; i < 1000000; i++); // Simple delay loop
    }
}
*/

// void check_m_mode(void) {
//     uint64_t mstatus;
//     uint64_t sstatus;
//     __asm__ volatile("csrr %0, mstatus" : "=r"(mstatus));
//     __asm__ volatile("csrr %0, sstatus" : "=r"(sstatus));
// }

void main(void) {
    // check_m_mode();
    if (mem_init() != 0) {
        // uart_puts("Memory initialization failed!\n");
        return;
    }
    // volatile uint64_t counter = 0;
    char buf[10];

    uart_init(115200, 100000000); // Initialize UART with 115200 baud rate and 100MHz clock
    uart_puts("Hello, World!\n");
    
    trap_init();
    uart_puts("Trap handler initialized.\n");
    
    // TEST: Trigger an exception (Load Page Fault) to verify the trap handler works!
    uart_puts("Triggering a page fault for testing...\n");
    volatile uint64_t *bad_ptr = (volatile uint64_t *)0;
    uint64_t val __attribute__((unused)) = *bad_ptr;

    uart_gets(buf, sizeof(buf));
    uart_puts("You entered: ");
    uart_puts(buf);
    uart_putc('\n');

    fdt_traverse(dtb_callback);

    virt_find_all_devices();
    

    uart_puts("End of main.\n");
}
