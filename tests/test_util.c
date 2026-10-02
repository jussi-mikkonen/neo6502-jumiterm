#include "check.h"
#include "util.h"

int main(void) {
    char buf[16];
    char *end;

    end = util_u16(buf, 0);
    CHECK_STR(buf, "0");
    CHECK_EQ(end - buf, 1);
    end = util_u16(buf, 65535);
    CHECK_STR(buf, "65535");
    CHECK_EQ(end - buf, 5);
    util_u16(buf, 23);
    CHECK_STR(buf, "23");

    end = util_u32(buf, 4294967295UL);
    CHECK_STR(buf, "4294967295");
    CHECK_EQ(end - buf, 10);
    util_u32(buf, 0);
    CHECK_STR(buf, "0");
    util_u32(buf, 70000);
    CHECK_STR(buf, "70000");

    util_kb(buf, 0);
    CHECK_STR(buf, "0.0");
    util_kb(buf, 49152);
    CHECK_STR(buf, "48.0");
    end = util_kb(buf, 217600);       /* 212.5 KB */
    CHECK_STR(buf, "212.5");
    CHECK_EQ(end - buf, 5);
    util_kb(buf, 1023);
    CHECK_STR(buf, "0.9");

    CHECK_EQ(util_percent(0, 100), 0);
    CHECK_EQ(util_percent(23, 100), 23);
    CHECK_EQ(util_percent(5, 0), 100);
    CHECK_EQ(util_percent(200, 100), 100);
    CHECK_EQ(util_percent(1000000000UL, 4000000000UL), 25);

    util_hms(buf, 3725);
    CHECK_STR(buf, "01:02:05");
    util_hms(buf, 0);
    CHECK_STR(buf, "00:00:00");
    util_hms(buf, 360000);   /* 100 hours wraps */
    CHECK_STR(buf, "00:00:00");
    return check_report();
}
