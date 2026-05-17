#include "utils.h"
#include <stddef.h>
extern volatile uint32_t *g_fb_ptr;
extern size_t g_fb_width, g_fb_height, g_fb_pitch;

void outb(uint16_t port, uint8_t val) {
  __asm__ volatile("outb %0, %1" : : "a"(val), "Nd"(port));
}

uint8_t inb(uint16_t port) {
  uint8_t ret;
  __asm__ volatile("inb %1, %0" : "=a"(ret) : "Nd"(port));
  return ret;
}

void io_wait(void) { outb(0x80, 0); }

// Tick function handling timer interrupt actions.
void tick(void) {
    static uint32_t ticks = 0;
    static int x_pos = 0;
    static int y_pos = 50;
    static int x_dir = 1;
    ticks++;
    if (g_fb_ptr) {
        if (ticks % 2 == 0) { // Update position every 2 ticks (~50 FPS)
            // Restore background for old square
            for (size_t y = 0; y < 50; y++) {
                for (size_t x = 0; x < 50; x++) {
                    size_t cur_x = x_pos + x;
                    size_t cur_y = y_pos + y;
                    if (cur_y < g_fb_height && cur_x < g_fb_width) {
                        uint32_t nX = cur_x * 255 / g_fb_width;
                        uint32_t nY = cur_y * 255 / g_fb_height;
                        g_fb_ptr[cur_y * (g_fb_pitch / 4) + cur_x] = (nY << 16) | nX;
                    }
                }
            }
            // Update position
            x_pos += x_dir * 5;
            if (x_pos >= (int)g_fb_width - 50) {
                x_pos = g_fb_width - 50;
                x_dir = -1;
            } else if (x_pos <= 0) {
                x_pos = 0;
                x_dir = 1;
            }
            // Draw new square (solid white)
            for (size_t y = 0; y < 50; y++) {
                for (size_t x = 0; x < 50; x++) {
                    size_t cur_x = x_pos + x;
                    size_t cur_y = y_pos + y;
                    if (cur_y < g_fb_height && cur_x < g_fb_width) {
                        g_fb_ptr[cur_y * (g_fb_pitch / 4) + cur_x] = 0x00FFFFFF;
                    }
                }
            }
        }
    }
}
