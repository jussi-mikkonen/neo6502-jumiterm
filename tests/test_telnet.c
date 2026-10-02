#include "check.h"
#include "telnet.h"

static uint16_t rx(const uint8_t *in, uint16_t n, uint8_t *out) {
    memcpy(out, in, n);
    return telnet_rx(out, n);
}

static void test_plain_data_passes(void) {
    uint8_t buf[8];
    telnet_init();
    CHECK_EQ(rx((const uint8_t *)"hello", 5, buf), 5);
    CHECK_MEM(buf, "hello", 5);
    CHECK_EQ(telnet.reply_len, 0);
}

static void test_escaped_ff_is_data(void) {
    const uint8_t in[] = { 'a', 255, 255, 'b' };
    uint8_t buf[8];
    telnet_init();
    CHECK_EQ(rx(in, 4, buf), 3);
    CHECK_EQ(buf[0], 'a');
    CHECK_EQ(buf[1], 255);
    CHECK_EQ(buf[2], 'b');
}

static void test_will_echo_and_binary(void) {
    const uint8_t in[] = { 'x', TN_IAC, TN_WILL, TNOPT_ECHO, TN_IAC, TN_WILL, TNOPT_BINARY, 'y' };
    const uint8_t want[] = { TN_IAC, TN_DO, TNOPT_ECHO, TN_IAC, TN_DO, TNOPT_BINARY };
    uint8_t buf[8];
    telnet_init();
    CHECK_EQ(rx(in, sizeof in, buf), 2);
    CHECK_MEM(buf, "xy", 2);
    CHECK_EQ(telnet.reply_len, sizeof want);
    CHECK_MEM(telnet.reply, want, sizeof want);
    CHECK(telnet.rx_binary);
    CHECK(!telnet.tx_binary);
}

static void test_ttype_negotiation(void) {
    const uint8_t in[] = { TN_IAC, TN_DO, TNOPT_TTYPE, TN_IAC, TN_SB, TNOPT_TTYPE, 1, TN_IAC, TN_SE };
    const uint8_t want[] = { TN_IAC, TN_WILL, TNOPT_TTYPE,
                             TN_IAC, TN_SB, TNOPT_TTYPE, 0, 'A', 'N', 'S', 'I', TN_IAC, TN_SE };
    uint8_t buf[16];
    telnet_init();
    CHECK_EQ(rx(in, sizeof in, buf), 0);
    CHECK_EQ(telnet.reply_len, sizeof want);
    CHECK_MEM(telnet.reply, want, sizeof want);
}

static void test_naws(void) {
    const uint8_t in[] = { TN_IAC, TN_DO, TNOPT_NAWS };
    const uint8_t want[] = { TN_IAC, TN_WILL, TNOPT_NAWS,
                             TN_IAC, TN_SB, TNOPT_NAWS, 0, 80, 0, 25, TN_IAC, TN_SE };
    uint8_t buf[4];
    telnet_init();
    rx(in, sizeof in, buf);
    CHECK_EQ(telnet.reply_len, sizeof want);
    CHECK_MEM(telnet.reply, want, sizeof want);
}

static void test_unknown_do_refused_once(void) {
    const uint8_t in[] = { TN_IAC, TN_DO, 32, TN_IAC, TN_DO, 32 };
    const uint8_t want[] = { TN_IAC, TN_WONT, 32 };
    uint8_t buf[8];
    telnet_init();
    rx(in, sizeof in, buf);
    CHECK_EQ(telnet.reply_len, 3);
    CHECK_MEM(telnet.reply, want, 3);
}

static void test_wont_dont_get_no_reply(void) {
    const uint8_t in[] = { TN_IAC, TN_WONT, TNOPT_ECHO, TN_IAC, TN_DONT, TNOPT_SGA };
    uint8_t buf[8];
    telnet_init();
    rx(in, sizeof in, buf);
    CHECK_EQ(telnet.reply_len, 0);
}

static void test_split_across_chunks(void) {
    const uint8_t a[] = { 'p', TN_IAC };
    const uint8_t b[] = { TN_WILL };
    const uint8_t c[] = { TNOPT_SGA, 'q' };
    const uint8_t want[] = { TN_IAC, TN_DO, TNOPT_SGA };
    uint8_t buf[4];
    telnet_init();
    CHECK_EQ(rx(a, sizeof a, buf), 1);
    CHECK_EQ(buf[0], 'p');
    CHECK_EQ(rx(b, sizeof b, buf), 0);
    CHECK_EQ(rx(c, sizeof c, buf), 1);
    CHECK_EQ(buf[0], 'q');
    CHECK_MEM(telnet.reply, want, 3);
}

static void test_other_commands_swallowed(void) {
    const uint8_t in[] = { 'x', TN_IAC, 241, 'y', TN_IAC, 249, 'z' };
    uint8_t buf[8];
    telnet_init();
    CHECK_EQ(rx(in, sizeof in, buf), 3);
    CHECK_MEM(buf, "xyz", 3);
}

static void test_escaped_iac_inside_subnegotiation(void) {
    const uint8_t in[] = { TN_IAC, TN_SB, 99, TN_IAC, TN_IAC, 5, TN_IAC, TN_SE, 'k' };
    uint8_t buf[12];
    telnet_init();
    CHECK_EQ(rx(in, sizeof in, buf), 1);
    CHECK_EQ(buf[0], 'k');
}

static void test_reply_buffer_never_overflows(void) {
    uint8_t in[3 * 40];
    uint8_t buf[sizeof in];
    telnet_init();
    for (int i = 0; i < 40; i++) {
        in[i * 3] = TN_IAC;
        in[i * 3 + 1] = TN_DO;
        in[i * 3 + 2] = (uint8_t)(100 + i);
    }
    rx(in, sizeof in, buf);
    CHECK(telnet.reply_len <= TELNET_REPLY_MAX);
    CHECK_EQ(telnet.reply_len, 63);
}

static void test_encode(void) {
    uint8_t out[2];
    telnet_init();
    CHECK_EQ(telnet_encode('a', out), 1);
    CHECK_EQ(out[0], 'a');
    CHECK_EQ(telnet_encode(0xFF, out), 2);
    CHECK_EQ(out[0], 0xFF);
    CHECK_EQ(out[1], 0xFF);
    CHECK_EQ(telnet_encode('\r', out), 2);
    CHECK_EQ(out[0], '\r');
    CHECK_EQ(out[1], 0);
}

static void test_do_binary_agreed(void) {
    const uint8_t in[] = { TN_IAC, TN_DO, TNOPT_BINARY };
    const uint8_t want[] = { TN_IAC, TN_WILL, TNOPT_BINARY };
    uint8_t buf[4];
    telnet_init();
    rx(in, sizeof in, buf);
    CHECK_EQ(telnet.reply_len, 3);
    CHECK_MEM(telnet.reply, want, 3);
    CHECK(telnet.tx_binary);
}

static void test_request_binary_and_acks(void) {
    const uint8_t want[] = { TN_IAC, TN_WILL, TNOPT_BINARY, TN_IAC, TN_DO, TNOPT_BINARY };
    const uint8_t acks[] = { TN_IAC, TN_DO, TNOPT_BINARY, TN_IAC, TN_WILL, TNOPT_BINARY };
    uint8_t buf[8];
    telnet_init();
    telnet_request_binary();
    CHECK_EQ(telnet.reply_len, sizeof want);
    CHECK_MEM(telnet.reply, want, sizeof want);
    telnet.reply_len = 0;
    rx(acks, sizeof acks, buf);
    CHECK_EQ(telnet.reply_len, 0);    /* the answers are acknowledgements, not new requests */
    CHECK(telnet.tx_binary);
    CHECK(telnet.rx_binary);
    telnet_request_binary();
    CHECK_EQ(telnet.reply_len, 0);
}

static void test_request_binary_refused(void) {
    const uint8_t no[] = { TN_IAC, TN_DONT, TNOPT_BINARY, TN_IAC, TN_WONT, TNOPT_BINARY };
    uint8_t buf[8];
    telnet_init();
    telnet_request_binary();
    telnet.reply_len = 0;
    rx(no, sizeof no, buf);
    CHECK_EQ(telnet.reply_len, 0);
    CHECK(!telnet.tx_binary);
    CHECK(!telnet.rx_binary);
    telnet_request_binary();
    CHECK_EQ(telnet.reply_len, 0);    /* asked once: never asks again */
}

static void test_request_binary_skips_agreed_direction(void) {
    const uint8_t in[] = { TN_IAC, TN_WILL, TNOPT_BINARY };
    const uint8_t want[] = { TN_IAC, TN_WILL, TNOPT_BINARY };
    uint8_t buf[4];
    telnet_init();
    rx(in, sizeof in, buf);
    telnet.reply_len = 0;
    telnet_request_binary();
    CHECK_EQ(telnet.reply_len, 3);
    CHECK_MEM(telnet.reply, want, 3);
}

static void test_cr_nul_in_text_mode(void) {
    const uint8_t in[] = { 'a', '\r', 0, 'b', 0, '\r', '\n' };
    uint8_t buf[8];
    telnet_init();
    CHECK_EQ(rx(in, sizeof in, buf), 6);
    CHECK_MEM(buf, "a\rb\0\r\n", 6);
}

static void test_cr_nul_split_across_chunks(void) {
    const uint8_t a[] = { 'a', '\r' };
    const uint8_t b[] = { 0, 'b' };
    uint8_t buf[4];
    telnet_init();
    CHECK_EQ(rx(a, sizeof a, buf), 2);
    CHECK_EQ(rx(b, sizeof b, buf), 1);
    CHECK_EQ(buf[0], 'b');
}

static void test_cr_nul_kept_in_binary_mode(void) {
    const uint8_t in[] = { TN_IAC, TN_WILL, TNOPT_BINARY, '\r', 0, 'x' };
    uint8_t buf[8];
    telnet_init();
    CHECK_EQ(rx(in, sizeof in, buf), 3);
    CHECK_MEM(buf, "\r\0x", 3);
}

static void test_encode_binary_cr(void) {
    const uint8_t in[] = { TN_IAC, TN_DO, TNOPT_BINARY };
    uint8_t buf[4], out[2];
    telnet_init();
    rx(in, sizeof in, buf);
    CHECK_EQ(telnet_encode('\r', out), 1);
    CHECK_EQ(telnet_encode(0xFF, out), 2);
}

int main(void) {
    test_plain_data_passes();
    test_escaped_ff_is_data();
    test_will_echo_and_binary();
    test_ttype_negotiation();
    test_naws();
    test_unknown_do_refused_once();
    test_wont_dont_get_no_reply();
    test_split_across_chunks();
    test_other_commands_swallowed();
    test_escaped_iac_inside_subnegotiation();
    test_reply_buffer_never_overflows();
    test_encode();
    test_do_binary_agreed();
    test_request_binary_and_acks();
    test_request_binary_refused();
    test_request_binary_skips_agreed_direction();
    test_cr_nul_in_text_mode();
    test_cr_nul_split_across_chunks();
    test_cr_nul_kept_in_binary_mode();
    test_encode_binary_cr();
    return check_report();
}
