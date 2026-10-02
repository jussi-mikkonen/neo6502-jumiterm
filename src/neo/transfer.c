/* src/neo/transfer.c */
#include <string.h>
#include "picker.h"
#include "transfer.h"
#include "ui.h"
#include "xferio.h"
#include "../field.h"
#include "../fname.h"
#include "../grid.h"
#include "../util.h"

/* Indexed by xf_result_t. */
static const char *const result_text[] = {
    "Done: ", "Cancelled", "Cancelled by remote", "Failed: timeout",
    "Failed: too many errors", "Failed: file error "
};

static void show_result(xf_result_t r) {
    char m[48], *p;
    strcpy(m, result_text[r]);
    p = m + strlen(m);
    if (r == XF_OK) {
        p = util_u16(p, xf_stats.files);
        strcpy(p, xf_stats.files == 1 ? " file, " : " files, ");
        p = util_kb(p + strlen(p), xf_stats.bytes);
        strcpy(p, " KB");
    } else if (r == XF_FILE_ERROR) {
        util_u16(p, xferio_file_error());
    }
    ui_status(m, "Press a key ");
    ui_wait_key();
}

/* XMODEM sends no name: asks for one. False when cancelled. */
static bool ask_name(char *name) {
    for (;;) {
        char clean[FNAME_MAX + 1];
        if (!ui_input(" Save as:", name, FNAME_MAX, FIELD_FILENAME)) return false;
        fname_sanitize(name, clean);
        strcpy(name, clean);
        if (!xferio_exists(name) || xferio_ask_overwrite(name)) return true;
    }
}

void transfer_download(void) {
    static char name[FNAME_MAX + 1];
    xf_result_t r;
    uint8_t k = ui_ask(" Download:  X XMODEM   Y YMODEM   Esc Cancel");
    if (k == 'y') {
        xferio_begin("Download YMODEM", 0, NULL, true);
        r = xf_receive(&xferio, XP_YMODEM, NULL);
    } else if (k == 'x') {
        name[0] = 0;                  /* a fresh name each time */
        if (!ask_name(name)) return;
        xferio_begin("Download XMODEM", 0, NULL, false);
        r = xf_receive(&xferio, XP_XMODEM, name);
    } else {
        return;
    }
    show_result(r);
}

/* Both indexed by xf_proto_t. */
static const char upload_keys[] = "xky";
static const char *const upload_label[] = { "Upload XMODEM", "Upload XMODEM-1K", "Upload YMODEM" };

void transfer_upload(void) {
    static const char *chosen[PICKER_MAX];
    const char *hit;
    xf_proto_t p;
    uint8_t n, k = ui_ask(" Upload:  X XMODEM   K XMODEM-1K   Y YMODEM   Esc Cancel");
    hit = k ? strchr(upload_keys, k) : NULL;
    if (!hit) return;
    p = (xf_proto_t)(hit - upload_keys);
    n = picker_run(p == XP_YMODEM, chosen);
    grid_redraw();                    /* the picker covered the terminal */
    if (!n) return;
    xferio_begin(upload_label[p], n, chosen, false);
    show_result(xf_send(&xferio, p, n));
    picker_restore();
}
