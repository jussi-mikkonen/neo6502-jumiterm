/* src/neo/picker.h - full-screen file chooser for uploads. */
#ifndef PICKER_H
#define PICKER_H

#include <stdbool.h>
#include <stdint.h>

#define PICKER_MAX 100

/* Lets the user choose files in the current directory or below it. With
   multi, Space marks several files. Fills chosen with pointers to the
   names (valid until the next picker_run) and returns how many; 0 when
   cancelled. The names are relative to the directory the picker ended in,
   which stays current until picker_restore. Draws over terminal rows 0-24. */
uint8_t picker_run(bool multi, const char **chosen);
/* Returns to the directory that was current before picker_run, so that
   downloads and BBS.DIR stay where they were. */
void picker_restore(void);

#endif
