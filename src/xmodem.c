/* src/xmodem.c */
#include <string.h>
#include "util.h"
#include "xmodem.h"

#define SOH 0x01
#define STX 0x02
#define EOT 0x04
#define ACK 0x06
#define BS  0x08
#define NAK 0x15
#define CAN 0x18
#define SUB 0x1A

#define T_BYTE       100    /* 1 s: inside a block, and line drain */
#define T_START      300    /* 3 s between start characters */
#define T_BLOCK      1000   /* 10 s for a block or an answer */
#define T_SEND_START 6000   /* 60 s for the receiver to start */
#define MAX_ERRORS   10
#define START_TRIES  3      /* per start character */

#define W_CANCEL (-2)       /* wait_byte: the user pressed Esc */

xf_stats_t xf_stats;

static const xfer_io_t *io;
static uint8_t blk[1024];
static uint16_t crc_table[256];
static bool crc_mode;
static bool file_open;          /* a file from open_write is open */
static uint8_t rx_num;          /* block number of the last packet read */
static uint16_t rx_len;         /* its data length */
static uint8_t total_errors, file_no;
static uint32_t bytes;
static int32_t total;

static void crc_init(void) {
    for (uint16_t i = 0; i < 256; i++) {
        uint16_t c = (uint16_t)(i << 8);
        for (uint8_t b = 0; b < 8; b++) c = (c & 0x8000) ? (uint16_t)((c << 1) ^ 0x1021) : (uint16_t)(c << 1);
        crc_table[i] = c;
    }
}

uint16_t xf_crc16(const uint8_t *p, uint16_t n) {
    uint16_t crc = 0;
    if (!crc_table[1]) crc_init();
    while (n--) crc = (uint16_t)((crc << 8) ^ crc_table[(uint8_t)(crc >> 8) ^ *p++]);
    return crc;
}

static uint8_t checksum(const uint8_t *p, uint16_t n) {
    uint8_t s = 0;
    while (n--) s = (uint8_t)(s + *p++);
    return s;
}

static void put1(uint8_t b) {
    io->write(&b, 1);
}

/* Waits up to ticks for a byte, checking for Esc once a second. */
static int16_t wait_byte(uint16_t ticks) {
    for (;;) {
        uint16_t step = ticks > T_BYTE ? T_BYTE : ticks;
        int16_t c = io->getc(step);
        if (c >= 0) return c;
        if (io->cancel()) return W_CANCEL;
        ticks -= step;
        if (!ticks) return -1;
    }
}

/* Reads and drops bytes until the line has been quiet for a second. */
static void drain(void) {
    while (io->getc(T_BYTE) >= 0) {
    }
}

/* True for CAN CAN: the other side cancelled. c is the byte just read. */
static bool can2(int16_t c) {
    return c == CAN && io->getc(T_BYTE) == CAN;
}

static void report(void) {
    io->progress(bytes, total, total_errors, file_no);
}

/* Ends the transfer: tells the other side to stop (unless it stopped
   first) and removes a partial download. */
static xf_result_t stop(xf_result_t r) {
    static const uint8_t cancel[] = { CAN, CAN, CAN, BS, BS, BS };
    if (r != XF_REMOTE_CANCEL) io->write(cancel, sizeof cancel);
    if (file_open) {
        io->close(false);
        file_open = false;
    }
    return r;
}

/* ---- receive ---- */

enum { RP_OK, RP_BAD, RP_EOT, RP_CAN, RP_USER };   /* RP_USER: Esc while waiting */

/* Reads the rest of a packet whose first byte is c. */
static uint8_t rx_packet(int16_t c) {
    uint8_t n, nc;
    if (c == EOT) return RP_EOT;
    if (c == CAN) return can2(c) ? RP_CAN : RP_BAD;
    if (c != SOH && c != STX) return RP_BAD;
    rx_len = c == SOH ? 128 : 1024;
    if ((c = io->getc(T_BYTE)) < 0) return RP_BAD;
    n = (uint8_t)c;
    if ((c = io->getc(T_BYTE)) < 0) return RP_BAD;
    nc = (uint8_t)c;
    for (uint16_t i = 0; i < rx_len; i++) {
        if ((c = io->getc(T_BYTE)) < 0) return RP_BAD;
        blk[i] = (uint8_t)c;
    }
    if ((c = io->getc(T_BYTE)) < 0) return RP_BAD;
    if (crc_mode) {
        uint16_t crc = (uint16_t)(c << 8);
        if ((c = io->getc(T_BYTE)) < 0) return RP_BAD;
        crc |= (uint8_t)c;
        if (crc != xf_crc16(blk, rx_len)) return RP_BAD;
    } else if ((uint8_t)c != checksum(blk, rx_len)) {
        return RP_BAD;
    }
    if ((uint8_t)(n ^ nc) != 0xFF) return RP_BAD;
    rx_num = n;
    return RP_OK;
}

/* Sends start characters until the sender answers: 'C' (CRC), then, if
   allowed, NAK (checksum). Returns the first byte, -1 or W_CANCEL. */
static int16_t rx_start(bool allow_checksum) {
    for (uint8_t i = 0; i < START_TRIES * 2; i++) {
        int16_t c;
        if (i >= START_TRIES && !allow_checksum) break;
        crc_mode = i < START_TRIES;
        put1(crc_mode ? 'C' : NAK);
        c = wait_byte(T_START);
        if (c != -1) return c;
    }
    return -1;
}

static bool write_pad(uint16_t n) {
    uint8_t pad[32];
    memset(pad, SUB, sizeof pad);
    while (n) {
        uint16_t k = n > sizeof pad ? sizeof pad : n;
        if (!io->write_file(pad, k)) return false;
        n -= k;
    }
    return true;
}

/* Receives data blocks 1, 2, ... up to EOT. c is the first byte already
   read. With a known total the file is cut to that size; otherwise
   trailing SUB padding is dropped. The SUBs at the end of each block are
   held back as a count and written only when another block follows. */
static xf_result_t rx_data(int16_t c) {
    uint8_t expect = 1, errors = 0;
    uint16_t held = 0;
    for (;;) {
        uint8_t r;
        if (c == W_CANCEL) return stop(XF_CANCELLED);
        if (c < 0) {
            if (++errors >= MAX_ERRORS) return stop(XF_TIMEOUT);
            total_errors++;
            put1(NAK);
            c = wait_byte(T_BLOCK);
            continue;
        }
        r = rx_packet(c);
        if (r == RP_EOT) {
            put1(ACK);
            return XF_OK;
        }
        if (r == RP_CAN) return stop(XF_REMOTE_CANCEL);
        if (r == RP_BAD) {
            drain();
            if (++errors >= MAX_ERRORS) return stop(XF_ERRORS);
            total_errors++;
            put1(NAK);
        } else if (rx_num == (uint8_t)(expect - 1)) {
            put1(ACK);                       /* our ACK was lost: already written */
        } else if (rx_num != expect) {
            return stop(XF_ERRORS);
        } else {
            uint16_t n = rx_len;
            if (total >= 0) {
                if ((uint32_t)total - bytes < n) n = (uint16_t)((uint32_t)total - bytes);
            } else {
                uint16_t subs = 0;
                while (subs < n && blk[n - 1 - subs] == SUB) subs++;
                if (file_open && held && !write_pad(held)) return stop(XF_FILE_ERROR);
                bytes += held;
                held = subs;
                n -= subs;
            }
            if (file_open && n && !io->write_file(blk, n)) return stop(XF_FILE_ERROR);
            bytes += n;
            expect++;
            errors = 0;
            report();
            put1(ACK);
        }
        if (io->cancel()) return stop(XF_CANCELLED);
        c = wait_byte(T_BLOCK);
    }
}

static void finish_file(void) {
    if (file_open) {
        io->close(true);
        file_open = false;
    }
    xf_stats.files++;
    xf_stats.bytes += bytes;
}

static xf_result_t rx_xmodem(const char *name) {
    int16_t c;
    xf_result_t r;
    if (io->open_write(name, -1) != OPEN_OK) return stop(XF_FILE_ERROR);
    file_open = true;
    file_no = 1;
    c = rx_start(true);
    if (c == -1) return stop(XF_TIMEOUT);
    r = rx_data(c);
    if (r == XF_OK) finish_file();
    return r;
}

/* Reads block 0 (c is its first byte). Returns RP_OK, RP_CAN, RP_USER, or
   RP_BAD when it never arrived intact. */
static uint8_t rx_header(int16_t c) {
    for (uint8_t errors = 0;;) {
        uint8_t r;
        if (c == W_CANCEL) return RP_USER;
        r = c < 0 ? RP_BAD : rx_packet(c);
        if (r == RP_CAN) return RP_CAN;
        if (r == RP_OK && rx_num == 0) return RP_OK;
        if (r == RP_EOT) put1(ACK);          /* a repeated EOT from the last file */
        else drain();
        if (++errors >= MAX_ERRORS) return RP_BAD;
        total_errors++;
        put1(r == RP_EOT ? 'C' : NAK);
        c = wait_byte(T_BLOCK);
    }
}

/* Decimal size from block 0; -1 when missing or too big for int32_t. */
static int32_t parse_size(const uint8_t *p, const uint8_t *end) {
    int32_t v = 0;
    if (p >= end || *p < '0' || *p > '9') return -1;
    while (p < end && *p >= '0' && *p <= '9') {
        if (v > (INT32_MAX - 9) / 10) return -1;
        v = v * 10 + (*p++ - '0');
    }
    return v;
}

static xf_result_t rx_ymodem(void) {
    for (;;) {
        char name[FNAME_MAX + 1];
        uint16_t len;
        xf_open_t o;
        xf_result_t r;
        int16_t c = rx_start(false);
        uint8_t h;
        if (c == -1) return stop(XF_TIMEOUT);
        h = rx_header(c);
        if (h == RP_CAN) return stop(XF_REMOTE_CANCEL);
        if (h == RP_USER) return stop(XF_CANCELLED);
        if (h != RP_OK) return stop(XF_ERRORS);
        if (blk[0] == 0) {                   /* empty name: end of batch */
            put1(ACK);
            return XF_OK;
        }
        blk[rx_len - 1] = 0;                 /* the name is always terminated */
        len = (uint16_t)strlen((const char *)blk);
        fname_sanitize((const char *)blk, name);
        total = parse_size(blk + len + 1, blk + rx_len);
        bytes = 0;
        file_no++;
        o = io->open_write(name, total);
        if (o == OPEN_FAIL) return stop(XF_FILE_ERROR);
        file_open = o == OPEN_OK;
        drain();                             /* repeats of block 0 sent while we asked */
        put1(ACK);
        report();
        crc_mode = true;
        put1('C');
        r = rx_data(wait_byte(T_BLOCK));
        if (r != XF_OK) return r;
        if (o == OPEN_OK) finish_file();
    }
}

static void begin(const xfer_io_t *ioface) {
    io = ioface;
    memset(&xf_stats, 0, sizeof xf_stats);
    file_open = false;
    total_errors = 0;
    file_no = 0;
    bytes = 0;
    total = -1;
}

xf_result_t xf_receive(const xfer_io_t *ioface, xf_proto_t p, const char *name) {
    begin(ioface);
    return p == XP_YMODEM ? rx_ymodem() : rx_xmodem(name);
}

/* ---- send ---- */

/* Sends blk[0..len) as block num until it is ACKed. */
static xf_result_t tx_block(uint8_t num, uint16_t len) {
    uint8_t head[3], tail[2];
    head[0] = len == 128 ? SOH : STX;
    head[1] = num;
    head[2] = (uint8_t)~num;
    if (crc_mode) {
        uint16_t crc = xf_crc16(blk, len);
        tail[0] = (uint8_t)(crc >> 8);
        tail[1] = (uint8_t)crc;
    } else {
        tail[0] = checksum(blk, len);
    }
    for (uint8_t tries = 0; tries < MAX_ERRORS; tries++) {
        int16_t c;
        if (tries) {
            total_errors++;
            report();
        }
        io->write(head, 3);
        io->write(blk, len);
        io->write(tail, crc_mode ? 2 : 1);
        for (;;) {
            c = wait_byte(T_BLOCK);
            if (c == ACK) return XF_OK;
            if (c == W_CANCEL) return stop(XF_CANCELLED);
            if (can2(c)) return stop(XF_REMOTE_CANCEL);
            if (c == NAK || c < 0) break;
            /* anything else (a stray 'C', noise) is ignored */
        }
    }
    return stop(XF_ERRORS);
}

static xf_result_t tx_eot(void) {
    for (uint8_t tries = 0; tries < MAX_ERRORS; tries++) {
        int16_t c;
        put1(EOT);
        c = wait_byte(T_BLOCK);
        if (c == ACK) return XF_OK;
        if (c == W_CANCEL) return stop(XF_CANCELLED);
        if (can2(c)) return stop(XF_REMOTE_CANCEL);
    }
    return stop(XF_ERRORS);
}

/* Waits for the receiver's 'C' (CRC) or NAK (checksum). */
static xf_result_t tx_start(void) {
    for (;;) {
        int16_t c = wait_byte(T_SEND_START);
        if (c == -1) return stop(XF_TIMEOUT);
        if (c == W_CANCEL) return stop(XF_CANCELLED);
        if (can2(c)) return stop(XF_REMOTE_CANCEL);
        if (c == 'C' || c == NAK) {
            crc_mode = c == 'C';
            return XF_OK;
        }
    }
}

static xf_result_t tx_header(const char *name, uint32_t size) {
    memset(blk, 0, 128);
    strcpy((char *)blk, name);
    util_u32((char *)blk + strlen(name) + 1, size);
    return tx_block(0, 128);
}

static xf_result_t tx_file(xf_proto_t p, uint32_t size) {
    uint8_t num = 1;
    xf_result_t r;
    bytes = 0;
    total = (int32_t)size;
    report();
    for (;;) {
        uint16_t len = 128;
        int16_t n;
        if (p != XP_XMODEM && crc_mode && size - bytes > 128) len = 1024;
        n = io->read(blk, len);
        if (n < 0) return stop(XF_FILE_ERROR);
        if (n == 0) break;
        memset(blk + n, SUB, len - (uint16_t)n);
        if ((r = tx_block(num++, len)) != XF_OK) return r;
        bytes += (uint16_t)n;
        report();
        if (io->cancel()) return stop(XF_CANCELLED);
        if ((uint16_t)n < len) break;
    }
    return tx_eot();
}

xf_result_t xf_send(const xfer_io_t *ioface, xf_proto_t p, uint8_t nfiles) {
    xf_result_t r;
    begin(ioface);
    for (file_no = 1; file_no <= nfiles; file_no++) {
        char name[FNAME_MAX + 1];
        uint32_t size;
        if (!io->open_read((uint8_t)(file_no - 1), name, &size)) return stop(XF_FILE_ERROR);
        r = tx_start();
        if (r == XF_OK && p == XP_YMODEM) {
            r = tx_header(name, size);
            if (r == XF_OK) r = tx_start();
        }
        if (r == XF_OK) r = tx_file(p, size);
        io->close(true);
        if (r != XF_OK) return r;
        xf_stats.files++;
        xf_stats.bytes += bytes;
        if (p != XP_YMODEM) return XF_OK;
    }
    if ((r = tx_start()) != XF_OK) return r;
    memset(blk, 0, 128);
    return tx_block(0, 128);
}
