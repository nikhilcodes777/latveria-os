#include "idt.h"
#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
//  Limine kernel code segment
#define GDT_OFFSET_KERNEL_CODE (0x28)
#define IDT_MAX_DESCRIPTORS (256)

__attribute__((aligned(0x10))) static idt_entry_t idt[IDT_MAX_DESCRIPTORS] = {
    0};
static bool vectors[IDT_MAX_DESCRIPTORS] = {false};
extern void *isr_stub_table[];
static idtr_t idtr;

extern volatile uint32_t *g_fb_ptr;
extern size_t g_fb_width, g_fb_height, g_fb_pitch;

void exception_handler(void) {
  if (g_fb_ptr) {
    for (size_t y = 0; y < g_fb_height; y++) {
      for (size_t x = 0; x < g_fb_width; x++) {
        g_fb_ptr[y * (g_fb_pitch / 4) + x] = 0x00FFFF00; // YELLOW
      }
    }
  }
  for (;;) {
    __asm__ volatile("cli; hlt"); // Completely hangs the computer
  }
}

void idt_set_descriptor(uint8_t vector, void *isr, uint8_t flags) {
  idt_entry_t *descriptor = &idt[vector];

  descriptor->isr_low = (uint64_t)isr & 0xFFFF;
  descriptor->isr_mid = ((uint64_t)isr >> 16) & 0xFFFF;
  descriptor->isr_high = ((uint64_t)isr >> 32) & 0xFFFFFFFF;

  uint16_t current_cs;
  __asm__ volatile ("mov %%cs, %0" : "=r" (current_cs));
  descriptor->kernel_cs = current_cs;
  descriptor->ist = 0;
  descriptor->attributes = flags;
  descriptor->reserved = 0;
}

void idt_init() {
  idtr.base = (uintptr_t)&idt[0];
  idtr.limit = (uint16_t)sizeof(idt_entry_t) * IDT_MAX_DESCRIPTORS - 1;

  extern void dummy_stub(void);
  for (uint16_t vector = 0; vector < IDT_MAX_DESCRIPTORS; vector++) {
    idt_set_descriptor(vector, dummy_stub, 0x8E);
  }

  for (uint8_t vector = 0; vector < 32; vector++) {
    idt_set_descriptor(vector, isr_stub_table[vector], 0x8E);
    vectors[vector] = true;
  }
  extern void irq0_handler(void);
  idt_set_descriptor(32, irq0_handler, 0x8E);
  vectors[32] = true;

  extern void irq1_keyboard_handler(void);
  idt_set_descriptor(33, irq1_keyboard_handler, 0x8E);
  vectors[33] = true;

  __asm__ volatile("lidt %0" : : "m"(idtr)); // load the new IDT
  
  extern void serial_puts(const char *s);
  serial_puts("[idt] IDT loaded successfully\n");
  //__asm__ volatile("sti");                   // set the interrupt flag
}
