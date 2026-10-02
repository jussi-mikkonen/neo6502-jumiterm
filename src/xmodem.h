/* src/xmodem.h - XMODEM (checksum, CRC, 1K) and YMODEM batch transfers.
   The protocol code only talks to the outside world through xfer_io_t, so
   it runs unchanged on the Neo (modem + telnet + file API) and on the PC
   (tests, tools/xfer_host.c). Times are in 100 Hz ticks. */
#ifndef XMODEM_H
#define XMODEM_H

#include <stdbool.h>
#include <stdint.h>
#include "fname.h"

typedef enum { XF_OK, XF_CANCELLED, XF_REMOTE_CANCEL, XF_TIMEOUT, XF_ERRORS, XF_FILE_ERROR } xf_result_t;
typedef enum { XP_XMODEM, XP_XMODEM_1K, XP_YMODEM } xf_proto_t;
typedef enum { OPEN_OK, OPEN_SKIP, OPEN_FAIL } xf_open_t;

typedef struct {
    int16_t (*getc)(uint16_t timeout);                  /* next byte, or -1 after timeout ticks */
    void (*write)(const uint8_t *p, uint16_t n);
    bool (*cancel)(void);                               /* true once the user pressed Esc */
    xf_open_t (*open_write)(const char *name, int32_t size);   /* size -1 = unknown */
    bool (*open_read)(uint8_t index, char *name, uint32_t *size);   /* name: FNAME_MAX + 1 */
    int16_t (*read)(uint8_t *p, uint16_t n);            /* bytes read (0 at end), -1 on error */
    bool (*write_file)(const uint8_t *p, uint16_t n);
    void (*close)(bool keep);                           /* keep=false deletes a file opened by open_write */
    void (*progress)(uint32_t bytes, int32_t total, uint8_t errors, uint8_t file_no);
} xfer_io_t;

typedef struct {
    uint8_t files;      /* files completed */
    uint32_t bytes;     /* bytes of completed files */
} xf_stats_t;

extern xf_stats_t xf_stats;

uint16_t xf_crc16(const uint8_t *p, uint16_t n);
/* name is the file to create for XMODEM; YMODEM takes names from the sender. */
xf_result_t xf_receive(const xfer_io_t *io, xf_proto_t p, const char *name);
xf_result_t xf_send(const xfer_io_t *io, xf_proto_t p, uint8_t nfiles);

#endif
