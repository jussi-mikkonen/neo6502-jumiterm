#include <stdbool.h>
#include <string.h>
#include "api.h"
#include "modem.h"
#include "../atparse.h"
#include "../config.h"
#include "../dirfile.h"
#include "../telnet.h"
#include "../util.h"

#define TICKS_PROBE    50     /* 0.5 s */
#define TICKS_CMD      200    /* 2 s */
#define TICKS_CONNECT  1500   /* 15 s: DNS lookup + TCP connect */
#define TICKS_GUARD    110    /* > 1 s of silence around +++ */

#define STR2(x) #x
#define STR(x) STR2(x)

static uint32_t last_tx;
static int8_t wifi;
static uint32_t cur_rate = 115200UL;   /* rate that last answered */

static void put_str(const char *s) {
    while (*s) api_uart_putc((uint8_t)*s++);
    last_tx = api_ticks();
}

static void discard_input(void) {
    while (api_uart_getc() >= 0) {
    }
}

static void pause_discarding(uint16_t ticks) {
    uint32_t start = api_ticks();
    while (api_ticks() - start < ticks) discard_input();
}

/* Reads until OK, ERROR, the '>' prompt or timeout (AT_MORE). */
static at_result_t wait_result(uint16_t timeout) {
    uint32_t start = api_ticks();
    while (api_ticks() - start < timeout) {
        int16_t b = api_uart_getc();
        at_result_t r;
        if (b < 0) continue;
        r = at_feed((uint8_t)b);
        if (r == AT_LINE) {
            int8_t w = at_wifi_status(at.line);
            if (w >= 0) wifi = w;
        } else if (r != AT_MORE) {
            return r;
        }
    }
    return AT_MORE;
}

static at_result_t cmd(const char *s, uint16_t timeout) {
    at_reset();
    put_str(s);
    put_str("\r\n");
    return wait_result(timeout);
}

static bool probe(void) {
    for (uint8_t i = 0; i < 3; i++) {
        discard_input();
        if (cmd("AT", TICKS_PROBE) == AT_OK) return true;
    }
    return false;
}

/* Leaves transparent mode: "+++" with a second of silence on each side. */
static void escape(void) {
    while (api_ticks() - last_tx < TICKS_GUARD) discard_input();
    put_str("+++");
    pause_discarding(TICKS_GUARD);
}

static bool find_modem(void) {
    uint32_t rates[2];
    rates[0] = cur_rate;
    rates[1] = cur_rate == 115200UL ? (uint32_t)UART_RATE : 115200UL;
    for (uint8_t i = 0; i < 2; i++) {
        api_uart_set_baud(rates[i]);
        if (!probe()) {
            escape();                /* an earlier run may have left transparent mode on */
            if (!probe()) continue;
        }
        cur_rate = rates[i];
        return true;
    }
    return false;
}

modem_status_t modem_init(void) {
    if (!find_modem()) return MODEM_NO_RESPONSE;
    cmd("AT+CIPCLOSE", TICKS_CMD);   /* drop a link left open by an earlier run */
    cmd("ATE0", TICKS_CMD);
    cmd("AT+CIPMODE=0", TICKS_CMD);
    cmd("AT+CIPMUX=0", TICKS_CMD);
    if (cmd("AT+UART_CUR=" STR(UART_RATE) ",8,1,0,0", TICKS_CMD) == AT_OK) {
        api_uart_set_baud(UART_RATE);
        cur_rate = UART_RATE;
        pause_discarding(10);
        if (!probe()) return MODEM_NO_RESPONSE;
    }
    wifi = 0;
    if (cmd("AT+CWSTATE?", TICKS_CMD) != AT_OK) cmd("AT+CIPSTATUS", TICKS_CMD);
    return wifi ? MODEM_OK : MODEM_NO_WIFI;
}

modem_status_t modem_connect(const char *host, uint16_t port) {
    char line[20 + DIR_HOST_MAX + 8];
    discard_input();
    cmd("AT+CIPCLOSE", TICKS_CMD);   /* clear a stale link: ignore the result */
    if (cmd("AT+CIPMODE=1", TICKS_CMD) != AT_OK) return MODEM_CONNECT_FAILED;
    strcpy(line, "AT+CIPSTART=\"TCP\",\"");
    strcat(line, host);
    strcat(line, "\",");
    util_u16(line + strlen(line), port);
    if (cmd(line, TICKS_CONNECT) != AT_OK) {
        cmd("AT+CIPMODE=0", TICKS_CMD);
        return MODEM_CONNECT_FAILED;
    }
    if (cmd("AT+CIPSEND", TICKS_CMD) != AT_OK || wait_result(TICKS_CMD) != AT_PROMPT) {
        modem_hangup();
        return MODEM_CONNECT_FAILED;
    }
    return MODEM_OK;   /* everything after '>' is BBS data, left for modem_poll */
}

uint16_t modem_poll(uint8_t *buf, uint16_t max) {
    uint16_t n = 0;
    int16_t b;
    while (n < max && (b = api_uart_getc()) >= 0) buf[n++] = (uint8_t)b;
    return n;
}

static void write_raw(const uint8_t *buf, uint8_t n) {
    while (n--) api_uart_putc(*buf++);
    last_tx = api_ticks();
}

void modem_write(const uint8_t *p, uint16_t n) {
    uint8_t out[64];
    uint8_t k = 0;
    while (n--) {
        k += telnet_encode(*p++, out + k);
        if (k >= sizeof out - 2) {
            write_raw(out, k);
            k = 0;
        }
    }
    if (k) write_raw(out, k);
}

void modem_flush_telnet(void) {
    if (telnet.reply_len) {
        write_raw(telnet.reply, telnet.reply_len);   /* already telnet-encoded */
        telnet.reply_len = 0;
    }
}

void modem_hangup(void) {
    escape();
    put_str("\r\n");   /* ends a stray "+++" line if the ESP was in command mode */
    pause_discarding(20);
    cmd("AT+CIPCLOSE", TICKS_CMD);
    cmd("AT+CIPMODE=0", TICKS_CMD);
}

/* AT+UART_CUR survives a Neo reset: leave the ESP at its default rate so
   other tools can talk to it. */
void modem_shutdown(void) {
    if (cur_rate == 115200UL) return;
    cmd("AT+UART_CUR=115200,8,1,0,0", TICKS_CMD);   /* result ignored */
    api_uart_set_baud(115200UL);
    cur_rate = 115200UL;
}
