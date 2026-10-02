/* Thin wrappers over the Neo6502 API control port for the calls llvm-mos
   does not wrap (blitter) or that we need in a cheaper form (UART, keys,
   timer). Each call waits for the previous command to finish first. */
#ifndef API_H
#define API_H

#include <stdint.h>

#define BLT_COPY  0
#define BLT_SOLID 2

#define BLTFMT_BYTE 0
#define BLTFMT_PAIR 1

/* Same layout as the firmware's struct BlitterArea (blitter.h). */
typedef struct {
    uint16_t address;
    uint8_t page;          /* 0x00 = 6502 RAM, 0x80/0x81 = video RAM */
    uint8_t pad;
    int16_t stride;        /* bytes from one line to the next, may be negative */
    uint8_t format;        /* BLTFMT_* */
    uint8_t transparent;   /* source value that BLT_SOLID skips */
    uint8_t solid;         /* colour written by BLT_SOLID */
    uint8_t height;        /* lines */
    uint16_t width;        /* source values (pixels) per line */
} blit_area_t;

void api_call(uint8_t group, uint8_t function);
void api_blit_image(uint8_t action, const blit_area_t *src, int16_t x, int16_t y);
void api_blit_complex(uint8_t action, const blit_area_t *src, const blit_area_t *dst);
void api_uart_set_baud(uint32_t baud);
int16_t api_uart_getc(void);        /* -1 when nothing is waiting */
void api_uart_putc(uint8_t b);
uint32_t api_ticks(void);           /* 100 Hz system timer */
uint8_t api_read_key(void);         /* 0 when the keyboard queue is empty */
uint8_t api_key_down(uint8_t hid);  /* non-zero while USB HID key 'hid' is held */

#endif
