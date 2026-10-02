#include "check.h"
#include "dirfile.h"

static dir_t d;

static void parse(const char *s) {
    dir_parse(&d, s, (uint16_t)strlen(s));
}

static void test_basic(void) {
    parse("Fozz,bbs.fozztexx.com,23\nLocal,192.168.1.5,2323\n");
    CHECK_EQ(d.count, 2);
    CHECK_STR(d.e[0].name, "Fozz");
    CHECK_STR(d.e[0].host, "bbs.fozztexx.com");
    CHECK_EQ(d.e[0].port, 23);
    CHECK_EQ(d.e[1].port, 2323);
    CHECK_EQ(d.bad_line, 0);
    CHECK(!d.truncated);
}

static void test_comments_and_blank_lines(void) {
    parse("# my boards\n\n   \nA,a.com,23\n#B,b.com,23\n");
    CHECK_EQ(d.count, 1);
    CHECK_EQ(d.bad_line, 0);
}

static void test_crlf_spaces_no_final_newline(void) {
    parse("  My BBS , host.org , 23 \r\nOther,o.net,1\r\nLast,l.net,65535");
    CHECK_EQ(d.count, 3);
    CHECK_STR(d.e[0].name, "My BBS");
    CHECK_STR(d.e[0].host, "host.org");
    CHECK_EQ(d.e[1].port, 1);
    CHECK_EQ(d.e[2].port, 65535);
}

static void test_bad_lines_skipped(void) {
    parse("Good,g.com,23\n"
          "NoPort,n.com\n"
          "Zero,z.com,0\n"
          "Big,b.com,70000\n"
          "Letters,l.com,23x\n"
          "Space,sp ace.com,23\n"
          "Extra,e.com,23,4\n"
          "This name is far too long for us,t.com,23\n"
          ",empty.com,23\n"
          "Good2,g2.com,24\n");
    CHECK_EQ(d.count, 2);
    CHECK_STR(d.e[1].name, "Good2");
    CHECK_EQ(d.bad_line, 2);
}

static void test_more_than_32_entries(void) {
    static char text[DIR_FILE_MAX];
    char *p = text;
    for (int i = 0; i < 40; i++) {
        memcpy(p, "B,b.com,23\n", 11);
        p += 11;
    }
    dir_parse(&d, text, (uint16_t)(p - text));
    CHECK_EQ(d.count, DIR_MAX);
    CHECK(d.truncated);
}

static void test_roundtrip(void) {
    char out[DIR_FILE_MAX];
    uint16_t n;
    parse("One,one.com,23\nTwo,two.com,6400\n");
    n = dir_serialize(&d, out, sizeof out);
    CHECK_EQ(n, strlen("One,one.com,23\nTwo,two.com,6400\n"));
    CHECK_MEM(out, "One,one.com,23\nTwo,two.com,6400\n", n);
}

static void test_serialize_respects_cap(void) {
    char out[20];
    parse("One,one.com,23\nTwo,two.com,6400\n");
    CHECK_EQ(dir_serialize(&d, out, sizeof out), 15);   /* only the first line fits */
}

static void test_add_delete_valid(void) {
    dir_entry_t e = { "New", "new.org", 23 };
    parse("");
    CHECK_EQ(d.count, 0);
    CHECK(dir_add(&d, &e));
    CHECK_EQ(d.count, 1);
    CHECK(dir_entry_valid(&e, true));
    e.port = 0;
    CHECK(!dir_entry_valid(&e, true));
    e.port = 23;
    e.name[0] = 0;
    CHECK(!dir_entry_valid(&e, true));
    CHECK(dir_entry_valid(&e, false));
    strcpy(e.host, "bad host");
    CHECK(!dir_entry_valid(&e, false));
    parse("A,a.com,1\nB,b.com,2\nC,c.com,3\n");
    dir_delete(&d, 1);
    CHECK_EQ(d.count, 2);
    CHECK_STR(d.e[1].name, "C");
    for (int i = 0; i < DIR_MAX; i++) dir_add(&d, &d.e[0]);
    CHECK_EQ(d.count, DIR_MAX);
    CHECK(!dir_add(&d, &d.e[0]));
}

static void test_name_must_round_trip(void) {
    dir_entry_t e = { "", "test.com", 23 };
    char out[DIR_FILE_MAX];
    uint16_t n;
    /* Invalid: starts with '#' */
    strcpy(e.name, "#hash");
    CHECK(!dir_entry_valid(&e, true));
    /* Invalid: leading space */
    strcpy(e.name, " lead");
    CHECK(!dir_entry_valid(&e, true));
    /* Invalid: trailing space */
    strcpy(e.name, "trail ");
    CHECK(!dir_entry_valid(&e, true));
    /* Invalid: all spaces */
    strcpy(e.name, "   ");
    CHECK(!dir_entry_valid(&e, true));
    /* Valid: normal name */
    strcpy(e.name, "ok name");
    CHECK(dir_entry_valid(&e, true));
    /* Round-trip test: add "My BBS", serialize, parse, verify identical */
    parse("");
    strcpy(e.name, "My BBS");
    CHECK(dir_add(&d, &e));
    CHECK_EQ(d.count, 1);
    n = dir_serialize(&d, out, sizeof out);
    CHECK(n > 0);
    dir_parse(&d, out, n);
    CHECK_EQ(d.count, 1);
    CHECK_STR(d.e[0].name, "My BBS");
}

int main(void) {
    test_basic();
    test_comments_and_blank_lines();
    test_crlf_spaces_no_final_newline();
    test_bad_lines_skipped();
    test_more_than_32_entries();
    test_roundtrip();
    test_serialize_respects_cap();
    test_add_delete_valid();
    test_name_must_round_trip();
    return check_report();
}
