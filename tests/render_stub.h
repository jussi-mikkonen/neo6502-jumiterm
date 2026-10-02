/* tests/render_stub.h - records render calls so tests can check them. */
#ifndef RENDER_STUB_H
#define RENDER_STUB_H

#include <stdint.h>

typedef struct {
    int cells, fills, scroll_ups, scroll_downs;
    uint8_t last_top, last_bottom;
} render_log_t;

extern render_log_t render_log;

#endif
