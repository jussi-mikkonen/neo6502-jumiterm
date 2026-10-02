/* Scripted-peer tests for src/xmodem.c. The peer's bytes come from a
   script. SILENCE in the script means the line stays quiet until our side
   writes something (a NAK, an ACK, ...) after the peer's last byte, which
   is how a real peer waits for an answer. A SILENCE at the end of what the
   peer has to say makes getc time out. */
#include "check.h"
#include "xmodem.h"

#define SILENCE (-1)
#define SOH 0x01
#define STX 0x02
#define EOT 0x04
#define ACK 0x06
#define NAK 0x15
#define CAN 0x18
#define SUB 0x1A

static int16_t script[20000];
static int script_len, script_pos;
static bool wrote;                       /* our side wrote since the last SILENCE */
static uint8_t sent[20000];              /* everything our side wrote */
static int sent_len;
static int cancel_after;                 /* cancel() is true on exactly this call, like one Esc press; 0 = never */
static int cancel_calls;

static uint8_t file[70000];              /* the download, or the upload source */
static uint32_t file_len, file_pos;
static char opened_name[FNAME_MAX + 1];
static int32_t opened_size;
static int opens, closes_keep, closes_delete;
static xf_open_t open_answer[4];         /* per open_write call */
static uint32_t upload_size;

static void reset(void) {
    script_len = script_pos = sent_len = 0;
    wrote = false;
    cancel_after = cancel_calls = 0;
    file_len = file_pos = 0;
    opened_name[0] = 0;
    opened_size = -2;
    opens = closes_keep = closes_delete = 0;
    for (int i = 0; i < 4; i++) open_answer[i] = OPEN_OK;
}

static void add(int16_t b) { script[script_len++] = b; }

static void add_bytes(const uint8_t *p, int n) {
    for (int i = 0; i < n; i++) add(p[i]);
}

/* Appends a block as a sender would send it. */
static void add_block(uint8_t num, const uint8_t *data, uint16_t len, bool crc) {
    add(len == 128 ? SOH : STX);
    add(num);
    add((uint8_t)~num);
    add_bytes(data, len);
    if (crc) {
        uint16_t c = xf_crc16(data, len);
        add(c >> 8);
        add(c & 0xFF);
    } else {
        uint8_t s = 0;
        for (uint16_t i = 0; i < len; i++) s = (uint8_t)(s + data[i]);
        add(s);
    }
}

static int16_t t_getc(uint16_t timeout) {
    (void)timeout;
    while (script_pos < script_len) {
        if (script[script_pos] != SILENCE) {
            wrote = false;            /* only answers to what the peer sends next count */
            return script[script_pos++];
        }
        if (!wrote) return -1;
        wrote = false;
        script_pos++;
    }
    return -1;
}

static void t_write(const uint8_t *p, uint16_t n) {
    memcpy(sent + sent_len, p, n);
    sent_len += n;
    wrote = true;
}

static bool t_cancel(void) {
    return cancel_after && ++cancel_calls == cancel_after;
}

static xf_open_t t_open_write(const char *name, int32_t size) {
    xf_open_t a = open_answer[opens++];
    strcpy(opened_name, name);
    opened_size = size;
    if (a == OPEN_OK) file_len = 0;
    return a;
}

static bool t_open_read(uint8_t index, char *name, uint32_t *size) {
    (void)index;
    strcpy(name, "up.bin");
    *size = upload_size;
    file_pos = 0;
    return true;
}

static int16_t t_read(uint8_t *p, uint16_t n) {
    uint32_t left = upload_size - file_pos;
    if (n > left) n = (uint16_t)left;
    memcpy(p, file + file_pos, n);
    file_pos += n;
    return (int16_t)n;
}

static bool t_write_file(const uint8_t *p, uint16_t n) {
    memcpy(file + file_len, p, n);
    file_len += n;
    return true;
}

static void t_close(bool keep) {
    if (keep) closes_keep++;
    else closes_delete++;
}

static void t_progress(uint32_t bytes, int32_t total, uint8_t errors, uint8_t file_no) {
    (void)bytes; (void)total; (void)errors; (void)file_no;
}

static const xfer_io_t tio = {
    t_getc, t_write, t_cancel, t_open_write, t_open_read, t_read, t_write_file, t_close, t_progress
};

static void fill(uint8_t *p, uint16_t n, uint8_t seed) {
    for (uint16_t i = 0; i < n; i++) p[i] = (uint8_t)(i * 7 + seed);
}

/* ---- CRC ---- */

static void test_crc16_check_value(void) {
    CHECK_EQ(xf_crc16((const uint8_t *)"123456789", 9), 0x31C3);
}

/* ---- XMODEM receive ---- */

static void test_rx_crc_two_blocks_trims_padding(void) {
    uint8_t a[128], b[128];
    const uint8_t want_sent[] = { 'C', ACK, ACK, ACK };
    reset();
    fill(a, 128, 1);
    memset(b, SUB, 128);
    fill(b, 50, 2);
    add_block(1, a, 128, true);
    add(SILENCE);
    add_block(2, b, 128, true);
    add(SILENCE);
    add(EOT);
    CHECK_EQ(xf_receive(&tio, XP_XMODEM, "dl.bin"), XF_OK);
    CHECK_STR(opened_name, "dl.bin");
    CHECK_EQ(file_len, 178);
    CHECK_MEM(file, a, 128);
    CHECK_MEM(file + 128, b, 50);
    CHECK_EQ(sent_len, sizeof want_sent);
    CHECK_MEM(sent, want_sent, sizeof want_sent);
    CHECK_EQ(closes_keep, 1);
    CHECK_EQ(xf_stats.files, 1);
    CHECK_EQ(xf_stats.bytes, 178);
}

static void test_rx_sub_inside_data_is_kept(void) {
    /* Block 1 ends in SUBs that are real data, because block 2 follows. */
    uint8_t a[128], b[128];
    reset();
    fill(a, 128, 1);
    memset(a + 100, SUB, 28);
    fill(b, 128, 9);
    add_block(1, a, 128, true);
    add(SILENCE);
    add_block(2, b, 128, true);
    add(SILENCE);
    add(EOT);
    CHECK_EQ(xf_receive(&tio, XP_XMODEM, "dl.bin"), XF_OK);
    CHECK_EQ(file_len, 256);
    CHECK_MEM(file, a, 128);
    CHECK_MEM(file + 128, b, 128);
}

static void test_rx_1k_blocks(void) {
    uint8_t a[1024];
    reset();
    fill(a, 1024, 3);
    add_block(1, a, 1024, true);
    add(SILENCE);
    add(EOT);
    CHECK_EQ(xf_receive(&tio, XP_XMODEM, "dl.bin"), XF_OK);
    CHECK_EQ(file_len, 1024);
    CHECK_MEM(file, a, 1024);
}

static void test_rx_falls_back_to_checksum(void) {
    uint8_t a[128];
    const uint8_t want_sent[] = { 'C', 'C', 'C', NAK, ACK, ACK };
    reset();
    fill(a, 128, 4);
    add(SILENCE);   /* ignores 'C' three times */
    add(SILENCE);
    add(SILENCE);
    add(SILENCE);
    add_block(1, a, 128, false);
    add(SILENCE);
    add(EOT);
    CHECK_EQ(xf_receive(&tio, XP_XMODEM, "dl.bin"), XF_OK);
    CHECK_EQ(file_len, 128);
    CHECK_EQ(sent_len, sizeof want_sent);
    CHECK_MEM(sent, want_sent, sizeof want_sent);
}

static void test_rx_bad_crc_is_resent(void) {
    uint8_t a[128];
    const uint8_t want_sent[] = { 'C', NAK, ACK, ACK };
    reset();
    fill(a, 128, 5);
    add_block(1, a, 128, true);
    script[script_len - 1] ^= 0x01;   /* corrupt the CRC */
    add(SILENCE);
    add_block(1, a, 128, true);
    add(SILENCE);
    add(EOT);
    CHECK_EQ(xf_receive(&tio, XP_XMODEM, "dl.bin"), XF_OK);
    CHECK_EQ(file_len, 128);
    CHECK_MEM(file, a, 128);
    CHECK_EQ(sent_len, sizeof want_sent);
    CHECK_MEM(sent, want_sent, sizeof want_sent);
}

static void test_rx_dropped_byte_is_resent(void) {
    uint8_t a[128];
    reset();
    fill(a, 128, 6);
    add_block(1, a, 128, true);
    memmove(script + 40, script + 41, (size_t)(script_len - 41) * sizeof script[0]);
    script_len--;                     /* one data byte lost on the line */
    add(SILENCE);
    add_block(1, a, 128, true);
    add(SILENCE);
    add(EOT);
    CHECK_EQ(xf_receive(&tio, XP_XMODEM, "dl.bin"), XF_OK);
    CHECK_EQ(file_len, 128);
    CHECK_MEM(file, a, 128);
    CHECK_EQ(sent[1], NAK);
}

static void test_rx_duplicate_block_written_once(void) {
    uint8_t a[128], b[128];
    reset();
    fill(a, 128, 7);
    fill(b, 128, 8);
    add_block(1, a, 128, true);
    add(SILENCE);
    add_block(1, a, 128, true);       /* the sender missed our ACK */
    add(SILENCE);
    add_block(2, b, 128, true);
    add(SILENCE);
    add(EOT);
    CHECK_EQ(xf_receive(&tio, XP_XMODEM, "dl.bin"), XF_OK);
    CHECK_EQ(file_len, 256);
    CHECK_MEM(file + 128, b, 128);
}

static void test_rx_block_out_of_sequence_aborts(void) {
    uint8_t a[128];
    reset();
    fill(a, 128, 9);
    add_block(1, a, 128, true);
    add(SILENCE);
    add_block(3, a, 128, true);
    CHECK_EQ(xf_receive(&tio, XP_XMODEM, "dl.bin"), XF_ERRORS);
    CHECK_EQ(closes_delete, 1);
    CHECK_EQ(sent[sent_len - 6], CAN);
}

static void test_rx_silence_times_out(void) {
    reset();
    CHECK_EQ(xf_receive(&tio, XP_XMODEM, "dl.bin"), XF_TIMEOUT);
    CHECK_EQ(closes_delete, 1);
    CHECK_EQ(sent[0], 'C');
    CHECK_EQ(sent[5], NAK);
}

static void test_rx_remote_cancel(void) {
    uint8_t a[128];
    reset();
    fill(a, 128, 10);
    add_block(1, a, 128, true);
    add(SILENCE);
    add(CAN);
    add(CAN);
    CHECK_EQ(xf_receive(&tio, XP_XMODEM, "dl.bin"), XF_REMOTE_CANCEL);
    CHECK_EQ(closes_delete, 1);
    CHECK_EQ(sent[sent_len - 1], ACK);   /* we do not answer a cancel with CAN */
}

static void test_rx_user_cancel(void) {
    uint8_t a[128];
    const uint8_t tail[] = { CAN, CAN, CAN, 8, 8, 8 };
    reset();
    fill(a, 128, 11);
    add_block(1, a, 128, true);
    add(SILENCE);
    add_block(2, a, 128, true);
    cancel_after = 1;
    CHECK_EQ(xf_receive(&tio, XP_XMODEM, "dl.bin"), XF_CANCELLED);
    CHECK_EQ(closes_delete, 1);
    CHECK_EQ(closes_keep, 0);
    CHECK_MEM(sent + sent_len - 6, tail, 6);
}

static void test_rx_open_failure(void) {
    reset();
    open_answer[0] = OPEN_FAIL;
    CHECK_EQ(xf_receive(&tio, XP_XMODEM, "dl.bin"), XF_FILE_ERROR);
    CHECK_EQ(closes_delete + closes_keep, 0);
}

/* ---- YMODEM receive ---- */

static void add_header(const char *name, const char *size) {
    uint8_t h[128];
    memset(h, 0, sizeof h);
    if (name) {
        strcpy((char *)h, name);
        strcpy((char *)h + strlen(name) + 1, size);
    }
    add_block(0, h, 128, true);
}

static void test_ry_batch_cuts_to_size(void) {
    uint8_t a[1024], b[128];
    const uint8_t want_sent[] = { 'C', ACK, 'C', ACK, ACK, ACK, 'C', ACK };
    reset();
    fill(a, 1024, 12);
    fill(b, 128, 13);
    add_header("dir/Report.TXT", "1100 1234567 644");
    add(SILENCE);                     /* until our ACK and 'C' */
    add_block(1, a, 1024, true);
    add(SILENCE);
    add_block(2, b, 128, true);
    add(SILENCE);
    add(EOT);
    add(SILENCE);
    add_header(NULL, NULL);
    CHECK_EQ(xf_receive(&tio, XP_YMODEM, NULL), XF_OK);
    CHECK_STR(opened_name, "Report.TXT");
    CHECK_EQ(opened_size, 1100);
    CHECK_EQ(file_len, 1100);
    CHECK_MEM(file, a, 1024);
    CHECK_MEM(file + 1024, b, 76);
    CHECK_EQ(sent_len, sizeof want_sent);
    CHECK_MEM(sent, want_sent, sizeof want_sent);
    CHECK_EQ(xf_stats.files, 1);
}

static void test_ry_skip_then_next_file(void) {
    uint8_t a[128], b[128];
    reset();
    fill(a, 128, 14);
    fill(b, 128, 15);
    open_answer[0] = OPEN_SKIP;
    add_header("one.bin", "128");
    add(SILENCE);
    add_block(1, a, 128, true);
    add(SILENCE);
    add(EOT);
    add(SILENCE);
    add_header("two.bin", "100");
    add(SILENCE);
    add_block(1, b, 128, true);
    add(SILENCE);
    add(EOT);
    add(SILENCE);
    add_header(NULL, NULL);
    CHECK_EQ(xf_receive(&tio, XP_YMODEM, NULL), XF_OK);
    CHECK_EQ(opens, 2);
    CHECK_STR(opened_name, "two.bin");
    CHECK_EQ(file_len, 100);
    CHECK_MEM(file, b, 100);
    CHECK_EQ(closes_keep, 1);
    CHECK_EQ(xf_stats.files, 1);
}

static void test_ry_empty_file(void) {
    reset();
    add_header("empty", "0");
    add(SILENCE);
    add(EOT);
    add(SILENCE);
    add_header(NULL, NULL);
    CHECK_EQ(xf_receive(&tio, XP_YMODEM, NULL), XF_OK);
    CHECK_EQ(opened_size, 0);
    CHECK_EQ(file_len, 0);
    CHECK_EQ(closes_keep, 1);
}

static void test_ry_missing_size_trims_padding(void) {
    uint8_t a[128];
    reset();
    memset(a, SUB, 128);
    fill(a, 30, 16);
    add_header("nosize", "");
    add(SILENCE);
    add_block(1, a, 128, true);
    add(SILENCE);
    add(EOT);
    add(SILENCE);
    add_header(NULL, NULL);
    CHECK_EQ(xf_receive(&tio, XP_YMODEM, NULL), XF_OK);
    CHECK_EQ(opened_size, -1);
    CHECK_EQ(file_len, 30);
}

static void test_ry_cancel_while_header_retries(void) {
    /* Esc while a bad block 0 is being retried: one keypress, "Cancelled". */
    reset();
    add_header("a.bin", "10");
    script[script_len - 1] ^= 0x01;   /* corrupt the CRC */
    cancel_after = 1;
    CHECK_EQ(xf_receive(&tio, XP_YMODEM, NULL), XF_CANCELLED);
    CHECK_EQ(opens, 0);
}

static void test_ry_oversized_size_is_unknown(void) {
    uint8_t a[128];
    reset();
    memset(a, SUB, 128);
    fill(a, 20, 24);
    add_header("big", "99999999999");
    add(SILENCE);
    add_block(1, a, 128, true);
    add(SILENCE);
    add(EOT);
    add(SILENCE);
    add_header(NULL, NULL);
    CHECK_EQ(xf_receive(&tio, XP_YMODEM, NULL), XF_OK);
    CHECK_EQ(opened_size, -1);        /* not a wrapped number */
    CHECK_EQ(file_len, 20);
}

/* ---- send ---- */

/* Checks that sent[at..] holds block num of len bytes from data. */
static int expect_block(int at, uint8_t num, const uint8_t *data, uint16_t len, uint16_t real) {
    uint16_t crc;
    uint8_t padded[1024];
    memset(padded, SUB, len);
    memcpy(padded, data, real);
    crc = xf_crc16(padded, len);
    CHECK_EQ(sent[at], len == 128 ? SOH : STX);
    CHECK_EQ(sent[at + 1], num);
    CHECK_EQ(sent[at + 2], (uint8_t)~num);
    CHECK_MEM(sent + at + 3, padded, len);
    CHECK_EQ(sent[at + 3 + len], crc >> 8);
    CHECK_EQ(sent[at + 4 + len], crc & 0xFF);
    return at + 5 + len;
}

static void test_tx_xmodem_1k_with_short_tail(void) {
    int at;
    reset();
    upload_size = 1100;
    fill(file, 1100, 17);
    add('C');
    add(SILENCE);
    add(ACK);
    add(SILENCE);
    add(ACK);
    add(SILENCE);
    add(ACK);   /* EOT */
    CHECK_EQ(xf_send(&tio, XP_XMODEM_1K, 1), XF_OK);
    at = expect_block(0, 1, file, 1024, 1024);
    at = expect_block(at, 2, file + 1024, 128, 76);
    CHECK_EQ(sent[at], EOT);
    CHECK_EQ(sent_len, at + 1);
    CHECK_EQ(closes_keep, 1);
}

static void test_tx_checksum_mode_uses_128(void) {
    reset();
    upload_size = 300;
    fill(file, 300, 18);
    add(NAK);
    for (int i = 0; i < 4; i++) {
        add(SILENCE);
        add(ACK);
    }
    CHECK_EQ(xf_send(&tio, XP_XMODEM_1K, 1), XF_OK);
    CHECK_EQ(sent[0], SOH);
    CHECK_EQ(sent_len, 3 * 132 + 1);
    {
        uint8_t s = 0;
        for (int i = 0; i < 128; i++) s = (uint8_t)(s + file[i]);
        CHECK_EQ(sent[131], s);       /* one checksum byte, no CRC */
    }
}

static void test_tx_nak_resends(void) {
    reset();
    upload_size = 10;
    fill(file, 10, 19);
    add('C');
    add(SILENCE);
    add(NAK);
    add(SILENCE);
    add(ACK);
    add(SILENCE);
    add(ACK);
    CHECK_EQ(xf_send(&tio, XP_XMODEM, 1), XF_OK);
    CHECK_EQ(sent_len, 2 * 133 + 1);
    CHECK_MEM(sent, sent + 133, 133);
}

static void test_tx_ymodem_header(void) {
    reset();
    upload_size = 5;
    fill(file, 5, 20);
    add('C');
    add(SILENCE);
    add(ACK);   /* block 0 */
    add('C');
    add(SILENCE);
    add(ACK);   /* block 1 */
    add(SILENCE);
    add(ACK);   /* EOT */
    add('C');
    add(SILENCE);
    add(ACK);   /* empty block 0 */
    CHECK_EQ(xf_send(&tio, XP_YMODEM, 1), XF_OK);
    CHECK_EQ(sent[0], SOH);
    CHECK_EQ(sent[1], 0);
    CHECK_STR((const char *)sent + 3, "up.bin");
    CHECK_STR((const char *)sent + 3 + 7, "5");
    CHECK_EQ(sent[133], SOH);         /* <= 128 bytes left: a 128-byte block */
    CHECK_EQ(sent[134], 1);
    CHECK_EQ(sent[133 + 133], EOT);
    CHECK_EQ(sent[133 + 133 + 1], SOH);
    CHECK_EQ(sent[133 + 133 + 1 + 3], 0);   /* empty name ends the batch */
}

static void test_tx_remote_cancel_keeps_source(void) {
    reset();
    upload_size = 200;
    fill(file, 200, 21);
    add('C');
    add(SILENCE);
    add(CAN);
    add(CAN);
    CHECK_EQ(xf_send(&tio, XP_XMODEM, 1), XF_REMOTE_CANCEL);
    CHECK_EQ(closes_delete, 0);
    CHECK_EQ(closes_keep, 1);
}

static void test_tx_user_cancel_keeps_source(void) {
    reset();
    upload_size = 2000;
    fill(file, 2000, 22);
    add('C');
    add(SILENCE);
    add(ACK);
    cancel_after = 1;
    CHECK_EQ(xf_send(&tio, XP_XMODEM, 1), XF_CANCELLED);
    CHECK_EQ(closes_delete, 0);
    CHECK_EQ(sent[sent_len - 6], CAN);
}

static void test_tx_no_receiver_times_out(void) {
    reset();
    upload_size = 10;
    CHECK_EQ(xf_send(&tio, XP_XMODEM, 1), XF_TIMEOUT);
    CHECK_EQ(closes_delete, 0);
}

static void test_tx_exact_multiple_ends_cleanly(void) {
    /* The last read returns 0 bytes: no empty block, straight to EOT. */
    reset();
    upload_size = 256;
    fill(file, 256, 23);
    add('C');
    for (int i = 0; i < 3; i++) {
        add(SILENCE);
        add(ACK);
    }
    CHECK_EQ(xf_send(&tio, XP_XMODEM, 1), XF_OK);
    CHECK_EQ(sent_len, 2 * 133 + 1);
    CHECK_EQ(sent[133 + 1], 2);
    CHECK_EQ(sent[2 * 133], EOT);
}

int main(void) {
    test_crc16_check_value();
    test_rx_crc_two_blocks_trims_padding();
    test_rx_sub_inside_data_is_kept();
    test_rx_1k_blocks();
    test_rx_falls_back_to_checksum();
    test_rx_bad_crc_is_resent();
    test_rx_dropped_byte_is_resent();
    test_rx_duplicate_block_written_once();
    test_rx_block_out_of_sequence_aborts();
    test_rx_silence_times_out();
    test_rx_remote_cancel();
    test_rx_user_cancel();
    test_rx_open_failure();
    test_ry_batch_cuts_to_size();
    test_ry_skip_then_next_file();
    test_ry_empty_file();
    test_ry_missing_size_trims_padding();
    test_ry_cancel_while_header_retries();
    test_ry_oversized_size_is_unknown();
    test_tx_xmodem_1k_with_short_tail();
    test_tx_checksum_mode_uses_128();
    test_tx_nak_resends();
    test_tx_ymodem_header();
    test_tx_remote_cancel_keeps_source();
    test_tx_user_cancel_keeps_source();
    test_tx_no_receiver_times_out();
    test_tx_exact_multiple_ends_cleanly();
    return check_report();
}
