#include <neo6502.h>
#include "api.h"

_Static_assert(sizeof(blit_area_t) == 12, "blit_area_t must match the firmware layout");

static void wait_idle(void) {
    while (ControlPort.command) {
    }
}

void api_call(uint8_t group, uint8_t function) {
    /* The firmware reads and writes 6502 RAM (blitter descriptors, buffers) by
       address, which the compiler cannot see: settle memory before triggering
       the call and re-read it after completion. */
    __asm__ volatile("" ::: "memory");
    ControlPort.function = function;
    ControlPort.command = group;
    wait_idle();
    __asm__ volatile("" ::: "memory");
}

void api_blit_image(uint8_t action, const blit_area_t *src, int16_t x, int16_t y) {
    wait_idle();
    ControlPort.params[0] = action;
    ControlPort.params[1] = (uint16_t)src & 0xFF;
    ControlPort.params[2] = (uint16_t)src >> 8;
    ControlPort.params[3] = (uint16_t)x & 0xFF;
    ControlPort.params[4] = (uint16_t)x >> 8;
    ControlPort.params[5] = (uint16_t)y & 0xFF;
    ControlPort.params[6] = (uint16_t)y >> 8;
    ControlPort.params[7] = 0;   /* write whole framebuffer bytes */
    api_call(12, 4);
}

void api_blit_complex(uint8_t action, const blit_area_t *src, const blit_area_t *dst) {
    wait_idle();
    ControlPort.params[0] = action;
    ControlPort.params[1] = (uint16_t)src & 0xFF;
    ControlPort.params[2] = (uint16_t)src >> 8;
    ControlPort.params[3] = (uint16_t)dst & 0xFF;
    ControlPort.params[4] = (uint16_t)dst >> 8;
    api_call(12, 3);
}

void api_uart_set_baud(uint32_t baud) {
    wait_idle();
    ControlPort.params[0] = baud & 0xFF;
    ControlPort.params[1] = (baud >> 8) & 0xFF;
    ControlPort.params[2] = (baud >> 16) & 0xFF;
    ControlPort.params[3] = (baud >> 24) & 0xFF;
    ControlPort.params[4] = 0;   /* 8N1 */
    api_call(10, 15);
}

int16_t api_uart_getc(void) {
    wait_idle();
    api_call(10, 17);
    if (ControlPort.error) return -1;
    return ControlPort.params[0];
}

void api_uart_putc(uint8_t b) {
    wait_idle();
    ControlPort.params[0] = b;
    api_call(10, 16);
}

uint32_t api_ticks(void) {
    wait_idle();
    api_call(1, 1);
    return ControlPort.params[0]
         | ((uint32_t)ControlPort.params[1] << 8)
         | ((uint32_t)ControlPort.params[2] << 16)
         | ((uint32_t)ControlPort.params[3] << 24);
}

uint8_t api_read_key(void) {
    wait_idle();
    api_call(2, 1);
    return ControlPort.params[0];
}

uint8_t api_key_down(uint8_t hid) {
    wait_idle();
    ControlPort.params[0] = hid;
    api_call(1, 2);
    return ControlPort.params[0];
}
