/* src/neo/dirio.h - BBS.DIR through the Neo file API. */
#ifndef DIRIO_H
#define DIRIO_H

#include <stdbool.h>
#include "../dirfile.h"

void dirio_load(dir_t *d);
bool dirio_save(const dir_t *d);

#endif
