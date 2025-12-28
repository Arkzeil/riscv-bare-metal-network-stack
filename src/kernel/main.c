#include <stdint.h>
#include "kernel/uart.h"

void main(void) {
    volatile uint64_t counter = 0;
    
    uart_init(115200, 100000000); // Initialize UART with 115200 baud rate and 100MHz clock
    uart_puts("Hello, World!\n");
    
    while (1) {
        asm volatile ("nop");
        counter++;
    }
}
