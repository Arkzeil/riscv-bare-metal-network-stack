#include <stdint.h>
#include "kernel/uart.h"
#include "kernel/dtb.h"

void main(void) {
    // volatile uint64_t counter = 0;
    char buf[10];

    uart_init(115200, 100000000); // Initialize UART with 115200 baud rate and 100MHz clock
    uart_puts("Hello, World!\n");
    uart_gets(buf, sizeof(buf));
    uart_puts("You entered: ");
    uart_puts(buf);
    uart_putc('\n');

    fdt_traverse(dtb_callback);

    uart_puts("End of main.\n");
}
