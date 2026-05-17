#include "idt.h"
#include <limine.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "pic.h"
#include "utils.h"

volatile uint32_t *g_fb_ptr = NULL;
size_t g_fb_width = 0;
size_t g_fb_height = 0;
size_t g_fb_pitch = 0;

void pit_init(uint32_t frequency) {
    uint32_t divisor = 1193180 / frequency;
    outb(0x43, 0x36);
    outb(0x40, (uint8_t)(divisor & 0xFF));
    outb(0x40, (uint8_t)((divisor >> 8) & 0xFF));
}

static inline void disable_apic(void) {
    uint32_t eax, edx;
    __asm__ volatile("rdmsr" : "=a"(eax), "=d"(edx) : "c"(0x1B));
    eax &= ~((1 << 11) | (1 << 10)); // Clear bit 11 (EN) and bit 10 (EXTD/x2APIC)
    __asm__ volatile("wrmsr" : : "a"(eax), "d"(edx), "c"(0x1B));
}

struct interrupt_frame;

__attribute__((interrupt)) void irq0_handler(struct interrupt_frame *frame) {
    (void)frame; // suppress unused warning
    tick();
    pic_send_eoi(0);
}


// Set the base revision to 6, this is recommended as this is the latest
// base revision described by the Limine boot protocol specification.
// See specification for further info.

__attribute__((used, section(".limine_requests"))) static volatile uint64_t
    limine_base_revision[] = LIMINE_BASE_REVISION(6);

// The Limine requests can be placed anywhere, but it is important that
// the compiler does not optimise them away, so, usually, they should
// be made volatile or equivalent, _and_ they should be accessed at least
// once or marked as used with the "used" attribute as done here.

__attribute__((
    used,
    section(
        ".limine_requests"))) static volatile struct limine_framebuffer_request
    framebuffer_request = {.id = LIMINE_FRAMEBUFFER_REQUEST_ID, .revision = 0};

// Finally, define the start and end markers for the Limine requests.
// These can also be moved anywhere, to any .c file, as seen fit.

__attribute__((used,
               section(".limine_requests_start"))) static volatile uint64_t
    limine_requests_start_marker[] = LIMINE_REQUESTS_START_MARKER;

__attribute__((used, section(".limine_requests_end"))) static volatile uint64_t
    limine_requests_end_marker[] = LIMINE_REQUESTS_END_MARKER;

// Halt and catch fire function.
static void hcf(void) {
  for (;;) {
    asm("hlt");
  }
}

// The following will be our kernel's entry point.
// If renaming kmain() to something else, make sure to change the
// linker script accordingly.
void kmain(void) {
  // Ensure the bootloader actually understands our base revision (see spec).
  if (LIMINE_BASE_REVISION_SUPPORTED(limine_base_revision) == false) {
    hcf();
  }

  // Ensure we got a framebuffer.
  if (framebuffer_request.response == NULL ||
      framebuffer_request.response->framebuffer_count < 1) {
    hcf();
  }

  // Fetch the first framebuffer.
  struct limine_framebuffer *framebuffer =
      framebuffer_request.response->framebuffers[0];

  g_fb_ptr = framebuffer->address;
  g_fb_width = framebuffer->width;
  g_fb_height = framebuffer->height;
  g_fb_pitch = framebuffer->pitch;

  // Print a nice pattern to screen as an example.
  // Note: we assume the framebuffer model is RGB with 32-bit pixels.
  for (size_t y = 0; y < framebuffer->height; y++) {
    for (size_t x = 0; x < framebuffer->width; x++) {
      uint32_t nX = x * 255 / framebuffer->width;
      uint32_t nY = y * 255 / framebuffer->height;
      g_fb_ptr[y * (framebuffer->pitch / 4) + x] = (nY << 16) | nX;
    }
  }

  idt_init();
  disable_apic();    // Disable Local APIC so legacy PIC interrupts reach CPU
  pic_remap(32, 40);
  pic_clear_mask(0); // Unmask IRQ0 (Timer)
  pit_init(100);     // Set PIT to 100 Hz
  __asm__ volatile("sti");

  // We're done, just hang...
  hcf();
}
