/* BBS.DIR: one "name,host,port" per line, '#' comments, blank lines ignored. */
#ifndef DIRFILE_H
#define DIRFILE_H

#include <stdbool.h>
#include <stdint.h>

#define DIR_MAX 32
#define DIR_NAME_MAX 24
#define DIR_HOST_MAX 40
#define DIR_FILE_MAX 2560

typedef struct {
    char name[DIR_NAME_MAX + 1];
    char host[DIR_HOST_MAX + 1];
    uint16_t port;
} dir_entry_t;

typedef struct {
    dir_entry_t e[DIR_MAX];
    uint8_t count;
    uint16_t bad_line;   /* first malformed line number (1-based), 0 = none */
    bool truncated;      /* more than DIR_MAX entries in the file */
} dir_t;

void dir_parse(dir_t *d, const char *text, uint16_t len);
uint16_t dir_serialize(const dir_t *d, char *out, uint16_t cap);
/* Port 1-65535 from s[0..n), spaces allowed around it. */
bool dir_parse_port(const char *s, uint16_t n, uint16_t *port);
bool dir_entry_valid(const dir_entry_t *e, bool need_name);
bool dir_add(dir_t *d, const dir_entry_t *e);
void dir_delete(dir_t *d, uint8_t index);

#endif
