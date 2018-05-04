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
#include <argp.h>
#include <zck.h>

#include "util_common.h"

static char doc[] = "zckdl - Download zchunk file";

static char args_doc[] = "<file>";

static struct argp_option options[] = {
    {"verbose", 'v', 0,        0, "Increase verbosity"},
    {"quiet",   'q', 0,        0,
     "Only show warnings (can be specified twice to only show errors)"},
    {"source",  's', "FILE",   0, "File to use as delta source"},
    {"version", 'V', 0,        0, "Show program version"},
    { 0 }
};

struct arguments {
  char *args[1];
  zck_log_type log_level;
  char *source;
};

static error_t parse_opt (int key, char *arg, struct argp_state *state) {
    struct arguments *arguments = state->input;

    switch (key) {
        case 'v':
            arguments->log_level = ZCK_LOG_DEBUG;
            break;
        case 'q':
            if(arguments->log_level < ZCK_LOG_INFO)
                arguments->log_level = ZCK_LOG_INFO;
            arguments->log_level += 1;
            if(arguments->log_level > ZCK_LOG_NONE)
                arguments->log_level = ZCK_LOG_NONE;
            break;
        case 's':
            arguments->source = arg;
            break;
        case 'V':
            version();
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
    zckCtx *zck_tgt = zck_create();

    if(zck_tgt == NULL)
        exit(1);

    zck_dl_global_init();

    struct arguments arguments = {0};

    /* Defaults */
    arguments.log_level = ZCK_LOG_INFO;

    argp_parse (&argp, argc, argv, 0, 0, &arguments);

    zck_set_log_level(arguments.log_level);

    int src_fd = open(arguments.source, O_RDONLY);
    if(src_fd < 0) {
        printf("Unable to open %s\n", arguments.source);
        perror("");
        exit(1);
    }
    zckCtx *zck_src = zck_init_read(src_fd);
    if(zck_src == NULL) {
        printf("Unable to open %s\n", arguments.source);
        exit(1);
    }

    zckDL *dl = zck_dl_init();
    if(dl == NULL)
        exit(1);
    dl->zck = zck_tgt;

    char *outname_full = calloc(1, strlen(arguments.args[0])+1);
    memcpy(outname_full, arguments.args[0], strlen(arguments.args[0]));
    char *outname = basename(outname_full);
    int dst_fd = open(outname, O_RDWR | O_CREAT, 0644);
    if(dst_fd < 0) {
        printf("Unable to open %s: %s\n", outname, strerror(errno));
        free(outname_full);
        exit(1);
    }
    free(outname_full);
    dl->dst_fd = dst_fd;

    if(!zck_dl_get_header(zck_tgt, dl, arguments.args[0]))
        exit(1);

    zck_range_close(&(dl->info));
    if(!zck_dl_copy_src_chunks(&(dl->info), zck_src, zck_tgt))
        exit(1);
    int max_ranges = 256;
    if(!zck_range_calc_segments(&(dl->info), max_ranges))
        exit(1);

    lseek(dl->dst_fd, 0, SEEK_SET);
    if(!zck_dl_range(dl, arguments.args[0]))
        exit(1);


    printf("Downloaded %lu bytes\n",
           (long unsigned)zck_dl_get_bytes_downloaded(dl));
    int exit_val = 0;
    switch(zck_hash_check_data(dl->zck, dl->dst_fd)) {
        case -1:
            exit_val = 1;
            break;
        case 0:
            exit_val = 1;
            break;
        default:
            break;
    }
    zck_dl_free(&dl);
    zck_free(&zck_tgt);
    zck_free(&zck_src);
    zck_dl_global_cleanup();
    exit(exit_val);
}
