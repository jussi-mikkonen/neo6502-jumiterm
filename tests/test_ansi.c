#include "ansi.h"
#include "check.h"
#include "grid.h"

static void feed(const char *s) {
    while (*s) ansi_feed((uint8_t)*s++);
}

static void setup(void) {
    ansi_init();
    grid_reset();
}

static const char *row(uint8_t y) {
    static char buf[GRID_COLS + 1];
    memcpy(buf, grid.ch[y], GRID_COLS);
    buf[GRID_COLS] = 0;
    for (int i = GRID_COLS - 1; i >= 0 && buf[i] == ' '; i--) buf[i] = 0;
    return buf;
}

static void test_text_and_crlf(void) {
    setup();
    feed("Hello\r\nWorld");
    CHECK_STR(row(0), "Hello");
    CHECK_STR(row(1), "World");
}

static void test_cursor_position(void) {
    setup();
    feed("\033[5;10HX");
    CHECK_EQ(grid.ch[4][9], 'X');
    feed("abc\033[HZ");
    CHECK_EQ(grid.ch[0][0], 'Z');
}

static void test_huge_params_clamp(void) {
    setup();
    feed("\033[9999;9999HX");
    CHECK_EQ(grid.ch[24][79], 'X');
    feed("\033[99999999999A");
    CHECK_EQ(grid.cy, 0);
    feed("\033[99999C");
    CHECK_EQ(grid.cx, 79);
}

static void test_too_many_params(void) {
    setup();
    feed("\033[1;2;3;4;5;6;7;8;9;10;11;12mX");
    CHECK_EQ(grid.ch[0][0], 'X');
    feed("Y");
    CHECK_EQ(grid.ch[0][1], 'Y');
}

static void test_relative_moves(void) {
    setup();
    feed("\033[10;10H\033[2A\033[3C\033[1B\033[5D");
    CHECK_EQ(grid.cx, 7);
    CHECK_EQ(grid.cy, 8);
    feed("\033[3G");
    CHECK_EQ(grid.cx, 2);
    feed("\033[4d");
    CHECK_EQ(grid.cy, 3);
}

static void test_sgr_colours(void) {
    setup();
    feed("\033[1;31;44mX");
    CHECK_EQ(grid.attr[0][0], 0x49);
    feed("\033[0mY");
    CHECK_EQ(grid.attr[0][1], 0x07);
    feed("\033[31m\033[mZ");
    CHECK_EQ(grid.attr[0][2], 0x07);
    feed("\033[7mR");
    CHECK_EQ(grid.attr[0][3], 0x70);
    feed("\033[0;5;44mB");
    CHECK_EQ(grid.attr[0][4], 0xC7);
    feed("\033[0;92mG");
    CHECK_EQ(grid.attr[0][5], 0x0A);
}

static void test_extended_colour_skipped(void) {
    setup();
    feed("\033[38;5;196;1mX");
    CHECK_EQ(grid.attr[0][0], 0x0F);
}

static void test_erase(void) {
    setup();
    feed("abc\033[2J");
    CHECK_STR(row(0), "");
    CHECK_EQ(grid.cx, 0);
    feed("abcdef\033[1;3H\033[K");
    CHECK_STR(row(0), "ab");
    feed("\033[44m\033[2J");
    CHECK_EQ(grid.attr[10][10], 0x47);
}

static void test_split_sequence(void) {
    setup();
    feed("\033[");
    feed("3");
    feed("1mX");
    CHECK_EQ(grid.attr[0][0], 0x01);
}

static void test_cp437_and_ignored_controls(void) {
    setup();
    ansi_feed(0xC9);
    CHECK_EQ(grid.ch[0][0], 0xC9);
    feed("a\007\016\017b\177c");
    CHECK_EQ(grid.ch[0][1], 'a');
    CHECK_EQ(grid.ch[0][2], 'b');
    CHECK_EQ(grid.ch[0][3], 'c');
}

static void test_status_reports(void) {
    setup();
    feed("\033[3;7H\033[6n");
    CHECK_EQ(ansi.reply_len, 6);
    CHECK_MEM(ansi.reply, "\033[3;7R", 6);
    ansi.reply_len = 0;
    feed("\033[c");
    CHECK_EQ(ansi.reply_len, 7);
    CHECK_MEM(ansi.reply, "\033[?1;0c", 7);
}

static void test_private_sequences_ignored(void) {
    setup();
    feed("\033[?25lX\033[=7hY");
    CHECK_STR(row(0), "XY");
}

static void test_scroll_region_sequence(void) {
    setup();
    feed("\033[5;10r");
    CHECK_EQ(grid.top, 4);
    CHECK_EQ(grid.bottom, 9);
    feed("\033[r");
    CHECK_EQ(grid.top, 0);
    CHECK_EQ(grid.bottom, 24);
}

static void test_escape_sequences(void) {
    setup();
    feed("\033(BX");
    CHECK_STR(row(0), "X");
    feed("\033[5;5H\033[s\033[1;1H\033[uQ");
    CHECK_EQ(grid.ch[4][4], 'Q');
    feed("\033[1;1H\033" "M");
    CHECK_STR(row(0), "");
    CHECK_STR(row(1), "X");
    feed("\033[31mzz\033" "c");
    CHECK_STR(row(1), "");
    CHECK_EQ(grid.cur_attr, 0x07);
}

static void test_full_line_then_crlf(void) {
    setup();
    for (int i = 0; i < 80; i++) ansi_feed('x');
    feed("\r\ny");
    CHECK_STR(row(1), "y");
    CHECK_EQ(grid.cy, 1);
}

static void test_form_feed_clears(void) {
    setup();
    feed("abc\014d");
    CHECK_STR(row(0), "d");
}

static void test_insert_delete_via_csi(void) {
    setup();
    feed("abcdef\033[1;2H\033[2@");
    CHECK_STR(row(0), "a  bcdef");
    feed("\033[2P");
    CHECK_STR(row(0), "abcdef");
    feed("\033[1;3H\033[2X");
    CHECK_STR(row(0), "ab  ef");
}

static void test_bare_cr_then_text_is_newline(void) {
    setup();
    feed("a\rb");
    CHECK_STR(row(0), "a");
    CHECK_STR(row(1), "b");
}

static void test_crlf_no_double_newline(void) {
    setup();
    feed("a\r\nb");
    CHECK_STR(row(1), "b");
    CHECK_STR(row(2), "");
}

static void test_cr_nul_is_plain_cr(void) {
    setup();
    feed("a\r");
    ansi_feed(0);
    feed("b");
    CHECK_STR(row(0), "b");
    CHECK_STR(row(1), "");
}

static void test_cr_cr_lf(void) {
    setup();
    feed("a\r\r\nb");
    CHECK_STR(row(1), "b");
    CHECK_STR(row(2), "");
}

static void test_cr_then_escape_redraws_in_place(void) {
    setup();
    feed("abc\r\033[Kx");
    CHECK_STR(row(0), "x");
}

static void test_bare_cr_split_across_feeds(void) {
    setup();
    feed("a\r");
    feed("b");
    CHECK_STR(row(1), "b");
}

static void test_commodore_menu(void) {
    setup();
    feed("\rSELECT TERMINAL\rC  COMMODORE 40\r8  COMMODORE 80\rA  ANSI\rP  PLAIN ASCII\rCHOICE  ");
    CHECK_STR(row(0), "");
    CHECK_STR(row(1), "SELECT TERMINAL");
    CHECK_STR(row(2), "C  COMMODORE 40");
    CHECK_STR(row(3), "8  COMMODORE 80");
    CHECK_STR(row(4), "A  ANSI");
    CHECK_STR(row(5), "P  PLAIN ASCII");
    CHECK_STR(row(6), "CHOICE");
}

static void test_cr_inside_csi_does_not_arm(void) {
    setup();
    feed("\033[1\r;1HZ");
    CHECK_EQ(grid.ch[0][0], 'Z');
    CHECK_STR(row(1), "");
}

int main(void) {
    test_text_and_crlf();
    test_cursor_position();
    test_huge_params_clamp();
    test_too_many_params();
    test_relative_moves();
    test_sgr_colours();
    test_extended_colour_skipped();
    test_erase();
    test_split_sequence();
    test_cp437_and_ignored_controls();
    test_status_reports();
    test_private_sequences_ignored();
    test_scroll_region_sequence();
    test_escape_sequences();
    test_full_line_then_crlf();
    test_form_feed_clears();
    test_insert_delete_via_csi();
    test_bare_cr_then_text_is_newline();
    test_crlf_no_double_newline();
    test_cr_nul_is_plain_cr();
    test_cr_cr_lf();
    test_cr_then_escape_redraws_in_place();
    test_bare_cr_split_across_feeds();
    test_commodore_menu();
    test_cr_inside_csi_does_not_arm();
    return check_report();
}
