/* Build-time settings. UART_RATE comes from the measurements in
   docs/superpowers/notes/2026-09-30-throughput.md. */
#ifndef CONFIG_H
#define CONFIG_H

#define UART_RATE 38400     /* plain integer: it is also pasted into AT+UART_CUR */
#define POLL_MAX 256        /* most bytes handled per main-loop pass */
#define DIR_FILE "BBS.DIR"

#endif
