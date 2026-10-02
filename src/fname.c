/* src/fname.c */
#include <string.h>
#include "fname.h"

#define EXT_MAX 8   /* longest extension kept when a name is shortened */

static char safe(char c) {
    unsigned char u = (unsigned char)c;
    if (u < 0x20 || u >= 0x7F || strchr("\"*:<>?|", c)) return '_';
    return c;
}

void fname_sanitize(const char *in, char *out) {
    const char *s = in, *dot;
    size_t len, keep, ext = 0, n = 0;
    for (const char *p = in; *p; p++)
        if (*p == '/' || *p == '\\') s = p + 1;
    while (*s == '.' || *s == ' ') s++;
    len = strlen(s);
    keep = len;
    if (len > FNAME_MAX) {
        dot = strrchr(s, '.');
        if (dot && (size_t)(len - (size_t)(dot - s)) <= EXT_MAX + 1) ext = len - (size_t)(dot - s);
        keep = FNAME_MAX - ext;
    }
    for (size_t i = 0; i < keep; i++) out[n++] = safe(s[i]);
    for (size_t i = len - ext; i < len; i++) out[n++] = safe(s[i]);
    out[n] = 0;
    if (n == 0) strcpy(out, "DOWNLOAD.BIN");
}
