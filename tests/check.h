/* Minimal helpers for the host-side unit tests. */
#ifndef CHECK_H
#define CHECK_H

#include <stdio.h>
#include <string.h>

static int check_failures;

#define CHECK(cond) do { \
    if (!(cond)) { \
        printf("%s:%d: CHECK(%s) failed\n", __FILE__, __LINE__, #cond); \
        check_failures++; \
    } \
} while (0)

#define CHECK_EQ(actual, expected) do { \
    long a_ = (long)(actual), e_ = (long)(expected); \
    if (a_ != e_) { \
        printf("%s:%d: %s is %ld, expected %ld\n", __FILE__, __LINE__, #actual, a_, e_); \
        check_failures++; \
    } \
} while (0)

#define CHECK_STR(actual, expected) do { \
    const char *a_ = (actual), *e_ = (expected); \
    if (strcmp(a_, e_) != 0) { \
        printf("%s:%d: %s is \"%s\", expected \"%s\"\n", __FILE__, __LINE__, #actual, a_, e_); \
        check_failures++; \
    } \
} while (0)

#define CHECK_MEM(actual, expected, len) do { \
    if (memcmp((actual), (expected), (len)) != 0) { \
        printf("%s:%d: %s differs from expected bytes\n", __FILE__, __LINE__, #actual); \
        check_failures++; \
    } \
} while (0)

static int check_report(void) {
    if (check_failures) {
        printf("FAILED: %d check(s)\n", check_failures);
        return 1;
    }
    printf("ok\n");
    return 0;
}

#endif
