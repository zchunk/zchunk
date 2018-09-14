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
#include <stdbool.h>
#include <string.h>
#include <zck.h>

#include "zck_private.h"

static bool create_chunk(zckCtx *zck) {
    VALIDATE_BOOL(zck);

    clear_work_index(zck);
    zck->work_index_item = zmalloc(sizeof(zckChunk));
    if(!hash_init(zck, &(zck->work_index_hash), &(zck->chunk_hash_type)))
        return false;
    return true;
}

static bool finish_chunk(zckIndex *index, zckChunk *item, char *digest,
                        bool valid, zckCtx *zck) {
    VALIDATE_BOOL(zck);
    ALLOCD_BOOL(zck, index);
    ALLOCD_BOOL(zck, item);

    item->digest = zmalloc(index->digest_size);
    if(digest) {
        memcpy(item->digest, digest, index->digest_size);
        item->digest_size = index->digest_size;
    }
    item->start = index->length;
    item->valid = valid;
    item->zck = zck;
    if(index->first == NULL) {
        index->first = item;
    } else {
        zckChunk *tmp = index->first;
        while(tmp->next)
            tmp = tmp->next;
        tmp->next = item;
    }
    index->count += 1;
    index->length += item->comp_length;
    return true;
}

bool index_create(zckCtx *zck) {
    VALIDATE_BOOL(zck);

    char *index;
    size_t index_malloc = 0;
    size_t index_size = 0;


    zck->full_hash_digest = hash_finalize(zck, &(zck->full_hash));
    if(zck->full_hash_digest == NULL)
        return false;

    /* Set initial malloc size */
    index_malloc  = MAX_COMP_SIZE * 2;

    /* Add digest size + MAX_COMP_SIZE bytes for length of each entry in
     * index */
    if(zck->index.first) {
        zckChunk *tmp = zck->index.first;
        while(tmp) {
            index_malloc += zck->index.digest_size + MAX_COMP_SIZE*2;
            tmp = tmp->next;
        }
    }

    /* Write index */
    index = zmalloc(index_malloc);
    compint_from_size(index+index_size, zck->index.hash_type, &index_size);
    compint_from_size(index+index_size, zck->index.count, &index_size);
    if(zck->index.first) {
        zckChunk *tmp = zck->index.first;
        while(tmp) {
            /* Write digest */
            memcpy(index+index_size, tmp->digest, zck->index.digest_size);
            index_size += zck->index.digest_size;
            /* Write compressed size */
            compint_from_size(index+index_size, tmp->comp_length,
                                  &index_size);
            /* Write uncompressed size */
            compint_from_size(index+index_size, tmp->length, &index_size);

            tmp = tmp->next;
        }
    }
    /* Shrink index to actual size */
    index = zrealloc(index, index_size);
    zck->index_string = index;
    zck->index_size = index_size;
    zck_log(ZCK_LOG_DEBUG, "Generated index: %lu bytes", zck->index_size);
    return true;
}

bool index_new_chunk(zckCtx *zck, zckIndex *index, char *digest, int digest_size,
                    size_t comp_size, size_t orig_size, bool finished) {
    VALIDATE_BOOL(zck);

    if(index == NULL) {
        set_error(zck, "Invalid index");
        return false;
    }
    if(digest_size == 0) {
        set_error(zck, "Digest size 0 too small");
        return false;
    }
    zckChunk *chk = zmalloc(sizeof(zckChunk));
    index->digest_size = digest_size;
    chk->comp_length = comp_size;
    chk->length = orig_size;
    return finish_chunk(index, chk, digest, finished, zck);
}

bool index_add_to_chunk(zckCtx *zck, char *data, size_t comp_size,
                           size_t orig_size) {
    VALIDATE_BOOL(zck);

    if(zck->work_index_item == NULL && !create_chunk(zck))
        return false;

    zck->work_index_item->length += orig_size;
    if(comp_size == 0)
        return true;

    if(!hash_update(zck, &(zck->full_hash), data, comp_size))
        return false;
    if(!hash_update(zck, &(zck->work_index_hash), data, comp_size))
        return false;

    zck->work_index_item->comp_length += comp_size;
    return true;
}

bool index_finish_chunk(zckCtx *zck) {
    VALIDATE_BOOL(zck);

    if(zck->work_index_item == NULL && !create_chunk(zck))
        return false;

    char *digest = NULL;
    if(zck->work_index_item->length > 0) {
        /* Finalize chunk checksum */
        digest = hash_finalize(zck, &(zck->work_index_hash));
        if(digest == NULL) {
            set_fatal_error(zck,
                            "Unable to calculate %s checksum for new chunk",
                            zck_hash_name_from_type(zck->index.hash_type));
            return false;
        }
    } else {
        digest = zmalloc(zck->chunk_hash_type.digest_size);
    }
    if(!finish_chunk(&(zck->index), zck->work_index_item, digest, true, zck))
        return false;

    free(digest);
    zck->work_index_item = NULL;
    hash_close(&(zck->work_index_hash));
    return true;
}
