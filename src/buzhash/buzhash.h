#ifndef URLBLOCK_BUZHASH_H
#define URLBLOCK_BUZHASH_H

#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>

extern const size_t buzhash_width;

uint32_t buzhash (const char *);
uint32_t buzhash_update (const char *, uint32_t);

#endif
