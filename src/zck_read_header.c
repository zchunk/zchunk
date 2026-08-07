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
#define STDERR_FILENO 2

#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
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

static char doc[] = "zck_read_header - Read header from a zchunk file";

static char args_doc[] = "<file>";

static struct argp_option options[] = {
    {"verbose",     'v', 0,        0,
     "Increase verbosity (can be specified more than once for debugging)"},
    {"show-chunks", 'c', 0,        0, "Show chunk information"},
    {"quiet",       'q', 0,        0, "Only show errors"},
    {"version",     'V', 0,        0, "Show program version"},
    {"verify",      'f', 0,        0, "Verify full zchunk file"},
    { 0 }
};

struct arguments {
  char *args[1];
  bool verify;
  bool quiet;
  bool show_chunks;
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
        case 'c':
            arguments->show_chunks = true;
            break;
        case 'q':
            arguments->quiet = true;
            break;
        case 'V':
            version();
            arguments->exit = true;
            break;
        case 'f':
            arguments->verify = true;
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
    arguments.log_level = ZCK_LOG_ERROR;

    int retval = argp_parse (&argp, argc, argv, 0, 0, &arguments);
    if(retval || arguments.exit)
        exit(retval);

    zck_set_log_level(arguments.log_level);

    int src_fd = open(arguments.args[0], O_RDONLY | O_BINARY);
    if(src_fd < 0) {
        printf("Unable to open %s\n", arguments.args[0]);
        perror("");
        exit(1);
    }

    zckCtx *zck = zck_create();
    if(zck == NULL) {
        exit(1);
    }
    if(!zck_init_read(zck, src_fd)) {
        LOG_ERROR("Error reading zchunk header: %s",
                zck_get_error(zck));
        zck_free(&zck);
        exit(1);
    }

    int valid_cks = 1;
    if(arguments.verify) {
        valid_cks = zck_validate_checksums(zck);
        if(!valid_cks)
            exit(1);
    }
    close(src_fd);

    if(!arguments.quiet) {
        zckChunk *dict = zck_get_first_chunk(zck);
        char *dict_digest = zck_get_chunk_digest(dict);
        if(dict_digest == NULL) {
            LOG_ERROR("%s", zck_get_error(zck));
            exit(1);
        }
        if(zck_is_detached_header(zck))
            printf("zchunk detached header\n\n");
        else
            printf("zchunk file\n\n");
        printf("Overall checksum type: %s\n",
               zck_hash_name_from_type(zck_get_full_hash_type(zck)));
        printf("Header size: %llu\n", (long long unsigned) zck_get_header_length(zck));
        char *digest = zck_get_header_digest(zck);
        printf("Header checksum: %s\n", digest);
        free(digest);
        ssize_t flags = zck_get_flags(zck);
        if(flags > 0) {
            printf("Flags:\n");
            if(flags & 1)
                printf("    Has streams\n");
            if(flags & 2)
                printf("    Has optional header elements\n");
            if(flags & 4)
                printf("    Has uncompressed checksums\n");
        }
        printf("Data size: %llu\n", (long long unsigned) zck_get_data_length(zck));
        digest = zck_get_data_digest(zck);
        printf("Data checksum: %s\n", digest);
        free(digest);
        printf("Chunk count: %llu\n", (long long unsigned) zck_get_chunk_count(zck));
        printf("Chunk checksum type: %s\n", zck_hash_name_from_type(zck_get_chunk_hash_type(zck)));
        if(zck_get_chunk_size(dict) == 0)
            printf("No dictionary\n");
        else
            printf("Dictionary: %s\n", dict_digest);
        free(dict_digest);
    }
    if(!arguments.quiet && arguments.show_chunks)
        printf("\n");
    if(arguments.show_chunks) {
        for(zckChunk *chk=zck_get_first_chunk(zck); chk;
            chk=zck_get_next_chunk(chk)) {
            char *digest = zck_get_chunk_digest(chk);
            if(digest == NULL) {
                LOG_ERROR("%s", zck_get_error(zck));
                exit(1);
            }
            char *digest_uncompressed = zck_get_chunk_digest_uncompressed(chk);
            if (!digest_uncompressed)
                digest_uncompressed = "";

            if (chk == zck_get_first_chunk(zck)) {
                bool has_uncompressed = (strlen(digest_uncompressed) > 0);
                if (has_uncompressed)
                        printf("       Chunk Checksum %*cUncompressed Checksum %*c       Start    Comp size         Size\n",
                           (((int)zck_get_chunk_digest_size(zck) * 2) - (int)strlen("Checksum")), ' ',
                           ((int)zck_get_chunk_digest_size(zck) * 2) - (int)strlen("Uncompressed Checksum"), ' ');
                    else
                        printf("       Chunk Checksum %*c        Start    Comp size         Size\n",
                              (((int)zck_get_chunk_digest_size(zck) * 2) - (int)strlen("Checksum")), ' ');

            }
            printf("%12llu %s %s %12llu %12llu %12llu",
                   (long long unsigned)zck_get_chunk_number(chk),
                   digest,
                   digest_uncompressed,
                   (long long unsigned)zck_get_chunk_start(chk),
                   (long long unsigned)zck_get_chunk_comp_size(chk),
                   (long long unsigned)zck_get_chunk_size(chk));
            if(arguments.verify) {
                if(zck_get_chunk_valid(chk) == 1)
                    printf("  +");
                else if(zck_is_detached_header(zck) && zck_get_chunk_valid(chk) == 0)
                    ;
                else
                    printf("  !");
            }
            printf("\n");
            free(digest);
        }
    }
    if(arguments.verify) {
        if(valid_cks == 1 && arguments.log_level <= ZCK_LOG_WARNING)
            printf("All checksums are valid\n");
        else if(valid_cks == -1)
            printf("Some checksums failed\n");
    }
    zck_free(&zck);
    return 1-valid_cks;
}
