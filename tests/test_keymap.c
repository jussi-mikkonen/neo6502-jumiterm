/* tests/test_keymap.c */
#include "check.h"
#include "keymap.h"

static void test_arrows_when_nav_key_down(void) {
    uint8_t out[4];
    CHECK_EQ(keymap_translate(0x17, true, out), 3);
    CHECK_MEM(out, "\033[A", 3);
    CHECK_EQ(keymap_translate(0x13, true, out), 3);
    CHECK_MEM(out, "\033[B", 3);
    CHECK_EQ(keymap_translate(0x04, true, out), 3);
    CHECK_MEM(out, "\033[C", 3);
    CHECK_EQ(keymap_translate(0x01, true, out), 3);
    CHECK_MEM(out, "\033[D", 3);
    CHECK_EQ(keymap_translate(0x12, true, out), 4);
    CHECK_MEM(out, "\033[5~", 4);
    CHECK_EQ(keymap_translate(0x1A, true, out), 1);
    CHECK_EQ(out[0], 0x7F);
}

static void test_ctrl_letters_when_nav_key_up(void) {
    uint8_t out[4];
    CHECK_EQ(keymap_translate(0x01, false, out), 1);
    CHECK_EQ(out[0], 0x01);
    CHECK_EQ(keymap_translate(0x17, false, out), 1);
    CHECK_EQ(out[0], 0x17);
}

static void test_plain_keys(void) {
    uint8_t out[4];
    CHECK_EQ(keymap_translate('a', false, out), 1);
    CHECK_EQ(out[0], 'a');
    CHECK_EQ(keymap_translate(0x0D, false, out), 1);
    CHECK_EQ(out[0], 0x0D);
    CHECK_EQ(keymap_translate(0x1B, false, out), 1);
    CHECK_EQ(out[0], 0x1B);
    CHECK_EQ(keymap_translate(0x08, false, out), 1);
    CHECK_EQ(out[0], 0x08);
    CHECK_EQ(keymap_translate(0, false, out), 0);
    CHECK_EQ(keymap_translate(KEY_LOCAL_MENU, false, out), 0);
}

static void test_nav_hid_codes(void) {
    CHECK_EQ(keymap_nav_hid(0x01), 0x50);
    CHECK_EQ(keymap_nav_hid(0x04), 0x4F);
    CHECK_EQ(keymap_nav_hid(0x17), 0x52);
    CHECK_EQ(keymap_nav_hid(0x13), 0x51);
    CHECK_EQ(keymap_nav_hid(0x1A), 0x4C);
    CHECK_EQ(keymap_nav_hid('a'), 0);
    CHECK_EQ(keymap_nav_hid(0x0D), 0);
}

int main(void) {
    test_arrows_when_nav_key_down();
    test_ctrl_letters_when_nav_key_up();
    test_plain_keys();
    test_nav_hid_codes();
    return check_report();
}
