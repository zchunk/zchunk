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
    size_t length = 0;

    /* Add index to checksum */
    if(!zck_hash_update(&(zck->check_full_hash), data, size))
        return False;

    /* Make sure there's at least enough data for full digest and index count */
    if(size < zck->hash_type.digest_size + MAX_COMP_SIZE*2) {
        zck_log(ZCK_LOG_ERROR, "Index is too small to read\n");
        return False;
    }

    /* Read and configure hash type */
    int hash_type;
    if(!compint_to_int(&hash_type, data + length, &length))
        return False;
    if(!zck_set_ioption(zck, ZCK_HASH_CHUNK_TYPE, hash_type))
        return False;

    /* Read number of index entries */
    size_t index_count;
    if(!compint_to_size(&index_count, data + length, &length))
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

    zckIndexItem *prev = zck->index.first;
    size_t idx_loc = 0;
    while(length < size) {
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
        if(!compint_to_size(&chunk_length, data+length, &length))
            return False;
        new->start = idx_loc;
        new->comp_length = chunk_length;

        /* Read and store uncompressed entry length */
        chunk_length = 0;
        if(!compint_to_size(&chunk_length, data+length, &length))
            return False;
        new->length = chunk_length;

        new->finished = False;
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
