/* src/util.c */
#include "util.h"

char *util_u32(char *out, uint32_t v) {
    char tmp[10];
    uint8_t n = 0;
    do {
        tmp[n++] = (char)('0' + v % 10);
        v /= 10;
    } while (v);
    while (n) *out++ = tmp[--n];
    *out = 0;
    return out;
}

char *util_u16(char *out, uint16_t v) {
    return util_u32(out, v);
}

char *util_kb(char *out, uint32_t bytes) {
    out = util_u32(out, bytes / 1024);
    *out++ = '.';
    *out++ = (char)('0' + (bytes % 1024) * 10 / 1024);
    *out = 0;
    return out;
}

uint8_t util_percent(uint32_t part, uint32_t whole) {
    if (whole == 0 || part >= whole) return 100;
    if (whole > 0x1000000UL) return (uint8_t)(part / (whole / 100));
    return (uint8_t)(part * 100 / whole);
}

uint8_t util_lower(uint8_t c) {
    return c >= 'A' && c <= 'Z' ? (uint8_t)(c + 32) : c;
}

static void two_digits(char *out, uint8_t v) {
    out[0] = (char)('0' + v / 10);
    out[1] = (char)('0' + v % 10);
}

void util_hms(char *out, uint32_t seconds) {
    two_digits(out, (uint8_t)((seconds / 3600) % 100));
    out[2] = ':';
    two_digits(out + 3, (uint8_t)((seconds / 60) % 60));
    out[5] = ':';
    two_digits(out + 6, (uint8_t)(seconds % 60));
    out[8] = 0;
}
