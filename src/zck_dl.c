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
#include <stdint.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <libgen.h>
#include <errno.h>
#include <zck.h>

int main (int argc, char *argv[]) {
    zckCtx *zck_src = zck_create();
    zckCtx *zck_tgt = zck_create();

    if(zck_src == NULL || zck_tgt == NULL)
        exit(1);

    zck_dl_global_init();
    zck_set_log_level(ZCK_LOG_DEBUG);

    if(argc != 3) {
        printf("Usage: %s <source> <target url>\n", argv[0]);
        exit(1);
    }

    int src_fd = open(argv[1], O_RDONLY);
    if(src_fd < 0) {
        printf("Unable to open %s\n", argv[1]);
        perror("");
        exit(1);
    }
    if(!zck_read_header(zck_src, src_fd)) {
        printf("Unable to open %s\n", argv[1]);
        exit(1);
    }

    zckDL *dl = zck_dl_init();
    if(dl == NULL)
        exit(1);
    dl->zck = zck_tgt;

    char *outname_full = calloc(1, strlen(argv[2])+1);
    memcpy(outname_full, argv[2], strlen(argv[2]));
    char *outname = basename(outname_full);
    int dst_fd = open(outname, O_EXCL | O_RDWR | O_CREAT, 0644);
    if(dst_fd < 0) {
        printf("Unable to open %s: %s\n", outname, strerror(errno));
        free(outname_full);
        exit(1);
    }
    free(outname_full);
    dl->dst_fd = dst_fd;

    if(!zck_dl_get_header(zck_tgt, dl, argv[2]))
        exit(1);

    zck_range_close(&(dl->info));
    if(!zck_dl_copy_src_chunks(&(dl->info), zck_src, zck_tgt))
        exit(1);
    int max_ranges = 256;
    if(!zck_range_calc_segments(&(dl->info), max_ranges))
        exit(1);

    lseek(dl->dst_fd, 0, SEEK_SET);
    if(!zck_dl_range(dl, argv[2], 1))
        exit(1);


    printf("Downloaded %lu bytes\n", zck_dl_get_bytes_downloaded(dl));
    switch(zck_hash_check_full_file(dl->zck, dl->dst_fd)) {
        case -1:
            exit(1);
            break;
        case 0:
            exit(1);
            break;
        default:
            break;
    }
    zck_dl_free(&dl);
    zck_free(&zck_tgt);
    zck_free(&zck_src);
    zck_dl_global_cleanup();
    exit(0);
}
