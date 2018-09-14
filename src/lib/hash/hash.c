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
#include <stdbool.h>
#include <string.h>
#include <zck.h>

#include "zck_private.h"

/***** If we're not using OpenSSL, use bundled sha libraries *****/
#ifndef ZCHUNK_OPENSSL
#include "sha1/sha1.h"
#include "sha2/sha2.h"
#define SHA256_CTX sha256_ctx
#define SHA256_Init sha256_init
#define SHA256_Update sha256_update
static void SHA256_Final(unsigned char *md, SHA256_CTX *c) {
    sha256_final(c, md);
}
#define SHA512_CTX sha512_ctx
#define SHA512_Init sha512_init
#define SHA512_Update sha512_update
static void SHA512_Final(unsigned char *md, SHA512_CTX *c) {
    sha512_final(c, md);
}
/***** If we are using OpenSSL, set the defines accordingly *****/
#else
#include <openssl/sha.h>
#define SHA512_DIGEST_SIZE SHA512_DIGEST_LENGTH
#define SHA256_DIGEST_SIZE SHA256_DIGEST_LENGTH
#define SHA1_DIGEST_LENGTH SHA_DIGEST_LENGTH
#define sha1_byte void
#endif

/* This needs to be updated to the largest hash size every time a new hash type
 * is added */
int get_max_hash_size() {
    return SHA512_DIGEST_SIZE;
}

static char unknown[] = "Unknown(\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0";

const static char *HASH_NAME[] = {
    "SHA-1",
    "SHA-256",
    "SHA-512",
    "SHA-512/128"
};

static int validate_checksums(zckCtx *zck, zck_log_type bad_checksums) {
    VALIDATE_READ_BOOL(zck);
    char buf[BUF_SIZE] = {0};

    if(zck->data_offset == 0) {
        set_error(zck, "Header hasn't been read yet");
        return 0;
    }

    if(!hash_init(zck, &(zck->check_full_hash), &(zck->hash_type)))
        return 0;

    if(!seek_data(zck, zck->data_offset, SEEK_SET))
        return 0;

    /* Check each chunk checksum */
    bool all_good = true;
    int count = 0;
    for(zckChunk *idx = zck->index.first; idx; idx = idx->next, count++) {
        if(idx == zck->index.first && idx->length == 0) {
            idx->valid = 1;
            continue;
        }

        if(!hash_init(zck, &(zck->check_chunk_hash), &(zck->chunk_hash_type)))
            return 0;

        size_t rlen = 0;
        while(rlen < idx->comp_length) {
            size_t rsize = BUF_SIZE;
            if(BUF_SIZE > idx->comp_length - rlen)
                rsize = idx->comp_length - rlen;
            if(read_data(zck, buf, rsize) != rsize)
                zck_log(ZCK_LOG_DEBUG, "No more data");
            if(!hash_update(zck, &(zck->check_chunk_hash), buf, rsize))
                return 0;
            if(!hash_update(zck, &(zck->check_full_hash), buf, rsize))
                return 0;
            rlen += rsize;
        }
        int valid_chunk = validate_chunk(zck, idx, bad_checksums, count);
        if(!valid_chunk)
            return 0;
        idx->valid = valid_chunk;
        if(all_good && valid_chunk != 1)
            all_good = false;
    }
    int valid_file = -1;
    if(all_good) {
        /* Check data checksum */
        valid_file = validate_file(zck, bad_checksums);
        if(!valid_file)
            return 0;

        /* If data checksum failed, invalidate *all* chunks */
        if(valid_file == -1)
            for(zckChunk *idx = zck->index.first; idx; idx = idx->next)
                idx->valid = -1;
    }

    /* Go back to beginning of data section */
    if(!seek_data(zck, zck->data_offset, SEEK_SET))
        return 0;

    /* Reinitialize data checksum */
    if(!hash_init(zck, &(zck->check_full_hash), &(zck->hash_type)))
        return 0;

    return valid_file;
}

char *get_digest_string(const char *digest, int size) {
    char *str = zmalloc(size*2+1);

    for(int i=0; i<size; i++)
        snprintf(str + i*2, 3, "%02x", (unsigned char)digest[i]);
    return str;
}

bool hash_setup(zckCtx *zck, zckHashType *ht, int h) {
    if(!ht) {
        set_error(zck, "zckHashType is null");
        return false;
    }
    if(h == ZCK_HASH_SHA1) {
        memset(ht, 0, sizeof(zckHashType));
        ht->type = ZCK_HASH_SHA1;
        ht->digest_size = SHA1_DIGEST_LENGTH;
        zck_log(ZCK_LOG_DEBUG, "Setting up hash type %s",
                zck_hash_name_from_type(ht->type));
        return true;
    } else if(h == ZCK_HASH_SHA256) {
        memset(ht, 0, sizeof(zckHashType));
        ht->type = ZCK_HASH_SHA256;
        ht->digest_size = SHA256_DIGEST_SIZE;
        zck_log(ZCK_LOG_DEBUG, "Setting up hash type %s",
                zck_hash_name_from_type(ht->type));
        return true;
    } else if(h >= ZCK_HASH_SHA512 &&
              h <= ZCK_HASH_SHA512_128) {
        memset(ht, 0, sizeof(zckHashType));
        ht->type = h;
        if(h == ZCK_HASH_SHA512)
            ht->digest_size = SHA512_DIGEST_SIZE;
        else if(h == ZCK_HASH_SHA512_128)
            ht->digest_size = 16;
        zck_log(ZCK_LOG_DEBUG, "Setting up hash type %s",
                zck_hash_name_from_type(ht->type));
        return true;
    }
    set_error(zck, "Unsupported hash type: %s", zck_hash_name_from_type(h));
    return false;
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

void hash_reset(zckHashType *ht) {
    memset(ht, 0, sizeof(zckHashType));
    return;
}

bool hash_init(zckCtx *zck, zckHash *hash, zckHashType *hash_type) {
    hash_close(hash);
    if(hash == NULL || hash_type == NULL) {
        set_error(zck, "Either zckHash or zckHashType struct is null");
        return false;
    }
    if(hash_type->type == ZCK_HASH_SHA1) {
        zck_log(ZCK_LOG_DDEBUG, "Initializing SHA-1 hash");
        hash->ctx = zmalloc(sizeof(SHA_CTX));
        hash->type = hash_type;
        SHA1_Init((SHA_CTX *) hash->ctx);
        return true;
    } else if(hash_type->type == ZCK_HASH_SHA256) {
        zck_log(ZCK_LOG_DDEBUG, "Initializing SHA-256 hash");
        hash->ctx = zmalloc(sizeof(SHA256_CTX));
        hash->type = hash_type;
        SHA256_Init((SHA256_CTX *) hash->ctx);
        return true;
    } else if(hash_type->type >= ZCK_HASH_SHA512 &&
              hash_type->type <= ZCK_HASH_SHA512_128) {
        zck_log(ZCK_LOG_DDEBUG, "Initializing SHA-512 hash");
        hash->ctx = zmalloc(sizeof(SHA512_CTX));
        hash->type = hash_type;
        SHA512_Init((SHA512_CTX *) hash->ctx);
        return true;
    }
    set_error(zck, "Unsupported hash type: %s",
              zck_hash_name_from_type(hash_type->type));
    return false;
}

bool hash_update(zckCtx *zck, zckHash *hash, const char *message,
                const size_t size) {
    if(message == NULL && size == 0)
        return true;
    if(message == NULL) {
        set_error(zck,
                  "Hash data is supposed to have %lu bytes, but is NULL", size);
        return false;
    }
    if(size == 0) {
        set_error(zck,
                  "Hash data is supposed to be 0-length, but is not NULL");
        return false;
    }
    if(hash && hash->ctx && hash->type) {
        if(hash->type->type == ZCK_HASH_SHA1) {
            SHA1_Update((SHA_CTX *)hash->ctx, (const sha1_byte *)message, size);
            return true;
        } else if(hash->type->type == ZCK_HASH_SHA256) {
            SHA256_Update((SHA256_CTX *)hash->ctx,
                          (const unsigned char *)message, size);
            return true;
        } else if(hash->type->type >= ZCK_HASH_SHA512 &&
                  hash->type->type <= ZCK_HASH_SHA512_128) {
            SHA512_Update((SHA512_CTX *)hash->ctx,
                          (const unsigned char *)message, size);
            return true;
        }
        set_error(zck, "Unsupported hash type: %s",
                  zck_hash_name_from_type(hash->type->type));

        return false;
    }
    set_error(zck, "Hash hasn't been initialized");
    return false;
}

char *hash_finalize(zckCtx *zck, zckHash *hash) {
    if(!hash || !hash->ctx || !hash->type) {
        set_error(zck, "Hash hasn't been initialized");
        hash_close(hash);
        return NULL;
    }
    if(hash->type->type == ZCK_HASH_SHA1) {
        unsigned char *digest = zmalloc(SHA1_DIGEST_LENGTH);
        SHA1_Final((sha1_byte*)digest, (SHA_CTX *)hash->ctx);
        hash_close(hash);
        return (char *)digest;
    } else if(hash->type->type == ZCK_HASH_SHA256) {
        unsigned char *digest = zmalloc(SHA256_DIGEST_SIZE);
        SHA256_Final(digest, (SHA256_CTX *)hash->ctx);
        hash_close(hash);
        return (char *)digest;
    } else if(hash->type->type >= ZCK_HASH_SHA512 &&
              hash->type->type <= ZCK_HASH_SHA512_128) {
        unsigned char *digest = zmalloc(SHA512_DIGEST_SIZE);
        SHA512_Final(digest, (SHA512_CTX *)hash->ctx);
        hash_close(hash);
        return (char *)digest;
    }
    set_error(zck, "Unsupported hash type: %s",
              zck_hash_name_from_type(hash->type->type));
    hash_close(hash);
    return NULL;
}

bool set_full_hash_type(zckCtx *zck, int hash_type) {
    VALIDATE_BOOL(zck);

    zck_log(ZCK_LOG_INFO, "Setting full hash to %s",
            zck_hash_name_from_type(hash_type));
    if(!hash_setup(zck, &(zck->hash_type), hash_type)) {
        set_error(zck, "Unable to set full hash");
        return false;
    }
    if(!hash_init(zck, &(zck->full_hash), &(zck->hash_type))) {
        set_error(zck, "Unable initialize full hash");
        return false;
    }
    return true;
}

bool set_chunk_hash_type(zckCtx *zck, int hash_type) {
    VALIDATE_BOOL(zck);

    memset(&(zck->chunk_hash_type), 0, sizeof(zckHashType));
    zck_log(ZCK_LOG_DEBUG, "Setting chunk hash to %s",
            zck_hash_name_from_type(hash_type));
    if(!hash_setup(zck, &(zck->chunk_hash_type), hash_type)) {
        set_error(zck, "Unable to set chunk hash");
        return false;
    }
    zck->index.hash_type = zck->chunk_hash_type.type;
    zck->index.digest_size = zck->chunk_hash_type.digest_size;
    return true;
}

/* Validate chunk, returning -1 if checksum fails, 1 if good, 0 if error */
int validate_chunk(zckCtx *zck, zckChunk *idx,
                       zck_log_type bad_checksum, int chunk_number) {
    VALIDATE_BOOL(zck);
    if(idx == NULL) {
        set_error(zck, "Index not initialized");
        return 0;
    }

    char *digest = hash_finalize(zck, &(zck->check_chunk_hash));
    if(digest == NULL) {
        set_error(zck, "Unable to calculate chunk checksum");
        return 0;
    }
    if(idx->comp_length == 0)
        memset(digest, 0, idx->digest_size);
    char *pdigest = zck_get_chunk_digest(idx);
    zck_log(ZCK_LOG_DDEBUG, "Expected chunk checksum:   %s", pdigest);
    free(pdigest);
    pdigest = get_digest_string(digest, idx->digest_size);
    zck_log(ZCK_LOG_DDEBUG, "Calculated chunk checksum: %s", pdigest);
    free(pdigest);
    if(memcmp(digest, idx->digest, idx->digest_size) != 0) {
        free(digest);
        if(chunk_number == -1)
            zck_log(bad_checksum, "Chunk checksum: FAILED!");
        else
            zck_log(bad_checksum, "Chunk %i's checksum: FAILED",
                    chunk_number);
        return -1;
    }
    if(chunk_number == -1)
        zck_log(ZCK_LOG_DEBUG, "Chunk checksum: valid");
    else
        zck_log(ZCK_LOG_DEBUG, "Chunk %i's checksum: valid", chunk_number);
    free(digest);
    return 1;
}

int validate_current_chunk(zckCtx *zck) {
    VALIDATE_BOOL(zck);

    return validate_chunk(zck, zck->comp.data_idx, ZCK_LOG_ERROR, -1);
}

int validate_file(zckCtx *zck, zck_log_type bad_checksums) {
    VALIDATE_BOOL(zck);
    char *digest = hash_finalize(zck, &(zck->check_full_hash));
    if(digest == NULL) {
        set_error(zck, "Unable to calculate full file checksum");
        return 0;
    }
    zck_log(ZCK_LOG_DEBUG, "Checking data checksum");
    char *cks = get_digest_string(zck->full_hash_digest,
                                  zck->hash_type.digest_size);
    zck_log(ZCK_LOG_DEBUG, "Expected data checksum:   %s", cks);
    free(cks);
    cks = get_digest_string(digest, zck->hash_type.digest_size);
    zck_log(ZCK_LOG_DEBUG, "Calculated data checksum: %s", cks);
    free(cks);
    if(memcmp(digest, zck->full_hash_digest, zck->hash_type.digest_size) != 0) {
        free(digest);
        zck_log(bad_checksums, "Data checksum failed!");
        return -1;
    }
    zck_log(ZCK_LOG_DEBUG, "Data checksum valid");
    free(digest);
    return 1;
}

int validate_header(zckCtx *zck) {
    VALIDATE_BOOL(zck);

    char *digest = hash_finalize(zck, &(zck->check_full_hash));
    if(digest == NULL) {
        set_error(zck, "Unable to calculate header checksum");
        return 0;
    }
    zck_log(ZCK_LOG_DEBUG, "Checking header checksum");
    char *cks = get_digest_string(zck->header_digest,
                                  zck->hash_type.digest_size);
    zck_log(ZCK_LOG_DEBUG, "Expected header checksum:   %s", cks);
    free(cks);
    cks = get_digest_string(digest, zck->hash_type.digest_size);
    zck_log(ZCK_LOG_DEBUG, "Calculated header checksum: %s", cks);
    free(cks);
    if(memcmp(digest, zck->header_digest, zck->hash_type.digest_size) != 0) {
        free(digest);
        zck_log(ZCK_LOG_INFO, "Header checksum failed!");
        return -1;
    }
    zck_log(ZCK_LOG_DEBUG, "Header checksum valid");
    free(digest);

    if(!hash_init(zck, &(zck->check_full_hash), &(zck->hash_type)))
        return 0;

    return 1;
}

/* Returns 1 if data hash matches, -1 if it doesn't and 0 if error */
int PUBLIC zck_validate_data_checksum(zckCtx *zck) {
    VALIDATE_READ_BOOL(zck);

    if(!seek_data(zck, zck->data_offset, SEEK_SET))
        return 0;
    if(!hash_init(zck, &(zck->check_full_hash), &(zck->hash_type)))
        return 0;
    char buf[BUF_SIZE] = {0};
    zckChunk *idx = zck->index.first;
    zck_log(ZCK_LOG_DEBUG, "Checking full hash");
    while(idx) {
        size_t to_read = idx->comp_length;
        while(to_read > 0) {
            size_t rb = BUF_SIZE;
            if(rb > to_read)
                rb = to_read;
            if(!read_data(zck, buf, rb))
                return 0;
            if(!hash_update(zck, &(zck->check_full_hash), buf, rb))
                return 0;
            to_read -= rb;
        }
        idx = idx->next;
    }
    int ret = validate_file(zck, ZCK_LOG_WARNING);
    if(!seek_data(zck, zck->data_offset, SEEK_SET))
        return 0;
    if(!hash_init(zck, &(zck->check_full_hash), &(zck->hash_type)))
        return 0;
    return ret;
}

const char PUBLIC *zck_hash_name_from_type(int hash_type) {
    if(hash_type >= ZCK_HASH_UNKNOWN || hash_type < 0) {
        snprintf(unknown+8, 21, "%i)", hash_type);
        return unknown;
    }
    return HASH_NAME[hash_type];
}

int PUBLIC zck_get_full_hash_type(zckCtx *zck) {
    VALIDATE_INT(zck);

    return zck->hash_type.type;
}

ssize_t PUBLIC zck_get_full_digest_size(zckCtx *zck) {
    VALIDATE_INT(zck);

    return zck->hash_type.digest_size;
}

int PUBLIC zck_get_chunk_hash_type(zckCtx *zck) {
    VALIDATE_INT(zck);

    return zck->index.hash_type;
}

ssize_t PUBLIC zck_get_chunk_digest_size(zckCtx *zck) {
    VALIDATE_INT(zck);

    return zck->index.digest_size;
}

char PUBLIC *zck_get_header_digest(zckCtx *zck) {
    VALIDATE_PTR(zck);

    return get_digest_string(zck->header_digest, zck->hash_type.digest_size);
}

char PUBLIC *zck_get_data_digest(zckCtx *zck) {
    VALIDATE_PTR(zck);

    return get_digest_string(zck->full_hash_digest, zck->hash_type.digest_size);
}

char PUBLIC *zck_get_chunk_digest(zckChunk *item) {
    if(item == NULL)
        return NULL;
    return get_digest_string(item->digest, item->digest_size);
}

/* Returns 1 if all chunks are valid, -1 if even one isn't and 0 if error */
int PUBLIC zck_find_valid_chunks(zckCtx *zck) {
    VALIDATE_READ_BOOL(zck);

    return validate_checksums(zck, ZCK_LOG_DEBUG);
}

/* Returns 1 if all checksums matched, -1 if even one doesn't and 0 if error */
int PUBLIC zck_validate_checksums(zckCtx *zck) {
    VALIDATE_READ_BOOL(zck);

    return validate_checksums(zck, ZCK_LOG_WARNING);
}
