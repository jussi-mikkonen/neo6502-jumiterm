#include <string.h>
#include "atparse.h"

at_parser_t at;

void at_reset(void) {
    at.len = 0;
    at.line[0] = 0;
}

at_result_t at_feed(uint8_t b) {
    if (b == '\r') return AT_MORE;
    if (b == '\n') {
        uint8_t len = at.len;
        at.line[len] = 0;
        at.len = 0;
        if (len == 0) return AT_MORE;
        if (!strcmp(at.line, "OK") || !strcmp(at.line, "SEND OK")) return AT_OK;
        if (!strcmp(at.line, "ERROR") || !strcmp(at.line, "FAIL") || !strcmp(at.line, "SEND FAIL")) return AT_ERROR;
        return AT_LINE;
    }
    if (at.len == 0 && b == '>') return AT_PROMPT;
    if (at.len < AT_LINE_MAX) at.line[at.len++] = (char)b;
    return AT_MORE;
}

int8_t at_wifi_status(const char *line) {
    if (!strncmp(line, "+CWSTATE:", 9)) return line[9] == '2';
    if (!strncmp(line, "STATUS:", 7)) return line[7] >= '2' && line[7] <= '4';
    return -1;
}
