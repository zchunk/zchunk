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
#include <endian.h>
#include <zck.h>

#include "zck_private.h"

#define VALIDATE(f)     if(!f) { \
                            zck_log(ZCK_LOG_ERROR, "zckCtx not initialized\n"); \
                            return False; \
                        }

int index_read(zckCtx *zck, char *data, size_t size, size_t max_length) {
    VALIDATE(zck);
    size_t length = 0;

    /* Read and configure hash type */
    int hash_type;
    if(!compint_to_int(&hash_type, data + length, &length, max_length))
        return False;
    if(!set_chunk_hash_type(zck, hash_type))
        return False;

    /* Read number of index entries */
    size_t index_count;
    if(!compint_to_size(&index_count, data + length, &length, max_length))
        return False;
    zck->index.count = index_count;

    zckIndexItem *prev = zck->index.first;
    size_t idx_loc = 0;
    while(length < size) {
        if(length + zck->index.digest_size > max_length) {
            zck_log(ZCK_LOG_ERROR, "Read past end of header\n");
            return False;
        }

        zckIndexItem *new = zmalloc(sizeof(zckIndexItem));
        if(!new) {
            zck_log(ZCK_LOG_ERROR, "Unable to allocate %lu bytes\n",
                    sizeof(zckIndexItem));
            return False;
        }

        /* Read index entry digest */
        new->digest = zmalloc(zck->index.digest_size);
        if(!new->digest) {
            zck_log(ZCK_LOG_ERROR, "Unable to allocate %lu bytes\n",
                    zck->index.digest_size);
            return False;
        }
        memcpy(new->digest, data+length, zck->index.digest_size);
        new->digest_size = zck->index.digest_size;
        length += zck->index.digest_size;

        /* Read and store entry length */
        size_t chunk_length = 0;
        if(!compint_to_size(&chunk_length, data+length, &length, max_length))
            return False;
        new->start = idx_loc;
        new->comp_length = chunk_length;

        /* Read and store uncompressed entry length */
        chunk_length = 0;
        if(!compint_to_size(&chunk_length, data+length, &length, max_length))
            return False;
        new->length = chunk_length;

        new->valid = 0;
        idx_loc += new->comp_length;
        zck->index.length = idx_loc;

        if(prev)
            prev->next = new;
        else
            zck->index.first = new;
        prev = new;
    }
    free(zck->index_string);
    zck->index_string = NULL;
    return True;
}

ssize_t PUBLIC zck_get_index_count(zckCtx *zck) {
    if(zck == NULL)
        return -1;
    return zck->index.count;
}

zckIndex PUBLIC *zck_get_index(zckCtx *zck) {
    if(zck == NULL)
        return NULL;
    return &(zck->index);
}

int PUBLIC zck_missing_chunks(zckCtx *zck) {
    if(zck == NULL) {
        zck_log(ZCK_LOG_ERROR, "zckCtx not initialized\n");
        return -1;
    }
    int missing = 0;
    for(zckIndexItem *idx = zck->index.first; idx; idx=idx->next)
        if(idx->valid == 0)
            missing++;
    return missing;
}

int PUBLIC zck_has_failed_chunks(zckCtx *zck) {
    if(zck == NULL) {
        zck_log(ZCK_LOG_ERROR, "zckCtx not initialized\n");
        return -1;
    }
    int failed = 0;
    for(zckIndexItem *idx = zck->index.first; idx; idx=idx->next)
        if(idx->valid == -1)
            failed++;
    return failed;
}

void PUBLIC zck_reset_failed_chunks(zckCtx *zck) {
    if(!zck)
        return;

    for(zckIndexItem *idx = zck->index.first; idx; idx=idx->next)
        if(idx->valid == -1)
            idx->valid = 0;
    return;
}
