/* Status bar, BBS directory list and entry form. */
#ifndef UI_H
#define UI_H

#include <stdbool.h>
#include <stdint.h>
#include "../dirfile.h"

#define A_TITLE 0x4F   /* bright white on blue */
#define A_TEXT  0x07
#define A_SEL   0x70   /* black on grey */
#define A_HELP  0x06   /* cyan */
#define A_ERR   0x09   /* bright red */

typedef enum { UI_CONNECT, UI_QUIT } ui_action_t;

void ui_status(const char *left, const char *right);
void ui_status_clock(uint8_t x, const char *hms);
uint8_t ui_wait_key(void);
/* Full-screen pages: clears rows 0-24, title on row 0, help on row 24. */
void ui_frame(const char *title, const char *help);
/* Moves *top so that row sel of a list shows in rows lines; true if it moved. */
bool ui_scroll(uint8_t sel, uint8_t *top, uint8_t rows);
ui_action_t ui_directory(dir_t *d, dir_entry_t *chosen, const char *message);

/* File transfer prompts and progress, all in the status rows (25-29). */
uint8_t ui_ask(const char *prompt);   /* prompt in the status bar; returns the key, lowercased */
bool ui_input(const char *prompt, char *buf, uint8_t cap, uint8_t kind);   /* false on Esc */
void ui_progress(const char *line1, const char *right1, const char *line2, const char *right2);
void ui_note(const char *msg);        /* a red line below the status bar */

#endif
