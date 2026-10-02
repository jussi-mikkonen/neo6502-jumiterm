#include <string.h>
#include <neo/console.h>
#include <neo/graphics.h>
#include <neo/system.h>
#include "api.h"
#include "../font.h"
#include "../render.h"

#define SCREEN_W 320

/* CGA palette in ANSI colour order: 0-7 normal, 8-15 bright. */
static const uint8_t cga[16][3] = {
    {0x00, 0x00, 0x00}, {0xAA, 0x00, 0x00}, {0x00, 0xAA, 0x00}, {0xAA, 0x55, 0x00},
    {0x00, 0x00, 0xAA}, {0xAA, 0x00, 0xAA}, {0x00, 0xAA, 0xAA}, {0xAA, 0xAA, 0xAA},
    {0x55, 0x55, 0x55}, {0xFF, 0x55, 0x55}, {0x55, 0xFF, 0x55}, {0xFF, 0xFF, 0x55},
    {0x55, 0x55, 0xFF}, {0xFF, 0x55, 0xFF}, {0x55, 0xFF, 0xFF}, {0xFF, 0xFF, 0xFF},
};

static uint8_t ones[SCREEN_W / 2];   /* one screen line of value-1 pixels, pair format */

static blit_area_t solid_src;        /* 'ones' with stride 0: shape for rectangle fills */
static blit_area_t glyph_src;        /* one font glyph */
static blit_area_t video_src, video_dst;

void render_init(void) {
    memset(ones, 0x11, sizeof ones);
    solid_src.address = (uint16_t)ones;
    solid_src.format = BLTFMT_PAIR;
    solid_src.stride = 0;
    glyph_src.format = BLTFMT_PAIR;
    glyph_src.stride = 2;
    glyph_src.height = 8;
    glyph_src.width = 4;
    video_src.format = BLTFMT_BYTE;
    video_src.width = SCREEN_W;
    video_dst.format = BLTFMT_BYTE;
    for (uint8_t i = 0; i < 16; i++) {
        neo_graphics_set_palette(i, cga[i][0], cga[i][1], cga[i][2]);
    }
    render_fill(0, 0, 79, 29, 0x00);
}

void render_shutdown(void) {
    neo_graphics_reset_palette();
    neo_console_clear_screen();
}

void render_fill(uint8_t x0, uint8_t y0, uint8_t x1, uint8_t y1, uint8_t attr) {
    solid_src.solid = attr >> 4;
    solid_src.width = (uint16_t)(x1 - x0 + 1) * 4;
    solid_src.height = (uint8_t)((y1 - y0 + 1) * 8);
    api_blit_image(BLT_SOLID, &solid_src, x0 * 4, y0 * 8);
}

void render_cell(uint8_t x, uint8_t y, uint8_t ch, uint8_t attr) {
    int16_t px = x * 4, py = y * 8;
    solid_src.solid = attr >> 4;
    solid_src.width = 4;
    solid_src.height = 8;
    api_blit_image(BLT_SOLID, &solid_src, px, py);
    if (ch == ' ' || ch == 0) return;
    glyph_src.address = (uint16_t)(font_data + (uint16_t)ch * FONT_BYTES_PER_GLYPH);
    glyph_src.solid = attr & 0x0F;
    api_blit_image(BLT_SOLID, &glyph_src, px, py);
}

static void video_line(blit_area_t *a, uint16_t line) {
    uint32_t off = (uint32_t)line * SCREEN_W;
    a->address = (uint16_t)off;
    a->page = 0x80 + (uint8_t)(off >> 16);
}

void render_scroll_up(uint8_t top, uint8_t bottom, uint8_t attr) {
    if (bottom > top) {
        video_line(&video_src, (top + 1) * 8);
        video_line(&video_dst, top * 8);
        video_src.stride = SCREEN_W;
        video_dst.stride = SCREEN_W;
        video_src.height = (uint8_t)((bottom - top) * 8);
        api_blit_complex(BLT_COPY, &video_src, &video_dst);
    }
    render_fill(0, bottom, 79, bottom, attr);
}

void render_scroll_down(uint8_t top, uint8_t bottom, uint8_t attr) {
    if (bottom > top) {
        video_line(&video_src, bottom * 8 - 1);         /* last line of row bottom-1 */
        video_line(&video_dst, (bottom + 1) * 8 - 1);   /* last line of row bottom */
        video_src.stride = -SCREEN_W;
        video_dst.stride = -SCREEN_W;
        video_src.height = (uint8_t)((bottom - top) * 8);
        api_blit_complex(BLT_COPY, &video_src, &video_dst);
    }
    render_fill(0, top, 79, top, attr);
}

void render_cursor(uint8_t x, uint8_t y, uint8_t attr) {
    uint8_t c = attr & 0x0F;
    if (c == (attr >> 4)) c ^= 0x0F;   /* stay visible on fg == bg cells */
    solid_src.solid = c;
    solid_src.width = 4;
    solid_src.height = 1;
    api_blit_image(BLT_SOLID, &solid_src, x * 4, y * 8 + 7);
}

void render_text(uint8_t x, uint8_t y, const char *s, uint8_t attr) {
    while (*s && x < 80) render_cell(x++, y, (uint8_t)*s++, attr);
}
