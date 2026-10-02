/* src/field.h - single-line text field editing rules. */
#ifndef FIELD_H
#define FIELD_H

#include <stdbool.h>
#include <stdint.h>

typedef enum { FIELD_TEXT, FIELD_HOST, FIELD_DIGITS, FIELD_FILENAME } field_kind_t;

typedef struct {
    char *buf;       /* NUL-terminated, room for cap characters + NUL */
    uint8_t cap;
    uint8_t kind;    /* field_kind_t */
} field_t;

bool field_allowed(uint8_t kind, uint8_t c);   /* may c be typed into a field of this kind? */
bool field_key(field_t *f, uint8_t key);

#endif
