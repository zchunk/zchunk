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

#define VALIDATE(f)     if(!f) { \
                            zck_log(ZCK_LOG_ERROR, "zckCtx not initialized\n"); \
                            return False; \
                        }

static int create_chunk(zckCtx *zck) {
    VALIDATE(zck);

    clear_work_index(zck);
    zck->work_index_item = zmalloc(sizeof(zckChunk));
    if(zck->work_index_item == NULL) {
        zck_log(ZCK_LOG_ERROR, "Unable to allocate %lu bytes\n",
                sizeof(zckChunk));
        return False;
    }
    if(!hash_init(&(zck->work_index_hash), &(zck->chunk_hash_type)))
        return False;
    return True;
}

static int finish_chunk(zckIndex *index, zckChunk *item, char *digest,
                        int valid, zckCtx *zck) {
    VALIDATE(index);
    VALIDATE(item);

    item->digest = zmalloc(index->digest_size);
    if(item->digest == NULL) {
        zck_log(ZCK_LOG_ERROR, "Unable to allocate %lu bytes\n",
                index->digest_size);
        return False;
    }
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
    return True;
}

int index_create(zckCtx *zck) {
    VALIDATE(zck);

    char *index;
    size_t index_malloc = 0;
    size_t index_size = 0;


    zck->full_hash_digest = hash_finalize(&(zck->full_hash));
    if(zck->full_hash_digest == NULL)
        return False;

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
    index = realloc(index, index_size);
    if(index == NULL) {
        zck_log(ZCK_LOG_ERROR, "Unable to reallocate %lu bytes\n", index_size);
        return False;
    }
    zck->index_string = index;
    zck->index_size = index_size;
    zck_log(ZCK_LOG_DEBUG, "Generated index: %lu bytes\n", zck->index_size);
    return True;
}

int index_new_chunk(zckIndex *index, char *digest, int digest_size,
                    size_t comp_size, size_t orig_size, int finished,
                    zckCtx *zck) {
    if(index == NULL) {
        zck_log(ZCK_LOG_ERROR, "Invalid index\n");
        return False;
    }
    if(digest_size == 0) {
        zck_log(ZCK_LOG_ERROR, "Digest size 0 too small\n");
        return False;
    }
    zckChunk *chk = zmalloc(sizeof(zckChunk));
    if(chk == NULL) {
        zck_log(ZCK_LOG_ERROR, "Unable to allocate %lu bytes\n",
                sizeof(zckChunk));
        return False;
    }
    index->digest_size = digest_size;
    chk->comp_length = comp_size;
    chk->length = orig_size;
    return finish_chunk(index, chk, digest, finished, zck);
}

int index_add_to_chunk(zckCtx *zck, char *data, size_t comp_size,
                           size_t orig_size) {
    VALIDATE(zck);

    if(zck->work_index_item == NULL && !create_chunk(zck))
        return False;

    zck->work_index_item->length += orig_size;
    if(comp_size == 0)
        return True;

    if(!hash_update(&(zck->full_hash), data, comp_size))
        return False;
    if(!hash_update(&(zck->work_index_hash), data, comp_size))
        return False;

    zck->work_index_item->comp_length += comp_size;
    return True;
}

int index_finish_chunk(zckCtx *zck) {
    VALIDATE(zck);

    if(zck->work_index_item == NULL && !create_chunk(zck))
        return False;

    char *digest = NULL;
    if(zck->work_index_item->length > 0) {
        /* Finalize chunk checksum */
        digest = hash_finalize(&(zck->work_index_hash));
        if(digest == NULL) {
            zck_log(ZCK_LOG_ERROR,
                    "Unable to calculate %s checksum for new chunk\n",
                    zck_hash_name_from_type(zck->index.hash_type));
            return False;
        }
    } else {
        digest = zmalloc(zck->chunk_hash_type.digest_size);
        if(digest == NULL) {
            zck_log(ZCK_LOG_ERROR, "Unable to allocate %lu bytes\n",
                    zck->chunk_hash_type.digest_size);
            return False;
        }
    }
    if(!finish_chunk(&(zck->index), zck->work_index_item, digest, True, zck))
        return False;

    free(digest);
    zck->work_index_item = NULL;
    hash_close(&(zck->work_index_hash));
    return True;
}
