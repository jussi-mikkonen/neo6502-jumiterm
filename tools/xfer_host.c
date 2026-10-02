/* tools/xfer_host.c - runs src/xmodem.c over stdin/stdout on the PC, for
   interop tests against lrzsz and for self-loopback tests.

     xfer_host recv x NAME        XMODEM download into NAME
     xfer_host recv y             YMODEM download into the current directory
     xfer_host send x|1k|y FILE...

   The result name (XF_OK, ...) is printed on stderr; the exit status is 0
   only for XF_OK. */
#define _DEFAULT_SOURCE   /* usleep */
#include <poll.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include "xmodem.h"

static char **files;
static FILE *cur;
static char cur_name[FNAME_MAX + 1];
static bool cur_writing;
static uint8_t inbuf[4096];
static ssize_t in_len, in_pos;

static int16_t h_getc(uint16_t timeout) {
    if (in_pos < in_len) return inbuf[in_pos++];
    struct pollfd p = { 0, POLLIN, 0 };
    if (poll(&p, 1, timeout * 10) <= 0) return -1;
    in_len = read(0, inbuf, sizeof inbuf);
    in_pos = 0;
    if (in_len <= 0) {
        in_len = 0;
        usleep(timeout * 10000);   /* peer closed: behave like silence */
        return -1;
    }
    return inbuf[in_pos++];
}

static void h_write(const uint8_t *p, uint16_t n) {
    while (n) {
        ssize_t w = write(1, p, n);
        if (w <= 0) return;
        p += w;
        n -= (uint16_t)w;
    }
}

static bool h_cancel(void) { return false; }

static xf_open_t h_open_write(const char *name, int32_t size) {
    (void)size;
    snprintf(cur_name, sizeof cur_name, "%s", name);
    cur = fopen(name, "wb");
    cur_writing = true;
    return cur ? OPEN_OK : OPEN_FAIL;
}

static bool h_open_read(uint8_t index, char *name, uint32_t *size) {
    struct stat st;
    const char *base = strrchr(files[index], '/');
    if (stat(files[index], &st) != 0) return false;
    cur = fopen(files[index], "rb");
    cur_writing = false;
    snprintf(name, FNAME_MAX + 1, "%s", base ? base + 1 : files[index]);
    *size = (uint32_t)st.st_size;
    return cur != NULL;
}

static int16_t h_read(uint8_t *p, uint16_t n) {
    size_t r = fread(p, 1, n, cur);
    return ferror(cur) ? -1 : (int16_t)r;
}

static bool h_write_file(const uint8_t *p, uint16_t n) {
    return fwrite(p, 1, n, cur) == n;
}

static void h_close(bool keep) {
    if (cur) fclose(cur);
    cur = NULL;
    if (cur_writing && !keep) remove(cur_name);
}

static void h_progress(uint32_t bytes, int32_t total, uint8_t errors, uint8_t file_no) {
    (void)bytes; (void)total; (void)errors; (void)file_no;
}

static const xfer_io_t host_io = {
    h_getc, h_write, h_cancel, h_open_write, h_open_read, h_read, h_write_file, h_close, h_progress
};

static const char *names[] = { "XF_OK", "XF_CANCELLED", "XF_REMOTE_CANCEL", "XF_TIMEOUT", "XF_ERRORS", "XF_FILE_ERROR" };

int main(int argc, char **argv) {
    xf_result_t r;
    xf_proto_t p;
    if (argc < 3) {
        fprintf(stderr, "usage: xfer_host recv x NAME | recv y | send x|1k|y FILE...\n");
        return 2;
    }
    p = !strcmp(argv[2], "y") ? XP_YMODEM : !strcmp(argv[2], "1k") ? XP_XMODEM_1K : XP_XMODEM;
    if (!strcmp(argv[1], "recv")) {
        r = xf_receive(&host_io, p, argc > 3 ? argv[3] : "");
    } else {
        files = argv + 3;
        r = xf_send(&host_io, p, (uint8_t)(argc - 3));
    }
    fprintf(stderr, "%s files=%u bytes=%lu\n", names[r], xf_stats.files, (unsigned long)xf_stats.bytes);
    return r == XF_OK ? 0 : 1;
}
