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

#define _GNU_SOURCE

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <argp.h>
#include <zck.h>

#include "util_common.h"

static char doc[] = "unzck - Decompress a zchunk file";

static char args_doc[] = "<file>";

static struct argp_option options[] = {
    {"verbose", 'v', 0,        0,
     "Increase verbosity (can be specified more than once for debugging)"},
    {"stdout",  'c', 0,        0, "Direct output to stdout"},
    {"version", 'V', 0,        0, "Show program version"},
    { 0 }
};

struct arguments {
  char *args[1];
  zck_log_type log_level;
  bool stdout;
};

static error_t parse_opt (int key, char *arg, struct argp_state *state) {
    struct arguments *arguments = state->input;

    switch (key) {
        case 'v':
            arguments->log_level--;
            if(arguments->log_level < ZCK_LOG_DDEBUG)
                arguments->log_level = ZCK_LOG_DDEBUG;
            break;
        case 'c':
            arguments->stdout = true;
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
    struct arguments arguments = {0};

    /* Defaults */
    arguments.log_level = ZCK_LOG_ERROR;

    argp_parse (&argp, argc, argv, 0, 0, &arguments);

    zck_set_log_level(arguments.log_level);

    int src_fd = open(arguments.args[0], O_RDONLY);
    if(src_fd < 0) {
        dprintf(STDERR_FILENO, "Unable to open %s\n", arguments.args[0]);
        perror("");
        exit(1);
    }
    char *base_name = basename(arguments.args[0]);
    char *out_name = malloc(strlen(base_name) - 3);
    snprintf(out_name, strlen(base_name) - 3, "%s", base_name);

    int dst_fd = STDOUT_FILENO;
    if(!arguments.stdout) {
        dst_fd = open(out_name, O_TRUNC | O_WRONLY | O_CREAT, 0644);
        if(dst_fd < 0) {
            dprintf(STDERR_FILENO, "Unable to open %s", out_name);
            perror("");
            free(out_name);
            exit(1);
        }
    }

    bool good_exit = false;

    zckCtx *zck = zck_create();
    if(zck == NULL) {
        dprintf(STDERR_FILENO, "%s", zck_get_error(NULL));
        zck_clear_error(NULL);
        goto error1;
    }

    char *data = malloc(BUF_SIZE);
    if(!zck_init_read(zck, src_fd))
        goto error2;

    int ret = zck_validate_data_checksum(zck);
    if(ret < 1) {
        if(ret == -1)
            dprintf(STDERR_FILENO, "Data checksum failed verification\n");
        goto error2;
    }

    size_t total = 0;
    while(true) {
        ssize_t read = zck_read(zck, data, BUF_SIZE);
        if(read < 0)
            goto error2;
        if(read == 0)
            break;
        if(write(dst_fd, data, read) != read) {
            dprintf(STDERR_FILENO, "Error writing to %s\n", out_name);
            goto error2;
        }
        total += read;
    }
    if(!zck_close(zck))
        goto error2;
    if(arguments.log_level <= ZCK_LOG_INFO)
        dprintf(STDERR_FILENO, "Decompressed %lu bytes\n", (unsigned long)total);
    good_exit = true;
error2:
    free(data);
    if(!good_exit)
        dprintf(STDERR_FILENO, "%s", zck_get_error(zck));
    zck_free(&zck);
error1:
    if(!good_exit)
        unlink(out_name);
    free(out_name);
    close(src_fd);
    close(dst_fd);
    if(!good_exit)
        exit(1);
    exit(0);
}
