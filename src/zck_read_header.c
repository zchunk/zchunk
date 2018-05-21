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
#include <stdint.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <argp.h>
#include <zck.h>

#include "util_common.h"

static char doc[] = "zck_read_header - Read header from a zchunk file";

static char args_doc[] = "<file>";

static struct argp_option options[] = {
    {"verbose", 'v', 0,        0,
     "Increase verbosity (can be specified more than once for debugging)"},
    {"quiet",   'q', 0,        0, "Only show errors"},
    {"version", 'V', 0,        0, "Show program version"},
    {"verify",  'f', 0,        0, "Verify full zchunk file"},
    { 0 }
};

struct arguments {
  char *args[1];
  int verify;
  zck_log_type log_level;
};

static error_t parse_opt (int key, char *arg, struct argp_state *state) {
    struct arguments *arguments = state->input;

    switch (key) {
        case 'v':
            arguments->log_level -= 1;
            if(arguments->log_level < ZCK_LOG_DEBUG)
                arguments->log_level = ZCK_LOG_DEBUG;
            break;
        case 'q':
            arguments->log_level = ZCK_LOG_ERROR;
            break;
        case 'V':
            version();
            break;
        case 'f':
            arguments->verify = True;
            break;

        case ARGP_KEY_ARG:
            if (state->arg_num >= 1) {
                argp_usage (state);
                return EINVAL;
            }
            arguments->args[state->arg_num] = arg;

            break;

        case ARGP_KEY_END:
            if (state->arg_num < 1) {
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
    arguments.log_level = ZCK_LOG_WARNING;

    argp_parse (&argp, argc, argv, 0, 0, &arguments);

    zck_set_log_level(arguments.log_level);

    int src_fd = open(arguments.args[0], O_RDONLY);
    if(src_fd < 0) {
        printf("Unable to open %s\n", arguments.args[0]);
        perror("");
        exit(1);
    }
    zckCtx *zck = zck_init_read(src_fd);
    if(zck == NULL) {
        printf("Unable to read header\n");
        exit(1);
    }

    int valid_cks = 1;
    if(arguments.verify) {
        valid_cks = zck_validate_checksums(zck);
        if(valid_cks < 0)
            exit(1);
    }
    close(src_fd);

    printf("Overall checksum type: %s\n", zck_hash_name_from_type(zck_get_full_hash_type(zck)));
    char *digest = zck_get_header_digest(zck);
    printf("Header checksum: %s\n", digest);
    free(digest);
    digest = zck_get_data_digest(zck);
    printf("Data checksum: %s\n", digest);
    free(digest);
    printf("Index count: %lu\n", (long unsigned)zck_get_index_count(zck));
    printf("Chunk checksum type: %s\n", zck_hash_name_from_type(zck_get_chunk_hash_type(zck)));

    zckIndex *idxi = zck_get_index(zck);
    if(idxi == NULL)
        exit(1);
    zckIndexItem *idx = idxi->first;
    while(idx) {
        char *digest = zck_get_chunk_digest(idx);
        if(digest == NULL)
            exit(1);
        printf("%s %12lu %12lu %12lu", digest,
               (long unsigned)(idx->start + zck_get_header_length(zck)),
               (long unsigned)idx->comp_length, (long unsigned)idx->length);
        if(arguments.verify) {
            if(idx->valid)
                printf("  +");
            else
                printf("  !");
        }
        printf("\n");
        free(digest);
        idx = idx->next;
    }
    zck_free(&zck);
    return 1-valid_cks;
}
