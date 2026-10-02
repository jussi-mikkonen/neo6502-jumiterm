/* ESP-AT (ESP8266, AT 2.x) driver using transparent mode. */
#ifndef MODEM_H
#define MODEM_H

#include <stdint.h>

typedef enum { MODEM_OK, MODEM_NO_RESPONSE, MODEM_NO_WIFI, MODEM_CONNECT_FAILED } modem_status_t;

modem_status_t modem_init(void);
modem_status_t modem_connect(const char *host, uint16_t port);
uint16_t modem_poll(uint8_t *buf, uint16_t max);
void modem_write(const uint8_t *p, uint16_t n);   /* data bytes, telnet-encoded */
void modem_flush_telnet(void);                    /* sends pending telnet replies */
void modem_hangup(void);
void modem_shutdown(void);   /* restore the ESP's default 115200 baud */

#endif
