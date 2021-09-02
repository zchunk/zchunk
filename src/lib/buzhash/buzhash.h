#ifndef URLBLOCK_BUZHASH_H
#define URLBLOCK_BUZHASH_H

#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>

typedef struct buzHash {
    uint32_t h;
    int window_size;
    char *window;
    int window_loc;
    int window_fill;
} buzHash;

bool buzhash_update (buzHash *b, const char *s, size_t window, uint32_t *output);
void buzhash_reset (buzHash *b);

#endif
