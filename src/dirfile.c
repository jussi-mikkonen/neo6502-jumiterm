#include <string.h>
#include "dirfile.h"
#include "field.h"
#include "util.h"

static bool is_space(char c) {
    return c == ' ' || c == '\t';
}

/* The same characters the host field of the entry form accepts. */
static bool host_ok(const char *h) {
    for (; *h; h++) {
        if (!field_allowed(FIELD_HOST, (uint8_t)*h)) return false;
    }
    return true;
}

/* Strips spaces and tabs from both ends of s[0..*n); returns the new start. */
static const char *trim(const char *s, uint16_t *n) {
    while (*n && is_space(*s)) {
        s++;
        (*n)--;
    }
    while (*n && is_space(s[*n - 1])) (*n)--;
    return s;
}

/* Copies the trimmed field into dst if it is 1..max characters long. */
static bool copy_field(char *dst, uint8_t max, const char *s, uint16_t n) {
    s = trim(s, &n);
    if (n == 0 || n > max) return false;
    memcpy(dst, s, n);
    dst[n] = 0;
    return true;
}

bool dir_parse_port(const char *s, uint16_t n, uint16_t *port) {
    uint32_t v = 0;
    s = trim(s, &n);
    if (n == 0 || n > 5) return false;
    for (uint16_t i = 0; i < n; i++) {
        if (s[i] < '0' || s[i] > '9') return false;
        v = v * 10 + (uint32_t)(s[i] - '0');
    }
    if (v == 0 || v > 65535) return false;
    *port = (uint16_t)v;
    return true;
}

static bool parse_line(const char *s, uint16_t n, dir_entry_t *e) {
    const char *c1 = memchr(s, ',', n);
    const char *rest, *c2, *p;
    uint16_t n1, nr, n2, np;
    if (!c1) return false;
    n1 = (uint16_t)(c1 - s);
    rest = c1 + 1;
    nr = (uint16_t)(n - n1 - 1);
    c2 = memchr(rest, ',', nr);
    if (!c2) return false;
    n2 = (uint16_t)(c2 - rest);
    p = c2 + 1;
    np = (uint16_t)(nr - n2 - 1);
    if (memchr(p, ',', np)) return false;
    if (!copy_field(e->name, DIR_NAME_MAX, s, n1)) return false;
    if (!copy_field(e->host, DIR_HOST_MAX, rest, n2) || !host_ok(e->host)) return false;
    return dir_parse_port(p, np, &e->port);
}

void dir_parse(dir_t *d, const char *text, uint16_t len) {
    uint16_t i = 0, line = 0;
    d->count = 0;
    d->bad_line = 0;
    d->truncated = false;
    while (i < len) {
        uint16_t start = i, end;
        while (i < len && text[i] != '\n') i++;
        end = i;
        if (i < len) i++;
        line++;
        while (end > start && (text[end - 1] == '\r' || is_space(text[end - 1]))) end--;
        while (start < end && is_space(text[start])) start++;
        if (start == end || text[start] == '#') continue;
        if (d->count == DIR_MAX) {
            d->truncated = true;
            continue;
        }
        if (parse_line(text + start, (uint16_t)(end - start), &d->e[d->count])) d->count++;
        else if (!d->bad_line) d->bad_line = line;
    }
}

uint16_t dir_serialize(const dir_t *d, char *out, uint16_t cap) {
    uint16_t pos = 0;
    for (uint8_t i = 0; i < d->count; i++) {
        char line[DIR_NAME_MAX + DIR_HOST_MAX + 9];
        const dir_entry_t *e = &d->e[i];
        char *p = line;
        uint16_t n;
        strcpy(p, e->name);
        p += strlen(p);
        *p++ = ',';
        strcpy(p, e->host);
        p += strlen(p);
        *p++ = ',';
        p = util_u16(p, e->port);
        *p++ = '\n';
        n = (uint16_t)(p - line);
        if (pos + n > cap) break;
        memcpy(out + pos, line, n);
        pos += n;
    }
    return pos;
}

bool dir_entry_valid(const dir_entry_t *e, bool need_name) {
    size_t nl = strlen(e->name);
    size_t hl = strlen(e->host);
    if (need_name) {
        if (e->name[0] == 0 || strchr(e->name, ',')) return false;
        if (nl > DIR_NAME_MAX) return false;
        if (e->name[0] == '#') return false;
        if (is_space(e->name[0]) || is_space(e->name[nl - 1])) return false;
    }
    if (hl == 0 || hl > DIR_HOST_MAX || !host_ok(e->host)) return false;
    return e->port != 0;
}

bool dir_add(dir_t *d, const dir_entry_t *e) {
    if (d->count >= DIR_MAX) return false;
    d->e[d->count++] = *e;
    return true;
}

void dir_delete(dir_t *d, uint8_t index) {
    if (index >= d->count) return;
    memmove(&d->e[index], &d->e[index + 1], (d->count - index - 1) * sizeof(dir_entry_t));
    d->count--;
}
