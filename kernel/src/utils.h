#pragma once
#include <stdint.h>
#include <stddef.h>
uint8_t inb(uint16_t port);
void outb(uint16_t port, uint8_t val);
void io_wait(void);
void tick(void);
