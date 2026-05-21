#include "libc.h"
#include "keyboard.h"
#include <stdint.h>
#include <stddef.h>

extern uint32_t* DG_ScreenBuffer;
extern volatile uint32_t *g_fb_ptr;
extern size_t g_fb_width, g_fb_height, g_fb_pitch;
extern volatile uint32_t g_ticks;

// DG_ScreenBuffer is DOOMGENERIC_RESX x DOOMGENERIC_RESY pixels,
// already scaled and color-converted by I_FinishUpdate in i_video.c.
// DOOM_SCALE is passed from the Makefile (default 2).
#ifndef DOOM_SCALE
#define DOOM_SCALE 2
#endif
#define DG_W (320 * DOOM_SCALE)
#define DG_H (200 * DOOM_SCALE)

void DG_Init(void) {
    // Nothing special needed here for now
}

void DG_DrawFrame(void) {
    if (!g_fb_ptr || !DG_ScreenBuffer) return;

    // Center the 640x400 image on the actual framebuffer
    size_t offset_x = 0;
    if (g_fb_width > DG_W) {
        offset_x = (g_fb_width - DG_W) / 2;
    }
    size_t offset_y = 0;
    if (g_fb_height > DG_H) {
        offset_y = (g_fb_height - DG_H) / 2;
    }

    size_t copy_w = DG_W;
    if (copy_w > g_fb_width) copy_w = g_fb_width;
    size_t copy_h = DG_H;
    if (copy_h > g_fb_height) copy_h = g_fb_height;

    size_t fb_stride = g_fb_pitch / 4;  // pixels per row in framebuffer

    for (size_t y = 0; y < copy_h; y++) {
        uint32_t *src = &DG_ScreenBuffer[y * DG_W];
        uint32_t *dst = (uint32_t *)&g_fb_ptr[(offset_y + y) * fb_stride + offset_x];
        memcpy(dst, src, copy_w * 4);
    }
}

int DG_GetKey(int *pressed, unsigned char *doomKey) {
    key_event_t event;
    if (keyboard_poll(&event)) {
        *pressed = event.pressed ? 1 : 0;

        // Map keyboard to Doom keys
        // WASD    = move forward/back, strafe left/right
        // H / L   = turn camera left / right
        // Space   = use (open doors, switches)
        // Ctrl    = fire
        // Q / E   = strafe left / right (alt)
        // Shift   = run
        // 1-6     = weapon select (passed as ASCII)
        unsigned char k = 0;
        switch (event.key) {
            // Movement
            case 'w':   k = 0xad; break; // KEY_UPARROW   (forward)
            case 's':   k = 0xaf; break; // KEY_DOWNARROW  (backward)
            case 'a':   k = 0xa0; break; // KEY_STRAFE_L   (strafe left)
            case 'd':   k = 0xa1; break; // KEY_STRAFE_R   (strafe right)

            // Camera / turning
            case 'h':   k = 0xac; break; // KEY_LEFTARROW  (turn left)
            case 'l':   k = 0xae; break; // KEY_RIGHTARROW (turn right)

            // Alt strafe
            case 'q':   k = 0xa0; break; // KEY_STRAFE_L
            case 'e':   k = 0xa1; break; // KEY_STRAFE_R

            // Actions
            case ' ':   k = 0xa2; break; // KEY_USE  (open doors / switches)

            // Arrow keys still work as fallback
            case KEY_UP:    k = 0xad; break;
            case KEY_DOWN:  k = 0xaf; break;
            case KEY_LEFT:  k = 0xac; break;
            case KEY_RIGHT: k = 0xae; break;

            // Modifiers
            case KEY_CTRL:  k = 0x80 + 0x1d; break; // KEY_RCTRL (fire)
            case KEY_ALT:   k = 0x80 + 0x38; break; // KEY_RALT
            case KEY_SHIFT: k = 0x80 + 0x36; break; // KEY_RSHIFT (run)
            case KEY_ESC:   k = 27; break;
            case KEY_ENTER: k = 13; break;

            // 1-6 weapon switch + everything else: pass as ASCII
            default:        k = (unsigned char)event.key; break;
        }
        *doomKey = k;
        return 1;
    }
    return 0;
}

void DG_SetWindowTitle(const char *title) {
    (void)title;
}

uint32_t DG_GetTicksMs(void) {
    // PIT runs at 100Hz -> 10ms per tick
    return g_ticks * 10;
}

void DG_SleepMs(uint32_t ms) {
    uint32_t start = g_ticks;
    uint32_t wait_ticks = ms / 10;
    if (wait_ticks == 0) wait_ticks = 1;
    
    while (g_ticks - start < wait_ticks) {
        __asm__ volatile ("hlt");
    }
}
