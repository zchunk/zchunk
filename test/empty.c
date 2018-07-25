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
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <zck.h>
#include "zck_private.h"
#include "util.h"

static char *checksum="8efaeb8e7b3d51a943353f7e6ca4a22266f18c3ef10478b20d50040f4226015d";

int main (int argc, char *argv[]) {
    /* Create empty zchunk file */
    int out = open("empty.zck", O_WRONLY | O_CREAT | O_TRUNC, 0666);
    if(out < 0) {
        perror("Unable to open empty.zck for writing");
        exit(1);
    }
    zckCtx *zck = zck_create();
    if(zck == NULL)
        exit(1);
    if(!zck_init_write(zck, out)) {
        printf("%s", zck_get_error(zck));
        exit(1);
    }

    if(!zck_close(zck))
        exit(1);
    close(out);
    zck_free(&zck);

    /* Open zchunk file and check that checksum matches */
    int in = open("empty.zck", O_RDONLY);
    if(in < 0) {
        perror("Unable to open empty.zck for reading");
        exit(1);
    }
    /* File should be 101 bytes, but we'll go a bit over */
    char *data = zmalloc(1000);
    if(data == NULL) {
        perror("Unable to allocate 1000 bytes");
        exit(1);
    }
    ssize_t len = read(in, data, 1000);
    if(len < 0) {
        perror("Unable to read from empty.zck");
        exit(1);
    }
    char *cksum = get_hash(data, len, ZCK_HASH_SHA256);
    printf("empty.zck: (SHA-256)%s\n", cksum);
    if(memcmp(cksum, checksum, strlen(cksum)) != 0) {
        printf("Expected checksum: (SHA-256)%s\n", checksum);
        exit(1);
    }

    /* Go back to beginning of file and read data from it */
    if(lseek(in, 0, SEEK_SET) != 0) {
        perror("Unable to seek to beginning of empty.zck");
        exit(1);
    }
    zck = zck_create();
    if(zck == NULL)
        exit(1);
    if(!zck_init_read(zck, in)) {
        printf("%s", zck_get_error(zck));
        exit(1);
    }
    memset(data, 0, 1000);
    len = zck_read(zck, data, 1000);
    if(len > 0) {
        printf("%li bytes read, but file should be empty\n", (long)len);
        exit(1);
    }
    if(!zck_close(zck))
        exit(1);

    return 0;
}
