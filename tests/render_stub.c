/* tests/render_stub.c */
#include "render.h"
#include "render_stub.h"

render_log_t render_log;

void render_cell(uint8_t x, uint8_t y, uint8_t ch, uint8_t attr) {
    (void)x; (void)y; (void)ch; (void)attr;
    render_log.cells++;
}

void render_fill(uint8_t x0, uint8_t y0, uint8_t x1, uint8_t y1, uint8_t attr) {
    (void)x0; (void)y0; (void)x1; (void)y1; (void)attr;
    render_log.fills++;
}

void render_scroll_up(uint8_t top, uint8_t bottom, uint8_t attr) {
    (void)attr;
    render_log.scroll_ups++;
    render_log.last_top = top;
    render_log.last_bottom = bottom;
}

void render_scroll_down(uint8_t top, uint8_t bottom, uint8_t attr) {
    (void)attr;
    render_log.scroll_downs++;
    render_log.last_top = top;
    render_log.last_bottom = bottom;
}
