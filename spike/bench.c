/* SPIKE - throwaway. Times cell drawing, scrolling and UART polling on real
   hardware, then offers a raw AT console for checking ESP-AT behaviour. */
#include <stdio.h>
#include <neo/console.h>
#include <neo/system.h>
#include "../src/neo/api.h"
#include "../src/render.h"

#define CELLS   2000
#define SCROLLS 100
#define POLLS   2000

static unsigned long per_second(uint32_t count, uint32_t ticks) {
    if (ticks == 0) ticks = 1;
    return (unsigned long)(count * 100UL / ticks);
}

static void at_console(void) {
    int16_t b;
    uint8_t c;
    neo_console_define_hotkey(1, "\201");   /* F1 -> 0x81 */
    printf("\nAT console at 115200. F1 sends +++, ESC exits.\n");
    for (;;) {
        while ((b = api_uart_getc()) >= 0) {
            if (b == '\n') continue;
            neo_console_write_char((char)b);
        }
        c = api_read_key();
        if (c == 0x1B) return;
        if (c == 0x81) {
            api_uart_putc('+');
            api_uart_putc('+');
            api_uart_putc('+');
        } else if (c == '\r') {
            api_uart_putc('\r');
            api_uart_putc('\n');
        } else if (c) {
            api_uart_putc(c);
        }
    }
}

int main(void) {
    uint32_t t, t1, ts, tp;
    uint16_t i;
    uint8_t c;

    render_init();

    t = api_ticks();
    for (i = 0; i < CELLS; i++) render_cell(i % 80, (i / 80) % 25, 'A' + i % 26, (uint8_t)(i & 0x7F));
    t1 = api_ticks() - t;

    t = api_ticks();
    for (i = 0; i < SCROLLS; i++) render_scroll_up(0, 24, 0x10);
    ts = api_ticks() - t;

    api_uart_set_baud(115200);
    t = api_ticks();
    for (i = 0; i < POLLS; i++) api_uart_getc();
    tp = api_ticks() - t;

    render_shutdown();
    printf("JumiTerm throughput spike\n\n");
    printf("cells:              %lu cells/s\n", per_second(CELLS, t1));
    printf("scroll 80x25:       %lu scrolls/s\n", per_second(SCROLLS, ts));
    printf("uart polls:         %lu polls/s\n", per_second(POLLS, tp));
    printf("\nA = AT console, any other key = reset\n");
    while (!(c = api_read_key())) {
    }
    if (c == 'a' || c == 'A') at_console();
    neo_system_reset();
    return 0;
}
