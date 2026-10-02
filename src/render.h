/* Drawing interface used by the terminal model and the UI.
   Coordinates are character cells: x 0..79, y 0..29 (rows 25..29 are the
   status area below the 80x25 terminal). attr = (background << 4) | foreground,
   colours 0..15 in ANSI order. */
#ifndef RENDER_H
#define RENDER_H

#include <stdint.h>

void render_init(void);
void render_shutdown(void);
void render_cell(uint8_t x, uint8_t y, uint8_t ch, uint8_t attr);
/* Fill the inclusive rectangle with blanks in attr's background colour. */
void render_fill(uint8_t x0, uint8_t y0, uint8_t x1, uint8_t y1, uint8_t attr);
/* Move rows top+1..bottom up one row; clear row bottom with attr's background. */
void render_scroll_up(uint8_t top, uint8_t bottom, uint8_t attr);
/* Move rows top..bottom-1 down one row; clear row top with attr's background. */
void render_scroll_down(uint8_t top, uint8_t bottom, uint8_t attr);
/* Underline cursor in the cell's foreground colour. */
void render_cursor(uint8_t x, uint8_t y, uint8_t attr);
void render_text(uint8_t x, uint8_t y, const char *s, uint8_t attr);

#endif
