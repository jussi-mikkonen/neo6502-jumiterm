#include <neo/console.h>
#include <neo/system.h>
#include "api.h"
#include "dirio.h"
#include "modem.h"
#include "transfer.h"
#include "ui.h"
#include "../ansi.h"
#include "../config.h"
#include "../grid.h"
#include "../keymap.h"
#include "../render.h"
#include "../telnet.h"
#include "../util.h"

typedef enum { END_DIRECTORY, END_QUIT } session_end_t;

static dir_t dir;
static uint8_t rxbuf[POLL_MAX];

static void flush_replies(void) {
    modem_flush_telnet();
    if (ansi.reply_len) {
        modem_write(ansi.reply, ansi.reply_len);
        ansi.reply_len = 0;
    }
}

#define CLOCK_X (79 - 19)   /* right-aligned "hh:mm:ss   F1 Menu " */

/* Draws the whole session status bar; the clock is updated by session_clock. */
static void session_status(const dir_entry_t *e) {
    ui_status(e->name, "00:00:00   F1 Menu ");
}

static void session_clock(uint32_t seconds) {
    char t[12];
    util_hms(t, seconds);
    ui_status_clock(CLOCK_X, t);
}

/* Sends a typed key to the host. Arrow keys share codes with Ctrl letters and
   are told apart by whether the nav key is still held, so this must run soon
   after the key is read. */
static void handle_key(uint8_t c) {
    uint8_t out[4];
    uint8_t hid = keymap_nav_hid(c);
    uint8_t k = keymap_translate(c, hid && api_key_down(hid), out);
    modem_write(out, k);
}

static bool menu_pending;   /* F1 was read during a long output batch */

/* One receive step: modem -> telnet -> ANSI -> screen. With keys set, the
   keyboard is polled every 32 bytes so a quick arrow tap is still held when
   read; F1 is remembered in menu_pending for the caller. */
static void pump(bool keys) {
    uint16_t n = modem_poll(rxbuf, POLL_MAX);
    if (!n) return;
    grid_refresh_cell(grid.cx, grid.cy);   /* hide the cursor */
    n = telnet_rx(rxbuf, n);
    for (uint16_t i = 0; i < n; i++) {
        ansi_feed(rxbuf[i]);
        if (keys && (i & 31) == 31) {
            uint8_t c = api_read_key();
            if (c == KEY_LOCAL_MENU) menu_pending = true;
            else if (c) handle_key(c);
        }
    }
    flush_replies();
    render_cursor(grid.cx, grid.cy, grid.attr[grid.cy][grid.cx]);
}

/* Returns 'h' (hang up), 'q' (hang up and quit), 'd' (download), 'u'
   (upload) or 'b' (back). The terminal (rows 0-24) keeps receiving; the
   menu only occupies the status rows. */
static uint8_t local_menu(void) {
    ui_status(" H Hang up   D Download   U Upload   Q Hang up and quit   B Back", "");
    for (;;) {
        uint8_t c;
        pump(false);
        c = util_lower(api_read_key());
        if (c == 'h' || c == 'q' || c == 'd' || c == 'u') return c;
        if (c == 'b' || c == 0x1B || c == KEY_LOCAL_MENU) return 'b';
    }
}

static session_end_t session(const dir_entry_t *e) {
    uint32_t start = api_ticks(), shown = 0xFFFFFFFFUL;
    ansi_init();
    grid_reset();
    telnet_init();
    render_cursor(0, 0, grid.cur_attr);
    session_status(e);
    for (;;) {
        uint8_t c;
        uint32_t secs;
        pump(true);
        c = menu_pending ? KEY_LOCAL_MENU : api_read_key();
        menu_pending = false;
        if (c == KEY_LOCAL_MENU) {
            uint8_t m = local_menu();
            if (m == 'd') transfer_download();
            else if (m == 'u') transfer_upload();
            else if (m != 'b') {
                modem_hangup();
                return m == 'q' ? END_QUIT : END_DIRECTORY;
            }
            render_cursor(grid.cx, grid.cy, grid.attr[grid.cy][grid.cx]);
            session_status(e);      /* the menu replaced the status bar */
            shown = 0xFFFFFFFFUL;
        } else if (c) {
            handle_key(c);
        }
        secs = (api_ticks() - start) / 100;
        if (secs != shown) {
            shown = secs;
            session_clock(secs);
        }
    }
}

static const char *modem_message(modem_status_t s) {
    switch (s) {
    case MODEM_NO_RESPONSE: return "No response from the modem - check the ESP8266 on UEXT";
    case MODEM_NO_WIFI: return "Wi-Fi not connected - configure the ESP first";
    default: return 0;
    }
}

int main(void) {
    const char *msg;
    modem_status_t ms;

    render_init();
    neo_console_define_hotkey(1, "\201");   /* F1 sends KEY_LOCAL_MENU (0x81) */
    ui_status(" JumiTerm", "Starting modem... ");
    ms = modem_init();
    dirio_load(&dir);
    msg = modem_message(ms);
    for (;;) {
        dir_entry_t e;
        if (ui_directory(&dir, &e, msg) == UI_QUIT) break;
        msg = 0;
        if (ms != MODEM_OK) {
            ui_status(" JumiTerm", "Retrying modem... ");
            ms = modem_init();
            if (ms != MODEM_OK) {
                msg = modem_message(ms);
                continue;
            }
        }
        ui_status(e.name, "Connecting... ");
        if (modem_connect(e.host, e.port) != MODEM_OK) {
            msg = "Connect failed (DNS, refused or timed out)";
            continue;
        }
        if (session(&e) == END_QUIT) break;
    }
    modem_shutdown();
    neo_console_define_hotkey(1, "");
    neo_system_reset();   /* a reset returns to BASIC and does not come back */
    /* Only reached where reset is a no-op (the emulator): show why we stop. */
    render_shutdown();
    for (const char *p = "JumiTerm ended - press reset."; *p; p++) neo_console_write_char(*p);
    neo_console_write_char(13);
    for (;;) {
    }
}
