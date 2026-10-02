#include <string.h>
#include "telnet.h"

enum { T_DATA, T_IAC, T_OPT, T_SB_OPT, T_SB_DATA, T_SB_IAC };

telnet_t telnet;

void telnet_init(void) {
    memset(&telnet, 0, sizeof telnet);
}

static void reply(const uint8_t *b, uint8_t n) {
    if (telnet.reply_len + n > TELNET_REPLY_MAX) return;
    memcpy(telnet.reply + telnet.reply_len, b, n);
    telnet.reply_len += n;
}

static void reply3(uint8_t cmd, uint8_t opt) {
    uint8_t b[3];
    b[0] = TN_IAC;
    b[1] = cmd;
    b[2] = opt;
    reply(b, 3);
}

/* Answer each option once per direction, so servers that repeat
   themselves cannot start a negotiation loop. */
static bool first_time(uint8_t which, uint8_t opt) {
    uint8_t mask = (uint8_t)(1 << (opt & 7));
    uint8_t *slot = &telnet.answered[which][opt >> 3];
    if (*slot & mask) return false;
    *slot |= mask;
    return true;
}

static void negotiate(uint8_t cmd, uint8_t opt) {
    static const uint8_t naws[] = { TN_IAC, TN_SB, TNOPT_NAWS, 0, 80, 0, 25, TN_IAC, TN_SE };
    if (cmd == TN_WILL) {
        if (opt == TNOPT_BINARY) telnet.rx_binary = true;
        if (!first_time(1, opt)) return;
        reply3((opt == TNOPT_ECHO || opt == TNOPT_SGA || opt == TNOPT_BINARY) ? TN_DO : TN_DONT, opt);
    } else if (cmd == TN_DO) {
        if (opt == TNOPT_BINARY) telnet.tx_binary = true;
        if (!first_time(0, opt)) return;
        if (opt == TNOPT_SGA || opt == TNOPT_TTYPE || opt == TNOPT_NAWS || opt == TNOPT_BINARY) {
            reply3(TN_WILL, opt);
            if (opt == TNOPT_NAWS) reply(naws, sizeof naws);
        } else {
            reply3(TN_WONT, opt);
        }
    } else if (opt == TNOPT_BINARY) {
        /* WONT/DONT BINARY: back to text mode in that direction, no answer needed. */
        if (cmd == TN_WONT) telnet.rx_binary = false;
        else telnet.tx_binary = false;
    }
    /* Other WONT and DONT need no answer: we never enable anything they could refuse. */
}

void telnet_request_binary(void) {
    if (!telnet.tx_binary && first_time(0, TNOPT_BINARY)) reply3(TN_WILL, TNOPT_BINARY);
    if (!telnet.rx_binary && first_time(1, TNOPT_BINARY)) reply3(TN_DO, TNOPT_BINARY);
}

static void subnegotiation_done(void) {
    static const uint8_t ttype[] = { TN_IAC, TN_SB, TNOPT_TTYPE, 0, 'A', 'N', 'S', 'I', TN_IAC, TN_SE };
    if (telnet.sb_opt == TNOPT_TTYPE && telnet.sb_len > 0 && telnet.sb_first == 1) {
        reply(ttype, sizeof ttype);
    }
}

static void sb_byte(uint8_t b) {
    if (telnet.sb_len == 0) telnet.sb_first = b;
    if (telnet.sb_len < 255) telnet.sb_len++;
}

uint16_t telnet_rx(uint8_t *buf, uint16_t n) {
    uint16_t out = 0;
    for (uint16_t i = 0; i < n; i++) {
        uint8_t b = buf[i];
        switch (telnet.state) {
        case T_DATA:
            if (b == TN_IAC) {
                telnet.state = T_IAC;
                break;
            }
            if (b == 0 && telnet.last_cr && !telnet.rx_binary) {
                telnet.last_cr = false;   /* text-mode CR NUL is a bare CR */
                break;
            }
            telnet.last_cr = b == '\r';
            buf[out++] = b;
            break;
        case T_IAC:
            if (b == TN_IAC) {
                buf[out++] = 0xFF;
                telnet.last_cr = false;
                telnet.state = T_DATA;
            } else if (b >= TN_WILL) {
                telnet.cmd = b;
                telnet.state = T_OPT;
            } else if (b == TN_SB) {
                telnet.state = T_SB_OPT;
            } else {
                telnet.state = T_DATA;   /* NOP, GA, AYT, ...: ignored */
            }
            break;
        case T_OPT:
            negotiate(telnet.cmd, b);
            telnet.state = T_DATA;
            break;
        case T_SB_OPT:
            telnet.sb_opt = b;
            telnet.sb_len = 0;
            telnet.state = T_SB_DATA;
            break;
        case T_SB_DATA:
            if (b == TN_IAC) telnet.state = T_SB_IAC;
            else sb_byte(b);
            break;
        case T_SB_IAC:
            if (b == TN_SE) {
                subnegotiation_done();
                telnet.state = T_DATA;
            } else {
                sb_byte(b);   /* IAC IAC inside SB is a data byte */
                telnet.state = T_SB_DATA;
            }
            break;
        }
    }
    return out;
}

uint8_t telnet_encode(uint8_t b, uint8_t *out) {
    out[0] = b;
    if (b == TN_IAC) {
        out[1] = TN_IAC;
        return 2;
    }
    if (b == '\r' && !telnet.tx_binary) {
        out[1] = 0;
        return 2;
    }
    return 1;
}
