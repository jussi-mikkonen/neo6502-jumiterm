/* Classifies ESP-AT output one byte at a time. */
#ifndef ATPARSE_H
#define ATPARSE_H

#include <stdint.h>

#define AT_LINE_MAX 64

typedef enum { AT_MORE = 0, AT_OK, AT_ERROR, AT_PROMPT, AT_LINE } at_result_t;

typedef struct {
    char line[AT_LINE_MAX + 1];
    uint8_t len;
} at_parser_t;

extern at_parser_t at;

void at_reset(void);
/* AT_LINE: at.line holds a complete informational line until the next call. */
at_result_t at_feed(uint8_t b);
/* 1 = Wi-Fi has an IP, 0 = not connected, -1 = not a status line. */
int8_t at_wifi_status(const char *line);

#endif
