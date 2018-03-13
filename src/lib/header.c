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
#include <zck.h>

#include "zck_private.h"

int zck_read_initial(zckCtx *zck, int src_fd) {
    char header[] = "      ";

    if(!zck_read(src_fd, header, 6))
        return False;

    if(memcmp(header, "\0ZCK1", 5) != 0) {
        zck_log(ZCK_LOG_ERROR,
                "Invalid header, perhaps this is not a zck file?\n");
        return False;
    }

    if(!zck_hash_setup(&(zck->hash_type), header[5]))
        return False;
    zck->preindex_size = 6;

    return True;
}

int zck_read_index_hash(zckCtx *zck, int src_fd) {
    char *header;
    header = zmalloc(zck->hash_type.digest_size);
    if(!header) {
        zck_log(ZCK_LOG_ERROR, "Unable to allocate %lu bytes\n",
                zck->hash_type.digest_size);
        return False;
    }
    if(!zck_read(src_fd, header, zck->hash_type.digest_size)) {
        free(header);
        return False;
    }
    zck->index_digest = header;
    zck->preindex_size += zck->hash_type.digest_size;
    return True;
}

int zck_read_comp_type(zckCtx *zck, int src_fd) {
    int8_t comp_type;

    if(!zck_read(src_fd, (char *)&comp_type, 1))
        return False;

    if(!zck_set_compression_type(zck, comp_type))
        return False;
    if(!zck_comp_init(zck))
        return False;

    zck->preindex_size += 1;
    return True;
}

int zck_read_index_size(zckCtx *zck, int src_fd) {
    uint64_t index_size;
    if(!zck_read(src_fd, (char *)&index_size, sizeof(uint64_t)))
        return False;

    zck->comp_index_size = le64toh(index_size);
    zck->preindex_size += sizeof(uint64_t);
    return True;
}

int zck_read_index(zckCtx *zck, int src_fd) {
    char *index = zmalloc(zck->comp_index_size);
    if(!index) {
        zck_log(ZCK_LOG_ERROR, "Unable to allocate %lu bytes\n",
                zck->comp_index_size);
        return False;
    }
    if(!zck_read(src_fd, index, zck->comp_index_size)) {
        free(index);
        return False;
    }
    if(!zck_index_read(zck, index, zck->comp_index_size)) {
        free(index);
        return False;
    }
    free(index);
    return True;
}

int zck_read_header(zckCtx *zck, int src_fd) {
    if(zck == NULL) {
        zck_log(ZCK_LOG_ERROR, "zckCtx not initialized\n");
        return False;
    }
    zck->fd = src_fd;
    if(!zck_read_initial(zck, src_fd))
        return False;
    if(!zck_read_index_hash(zck, src_fd))
        return False;
    if(!zck_read_comp_type(zck, src_fd))
        return False;
    if(!zck_read_index_size(zck, src_fd))
        return False;
    if(!zck_read_index(zck, src_fd))
        return False;
    return True;
}

int zck_write_header(zckCtx *zck) {
    uint64_t index_size;

    if(!zck_write(zck->fd, "\0ZCK1", 5))
        return False;
    if(!zck_write(zck->fd, (const char *)&(zck->hash_type.type), 1))
        return False;
    if(!zck_write(zck->fd, zck->index_digest, zck->hash_type.digest_size))
        return False;
    if(!zck_write(zck->fd, (const char *)&(zck->comp.type), 1))
        return False;
    index_size = htole64(zck->comp_index_size);
    if(!zck_write(zck->fd, (const char *)&index_size, sizeof(uint64_t)))
        return False;
    return True;
}

