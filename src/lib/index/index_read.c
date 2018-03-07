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
    zckHash index_hash;
    char *digest;
    uint64_t index_size;
    uint64_t index_count;
    uint8_t hash_type;
    char *dst = NULL;
    size_t dst_size = 0;
    char *cur_loc;

    /* Check that index checksum matches stored checksum */
    zck_log(ZCK_LOG_DEBUG, "Reading index size\n");
    index_size = htole64(size);
    if(!zck_hash_init(&index_hash, &(zck->hash_type)))
        return False;
    if(!zck_hash_update(&index_hash, (const char *)&(zck->comp.type), 1))
        return False;
    if(!zck_hash_update(&index_hash, (const char *)&index_size, sizeof(uint64_t)))
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
    free(digest);
    zck_log(ZCK_LOG_DEBUG, "Decompressing index\n");
    if(!zck_decompress(zck, data, size, &dst, &dst_size)) {
        zck_log(ZCK_LOG_ERROR, "Unable to decompress index\n");
        return False;
    }

    /* Make sure there's at least enough data for full digest and index count */
    if(dst_size < zck->hash_type.digest_size + sizeof(uint64_t) + 1) {
        zck_log(ZCK_LOG_ERROR, "Index is too small to read\n");
        if(dst)
            free(dst);
        return False;
    }

    zckIndex *prev = zck->index.first;
    zck->full_hash_digest = zmalloc(zck->hash_type.digest_size);
    if(!zck->full_hash_digest) {
        if(dst)
            free(dst);
        zck_log(ZCK_LOG_ERROR, "Unable to allocate %lu bytes\n",
                zck->hash_type.digest_size);
        return False;
    }
    memcpy(&hash_type, dst, 1);
    if(!zck_hash_setup(zck->index.hash_type, hash_type)) {
        if(dst)
            free(dst);
        return False;
    }

    if((dst_size - (zck->hash_type.digest_size + sizeof(uint64_t)+ 1)) %
       (zck->index.hash_type->digest_size + sizeof(uint64_t)) != 0) {
        zck_log(ZCK_LOG_ERROR, "Index size is invalid\n");
        if(dst)
            free(dst);
        return False;
    }
    cur_loc = dst + 1;
    memcpy(&index_count, cur_loc, sizeof(uint64_t));
    zck->index.count = le64toh(index_count);
    cur_loc += sizeof(uint64_t);
    memcpy(zck->full_hash_digest, cur_loc, zck->hash_type.digest_size);
    cur_loc += zck->hash_type.digest_size;
    uint64_t prev_loc = 0;
    while(cur_loc < dst + dst_size) {
        zckIndex *new = zmalloc(sizeof(zckIndex));
        if(!new) {
            zck_log(ZCK_LOG_ERROR, "Unable to allocate %lu bytes\n",
                    sizeof(zckIndex));
            return False;
        }
        uint64_t end = 0;

        new->digest = zmalloc(zck->index.hash_type->digest_size);
        if(!new->digest) {
            zck_log(ZCK_LOG_ERROR, "Unable to allocate %lu bytes\n",
                    zck->index.hash_type->digest_size);
            return False;
        }
        memcpy(new->digest, cur_loc, zck->index.hash_type->digest_size);
        cur_loc += zck->index.hash_type->digest_size;
        memcpy(&end, cur_loc, sizeof(uint64_t));
        new->start = prev_loc;
        new->length = le64toh(end) - prev_loc;
        prev_loc = le64toh(end);
        zck->index.length += new->length;
        cur_loc += sizeof(uint64_t);
        if(prev) {
            prev->next = new;
        } else {
            zck->index.first = new;
        }
        prev = new;
    }
    free(dst);
    return True;
}
