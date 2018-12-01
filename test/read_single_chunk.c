/*
 * Copyright 2018 Jonathan Dieter <jdieter@gmail.com>
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

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <zck.h>
#include "zck_private.h"
#include "util.h"

char *echecksum =
    "31367eeea6aa48903f2b167149c468d85c4a5e0262b8b52a605e12abb174a18b";

int main (int argc, char *argv[]) {
    /* Open zchunk file and verify second checksum */
    int in = open(argv[1], O_RDONLY);
    if(in < 0) {
        perror("Unable to open LICENSE.dict.fodt.zck for reading");
        exit(1);
    }

    zckCtx *zck = zck_create();
    if(zck == NULL)
        exit(1);
    if(!zck_init_read(zck, in)) {
        printf("%s", zck_get_error(zck));
        zck_free(&zck);
        exit(1);
    }
    zckChunk *chunk = zck_get_first_chunk(zck);
    if(chunk == NULL) {
        printf("%s", zck_get_error(zck));
        zck_free(&zck);
        exit(1);
    }
    chunk = zck_get_next_chunk(chunk);
    if(chunk == NULL) {
        printf("%s", zck_get_error(zck));
        zck_free(&zck);
        exit(1);
    }
    ssize_t chunk_size = zck_get_chunk_size(chunk);
    if(chunk_size < 0) {
        printf("%s", zck_get_error(zck));
        zck_free(&zck);
        exit(1);
    }
    char *data = calloc(chunk_size, 1);
    ssize_t read_size = zck_get_chunk_data(chunk, data, chunk_size);
    if(read_size != chunk_size) {
        if(read_size < 0)
            printf("%s", zck_get_error(zck));
        else
            printf("chunk size didn't match expected size: %li != %li\n",
                   read_size, chunk_size);
        free(data);
        zck_free(&zck);
        exit(1);
    }
    char *cksum = get_hash(data, chunk_size, ZCK_HASH_SHA256);
    printf("Calculated checksum: (SHA-256)%s\n", cksum);
    printf("Expected checksum: (SHA-256)%s\n", echecksum);
    if(memcmp(cksum, echecksum, strlen(echecksum)) != 0) {
        free(data);
        free(cksum);
        zck_free(&zck);
        printf("Checksums don't match!\n");
        exit(1);
    }
    free(data);
    free(cksum);
    zck_free(&zck);
}
