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

/* Check that the multipart parser rejects malicious server responses */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <zck.h>
#include "zck_private.h"
#include "util.h"

static zckCtx *zck = NULL;
static zckDL *dl = NULL;

/* Set up a fresh download context and feed it a Content-Type header */
static void setup(const char *boundary) {
    if(dl)
        zck_dl_free(&dl);
    zck_free(&zck);
    zck = zck_create();
    if(zck == NULL)
        exit(1);
    dl = zck_dl_init(zck);
    if(dl == NULL)
        exit(1);

    char header[256];
    int len = snprintf(header, sizeof(header),
                       "Content-Type: multipart/byteranges; boundary=%s\r\n",
                       boundary);
    zck_header_cb(header, 1, len, dl);
}

static void fail(const char *msg) {
    printf("ERROR: %s\n", msg);
    exit(1);
}

int main(void) {
    /* Boundaries are matched literally, even when they contain regex syntax
     * or characters outside RFC 2046 (IIS wraps its boundary in <>) */
    char wrong[] = "\r\n--<abbc>\r\nContent-Range: bytes 0-9/10\r\n\r\nx";
    setup("<a(b*)*c>");
    if(dl->boundary == NULL)
        fail("boundary was rejected");
    if(multipart_extract(dl, wrong, strlen(wrong)) == 0 ||
       dl->mp->state != 0)
        fail("boundary was treated as a regex");

    /* There's nowhere to write the matched range, so extraction fails after
     * the part header is parsed; the parser state shows whether it matched */
    char right[] = "\r\n--<a(b*)*c>\r\nContent-Range: bytes 0-9/10\r\n\r\nx";
    setup("<a(b*)*c>");
    size_t ret = multipart_extract(dl, right, strlen(right));
    if(dl->mp->state != 1)
        fail("boundary didn't match itself");
    (void)ret;

    /* A range that ends before it starts is rejected */
    char reversed[] = "\r\n--abc\r\nContent-Range: bytes 100-0/200\r\n\r\nx";
    setup("abc");
    if(multipart_extract(dl, reversed, strlen(reversed)) != 0 ||
       dl->mp->state != 0)
        fail("reversed range was accepted");

    /* A range value that overflows is rejected */
    char overflow[] = "\r\n--abc\r\nContent-Range: bytes "
                      "0-99999999999999999999999999/200\r\n\r\nx";
    setup("abc");
    if(multipart_extract(dl, overflow, strlen(overflow)) != 0 ||
       dl->mp->state != 0)
        fail("overflowing range was accepted");

    /* A part header that never ends isn't buffered forever */
    size_t big_len = 128*1024;
    char *big = zmalloc(big_len);
    if(big == NULL)
        exit(1);
    memset(big, 'x', big_len);
    memcpy(big, "\r\n--abc\r\n", 9);
    setup("abc");
    if(multipart_extract(dl, big, big_len) != 0)
        fail("oversized part header was buffered");
    free(big);

    zck_dl_free(&dl);
    zck_free(&zck);
    return 0;
}
