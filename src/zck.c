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
#include "buzhash/buzhash.h"
#include "memmem.h"

static char doc[] = "zck - Create a new zchunk file";

static char args_doc[] = "<file>";

static struct argp_option options[] = {
    {"verbose", 'v', 0,        0,
     "Increase verbosity (can be specified more than once for debugging)"},
    {"quiet",   'q', 0,        0, "Only show errors"},
    {"split",   's', "STRING", 0, "Split chunks at beginning of STRING"},
    {"dict",    'D', "FILE",   0, "Set zstd compression dictionary to FILE"},
    {"version", 'V', 0,        0, "Show program version"},
    { 0 }
};

struct arguments {
  char *args[1];
  zck_log_type log_level;
  char *split_string;
  char *dict;
};

static error_t parse_opt (int key, char *arg, struct argp_state *state) {
    struct arguments *arguments = state->input;

    switch (key) {
        case 'v':
            arguments->log_level--;
            if(arguments->log_level < ZCK_LOG_DDEBUG)
                arguments->log_level = ZCK_LOG_DDEBUG;
            break;
        case 'q':
            arguments->log_level = ZCK_LOG_ERROR;
            break;
        case 's':
            arguments->split_string = arg;
            break;
        case 'D':
            arguments->dict = arg;
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
    arguments.log_level = ZCK_LOG_WARNING;

    argp_parse (&argp, argc, argv, 0, 0, &arguments);

    zck_set_log_level(arguments.log_level);

    char *out_name;
    out_name = malloc(strlen(arguments.args[0]) + 5);
    snprintf(out_name, strlen(arguments.args[0]) + 5, "%s.zck",
             arguments.args[0]);

    /* Set dictionary if available */
    char *dict = NULL;
    size_t dict_size = 0;
    if(arguments.dict != NULL) {
        int dict_fd = open(arguments.dict, O_RDONLY);
        if(dict_fd < 0) {
            printf("Unable to open dictionary %s for reading", arguments.dict);
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
        if(read(dict_fd, dict, dict_size) < dict_size) {
            free(dict);
            perror("Error reading dict:");
            exit(1);
        }
        close(dict_fd);
    }

    int dst_fd = open(out_name, O_TRUNC | O_WRONLY | O_CREAT, 0644);
    if(dst_fd < 0) {
        printf("Unable to open %s", out_name);
        perror("");
        if(dict) {
            free(dict);
            dict = NULL;
        }
        free(out_name);
        exit(1);
    }
    free(out_name);

    zckCtx *zck = zck_init_write(dst_fd);
    if(zck == NULL) {
        printf("Unable to write to %s\n", out_name);
        exit(1);
    }

    /*if(!zck_set_ioption(zck, ZCK_COMP_TYPE, ZCK_COMP_NONE)) {
        perror("Unable to set compression type\n");
        exit(1);
    }*/
    if(dict_size > 0) {
        if(!zck_set_soption(zck, ZCK_COMP_DICT, dict, dict_size))
            exit(1);
    }
    free(dict);

    char *data;
    int in_fd = open(arguments.args[0], O_RDONLY);
    off_t in_size = 0;
    if(in_fd < 0) {
        printf("Unable to open %s for reading", arguments.args[0]);
        perror("");
        exit(1);
    }
    in_size = lseek(in_fd, 0, SEEK_END);
    if(in_size < 0) {
        perror("Unable to seek to end of input file");
        exit(1);
    }
    if(lseek(in_fd, 0, SEEK_SET) < 0) {
        perror("Unable to seek to beginning of input file");
        exit(1);
    }
    if(in_size > 0) {
        data = malloc(in_size);
        if(read(in_fd, data, in_size) < in_size) {
            printf("Unable to read from input file\n");
            exit(1);
        }
        close(in_fd);

        if(arguments.split_string) {
            char *found = data;
            char *search = found;
            char *prev_srpm = memmem(search, in_size - (search-data), "<rpm:sourcerpm", 14);
            while(search) {
                char *next = memmem(search, in_size - (search-data),
                                    arguments.split_string,
                                    strlen(arguments.split_string));
                if(next) {
                    char *next_srpm = memmem(next, in_size - (next-data), "<rpm:sourcerpm", 14);

                    if(prev_srpm > next)
                        prev_srpm = NULL;
                    if(prev_srpm) {
                        int matched=0;
                        char prev = '\0';
                        for(int i=0;;i++) {
                            if(next_srpm[i] != prev_srpm[i])
                                break;
                            if(next_srpm[i] == '/' && prev == '<') {
                                matched = 1;
                                break;
                            }
                            prev = next_srpm[i];
                        }
                        if(matched) {
                            search = next + 1;
                            if(search > data + in_size)
                                search = data + in_size;
                            continue;
                        }

                    }
                    prev_srpm = next_srpm;
                    printf("Compressing %li bytes\n", (long)(next-found));
                    if(zck_write(zck, found, next-found) < 0)
                        exit(1);
                    if(zck_end_chunk(zck) < 0)
                        exit(1);
                    found = next;
                    search = next + 1;
                    if(search > data + in_size)
                        search = data + in_size;
                } else {
                    printf("Completing %li bytes\n",
                           (long)(data+in_size-found));
                    if(zck_write(zck, found, data+in_size-found) < 0)
                        exit(1);
                    if(zck_end_chunk(zck) < 0)
                        exit(1);
                    search = NULL;
                }
            }
        /* Buzhash rolling window */
        } else {
            char *cur_loc = data;
            char *start = data;
            char *window_loc;
            buzHash b = {0};
            size_t buzhash_width = 48;
            size_t match_bits = 32768;

            if(arguments.log_level <= ZCK_LOG_INFO) {
                printf("Using buzhash algorithm for automatic chunking\n");
                printf("Window size: %lu\n", (unsigned long)buzhash_width);
            }
            while(cur_loc < data + in_size) {
                uint32_t bh = 0;
                window_loc = cur_loc;
                if(cur_loc + buzhash_width < data + in_size) {
                    bh = buzhash_setup(&b, window_loc, buzhash_width);
                    cur_loc += buzhash_width;
                    while(cur_loc < data + in_size) {
                        bh = buzhash_update(&b, cur_loc);
                        if(((bh) & (match_bits - 1)) == 0)
                            break;
                        cur_loc++;
                    }
                } else {
                    cur_loc = data + in_size;
                }
                if(arguments.log_level <= ZCK_LOG_DEBUG)
                    printf("Completing %li bytes\n", (long)(cur_loc-start));
                if(zck_write(zck, start, cur_loc-start) < 0)
                    exit(1);
                if(zck_end_chunk(zck) < 0)
                    exit(1);
                start = cur_loc;
            }
        }
        free(data);
    }
    if(!zck_close(zck))
        exit(1);
    if(arguments.log_level <= ZCK_LOG_INFO) {
        printf("Wrote %lu bytes in %lu chunks\n",
               (unsigned long)(zck_get_data_length(zck) +
                               zck_get_header_length(zck)),
               (long)zck_get_chunk_count(zck));
    }

    zck_free(&zck);
    close(dst_fd);
}
