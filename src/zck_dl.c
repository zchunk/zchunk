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
#include <curl/curl.h>

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

int dl_range(CURL *curl, zckDL *dl, char *url, char *range, int is_chunk) {
    if(dl == NULL || dl->priv == NULL) {
        printf("Struct not defined\n");
        return 0;
    }

    CURLcode res;

    zck_dl_reset(dl);

    curl_easy_setopt(curl, CURLOPT_URL, url);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_HEADERFUNCTION, zck_header_cb);
    curl_easy_setopt(curl, CURLOPT_HEADERDATA, dl);
    if(is_chunk)
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, zck_write_chunk_cb);
    else
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, zck_write_zck_header_cb);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, dl);
    curl_easy_setopt(curl, CURLOPT_RANGE, range);
    res = curl_easy_perform(curl);

    if(res != CURLE_OK) {
        printf("Download failed: %s\n", curl_easy_strerror(res));
        return False;
    }
    long code;
    curl_easy_getinfo (curl, CURLINFO_RESPONSE_CODE, &code);
    if (code != 206 && code != 200) {
        printf("HTTP Error: %li when download %s\n", code,
                url);
        return False;
    }

    return True;
}

int dl_byte_range(CURL *curl, zckDL *dl, char *url, int start, int end) {
    char *range = zck_get_range(start, end);
    return dl_range(curl, dl, url, range, 0);
}

int dl_bytes(CURL *curl, zckDL *dl, char *url, size_t bytes, size_t start, size_t *buffer_len, int log_level) {
    if(start + bytes > *buffer_len) {
        int fd = zck_get_fd(dl->zck);

        if(lseek(fd, 0, SEEK_END) == -1) {
            printf("Seek to end of temporary file failed: %s\n",
                   strerror(errno));
            return 0;
        }
        if(*buffer_len >= start + bytes)
            return 1;
        if(!dl_byte_range(curl, dl, url, *buffer_len,
                          (start + bytes - *buffer_len) - 1)) {
            printf("Error downloading bytes\n");
            return 0;
        }
        if(log_level <= ZCK_LOG_DEBUG)
            printf("Downloading %lu bytes at position %lu\n",
                   start+bytes-*buffer_len, *buffer_len);
        *buffer_len += start + bytes - *buffer_len;
        if(lseek(fd, start, SEEK_SET) == -1) {
            printf("Seek to byte %lu of temporary file failed: %s\n", start,
                   strerror(errno));
            return 0;
        }
    }
    return 1;
}

int dl_header(CURL *curl, zckDL *dl, char *url, int log_level) {
    size_t buffer_len = 0;
    size_t start = 0;

    /* Download first two hundred bytes and read magic and hash type */
    if(!dl_bytes(curl, dl, url, get_min_download_size(), start, &buffer_len, log_level))
        return 0;
    if(!zck_read_lead(dl->zck))
        return 0;
    start = zck_get_lead_length(dl->zck);
    printf("Now we need %lu bytes\n", zck_get_header_length(dl->zck) - start);

    if(!dl_bytes(curl, dl, url, zck_get_header_length(dl->zck) - start,
                 start, &buffer_len, log_level))
        return 0;
    if(!zck_read_header(dl->zck))
        return 0;
    return 1;
}

int main (int argc, char *argv[]) {
    curl_global_init(CURL_GLOBAL_ALL);

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
    zckCtx *zck_tgt = zck_init_adv_read(dst_fd);
    if(zck_tgt == NULL)
        exit(1);

    CURL *curl_ctx = curl_easy_init();
    if(!curl_ctx) {
        printf("Unable to allocate %lu bytes for curl context\n",
                sizeof(CURL));
        exit(1);
    }

    zckDL *dl = zck_dl_init(zck_tgt);
    if(dl == NULL)
        exit(1);
    dl->zck = zck_tgt;

    if(!dl_header(curl_ctx, dl, arguments.args[0], arguments.log_level))
        exit(1);

    if(!zck_copy_chunks(zck_src, zck_tgt))
        exit(1);


    printf("Downloaded %lu bytes\n",
           (long unsigned)zck_dl_get_bytes_downloaded(dl));
    int exit_val = 0;
    switch(zck_validate_data_checksum(dl->zck)) {
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
    curl_easy_cleanup(curl_ctx);
    curl_global_cleanup();
    exit(exit_val);
}
