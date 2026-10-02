/* src/keymap.c */
#include "keymap.h"

static uint8_t seq(uint8_t *out, const char *s) {
    uint8_t n = 0;
    while (s[n]) {
        out[n] = (uint8_t)s[n];
        n++;
    }
    return n;
}

uint8_t keymap_nav_hid(uint8_t c) {
    switch (c) {
    case 0x01: return 0x50;   /* Left      (also Ctrl-A) */
    case 0x04: return 0x4F;   /* Right     (Ctrl-D) */
    case 0x17: return 0x52;   /* Up        (Ctrl-W) */
    case 0x13: return 0x51;   /* Down      (Ctrl-S) */
    case 0x14: return 0x4A;   /* Home      (Ctrl-T) */
    case 0x07: return 0x4D;   /* End       (Ctrl-G) */
    case 0x12: return 0x4B;   /* Page Up   (Ctrl-R) */
    case 0x06: return 0x4E;   /* Page Down (Ctrl-F) */
    case 0x05: return 0x49;   /* Insert    (Ctrl-E) */
    case 0x1A: return 0x4C;   /* Delete    (Ctrl-Z) */
    default: return 0;
    }
}

uint8_t keymap_translate(uint8_t c, bool nav_down, uint8_t *out) {
    if (nav_down) {
        switch (c) {
        case 0x01: return seq(out, "\033[D");
        case 0x04: return seq(out, "\033[C");
        case 0x17: return seq(out, "\033[A");
        case 0x13: return seq(out, "\033[B");
        case 0x14: return seq(out, "\033[H");
        case 0x07: return seq(out, "\033[F");
        case 0x12: return seq(out, "\033[5~");
        case 0x06: return seq(out, "\033[6~");
        case 0x05: return seq(out, "\033[2~");
        case 0x1A: out[0] = 0x7F; return 1;
        default: break;
        }
    }
    if (c == 0 || c >= 0x80) return 0;
    out[0] = c;
    return 1;
}
