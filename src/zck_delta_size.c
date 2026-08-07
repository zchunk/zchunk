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
#include <stdbool.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#ifndef _AIX
#include <argp.h>
#endif
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
  bool exit;
};

static error_t parse_opt (int key, char *arg, struct argp_state *state) {
    struct arguments *arguments = state->input;

    if(arguments->exit)
        return 0;

    switch (key) {
        case 'v':
            arguments->log_level--;
            if(arguments->log_level < ZCK_LOG_DDEBUG)
                arguments->log_level = ZCK_LOG_DDEBUG;
            break;
        case 'V':
            version();
            arguments->exit = true;
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

    int retval = argp_parse(&argp, argc, argv, 0, 0, &arguments);
    if(retval || arguments.exit)
        exit(retval);

    zck_set_log_level(arguments.log_level);

    int src_fd = open(arguments.args[0], O_RDONLY | O_BINARY);
    if(src_fd < 0) {
        LOG_ERROR("Unable to open %s\n", arguments.args[0]);
        perror("");
        exit(1);
    }
    zckCtx *zck_src = zck_create();
    if(zck_src == NULL) {
        exit(1);
    }
    if(!zck_init_read(zck_src, src_fd)) {
        LOG_ERROR("Error reading %s: %s", arguments.args[0],
                zck_get_error(zck_src));
        zck_free(&zck_src);
        exit(1);
    }
    close(src_fd);

    int tgt_fd = open(arguments.args[1], O_RDONLY | O_BINARY);
    if(tgt_fd < 0) {
        LOG_ERROR("Unable to open %s\n", arguments.args[1]);
        perror("");
        zck_free(&zck_src);
        exit(1);
    }
    zckCtx *zck_tgt = zck_create();
    if(zck_tgt == NULL) {
        zck_free(&zck_src);
        exit(1);
    }
    if(!zck_init_read(zck_tgt, tgt_fd)) {
        LOG_ERROR("Error reading %s: %s", arguments.args[1],
                zck_get_error(zck_tgt));
        zck_free(&zck_src);
        zck_free(&zck_tgt);
        exit(1);
    }
    close(tgt_fd);

    if(zck_get_chunk_hash_type(zck_tgt) != zck_get_chunk_hash_type(zck_src)) {
        LOG_ERROR("ERROR: Chunk hash types don't match:\n");
        LOG_ERROR("   %s: %s\n", arguments.args[0],
                zck_hash_name_from_type(zck_get_chunk_hash_type(zck_tgt)));
        LOG_ERROR("   %s: %s\n", arguments.args[1],
                zck_hash_name_from_type(zck_get_chunk_hash_type(zck_src)));
        zck_free(&zck_src);
        zck_free(&zck_tgt);
        exit(1);
    }
    zckChunk *tgt_idx = zck_get_first_chunk(zck_tgt);
    zckChunk *src_idx = zck_get_first_chunk(zck_src);
    if(tgt_idx == NULL || src_idx == NULL)
        exit(1);

    if(!zck_compare_chunk_digest(tgt_idx, src_idx))
        LOG_ERROR("WARNING: Dicts don't match\n");
    ssize_t dl_size = zck_get_header_length(zck_tgt);
    if(dl_size < 0)
        exit(1);
    ssize_t header_size = zck_get_header_length(zck_tgt);
    ssize_t total_size = header_size;
    ssize_t matched_chunks = 0;
    for(tgt_idx = zck_get_first_chunk(zck_tgt); tgt_idx;
        tgt_idx = zck_get_next_chunk(tgt_idx)) {
        bool found = false;
        for(src_idx = zck_get_first_chunk(zck_src); src_idx;
            src_idx = zck_get_next_chunk(src_idx)) {
            if(zck_compare_chunk_digest(tgt_idx, src_idx)) {
                found = true;
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
    printf("Would download in total %lli of %lli bytes (%lli%%), %lli in the header and the rest in %lli chunks\n",
           (long long) dl_size, (long long) total_size,
           (long long) (dl_size * 100 / total_size),
           (long long) header_size,
           (long long) (zck_get_chunk_count(zck_tgt) - matched_chunks));
    printf("Matched %lli of %llu (%lli%%) chunks\n", (long long) matched_chunks,
           (long long unsigned) zck_get_chunk_count(zck_tgt),
           (long long) (matched_chunks * 100 / zck_get_chunk_count(zck_tgt)));

    zck_free(&zck_tgt);
    zck_free(&zck_src);
}
