/* Telnet (RFC 854) receive filter and option negotiation. */
#ifndef TELNET_H
#define TELNET_H

#include <stdbool.h>
#include <stdint.h>

#define TN_SE   240
#define TN_SB   250
#define TN_WILL 251
#define TN_WONT 252
#define TN_DO   253
#define TN_DONT 254
#define TN_IAC  255

#define TNOPT_BINARY 0
#define TNOPT_ECHO  1
#define TNOPT_SGA   3
#define TNOPT_TTYPE 24
#define TNOPT_NAWS  31

#define TELNET_REPLY_MAX 64

typedef struct {
    uint8_t state, cmd, sb_opt, sb_first, sb_len;
    bool rx_binary, tx_binary;          /* BINARY (RFC 856) agreed for data from / to the server */
    bool last_cr;                       /* previous data byte was CR (for CR NUL in text mode) */
    uint8_t answered[2][32];            /* [0] DO answered, [1] WILL answered; one bit per option */
    uint8_t reply[TELNET_REPLY_MAX];    /* negotiation bytes to send, already encoded */
    uint8_t reply_len;
} telnet_t;

extern telnet_t telnet;

void telnet_init(void);
uint16_t telnet_rx(uint8_t *buf, uint16_t n);
uint8_t telnet_encode(uint8_t b, uint8_t *out);
/* Asks the server for BINARY in each direction that does not have it yet. */
void telnet_request_binary(void);

#endif
