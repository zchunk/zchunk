/*
 * Copyright 2018, 2020 Jonathan Dieter <jdieter@gmail.com>
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
#define STDERR_FILENO 2

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

#include "util_common.h"

static char doc[] = "unzck - Decompress a zchunk file";

static char args_doc[] = "<file>";

static struct argp_option options[] = {
    {"verbose", 'v', 0,        0,
     "Increase verbosity (can be specified more than once for debugging)"},
    {"stdout",  'c', 0,        0, "Direct output to stdout"},
    {"dict",   1000, 0,        0, "Only extract the dictionary (can't be run with --header)"},
    {"header", 1001, 0,        0, "Only extract the header (can't be run with --dict)"},
    {"version", 'V', 0,        0, "Show program version"},
    { 0 }
};

struct arguments {
  char *args[1];
  zck_log_type log_level;
  bool dict;
  bool header;
  bool std_out;
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
            arguments->std_out = true;
            break;
        case 'V':
            version();
            arguments->exit = true;
            break;
        case 1000: // Header and dict can't both be set
            arguments->dict = true;
            arguments->header = false;
            break;
        case 1001: // Header and dict can't both be set
            arguments->header = true;
            arguments->dict = false;
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
        LOG_ERROR("Unable to open %s\n", arguments.args[0]);
        perror("");
        exit(1);
    }
    char *base_name = basename(arguments.args[0]);
    if(strlen(base_name) > 4 && strncmp(base_name + strlen(base_name)-4, ".zck", 4) == 0) {
        base_name[strlen(base_name)-4] = '\0';
    }
    printf("%s\n", base_name);
    char *out_name = NULL;
    if(arguments.dict)
        out_name = calloc(strlen(base_name) + 7, 1); // add .zdict suffix
    else if(arguments.header)
        out_name = calloc(strlen(base_name) + 5, 1); // add .zhr suffix
    else
        out_name = calloc(strlen(base_name) + 1, 1); // strip .zck
    assert(out_name);
    strncpy(out_name, base_name, strlen(base_name));
    if(arguments.dict)
        snprintf(out_name + strlen(base_name), 7, ".zdict");
    else if(arguments.header)
        snprintf(out_name + strlen(base_name), 5, ".zhr");

#ifdef _WIN32
    int dst_fd = _fileno(stdout);
#else
    int dst_fd = STDOUT_FILENO;
#endif
    if(!arguments.std_out) {
        dst_fd = open(out_name, O_TRUNC | O_WRONLY | O_CREAT | O_BINARY, 0666);
        if(dst_fd < 0) {
            LOG_ERROR("Unable to open %s", out_name);
            perror("");
            free(out_name);
            exit(1);
        }
    }

    bool good_exit = false;

    char *data = NULL;
    zckCtx *zck = zck_create();
    if(!zck_init_read(zck, src_fd)) {
        LOG_ERROR("%s", zck_get_error(zck));
        goto error2;
    }

    /* Only write dictionary */
    if(arguments.dict) {
        zckChunk *dict = zck_get_first_chunk(zck);
        ssize_t dict_size = zck_get_chunk_size(dict);
        if(dict_size < 0) {
            LOG_ERROR("%s", zck_get_error(zck));
            goto error2;
        } else if(dict_size == 0) {
            LOG_ERROR("%s doesn't contain a dictionary\n", arguments.args[0]);
            goto error2;
        }
        data = calloc(dict_size, 1);
        assert(data);
        ssize_t read_size = zck_get_chunk_data(dict, data, dict_size);
        if(read_size != dict_size) {
            if(read_size < 0)
                LOG_ERROR("%s", zck_get_error(zck));
            else
                LOG_ERROR(
                        "Dict size doesn't match expected size: %lli != %lli\n",
                        (long long) read_size, (long long) dict_size);
            goto error2;
        }
        if(write(dst_fd, data, dict_size) != dict_size) {
            LOG_ERROR("Error writing to %s\n", out_name);
            goto error2;
        }
        if(dict_size > 0) {
            int ret = zck_get_chunk_valid(dict);
            if(ret < 1) {
                if(ret == -1)
                    LOG_ERROR("Dictionary checksum failed verification\n");
                else
                    LOG_ERROR("%s", zck_get_error(zck));
                goto error2;
            }
        }
        good_exit = true;
        goto error2;
    } else if(arguments.header) {
        if(zck_is_detached_header(zck)) {
            LOG_ERROR("%s is already a detached header\n", arguments.args[0]);
            goto error2;
        }

        ssize_t header_size = zck_get_header_length(zck);
        if(header_size == -1) {
            LOG_ERROR("%s", zck_get_error(zck));
            goto error2;
        }

        zckChunk *dict = zck_get_first_chunk(zck);
        ssize_t dict_size = zck_get_chunk_comp_size(dict);
        if(dict_size < 0) {
            LOG_ERROR("%s", zck_get_error(zck));
            goto error2;
        }

        data = calloc(BUF_SIZE, 1);
        if(data == NULL) {
            LOG_ERROR("Unable to allocate %i bytes\n", BUF_SIZE);
            goto error2;
        }

        if(lseek(src_fd, 5, SEEK_SET) < 0) {
            perror("Unable to seek to beginning of source file");
            exit(1);
        }
        write(dst_fd, "\0ZHR1", 5);
        for(ssize_t i=5; i<header_size + dict_size; i+=BUF_SIZE) {
            ssize_t write_size = i + BUF_SIZE < header_size + dict_size ? BUF_SIZE : header_size + dict_size - i;
            ssize_t read_size = read(src_fd, data, write_size);
            if(read_size < write_size) {
                LOG_ERROR("Unable to read %llu bytes from source\n", (long long unsigned) write_size);
                goto error2;
            }
            if(write(dst_fd, data, write_size) != write_size) {
                LOG_ERROR("Error writing to %s\n", out_name);
                goto error2;
            }
        }
        good_exit = true;
        goto error2;
    }

    if(zck_is_detached_header(zck)) {
        LOG_ERROR("%s is a detached header, not a full zchunk file.  The only operation unzck\n"
                  "can run on a detached header is --dict\n", arguments.args[0]);
        goto error2;
    }

    int ret = zck_validate_data_checksum(zck);
    if(ret < 1) {
        if(ret == -1)
            LOG_ERROR("Data checksum failed verification\n");
        goto error2;
    }

    data = calloc(BUF_SIZE, 1);
    assert(data);
    size_t total = 0;
    while(true) {
        ssize_t read_size = zck_read(zck, data, BUF_SIZE);
        if(read_size < 0) {
            LOG_ERROR("%s", zck_get_error(zck));
            goto error2;
        }
        if(read_size == 0)
            break;
        if(write(dst_fd, data, read_size) != read_size) {
            LOG_ERROR("Error writing to %s\n", out_name);
            goto error2;
        }
        total += read_size;
    }
    if(!zck_close(zck)) {
        LOG_ERROR("%s", zck_get_error(zck));
        goto error2;
    }
    if(arguments.log_level <= ZCK_LOG_INFO)
        LOG_ERROR(
            "Decompressed %llu bytes\n",
            (long long unsigned) total
        );
    good_exit = true;
error2:
    free(data);
    zck_free(&zck);
    if(!good_exit)
        unlink(out_name);
    free(out_name);
    close(src_fd);
    close(dst_fd);
    if(!good_exit)
        exit(1);
    exit(0);
}
