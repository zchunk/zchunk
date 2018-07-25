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
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <zck.h>

#include "zck_private.h"

ssize_t read_data(zckCtx *zck, char *data, size_t length) {
    VALIDATE_READ_INT(zck);

    if(length == 0)
        return 0;
    if(data == NULL) {
        set_error(zck, "Unable to read to NULL data pointer");
        return -1;
    }
    ssize_t read_bytes = read(zck->fd, data, length);
    if(read_bytes == -1) {
        set_error(zck, "Error reading data: %s", strerror(errno));
        return -1;
    }
    return read_bytes;
}

int write_data(zckCtx *zck, int fd, const char *data, size_t length) {
    VALIDATE_INT(zck);

    if(length == 0)
        return True;
    if(data == NULL) {
        set_error(zck, "Unable to write from NULL data pointer");
        return False;
    }
    ssize_t write_bytes = write(fd, data, length);
    if(write_bytes == -1) {
        set_error(zck, "Error write data: %s", strerror(errno));
        return False;
    } else if(write_bytes != length) {
        set_fatal_error(zck, "Short write");
        return False;
    }
    return True;
}

int seek_data(zckCtx *zck, off_t offset, int whence) {
    VALIDATE_INT(zck);

    if(lseek(zck->fd, offset, whence) == -1) {
        char *wh_str = NULL;

        if(whence == SEEK_CUR) {
            wh_str = "from current position";
        } else if(whence == SEEK_END) {
            wh_str = "from end of file";
        } else if(whence == SEEK_SET) {
            wh_str = "from beginning of file";
        } else {
            wh_str = "using unknown measurement";
        }
        set_error(zck, "Unable to seek to %lu %s: %s", offset, wh_str,
                  strerror(errno));
        return False;
    }
    return True;
}

ssize_t tell_data(zckCtx *zck) {
    ssize_t loc = lseek(zck->fd, 0, SEEK_CUR);
    return loc;
}

int chunks_from_temp(zckCtx *zck) {
    int read_count;
    char *data = zmalloc(BUF_SIZE);
    if(data == NULL)
        return False;

    if(lseek(zck->temp_fd, 0, SEEK_SET) == -1)
        return False;

    while((read_count = read(zck->temp_fd, data, BUF_SIZE)) > 0) {
        if(read_count == -1 || !write_data(zck, zck->fd, data, read_count)) {
            free(data);
            return False;
        }
    }
    free(data);
    return True;
}
