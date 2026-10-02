#include "check.h"
#include "fname.h"

static const char *clean(const char *in) {
    static char out[FNAME_MAX + 1];
    memset(out, '#', sizeof out);
    fname_sanitize(in, out);
    return out;
}

static void test_plain_name_unchanged(void) {
    CHECK_STR(clean("FILES.ZIP"), "FILES.ZIP");
    CHECK_STR(clean("my long file name.tar.gz"), "my long file name.tar.gz");
}

static void test_path_is_dropped(void) {
    CHECK_STR(clean("/pub/files/demo.zip"), "demo.zip");
    CHECK_STR(clean("C:\\DOS\\EDIT.COM"), "EDIT.COM");
    CHECK_STR(clean("a/b\\c.txt"), "c.txt");
}

static void test_bad_characters_replaced(void) {
    CHECK_STR(clean("what?.txt"), "what_.txt");
    CHECK_STR(clean("a\"b*c:d<e>f|g"), "a_b_c_d_e_f_g");
    CHECK_STR(clean("tab\there"), "tab_here");
    CHECK_STR(clean("caf\xe9.txt"), "caf_.txt");
}

static void test_leading_dots_and_spaces_removed(void) {
    CHECK_STR(clean("..hidden"), "hidden");
    CHECK_STR(clean("  . x.txt"), "x.txt");
    CHECK_STR(clean("../../etc/passwd"), "passwd");
}

static void test_empty_becomes_default(void) {
    CHECK_STR(clean(""), "DOWNLOAD.BIN");
    CHECK_STR(clean("dir/"), "DOWNLOAD.BIN");
    CHECK_STR(clean("..."), "DOWNLOAD.BIN");
}

static void test_long_name_keeps_extension(void) {
    char in[100], want[FNAME_MAX + 1];
    memset(in, 'a', 90);
    strcpy(in + 90, ".zip");
    memset(want, 'a', FNAME_MAX - 4);
    strcpy(want + FNAME_MAX - 4, ".zip");
    CHECK_STR(clean(in), want);
    CHECK_EQ(strlen(clean(in)), FNAME_MAX);
}

static void test_long_name_with_long_extension_is_cut(void) {
    char in[100], want[FNAME_MAX + 1];
    memset(in, 'b', 70);
    strcpy(in + 70, ".verylongext");
    memcpy(want, in, FNAME_MAX);
    want[FNAME_MAX] = 0;
    CHECK_STR(clean(in), want);
}

static void test_exactly_max_is_kept(void) {
    char in[FNAME_MAX + 1];
    memset(in, 'c', FNAME_MAX);
    in[FNAME_MAX] = 0;
    CHECK_STR(clean(in), in);
}

int main(void) {
    test_plain_name_unchanged();
    test_path_is_dropped();
    test_bad_characters_replaced();
    test_leading_dots_and_spaces_removed();
    test_empty_becomes_default();
    test_long_name_keeps_extension();
    test_long_name_with_long_extension_is_cut();
    test_exactly_max_is_kept();
    return check_report();
}
