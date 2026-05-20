#include "keyboard.h"
#include "utils.h"
#include "pic.h"
#include "serial.h"
#include <stdint.h>
#include <stdbool.h>

// ---------------------------------------------------------------------------
// PS/2 scancode set 1 → key_t table (only keys we care about for Doom)
// ---------------------------------------------------------------------------
#define SC_W      0x11
#define SC_A      0x1E
#define SC_S      0x1F
#define SC_D      0x20
#define SC_H      0x23
#define SC_L      0x26
#define SC_SPACE  0x39
#define SC_1      0x02
#define SC_2      0x03
#define SC_3      0x04
#define SC_4      0x05
#define SC_5      0x06
#define SC_6      0x07
#define SC_ESC    0x01
#define SC_ENTER  0x1C
#define SC_LCTRL  0x1D
#define SC_LSHIFT 0x2A
#define SC_RSHIFT 0x36
#define SC_LALT   0x38
#define SC_UP     0x48
#define SC_DOWN   0x50
#define SC_LEFT   0x4B
#define SC_RIGHT  0x4D

static key_t sc_to_key(uint8_t sc) {
    switch (sc) {
    case SC_W:     return KEY_W;
    case SC_A:     return KEY_A;
    case SC_S:     return KEY_S;
    case SC_D:     return KEY_D;
    case SC_H:     return KEY_H;
    case SC_L:     return KEY_L;
    case SC_SPACE: return KEY_SPACE;
    case SC_1:     return KEY_1;
    case SC_2:     return KEY_2;
    case SC_3:     return KEY_3;
    case SC_4:     return KEY_4;
    case SC_5:     return KEY_5;
    case SC_6:     return KEY_6;
    case SC_ESC:   return KEY_ESC;
    case SC_ENTER: return KEY_ENTER;
    case SC_LCTRL: return KEY_CTRL;
    case SC_LSHIFT:
    case SC_RSHIFT:return KEY_SHIFT;
    case SC_LALT:  return KEY_ALT;
    case SC_UP:    return KEY_UP;
    case SC_DOWN:  return KEY_DOWN;
    case SC_LEFT:  return KEY_LEFT;
    case SC_RIGHT: return KEY_RIGHT;
    default:       return KEY_NONE;
    }
}

// ---------------------------------------------------------------------------
// Simple ring-buffer event queue
// ---------------------------------------------------------------------------
#define KB_BUF_SIZE 64
static key_event_t kb_buf[KB_BUF_SIZE];
static volatile uint32_t kb_head = 0; // write index
static volatile uint32_t kb_tail = 0; // read index

static void kb_push(key_event_t ev) {
    uint32_t next = (kb_head + 1) % KB_BUF_SIZE;
    if (next != kb_tail) { // drop if full
        kb_buf[kb_head] = ev;
        kb_head = next;
    }
}

bool keyboard_poll(key_event_t *out) {
    if (kb_tail == kb_head) return false;
    *out = kb_buf[kb_tail];
    kb_tail = (kb_tail + 1) % KB_BUF_SIZE;
    return true;
}

// ---------------------------------------------------------------------------
// IRQ1 handler
// ---------------------------------------------------------------------------
struct interrupt_frame;
__attribute__((interrupt)) void irq1_keyboard_handler(struct interrupt_frame *frame) {
    (void)frame;
    uint8_t sc = inb(0x60);
    bool released = (sc & 0x80) != 0;
    sc &= 0x7F;

    key_t k = sc_to_key(sc);
    if (k != KEY_NONE) {
        kb_push((key_event_t){ .key = k, .pressed = !released });
        serial_printf("[kbd] %s 0x%x\n", released ? "up" : "dn", (unsigned)k);
    }

    pic_send_eoi(1);
}

void keyboard_init(void) {
    // Flush any pending PS/2 data
    while (inb(0x64) & 0x01) inb(0x60);
    serial_puts("[kbd] initialized\n");
}
