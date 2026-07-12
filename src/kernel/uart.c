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
    while (!(UART_LSR & (1 << 5) ) ) // Wait until TX is ready (Transmitter Holding Register Empty)
        asm volatile ("nop");

    UART_THR = (unsigned char)c;
}

char uart_getc(void) {
    while (!(UART_LSR & (1 << 0) ) ) // Wait until RX has data (DR bit set)
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

void uart_dec(unsigned int num) {
    if (num == 0) {
        uart_putc('0');
        return;
    }

    char buffer[10]; // Enough to hold the decimal representation of a 32-bit integer
    int i = 0;

    while (num > 0) {
        buffer[i++] = '0' + (num % 10);
        num /= 10;
    }

    // The digits are in reverse order, so print them in reverse
    for (int j = i - 1; j >= 0; j--) {
        uart_putc(buffer[j]);
    }
}

void uart_dec64(uint64_t num) {
    if (num == 0) {
        uart_putc('0');
        return;
    }

    char buffer[20]; // Enough to hold the decimal representation of a 64-bit integer
    int i = 0;

    while (num > 0) {
        buffer[i++] = '0' + (num % 10);
        num /= 10;
    }

    // The digits are in reverse order, so print them in reverse
    for (int j = i - 1; j >= 0; j--) {
        uart_putc(buffer[j]);
    }
}

void uart_b2x(unsigned int b){
    int i;
    unsigned int t;
    uart_puts("0x");
    // take [32,29] then [28,25] ...
    for(i = 28; i >=0; i-=4){
        // this is the equivalent to following method, as '0' = 0x30 and 0x37 + 10 = 'A'
        // thus convert to ASCII
        t = (b >> i) & 0xF;
        t += (t > 9 ? 0x37:0x30);
        uart_putc(t);
    }
}

void uart_puts_fixed(const char *str, size_t len) {
    for (size_t i = 0; i < len; i++) {
        uart_putc(str[i]);
    }
}

void print_hex64(uint64_t val) {
    uart_puts("0x");
    for (int i = 60; i >= 0; i -= 4) {
        uint64_t t = (val >> i) & 0xF;
        t += (t > 9 ? 0x37 : 0x30);
        uart_putc((char)t);
    }
}