#ifndef UART_H
#define UART_H

#include <stdint.h>

// the base address of UART0 (virt machine)
// p.s. virt machine is unlike real hardware platforms, 
// it provides a simple UART device at this fixed address and does not require to configure GPIOs
#define UART0_BASE 0x10000000UL

#define UART_REG(offset) (*(volatile unsigned char *)(UART0_BASE + (offset)))
#define UART_TXDATA UART_REG(0x00)
#define UART_RXDATA UART_REG(0x04)
#define UART_TXCTRL UART_REG(0x08)      // Transmit Control Register
#define UART_RXCTRL UART_REG(0x0C)      // Receive Control Register   
#define UART_IE     UART_REG(0x10)      // Interrupt Enable Register
#define UART_IP     UART_REG(0x14)      // Interrupt Pending Register
#define UART_DIV    UART_REG(0x18)      // Baud Rate Divisor Register

void uart_init(uint32_t baudrate, uint64_t clock_freq);
void uart_putc(char c);
char uart_getc(void);
void uart_puts(const char *str);
int uart_gets(char *buf, int maxlen);

#endif