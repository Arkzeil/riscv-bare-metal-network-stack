#include "kernel/uart.h"

void uart_init(uint32_t baudrate, uint64_t clock_freq) {
    unsigned int divisor = clock_freq / baudrate;
    // Enable DLAB to set baud rate divisor
    UART_LCR |= LCR_DLAB;
    UART_LSB = (unsigned char)(divisor & 0xFF);         //
    UART_MSB = (unsigned char)((divisor >> 8) & 0xFF);  //
    UART_LCR &= ~LCR_DLAB; // Disable DLAB

    // Set data format: 8 bits, no parity, one stop bit
    UART_LCR = 0x03;

    // Enable FIFO, clear them, with 14-byte threshold
    UART_FCR = FCR_FIFO_ENABLE | FCR_FIFO_CLEAR;
}

void uart_putc(char c) {    
    while (UART_THR & 0x80) // Wait until TX is ready
        asm volatile ("nop");

    UART_THR = (unsigned char)c;
}

char uart_getc(void) {
    while (UART_RHR & 0x80) // Wait until RX has data
        asm volatile ("nop");

    return (char)(UART_RHR & 0xFF);
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