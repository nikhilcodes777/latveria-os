#include "serial.h"
#include "utils.h"
#include <stdarg.h>
#include <stdint.h>

#define COM1 0x3F8

void serial_init(void) {
    outb(COM1 + 1, 0x00); // Disable interrupts
    outb(COM1 + 3, 0x80); // Enable DLAB
    outb(COM1 + 0, 0x03); // Baud divisor lo: 38400 baud
    outb(COM1 + 1, 0x00); // Baud divisor hi
    outb(COM1 + 3, 0x03); // 8 bits, no parity, one stop bit
    outb(COM1 + 2, 0xC7); // Enable FIFO, clear, 14-byte threshold
    outb(COM1 + 4, 0x03); // IRQs disabled, RTS/DSR set
}

static void serial_wait(void) {
    while (!(inb(COM1 + 5) & 0x20));
}

void serial_putc(char c) {
    if (c == '\n') { serial_wait(); outb(COM1, '\r'); }
    serial_wait();
    outb(COM1, (uint8_t)c);
}

void serial_puts(const char *s) {
    for (; *s; s++) serial_putc(*s);
}

// Minimal printf: %s %d %u %x %c %%
void serial_printf(const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    for (; *fmt; fmt++) {
        if (*fmt != '%') { serial_putc(*fmt); continue; }
        fmt++;
        switch (*fmt) {
        case 's': { const char *s = va_arg(ap, const char *); serial_puts(s ? s : "(null)"); break; }
        case 'c': { serial_putc((char)va_arg(ap, int)); break; }
        case 'd':
        case 'i': {
            long v = va_arg(ap, int);
            if (v < 0) { serial_putc('-'); v = -v; }
            char buf[21]; int i = 20; buf[i] = 0;
            do { buf[--i] = '0' + (v % 10); v /= 10; } while (v);
            serial_puts(buf + i); break;
        }
        case 'u': {
            unsigned long v = va_arg(ap, unsigned int);
            char buf[21]; int i = 20; buf[i] = 0;
            do { buf[--i] = '0' + (v % 10); v /= 10; } while (v);
            serial_puts(buf + i); break;
        }
        case 'x':
        case 'X': {
            unsigned long v = va_arg(ap, unsigned int);
            const char *hex = (*fmt == 'x') ? "0123456789abcdef" : "0123456789ABCDEF";
            char buf[17]; int i = 16; buf[i] = 0;
            do { buf[--i] = hex[v & 0xF]; v >>= 4; } while (v);
            serial_puts(buf + i); break;
        }
        case 'p': {
            unsigned long v = (unsigned long)va_arg(ap, void *);
            serial_puts("0x");
            char buf[17]; int i = 16; buf[i] = 0;
            do { buf[--i] = "0123456789abcdef"[v & 0xF]; v >>= 4; } while (v);
            serial_puts(buf + i); break;
        }
        case '%': serial_putc('%'); break;
        default:  serial_putc('%'); serial_putc(*fmt); break;
        }
    }
    va_end(ap);
}
