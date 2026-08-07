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
#include <errno.h>
#ifndef _AIX
#include <argp.h>
#endif
#include <zck.h>
#include <curl/curl.h>

#include "util_common.h"

static char doc[] = "zckdl - Download zchunk file";

static char args_doc[] = "<file>";

static struct argp_option options[] = {
    {"verbose",        'v',  0,        0, "Increase verbosity"},
    {"quiet",          'q',  0,        0,
     "Only show warnings (can be specified twice to only show errors)"},
    {"source",         's', "FILE",    0, "File to use as delta source"},
    {"fail-no-ranges", 1000, 0,        0,
     "If server doesn't support ranges, fail instead of downloading full file"},
    {"version",        'V',  0,        0, "Show program version"},
    { 0 }
};

static int range_attempt[] = {
    255,
    127,
    7,
    2,
    1
};

struct arguments {
  char *args[1];
  zck_log_type log_level;
  char *source;
  int fail_no_ranges;
  bool exit;
};

static error_t parse_opt (int key, char *arg, struct argp_state *state) {
    struct arguments *arguments = state->input;

    if(arguments->exit)
        return 0;

    switch (key) {
        case 'v':
            if(arguments->log_level > ZCK_LOG_INFO)
                arguments->log_level = ZCK_LOG_INFO;
            arguments->log_level--;
            if(arguments->log_level < ZCK_LOG_DDEBUG)
                arguments->log_level = ZCK_LOG_DDEBUG;
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
            arguments->exit = true;
            break;
        case 1000:
            arguments->fail_no_ranges = 1;
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

typedef struct dlCtx {
    CURL *curl;
    zckDL *dl;
    int fail_no_ranges;
    int range_fail;
    int max_ranges;
} dlCtx;

/* Fail if dl_ctx->fail_no_ranges is set and we get a 200 response */
size_t dl_header_cb(char *b, size_t l, size_t c, void *dl_v) {
    dlCtx *dl_ctx = (dlCtx*)dl_v;
    if(dl_ctx->fail_no_ranges) {
        long code = -1;
        curl_easy_getinfo(dl_ctx->curl, CURLINFO_RESPONSE_CODE, &code);
        if(code == 200) {
            dl_ctx->range_fail = 1;
            return 0;
        }
    }
    return zck_header_cb(b, l, c, dl_ctx->dl);
}

/* Return 0 on error, -1 on 200 response (if dl_ctx->fail_no_ranges),
 * and 1 on complete success */
int dl_range(dlCtx *dl_ctx, char *url, char *range, int is_chunk) {
    if(dl_ctx == NULL || dl_ctx->dl == NULL) {
        free(range);
        LOG_ERROR("Struct not defined\n");
        return 0;
    }

    CURL *curl = dl_ctx->curl;
    CURLcode res;

    if(curl_easy_setopt(curl, CURLOPT_URL, url) != CURLE_OK) {
        LOG_ERROR("Unable to set URL\n");
        return 0;
    }
    if(curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L) != CURLE_OK) {
        LOG_ERROR("Unable to enable option to follow redirects\n");
        return 0;
    }
    if(curl_easy_setopt(curl, CURLOPT_HEADERFUNCTION, dl_header_cb) != CURLE_OK) {
        LOG_ERROR("Unable to set header callback\n");
        return 0;
    }
    if(curl_easy_setopt(curl, CURLOPT_HEADERDATA, dl_ctx) != CURLE_OK) {
        LOG_ERROR("Unable to set header callback data\n");
        return 0;
    }
    if(is_chunk) {
        if(curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, zck_write_chunk_cb) != CURLE_OK) {
            LOG_ERROR("Unable to set write callback\n");
            return 0;
        }
    } else {
        if(curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, zck_write_zck_header_cb) != CURLE_OK) {
            LOG_ERROR("Unable to set write callback\n");
            return 0;
        }
    }
    if(curl_easy_setopt(curl, CURLOPT_WRITEDATA, dl_ctx->dl) != CURLE_OK) {
        LOG_ERROR("Unable to set write callback data\n");
        return 0;
    }
    if(curl_easy_setopt(curl, CURLOPT_RANGE, range) != CURLE_OK) {
        LOG_ERROR("Unable to set download range\n");
        return 0;
    }
    res = curl_easy_perform(curl);
    free(range);

    if(dl_ctx->range_fail)
        return -1;

    if(res != CURLE_OK) {
        LOG_ERROR("Download failed: %s\n",
                  curl_easy_strerror(res));
        return 0;
    }
    long code;
    if(curl_easy_getinfo (curl, CURLINFO_RESPONSE_CODE, &code) != CURLE_OK) {
        LOG_ERROR("Unable to get response code\n");
        return 0;
    }
    if(code != 206 && code != 200) {
        LOG_ERROR("HTTP Error: %li when downloading %s\n", code,
                  url);
        return 0;
    }


    return 1;
}


int dl_byte_range(dlCtx *dl_ctx, char *url, int start, int end) {
    char *range = NULL;
    zck_dl_reset(dl_ctx->dl);
    if(start > -1 && end > -1)
        range = zck_get_range(start, end);
    return dl_range(dl_ctx, url, range, 0);
}

int dl_bytes(dlCtx *dl_ctx, char *url, size_t bytes, size_t start,
             size_t *buffer_len, int log_level) {
    if(start + bytes > *buffer_len) {
        zckDL *dl = dl_ctx->dl;

        int fd = zck_get_fd(zck_dl_get_zck(dl));

        if(lseek(fd, *buffer_len, SEEK_SET) == -1) {
            LOG_ERROR("Seek to download location failed: %s\n",
                      strerror(errno));
            return 0;
        }
        if(*buffer_len >= start + bytes)
            return 1;

        int retval = dl_byte_range(dl_ctx, url, *buffer_len,
                                   (start + bytes) - 1);
        if(retval < 1)
            return retval;

        if(log_level <= ZCK_LOG_DEBUG)
            LOG_ERROR(
                "Downloading %llu bytes at position %llu\n",
                (long long unsigned) start+bytes-*buffer_len,
                (long long unsigned) *buffer_len
            );
        *buffer_len += start + bytes - *buffer_len;
        if(lseek(fd, start, SEEK_SET) == -1) {
            LOG_ERROR(
                "Seek to byte %llu of temporary file failed: %s\n",
                (long long unsigned) start,
                strerror(errno)
            );
            return 0;
        }
    }
    return 1;
}

int dl_header(CURL *curl, zckDL *dl, char *url, int fail_no_ranges,
              int log_level) {
    size_t buffer_len = 0;
    size_t start = 0;

    dlCtx dl_ctx = {0};
    dl_ctx.fail_no_ranges = 1;
    dl_ctx.dl = dl;
    dl_ctx.curl = curl;
    dl_ctx.max_ranges = 1;

    /* Download minimum download size and read magic and hash type */
    int retval = dl_bytes(&dl_ctx, url, zck_get_min_download_size(), start,
                          &buffer_len, log_level);
    if(retval < 1)
        return retval;

    zckCtx *zck = zck_dl_get_zck(dl);
    if(zck == NULL)
        return 0;

    if(!zck_read_lead(zck))
        return 0;
    start = zck_get_lead_length(zck);
    if(!dl_bytes(&dl_ctx, url, zck_get_header_length(zck) - start,
                 start, &buffer_len, log_level))
        return 0;
    if(!zck_read_header(zck))
        return 0;
    return 1;
}

int main (int argc, char *argv[]) {
    struct arguments arguments = {0};

    /* Defaults */
    arguments.log_level = ZCK_LOG_INFO;

    int retval = argp_parse (&argp, argc, argv, 0, 0, &arguments);
    if(retval || arguments.exit)
        exit(retval);

    curl_global_init(CURL_GLOBAL_ALL);

    zck_set_log_level(arguments.log_level);

    zckCtx *zck_src = NULL;
    if(arguments.source) {
        int src_fd = open(arguments.source, O_RDONLY);
        if(src_fd < 0) {
            LOG_ERROR("Unable to open %s\n", arguments.source);
            perror("");
            exit(10);
        }
        zck_src = zck_create();
        if(zck_src == NULL) {
            exit(10);
        }
        if(!zck_init_read(zck_src, src_fd)) {
            LOG_ERROR("Unable to open %s: %s", arguments.source,
                      zck_get_error(zck_src));
            exit(10);
        }
    }

    CURL *curl_ctx = curl_easy_init();
    if(!curl_ctx) {
        LOG_ERROR(
            "Unable to allocate %llu bytes for curl context\n",
            (long long unsigned)sizeof(CURL)
        );
        exit(10);
    }

    char *outname = basename(arguments.args[0]);
    int dst_fd = open(outname, O_RDWR | O_CREAT, 0666);
    if(dst_fd < 0) {
        LOG_ERROR("Unable to open %s: %s\n", outname,
                  strerror(errno));
        exit(10);
    }
    zckCtx *zck_tgt = zck_create();
    if(zck_tgt == NULL) {
        exit(10);
    }
    if(!zck_init_adv_read(zck_tgt, dst_fd)) {
        LOG_ERROR("%s", zck_get_error(zck_tgt));
        exit(10);
    }

    zckDL *dl = zck_dl_init(zck_tgt);
    if(dl == NULL) {
        exit(10);
    }

    int exit_val = 0;

    retval = dl_header(curl_ctx, dl, arguments.args[0],
                       arguments.fail_no_ranges, arguments.log_level);
    if(!retval) {
        exit_val = 10;
        goto out;
    }

    /* The server doesn't support ranges */
    if(retval == -1) {
        if(arguments.fail_no_ranges) {
            LOG_ERROR("Server doesn't support ranges and --fail-no-ranges was "
                      "set\n");
            exit_val = 2;
            goto out;
        }
        /* Download the full file */
        lseek(dst_fd, 0, SEEK_SET);
        if(ftruncate(dst_fd, 0) < 0) {
            perror(NULL);
            exit_val = 10;
            goto out;
        }
        dlCtx dl_ctx = {0};
        dl_ctx.dl = dl;
        dl_ctx.curl = curl_ctx;
        dl_ctx.max_ranges = 0;
        if(!dl_byte_range(&dl_ctx, arguments.args[0], -1, -1)) {
            exit_val = 10;
            goto out;
        }
        lseek(dst_fd, 0, SEEK_SET);
        if(!zck_read_lead(zck_tgt) || !zck_read_header(zck_tgt)) {
            exit_val = 10;
            goto out;
        }
    } else {
        /* If file is already fully downloaded, let's get out of here! */
        int retval = zck_find_valid_chunks(zck_tgt);
        if(retval == 0) {
            exit_val = 10;
            goto out;
        }
        if(retval == 1) {
            printf("Missing chunks: 0\n");
            printf(
                "Downloaded %llu bytes\n",
                (long long unsigned) zck_dl_get_bytes_downloaded(dl)
            );
            if(ftruncate(dst_fd, zck_get_length(zck_tgt)) < 0) {
                perror(NULL);
                exit_val = 10;
                goto out;
            }
            exit_val = 0;
            goto out;
        }
        if(zck_src && !zck_copy_chunks(zck_src, zck_tgt)) {
            exit_val = 10;
            goto out;
        }
        zck_reset_failed_chunks(zck_tgt);
        dlCtx dl_ctx = {0};
        dl_ctx.dl = dl;
        dl_ctx.curl = curl_ctx;
        dl_ctx.max_ranges = range_attempt[0];
        dl_ctx.fail_no_ranges = 1;
        int ra_index = 0;
        printf("Missing chunks: %i\n", zck_missing_chunks(zck_tgt));
        while(zck_missing_chunks(zck_tgt) > 0) {
            dl_ctx.range_fail = 0;
            zck_dl_reset(dl);
            zckRange *range = zck_get_missing_range(zck_tgt, dl_ctx.max_ranges);
            if(range == NULL || !zck_dl_set_range(dl, range)) {
                exit_val = 10;
                goto out;
            }
            while(range_attempt[ra_index] > 1 &&
                  range_attempt[ra_index+1] > zck_get_range_count(range))
                ra_index++;
            char *range_string = zck_get_range_char(zck_src, range);
            if(range_string == NULL) {
                exit_val = 10;
                goto out;
            }
            int retval = dl_range(&dl_ctx, arguments.args[0], range_string, 1);
            if(retval == -1) {
                if(dl_ctx.max_ranges > 1) {
                    ra_index += 1;
                    dl_ctx.max_ranges = range_attempt[ra_index];
                }
                LOG_ERROR("Tried downloading too many ranges, reducing to %i\n",
                          dl_ctx.max_ranges);
            }
            if(!zck_dl_set_range(dl, NULL)) {
                exit_val = 10;
                goto out;
            }
            zck_range_free(&range);
            if(!retval) {
                exit_val = 1;
                goto out;
            }
        }
    }
    printf("Downloaded %llu bytes\n",
           (long long unsigned) zck_dl_get_bytes_downloaded(dl));
    if(ftruncate(dst_fd, zck_get_length(zck_tgt)) < 0) {
        perror(NULL);
        exit_val = 10;
        goto out;
    }

    switch(zck_validate_data_checksum(zck_tgt)) {
        case -1:
            exit_val = 1;
            break;
        case 0:
            exit_val = 1;
            break;
        default:
            break;
    }
out:
    if(exit_val > 0) {
        if(zck_is_error(zck_src))
            LOG_ERROR("%s", zck_get_error(zck_src));
        if(zck_is_error(zck_tgt))
            LOG_ERROR("%s", zck_get_error(zck_tgt));
    }
    zck_dl_free(&dl);
    zck_free(&zck_tgt);
    zck_free(&zck_src);
    curl_easy_cleanup(curl_ctx);
    curl_global_cleanup();
    exit(exit_val);
}
