#include "check.h"
#include "grid.h"
#include "render_stub.h"

static const char *row(uint8_t y) {
    static char buf[GRID_COLS + 1];
    memcpy(buf, grid.ch[y], GRID_COLS);
    buf[GRID_COLS] = 0;
    for (int i = GRID_COLS - 1; i >= 0 && buf[i] == ' '; i--) buf[i] = 0;
    return buf;
}

static void put_str(const char *s) {
    while (*s) grid_put((uint8_t)*s++);
}

static void fresh(void) {
    grid_reset();
    memset(&render_log, 0, sizeof render_log);
}

static void test_reset_clears_and_homes(void) {
    fresh();
    CHECK_EQ(grid.cx, 0);
    CHECK_EQ(grid.cy, 0);
    CHECK_STR(row(0), "");
    CHECK_EQ(grid.attr[24][79], ATTR_DEFAULT);
    CHECK_EQ(grid.top, 0);
    CHECK_EQ(grid.bottom, 24);
}

static void test_put_advances_and_renders(void) {
    fresh();
    put_str("Hi");
    CHECK_STR(row(0), "Hi");
    CHECK_EQ(grid.cx, 2);
    CHECK_EQ(render_log.cells, 2);
}

static void test_deferred_wrap(void) {
    fresh();
    for (int i = 0; i < 80; i++) grid_put('x');
    CHECK_EQ(grid.cx, 79);
    CHECK_EQ(grid.cy, 0);
    CHECK(grid.wrap_pending);
    grid_cr();
    grid_lf();
    CHECK_EQ(grid.cy, 1);
    grid_put('y');
    CHECK_STR(row(1), "y");
    CHECK_STR(row(2), "");
}

static void test_wrap_on_next_char(void) {
    fresh();
    for (int i = 0; i < 80; i++) grid_put('x');
    grid_put('y');
    CHECK_EQ(grid.cy, 1);
    CHECK_EQ(grid.cx, 1);
    CHECK_STR(row(1), "y");
}

static void test_lf_at_bottom_scrolls(void) {
    fresh();
    put_str("top");
    grid_goto(0, 24);
    put_str("last");
    memset(&render_log, 0, sizeof render_log);
    grid_lf();
    CHECK_EQ(grid.cy, 24);
    CHECK_STR(row(23), "last");
    CHECK_STR(row(24), "");
    CHECK_STR(row(0), "");
    CHECK_EQ(render_log.scroll_ups, 1);
}

static void test_scroll_region(void) {
    fresh();
    grid_set_region(5, 10);
    CHECK_EQ(grid.cx, 0);
    CHECK_EQ(grid.cy, 0);
    grid_goto(0, 5);
    put_str("five");
    grid_goto(0, 10);
    put_str("ten");
    grid_goto(0, 20);
    put_str("twenty");
    grid_goto(0, 10);
    grid_lf();
    CHECK_STR(row(9), "ten");
    CHECK_STR(row(5), "");
    CHECK_STR(row(20), "twenty");
    CHECK_EQ(render_log.last_top, 5);
    CHECK_EQ(render_log.last_bottom, 10);
}

static void test_lf_below_region_does_not_scroll(void) {
    fresh();
    grid_set_region(0, 10);
    grid_goto(0, 24);
    memset(&render_log, 0, sizeof render_log);
    grid_lf();
    CHECK_EQ(grid.cy, 24);
    CHECK_EQ(render_log.scroll_ups, 0);
}

static void test_invalid_region_ignored(void) {
    fresh();
    grid_set_region(10, 5);
    CHECK_EQ(grid.top, 0);
    CHECK_EQ(grid.bottom, 24);
    grid_set_region(0, 30);
    CHECK_EQ(grid.bottom, 24);
}

static void test_goto_clamps(void) {
    fresh();
    grid_goto(200, 200);
    CHECK_EQ(grid.cx, 79);
    CHECK_EQ(grid.cy, 24);
}

static void test_erase_line_modes(void) {
    fresh();
    put_str("abcdef");
    grid_goto(2, 0);
    grid_erase_line(0);
    CHECK_STR(row(0), "ab");
    fresh();
    put_str("abcdef");
    grid_goto(2, 0);
    grid_erase_line(1);
    CHECK_STR(row(0), "   def");
    fresh();
    put_str("abcdef");
    grid_erase_line(2);
    CHECK_STR(row(0), "");
}

static void setup_three_rows(void) {
    fresh();
    put_str("aaaa");
    grid_goto(0, 1);
    put_str("bbbb");
    grid_goto(0, 2);
    put_str("cccc");
    grid_goto(2, 1);
}

static void test_erase_display_modes(void) {
    setup_three_rows();
    grid_erase_display(0);
    CHECK_STR(row(0), "aaaa");
    CHECK_STR(row(1), "bb");
    CHECK_STR(row(2), "");
    setup_three_rows();
    grid_erase_display(1);
    CHECK_STR(row(0), "");
    CHECK_STR(row(1), "   b");
    CHECK_STR(row(2), "cccc");
    setup_three_rows();
    grid_erase_display(2);
    CHECK_STR(row(1), "");
    CHECK_EQ(grid.cx, 0);
    CHECK_EQ(grid.cy, 0);
}

static void test_erase_uses_current_background(void) {
    fresh();
    grid.cur_attr = 0x47;
    grid_erase_display(2);
    CHECK_EQ(grid.attr[0][0], 0x47);
    CHECK_EQ(grid.attr[24][79], 0x47);
}

static void test_erase_chars(void) {
    fresh();
    put_str("abcdef");
    grid_goto(1, 0);
    grid_erase_chars(2);
    CHECK_STR(row(0), "a  def");
    CHECK_EQ(grid.cx, 1);
    grid_goto(78, 0);
    grid_erase_chars(50);
    CHECK_EQ(grid.ch[1][0], ' ');
}

static void test_insert_delete_chars(void) {
    fresh();
    put_str("abcdef");
    grid_goto(1, 0);
    grid_insert_chars(2);
    CHECK_STR(row(0), "a  bcdef");
    grid_delete_chars(2);
    CHECK_STR(row(0), "abcdef");
    grid_goto(78, 0);
    grid_insert_chars(200);
    CHECK_STR(row(0), "abcdef");
    CHECK_EQ(grid.ch[1][0], ' ');
}

static void test_insert_delete_lines(void) {
    fresh();
    put_str("one");
    grid_goto(0, 1);
    put_str("two");
    grid_goto(5, 0);
    grid_insert_lines(1);
    CHECK_STR(row(0), "");
    CHECK_STR(row(1), "one");
    CHECK_STR(row(2), "two");
    CHECK_EQ(grid.cx, 0);
    grid_delete_lines(1);
    CHECK_STR(row(0), "one");
    CHECK_STR(row(1), "two");
}

static void test_tab_and_backspace(void) {
    fresh();
    grid_tab();
    CHECK_EQ(grid.cx, 8);
    grid_goto(78, 0);
    grid_tab();
    CHECK_EQ(grid.cx, 79);
    grid_bs();
    CHECK_EQ(grid.cx, 78);
    grid_goto(0, 0);
    grid_bs();
    CHECK_EQ(grid.cx, 0);
}

static void test_reverse_index_at_top_scrolls_down(void) {
    fresh();
    put_str("first");
    grid_goto(0, 0);
    grid_ri();
    CHECK_STR(row(0), "");
    CHECK_STR(row(1), "first");
    CHECK_EQ(render_log.scroll_downs, 1);
}

static void test_save_restore(void) {
    fresh();
    grid_goto(10, 5);
    grid_save_cursor();
    grid_goto(0, 0);
    grid_restore_cursor();
    CHECK_EQ(grid.cx, 10);
    CHECK_EQ(grid.cy, 5);
}

static void test_redraw_draws_every_cell(void) {
    fresh();
    put_str("hi");
    memset(&render_log, 0, sizeof render_log);
    grid_redraw();
    CHECK_EQ(render_log.cells, GRID_COLS * GRID_ROWS);
    CHECK_EQ(render_log.fills, 0);
}

int main(void) {
    test_redraw_draws_every_cell();
    test_reset_clears_and_homes();
    test_put_advances_and_renders();
    test_deferred_wrap();
    test_wrap_on_next_char();
    test_lf_at_bottom_scrolls();
    test_scroll_region();
    test_lf_below_region_does_not_scroll();
    test_invalid_region_ignored();
    test_goto_clamps();
    test_erase_line_modes();
    test_erase_display_modes();
    test_erase_uses_current_background();
    test_erase_chars();
    test_insert_delete_chars();
    test_insert_delete_lines();
    test_tab_and_backspace();
    test_reverse_index_at_top_scrolls_down();
    test_save_restore();
    return check_report();
}
