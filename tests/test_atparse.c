#include "atparse.h"
#include "check.h"

/* Feeds s and returns the last non-AT_MORE result (AT_MORE if none). */
static at_result_t feed(const char *s) {
    at_result_t last = AT_MORE;
    while (*s) {
        at_result_t r = at_feed((uint8_t)*s++);
        if (r != AT_MORE) last = r;
    }
    return last;
}

static void test_results(void) {
    at_reset();
    CHECK_EQ(feed("\r\nOK\r\n"), AT_OK);
    CHECK_EQ(feed("ERROR\r\n"), AT_ERROR);
    CHECK_EQ(feed("FAIL\r\n"), AT_ERROR);
    CHECK_EQ(feed("SEND OK\r\n"), AT_OK);
    CHECK_EQ(feed("SEND FAIL\r\n"), AT_ERROR);
    CHECK_EQ(feed("\r\n\r\n"), AT_MORE);
}

static void test_prompt_after_ok(void) {
    at_reset();
    CHECK_EQ(feed("\r\nOK\r\n"), AT_OK);
    CHECK_EQ(feed("\r\n>"), AT_PROMPT);
}

static void test_chatter_is_not_a_result(void) {
    at_reset();
    CHECK_EQ(feed("AT+CWSTATE?\r\n"), AT_LINE);
    CHECK_STR(at.line, "AT+CWSTATE?");
    CHECK_EQ(feed("WIFI GOT IP\r\n"), AT_LINE);
    CHECK_EQ(feed("busy p...\r\n"), AT_LINE);
    CHECK_EQ(feed("OKAY\r\n"), AT_LINE);
    CHECK_EQ(feed("CONNECT\r\n"), AT_LINE);
    CHECK_STR(at.line, "CONNECT");
}

static void test_long_line_truncated(void) {
    at_reset();
    for (int i = 0; i < 200; i++) at_feed('x');
    CHECK_EQ(at_feed('\n'), AT_LINE);
    CHECK_EQ(strlen(at.line), AT_LINE_MAX);
}

static void test_wifi_status(void) {
    CHECK_EQ(at_wifi_status("+CWSTATE:2,\"home\""), 1);
    CHECK_EQ(at_wifi_status("+CWSTATE:4,\"\""), 0);
    CHECK_EQ(at_wifi_status("+CWSTATE:0,\"\""), 0);
    CHECK_EQ(at_wifi_status("STATUS:2"), 1);
    CHECK_EQ(at_wifi_status("STATUS:3"), 1);
    CHECK_EQ(at_wifi_status("STATUS:4"), 1);
    CHECK_EQ(at_wifi_status("STATUS:5"), 0);
    CHECK_EQ(at_wifi_status("WIFI GOT IP"), -1);
}

int main(void) {
    test_results();
    test_prompt_after_ok();
    test_chatter_is_not_a_result();
    test_long_line_truncated();
    test_wifi_status();
    return check_report();
}
