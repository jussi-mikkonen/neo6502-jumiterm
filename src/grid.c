#include <string.h>
#include "grid.h"
#include "render.h"

grid_t grid;

/* Blanks cells in the model only; the caller updates the screen. */
static void clear_cells(uint8_t x0, uint8_t y0, uint8_t x1, uint8_t y1) {
    for (uint8_t y = y0; y <= y1; y++) {
        memset(&grid.ch[y][x0], ' ', x1 - x0 + 1);
        memset(&grid.attr[y][x0], grid.cur_attr, x1 - x0 + 1);
    }
}

static void clear_rect(uint8_t x0, uint8_t y0, uint8_t x1, uint8_t y1) {
    clear_cells(x0, y0, x1, y1);
    render_fill(x0, y0, x1, y1, grid.cur_attr);
}

static void scroll_up(uint8_t top, uint8_t bottom) {
    if (bottom > top) {
        memmove(grid.ch[top], grid.ch[top + 1], (bottom - top) * GRID_COLS);
        memmove(grid.attr[top], grid.attr[top + 1], (bottom - top) * GRID_COLS);
    }
    clear_cells(0, bottom, GRID_COLS - 1, bottom);
    render_scroll_up(top, bottom, grid.cur_attr);
}

static void scroll_down(uint8_t top, uint8_t bottom) {
    if (bottom > top) {
        memmove(grid.ch[top + 1], grid.ch[top], (bottom - top) * GRID_COLS);
        memmove(grid.attr[top + 1], grid.attr[top], (bottom - top) * GRID_COLS);
    }
    clear_cells(0, top, GRID_COLS - 1, top);
    render_scroll_down(top, bottom, grid.cur_attr);
}

static void redraw_span(uint8_t x0, uint8_t x1, uint8_t y) {
    for (uint8_t x = x0; x <= x1; x++) render_cell(x, y, grid.ch[y][x], grid.attr[y][x]);
}

void grid_reset(void) {
    grid.cur_attr = ATTR_DEFAULT;
    grid.top = 0;
    grid.bottom = GRID_ROWS - 1;
    grid.cx = 0;
    grid.cy = 0;
    grid.wrap_pending = false;
    grid.saved_cx = 0;
    grid.saved_cy = 0;
    clear_rect(0, 0, GRID_COLS - 1, GRID_ROWS - 1);
}

void grid_lf(void) {
    grid.wrap_pending = false;
    if (grid.cy == grid.bottom) scroll_up(grid.top, grid.bottom);
    else if (grid.cy < GRID_ROWS - 1) grid.cy++;
}

void grid_ri(void) {
    grid.wrap_pending = false;
    if (grid.cy == grid.top) scroll_down(grid.top, grid.bottom);
    else if (grid.cy > 0) grid.cy--;
}

void grid_put(uint8_t ch) {
    if (grid.wrap_pending) {
        grid.cx = 0;
        grid_lf();
    }
    grid.ch[grid.cy][grid.cx] = ch;
    grid.attr[grid.cy][grid.cx] = grid.cur_attr;
    render_cell(grid.cx, grid.cy, ch, grid.cur_attr);
    if (grid.cx == GRID_COLS - 1) grid.wrap_pending = true;
    else grid.cx++;
}

void grid_cr(void) {
    grid.cx = 0;
    grid.wrap_pending = false;
}

void grid_bs(void) {
    if (grid.cx > 0) grid.cx--;
    grid.wrap_pending = false;
}

void grid_tab(void) {
    grid.cx = (uint8_t)((grid.cx | 7) + 1);
    if (grid.cx > GRID_COLS - 1) grid.cx = GRID_COLS - 1;
    grid.wrap_pending = false;
}

void grid_goto(uint8_t x, uint8_t y) {
    grid.cx = x > GRID_COLS - 1 ? GRID_COLS - 1 : x;
    grid.cy = y > GRID_ROWS - 1 ? GRID_ROWS - 1 : y;
    grid.wrap_pending = false;
}

void grid_erase_display(uint8_t mode) {
    switch (mode) {
    case 0:
        clear_rect(grid.cx, grid.cy, GRID_COLS - 1, grid.cy);
        if (grid.cy < GRID_ROWS - 1) clear_rect(0, grid.cy + 1, GRID_COLS - 1, GRID_ROWS - 1);
        break;
    case 1:
        if (grid.cy > 0) clear_rect(0, 0, GRID_COLS - 1, grid.cy - 1);
        clear_rect(0, grid.cy, grid.cx, grid.cy);
        break;
    case 2:
        clear_rect(0, 0, GRID_COLS - 1, GRID_ROWS - 1);
        grid_goto(0, 0);   /* ANSI.SYS homes the cursor; BBS art relies on it */
        break;
    default:
        break;
    }
}

void grid_erase_line(uint8_t mode) {
    switch (mode) {
    case 0: clear_rect(grid.cx, grid.cy, GRID_COLS - 1, grid.cy); break;
    case 1: clear_rect(0, grid.cy, grid.cx, grid.cy); break;
    case 2: clear_rect(0, grid.cy, GRID_COLS - 1, grid.cy); break;
    default: break;
    }
}

void grid_erase_chars(uint8_t n) {
    uint16_t end = (uint16_t)grid.cx + (n ? n : 1) - 1;
    if (end > GRID_COLS - 1) end = GRID_COLS - 1;
    clear_rect(grid.cx, grid.cy, (uint8_t)end, grid.cy);
}

void grid_set_region(uint8_t top, uint8_t bottom) {
    if (top >= bottom || bottom >= GRID_ROWS) return;
    grid.top = top;
    grid.bottom = bottom;
    grid_goto(0, 0);
}

/* IL/DL: scrolls the region from the cursor row down by n lines. */
static void shift_lines(uint8_t n, bool insert) {
    uint8_t max;
    if (grid.cy < grid.top || grid.cy > grid.bottom) return;
    max = grid.bottom - grid.cy + 1;
    if (n > max) n = max;
    while (n--) {
        if (insert) scroll_down(grid.cy, grid.bottom);
        else scroll_up(grid.cy, grid.bottom);
    }
    grid.cx = 0;
    grid.wrap_pending = false;
}

void grid_insert_lines(uint8_t n) {
    shift_lines(n, true);
}

void grid_delete_lines(uint8_t n) {
    shift_lines(n, false);
}

void grid_insert_chars(uint8_t n) {
    uint8_t x = grid.cx, y = grid.cy;
    uint8_t max = GRID_COLS - x;
    if (n > max) n = max;
    memmove(&grid.ch[y][x + n], &grid.ch[y][x], max - n);
    memmove(&grid.attr[y][x + n], &grid.attr[y][x], max - n);
    memset(&grid.ch[y][x], ' ', n);
    memset(&grid.attr[y][x], grid.cur_attr, n);
    redraw_span(x, GRID_COLS - 1, y);
    grid.wrap_pending = false;
}

void grid_delete_chars(uint8_t n) {
    uint8_t x = grid.cx, y = grid.cy;
    uint8_t max = GRID_COLS - x;
    if (n > max) n = max;
    memmove(&grid.ch[y][x], &grid.ch[y][x + n], max - n);
    memmove(&grid.attr[y][x], &grid.attr[y][x + n], max - n);
    memset(&grid.ch[y][GRID_COLS - n], ' ', n);
    memset(&grid.attr[y][GRID_COLS - n], grid.cur_attr, n);
    redraw_span(x, GRID_COLS - 1, y);
    grid.wrap_pending = false;
}

void grid_save_cursor(void) {
    grid.saved_cx = grid.cx;
    grid.saved_cy = grid.cy;
}

void grid_restore_cursor(void) {
    grid_goto(grid.saved_cx, grid.saved_cy);
}

void grid_refresh_cell(uint8_t x, uint8_t y) {
    render_cell(x, y, grid.ch[y][x], grid.attr[y][x]);
}

void grid_redraw(void) {
    for (uint8_t y = 0; y < GRID_ROWS; y++)
        for (uint8_t x = 0; x < GRID_COLS; x++) grid_refresh_cell(x, y);
}
