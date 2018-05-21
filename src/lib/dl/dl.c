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
#include <sys/types.h>
#include <errno.h>
#include <zck.h>

#include "zck_private.h"

#define VALIDATE(f)     if(!f) { \
                            zck_log(ZCK_LOG_ERROR, "zckDL not allocated\n"); \
                            return False; \
                        }

/* Free zckDL regex's used for downloading ranges */
void zck_dl_free_dl_regex(zckDL *dl) {
    if(dl == NULL || dl->priv == NULL)
        return;

    if(dl->priv->dl_regex) {
        regfree(dl->priv->dl_regex);
        free(dl->priv->dl_regex);
        dl->priv->dl_regex = NULL;
    }
    if(dl->priv->end_regex) {
        regfree(dl->priv->end_regex);
        free(dl->priv->end_regex);
        dl->priv->end_regex = NULL;
    }
}

/* Write zeros to tgt->fd in location of tgt_idx */
int zck_dl_write_zero(zckCtx *tgt, zckIndexItem *tgt_idx) {
    char buf[BUF_SIZE] = {0};
    size_t to_read = tgt_idx->comp_length;
    if(!seek_data(tgt->fd, tgt->data_offset + tgt_idx->start, SEEK_SET))
        return False;
    while(to_read > 0) {
        int rb = BUF_SIZE;
        if(rb > to_read)
            rb = to_read;
        if(!write_data(tgt->fd, buf, rb))
            return False;
        to_read -= rb;
    }
    return True;
}

int zck_dl_write(zckDL *dl, const char *at, size_t length) {
    VALIDATE(dl);
    VALIDATE(dl->priv);
    if(dl->priv->write_in_chunk < length)
        length = dl->priv->write_in_chunk;
    if(!write_data(dl->dst_fd, at, length))
        return -1;
    dl->priv->write_in_chunk -= length;
    return length;
}

int zck_dl_md_write(zckDL *dl, const char *at, size_t length) {
    VALIDATE(dl);
    VALIDATE(dl->priv);
    int wb = 0;
    if(dl->priv->write_in_chunk > 0) {
        wb = zck_dl_write(dl, at, length);
        if(!zck_hash_update(dl->priv->chunk_hash, at, wb))
            return 0;
        if(wb < 0)
            return 0;
        zck_log(ZCK_LOG_DEBUG, "Writing %lu bytes\n", wb);
        dl->priv->dl_chunk_data += wb;
    }
    return wb;
}

int zck_dl_write_chunk(zckDL *dl) {
    VALIDATE(dl);
    VALIDATE(dl->priv);
    if(dl->priv->chunk_hash == NULL) {
        zck_log(ZCK_LOG_ERROR, "Chunk hash not initialized\n");
        return False;
    }
    char *digest = zck_hash_finalize(dl->priv->chunk_hash);
    free(dl->priv->chunk_hash);
    if(memcmp(digest, dl->priv->tgt_check->digest,
              dl->priv->tgt_check->digest_size) != 0) {
        zck_log(ZCK_LOG_WARNING,
                "Downloaded chunk failed hash check\n");
        if(!zck_dl_write_zero(dl->zck, dl->priv->tgt_check))
            return False;
    } else {
        dl->priv->tgt_check->valid = True;
    }
    dl->priv->tgt_check = NULL;
    dl->priv->chunk_hash = NULL;
    free(digest);
    return True;
}

int zck_dl_write_range(zckDL *dl, const char *at, size_t length) {
    VALIDATE(dl);
    VALIDATE(dl->priv);
    if(dl->info.index.first == NULL) {
        zck_log(ZCK_LOG_ERROR, "zckDL index not initialized\n");
        return 0;
    }
    if(dl->zck == NULL || dl->zck->index.first == NULL) {
        zck_log(ZCK_LOG_ERROR, "zckCtx index not initialized\n");
        return 0;
    }
    int wb = zck_dl_md_write(dl, at, length);
    if(dl->priv->write_in_chunk == 0) {
        /* Check whether we just finished downloading a chunk and verify it */
        if(dl->priv->tgt_check && !zck_dl_write_chunk(dl))
            return False;
        zckIndexItem *idx = dl->info.index.first;
        while(idx) {
            if(dl->priv->dl_chunk_data == idx->start) {
                zckIndexItem *tgt_idx = dl->zck->index.first;
                while(tgt_idx) {
                    if(tgt_idx->valid)
                        tgt_idx = tgt_idx->next;
                    if(idx->comp_length == tgt_idx->comp_length &&
                       memcmp(idx->digest, tgt_idx->digest,
                              idx->digest_size) == 0) {
                        dl->priv->tgt_check = tgt_idx;
                        dl->priv->chunk_hash = zmalloc(sizeof(zckHash));
                        if(!zck_hash_init(dl->priv->chunk_hash,
                                          &(dl->zck->chunk_hash_type)))
                            return 0;
                        dl->priv->write_in_chunk = idx->comp_length;
                        if(!seek_data(dl->dst_fd,
                                      dl->zck->data_offset + tgt_idx->start,
                                      SEEK_SET))
                            return 0;
                        idx = NULL;
                        tgt_idx = NULL;
                    } else {
                        tgt_idx = tgt_idx->next;
                    }
                }
            }
            if(idx)
                idx = idx->next;
        }
    }
    int wb2 = 0;
    if(dl->priv->write_in_chunk > 0 && wb < length) {
        wb2 = zck_dl_write_range(dl, at+wb, length-wb);
        if(wb2 == 0)
            return 0;
    }
    return wb + wb2;
}

char *zck_dl_get_range_char(unsigned int start, unsigned int end) {
    zckRangeItem r = {0};
    zckRangeItem *range = &r;

    r.start = start;
    r.end = end;
    char *range_header = zck_range_get_char(&range, 2);
    return range_header;
}

static size_t dl_write_data(void *ptr, size_t l, size_t c, void *dl_v) {
    if(dl_v == NULL)
        return 0;
    zckDL *dl = (zckDL*)dl_v;
    size_t wb = 0;
    dl->dl += l*c;
    if(dl->boundary != NULL) {
        int retval = zck_multipart_extract(dl, ptr, l*c);
        if(retval == 0)
            wb = 0;
        else
            wb = l*c;
    } else if(dl->priv->is_chunk) {
        int retval = zck_dl_write_range(dl, ptr, l*c);
        if(retval == 0)
            wb = 0;
        else
            wb = l*c;
    } else {
        wb = write(dl->dst_fd, ptr, l*c);
    }
    return wb;
}

int zck_dl_write_and_verify(zckRange *info, zckCtx *src, zckCtx *tgt,
                            zckIndexItem *src_idx, zckIndexItem *tgt_idx) {
    static char buf[BUF_SIZE] = {0};

    size_t to_read = src_idx->comp_length;
    if(!seek_data(src->fd, src->data_offset + src_idx->start, SEEK_SET))
        return False;
    if(!seek_data(tgt->fd, tgt->data_offset + tgt_idx->start, SEEK_SET))
        return False;
    zckHash check_hash = {0};
    if(!zck_hash_init(&check_hash, &(src->chunk_hash_type)))
        return False;
    while(to_read > 0) {
        int rb = BUF_SIZE;
        if(rb > to_read)
            rb = to_read;
        if(!read_data(src->fd, buf, rb))
            return False;
        if(!zck_hash_update(&check_hash, buf, rb))
            return False;
        if(!write_data(tgt->fd, buf, rb))
            return False;
        to_read -= rb;
    }
    char *digest = zck_hash_finalize(&check_hash);
    /* If chunk is invalid, overwrite with zeros and add to download range */
    if(memcmp(digest, src_idx->digest, src_idx->digest_size) != 0) {
        char *pdigest = zck_get_chunk_digest(src_idx);
        zck_log(ZCK_LOG_WARNING, "Source hash: %s\n", pdigest);
        free(pdigest);
        pdigest = get_digest_string(digest, src_idx->digest_size);
        zck_log(ZCK_LOG_WARNING, "Target hash: %s\n", pdigest);
        free(pdigest);
        if(!zck_dl_write_zero(tgt, tgt_idx))
            return False;
        if(!zck_range_add(info, tgt_idx, tgt))
            return False;
    } else {
        tgt_idx->valid = True;
        zck_log(ZCK_LOG_DEBUG, "Writing %lu bytes at %lu\n",
                tgt_idx->comp_length, tgt_idx->start);
    }
    free(digest);
    return True;
}

int PUBLIC zck_dl_copy_src_chunks(zckRange *info, zckCtx *src, zckCtx *tgt) {
    zckIndex *tgt_info = zck_get_index(tgt);
    zckIndex *src_info = zck_get_index(src);
    zckIndexItem *tgt_idx = tgt_info->first;
    zckIndexItem *src_idx = src_info->first;
    while(tgt_idx) {
        int found = False;
        src_idx = src_info->first;

        while(src_idx) {
            if(tgt_idx->comp_length == src_idx->comp_length &&
               tgt_idx->length == src_idx->length &&
               memcmp(tgt_idx->digest, src_idx->digest,
                      tgt_idx->digest_size) == 0) {
                found = True;
                break;
            }
            src_idx = src_idx->next;
        }
        /* Write out found chunk, then verify that it's valid */
        if(found && !zck_dl_write_and_verify(info, src, tgt, src_idx, tgt_idx))
            return False;
        if(!found && !zck_range_add(info, tgt_idx, tgt))
            return False;
        tgt_idx = tgt_idx->next;
    }
    return True;
}

static size_t get_header(char *b, size_t l, size_t c, void *dl_v) {
    if(dl_v == NULL)
        return 0;
    zckDL *dl = (zckDL*)dl_v;

    return zck_multipart_get_boundary(dl, b, c*l);
}

int zck_dl_range_chk_chunk(zckDL *dl, char *url, int is_chunk) {
    if(dl == NULL || dl->priv == NULL || dl->info.first == NULL) {
        zck_log(ZCK_LOG_ERROR, "Struct not defined\n");
        return False;
    }
    if(dl->priv->parser_started) {
        zck_log(ZCK_LOG_ERROR, "Multipart parser already started\n");
        return False;
    }
    if(dl->info.segments == 0)
        dl->info.segments = 1;
    dl->priv->is_chunk = is_chunk;

    char **ra = calloc(sizeof(char*), dl->info.segments);
    if(!zck_range_get_array(&(dl->info), ra)) {
        free(ra);
        return False;
    }
    CURLcode res;

    for(int i=0; i<dl->info.segments; i++) {
        if(dl->priv->dl_regex != NULL)
            zck_dl_free_dl_regex(dl);
        if(dl->boundary != NULL)
            free(dl->boundary);

        curl_easy_setopt(dl->priv->curl_ctx, CURLOPT_URL, url);
        curl_easy_setopt(dl->priv->curl_ctx, CURLOPT_FOLLOWLOCATION, 1L);
        curl_easy_setopt(dl->priv->curl_ctx, CURLOPT_HEADERFUNCTION, get_header);
        curl_easy_setopt(dl->priv->curl_ctx, CURLOPT_HEADERDATA, dl);
        curl_easy_setopt(dl->priv->curl_ctx, CURLOPT_WRITEFUNCTION, dl_write_data);
        curl_easy_setopt(dl->priv->curl_ctx, CURLOPT_WRITEDATA, dl);
        curl_easy_setopt(dl->priv->curl_ctx, CURLOPT_RANGE, ra[i]);
        res = curl_easy_perform(dl->priv->curl_ctx);
        free(ra[i]);

        if(res != CURLE_OK) {
            zck_log(ZCK_LOG_ERROR, "Download failed: %s\n",
                    curl_easy_strerror(res));
            return False;
        }
        long code;
        curl_easy_getinfo (dl->priv->curl_ctx, CURLINFO_RESPONSE_CODE, &code);
        if (code != 206 && code != 200) {
            zck_log(ZCK_LOG_ERROR, "HTTP Error: %li when download %s\n", code,
                    url);
            return False;
        }
        zck_dl_clear_regex(dl);
    }
    free(ra);
    return True;
}

int PUBLIC zck_dl_range(zckDL *dl, char *url) {
    return zck_dl_range_chk_chunk(dl, url, 1);
}

int zck_dl_bytes(zckDL *dl, char *url, size_t bytes, size_t start,
                 size_t *buffer_len) {
    if(dl == NULL) {
        zck_log(ZCK_LOG_ERROR, "zckDL not initialized\n");
        return False;
    }
    if(start + bytes > *buffer_len) {
        zckIndexItem idx = {0};

        zck_log(ZCK_LOG_DEBUG, "Seeking to end of temporary file\n");
        if(lseek(dl->dst_fd, 0, SEEK_END) == -1) {
            zck_log(ZCK_LOG_ERROR, "Seek to end of temporary file failed: %s\n",
                    strerror(errno));
            return False;
        }
        zck_log(ZCK_LOG_DEBUG, "Downloading %lu bytes at position %lu\n", start+bytes-*buffer_len, *buffer_len);
        idx.start = *buffer_len;
        idx.comp_length = start+bytes-*buffer_len;
        zck_range_close(&(dl->info));
        if(!zck_range_add(&(dl->info), &idx, NULL))
            return False;
        if(!zck_dl_range_chk_chunk(dl, url, 0))
            return False;
        zck_range_close(&(dl->info));
        *buffer_len = start+bytes;
        zck_log(ZCK_LOG_DEBUG, "Seeking to position %lu\n", start);
        if(lseek(dl->dst_fd, start, SEEK_SET) == -1) {
            zck_log(ZCK_LOG_ERROR,
                    "Seek to byte %lu of temporary file failed: %s\n", start,
                    strerror(errno));
            return False;
        }
    }
    return True;
}

int zck_zero_bytes(zckDL *dl, size_t bytes, size_t start, size_t *buffer_len) {
    char buf[BUF_SIZE] = {0};
    if(start + bytes > *buffer_len) {
        zck_log(ZCK_LOG_DEBUG, "Seeking to end of temporary file\n");
        if(lseek(dl->dst_fd, 0, SEEK_END) == -1) {
            zck_log(ZCK_LOG_ERROR, "Seek to end of temporary file failed: %s\n",
                    strerror(errno));
            return False;
        }
        size_t write = *buffer_len;
        while(write < start + bytes) {
            size_t wb = BUF_SIZE;
            if(write + wb > start + bytes)
                wb = (start + bytes) - write;
            if(!write_data(dl->dst_fd, buf, wb))
                return False;
            write += wb;
        }
        zck_log(ZCK_LOG_DEBUG, "Wrote %lu zeros at position %lu\n", start+bytes-*buffer_len, *buffer_len);
        *buffer_len = start+bytes;
        zck_log(ZCK_LOG_DEBUG, "Seeking to position %lu\n", start);
        if(lseek(dl->dst_fd, start, SEEK_SET) == -1) {
            zck_log(ZCK_LOG_ERROR,
                    "Seek to byte %lu of temporary file failed: %s\n", start,
                    strerror(errno));
            return False;
        }
    }
    return True;
}

/* Download header */
int PUBLIC zck_dl_get_header(zckCtx *zck, zckDL *dl, char *url) {
    if(zck == NULL) {
        zck_log(ZCK_LOG_ERROR, "zckCtx not initialized\n");
        return False;
    }
    if(dl == NULL) {
        zck_log(ZCK_LOG_ERROR, "zckDL not initialized\n");
        return False;
    }
    size_t buffer_len = 0;
    size_t start = 0;
    zck->fd = dl->dst_fd;

    /* Download first hundred bytes and read magic and hash type */
    if(!zck_dl_bytes(dl, url, 200, start, &buffer_len))
        return False;
    if(!read_lead_1(zck))
        return False;
    start = tell_data(dl->dst_fd);

    if(!zck_dl_bytes(dl, url, zck->lead_size + zck->hash_type.digest_size,
                     start, &buffer_len))
        return False;
    if(!read_lead_2(zck))
        return False;
    zck_log(ZCK_LOG_DEBUG, "Header hash: (%s)",
            zck_hash_name_from_type(zck_get_full_hash_type(zck)));
    char *digest = zck_get_header_digest(zck);
    zck_log(ZCK_LOG_DEBUG, "%s\n", digest);
    free(digest);
    start = tell_data(dl->dst_fd);

    /* If we haven't downloaded enough for the index hash plus a few others, do
     * it now */
    if(!zck_dl_bytes(dl, url, zck->lead_size + zck->header_length,
                     start, &buffer_len))
        return False;

    /* Verify header checksum */
    if(!validate_header(zck))
        return False;
    zck_hash_close(&(zck->check_full_hash));

    /* Read the header */
    if(!read_preface(zck))
        return False;
    start += zck->preface_size;
    zck_log(ZCK_LOG_DEBUG, "Index size: %llu\n", zck->index_size);

    /* Read the index */
    if(!read_index(zck))
        return False;

    /* Read signatures */
    if(!read_sig(zck))
        return False;

    /* Write zeros to rest of file */
    zckIndex *info = &(dl->info.index);
    info->hash_type = zck->index.hash_type;
    zck_log(ZCK_LOG_DEBUG, "Writing zeros to rest of file: %llu\n", zck->index.length + zck->index_size + start);
    if(!zck_zero_bytes(dl, zck->index.length, zck->data_offset, &buffer_len))
        return False;
    return True;
}

size_t PUBLIC zck_dl_get_bytes_downloaded(zckDL *dl) {
    VALIDATE(dl);
    return dl->dl;
}

size_t PUBLIC zck_dl_get_bytes_uploaded(zckDL *dl) {
    VALIDATE(dl);
    return dl->ul;
}

void PUBLIC zck_dl_global_init() {
    curl_global_init(CURL_GLOBAL_ALL);
}

void PUBLIC zck_dl_global_cleanup() {
    curl_global_cleanup();
}

/* Free zckDL header regex used for downloading ranges */
void zck_dl_clear_regex(zckDL *dl) {
    if(dl == NULL || dl->priv == NULL)
        return;

    zck_dl_free_dl_regex(dl);
    if(dl->priv->hdr_regex) {
        regfree(dl->priv->hdr_regex);
        free(dl->priv->hdr_regex);
        dl->priv->hdr_regex = NULL;
    }
}

/* Initialize zckDL.  When finished, zckDL *must* be freed by zck_dl_free() */
zckDL PUBLIC *zck_dl_init() {
    zckDL *dl = zmalloc(sizeof(zckDL));
    if(!dl) {
        zck_log(ZCK_LOG_ERROR, "Unable to allocate %lu bytes for zckDL\n",
                sizeof(zckDL));
        return NULL;
    }
    dl->priv = zmalloc(sizeof(zckDLPriv));
    if(!dl->priv) {
        zck_log(ZCK_LOG_ERROR, "Unable to allocate %lu bytes for dl->priv\n",
                sizeof(zckDL));
        return NULL;
    }
    dl->priv->mp = zmalloc(sizeof(zckMP));
    if(!dl->priv->mp) {
        zck_log(ZCK_LOG_ERROR,
                "Unable to allocate %lu bytes for dl->priv->mp\n",
                sizeof(zckMP));
        return NULL;
    }
    dl->priv->curl_ctx = curl_easy_init();
    if(!dl->priv->curl_ctx) {
        zck_log(ZCK_LOG_ERROR, "Unable to allocate %lu bytes for dl->curl_ctx\n",
                sizeof(CURL));
        return NULL;
    }
    return dl;
}

/* Free zckDL and set pointer to NULL */
void PUBLIC zck_dl_free(zckDL **dl) {
    if(!*dl)
        return;
    if((*dl)->priv) {
        if((*dl)->priv->mp) {
            if((*dl)->priv->mp->buffer)
                free((*dl)->priv->mp->buffer);
            free((*dl)->priv->mp);
        }
        zck_dl_clear_regex(*dl);
        curl_easy_cleanup((*dl)->priv->curl_ctx);
        free((*dl)->priv);
    }
    if((*dl)->info.first)
        zck_range_close(&((*dl)->info));
    if((*dl)->boundary)
        free((*dl)->boundary);
    free(*dl);
    *dl = NULL;
}
