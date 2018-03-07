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
#include <stdint.h>
#include <string.h>
#include <curl/curl.h>
#include <unistd.h>
#include <errno.h>
#include <zck.h>

#include "zck_private.h"

#define VALIDATE(f)     if(!f) { \
                            zck_log(ZCK_LOG_ERROR, "zckDL not allocated\n"); \
                            return False; \
                        }
zckDL *zck_dl_init() {
    zckDL *dl = zmalloc(sizeof(zckDL));
    if(!dl) {
        zck_log(ZCK_LOG_ERROR, "Unable to allocate %lu bytes\n",
                sizeof(zckDL));
        return NULL;
    }

    dl->curl_ctx = curl_easy_init();
    if(!dl->curl_ctx) {
        zck_log(ZCK_LOG_ERROR, "Unable to allocate %lu bytes\n",
                sizeof(CURL));
        return NULL;
    }
    return dl;
}

void zck_dl_free(zckDL *dl) {
    if(!dl)
        return;
    curl_easy_cleanup(dl->curl_ctx);
    free(dl);
    dl = NULL;
}

char *zck_dl_get_range_char(unsigned int start, unsigned int end) {
    zckRange r = {0};
    zckRange *range = &r;

    r.start = start;
    r.end = end;
    char *range_header = zck_range_get_char(&range, 2);
    return range_header;
}

static size_t write_data(void *ptr, size_t size, size_t nmemb, void *stream) {
    size_t wb = write(*(int*)stream, ptr, size*nmemb);
    return wb;
}

int zck_dl_range(zckDL *dl, char *url, int dst_fd, zckRangeInfo *info) {
    if(info == NULL || info->first == NULL) {
        zck_log(ZCK_LOG_ERROR, "Range not defined\n");
        return False;
    }
    if(info->segments == 0)
        info->segments = 1;

    char **ra = calloc(sizeof(char*), info->segments);
    if(!zck_range_get_array(info, ra)) {
        free(ra);
        return False;
    }
    CURLcode res;

    for(int i=0; i<info->segments; i++) {
        struct curl_slist *header = NULL;
        double size;
        header = curl_slist_append(header, ra[i]);
        curl_easy_setopt(dl->curl_ctx, CURLOPT_URL, url);
        curl_easy_setopt(dl->curl_ctx, CURLOPT_FOLLOWLOCATION, 1L);
        curl_easy_setopt(dl->curl_ctx, CURLOPT_WRITEFUNCTION, write_data);
        curl_easy_setopt(dl->curl_ctx, CURLOPT_WRITEDATA, &dst_fd);
        curl_easy_setopt(dl->curl_ctx, CURLOPT_HTTPHEADER, header);
        res = curl_easy_perform(dl->curl_ctx);
        curl_slist_free_all(header);
        free(ra[i]);
        if(res != CURLE_OK) {
            zck_log(ZCK_LOG_ERROR, "Download failed: %s\n",
                    curl_easy_strerror(res));
            return False;
        }
        long code;
        curl_easy_getinfo (dl->curl_ctx, CURLINFO_RESPONSE_CODE, &code);
        res = curl_easy_getinfo(dl->curl_ctx, CURLINFO_CONTENT_LENGTH_DOWNLOAD,
                                &size);
        if(res != CURLE_OK)
            zck_log(ZCK_LOG_WARNING, "Unable to get download size\n");
        else
            dl->dl += (size_t)(size + 0.5);
        if (code != 206 && code != 200) {
            zck_log(ZCK_LOG_ERROR, "HTTP Error: %li when download %s\n", code, url);
            return False;
        }
    }
    free(ra);
    return True;
}

int zck_dl_bytes(zckDL *dl, char *url, int dst_fd, size_t bytes, size_t start,
                 size_t *buffer_len) {
    if(start + bytes > *buffer_len) {
        zck_log(ZCK_LOG_DEBUG, "Seeking to end of temporary file\n");
        if(lseek(dst_fd, 0, SEEK_END) == -1) {
            zck_log(ZCK_LOG_ERROR, "Seek to end of temporary file failed: %s\n",
                    strerror(errno));
            return False;
        }
        zck_log(ZCK_LOG_DEBUG, "Downloading %lu bytes at position %lu\n", start+bytes-*buffer_len, *buffer_len);
        zckRangeInfo info = {0};
        zck_range_add(&info, *buffer_len, start+bytes-1);
        if(!zck_dl_range(dl, url, dst_fd, &info))
            return False;
        zck_range_close(&info);
        *buffer_len = start+bytes;
        zck_log(ZCK_LOG_DEBUG, "Seeking to position %lu\n", start);
        if(lseek(dst_fd, start, SEEK_SET) == -1) {
            zck_log(ZCK_LOG_ERROR,
                    "Seek to byte %lu of temporary file failed: %s\n", start,
                    strerror(errno));
            return False;
        }
    }
    return True;
}

int zck_dl_get_header(zckCtx *zck, zckDL *dl, char *url) {
    size_t buffer_len = 0;
    size_t start = 0;
    int temp_fd = zck_get_tmp_fd();

    if(!zck_dl_bytes(dl, url, temp_fd, 100, start, &buffer_len))
        return False;
    if(!zck_read_initial(zck, temp_fd))
        return False;
    start += 6;
    if(!zck_dl_bytes(dl, url, temp_fd, zck->hash_type.digest_size+9, start,
                     &buffer_len))
        return False;
    if(!zck_read_index_hash(zck, temp_fd))
        return False;
    start += zck->hash_type.digest_size;

    char *digest = zck_get_index_digest(zck);
    zck_log(ZCK_LOG_DEBUG, "Index hash: (%s)", zck_hash_name_from_type(zck_get_full_hash_type(zck)));
    for(int i=0; i<zck_get_full_digest_size(zck); i++)
        zck_log(ZCK_LOG_DEBUG, "%02x", (unsigned char)digest[i]);
    zck_log(ZCK_LOG_DEBUG, "\n");
    if(!zck_read_comp_type(zck, temp_fd))
        return False;
    start += 1;
    if(!zck_read_index_size(zck, temp_fd))
        return False;
    start += sizeof(uint64_t);
    zck_log(ZCK_LOG_DEBUG, "Index size: %llu\n", zck->comp_index_size);
    if(!zck_dl_bytes(dl, url, temp_fd, zck->comp_index_size, start,
                     &buffer_len))
        return False;
    if(!zck_read_index(zck, temp_fd))
        return False;
    return True;
}

size_t zck_dl_get_bytes_downloaded(zckDL *dl) {
    VALIDATE(dl);
    return dl->dl;
}

size_t zck_dl_get_bytes_uploaded(zckDL *dl) {
    VALIDATE(dl);
    return dl->ul;
}

int zck_dl_get_index(zckDL *dl, char *url) {
    VALIDATE(dl);
    return True;
}

void zck_dl_global_init() {
    curl_global_init(CURL_GLOBAL_ALL);
}

void zck_dl_global_cleanup() {
    curl_global_cleanup();
}
