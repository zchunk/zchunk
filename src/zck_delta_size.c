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
#include <argp.h>
#include <zck.h>

#include "util_common.h"

static char doc[] = "zck_delta_size - Calculate the difference between"
                    " two zchunk files";

static char args_doc[] = "<file 1> <file 2>";

static struct argp_option options[] = {
    {"verbose", 'v', 0,        0,
     "Increase verbosity (can be specified more than once for debugging)"},
    {"version", 'V', 0,        0, "Show program version"},
    { 0 }
};

struct arguments {
  char *args[2];
  zck_log_type log_level;
};

static error_t parse_opt (int key, char *arg, struct argp_state *state) {
    struct arguments *arguments = state->input;

    switch (key) {
        case 'v':
            arguments->log_level--;
            if(arguments->log_level < ZCK_LOG_DDEBUG)
                arguments->log_level = ZCK_LOG_DDEBUG;
            break;
        case 'V':
            version();
            break;

        case ARGP_KEY_ARG:
            if (state->arg_num >= 2) {
                argp_usage (state);
                return EINVAL;
            }
            arguments->args[state->arg_num] = arg;

            break;

        case ARGP_KEY_END:
            if (state->arg_num < 2) {
                argp_usage (state);
                return EINVAL;
            }
            break;

        default:
            return ARGP_ERR_UNKNOWN;
    }
    return 0;
}

static struct argp argp = {options, parse_opt, args_doc, doc};

int main (int argc, char *argv[]) {
    struct arguments arguments = {0};

    /* Defaults */
    arguments.log_level = ZCK_LOG_ERROR;

    argp_parse (&argp, argc, argv, 0, 0, &arguments);

    zck_set_log_level(arguments.log_level);

    int src_fd = open(arguments.args[0], O_RDONLY);
    if(src_fd < 0) {
        printf("Unable to open %s\n", arguments.args[0]);
        perror("");
        exit(1);
    }
    zckCtx *zck_src = zck_create();
    if(zck_src == NULL) {
        printf("Unable to create zchunk context\n");
        exit(1);
    }
    if(!zck_init_read(zck_src, src_fd)) {
        printf("Error reading %s: %s", arguments.args[0],
               zck_get_error(zck_src));
        exit(1);
    }
    close(src_fd);

    int tgt_fd = open(arguments.args[1], O_RDONLY);
    if(tgt_fd < 0) {
        printf("Unable to open %s\n", arguments.args[1]);
        perror("");
        exit(1);
    }
    zckCtx *zck_tgt = zck_create();
    if(zck_tgt == NULL) {
        printf("Unable to create zchunk context\n");
        exit(1);
    }
    if(!zck_init_read(zck_tgt, tgt_fd)) {
        printf("Error reading %s: %s", arguments.args[1],
               zck_get_error(zck_tgt));
        exit(1);
    }
    close(tgt_fd);

    if(zck_get_chunk_hash_type(zck_tgt) != zck_get_chunk_hash_type(zck_src)) {
        printf("ERROR: Chunk hash types don't match:\n");
        printf("   %s: %s\n", arguments.args[0], zck_hash_name_from_type(zck_get_chunk_hash_type(zck_tgt)));
        printf("   %s: %s\n", arguments.args[1], zck_hash_name_from_type(zck_get_chunk_hash_type(zck_src)));
        exit(1);
    }
    zckChunk *tgt_idx = zck_get_first_chunk(zck_tgt);
    zckChunk *src_idx = zck_get_first_chunk(zck_src);
    if(tgt_idx == NULL || src_idx == NULL)
        exit(1);

    if(!zck_compare_chunk_digest(tgt_idx, src_idx))
        printf("WARNING: Dicts don't match\n");
    ssize_t dl_size = zck_get_header_length(zck_tgt);
    if(dl_size < 0)
        exit(1);
    ssize_t total_size = zck_get_header_length(zck_tgt);
    ssize_t matched_chunks = 0;
    for(tgt_idx = zck_get_first_chunk(zck_tgt); tgt_idx;
        tgt_idx = zck_get_next_chunk(tgt_idx)) {
        int found = False;
        for(src_idx = zck_get_first_chunk(zck_src); src_idx;
            src_idx = zck_get_next_chunk(src_idx)) {
            if(zck_compare_chunk_digest(tgt_idx, src_idx)) {
                found = True;
                break;
            }
        }
        if(!found) {
            dl_size += zck_get_chunk_comp_size(tgt_idx);
        } else {
            matched_chunks += 1;
        }
        total_size += zck_get_chunk_comp_size(tgt_idx);
    }
    printf("Would download %li of %li bytes\n", (long)dl_size,
           (long)total_size);
    printf("Matched %li of %lu chunks\n", (long)matched_chunks,
           (long unsigned)zck_get_chunk_count(zck_tgt));
    zck_free(&zck_tgt);
    zck_free(&zck_src);
}
