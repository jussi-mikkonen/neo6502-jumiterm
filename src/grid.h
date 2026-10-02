/* 80x25 terminal model: character/attribute shadow buffer, cursor,
   deferred autowrap and scroll region. Every change is drawn through
   render.h. Coordinates are 0-based. */
#ifndef GRID_H
#define GRID_H

#include <stdbool.h>
#include <stdint.h>

#define GRID_COLS 80
#define GRID_ROWS 25
#define ATTR_DEFAULT 0x07

typedef struct {
    uint8_t ch[GRID_ROWS][GRID_COLS];
    uint8_t attr[GRID_ROWS][GRID_COLS];
    uint8_t cx, cy;
    bool wrap_pending;          /* printed in column 80; wrap before the next character */
    uint8_t top, bottom;        /* scroll region, inclusive */
    uint8_t cur_attr;           /* used for new characters and for erasing */
    uint8_t saved_cx, saved_cy;
} grid_t;

extern grid_t grid;

void grid_reset(void);
void grid_put(uint8_t ch);
void grid_cr(void);
void grid_lf(void);
void grid_ri(void);
void grid_bs(void);
void grid_tab(void);
void grid_goto(uint8_t x, uint8_t y);
void grid_erase_display(uint8_t mode);   /* 0: cursor to end, 1: start to cursor, 2: all + home */
void grid_erase_line(uint8_t mode);      /* 0: cursor to end, 1: start to cursor, 2: whole line */
void grid_erase_chars(uint8_t n);
void grid_set_region(uint8_t top, uint8_t bottom);
void grid_insert_lines(uint8_t n);
void grid_delete_lines(uint8_t n);
void grid_insert_chars(uint8_t n);
void grid_delete_chars(uint8_t n);
void grid_save_cursor(void);
void grid_restore_cursor(void);
void grid_refresh_cell(uint8_t x, uint8_t y);
void grid_redraw(void);                  /* draw every cell again from the shadow buffer */

#endif
