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

/* Regression test for CWE-409: a .zck file whose index declares an
 * implausible uncompressed chunk length must be rejected at parse time,
 * before any large allocation is attempted in import_dict() or
 * end_dchunk(). The accompanying huge_chunk_length.zck was produced by
 * patching a legitimate .zck file's first index entry length to 4 GiB
 * while leaving the compressed length at its original small value. */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <zck.h>
#include "zck_private.h"
#include "util.h"

int main (int argc, char *argv[]) {
    int in = open(argv[1], O_RDONLY | O_BINARY);
    if(in < 0) {
        perror("Unable to open huge_chunk_length.zck for reading");
        exit(1);
    }

    zckCtx *zck = zck_create();
    if(zck == NULL)
        exit(1);

    /* zck_init_read parses the lead, header, and index. The patched
     * file's first index entry claims a 4 GiB uncompressed length while
     * the compressed length is ~2 KiB, so index_read() must reject it
     * with a fatal error before zck_get_chunk_data()/zck_read() ever
     * gets a chance to zmalloc() the bogus length. */
    if(zck_init_read(zck, in)) {
        printf("ERROR: zck_init_read succeeded on a file with an "
               "implausible chunk length\n");
        zck_free(&zck);
        exit(1);
    }

    printf("%s", zck_get_error(zck));
    zck_free(&zck);
    return 0;
}
