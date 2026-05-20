#pragma once
#include <stdint.h>
#include <stddef.h>

void serial_init(void);
void serial_putc(char c);
void serial_puts(const char *s);
void serial_printf(const char *fmt, ...);
