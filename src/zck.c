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
#include "memmem.h"

static char doc[] = "zck - Create a new zchunk file";

static char args_doc[] = "<file>";

static struct argp_option options[] = {
    {"output",             'o', "FILE",      0,
     "Output to specified FILE"},
    {"split",              's', "STRING",    0, "Split chunks at beginning of STRING"},
    {"dict",               'D', "FILE",      0,
     "Set zstd compression dictionary to FILE"},
    {"manual-chunk",       'm', 0,           0,
     "Don't do any automatic chunking (implies -s)"},
    {"chunk-hash-type",     'h', "HASH",     0,
     "Set hash type to one of sha256, sha512, sha512_128"},
    {"uncompressed",       'u', 0,           0,
     "Add extension in header for uncompressed data"},
    {"version",            'V', 0,           0, "Show program version"},
    {"compression-format", 200,   "none/zstd", 0,
     "Set compression format for file (none/zstd) (default: zstd)", 1},
    {"verbose",            'v', 0,           0,
     "Increase verbosity (can be specified more than once for debugging)", 1},
    { 0 }
};

struct arguments {
  char *args[1];
  zck_log_type log_level;
  char *split_string;
  bool manual_chunk;
  char *output;
  char *dict;
  char *compression_format;
  bool exit;
  bool uncompressed;
  zck_hash chunk_hashtype;
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
            if (strlen(arg) >= BUF_SIZE) {
                LOG_ERROR("Split string size must be less than %i\n", BUF_SIZE);
                return -EINVAL;
            }
            break;
        case 'm':
            arguments->manual_chunk = true;
            break;
        case 'h':
            if (!strcmp(arg, "sha256"))
                arguments->chunk_hashtype = ZCK_HASH_SHA256;
            else if (!strcmp(arg, "sha512"))
                arguments->chunk_hashtype = ZCK_HASH_SHA512;
            else if (!strcmp(arg, "sha512_128"))
                arguments->chunk_hashtype = ZCK_HASH_SHA512_128;
            else {
                LOG_ERROR("Wrong value for chunk hashtype. \n "
                        "It should be one of sha1|sha256|sha512|sha512_128 instead of %s\n", arg);
                return -EINVAL;
            }

            break;
        case 'o':
            arguments->output = arg;
            break;
        case 'D':
            arguments->dict = arg;
            break;
        case 'u':
            arguments->uncompressed = true;
            break;
        case 200:
            arguments->compression_format = arg;
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

static void write_data(zckCtx *zck, char *data, ssize_t in_size) {
    if(zck_write(zck, data, in_size) < 0) {
        LOG_ERROR("%s", zck_get_error(zck));
        exit(1);
    }
}

int main (int argc, char *argv[]) {
    struct arguments arguments = {0};

    /* Defaults */
    arguments.log_level = ZCK_LOG_ERROR;
    arguments.chunk_hashtype = ZCK_HASH_UNKNOWN;
    arguments.compression_format = "zstd";

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
        int dict_fd = open(arguments.dict, O_RDONLY | O_BINARY);
        if(dict_fd < 0) {
            LOG_ERROR("Unable to open dictionary %s for reading",
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

    int dst_fd = open(out_name, O_TRUNC | O_WRONLY | O_CREAT | O_BINARY, 0666);
    if(dst_fd < 0) {
        LOG_ERROR("Unable to open %s", out_name);
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
        exit(1);
    }
    if(!zck_init_write(zck, dst_fd)) {
        LOG_ERROR("Unable to write to %s: %s", out_name,
                zck_get_error(zck));
        exit(1);
    }
    free(out_name);

    if(arguments.compression_format) {
        if(strncmp(arguments.compression_format, "zstd", 4) == 0) {
            if(!zck_set_ioption(zck, ZCK_COMP_TYPE, ZCK_COMP_ZSTD)) {
                LOG_ERROR("%s\n", zck_get_error(zck));
                exit(1);
            }
        } else if(strncmp(arguments.compression_format, "none", 4) == 0) {
            if(!zck_set_ioption(zck, ZCK_COMP_TYPE, ZCK_COMP_NONE)) {
                LOG_ERROR("%s\n", zck_get_error(zck));
                exit(1);
            }
        } else {
            LOG_ERROR("Unknown compression type: %s\n", arguments.compression_format);
            exit(1);
        }
    }
    if(dict_size > 0) {
        if(!zck_set_soption(zck, ZCK_COMP_DICT, dict, dict_size)) {
            LOG_ERROR("%s\n", zck_get_error(zck));
            exit(1);
        }
    }
    free(dict);
    if(arguments.manual_chunk) {
        if(!zck_set_ioption(zck, ZCK_MANUAL_CHUNK, 1)) {
            LOG_ERROR("%s\n", zck_get_error(zck));
            exit(1);
        }
    }
    if(arguments.uncompressed) {
        if(!zck_set_ioption(zck, ZCK_UNCOMP_HEADER, 1)) {
            LOG_ERROR("%s\n", zck_get_error(zck));
            exit(1);
        }
    }
    if (arguments.chunk_hashtype != ZCK_HASH_UNKNOWN) {
        if(!zck_set_ioption(zck, ZCK_HASH_CHUNK_TYPE, arguments.chunk_hashtype)) {
            LOG_ERROR("Unable to set hash type %s\n", zck_get_error(zck));
            exit(1);
        }
    }
    char data[BUF_SIZE] = {0};
    int in_fd = open(arguments.args[0], O_RDONLY | O_BINARY);
    ssize_t in_size = 0;
    if(in_fd < 0) {
        LOG_ERROR("Unable to open %s for reading",
                arguments.args[0]);
        perror("");
        exit(1);
    }

    int split_size = 0;
    int matched = 0;
    if(arguments.split_string)
        split_size = strlen(arguments.split_string);

    while((in_size = read(in_fd, data, BUF_SIZE)) > 0) {
        ssize_t start = 0;
        if(split_size > 0) {
            /* If we're doing split strings, things get a bit hairy */
            for(int l=0; l<in_size; l++) {
                if(data[l] == arguments.split_string[matched]) {
                    /* Check each byte to see if it matches the next byte in the
                     * split string.  If the we match the whole split string,
                     * write out all queued data, end the chunk, and then write
                     * out the split string */
                    matched++;
                    if(matched == split_size) {
                        if(l > matched)
                            write_data(zck, data + start, l - (start + matched - 1));
                        if(zck_end_chunk(zck) < 0)
                            exit(1);
                        write_data(zck, arguments.split_string, split_size);
                        start = l+1;
                        matched = 0;
                    }
                } else if(matched > 0) {
                    /* If we're at the start of a data block with no more match,
                     * and some data had matched in the previous block, write
                     * out previously matched data */
                    if(l < matched)
                        write_data(zck, arguments.split_string, matched - l);
                    matched = 0;
                }
            }
        }
        write_data(zck, data + start, in_size - (start + matched));
    }

    close(in_fd);

    if(!zck_close(zck)) {
        printf("%s", zck_get_error(zck));
        exit(1);
    }
    if(arguments.log_level <= ZCK_LOG_INFO) {
        LOG_ERROR(
            "Wrote %llu bytes in %llu chunks\n",
            (long long unsigned) (zck_get_data_length(zck) + zck_get_header_length(zck)),
            (long long unsigned) zck_get_chunk_count(zck)
        );
    }

    zck_free(&zck);
    close(dst_fd);
}
