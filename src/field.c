/* src/field.c */
#include <string.h>
#include "field.h"

bool field_allowed(uint8_t kind, uint8_t c) {
    switch (kind) {
    case FIELD_DIGITS:
        return c >= '0' && c <= '9';
    case FIELD_FILENAME:
        return c >= 0x20 && c < 0x7F && !strchr("\"*:<>?|/\\", c);
    case FIELD_HOST:
        return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
               (c >= '0' && c <= '9') || c == '.' || c == '-';
    default:
        return c >= 0x20 && c < 0x7F && c != ',';
    }
}

bool field_key(field_t *f, uint8_t key) {
    uint8_t len = (uint8_t)strlen(f->buf);
    if (key == 0x08) {
        if (len == 0) return false;
        f->buf[len - 1] = 0;
        return true;
    }
    if (!field_allowed(f->kind, key) || len >= f->cap) return false;
    f->buf[len] = (char)key;
    f->buf[len + 1] = 0;
    return true;
}
