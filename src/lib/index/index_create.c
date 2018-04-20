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

int zck_index_finalize(zckCtx *zck) {
    VALIDATE(zck);

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
            index_malloc += zck->index.digest_size + MAX_COMP_SIZE*2;
            tmp = tmp->next;
        }
    }

    /* Write index */
    index = zmalloc(index_malloc);
    compint_from_size(index+index_size, zck->index.hash_type, &index_size);
    compint_from_size(index+index_size, zck->index.count, &index_size);
    memcpy(index+index_size, zck->full_hash_digest, zck->hash_type.digest_size);
    index_size += zck->hash_type.digest_size;
    if(zck->index.first) {
        zckIndexItem *tmp = zck->index.first;
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

    /* Rebuild header without index hash */
    if(zck->header_digest) {
        free(zck->header_digest);
        zck->header_digest = NULL;
    }
    if(!zck_header_create(zck))
        return False;

    /* Rebuild signatures */
    if(!zck_sig_create(zck))
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
    if(!zck_hash_update(&index_hash, zck->sig_string, zck->sig_size)) {
        free(index);
        return False;
    }
    zck->header_digest = zck_hash_finalize(&index_hash);
    if(zck->header_digest == NULL) {
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

static int finish_chunk(zckIndex *index, zckIndexItem *item, char *digest,
                        int finished) {
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
    item->finished = finished;
    if(index->first == NULL) {
        index->first = item;
    } else {
        zckIndexItem *tmp = index->first;
        while(tmp->next)
            tmp = tmp->next;
        tmp->next = item;
    }
    index->count += 1;
    index->length += item->comp_length;
    return True;
}

int zck_index_new_chunk(zckIndex *index, char *digest, int digest_size,
                        size_t comp_size, size_t orig_size, int finished) {
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
    index->digest_size = digest_size;
    idx->comp_length = comp_size;
    idx->length = orig_size;
    return finish_chunk(index, idx, digest, finished);
}

int zck_index_create_chunk(zckCtx *zck) {
    VALIDATE(zck);

    zck_clear_work_index(zck);
    zck->work_index_item = zmalloc(sizeof(zckIndexItem));
    if(zck->work_index_item == NULL) {
        zck_log(ZCK_LOG_ERROR, "Unable to allocate %lu bytes\n",
                sizeof(zckIndexItem));
        return False;
    }
    if(!zck_hash_init(&(zck->work_index_hash), &(zck->chunk_hash_type)))
        return False;
    return True;
}

int zck_index_add_to_chunk(zckCtx *zck, char *data, size_t comp_size,
                           size_t orig_size) {
    VALIDATE(zck);

    if(zck->work_index_item == NULL && !zck_index_create_chunk(zck))
        return False;

    zck->work_index_item->length += orig_size;
    if(comp_size == 0)
        return True;

    if(!zck_hash_update(&(zck->full_hash), data, comp_size))
        return False;
    if(!zck_hash_update(&(zck->work_index_hash), data, comp_size))
        return False;

    zck->work_index_item->comp_length += comp_size;
    return True;
}

int zck_index_finish_chunk(zckCtx *zck) {
    VALIDATE(zck);

    if(zck->work_index_item == NULL && !zck_index_create_chunk(zck))
        return False;

    char *digest = NULL;
    if(zck->work_index_item->length > 0) {
        /* Finalize chunk checksum */
        digest = zck_hash_finalize(&(zck->work_index_hash));
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
    if(!finish_chunk(&(zck->index), zck->work_index_item, digest, True))
        return False;

    free(digest);
    zck->work_index_item = NULL;
    zck_hash_close(&(zck->work_index_hash));
    return True;
}

int zck_write_index(zckCtx *zck) {
    return write_data(zck->fd, zck->index_string, zck->index_size);
}
