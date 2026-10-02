#include "ansi.h"
#include "grid.h"
#include "util.h"

enum { S_GROUND, S_ESC, S_ESC_SKIP, S_CSI };

ansi_t ansi;

static void update_attr(void) {
    uint8_t fg = ansi.fg | (ansi.bold ? 8 : 0);
    uint8_t bg = ansi.bg | (ansi.blink ? 8 : 0);
    if (ansi.reverse) {
        uint8_t t = fg;
        fg = bg;
        bg = t;
    }
    grid.cur_attr = (uint8_t)((bg << 4) | fg);
}

static void sgr_reset(void) {
    ansi.fg = 7;
    ansi.bg = 0;
    ansi.bold = false;
    ansi.blink = false;
    ansi.reverse = false;
}

void ansi_init(void) {
    ansi.state = S_GROUND;
    ansi.cr_pending = false;
    ansi.reply_len = 0;
    sgr_reset();
    update_attr();
}

static uint16_t param(uint8_t i, uint16_t def) {
    uint16_t v = (i <= ansi.nparams) ? ansi.params[i] : 0;
    return v ? v : def;
}

static uint8_t clamp8(uint16_t v) {
    return v > 255 ? 255 : (uint8_t)v;
}

static void reply_str(const char *s) {
    while (*s && ansi.reply_len < ANSI_REPLY_MAX) ansi.reply[ansi.reply_len++] = (uint8_t)*s++;
}

static void report_cursor(void) {
    char buf[16];
    char *p = buf;
    *p++ = '\033';
    *p++ = '[';
    p = util_u16(p, grid.cy + 1);
    *p++ = ';';
    p = util_u16(p, grid.cx + 1);
    *p++ = 'R';
    *p = 0;
    reply_str(buf);
}

static void sgr(void) {
    for (uint8_t i = 0; i <= ansi.nparams; i++) {
        uint16_t v = ansi.params[i];
        if (v == 0) sgr_reset();
        else if (v == 1) ansi.bold = true;
        else if (v == 5 || v == 6) ansi.blink = true;
        else if (v == 7) ansi.reverse = true;
        else if (v == 22) ansi.bold = false;
        else if (v == 25) ansi.blink = false;
        else if (v == 27) ansi.reverse = false;
        else if (v >= 30 && v <= 37) ansi.fg = (uint8_t)(v - 30);
        else if (v == 39) ansi.fg = 7;
        else if (v >= 40 && v <= 47) ansi.bg = (uint8_t)(v - 40);
        else if (v == 49) ansi.bg = 0;
        else if (v >= 90 && v <= 97) ansi.fg = (uint8_t)(v - 90 + 8);
        else if (v >= 100 && v <= 107) ansi.bg = (uint8_t)(v - 100 + 8);
        else if (v == 38 || v == 48) {   /* 256-colour / true colour: skip their arguments */
            if (i < ansi.nparams && ansi.params[i + 1] == 5) i += 2;
            else if (i < ansi.nparams && ansi.params[i + 1] == 2) i += 4;
        }
    }
    update_attr();
}

static void csi_dispatch(uint8_t final) {
    uint8_t n = clamp8(param(0, 1));
    if (ansi.private_marker) return;
    switch (final) {
    case 'A': grid_goto(grid.cx, grid.cy > n ? grid.cy - n : 0); break;
    case 'B': grid_goto(grid.cx, clamp8(grid.cy + n)); break;
    case 'C': grid_goto(clamp8(grid.cx + n), grid.cy); break;
    case 'D': grid_goto(grid.cx > n ? grid.cx - n : 0, grid.cy); break;
    case 'E': grid_goto(0, clamp8(grid.cy + n)); break;
    case 'F': grid_goto(0, grid.cy > n ? grid.cy - n : 0); break;
    case 'G': grid_goto(n - 1, grid.cy); break;
    case 'd': grid_goto(grid.cx, n - 1); break;
    case 'H':
    case 'f': grid_goto(clamp8(param(1, 1)) - 1, n - 1); break;
    case 'J': if (param(0, 0) <= 2) grid_erase_display((uint8_t)param(0, 0)); break;
    case 'K': if (param(0, 0) <= 2) grid_erase_line((uint8_t)param(0, 0)); break;
    case 'L': grid_insert_lines(n); break;
    case 'M': grid_delete_lines(n); break;
    case '@': grid_insert_chars(n); break;
    case 'P': grid_delete_chars(n); break;
    case 'X': grid_erase_chars(n); break;
    case 'm': sgr(); break;
    case 'r': grid_set_region(clamp8(param(0, 1)) - 1, clamp8(param(1, GRID_ROWS)) - 1); break;
    case 's': grid_save_cursor(); break;
    case 'u': grid_restore_cursor(); break;
    case 'n':
        if (param(0, 0) == 6) report_cursor();
        else if (param(0, 0) == 5) reply_str("\033[0n");
        break;
    case 'c': if (param(0, 0) == 0) reply_str("\033[?1;0c"); break;
    default: break;
    }
}

static void control(uint8_t b) {
    switch (b) {
    case '\r':
        grid_cr();
        if (ansi.state == S_GROUND) ansi.cr_pending = true;
        break;
    case '\n':
    case 0x0B: grid_lf(); break;
    case 0x0C: grid_erase_display(2); break;
    case 0x08: grid_bs(); break;
    case 0x09: grid_tab(); break;
    default: break;   /* BEL, NUL, SO, SI and the rest are ignored */
    }
}

void ansi_feed(uint8_t b) {
    /* Telnet wants CR followed by LF or NUL; some BBSes send a bare CR.
     * Treat CR followed by printable text as a newline. */
    if (ansi.cr_pending) {
        ansi.cr_pending = false;
        if (ansi.state == S_GROUND && b >= 0x20 && b != 0x7F) grid_lf();
    }
    switch (ansi.state) {
    case S_GROUND:
        if (b == 0x1B) ansi.state = S_ESC;
        else if (b < 0x20) control(b);
        else if (b != 0x7F) grid_put(b);
        break;
    case S_ESC:
        ansi.state = S_GROUND;
        switch (b) {
        case '[':
            ansi.state = S_CSI;
            ansi.nparams = 0;
            ansi.params[0] = 0;
            ansi.private_marker = false;
            break;
        case '7': grid_save_cursor(); break;
        case '8': grid_restore_cursor(); break;
        case 'D': grid_lf(); break;
        case 'E': grid_cr(); grid_lf(); break;
        case 'M': grid_ri(); break;
        case 'c': ansi_init(); grid_reset(); break;
        case '(': case ')': case '#': case '%': ansi.state = S_ESC_SKIP; break;
        case 0x1B: ansi.state = S_ESC; break;
        default: break;
        }
        break;
    case S_ESC_SKIP:
        ansi.state = S_GROUND;
        break;
    case S_CSI:
        if (b >= '0' && b <= '9') {
            uint16_t *p = &ansi.params[ansi.nparams];
            if (*p < 1000) *p = (uint16_t)(*p * 10 + (b - '0'));
        } else if (b == ';') {
            if (ansi.nparams < ANSI_MAX_PARAMS - 1) ansi.nparams++;
            ansi.params[ansi.nparams] = 0;   /* extra parameters reuse the last slot */
        } else if (b >= 0x3C && b <= 0x3F) {
            ansi.private_marker = true;
        } else if (b >= 0x40 && b <= 0x7E) {
            ansi.state = S_GROUND;
            csi_dispatch(b);
        } else if (b == 0x1B) {
            ansi.state = S_ESC;
        } else if (b < 0x20) {
            control(b);
        }
        break;
    }
}
