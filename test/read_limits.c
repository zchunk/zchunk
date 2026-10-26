/*
 * Copyright 2026 Jonathan Dieter <jonathan@dieter.ie>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 *  1. Redistributions of source code must retain the above copyright notice,
 *     this list of conditions and the following disclaimer.
 *
 *  2. Redistributions in binary form must reproduce the above copyright notice,
 *     this list of conditions and the following disclaimer in the documentation
 *     and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

/* Check that out-of-range values are rejected when reading */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>
#include <limits.h>
#include <unistd.h>
#include <fcntl.h>
#include <zck.h>
#include "zck_private.h"
#include "util.h"

static void fail(const char *msg) {
    printf("ERROR: %s\n", msg);
    exit(1);
}

/* Decode a compressed integer with a fresh context, since a failure leaves
 * the context in an error state */
static bool to_size(const char *compint, size_t len, size_t *val) {
    zckCtx *zck = zck_create();
    if(zck == NULL)
        exit(1);
    size_t length = 0;
    bool ret = compint_to_size(zck, val, compint, &length, len);
    zck_free(&zck);
    return ret;
}

static bool to_int(const char *compint, size_t len, int *val) {
    zckCtx *zck = zck_create();
    if(zck == NULL)
        exit(1);
    size_t length = 0;
    bool ret = compint_to_int(zck, val, compint, &length, len);
    zck_free(&zck);
    return ret;
}

/* Read a file's header with the given maximum chunk size (0 for default) */
static bool read_header(const char *path, ssize_t chunk_max) {
    int in = open(path, O_RDONLY | O_BINARY);
    if(in < 0) {
        perror("Unable to open file for reading");
        exit(1);
    }
    zckCtx *zck = zck_create();
    if(zck == NULL || !zck_init_adv_read(zck, in))
        exit(1);
    if(chunk_max > 0 && !zck_set_ioption(zck, ZCK_CHUNK_MAX, chunk_max))
        fail("unable to set maximum chunk size when reading");
    bool ret = zck_read_lead(zck) && zck_read_header(zck);
    zck_free(&zck);
    close(in);
    return ret;
}

int main(int argc, char *argv[]) {
    if(argc != 2) {
        printf("Usage: %s <zchunk file>\n", argv[0]);
        exit(1);
    }

    /* The largest value still decodes */
    char compint[MAX_COMP_SIZE];
    size_t len = 0;
    size_t val = 0;
    compint_from_size(compint, SIZE_MAX, &len);
    if(!to_size(compint, len, &val) || val != SIZE_MAX)
        fail("SIZE_MAX didn't decode");

    /* A final byte that overflows is rejected */
    memset(compint, 0x7f, MAX_COMP_SIZE - 1);
    compint[MAX_COMP_SIZE - 1] = (char)0xff;
    if(to_size(compint, MAX_COMP_SIZE, &val))
        fail("overflowing compressed integer was accepted");

    /* Values that don't fit in an int are rejected */
    int ival = 0;
    len = 0;
    compint_from_size(compint, INT_MAX, &len);
    if(!to_int(compint, len, &ival) || ival != INT_MAX)
        fail("INT_MAX didn't decode");
    len = 0;
    compint_from_size(compint, (size_t)INT_MAX + 1, &len);
    if(to_int(compint, len, &ival))
        fail("compressed integer larger than INT_MAX was accepted");
#if SIZE_MAX > UINT_MAX
    /* This one truncates to 0 when cast to an int */
    len = 0;
    compint_from_size(compint, (size_t)1 << 32, &len);
    if(to_int(compint, len, &ival))
        fail("compressed integer of 2^32 was accepted");
#endif

    /* Readers can limit the chunk size they'll accept */
    if(!read_header(argv[1], 0))
        fail("unable to read header with default maximum chunk size");
    if(!read_header(argv[1], INT_MAX))
        fail("unable to read header with largest maximum chunk size");
    if(read_header(argv[1], 1))
        fail("chunk larger than maximum chunk size was accepted");

    return 0;
}
