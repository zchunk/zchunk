/*
 * Copyright 2021 Stefano Babic <stefano.babic@babic.homelinux.org>
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

#define _GNU_SOURCE

#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#ifndef _WIN32
#include <libgen.h>
#endif
#include <unistd.h>
#ifndef _AIX
#include <argp.h>
#endif
#include <zck.h>
#include "util.h"

#define BUFSIZE 16384
static char doc[] = "zck_gen_header - Compare a file with a zck and reports which chunks changed";

static char args_doc[] = "<uncompressed file> <zck file>";

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
  char *args[2];
  bool verify;
  bool quiet;
  bool show_chunks;
  zck_log_type log_level;
  bool exit;
};

static void version(void) {
    exit(EXIT_SUCCESS);
}

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
    arguments.log_level = ZCK_LOG_ERROR;

    int retval = argp_parse (&argp, argc, argv, 0, 0, &arguments);
    if(retval || arguments.exit)
        exit(retval);

    if (argc < 1) {
        LOG_ERROR("Usage : %s filename", argv[0]);
        exit(1);
    }

    zck_set_log_level(arguments.log_level);
    int dst_fd = open("/dev/null", O_TRUNC | O_WRONLY | O_CREAT | O_BINARY, 0666);

    zckCtx *zckSrc = zck_create();
    zckCtx *zckDst = zck_create();
    if(!zckSrc || !zckDst) {
        exit(1);
    }
    if(!zck_init_write(zckSrc, dst_fd)) {
        LOG_ERROR("Unable to write %s ",
                zck_get_error(zckSrc));
        exit(1);
    }
   
    int in_fd = open(arguments.args[0], O_RDONLY | O_BINARY);
    off_t in_size = 0;
    if(in_fd < 0) {
        LOG_ERROR("Unable to open %s for reading",
                arguments.args[0]);
        perror("");
        exit(1);
    }

    /*
     * Read header in the zck file
     */
    int zck_fd = open(arguments.args[1], O_RDONLY | O_BINARY);
    if(zck_fd < 0) {
        LOG_ERROR("Unable to open %s for reading",
                arguments.args[1]);
        perror("");
        exit(1);
    }

    if(!zck_init_read(zckDst, zck_fd)) {
        LOG_ERROR("Error reading zchunk header: %s",
                zck_get_error(zckDst));
        zck_free(&zckSrc);
        zck_free(&zckDst);
        exit(1);
    }

    in_size = lseek(in_fd, 0, SEEK_END);
    if(in_size < 0) {
        LOG_ERROR("Unable to seek to end of input file");
        exit(1);
    }
    if(lseek(in_fd, 0, SEEK_SET) < 0) {
        perror("Unable to seek to beginning of input file");
        exit(1);
    }

    if(!zck_set_ioption(zckSrc, ZCK_UNCOMP_HEADER, 1)) {
        LOG_ERROR("%s\n", zck_get_error(zckSrc));
        exit(1);
    }
    if(!zck_set_ioption(zckSrc, ZCK_COMP_TYPE, ZCK_COMP_NONE))
        exit(1);
    if(!zck_set_ioption(zckSrc, ZCK_HASH_CHUNK_TYPE, ZCK_HASH_SHA256)) {
        LOG_ERROR("Unable to set hash type %s\n", zck_get_error(zckSrc));
        exit(1);
    }

    char *buf = malloc(BUFSIZE);
    if (!buf) {
        LOG_ERROR("Unable to allocate buffer\n");
        exit(1);
    }
    ssize_t n;
    while ((n = read(in_fd, buf, BUFSIZE)) > 0) {
        if (zck_write(zckSrc, buf, n) < 0) {
            LOG_ERROR("zck_write failed: %s\n", zck_get_error(zckSrc));
            exit(1);
        }
    }
    /*
     * Start comparison
     */
    printf("Compare original image with chunks in zck\n");
    printf("-----------------------------------------\n");
    
    zck_generate_hashdb(zckSrc);
    zck_find_matching_chunks(zckSrc, zckDst);

    zckChunk *iter = zck_get_first_chunk(zckDst);
    size_t todwl = 0;
    size_t reuse = 0;
    while (iter) {
        printf("%zd %s %s %zd %zd\n",
                zck_get_chunk_number(iter),
                zck_get_chunk_valid(iter) ? "SRC" : "DST",
                zck_get_chunk_digest_uncompressed(iter),
                zck_get_chunk_start(iter),
                zck_get_chunk_size(iter));

        if (!zck_get_chunk_valid(iter)) {
                   todwl += zck_get_chunk_comp_size(iter);
        } else {
            reuse += zck_get_chunk_size(iter);

        }
        iter = zck_get_next_chunk(iter);
    }

    printf("\n\nTotal to be reused : %zu\n", reuse);
    printf("Total to be downloaded : %zu\n", todwl);

    close(in_fd);
    close(zck_fd);
    zck_free(&zckSrc);
    zck_free(&zckDst);
    close(dst_fd);
}
