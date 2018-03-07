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
#include <zck.h>

int main (int argc, char *argv[]) {
    zckCtx *zck_src = zck_create();
    zckCtx *zck_tgt = zck_create();

    /*zck_set_log_level(ZCK_LOG_DEBUG);*/

    if(argc != 3) {
        printf("Usage: %s <source> <target>\n", argv[0]);
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
    close(src_fd);

    int tgt_fd = open(argv[2], O_RDONLY);
    if(tgt_fd < 0) {
        printf("Unable to open %s\n", argv[2]);
        perror("");
        exit(1);
    }
    if(!zck_read_header(zck_tgt, tgt_fd)) {
        printf("Unable to open %s\n", argv[2]);
        exit(1);
    }
    close(tgt_fd);

    if(zck_get_chunk_hash_type(zck_tgt) != zck_get_chunk_hash_type(zck_src)) {
        printf("ERROR: Chunk hash types don't match:\n");
        printf("   %s: %s\n", argv[1], zck_hash_name_from_type(zck_get_chunk_hash_type(zck_tgt)));
        printf("   %s: %s\n", argv[2], zck_hash_name_from_type(zck_get_chunk_hash_type(zck_src)));
        return 1;
    }
    zckIndex *tgt_idx = zck_get_index(zck_tgt);
    zckIndex *src_idx = zck_get_index(zck_src);
    if(memcmp(tgt_idx->digest, src_idx->digest, zck_get_chunk_digest_size(zck_tgt)) != 0)
        printf("WARNING: Dicts don't match\n");
    int dl_size = 0;
    int total_size = 0;
    int matched_chunks = 0;
    while(tgt_idx) {
        int found = False;
        src_idx = zck_get_index(zck_src);

        while(src_idx) {
            if(memcmp(tgt_idx->digest, src_idx->digest, zck_get_chunk_digest_size(zck_tgt)) == 0) {
                found = True;
                break;
            }
            src_idx = src_idx->next;
        }
        if(!found) {
            dl_size += tgt_idx->length;
        } else {
            matched_chunks += 1;
        }
        total_size += tgt_idx->length;
        tgt_idx = tgt_idx->next;
    }
    printf("Would download %i of %i bytes\n", dl_size, total_size);
    printf("Matched %i of %lu chunks\n", matched_chunks, zck_get_index_count(zck_tgt));
    zck_free(zck_tgt);
    zck_free(zck_src);
}
