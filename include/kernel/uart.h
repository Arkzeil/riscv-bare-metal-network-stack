#ifndef UART_H
#define UART_H

#include <stdint.h>
#include <stddef.h>

// the base address of UART0 (virt machine)
// p.s. virt machine is unlike real hardware platforms, 
// it provides a simple UART device at this fixed address and does not require to configure GPIOs
// by inspecting the device tree blob (dtb) of the virt machine, we can confirm this address
#define UART0_BASE  0x10000000UL

#define UART_REG(offset) (*(volatile unsigned char *)(UART0_BASE + (offset)))

// NS16550A UART Registers
#define UART_THR    UART_REG(0x00)      // Transmit Holding Register
#define UART_RHR    UART_REG(0x00)      // Receive Holding Register
#define UART_LSB    UART_REG(0x00)      // Divisor Latch LSB (when DLAB (Divisor Latch Access Bit in the LCR)=1)
                                        // holds the lower 8 bits of a 16-bit divisor value

#define UART_IER    UART_REG(0x01)      // Interrupt Enable Register
#define UART_MSB    UART_REG(0x01)      // Divisor Latch MSB (when DLAB=1)
                                        // holds the higher 8 bits of a 16-bit divisor value
                                        
#define UART_ISR    UART_REG(0x02)      // Interrupt Status Register
#define UART_FCR    UART_REG(0x02)      // FIFO Control Register
#define UART_LCR    UART_REG(0x03)      // Line Control Register
#define UART_LSR    UART_REG(0x05)      // Line Status Register 
                                        // (Check if data is ready or if THR is empty)

#define IER_RX_ENABLE   (1 << 0)
#define IER_TX_ENABLE   (1 << 1)
#define FCR_FIFO_ENABLE (1 << 0)
#define FCR_FIFO_CLEAR  (3 << 1)
#define LCR_DLAB        (1 << 7)
#define LSR_RX_READY    (1 << 0)
#define LSR_TX_IDLE     (1 << 5)

void uart_init(uint32_t baudrate, uint64_t clock_freq);
void uart_putc(char c);
char uart_getc(void);
void uart_puts(const char *str);
int uart_gets(char *buf, int maxlen);

#endif