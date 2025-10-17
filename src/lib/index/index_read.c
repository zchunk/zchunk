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
#ifdef FREEBSD
#include <sys/endian.h>
#elif __APPLE__
#include <machine/endian.h>
#elif defined(_WIN32)
// no endian.h available
#else
#include <endian.h>
#endif
#include <zck.h>

#include "zck_private.h"

bool index_read(zckCtx *zck, char *data, size_t size, size_t max_length) {
    VALIDATE_BOOL(zck);
    size_t length = 0;

    /* Read and configure hash type */
    int hash_type = 0;
    if(!compint_to_int(zck, &hash_type, data + length, &length, max_length)) {
        set_fatal_error(zck, "Unable to read hash type");
        return false;
    }
    if(!set_chunk_hash_type(zck, hash_type)) {
        set_fatal_error(zck, "Unable to set chunk hash type");
        return false;
    }

    /* Read number of index entries */
    size_t index_count;
    if(!compint_to_size(zck, &index_count, data + length, &length,
                        max_length)) {
        set_fatal_error(zck, "Unable to read index count");
        return false;
    }
    zck->index.count = index_count;

    zckChunk *prev = zck->index.first;
    size_t idx_loc = 0;
    int count = 0;
    while(length < size) {
        if(length + zck->index.digest_size > max_length) {
            set_fatal_error(zck, "Read past end of header");
            return false;
        }

        zckChunk *tmp = NULL;
        zckChunk *new = zmalloc(sizeof(zckChunk));
        if (!new) {
           zck_log(ZCK_LOG_ERROR, "OOM in %s", __func__);
           return false;
        }

        /* Read index entry digest */
        new->digest = zmalloc(zck->index.digest_size);
        if (!new->digest) {
           zck_log(ZCK_LOG_ERROR, "OOM in %s", __func__);
           free(new);
           return false;
        }
        memcpy(new->digest, data+length, zck->index.digest_size);
        new->digest_size = zck->index.digest_size;
        HASH_FIND(hh, zck->index.ht, new->digest, new->digest_size, tmp);
        if(!tmp)
            HASH_ADD_KEYPTR(hh, zck->index.ht, new->digest, new->digest_size,
                            new);
        length += zck->index.digest_size;

        /* Read uncompressed entry digest, if any */
        if (zck->has_uncompressed_source) {
            /* same size for digest as compressed */
            new->digest_uncompressed = zmalloc(zck->index.digest_size);
            if (!new->digest_uncompressed) {
                free(new->digest);
                free(new);
                zck_log(ZCK_LOG_ERROR, "OOM in %s", __func__);
                return false;
            }
            memcpy(new->digest_uncompressed, data+length, zck->index.digest_size);
            HASH_FIND(hhuncomp, zck->index.htuncomp, new->digest_uncompressed, new->digest_size, tmp);
            if(!tmp)
               HASH_ADD_KEYPTR(hhuncomp, zck->index.htuncomp, new->digest_uncompressed, new->digest_size,
                               new);
            length += zck->index.digest_size;
	}
        /* Read and store entry length */
        size_t chunk_length = 0;
        if(!compint_to_size(zck, &chunk_length, data+length, &length,
                            max_length)) {
            set_fatal_error(zck, "Unable to read chunk %i compressed size",
                            count);
            return false;
        }
        new->start = idx_loc;
        new->comp_length = chunk_length;

        /* Read and store uncompressed entry length */
        chunk_length = 0;
        if(!compint_to_size(zck, &chunk_length, data+length, &length,
                            max_length)) {
            set_fatal_error(zck, "Unable to read chunk %i uncompressed size",
                            count);
            return false;
        }
        new->length = chunk_length;
        new->zck = zck;
        new->valid = 0;
        new->number = count;
        idx_loc += new->comp_length;
        count++;
        zck->index.length = idx_loc;

        if(prev)
            prev->next = new;
        else
            zck->index.first = new;
        prev = new;
    }
    free(zck->index_string);
    zck->index_string = NULL;
    return true;
}

ssize_t ZCK_PUBLIC_API zck_get_chunk_count(zckCtx *zck) {
    VALIDATE_INT(zck);

    return zck->index.count;
}

zckChunk ZCK_PUBLIC_API *zck_get_chunk(zckCtx *zck, size_t number) {
    VALIDATE_PTR(zck);

    for(zckChunk *idx=zck->index.first; idx!=NULL; idx=idx->next) {
        if(idx->number == number)
            return idx;
    }
    zck_log(
        ZCK_LOG_WARNING,
        "Chunk %llu not found",
        (long long unsigned) number
    );
    return NULL;
}

zckChunk ZCK_PUBLIC_API *zck_get_first_chunk(zckCtx *zck) {
    VALIDATE_PTR(zck);

    return zck->index.first;
}

zckChunk ZCK_PUBLIC_API *zck_get_next_chunk(zckChunk *idx) {
    if(idx && idx->zck) {
        VALIDATE_PTR(idx->zck);
        ALLOCD_PTR(idx->zck, idx);
    } else {
        ALLOCD_PTR(NULL, idx);
    }

    return idx->next;
}

zckChunk ZCK_PUBLIC_API *zck_get_src_chunk(zckChunk *idx) {
    if(idx && idx->zck) {
        VALIDATE_PTR(idx->zck);
        ALLOCD_PTR(idx->zck, idx);
    } else {
        ALLOCD_PTR(NULL, idx);
    }

    return idx->src;
}

ssize_t ZCK_PUBLIC_API zck_get_chunk_start(zckChunk *idx) {
    if(idx && idx->zck) {
        VALIDATE_INT(idx->zck);
        ALLOCD_INT(idx->zck, idx);
    } else {
        ALLOCD_INT(NULL, idx);
    }

    if(idx->zck)
        return idx->start + zck_get_header_length(idx->zck);
    else
        return idx->start;
}

ssize_t ZCK_PUBLIC_API zck_get_chunk_size(zckChunk *idx) {
    if(idx && idx->zck) {
        VALIDATE_INT(idx->zck);
        ALLOCD_INT(idx->zck, idx);
    } else {
        ALLOCD_INT(NULL, idx);
    }

    return idx->length;
}

ssize_t ZCK_PUBLIC_API zck_get_chunk_comp_size(zckChunk *idx) {
    if(idx && idx->zck) {
        VALIDATE_INT(idx->zck);
        ALLOCD_INT(idx->zck, idx);
    } else {
        ALLOCD_INT(NULL, idx);
    }

    return idx->comp_length;
}

ssize_t ZCK_PUBLIC_API zck_get_chunk_number(zckChunk *idx) {
    if(idx && idx->zck) {
        VALIDATE_INT(idx->zck);
        ALLOCD_INT(idx->zck, idx);
    } else {
        ALLOCD_INT(NULL, idx);
    }

    return idx->number;
}

int ZCK_PUBLIC_API zck_get_chunk_valid(zckChunk *idx) {
    if(idx && idx->zck) {
        VALIDATE_INT(idx->zck);
        ALLOCD_INT(idx->zck, idx);
    } else {
        ALLOCD_INT(NULL, idx);
    }

    return idx->valid;
}

bool ZCK_PUBLIC_API zck_compare_chunk_digest(zckChunk *a, zckChunk *b) {
    if(a && a->zck) {
        VALIDATE_BOOL(a->zck);
        ALLOCD_BOOL(a->zck, a);
    } else {
        ALLOCD_BOOL(NULL, a);
    }
    if(b && b->zck) {
        VALIDATE_BOOL(b->zck);
        ALLOCD_BOOL(b->zck, b);
    } else {
        ALLOCD_BOOL(NULL, b);
    }
    
    if(a->digest_size != b->digest_size)
        return false;
	
	if(a->zck->has_uncompressed_source && b->zck->has_uncompressed_source){
		/* If both archives has uncompressed digest, compare them instead */
        if(memcmp(a->digest_uncompressed, b->digest_uncompressed, a->digest_size) != 0)
            return false;
    }else {
        if(memcmp(a->digest, b->digest, a->digest_size) != 0)
            return false;
    }

    return true;
}

int ZCK_PUBLIC_API zck_missing_chunks(zckCtx *zck) {
    VALIDATE_READ_INT(zck);

    int missing = 0;
    for(zckChunk *idx = zck->index.first; idx; idx=idx->next)
        if(idx->valid == 0)
            missing++;
    return missing;
}

int ZCK_PUBLIC_API zck_failed_chunks(zckCtx *zck) {
    VALIDATE_READ_INT(zck);

    int failed = 0;
    for(zckChunk *idx = zck->index.first; idx; idx=idx->next)
        if(idx->valid == -1)
            failed++;
    return failed;
}

void ZCK_PUBLIC_API zck_reset_failed_chunks(zckCtx *zck) {
    if(!zck)
        return;

    for(zckChunk *idx = zck->index.first; idx; idx=idx->next)
        if(idx->valid == -1)
            idx->valid = 0;
    return;
}

bool ZCK_PUBLIC_API zck_generate_hashdb(zckCtx *zck) {
    if (zck->index.ht || zck->index.htuncomp) {
        zck_log(ZCK_LOG_ERROR, "Hash DB already present, it could not be created");
        return false;
    }

    for(zckChunk *idx = zck->index.first; idx; idx=idx->next) {
        zckChunk *tmp = NULL;
        HASH_FIND(hh, zck->index.ht, idx->digest, idx->digest_size, tmp);
        if(!tmp)
            HASH_ADD_KEYPTR(hh, zck->index.ht, idx->digest, idx->digest_size,
                            idx);
        /*
         * Do the same if there is uncompressed digest
         */
        if (zck->has_uncompressed_source && idx->digest_uncompressed) {
            HASH_FIND(hhuncomp, zck->index.htuncomp, idx->digest_uncompressed, idx->digest_size, tmp);
            if(!tmp)
               HASH_ADD_KEYPTR(hhuncomp, zck->index.htuncomp, idx->digest_uncompressed, idx->digest_size,
                               idx);
        }
    }

    return true;
}
