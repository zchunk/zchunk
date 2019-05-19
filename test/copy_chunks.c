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
#include <libgen.h>
#include "zck_private.h"
#include "util.h"

int main (int argc, char *argv[]) {
    zck_set_log_level(ZCK_LOG_DEBUG);
    char *path = zmalloc(strlen(argv[1])+1);
    strcpy(path, argv[1]);

    char *base_name = basename(path);
    int in = open(argv[1], O_RDONLY);
    if(in < 0) {
        perror("Unable to open LICENSE.header.new.nodict.fodt.zck for reading");
        exit(1);
    }
    int tgt = open(base_name, O_RDWR | O_CREAT, 0666);
    if(tgt < 0) {
        perror("Unable to open LICENSE.header.new.nodict.fodt.zck for writing");
        exit(1);
    }
    char buffer[4096] = {0};
    int len = 0;
    while((len=read(in, buffer, 4096)) > 0) {
        if(write(tgt, buffer, len) < len) {
            perror("Unable to write to LICENSE.header.new.nodict.fodt.zck");
            exit(1);
        }
    }
    lseek(tgt, 0, SEEK_SET);

    /* Open target zchunk header and read */
    zckCtx *tgt_zck = zck_create();
    if(tgt_zck == NULL)
        exit(1);
    if(!zck_init_adv_read(tgt_zck, tgt)) {
        printf("%s", zck_get_error(tgt_zck));
        exit(1);
    }

    /* Open source zchunk file and read header */
    int src = open(argv[2], O_RDONLY);
    if(src < 0) {
        perror("Unable to open LICENSE.nodict.fodt.zck for reading");
        exit(1);
    }

    zckCtx *src_zck = zck_create();
    if(src_zck == NULL)
        exit(1);
    if(!zck_init_read(src_zck, src)) {
        printf("%s", zck_get_error(src_zck));
        exit(1);
    }

    if(!zck_read_lead(tgt_zck) || !zck_read_header(tgt_zck)) {
        printf("%s", zck_get_error(tgt_zck));
        exit(1);
    }
    if(zck_find_valid_chunks(tgt_zck) > 0) {
        printf("All chunks were valid, but the target shouldn't have any "
               "chunks\n");
        exit(1);
    }
    int out = zck_copy_chunks(src_zck, tgt_zck);
    if(!out) {
        printf("%i: %s%s\n", out, zck_get_error(src_zck), zck_get_error(tgt_zck));
        exit(1);
    }
    zck_reset_failed_chunks(tgt_zck);
    printf("Missing chunks: %i\n", zck_missing_chunks(tgt_zck));
    if(zck_missing_chunks(tgt_zck) != 1) {
        printf("Should be only one chunk missing");
        exit(1);
    }

    if(zck_close(tgt_zck)) {
        printf("Should have run into an error closing "
               "LICENSE.header.new.nodict.fodt.zck");
        exit(1);
    }

    zck_free(&tgt_zck);
    zck_free(&src_zck);
    free(path);
    return 0;
}
