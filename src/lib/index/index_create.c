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
    char *index_loc;
    size_t index_size = 0;
    uint64_t index_count = 0;


    zck->full_hash_digest = zck_hash_finalize(&(zck->full_hash));
    if(zck->full_hash_digest == NULL)
        return False;

    index_size = 1; // Chunk hash type;
    index_size += sizeof(uint64_t); // Number of index entries
    index_size += zck->hash_type.digest_size; // Full hash digest

    /* Add digest size + 8 bytes for end location for each entry in index */
    if(zck->index.first) {
        zckIndex *tmp = zck->index.first;
        while(tmp) {
            index_size += zck->index.hash_type->digest_size + sizeof(uint64_t);
            tmp = tmp->next;
        }
    }

    /* Write index */
    index = zmalloc(index_size);
    index_loc = index;
    memcpy(index_loc, &(zck->index.hash_type->type), 1);
    index_loc += 1;
    index_count = htole64(zck->index.count);
    memcpy(index_loc, &index_count, sizeof(uint64_t));
    index_loc += sizeof(uint64_t);
    memcpy(index_loc, zck->full_hash_digest, zck->hash_type.digest_size);
    index_loc += zck->hash_type.digest_size;
    if(zck->index.first) {
        zckIndex *tmp = zck->index.first;
        while(tmp) {
            uint64_t end = htole64(tmp->start + tmp->length);
            memcpy(index_loc, tmp->digest, zck->index.hash_type->digest_size);
            index_loc += zck->index.hash_type->digest_size;
            memcpy(index_loc, &end, sizeof(uint64_t));
            index_loc += sizeof(uint64_t);
            tmp = tmp->next;
        }
    }

    if(!zck->comp.compress(&zck->comp, index, index_size, &(zck->comp_index),
                           &(zck->comp_index_size), 0)) {
        free(index);
        return False;
    }
    index_size = htole64((uint64_t) zck->comp_index_size);

    /* Calculate hash of index, including compressed size at beginning */
    if(!zck_hash_init(&index_hash, &(zck->hash_type))) {
        free(index);
        return False;
    }
    if(!zck_hash_update(&index_hash, (const char *)&(zck->comp.type), 1)) {
        free(index);
        return False;
    }
    if(!zck_hash_update(&index_hash, (const char *)&index_size,
                        sizeof(uint64_t))) {
        free(index);
        return False;
    }
    if(!zck_hash_update(&index_hash, zck->comp_index, zck->comp_index_size)) {
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
    free(index);
    return True;
}

int zck_index_new_chunk(zckIndexInfo *index, char *digest, size_t length) {
    if(index == NULL) {
        zck_log(ZCK_LOG_ERROR, "Invalid index");
        return False;
    }
    zckIndex *idx = zmalloc(sizeof(zckIndex));
    if(idx == NULL) {
        zck_log(ZCK_LOG_ERROR, "Unable to allocate %lu bytes\n",
                sizeof(zckIndex));
        return False;
    }
    if(digest == NULL || length == 0) {
        idx->digest = zmalloc(index->hash_type->digest_size);
        if(idx->digest == NULL) {
            zck_log(ZCK_LOG_ERROR, "Unable to allocate %lu bytes\n",
                    index->hash_type->digest_size);
            return False;
        }
    } else {
        idx->digest = digest;
    }
    idx->start = index->length;
    idx->length = length;
    if(index->first == NULL) {
        index->first = idx;
    } else {
        zckIndex *tmp=index->first;
        while(tmp->next)
            tmp = tmp->next;
        tmp->next = idx;
    }
    index->count += 1;
    index->length += length;
    return True;
}

int zck_index_add_dl_chunk(zckDL *dl, char *digest, size_t size) {
    if(dl == NULL) {
        zck_log(ZCK_LOG_ERROR, "Invalid dl context");
        return False;
    }
    zckIndex *new_index = zmalloc(sizeof(zckIndex));
    if(new_index == NULL) {
        zck_log(ZCK_LOG_ERROR, "Unable to allocate %lu bytes\n",
                sizeof(zckIndex));
        return False;
    }
    dl->index.count = dl->index.count + 1;
}

int zck_index_add_chunk(zckCtx *zck, char *data, size_t size) {
    zckIndex *new_index;
    zckHash hash;

    if(zck == NULL) {
        zck_log(ZCK_LOG_ERROR, "Invalid zck context");
        return False;
    }

    if(size == 0) {
        if(!zck_index_new_chunk(&(zck->index), NULL, size))
            return False;
    } else {
        if(!zck_hash_update(&(zck->full_hash), data, size)) {
            free(new_index);
            return False;
        }
        if(!zck_hash_init(&hash, zck->index.hash_type)) {
            free(new_index);
            return False;
        }
        if(!zck_hash_update(&hash, data, size)) {
            free(new_index);
            return False;
        }
        char *digest = zck_hash_finalize(&hash);
        if(digest == NULL) {
            zck_log(ZCK_LOG_ERROR,
                    "Unable to calculate %s checksum for new chunk\n",
                    zck_hash_name_from_type(zck->index.hash_type->type));
            return False;
        }
        if(!zck_index_new_chunk(&(zck->index), digest, size))
            return False;
    }
    return True;
}

int zck_write_index(zckCtx *zck) {
    return zck_write(zck->fd, zck->comp_index, zck->comp_index_size);
}
