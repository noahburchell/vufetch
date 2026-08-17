#ifndef FETCH_H
#define FETCH_H

#include <stddef.h>

#define ARRAY_LEN(a) (sizeof(a) / sizeof(*(a)))

typedef struct fetch_line {
        const char *label;
        const char *info;
} fetch_line;

[[nodiscard]] int construct(char *out, size_t size, const fetch_line *lines, size_t count);

#endif
