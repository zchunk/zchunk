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
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <zck.h>

#include "zck_private.h"
#include "sha1/sha1.h"
#include "sha2/sha2.h"

#define VALIDATE(f)     if(!f) { \
                            zck_log(ZCK_LOG_ERROR, "zckCtx not initialized\n"); \
                            return False; \
                        }

#define VALIDATE_READ(f)    VALIDATE(f); \
                            if(f->mode != ZCK_MODE_READ) { \
                                zck_log(ZCK_LOG_ERROR, \
                                        "zckCtx not opened for reading\n"); \
                                return False; \
                            }

#define VALIDATE_WRITE(f)   VALIDATE(f); \
                            if(f->mode != ZCK_MODE_WRITE) { \
                                zck_log(ZCK_LOG_ERROR, \
                                        "zckCtx not opened for writing\n"); \
                                return False; \
                            }
/* This needs to be updated to the largest hash size every time a new hash type
 * is added */
int get_max_hash_size() {
    return SHA256_DIGEST_SIZE;
}



static char unknown[] = "Unknown(\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0";

const static char *HASH_NAME[] = {
    "SHA-1",
    "SHA-256"
};

static int validate_checksums(zckCtx *zck, zck_log_type bad_checksums) {
    VALIDATE_READ(zck);
    char buf[BUF_SIZE];

    if(zck->data_offset == 0) {
        zck_log(ZCK_LOG_ERROR, "Header hasn't been read yet\n");
        return 0;
    }

    hash_close(&(zck->check_full_hash));
    if(!hash_init(&(zck->check_full_hash), &(zck->hash_type)))
        return 0;

    if(!seek_data(zck->fd, zck->data_offset, SEEK_SET))
        return 0;

    /* Check each chunk checksum */
    int all_good = True;
    for(zckIndexItem *idx = zck->index.first; idx; idx = idx->next) {
        if(idx == zck->index.first && idx->length == 0) {
            idx->valid = 1;
            continue;
        }

        if(!hash_init(&(zck->check_chunk_hash), &(zck->chunk_hash_type)))
            return 0;

        size_t rlen = 0;
        while(rlen < idx->comp_length) {
            size_t rsize = BUF_SIZE;
            if(BUF_SIZE > idx->comp_length - rlen)
                rsize = idx->comp_length - rlen;
            if(read_data(zck->fd, buf, rsize) != rsize)
                zck_log(ZCK_LOG_DEBUG, "No more data\n");
            if(!hash_update(&(zck->check_chunk_hash), buf, rsize))
                return 0;
            if(!hash_update(&(zck->check_full_hash), buf, rsize))
                return 0;
            rlen += rsize;
        }
        int valid_chunk = validate_chunk(zck, idx, bad_checksums);
        if(!valid_chunk)
            return 0;
        idx->valid = valid_chunk;
        if(all_good && valid_chunk != 1)
            all_good = False;
    }
    int valid_file = -1;
    if(all_good) {
        /* Check data checksum */
        valid_file = validate_file(zck, bad_checksums);
        if(!valid_file)
            return 0;

        /* If data checksum failed, invalidate *all* chunks */
        if(valid_file == -1)
            for(zckIndexItem *idx = zck->index.first; idx; idx = idx->next)
                idx->valid = -1;
    }

    /* Go back to beginning of data section */
    if(!seek_data(zck->fd, zck->data_offset, SEEK_SET))
        return 0;

    /* Reinitialize data checksum */
    if(!hash_init(&(zck->check_full_hash), &(zck->hash_type)))
        return 0;

    return valid_file;
}

char *get_digest_string(const char *digest, int size) {
    char *str = zmalloc(size*2+1);

    for(int i=0; i<size; i++)
        snprintf(str + i*2, 3, "%02x", (unsigned char)digest[i]);
    return str;
}

int hash_setup(zckHashType *ht, int h) {
    if(ht) {
        if(h == ZCK_HASH_SHA1) {
            memset(ht, 0, sizeof(zckHashType));
            ht->type = ZCK_HASH_SHA1;
            ht->digest_size = SHA1_DIGEST_LENGTH;
            return True;
        }else if(h == ZCK_HASH_SHA256) {
            memset(ht, 0, sizeof(zckHashType));
            ht->type = ZCK_HASH_SHA256;
            ht->digest_size = SHA256_DIGEST_SIZE;
            return True;
        }
        zck_log(ZCK_LOG_ERROR, "Unsupported hash type: %s\n",
                zck_hash_name_from_type(h));
        return False;
    }
    zck_log(ZCK_LOG_ERROR, "zckHashType is null\n");
    return False;
}

void hash_close(zckHash *hash) {
    if(!hash)
        return;

    if(hash->ctx) {
        free(hash->ctx);
        hash->ctx = NULL;
    }
    hash->type = NULL;
    return;
}

int hash_init(zckHash *hash, zckHashType *hash_type) {
    if(hash && hash_type) {
        if(hash_type->type == ZCK_HASH_SHA1) {
            zck_log(ZCK_LOG_DEBUG, "Initializing SHA-1 hash\n");
            hash->ctx = zmalloc(sizeof(SHA_CTX));
            hash->type = hash_type;
            if(hash->ctx == NULL)
                return False;
            SHA1_Init((SHA_CTX *) hash->ctx);
            return True;
        }else if(hash_type->type == ZCK_HASH_SHA256) {
            zck_log(ZCK_LOG_DEBUG, "Initializing SHA-256 hash\n");
            hash->ctx = zmalloc(sizeof(sha256_ctx));
            hash->type = hash_type;
            if(hash->ctx == NULL)
                return False;
            sha256_init((sha256_ctx *) hash->ctx);
            return True;
        }
        zck_log(ZCK_LOG_ERROR, "Unsupported hash type: %i\n", hash_type->type);
        return False;
    }
    zck_log(ZCK_LOG_ERROR, "Either zckHash or zckHashType struct is null\n");
    return False;
}

int hash_update(zckHash *hash, const char *message, const size_t size) {
    if(message == NULL && size == 0)
        return True;
    if(message == NULL) {
        zck_log(ZCK_LOG_ERROR,
                "Hash data is supposed to have %lu bytes, but is NULL\n", size);
        return False;
    }
    if(size == 0) {
        zck_log(ZCK_LOG_ERROR,
                "Hash data is supposed to be 0-length, but is not NULL\n");
        return False;
    }
    if(hash && hash->ctx && hash->type) {
        if(hash->type->type == ZCK_HASH_SHA1) {
            SHA1_Update((SHA_CTX *)hash->ctx, (const sha1_byte *)message, size);
            return True;
        } else if(hash->type->type == ZCK_HASH_SHA256) {
            sha256_update((sha256_ctx *)hash->ctx, (const unsigned char *)message, size);
            return True;
        }
        zck_log(ZCK_LOG_ERROR, "Unsupported hash type: %i\n", hash->type);
        return False;
    }
    zck_log(ZCK_LOG_ERROR, "Hash hasn't been initialized\n");
    return False;
}

char *hash_finalize(zckHash *hash) {
    if(hash && hash->ctx && hash->type) {
        if(hash->type->type == ZCK_HASH_SHA1) {
            unsigned char *digest = zmalloc(hash->type->digest_size);
            SHA1_Final((sha1_byte*)digest, (SHA_CTX *)hash->ctx);
            hash_close(hash);
            return (char *)digest;
        } else if(hash->type->type == ZCK_HASH_SHA256) {
            unsigned char *digest = zmalloc(hash->type->digest_size);
            sha256_final((sha256_ctx *)hash->ctx, digest);
            hash_close(hash);
            return (char *)digest;
        }
        zck_log(ZCK_LOG_ERROR, "Unsupported hash type: %i\n", hash->type);
        return NULL;
    }
    zck_log(ZCK_LOG_ERROR, "Hash hasn't been initialized\n");
    return NULL;
}

int set_full_hash_type(zckCtx *zck, int hash_type) {
    VALIDATE(zck);
    zck_log(ZCK_LOG_INFO, "Setting full hash to %s\n",
            zck_hash_name_from_type(hash_type));
    if(!hash_setup(&(zck->hash_type), hash_type)) {
        zck_log(ZCK_LOG_ERROR, "Unable to set full hash to %s\n",
                zck_hash_name_from_type(hash_type));
        return False;
    }
    hash_close(&(zck->full_hash));
    if(!hash_init(&(zck->full_hash), &(zck->hash_type))) {
        zck_log(ZCK_LOG_ERROR, "Unable initialize full hash\n");
        return False;
    }
    return True;
}

int set_chunk_hash_type(zckCtx *zck, int hash_type) {
    VALIDATE(zck);
    memset(&(zck->chunk_hash_type), 0, sizeof(zckHashType));
    zck_log(ZCK_LOG_DEBUG, "Setting chunk hash to %s\n",
            zck_hash_name_from_type(hash_type));
    if(!hash_setup(&(zck->chunk_hash_type), hash_type)) {
        zck_log(ZCK_LOG_ERROR, "Unable to set chunk hash to %s\n",
                zck_hash_name_from_type(hash_type));
        return False;
    }
    zck->index.hash_type = zck->chunk_hash_type.type;
    zck->index.digest_size = zck->chunk_hash_type.digest_size;
    return True;
}

/* Validate chunk, returning -1 if checksum fails, 1 if good, 0 if error */
int validate_chunk(zckCtx *zck, zckIndexItem *idx,
                       zck_log_type bad_checksum) {
    VALIDATE(zck);
    if(idx == NULL) {
        zck_log(ZCK_LOG_ERROR, "Index not initialized\n");
        return 0;
    }

    char *digest = hash_finalize(&(zck->check_chunk_hash));
    if(digest == NULL) {
        zck_log(ZCK_LOG_ERROR,
                "Unable to calculate %s checksum for chunk\n");
        return 0;
    }
    if(idx->comp_length == 0)
        memset(digest, 0, idx->digest_size);
    zck_log(ZCK_LOG_DEBUG, "Checking chunk checksum\n");
    char *pdigest = zck_get_chunk_digest(idx);
    zck_log(ZCK_LOG_DEBUG, "Expected chunk checksum:   %s\n", pdigest);
    free(pdigest);
    pdigest = get_digest_string(digest, idx->digest_size);
    zck_log(ZCK_LOG_DEBUG, "Calculated chunk checksum: %s\n", pdigest);
    free(pdigest);
    if(memcmp(digest, idx->digest, idx->digest_size) != 0) {
        free(digest);
        zck_log(bad_checksum, "Chunk checksum failed!\n");
        return -1;
    }
    zck_log(ZCK_LOG_DEBUG, "Chunk checksum valid\n");
    free(digest);
    return 1;
}

int validate_current_chunk(zckCtx *zck) {
    VALIDATE(zck);

    return validate_chunk(zck, zck->comp.data_idx, ZCK_LOG_ERROR);
}

int validate_file(zckCtx *zck, zck_log_type bad_checksums) {
    VALIDATE(zck);
    char *digest = hash_finalize(&(zck->check_full_hash));
    if(digest == NULL) {
        zck_log(ZCK_LOG_ERROR,
                "Unable to calculate %s checksum for full file\n");
        return 0;
    }
    zck_log(ZCK_LOG_DEBUG, "Checking data checksum\n");
    char *cks = get_digest_string(zck->full_hash_digest,
                                  zck->hash_type.digest_size);
    zck_log(ZCK_LOG_DEBUG, "Expected data checksum:   %s\n", cks);
    free(cks);
    cks = get_digest_string(digest, zck->hash_type.digest_size);
    zck_log(ZCK_LOG_DEBUG, "Calculated data checksum: %s\n", cks);
    free(cks);
    if(memcmp(digest, zck->full_hash_digest, zck->hash_type.digest_size) != 0) {
        free(digest);
        zck_log(bad_checksums, "Data checksum failed!\n");
        return -1;
    }
    zck_log(ZCK_LOG_DEBUG, "Data checksum valid\n");
    free(digest);
    return 1;
}

int validate_header(zckCtx *zck) {
    VALIDATE(zck);
    char *digest = hash_finalize(&(zck->check_full_hash));
    if(digest == NULL) {
        zck_log(ZCK_LOG_ERROR,
                "Unable to calculate %s checksum for header\n");
        return 0;
    }
    zck_log(ZCK_LOG_DEBUG, "Checking header checksum\n");
    char *cks = get_digest_string(zck->header_digest,
                                  zck->hash_type.digest_size);
    zck_log(ZCK_LOG_DEBUG, "Expected header checksum:   %s\n", cks);
    free(cks);
    cks = get_digest_string(digest, zck->hash_type.digest_size);
    zck_log(ZCK_LOG_DEBUG, "Calculated header checksum: %s\n", cks);
    free(cks);
    if(memcmp(digest, zck->header_digest, zck->hash_type.digest_size) != 0) {
        free(digest);
        zck_log(ZCK_LOG_ERROR, "Header checksum failed!\n");
        return -1;
    }
    zck_log(ZCK_LOG_DEBUG, "Header checksum valid\n");
    free(digest);

    if(!hash_init(&(zck->check_full_hash), &(zck->hash_type)))
        return 0;

    return 1;
}

/* Returns 1 if data hash matches, -1 if it doesn't and 0 if error */
int PUBLIC zck_validate_data_checksum(zckCtx *zck) {
    hash_close(&(zck->check_full_hash));
    if(!seek_data(zck->fd, zck->data_offset, SEEK_SET))
        return -1;
    if(!hash_init(&(zck->check_full_hash), &(zck->hash_type)))
        return -1;
    char buf[BUF_SIZE] = {0};
    zckIndexItem *idx = zck->index.first;
    zck_log(ZCK_LOG_DEBUG, "Checking full hash\n");
    while(idx) {
        size_t to_read = idx->comp_length;
        while(to_read > 0) {
            size_t rb = BUF_SIZE;
            if(rb > to_read)
                rb = to_read;
            if(!read_data(zck->fd, buf, rb))
                return -1;
            if(!hash_update(&(zck->check_full_hash), buf, rb))
                return -1;
            to_read -= rb;
        }
        idx = idx->next;
    }
    return validate_file(zck, ZCK_LOG_WARNING);
}

const char PUBLIC *zck_hash_name_from_type(int hash_type) {
    if(hash_type > 1 || hash_type < 0) {
        snprintf(unknown+8, 21, "%i)", hash_type);
        return unknown;
    }
    return HASH_NAME[hash_type];
}

int PUBLIC zck_get_full_hash_type(zckCtx *zck) {
    if(zck == NULL)
        return -1;
    return zck->hash_type.type;
}

ssize_t PUBLIC zck_get_full_digest_size(zckCtx *zck) {
    if(zck == NULL)
        return -1;
    return zck->hash_type.digest_size;
}

int PUBLIC zck_get_chunk_hash_type(zckCtx *zck) {
    if(zck == NULL)
        return -1;
    return zck->index.hash_type;
}

ssize_t PUBLIC zck_get_chunk_digest_size(zckCtx *zck) {
    if(zck == NULL)
        return -1;
    return zck->index.digest_size;
}

char PUBLIC *zck_get_header_digest(zckCtx *zck) {
    if(zck == NULL)
        return NULL;
    return get_digest_string(zck->header_digest, zck->hash_type.digest_size);
}

char PUBLIC *zck_get_data_digest(zckCtx *zck) {
    if(zck == NULL)
        return NULL;
    return get_digest_string(zck->full_hash_digest, zck->hash_type.digest_size);
}

char PUBLIC *zck_get_chunk_digest(zckIndexItem *item) {
    if(item == NULL)
        return NULL;
    return get_digest_string(item->digest, item->digest_size);
}

/* Returns 1 if all chunks are valid, -1 if even one isn't and 0 if error */
int PUBLIC zck_find_valid_chunks(zckCtx *zck) {
    return validate_checksums(zck, ZCK_LOG_DEBUG);
}

/* Returns 1 if all checksums matched, -1 if even one doesn't and 0 if error */
int PUBLIC zck_validate_checksums(zckCtx *zck) {
    return validate_checksums(zck, ZCK_LOG_WARNING);
}
