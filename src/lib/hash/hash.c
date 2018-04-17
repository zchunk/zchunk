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

static char unknown[] = "Unknown(\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0";

const static char *HASH_NAME[] = {
    "SHA-1",
    "SHA-256"
};

int zck_hash_setup(zckHashType *ht, int h) {
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

int zck_hash_init(zckHash *hash, zckHashType *hash_type) {
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

int zck_hash_update(zckHash *hash, const char *message, const size_t size) {
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

void zck_hash_close(zckHash *hash) {
    if(!hash)
        return;

    if(hash->ctx) {
        free(hash->ctx);
        hash->ctx = NULL;
    }
    hash->type = NULL;
    return;
}

/* Returns 1 if data hash matches, 0 if it doesn't and -1 if failure */
int PUBLIC zck_hash_check_data(zckCtx *zck, int dst_fd) {
    if(!seek_data(dst_fd, zck->header_size + zck->index_size, SEEK_SET))
        return -1;
    if(!zck_hash_init(&(zck->check_full_hash), &(zck->hash_type)))
        return -1;
    char buf[BUF_SIZE] = {0};
    zckIndexItem *idx = zck->index.first;
    zck_log(ZCK_LOG_INFO, "Checking full hash\n");
    while(idx) {
        size_t to_read = idx->comp_length;
        while(to_read > 0) {
            size_t rb = BUF_SIZE;
            if(rb > to_read)
                rb = to_read;
            if(!read_data(dst_fd, buf, rb))
                return -1;
            if(!zck_hash_update(&(zck->check_full_hash), buf, rb))
                return -1;
            to_read -= rb;
        }
        idx = idx->next;
    }
    return zck_validate_file(zck);
    return True;
}

char *zck_hash_finalize(zckHash *hash) {
    if(hash && hash->ctx && hash->type) {
        if(hash->type->type == ZCK_HASH_SHA1) {
            unsigned char *digest = zmalloc(hash->type->digest_size);
            SHA1_Final((sha1_byte*)digest, (SHA_CTX *)hash->ctx);
            zck_hash_close(hash);
            return (char *)digest;
        } else if(hash->type->type == ZCK_HASH_SHA256) {
            unsigned char *digest = zmalloc(hash->type->digest_size);
            sha256_final((sha256_ctx *)hash->ctx, digest);
            zck_hash_close(hash);
            return (char *)digest;
        }
        zck_log(ZCK_LOG_ERROR, "Unsupported hash type: %i\n", hash->type);
        return NULL;
    }
    zck_log(ZCK_LOG_ERROR, "Hash hasn't been initialized\n");
    return NULL;
}

const char PUBLIC *zck_hash_name_from_type(int hash_type) {
    if(hash_type > 1) {
        snprintf(unknown+8, 21, "%i)", hash_type);
        return unknown;
    }
    return HASH_NAME[hash_type];
}

int set_full_hash_type(zckCtx *zck, int hash_type) {
    VALIDATE(zck);
    zck_log(ZCK_LOG_INFO, "Setting full hash to %s\n",
            zck_hash_name_from_type(hash_type));
    if(!zck_hash_setup(&(zck->hash_type), hash_type)) {
        zck_log(ZCK_LOG_ERROR, "Unable to set full hash to %s\n",
                zck_hash_name_from_type(hash_type));
        return False;
    }
    zck_hash_close(&(zck->full_hash));
    if(!zck_hash_init(&(zck->full_hash), &(zck->hash_type))) {
        zck_log(ZCK_LOG_ERROR, "Unable initialize full hash\n");
        return False;
    }
    return True;
}

int set_chunk_hash_type(zckCtx *zck, int hash_type) {
    VALIDATE(zck);
    memset(&(zck->chunk_hash_type), 0, sizeof(zckHashType));
    zck_log(ZCK_LOG_INFO, "Setting chunk hash to %s\n",
            zck_hash_name_from_type(hash_type));
    if(!zck_hash_setup(&(zck->chunk_hash_type), hash_type)) {
        zck_log(ZCK_LOG_ERROR, "Unable to set chunk hash to %s\n",
                zck_hash_name_from_type(hash_type));
        return False;
    }
    zck->index.hash_type = zck->chunk_hash_type.type;
    zck->index.digest_size = zck->chunk_hash_type.digest_size;
    return True;
}
