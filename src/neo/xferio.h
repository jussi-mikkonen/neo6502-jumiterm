/* src/neo/xferio.h - xfer_io_t on the Neo: modem + telnet for the line,
   Esc for cancel, the file API for files, the status bar for progress. */
#ifndef XFERIO_H
#define XFERIO_H

#include <stdbool.h>
#include <stdint.h>
#include "../xmodem.h"

extern const xfer_io_t xferio;

/* Prepares a transfer and asks the server for telnet BINARY. label heads
   the progress bar ("Download YMODEM"). For uploads, names[0..nfiles) are
   the files to send; for downloads pass 0 and NULL. ask_overwrite makes
   open_write ask before replacing a file (YMODEM downloads). */
void xferio_begin(const char *label, uint8_t nfiles, const char *const *names, bool ask_overwrite);
bool xferio_exists(const char *name);
bool xferio_ask_overwrite(const char *name);   /* "NAME exists - overwrite? y/n" */
uint8_t xferio_file_error(void);               /* firmware error of the last failed file call */

#endif
