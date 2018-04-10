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
#include <stdint.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <zck.h>

#define BLK_SIZE 32768

int main (int argc, char *argv[]) {
    char *out_name;

    zck_set_log_level(ZCK_LOG_DEBUG);

    if(argc != 2) {
        printf("Usage: %s <file>\n", argv[0]);
        exit(1);
    }

    int src_fd = open(argv[1], O_RDONLY);
    if(src_fd < 0) {
        printf("Unable to open %s\n", argv[1]);
        perror("");
        exit(1);
    }

    out_name = malloc(strlen(argv[1]) - 3);
    snprintf(out_name, strlen(argv[1]) - 3, "%s", argv[1]);

    int dst_fd = open(out_name, O_EXCL | O_WRONLY | O_CREAT, 0644);
    if(dst_fd < 0) {
        printf("Unable to open %s", out_name);
        perror("");
        free(out_name);
        exit(1);
    }

    zckCtx *zck = zck_init_read(src_fd);
    if(zck == NULL)
        exit(1);

    char *data = malloc(BLK_SIZE);
    int good_exit = False;
    while(True) {
        ssize_t read = zck_read(zck, data, BLK_SIZE);
        if(read < 0)
            goto error;
        if(read == 0)
            break;
        if(read > BLK_SIZE)
            printf("read: %lu\n", read);
        if(write(dst_fd, data, read) != read) {
            printf("Error writing to %s\n", out_name);
            goto error;
        }
    }
    good_exit = True;
error:
    free(data);
    if(!good_exit)
        unlink(out_name);
    free(out_name);
    zck_close(zck);
    close(src_fd);
    close(dst_fd);
    zck_free(&zck);
    if(!good_exit)
        exit(1);
    exit(0);
}
