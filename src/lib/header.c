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
#include <zck.h>

#include "zck_private.h"


int zck_read_initial(zckCtx *zck, int src_fd) {
    char *header = zmalloc(5 + MAX_COMP_SIZE);
    size_t length = 0;
    if(header == NULL) {
        zck_log(ZCK_LOG_ERROR, "Unable to allocate %lu bytes\n",
                zck->hash_type.digest_size);
        return False;
    }

    zck_log(ZCK_LOG_DEBUG, "Reading magic and hash type\n");
    if(!zck_read(src_fd, header, 5 + MAX_COMP_SIZE)) {
        free(header);
        return False;
    }

    if(memcmp(header, "\0ZCK1", 5) != 0) {
        free(header);
        zck_log(ZCK_LOG_ERROR,
                "Invalid header, perhaps this is not a zck file?\n");
        return False;
    }
    length += 5;

    int hash_type = 0;
    if(!zck_compint_to_int(&hash_type, header+length, &length))
        return False;
    if(!zck_hash_setup(&(zck->hash_type), hash_type))
        return False;
    if(!zck_seek(src_fd, length, SEEK_SET))
        return False;
    zck->header_string = header;
    zck->header_size = length;
    return True;
}

int zck_read_index_hash(zckCtx *zck, int src_fd) {
    if(zck->header_string == NULL) {
        zck_log(ZCK_LOG_ERROR,
                "Reading index hash before initial bytes are read\n");
        return False;
    }
    size_t length = zck->header_size;
    char *header = zck->header_string;
    zck->header_string = NULL;
    zck->header_size = 0;
    header = realloc(header, length + zck->hash_type.digest_size);
    if(header == NULL) {
        zck_log(ZCK_LOG_ERROR, "Unable to reallocate %lu bytes\n",
                length + zck->hash_type.digest_size);
        return False;
    }
    char *digest = zmalloc(zck->hash_type.digest_size);
    if(digest == NULL) {
        zck_log(ZCK_LOG_ERROR, "Unable to allocate %lu bytes\n",
                zck->hash_type.digest_size);
        return False;
    }
    zck_log(ZCK_LOG_DEBUG, "Reading index hash\n");
    if(!zck_read(src_fd, digest, zck->hash_type.digest_size)) {
        free(digest);
        free(header);
        return False;
    }

    /* Set hash to zeros in header string so we can validate it later */
    memset(header + length, 0, zck->hash_type.digest_size);
    length += zck->hash_type.digest_size;
    zck->index_digest = digest;
    zck->header_string = header;
    zck->header_size = length;
    return True;
}

int zck_read_ct_is(zckCtx *zck, int src_fd) {
    if(zck->header_string == NULL) {
        zck_log(ZCK_LOG_ERROR,
                "Reading compression type before hash type is read\n");
        return False;
    }
    size_t length = zck->header_size;
    char *header = zck->header_string;
    zck->header_string = NULL;
    zck->header_size = 0;

    header = realloc(header, length + MAX_COMP_SIZE*2);
    if(header == NULL) {
        zck_log(ZCK_LOG_ERROR, "Unable to allocate %lu bytes\n",
                length + MAX_COMP_SIZE);
        return False;
    }
    zck_log(ZCK_LOG_DEBUG, "Reading compression type and index size\n");
    if(!zck_read(src_fd, header + length, MAX_COMP_SIZE*2))
        return False;

    int tmp = 0;

    /* Read and initialize compression type */
    if(!zck_compint_to_int(&tmp, header + length, &length))
        return False;
    if(!zck_set_compression_type(zck, tmp))
        return False;
    if(!zck_comp_init(zck))
        return False;

    /* Read and initialize index size */
    if(!zck_compint_to_int(&tmp, header + length, &length))
        return False;
    zck->index_size = tmp;

    if(!zck_seek(src_fd, length, SEEK_SET))
        return False;
    zck->header_string = header;
    zck->header_size = length;
    return True;
}

int zck_read_index(zckCtx *zck, int src_fd) {
    char *index = zmalloc(zck->index_size);
    if(!index) {
        zck_log(ZCK_LOG_ERROR, "Unable to allocate %lu bytes\n",
                zck->index_size);
        return False;
    }
    zck_log(ZCK_LOG_DEBUG, "Reading index\n");
    if(!zck_read(src_fd, index, zck->index_size)) {
        free(index);
        return False;
    }
    if(!zck_index_read(zck, index, zck->index_size)) {
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
    if(!zck_read_ct_is(zck, src_fd))
        return False;
    if(!zck_read_index(zck, src_fd))
        return False;
    return True;
}

int zck_header_create(zckCtx *zck) {
    int header_malloc = 5 + MAX_COMP_SIZE + zck->hash_type.digest_size +
                        MAX_COMP_SIZE*2;
    char *header = zmalloc(header_malloc);
    if(header == NULL) {
        zck_log(ZCK_LOG_ERROR, "Unable to allocate %lu bytes\n", header_malloc);
        return False;
    }
    size_t length = 0;
    memcpy(header+length, "\0ZCK1", 5);
    length += 5;
    if(!zck_compint_from_size(header+length, zck->hash_type.type, &length)) {
        free(header);
        return False;
    }

    /* If we have the digest, write it in, otherwise write zeros */
    if(zck->index_digest)
        memcpy(header+length, zck->index_digest, zck->hash_type.digest_size);
    else
        memset(header+length, 0, zck->hash_type.digest_size);
    length += zck->hash_type.digest_size;

    if(!zck_compint_from_int(header+length, zck->comp.type, &length)) {
        free(header);
        return False;
    }
    if(!zck_compint_from_size(header+length, zck->index_size, &length)) {
        free(header);
        return False;
    }
    header = realloc(header, length);
    if(header == NULL) {
        zck_log(ZCK_LOG_ERROR, "Unable to reallocate %lu bytes\n", length);
        return False;
    }
    if(zck->header_string)
        free(zck->header_string);
    zck->header_string = header;
    zck->header_size = length;
    return True;
}

int zck_write_header(zckCtx *zck) {
    if(!zck_write(zck->fd, zck->header_string, zck->header_size))
        return False;
    return True;
}

