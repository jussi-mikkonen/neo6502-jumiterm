/* src/neo/picker.c */
#include <string.h>
#include <neo/file.h>
#include <neo/system.h>
#include <neo/types.h>
#include "picker.h"
#include "ui.h"
#include "../fname.h"
#include "../render.h"
#include "../util.h"

#define LIST_TOP  1
#define LIST_ROWS 22    /* rows 1-22; row 23 is for messages, row 24 for help */
#define NAME_COLS 60
#define POOL_SIZE 2048  /* names are packed: most are far shorter than FNAME_MAX */

typedef struct {
    uint16_t name;      /* offset of the NUL-terminated name in pool */
    uint32_t size;
    bool dir, mark;
} entry_t;

static entry_t ent[PICKER_MAX];
static char pool[POOL_SIZE];
static uint16_t pool_used;
static uint8_t order[PICKER_MAX];    /* display order: "..", directories, then files */
static uint8_t count;
static uint8_t depth;                /* directories entered below the starting one */
static bool more;                    /* the directory had more than PICKER_MAX entries */
static uint8_t raw[256];             /* length-prefixed name from the firmware */

static bool add_name(entry_t *e, const char *s, uint8_t len) {
    if (pool_used + len + 1 > POOL_SIZE) return false;
    e->name = pool_used;
    memcpy(pool + pool_used, s, len);
    pool[pool_used + len] = 0;
    pool_used += len + 1;
    return true;
}

static bool before(const entry_t *a, const entry_t *b) {
    const uint8_t *p = (const uint8_t *)pool + a->name, *q = (const uint8_t *)pool + b->name;
    if (a->dir != b->dir) return a->dir;
    while (*p && util_lower(*p) == util_lower(*q)) {
        p++;
        q++;
    }
    return util_lower(*p) < util_lower(*q);
}

static void load(void) {
    neo_file_stat_t st;
    uint8_t first;
    count = 0;
    pool_used = 0;
    more = false;
    if (depth) {                     /* never above the starting directory */
        memset(&ent[0], 0, sizeof ent[0]);
        add_name(&ent[0], "..", 2);
        ent[0].dir = true;
        order[0] = 0;
        count = 1;
    }
    first = count;
    neo_file_opendir(".");
    if (neo_api_error()) return;
    for (;;) {
        entry_t *e;
        raw[0] = sizeof raw - 1;     /* on input the length byte is the buffer's capacity */
        neo_file_readdir_p((neo_pstring_t *)raw, &st);
        if (neo_api_error()) break;
        if (raw[0] == 0 || raw[0] > FNAME_MAX || raw[1] == '.') continue;   /* too long, hidden, . and .. */
        e = &ent[count];
        if (count == PICKER_MAX || !add_name(e, (const char *)raw + 1, raw[0])) {
            more = true;
            break;
        }
        e->size = st.size;
        e->dir = (st.attr & NEO_FIOATTR_DIR) != 0;
        e->mark = false;
        order[count] = count;
        count++;
    }
    neo_file_closedir();
    for (uint8_t i = first + 1; i < count; i++) {   /* insertion sort, ".." stays first */
        uint8_t v = order[i], j = i;
        while (j > first && before(&ent[v], &ent[order[j - 1]])) {
            order[j] = order[j - 1];
            j--;
        }
        order[j] = v;
    }
}

static void draw_row(uint8_t i, uint8_t top, uint8_t sel) {
    char text[NAME_COLS + 1], size[11];
    const entry_t *e;
    uint8_t y, attr;
    if (i < top || i >= top + LIST_ROWS || i >= count) return;
    e = &ent[order[i]];
    y = LIST_TOP + (i - top);
    attr = i == sel ? A_SEL : A_TEXT;
    render_fill(0, y, 79, y, attr);
    if (e->mark) render_text(1, y, "*", attr);
    strncpy(text, pool + e->name, NAME_COLS);
    text[NAME_COLS] = 0;
    render_text(3, y, text, attr);
    if (e->dir) {
        render_text(74, y, "<DIR>", attr);
    } else {
        util_u32(size, e->size);
        render_text((uint8_t)(79 - strlen(size)), y, size, attr);
    }
}

static void draw_all(bool multi, uint8_t top, uint8_t sel) {
    ui_frame(multi ? "Upload - choose files (Space marks)" : "Upload - choose a file",
             multi ? "Enter Send/Open  Space Mark  Backspace Up a level  Esc Cancel"
                   : "Enter Send/Open  Backspace Up a level  Esc Cancel");
    for (uint8_t r = 0; r < LIST_ROWS; r++) draw_row(top + r, top, sel);
    if (count == 0) render_text(3, LIST_TOP, "No files here.", A_TEXT);
    if (more) render_text(1, 23, "(more not shown)", A_ERR);
}

static bool enter_dir(const char *name) {
    bool up = strcmp(name, "..") == 0;
    if (up && depth == 0) return false;
    neo_file_chdir(name);
    if (neo_api_error()) return false;
    depth = up ? depth - 1 : depth + 1;
    load();
    return true;
}

void picker_restore(void) {
    for (; depth; depth--) neo_file_chdir("..");
}

uint8_t picker_run(bool multi, const char **chosen) {
    uint8_t sel = 0, top = 0;
    bool full = true;
    depth = 0;
    load();
    for (;;) {
        uint8_t c, old;
        entry_t *e;
        if (ui_scroll(sel, &top, LIST_ROWS)) full = true;
        if (full) {
            draw_all(multi, top, sel);
            full = false;
        }
        c = ui_wait_key();
        old = sel;
        if (count == 0 && c != 0x1B && c != 0x08) continue;
        e = &ent[order[sel]];
        switch (c) {
        case 0x17:   /* Up */
            if (sel > 0) sel--;
            break;
        case 0x13:   /* Down */
            if (sel + 1 < count) sel++;
            break;
        case 0x12:   /* Page Up */
            sel = sel > LIST_ROWS ? (uint8_t)(sel - LIST_ROWS) : 0;
            break;
        case 0x06:   /* Page Down */
            sel = (uint8_t)(sel + LIST_ROWS < count ? sel + LIST_ROWS : count - 1);
            break;
        case ' ':
            if (multi && !e->dir) {
                e->mark = !e->mark;
                draw_row(sel, top, sel);
                if (sel + 1 < count) sel++;
            }
            break;
        case 0x08:   /* Backspace: up a level */
            if (enter_dir("..")) {
                sel = top = 0;
                full = true;
            }
            break;
        case 0x0D: {
            uint8_t n = 0;
            if (e->dir) {
                if (enter_dir(pool + e->name)) {
                    sel = top = 0;
                    full = true;
                }
                break;
            }
            for (uint8_t i = 0; i < count; i++)
                if (ent[order[i]].mark) chosen[n++] = pool + ent[order[i]].name;
            if (n == 0) chosen[n++] = pool + e->name;
            return n;
        }
        case 0x1B:
            picker_restore();
            return 0;
        default:
            break;
        }
        if (!full && sel != old) {
            draw_row(old, top, sel);
            draw_row(sel, top, sel);
        }
    }
}
