#ifndef __LIBSHA_H__
#define __LIBSHA_H__

#include "zck_private.h"
#include "sha1/sha1.h"
#include "sha2/sha2.h"

#define SHA256_CTX sha256_ctx
#define SHA256_Init sha256_init
#define SHA256_Update sha256_update

#define SHA512_CTX sha512_ctx
#define SHA512_Init sha512_init
#define SHA512_Update sha512_update

void lib_hash_ctx_close(zckHash *hash);
bool lib_hash_init(zckCtx *zck, zckHash *hash);
bool lib_hash_update(zckCtx *zck, zckHash *hash, const char *message, const size_t size);
char *lib_hash_final(zckCtx *zck, zckHash *hash);

#endif
