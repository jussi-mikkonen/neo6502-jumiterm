/* src/neo/xferio.c */
#include <string.h>
#include <neo/file.h>
#include <neo/system.h>
#include "api.h"
#include "modem.h"
#include "ui.h"
#include "xferio.h"
#include "../config.h"
#include "../telnet.h"
#include "../util.h"

#define CHANNEL       1
#define TICKS_SETTLE  50    /* 0.5 s for the BINARY answers */
#define TICKS_REDRAW  25    /* progress at most 4 times a second */

static uint8_t chunk[POLL_MAX];
static uint16_t chunk_len, chunk_pos;
static const char *label;
static uint8_t nfiles;
static const char *const *upload_names;
static bool ask_overwrite, writing;
static char cur_name[FNAME_MAX + 1];
static uint8_t file_error;
static uint32_t start_ticks, last_draw;

/* Refills chunk with decoded data bytes, if any have arrived. */
static void refill(void) {
    uint16_t n = modem_poll(chunk, POLL_MAX);
    chunk_pos = 0;
    chunk_len = n ? telnet_rx(chunk, n) : 0;
    modem_flush_telnet();
}

static int16_t x_getc(uint16_t timeout) {
    uint32_t start = api_ticks();
    for (;;) {
        if (chunk_pos < chunk_len) return chunk[chunk_pos++];
        refill();
        if (chunk_pos < chunk_len) continue;
        if (api_ticks() - start >= timeout) return -1;
    }
}

static bool x_cancel(void) {
    return api_read_key() == 0x1B;
}

static bool file_ok(void) {
    uint8_t e = neo_api_error();
    if (e) file_error = e;
    return e == 0;
}

bool xferio_exists(const char *name) {
    neo_file_stat_t st;
    neo_file_stat(name, &st);
    return neo_api_error() == 0;
}

bool xferio_ask_overwrite(const char *name) {
    char m[FNAME_MAX + 32];
    strncpy(m, name, 40);             /* keep "y/n" on screen for long names */
    m[40] = 0;
    strcat(m, " exists - overwrite? y/n");
    return ui_ask(m) == 'y';
}

static xf_open_t x_open_write(const char *name, int32_t size) {
    (void)size;
    strcpy(cur_name, name);
    if (ask_overwrite && xferio_exists(name) && !xferio_ask_overwrite(name)) return OPEN_SKIP;
    neo_file_open(CHANNEL, name, NEO_FILE_CREATE);
    if (!file_ok()) return OPEN_FAIL;
    writing = true;
    return OPEN_OK;
}

static bool x_open_read(uint8_t index, char *name, uint32_t *size) {
    strcpy(cur_name, upload_names[index]);
    strcpy(name, cur_name);
    neo_file_open(CHANNEL, name, NEO_FILE_READ);
    if (!file_ok()) return false;
    writing = false;
    *size = neo_file_size(CHANNEL);
    return file_ok();
}

/* The firmware flags EOF as an error when a read returns nothing. */
static int16_t x_read(uint8_t *p, uint16_t n) {
    uint16_t got = neo_file_read(CHANNEL, p, n);
    if (neo_api_error() == NEO_FIOERROR_EOF) return 0;
    return file_ok() ? (int16_t)got : -1;
}

static bool x_write_file(const uint8_t *p, uint16_t n) {
    neo_file_write(CHANNEL, p, n);
    return file_ok();
}

/* An upload's source file is never deleted, whatever keep says. */
static void x_close(bool keep) {
    neo_file_close(CHANNEL);
    if (writing && !keep) neo_file_delete(cur_name);
    writing = false;
}

static void x_progress(uint32_t bytes, int32_t total, uint8_t errors, uint8_t file_no) {
    char l1[80], r1[12], l2[64], *p;   /* l2: "4194303.9 KB / 4194303.9 KB   100%   ..." fits */
    uint32_t now = api_ticks(), secs;
    if (now - last_draw < TICKS_REDRAW && total != (int32_t)bytes) return;
    last_draw = now;

    strcpy(l1, label);
    strcat(l1, "   ");
    strncat(l1, cur_name, 48);         /* leaves room for "1 of 3" */
    p = util_u16(r1, file_no);
    if (nfiles) {
        strcpy(p, " of ");
        p = util_u16(p + 4, nfiles);
    }
    strcpy(p, " ");

    p = util_kb(l2, bytes);
    strcpy(p, " KB");
    p += 3;
    if (total >= 0) {
        strcpy(p, " / ");
        p = util_kb(p + 3, (uint32_t)total);
        strcpy(p, " KB   ");
        p = util_u16(p + 6, util_percent(bytes, (uint32_t)total));
        *p++ = '%';
    }
    secs = (now - start_ticks) / 100;
    if (secs) {
        strcpy(p, "   ");
        p = util_kb(p + 3, bytes / secs);
        strcpy(p, " KB/s");
        p += 5;
    }
    strcpy(p, "   errors ");
    util_u16(p + 10, errors);
    ui_progress(l1, r1, l2, "Esc Cancel ");
}

const xfer_io_t xferio = {
    x_getc, modem_write, x_cancel, x_open_write, x_open_read, x_read, x_write_file, x_close, x_progress
};

void xferio_begin(const char *lbl, uint8_t n, const char *const *names, bool ask) {
    uint32_t t;
    label = lbl;
    nfiles = n;
    upload_names = names;
    ask_overwrite = ask;
    writing = false;
    file_error = 0;
    cur_name[0] = 0;
    chunk_len = chunk_pos = 0;
    telnet_request_binary();
    modem_flush_telnet();
    t = api_ticks();
    while (api_ticks() - t < TICKS_SETTLE && chunk_pos == chunk_len) refill();
    ui_progress(lbl, "", "Waiting for the other side...", "Esc Cancel ");
    if (!telnet.rx_binary || !telnet.tx_binary) ui_note("Telnet binary mode not confirmed - transfer may fail");
    start_ticks = last_draw = api_ticks();
}

uint8_t xferio_file_error(void) {
    return file_error;
}
