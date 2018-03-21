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

int zck_index_read(zckCtx *zck, char *data, size_t size) {
    zckHash index_hash = {0};
    char *digest = NULL;
    size_t length = 0;

    /* Check that index checksum matches stored checksum */
    zck_log(ZCK_LOG_DEBUG, "Calculating index checksum\n");
    if(!zck_hash_init(&index_hash, &(zck->hash_type)))
        return False;
    if(!zck_hash_update(&index_hash, zck->header_string, zck->header_size))
        return False;
    if(!zck_hash_update(&index_hash, data, size))
        return False;
    digest = zck_hash_finalize(&index_hash);
    if(digest == NULL) {
        zck_log(ZCK_LOG_ERROR,
                "Unable to calculate %s checksum for header\n",
                zck_hash_name_from_type(zck->hash_type.type));
        return False;
    }
    zck_log(ZCK_LOG_DEBUG, "Checking index checksum\n");
    if(memcmp(digest, zck->index_digest, zck->hash_type.digest_size) != 0) {
        free(digest);
        zck_log(ZCK_LOG_ERROR, "Index fails checksum test\n");
        return False;
    }
    zck_log(ZCK_LOG_DEBUG, "Checksum is valid\n");
    free(digest);

    /* Make sure there's at least enough data for full digest and index count */
    if(size < zck->hash_type.digest_size + MAX_COMP_SIZE*2) {
        zck_log(ZCK_LOG_ERROR, "Index is too small to read\n");
        return False;
    }

    /* Read and configure hash type */
    int hash_type;
    if(!zck_compint_to_int(&hash_type, data + length, &length))
        return False;
    if(!zck_set_chunk_hash_type(zck, hash_type))
        return False;

    /* Read number of index entries */
    size_t index_count;
    if(!zck_compint_to_size(&index_count, data + length, &length))
        return False;
    zck->index.count = index_count;

    /* Read full data hash */
    zck->full_hash_digest = zmalloc(zck->hash_type.digest_size);
    if(!zck->full_hash_digest) {
        zck_log(ZCK_LOG_ERROR, "Unable to allocate %lu bytes\n",
                zck->hash_type.digest_size);
        return False;
    }
    memcpy(zck->full_hash_digest, data + length, zck->hash_type.digest_size);
    length += zck->hash_type.digest_size;

    zckIndex *prev = zck->index.first;
    size_t idx_loc = 0;
    while(length < size) {
        zckIndex *new = zmalloc(sizeof(zckIndex));
        if(!new) {
            zck_log(ZCK_LOG_ERROR, "Unable to allocate %lu bytes\n",
                    sizeof(zckIndex));
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
        if(!zck_compint_to_size(&chunk_length, data+length, &length))
            return False;
        new->start = idx_loc;
        new->length = chunk_length;
        new->finished = False;

        idx_loc += chunk_length;
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
