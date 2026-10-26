/*
 * Copyright 2026 Jonathan Dieter <jonathan@dieter.ie>
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

/* Regression test for the read_preface() out-of-bounds read - Only will
 * report an issue when run under a sanitizer */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <zck.h>
#include "zck_private.h"
#include "util.h"

#ifndef O_BINARY
#define O_BINARY 0
#endif

#define TEST_FILE "read_header_bounds.zck"

/* Exit code that tells meson the test was skipped */
#define SKIP 77

/* The over-read is only caught by a memory checker, so only run under
 * valgrind (which preloads its own libraries) or AddressSanitizer */
static bool memory_checked(void) {
#ifdef __SANITIZE_ADDRESS__
    return true;
#else
    const char *preload = getenv("LD_PRELOAD");
    return preload != NULL && strstr(preload, "vgpreload") != NULL;
#endif
}

/* Compute the SHA-256 header digest over the given bytes, using libzck's own
 * hashing so it matches what read_header_from_file() will calculate. */
static char *header_digest(const char *buf, size_t size) {
    zckCtx *zck = zck_create();
    if(zck == NULL)
        return NULL;

    zckHashType ht;
    zckHash hash;
    memset(&hash, 0, sizeof(hash));  /* hash_init() closes it before use */
    if(!hash_setup(zck, &ht, ZCK_HASH_SHA256) ||
       !hash_init(zck, &hash, &ht) ||
       !hash_update(zck, &hash, buf, size)) {
        zck_free(&zck);
        return NULL;
    }
    char *digest = hash_finalize(zck, &hash);
    zck_free(&zck);
    return digest;
}

int main(void) {
    if(!memory_checked()) {
        printf("Skipping: needs valgrind or AddressSanitizer\n");
        return SKIP;
    }

    char cibuf[MAX_COMP_SIZE];
    size_t cilen;

    /* SHA-256 digest size, read from libzck rather than hard-coded. */
    zckHashType ht;
    if(!hash_setup(NULL, &ht, ZCK_HASH_SHA256)) {
        printf("ERROR: unable to set up SHA-256\n");
        exit(1);
    }
    size_t digest_size = ht.digest_size;

    /* --- Header body (the preface) ---
     * full data digest (zero bytes) + flags(0) + a truncated compression-type
     * compint.  The final byte has its high bit clear, so the reader believes
     * the integer continues past the end of the header. */
    char body[64];
    size_t body_len = 0;

    memset(body + body_len, 0, digest_size);         /* data digest */
    body_len += digest_size;

    cilen = 0;
    compint_from_size(cibuf, 0, &cilen);             /* flags = 0 */
    memcpy(body + body_len, cibuf, cilen);
    body_len += cilen;

    body[body_len++] = 0x00;  /* unterminated compression-type compint */

    /* --- Lead ---
     * "\0ZCK1" + compint(hash type) + compint(header length).  The header
     * digest that follows is computed over the lead prefix plus the body,
     * which is exactly the range read_header_from_file() hashes. */
    char lead[16];
    size_t lead_len = 0;

    memcpy(lead, "\0ZCK1", 5);
    lead_len = 5;

    cilen = 0;
    compint_from_size(cibuf, ZCK_HASH_SHA256, &cilen);
    memcpy(lead + lead_len, cibuf, cilen);
    lead_len += cilen;

    cilen = 0;
    compint_from_size(cibuf, body_len, &cilen);
    memcpy(lead + lead_len, cibuf, cilen);
    lead_len += cilen;

    /* digest = SHA256(lead prefix + body) */
    char *to_hash = zmalloc(lead_len + body_len);
    if(to_hash == NULL)
        exit(1);
    memcpy(to_hash, lead, lead_len);
    memcpy(to_hash + lead_len, body, body_len);
    char *digest = header_digest(to_hash, lead_len + body_len);
    free(to_hash);
    if(digest == NULL) {
        printf("ERROR: unable to compute header digest\n");
        exit(1);
    }

    /* Write the file: lead, digest, body */
    int out = open(TEST_FILE, O_WRONLY | O_CREAT | O_TRUNC | O_BINARY, 0666);
    if(out < 0) {
        perror("Unable to open " TEST_FILE " for writing");
        free(digest);
        exit(1);
    }
    if(write(out, lead, lead_len) != (ssize_t)lead_len ||
       write(out, digest, digest_size) != (ssize_t)digest_size ||
       write(out, body, body_len) != (ssize_t)body_len) {
        perror("Short write");
        close(out);
        free(digest);
        exit(1);
    }
    close(out);
    free(digest);

    /* Read it back: the header must be rejected, not over-read */
    int in = open(TEST_FILE, O_RDONLY | O_BINARY);
    if(in < 0) {
        perror("Unable to open " TEST_FILE " for reading");
        exit(1);
    }

    /* Drive the read stages individually so the test targets the preface */
    zckCtx *zck = zck_create();
    if(zck == NULL) {
        close(in);
        exit(1);
    }
    if(!zck_init_adv_read(zck, in) || !zck_read_lead(zck)) {
        printf("ERROR: lead parse failed unexpectedly: %s\n",
               zck_get_error(zck));
        zck_free(&zck);
        close(in);
        unlink(TEST_FILE);
        exit(1);
    }

    int rc = 0;
    if(zck_read_header(zck)) {
        printf("ERROR: zck_read_header accepted a truncated preface\n");
        rc = 1;
    } else {
        printf("zck_read_header correctly rejected truncated preface: %s\n",
               zck_get_error(zck));
    }

    zck_free(&zck);
    close(in);
    unlink(TEST_FILE);
    return rc;
}
