#include "kernel/uart.h"

void uart_init(uint32_t baudrate, uint64_t clock_freq) {
    unsigned int divisor = clock_freq / baudrate;
    UART_DIV = (unsigned char)(divisor & 0xFF);
    UART_TXCTRL = 0x01; // Enable transmitter
    UART_RXCTRL = 0x01; // Enable receiver
}


void uart_putc(char c) {    
    while (UART_TXDATA & 0x80) // Wait until TX is ready
        asm volatile ("nop");
    
    UART_TXDATA = (unsigned char)c;
}

char uart_getc(void) {
    while (UART_RXDATA & 0x80) // Wait until RX has data
        asm volatile ("nop");

    return (char)(UART_RXDATA & 0xFF);
}

void uart_puts(const char *str) {
    while (*str) {
        uart_putc(*str++);
    }
}

int uart_gets(char *buf, int maxlen) {
    int i = 0;
    char c;
    while (i < maxlen - 1) {
        c = uart_getc();
        if (c == '\n' || c == '\r') {
            break;
        }
        buf[i++] = c;
    }
    buf[i] = '\0';
    return i;
}