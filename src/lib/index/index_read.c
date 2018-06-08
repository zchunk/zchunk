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

    zckChunk *prev = zck->index.first;
    size_t idx_loc = 0;
    while(length < size) {
        if(length + zck->index.digest_size > max_length) {
            zck_log(ZCK_LOG_ERROR, "Read past end of header\n");
            return False;
        }

        zckChunk *new = zmalloc(sizeof(zckChunk));
        if(!new) {
            zck_log(ZCK_LOG_ERROR, "Unable to allocate %lu bytes\n",
                    sizeof(zckChunk));
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
        new->zck = zck;
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

ssize_t PUBLIC zck_get_chunk_count(zckCtx *zck) {
    if(zck == NULL)
        return -1;
    return zck->index.count;
}

zckChunk PUBLIC *zck_get_first_chunk(zckCtx *zck) {
    if(zck == NULL)
        return NULL;
    return zck->index.first;
}

zckChunk PUBLIC *zck_get_next_chunk(zckChunk *idx) {
    if(idx == NULL)
        return NULL;
    return idx->next;
}

ssize_t PUBLIC zck_get_chunk_start(zckChunk *idx) {
    if(idx == NULL)
        return -1;
    if(idx->zck)
        return idx->start + zck_get_header_length(idx->zck);
    else
        return idx->start;
}

ssize_t PUBLIC zck_get_chunk_size(zckChunk *idx) {
    if(idx == NULL)
        return -1;
    return idx->length;
}

ssize_t PUBLIC zck_get_chunk_comp_size(zckChunk *idx) {
    if(idx == NULL)
        return -1;
    return idx->comp_length;
}

int PUBLIC zck_get_chunk_valid(zckChunk *idx) {
    if(idx == NULL)
        return -1;
    return idx->valid;
}

int PUBLIC zck_compare_chunk_digest(zckChunk *a, zckChunk *b) {
    if(a == NULL || b == NULL)
        return False;
    if(a->digest_size != b->digest_size)
        return False;
    if(memcmp(a->digest, b->digest, a->digest_size) != 0)
        return False;
    return True;
}

int PUBLIC zck_missing_chunks(zckCtx *zck) {
    if(zck == NULL) {
        zck_log(ZCK_LOG_ERROR, "zckCtx not initialized\n");
        return -1;
    }
    int missing = 0;
    for(zckChunk *idx = zck->index.first; idx; idx=idx->next)
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
    for(zckChunk *idx = zck->index.first; idx; idx=idx->next)
        if(idx->valid == -1)
            failed++;
    return failed;
}

void PUBLIC zck_reset_failed_chunks(zckCtx *zck) {
    if(!zck)
        return;

    for(zckChunk *idx = zck->index.first; idx; idx=idx->next)
        if(idx->valid == -1)
            idx->valid = 0;
    return;
}
