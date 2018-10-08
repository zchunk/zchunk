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

#include <assert.h>
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
#include "memmem.h"

static char doc[] = "zck - Create a new zchunk file";

static char args_doc[] = "<file>";

static struct argp_option options[] = {
    {"verbose",      'v', 0,        0,
     "Increase verbosity (can be specified more than once for debugging)"},
    {"output",       'o', "FILE",   0,
     "Output to specified FILE"},
    {"split",        's', "STRING", 0, "Split chunks at beginning of STRING"},
    {"dict",         'D', "FILE",   0,
     "Set zstd compression dictionary to FILE"},
    {"manual-chunk", 'm', 0,        0,
     "Don't do any automatic chunking (implies -s)"},
    {"version",      'V', 0,        0, "Show program version"},
    { 0 }
};

struct arguments {
  char *args[1];
  zck_log_type log_level;
  char *split_string;
  bool manual_chunk;
  char *output;
  char *dict;
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
        case 's':
            arguments->split_string = arg;
            break;
        case 'm':
            arguments->manual_chunk = true;
            break;
        case 'o':
            arguments->output = arg;
            break;
        case 'D':
            arguments->dict = arg;
            break;
        case 'V':
            version();
            arguments->exit = true;
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

    int retval = argp_parse(&argp, argc, argv, 0, 0, &arguments);
    if(retval || arguments.exit)
        exit(retval);

    zck_set_log_level(arguments.log_level);

    char *base_name = NULL;
    char *out_name = NULL;
    if(arguments.output == NULL) {
        base_name = basename(arguments.args[0]);
        out_name = malloc(strlen(base_name) + 5);
        assert(out_name);
        snprintf(out_name, strlen(base_name) + 5, "%s.zck", base_name);
    } else {
        base_name = arguments.output;
        out_name = malloc(strlen(base_name) + 1);
        assert(out_name);
        snprintf(out_name, strlen(base_name) + 1, "%s", base_name);
    }

    /* Set dictionary if available */
    char *dict = NULL;
    off_t dict_size = 0;
    if(arguments.dict != NULL) {
        int dict_fd = open(arguments.dict, O_RDONLY);
        if(dict_fd < 0) {
            dprintf(STDERR_FILENO, "Unable to open dictionary %s for reading",
                    arguments.dict);
            perror("");
            exit(1);
        }
        dict_size = lseek(dict_fd, 0, SEEK_END);
        if(dict_size < 0) {
            perror("Unable to seek to end of dictionary");
            exit(1);
        }
        if(lseek(dict_fd, 0, SEEK_SET) < 0) {
            perror("Unable to seek to beginning of dictionary");
            exit(1);
        }
        dict = malloc(dict_size);
        assert(dict);
        if(read(dict_fd, dict, dict_size) < dict_size) {
            free(dict);
            perror("Error reading dict:");
            exit(1);
        }
        close(dict_fd);
    }

    int dst_fd = open(out_name, O_TRUNC | O_WRONLY | O_CREAT, 0644);
    if(dst_fd < 0) {
        dprintf(STDERR_FILENO, "Unable to open %s", out_name);
        perror("");
        if(dict) {
            free(dict);
            dict = NULL;
        }
        free(out_name);
        exit(1);
    }

    zckCtx *zck = zck_create();
    if(zck == NULL) {
        dprintf(STDERR_FILENO, "%s", zck_get_error(NULL));
        zck_clear_error(NULL);
        exit(1);
    }
    if(!zck_init_write(zck, dst_fd)) {
        dprintf(STDERR_FILENO, "Unable to write to %s: %s", out_name,
                zck_get_error(zck));
        exit(1);
    }
    free(out_name);

    /*if(!zck_set_ioption(zck, ZCK_COMP_TYPE, ZCK_COMP_NONE)) {
        perror("Unable to set compression type\n");
        exit(1);
    }*/
    if(dict_size > 0) {
        if(!zck_set_soption(zck, ZCK_COMP_DICT, dict, dict_size)) {
            dprintf(STDERR_FILENO, "%s\n", zck_get_error(zck));
            exit(1);
        }
    }
    free(dict);
    if(arguments.manual_chunk) {
        if(!zck_set_ioption(zck, ZCK_MANUAL_CHUNK, 1)) {
            dprintf(STDERR_FILENO, "%s\n", zck_get_error(zck));
            exit(1);
        }
    }

    char *data;
    int in_fd = open(arguments.args[0], O_RDONLY);
    off_t in_size = 0;
    if(in_fd < 0) {
        dprintf(STDERR_FILENO, "Unable to open %s for reading",
                arguments.args[0]);
        perror("");
        exit(1);
    }
    in_size = lseek(in_fd, 0, SEEK_END);
    if(in_size < 0) {
        dprintf(STDERR_FILENO, "Unable to seek to end of input file");
        exit(1);
    }
    if(lseek(in_fd, 0, SEEK_SET) < 0) {
        perror("Unable to seek to beginning of input file");
        exit(1);
    }
    if(in_size > 0) {
        data = malloc(in_size);
        assert(data);
        if(read(in_fd, data, in_size) < in_size) {
            dprintf(STDERR_FILENO, "Unable to read from input file\n");
            exit(1);
        }
        close(in_fd);

        if(arguments.split_string) {
            char *found = data;
            char *search = found;
            while(search) {
                char *next = memmem(search, in_size - (search-data),
                                    arguments.split_string,
                                    strlen(arguments.split_string));
                if(next) {
                    if(zck_write(zck, found, next-found) < 0)
                        exit(1);
                    if(zck_end_chunk(zck) < 0)
                        exit(1);
                    found = next;
                    search = next + 1;
                    if(search > data + in_size)
                        search = data + in_size;
                } else {
                    if(zck_write(zck, found, data+in_size-found) < 0)
                        exit(1);
                    if(zck_end_chunk(zck) < 0)
                        exit(1);
                    search = NULL;
                }
            }
        /* Buzhash rolling window */
        } else {
            if(zck_write(zck, data, in_size) < 0) {
                dprintf(STDERR_FILENO, "%s", zck_get_error(zck));
                exit(1);
            }
        }
        free(data);
    }
    if(!zck_close(zck)) {
        printf("%s", zck_get_error(zck));
        exit(1);
    }
    if(arguments.log_level <= ZCK_LOG_INFO) {
        dprintf(STDERR_FILENO, "Wrote %lu bytes in %lu chunks\n",
                (unsigned long)(zck_get_data_length(zck) +
                                zck_get_header_length(zck)),
                (long)zck_get_chunk_count(zck));
    }

    zck_free(&zck);
    close(dst_fd);
}
