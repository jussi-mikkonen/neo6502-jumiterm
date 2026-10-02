/* src/fname.h - make received or typed filenames safe for FAT. */
#ifndef FNAME_H
#define FNAME_H

#define FNAME_MAX 64

/* Writes a safe name of at most FNAME_MAX characters (+ NUL) to out. */
void fname_sanitize(const char *in, char *out);

#endif
