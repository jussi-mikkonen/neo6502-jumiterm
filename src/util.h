/* src/util.h - small formatting helpers (no printf on the 6502). */
#ifndef UTIL_H
#define UTIL_H

#include <stdint.h>

char *util_u16(char *out, uint16_t v);
char *util_u32(char *out, uint32_t v);
/* bytes as kilobytes with one decimal, e.g. "48.0"; returns the end. */
char *util_kb(char *out, uint32_t bytes);
uint8_t util_percent(uint32_t part, uint32_t whole);   /* 0..100; 100 when whole is 0 */
void util_hms(char *out, uint32_t seconds);
uint8_t util_lower(uint8_t c);   /* ASCII A-Z to a-z */

#endif
