#pragma once
#include <stdint.h>
#include <stdbool.h>

// Doom-oriented key codes
typedef enum {
    KEY_NONE   = 0,
    KEY_W      = 'w',
    KEY_A      = 'a',
    KEY_S      = 's',
    KEY_D      = 'd',
    KEY_H      = 'h',
    KEY_L      = 'l',
    KEY_SPACE  = ' ',
    KEY_CTRL   = 0x80,
    KEY_ALT    = 0x81,
    KEY_SHIFT  = 0x82,
    KEY_ESC    = 0x1B,
    KEY_ENTER  = '\r',
    KEY_1 = '1', KEY_2 = '2', KEY_3 = '3',
    KEY_4 = '4', KEY_5 = '5', KEY_6 = '6',
    KEY_UP    = 0x90,
    KEY_DOWN  = 0x91,
    KEY_LEFT  = 0x92,
    KEY_RIGHT = 0x93,
} key_t;

typedef struct {
    key_t  key;
    bool   pressed; // true = keydown, false = keyup
} key_event_t;

void keyboard_init(void);

// Returns true if an event is available, fills *out_event
bool keyboard_poll(key_event_t *out_event);


