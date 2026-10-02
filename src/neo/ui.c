#include <string.h>
#include "api.h"
#include "dirio.h"
#include "ui.h"
#include "../field.h"
#include "../render.h"
#include "../util.h"

#define A_FIELD    0x70
#define A_FIELD_ON 0x4F

#define LIST_TOP  2
#define LIST_ROWS 20
#define MSG_ROW   23
#define HELP_ROW  24

static void status_row(uint8_t y, const char *left, const char *right) {
    uint8_t len = (uint8_t)strlen(right);
    render_fill(0, y, 79, y, A_TITLE);
    render_text(1, y, left, A_TITLE);
    render_text(len < 79 ? 79 - len : 0, y, right, A_TITLE);
}

void ui_status(const char *left, const char *right) {
    render_fill(0, 25, 79, 29, 0x00);
    status_row(26, left, right);
}

/* Redraws only the clock cells of the status bar (hms is 8 characters). */
void ui_status_clock(uint8_t x, const char *hms) {
    render_text(x, 26, hms, A_TITLE);
}

uint8_t ui_ask(const char *prompt) {
    ui_status(prompt, "");
    return util_lower(ui_wait_key());
}

bool ui_input(const char *prompt, char *buf, uint8_t cap, uint8_t kind) {
    field_t f;
    f.buf = buf;
    f.cap = cap;
    f.kind = kind;
    ui_status(prompt, "Enter OK   Esc Cancel ");
    for (;;) {
        uint8_t c;
        render_fill(1, 27, 1 + cap, 27, A_FIELD);
        render_text(1, 27, buf, A_FIELD);
        render_cursor(1 + (uint8_t)strlen(buf), 27, A_FIELD);
        c = ui_wait_key();
        if (c == 0x1B) return false;
        if (c == 0x0D && buf[0]) return true;
        field_key(&f, c);
    }
}

void ui_progress(const char *line1, const char *right1, const char *line2, const char *right2) {
    status_row(26, line1, right1);
    status_row(27, line2, right2);
}

void ui_note(const char *msg) {
    render_fill(0, 28, 79, 28, 0x00);
    render_text(1, 28, msg, A_ERR);
}

uint8_t ui_wait_key(void) {
    uint8_t c;
    while (!(c = api_read_key())) {
    }
    return c;
}

static void message(const char *m, uint8_t attr) {
    render_fill(0, MSG_ROW, 79, MSG_ROW, A_TEXT);
    render_text(1, MSG_ROW, m, attr);
}

void ui_frame(const char *t, const char *help) {
    render_fill(0, 0, 79, 24, A_TEXT);
    render_fill(0, 0, 79, 0, A_TITLE);
    render_text(1, 0, t, A_TITLE);
    render_text(1, HELP_ROW, help, A_HELP);
}

bool ui_scroll(uint8_t sel, uint8_t *top, uint8_t rows) {
    if (sel < *top) *top = sel;
    else if (sel >= *top + rows) *top = (uint8_t)(sel - rows + 1);
    else return false;
    return true;
}

static void format_entry(char *out, const dir_entry_t *e) {
    uint8_t i = 0;
    for (const char *s = e->name; *s; s++) out[i++] = *s;
    while (i < DIR_NAME_MAX + 2) out[i++] = ' ';
    for (const char *s = e->host; *s; s++) out[i++] = *s;
    out[i++] = ':';
    util_u16(out + i, e->port);
}

static void draw_row(const dir_t *d, uint8_t index, uint8_t top, uint8_t sel) {
    char line[81];
    uint8_t y, attr;
    if (index < top || index >= top + LIST_ROWS || index >= d->count) return;
    y = LIST_TOP + (index - top);
    attr = index == sel ? A_SEL : A_TEXT;
    format_entry(line, &d->e[index]);
    render_fill(0, y, 79, y, attr);
    render_text(1, y, line, attr);
}

static void draw_list(const dir_t *d, uint8_t sel, uint8_t top) {
    ui_frame("JumiTerm - BBS directory",
             "Enter Connect  A Add  E Edit  D Delete  C Quick connect  Q Quit");
    if (d->count == 0) render_text(2, LIST_TOP, "No entries. Press A to add one, or C to quick connect.", A_TEXT);
    for (uint8_t r = 0; r < LIST_ROWS; r++) draw_row(d, top + r, top, sel);
}

static void draw_field(const field_t *f, const char *label, uint8_t index, bool active) {
    uint8_t y = 4 + index * 2;
    uint8_t attr = active ? A_FIELD_ON : A_FIELD;
    render_text(4, y, label, A_TEXT);
    render_fill(12, y, 12 + f->cap, y, attr);
    render_text(12, y, f->buf, attr);
    if (active) render_cursor(12 + (uint8_t)strlen(f->buf), y, attr);
}

/* Edits e in place. Returns false if cancelled with Esc. */
static bool edit_form(dir_entry_t *e, const char *heading, bool with_name) {
    char port[6];
    field_t fields[3];
    const char *labels[3];
    uint8_t nf = 0, cur = 0;

    util_u16(port, e->port);
    if (with_name) {
        fields[nf].buf = e->name;
        fields[nf].cap = DIR_NAME_MAX;
        fields[nf].kind = FIELD_TEXT;
        labels[nf++] = "Name";
    }
    fields[nf].buf = e->host;
    fields[nf].cap = DIR_HOST_MAX;
    fields[nf].kind = FIELD_HOST;
    labels[nf++] = "Host";
    fields[nf].buf = port;
    fields[nf].cap = 5;
    fields[nf].kind = FIELD_DIGITS;
    labels[nf++] = "Port";

    ui_frame(heading, "Tab/Enter Next field   Enter on Port Save   Esc Cancel");
    for (uint8_t i = 0; i < nf; i++) draw_field(&fields[i], labels[i], i, i == cur);
    for (;;) {
        uint8_t c = ui_wait_key();
        if (c == 0x1B) return false;
        if (c == 0x09 || (c == 0x0D && cur + 1 < nf)) {
            draw_field(&fields[cur], labels[cur], cur, false);
            cur = (uint8_t)((cur + 1) % nf);
            draw_field(&fields[cur], labels[cur], cur, true);
            continue;
        }
        if (c == 0x0D) {
            if (!dir_parse_port(port, (uint16_t)strlen(port), &e->port)) e->port = 0;
            if (dir_entry_valid(e, with_name)) return true;
            message("Name and host are required; port must be 1-65535.", A_ERR);
            continue;
        }
        if (field_key(&fields[cur], c)) draw_field(&fields[cur], labels[cur], cur, true);
    }
}

static bool confirm_delete(const dir_entry_t *e) {
    char m[48];
    strcpy(m, "Delete ");
    strcat(m, e->name);
    strcat(m, "? (y/n)");
    message(m, A_ERR);
    return util_lower(ui_wait_key()) == 'y';
}

ui_action_t ui_directory(dir_t *d, dir_entry_t *chosen, const char *msg) {
    uint8_t sel = 0, top = 0;
    bool full = true;
    ui_status(" JumiTerm", "F1 = menu while connected ");
    for (;;) {
        uint8_t c, old;
        if (sel >= d->count) sel = d->count ? d->count - 1 : 0;
        if (ui_scroll(sel, &top, LIST_ROWS)) full = true;
        if (full) {
            draw_list(d, sel, top);
            full = false;
            if (d->bad_line) {
                char m[64];
                char *p;
                strcpy(m, "BBS.DIR line ");
                p = util_u16(m + strlen(m), d->bad_line);
                strcpy(p, " is malformed and was skipped");
                message(m, A_ERR);
                d->bad_line = 0;
            } else if (d->truncated) {
                message("BBS.DIR has more than 32 entries; the rest were ignored", A_ERR);
                d->truncated = false;
            } else if (msg) {
                message(msg, A_ERR);
            }
            msg = 0;
        }
        c = util_lower(ui_wait_key());
        old = sel;
        switch (c) {
        case 0x17:   /* Up (also Ctrl-W) */
            if (sel > 0) sel--;
            break;
        case 0x13:   /* Down (also Ctrl-S) */
            if (sel + 1 < d->count) sel++;
            break;
        case 0x0D:
            if (d->count) {
                *chosen = d->e[sel];
                return UI_CONNECT;
            }
            break;
        case 'a': {
            dir_entry_t e;
            memset(&e, 0, sizeof e);
            e.port = 23;
            if (edit_form(&e, "Add entry", true)) {
                if (!dir_add(d, &e)) msg = "The directory is full (32 entries)";
                else {
                    sel = d->count - 1;
                    if (!dirio_save(d)) msg = "Could not save BBS.DIR";
                }
            }
            full = true;
            break;
        }
        case 'e':
            if (d->count) {
                dir_entry_t e = d->e[sel];
                if (edit_form(&e, "Edit entry", true)) {
                    d->e[sel] = e;
                    if (!dirio_save(d)) msg = "Could not save BBS.DIR";
                }
                full = true;
            }
            break;
        case 'd':
            if (d->count && confirm_delete(&d->e[sel])) {
                dir_delete(d, sel);
                if (!dirio_save(d)) msg = "Could not save BBS.DIR";
            }
            full = true;
            break;
        case 'c': {
            dir_entry_t e;
            memset(&e, 0, sizeof e);
            e.port = 23;
            if (edit_form(&e, "Quick connect", false)) {
                strcpy(e.name, "Quick connect");
                *chosen = e;
                return UI_CONNECT;
            }
            full = true;
            break;
        }
        case 'q':
            return UI_QUIT;
        default:
            break;
        }
        if (!full && sel != old) {
            draw_row(d, old, top, sel);
            draw_row(d, sel, top, sel);
        }
    }
}
