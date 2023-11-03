/*
 * Copyright 2018 Jonathan Dieter <jdieter@gmail.com>
 * Copyright 2023 Matt Wood <matt.wood@microchip.com>
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
#include <zck.h>
#include "zck_private.h"
#include "libsha.h"

static void SHA256_Final(unsigned char *md, SHA256_CTX *c)
{
    sha256_final(c, md);
}

static void SHA512_Final(unsigned char *md, SHA512_CTX *c)
{
    sha512_final(c, md);
}

void lib_hash_ctx_close(zckHash *hash)
{
        free(hash->ctx);
}

bool lib_hash_init(zckCtx *zck, zckHash *hash)
{
        if(hash->type->type == ZCK_HASH_SHA1) {
                zck_log(ZCK_LOG_DDEBUG, "Initializing SHA-1 hash");
                hash->ctx = zmalloc(sizeof(SHA_CTX));
                if (!hash->ctx) {
                        zck_log(ZCK_LOG_ERROR, "OOM in %s", __func__);
                        return false;
                }
                SHA1_Init((SHA_CTX *) hash->ctx);
                return true;
        } else if(hash->type->type == ZCK_HASH_SHA256) {
                 zck_log(ZCK_LOG_DDEBUG, "Initializing SHA-256 hash");
                hash->ctx = zmalloc(sizeof(SHA256_CTX));
                if (!hash->ctx) {
                        zck_log(ZCK_LOG_ERROR, "OOM in %s", __func__);
                        return false;
                }
                SHA256_Init((SHA256_CTX *) hash->ctx);
                return true;
        } else if(hash->type->type >= ZCK_HASH_SHA512 &&
                hash->type->type <= ZCK_HASH_SHA512_128) {
                zck_log(ZCK_LOG_DDEBUG, "Initializing SHA-512 hash");
                hash->ctx = zmalloc(sizeof(SHA512_CTX));
                if (!hash->ctx) {
                        zck_log(ZCK_LOG_ERROR, "OOM in %s", __func__);
                        return false;
                }
                SHA512_Init((SHA512_CTX *) hash->ctx);
                return true;
        }
        set_error(zck, "Unsupported hash type: %s", zck_hash_name_from_type(hash->type->type));
        return false;
}

bool lib_hash_update(zckCtx *zck, zckHash *hash, const char *message, const size_t size)
{
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
        set_error(zck, "Unsupported hash type: %s", zck_hash_name_from_type(hash->type->type));
        return false;
}

char *lib_hash_final(zckCtx *zck, zckHash *hash)
{
        if(hash->type->type == ZCK_HASH_SHA1) {
        unsigned char *digest = zmalloc(SHA1_DIGEST_LENGTH);
        if (!digest) {
           zck_log(ZCK_LOG_ERROR, "OOM in %s", __func__);
           return false;
        }
        SHA1_Final((sha1_byte*)digest, (SHA_CTX *)hash->ctx);
        hash_close(hash);
        return (char *)digest;
    } else if(hash->type->type == ZCK_HASH_SHA256) {
        unsigned char *digest = zmalloc(SHA256_DIGEST_SIZE);
        if (!digest) {
           zck_log(ZCK_LOG_ERROR, "OOM in %s", __func__);
           return false;
        }
        SHA256_Final(digest, (SHA256_CTX *)hash->ctx);
        hash_close(hash);
        return (char *)digest;
    } else if(hash->type->type >= ZCK_HASH_SHA512 &&
              hash->type->type <= ZCK_HASH_SHA512_128) {
        unsigned char *digest = zmalloc(SHA512_DIGEST_SIZE);
        if (!digest) {
           zck_log(ZCK_LOG_ERROR, "OOM in %s", __func__);
           return false;
        }
        SHA512_Final(digest, (SHA512_CTX *)hash->ctx);
        hash_close(hash);
        return (char *)digest;
    }
    set_error(zck, "Unsupported hash type: %s", zck_hash_name_from_type(hash->type->type));
    hash_close(hash);
    return NULL;
}
