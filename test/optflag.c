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
    zck_set_log_level(ZCK_LOG_DEBUG);
    char data[1000] = {0};

    /* Open zchunk file and verify that zck->has_optional_flags is set */
    int in = open(argv[1], O_RDONLY);
    if(in < 0) {
        perror("Unable to open empty.zck for reading");
        exit(1);
    }

    zckCtx *zck = zck_create();
    if(zck == NULL)
        exit(1);
    if(!zck_init_read(zck, in)) {
        printf("%s", zck_get_error(zck));
        exit(1);
    }
    if(!zck->has_optional_flags) {
        printf("zck->has_optional_flags should be set, but isn't");
        exit(1);
    }
    memset(data, 0, 1000);
    ssize_t len = zck_read(zck, data, 1000);
    if(len > 0) {
        printf("%li bytes read, but file should be empty\n", (long)len);
        exit(1);
    }
    if(!zck_close(zck))
        exit(1);

    zck_free(&zck);
    return 0;
}
