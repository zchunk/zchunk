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
#include <stdint.h>
#include <unistd.h>
#include <zck.h>

#include "zck_private.h"

#define BLK_SIZE 32768

int zck_read(int fd, char *data, size_t length) {
    if(length == 0)
        return True;

    if(read(fd, data, length) != length) {
        zck_log(ZCK_LOG_ERROR, "Short read\n");
        return False;
    }

    return True;
}

int zck_write(int fd, const char *data, size_t length) {
    if(length == 0)
        return True;

    if(write(fd, data, length) != length)
        return False;
    return True;
}

int zck_chunks_from_temp(zckCtx *zck) {
    int read_count;
    char *data = zmalloc(BLK_SIZE);
    if(data == NULL)
        return False;

    if(lseek(zck->temp_fd, 0, SEEK_SET) == -1)
        return False;

    while((read_count = read(zck->temp_fd, data, BLK_SIZE)) > 0) {
        if(read_count == -1 || !zck_write(zck->fd, data, read_count)) {
            free(data);
            return False;
        }
    }
    free(data);
    return True;
}
