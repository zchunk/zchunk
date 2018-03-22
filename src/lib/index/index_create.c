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

int zck_index_finalize(zckCtx *zck) {
    zckHash index_hash;
    char *index;
    size_t index_malloc = 0;
    size_t index_size = 0;


    zck->full_hash_digest = zck_hash_finalize(&(zck->full_hash));
    if(zck->full_hash_digest == NULL)
        return False;

    index_malloc  = MAX_COMP_SIZE * 2; // Chunk hash type and # of index entries
    index_malloc += zck->hash_type.digest_size; // Full hash digest

    /* Add digest size + MAX_COMP_SIZE bytes for length of each entry in
     * index */
    if(zck->index.first) {
        zckIndexItem *tmp = zck->index.first;
        while(tmp) {
            index_malloc += zck->index.digest_size + MAX_COMP_SIZE;
            tmp = tmp->next;
        }
    }

    /* Write index */
    index = zmalloc(index_malloc);
    zck_compint_from_size(index+index_size, zck->index.hash_type, &index_size);
    zck_compint_from_size(index+index_size, zck->index.count, &index_size);
    memcpy(index+index_size, zck->full_hash_digest, zck->hash_type.digest_size);
    index_size += zck->hash_type.digest_size;
    if(zck->index.first) {
        zckIndexItem *tmp = zck->index.first;
        while(tmp) {
            memcpy(index+index_size, tmp->digest, zck->index.digest_size);
            index_size += zck->index.digest_size;
            zck_compint_from_size(index+index_size, tmp->length, &index_size);
            tmp = tmp->next;
        }
    }
    /* Shrink index to actual size */
    index = realloc(index, index_size);
    if(index == NULL) {
        zck_log(ZCK_LOG_ERROR, "Unable to reallocate %lu bytes\n", index_size);
        return False;
    }
    zck->index_string = index;
    zck->index_size = index_size;

    /* Rebuild header with index hash set to zeros */
    if(zck->index_digest) {
        free(zck->index_digest);
        zck->index_digest = NULL;
    }
    if(!zck_header_create(zck))
        return False;

    /* Calculate hash of header */
    if(!zck_hash_init(&index_hash, &(zck->hash_type))) {
        free(index);
        return False;
    }
    if(!zck_hash_update(&index_hash, zck->header_string, zck->header_size)) {
        free(index);
        return False;
    }
    if(!zck_hash_update(&index_hash, zck->index_string, zck->index_size)) {
        free(index);
        return False;
    }
    zck->index_digest = zck_hash_finalize(&index_hash);
    if(zck->index_digest == NULL) {
        zck_log(ZCK_LOG_ERROR,
                "Unable to calculate %s checksum for index\n",
                zck_hash_name_from_type(zck->hash_type.type));
        return False;
    }

    /* Rebuild header string with calculated index hash */
    if(!zck_header_create(zck))
        return False;

    return True;
}

int zck_index_new_chunk(zckIndex *index, char *digest, int digest_size,
                        size_t length, int finished) {
    if(index == NULL) {
        zck_log(ZCK_LOG_ERROR, "Invalid index\n");
        return False;
    }
    if(digest_size == 0) {
        zck_log(ZCK_LOG_ERROR, "Digest size 0 too small\n");
        return False;
    }
    zckIndexItem *idx = zmalloc(sizeof(zckIndexItem));
    if(idx == NULL) {
        zck_log(ZCK_LOG_ERROR, "Unable to allocate %lu bytes\n",
                sizeof(zckIndexItem));
        return False;
    }
    idx->digest = zmalloc(digest_size);
    if(idx->digest == NULL) {
        zck_log(ZCK_LOG_ERROR, "Unable to allocate %lu bytes\n", digest_size);
        return False;
    }
    if(digest)
        memcpy(idx->digest, digest, digest_size);
    idx->digest_size = digest_size;
    idx->start = index->length;
    idx->length = length;
    idx->finished = finished;
    if(index->first == NULL) {
        index->first = idx;
    } else {
        zckIndexItem *tmp=index->first;
        while(tmp->next)
            tmp = tmp->next;
        tmp->next = idx;
    }
    index->count += 1;
    index->length += length;
    return True;
}

int zck_index_add_chunk(zckCtx *zck, char *data, size_t size) {
    zckHash hash;

    if(zck == NULL) {
        zck_log(ZCK_LOG_ERROR, "Invalid zck context");
        return False;
    }

    if(size == 0) {
        if(!zck_index_new_chunk(&(zck->index), NULL, zck->index.digest_size,
                                size, True))
            return False;
    } else {
        if(!zck_hash_update(&(zck->full_hash), data, size))
            return False;
        if(!zck_hash_init(&hash, &(zck->chunk_hash_type)))
            return False;
        if(!zck_hash_update(&hash, data, size))
            return False;

        char *digest = zck_hash_finalize(&hash);
        if(digest == NULL) {
            zck_log(ZCK_LOG_ERROR,
                    "Unable to calculate %s checksum for new chunk\n",
                    zck_hash_name_from_type(zck->index.hash_type));
            return False;
        }
        if(!zck_index_new_chunk(&(zck->index), digest, zck->index.digest_size,
                                size, True))
            return False;
        free(digest);
    }
    return True;
}

int zck_write_index(zckCtx *zck) {
    return zck_write(zck->fd, zck->index_string, zck->index_size);
}
