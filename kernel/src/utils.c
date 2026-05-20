#include "utils.h"
#include <stddef.h>

void outb(uint16_t port, uint8_t val) {
  __asm__ volatile("outb %0, %1" : : "a"(val), "Nd"(port));
}

uint8_t inb(uint16_t port) {
  uint8_t ret;
  __asm__ volatile("inb %1, %0" : "=a"(ret) : "Nd"(port));
  return ret;
}

void io_wait(void) { outb(0x80, 0); }

// Global tick counter — incremented by the PIT IRQ0 handler at 100Hz.
volatile uint32_t g_ticks = 0;

// Tick function handling timer interrupt actions.
void tick(void) {
    g_ticks++;
}
