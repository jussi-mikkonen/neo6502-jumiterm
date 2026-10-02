/* src/neo/dirio.c */
#include <neo/file.h>
#include <neo/system.h>
#include "dirio.h"
#include "../config.h"

static char filebuf[DIR_FILE_MAX];

void dirio_load(dir_t *d) {
    uint16_t n;
    neo_file_open(0, DIR_FILE, NEO_FILE_READ);
    if (neo_api_error()) {
        dir_parse(d, filebuf, 0);   /* no file yet: empty directory */
        return;
    }
    n = neo_file_read(0, filebuf, sizeof filebuf);
    neo_file_close(0);
    dir_parse(d, filebuf, n);
}

bool dirio_save(const dir_t *d) {
    uint16_t n = dir_serialize(d, filebuf, sizeof filebuf);
    neo_file_store(DIR_FILE, filebuf, n);
    return neo_api_error() == 0;
}
