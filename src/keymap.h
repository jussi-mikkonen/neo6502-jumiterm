/* src/keymap.h - Neo key codes to bytes sent to the BBS. */
#ifndef KEYMAP_H
#define KEYMAP_H

#include <stdbool.h>
#include <stdint.h>

#define KEY_LOCAL_MENU 0x81   /* F1 is defined as a hotkey sending this byte */

uint8_t keymap_nav_hid(uint8_t c);
uint8_t keymap_translate(uint8_t c, bool nav_down, uint8_t *out);

#endif
