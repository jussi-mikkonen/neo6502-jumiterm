/* Byte-at-a-time ANSI/VT100 parser. Printable bytes (including CP437
   0x80-0xFF) go to the grid; control and escape sequences move the cursor,
   erase, scroll and set colours. Replies for the host (cursor position
   reports, device attributes) collect in ansi.reply. */
#ifndef ANSI_H
#define ANSI_H

#include <stdbool.h>
#include <stdint.h>

#define ANSI_MAX_PARAMS 8
#define ANSI_REPLY_MAX 16

typedef struct {
    uint8_t state;
    uint8_t nparams;                    /* index of the parameter being read */
    uint16_t params[ANSI_MAX_PARAMS];
    bool private_marker;                /* '<', '=', '>' or '?' seen: sequence ignored */
    uint8_t fg, bg;                     /* 0..15 before bold/blink are applied */
    bool bold, blink, reverse;
    bool cr_pending;                    /* ground-state CR just executed: printable text next means newline */
    uint8_t reply[ANSI_REPLY_MAX];
    uint8_t reply_len;
} ansi_t;

extern ansi_t ansi;

void ansi_init(void);
void ansi_feed(uint8_t b);

#endif
