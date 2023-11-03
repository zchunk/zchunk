#ifndef __OPENSSL_H__
#define __OPENSSL_H__

#include "zck_private.h"
#include <openssl/sha.h>

#define SHA1_DIGEST_LENGTH      SHA_DIGEST_LENGTH
#define SHA224_DIGEST_SIZE      SHA224_DIGEST_LENGTH
#define SHA256_DIGEST_SIZE      SHA256_DIGEST_LENGTH
#define SHA384_DIGEST_SIZE      SHA384_DIGEST_LENGTH
#define SHA512_DIGEST_SIZE      SHA512_DIGEST_LENGTH

#if defined(ZCHUNK_OPENSSL_DEPRECATED)
/***** using legacy OpenSSL API *****/
#define sha1_byte void
#else
/***** using OpenSSL3 EVP API *****/
#include <openssl/evp.h>
#endif

void lib_hash_ctx_close(zckHash *hash);
bool lib_hash_init(zckCtx *zck, zckHash *hash);
bool lib_hash_update(zckCtx *zck, zckHash *hash, const char *message, const size_t size);
char *lib_hash_final(zckCtx *zck, zckHash *hash);

#endif
