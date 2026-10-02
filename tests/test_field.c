/* tests/test_field.c */
#include "check.h"
#include "field.h"

static void test_text_field(void) {
    char buf[5] = "";
    field_t f = { buf, 4, FIELD_TEXT };
    CHECK(field_key(&f, 'a'));
    CHECK(field_key(&f, ' '));
    CHECK(!field_key(&f, ','));
    CHECK(field_key(&f, 'b'));
    CHECK(field_key(&f, 'c'));
    CHECK(!field_key(&f, 'd'));   /* full */
    CHECK_STR(buf, "a bc");
    CHECK(field_key(&f, 0x08));
    CHECK_STR(buf, "a b");
    CHECK(!field_key(&f, 0x17));  /* control codes ignored */
}

static void test_backspace_on_empty(void) {
    char buf[3] = "";
    field_t f = { buf, 2, FIELD_TEXT };
    CHECK(!field_key(&f, 0x08));
    CHECK_STR(buf, "");
}

static void test_host_field(void) {
    char buf[20] = "";
    field_t f = { buf, 19, FIELD_HOST };
    const char *ok = "Bbs-1.example.com";
    for (const char *p = ok; *p; p++) CHECK(field_key(&f, (uint8_t)*p));
    CHECK(!field_key(&f, ' '));
    CHECK(!field_key(&f, '"'));
    CHECK(!field_key(&f, ','));
    CHECK_STR(buf, ok);
}

static void test_digits_field(void) {
    char buf[6] = "";
    field_t f = { buf, 5, FIELD_DIGITS };
    CHECK(field_key(&f, '2'));
    CHECK(!field_key(&f, 'x'));
    CHECK(field_key(&f, '3'));
    CHECK_STR(buf, "23");
}

static void test_filename_field(void) {
    char buf[20] = "";
    field_t f = { buf, 19, FIELD_FILENAME };
    const char *ok = "My file,v2.tar.gz";
    for (const char *p = ok; *p; p++) CHECK(field_key(&f, (uint8_t)*p));
    CHECK_STR(buf, ok);
    for (const char *p = "\"*:<>?|/\\"; *p; p++) CHECK(!field_key(&f, (uint8_t)*p));
    CHECK(!field_key(&f, 0x09));
    CHECK(!field_key(&f, 0x81));
    CHECK_STR(buf, ok);
}

int main(void) {
    test_filename_field();
    test_text_field();
    test_backspace_on_empty();
    test_host_field();
    test_digits_field();
    return check_report();
}
