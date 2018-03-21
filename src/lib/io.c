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

int zck_read(int fd, char *data, size_t length) {
    if(length == 0)
        return True;
    if(data == NULL) {
        zck_log(ZCK_LOG_ERROR, "Unable to read to NULL data pointer\n");
        return False;
    }
    ssize_t read_bytes = read(fd, data, length);
    if(read_bytes == -1) {
        zck_log(ZCK_LOG_ERROR, "Error reading data: %s\n", strerror(errno));
        return False;
    } else if(read_bytes != length) {
        zck_log(ZCK_LOG_ERROR, "Short read\n");
        return False;
    }
    return True;
}

int zck_write(int fd, const char *data, size_t length) {
    if(length == 0)
        return True;
    if(data == NULL) {
        zck_log(ZCK_LOG_ERROR, "Unable to write from NULL data pointer\n");
        return False;
    }
    ssize_t write_bytes = write(fd, data, length);
    if(write_bytes == -1) {
        zck_log(ZCK_LOG_ERROR, "Error write data: %s\n", strerror(errno));
        return False;
    } else if(write_bytes != length) {
        zck_log(ZCK_LOG_ERROR, "Short write\n");
        return False;
    }
    return True;
}

int zck_write_comp_size(int fd, size_t val) {
    char data[sizeof(size_t)*2] = {0};
    size_t length = 0;
    if(!zck_compint_from_size(data, val, &length))
        return False;
    return zck_write(fd, data, length);
}

int zck_read_comp_size(int fd, size_t *val, size_t *length) {
    char data[MAX_COMP_SIZE] = {0};
    int i=0;
    for(char c=zck_read(fd, data+i, 1); c < 128 && i < MAX_COMP_SIZE;
        i++,c=zck_read(fd, data+i, 1));
    if(i == MAX_COMP_SIZE && data[i] < 128) {
        zck_log(ZCK_LOG_ERROR, "Number too large\n");
        *val = 0;
        return False;
    }
    return !zck_compint_to_size(val, data, length);
}

int zck_seek(int fd, off_t offset, int whence) {
    if(lseek(fd, offset, whence) == -1) {
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
        zck_log(ZCK_LOG_ERROR, "Unable to seek to %lu %s: %s\n", offset, wh_str,
                strerror(errno));
        return False;
    }
    return True;
}

int zck_chunks_from_temp(zckCtx *zck) {
    int read_count;
    char *data = zmalloc(BUF_SIZE);
    if(data == NULL)
        return False;

    if(lseek(zck->temp_fd, 0, SEEK_SET) == -1)
        return False;

    while((read_count = read(zck->temp_fd, data, BUF_SIZE)) > 0) {
        if(read_count == -1 || !zck_write(zck->fd, data, read_count)) {
            free(data);
            return False;
        }
    }
    free(data);
    return True;
}
