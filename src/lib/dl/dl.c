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
static void free_dl_regex(zckDL *dl) {
    if(dl == NULL || dl->priv == NULL)
        return;

    if(dl->priv->hdr_regex) {
        regfree(dl->priv->hdr_regex);
        free(dl->priv->hdr_regex);
        dl->priv->hdr_regex = NULL;
    }
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
static int zero_chunk(zckCtx *tgt, zckIndexItem *tgt_idx) {
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

static int dl_write(zckDL *dl, const char *at, size_t length) {
    VALIDATE(dl);
    VALIDATE(dl->priv);
    int wb = 0;
    if(dl->priv->write_in_chunk > 0) {
        if(dl->priv->write_in_chunk < length)
            wb = dl->priv->write_in_chunk;
        else
            wb = length;
        if(!write_data(dl->zck->fd, at, wb))
            return -1;
        dl->priv->write_in_chunk -= wb;
        if(!zck_hash_update(dl->priv->chunk_hash, at, wb))
            return -1;
        zck_log(ZCK_LOG_DEBUG, "Writing %lu bytes\n", wb);
        dl->priv->dl_chunk_data += wb;
    }
    return wb;
}

static int validate_chunk(zckDL *dl) {
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
        if(!zero_chunk(dl->zck, dl->priv->tgt_check))
            return False;
        dl->priv->tgt_check->valid = -1;
    } else {
        dl->priv->tgt_check->valid = 1;
    }
    dl->priv->tgt_check = NULL;
    dl->priv->chunk_hash = NULL;
    free(digest);
    return True;
}

int zck_dl_write_range(zckDL *dl, const char *at, size_t length) {
    VALIDATE(dl);
    VALIDATE(dl->priv);
    if(dl->range == NULL) {
        zck_log(ZCK_LOG_ERROR, "zckDL range not initialized\n");
        return 0;
    }

    if(dl->range->index.first == NULL) {
        zck_log(ZCK_LOG_ERROR, "zckDL index not initialized\n");
        return 0;
    }
    if(dl->zck == NULL || dl->zck->index.first == NULL) {
        zck_log(ZCK_LOG_ERROR, "zckCtx index not initialized\n");
        return 0;
    }
    int wb = dl_write(dl, at, length);
    if(wb < 0)
        return 0;
    if(dl->priv->write_in_chunk == 0) {
        /* Check whether we just finished downloading a chunk and verify it */
        if(dl->priv->tgt_check && !validate_chunk(dl)) {
            dl->priv->tgt_check->valid = -1;
            return False;
        }
        zckIndexItem *idx = dl->range->index.first;
        while(idx) {
            if(dl->priv->dl_chunk_data == idx->start) {
                zckIndexItem *tgt_idx = dl->zck->index.first;
                while(tgt_idx) {
                    if(tgt_idx->valid == 1)
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
                        if(!seek_data(dl->zck->fd,
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

size_t PUBLIC zck_write_zck_header_cb(void *ptr, size_t l, size_t c, void *dl_v) {
    if(dl_v == NULL)
        return 0;
    zckDL *dl = (zckDL*)dl_v;
    size_t wb = 0;
    dl->dl += l*c;
    size_t loc = tell_data(dl->zck->fd);
    zck_log(ZCK_LOG_DEBUG, "Downloading %lu bytes to position %lu\n", l*c, loc);
    wb = write(dl->zck->fd, ptr, l*c);
    if(dl->write_cb)
        return dl->write_cb(ptr, l, c, dl->wdata);
    return wb;
}

size_t PUBLIC zck_write_chunk_cb(void *ptr, size_t l, size_t c, void *dl_v) {
    if(dl_v == NULL)
        return 0;
    zckDL *dl = (zckDL*)dl_v;
    size_t wb = 0;
    dl->dl += l*c;
    if(dl->priv->boundary != NULL) {
        int retval = zck_multipart_extract(dl, ptr, l*c);
        if(retval == 0)
            wb = 0;
        else
            wb = l*c;
    } else {
        int retval = zck_dl_write_range(dl, ptr, l*c);
        if(retval == 0)
            wb = 0;
        else
            wb = l*c;
    }
    if(dl->write_cb)
        return dl->write_cb(ptr, l, c, dl->wdata);
    return wb;
}

int write_and_verify_chunk(zckCtx *src, zckCtx *tgt, zckIndexItem *src_idx,
                           zckIndexItem *tgt_idx) {
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
        if(!zero_chunk(tgt, tgt_idx))
            return False;
        tgt_idx->valid = -1;
    } else {
        tgt_idx->valid = 1;
        zck_log(ZCK_LOG_DEBUG, "Wrote %lu bytes at %lu\n",
                tgt_idx->comp_length, tgt_idx->start);
    }
    free(digest);
    return True;
}

int PUBLIC zck_copy_chunks(zckCtx *src, zckCtx *tgt) {
    zckIndex *tgt_info = zck_get_index(tgt);
    zckIndex *src_info = zck_get_index(src);
    zckIndexItem *tgt_idx = tgt_info->first;
    zckIndexItem *src_idx = src_info->first;
    while(tgt_idx) {
        /* No need to copy already valid chunk */
        if(tgt_idx->valid == 1) {
            tgt_idx = tgt_idx->next;
            continue;
        }

        int found = False;
        src_idx = src_info->first;

        while(src_idx) {
            if(tgt_idx->comp_length == src_idx->comp_length &&
               tgt_idx->length == src_idx->length &&
               tgt_idx->digest_size == src_idx->digest_size &&
               memcmp(tgt_idx->digest, src_idx->digest,
                      tgt_idx->digest_size) == 0) {
                found = True;
                break;
            }
            src_idx = src_idx->next;
        }
        /* Write out found chunk, then verify that it's valid */
        if(found)
            write_and_verify_chunk(src, tgt, src_idx, tgt_idx);
        tgt_idx = tgt_idx->next;
    }
    return True;
}

size_t PUBLIC zck_header_cb(char *b, size_t l, size_t c, void *dl_v) {
    if(dl_v == NULL)
        return 0;
    zckDL *dl = (zckDL*)dl_v;

    if(zck_multipart_get_boundary(dl, b, c*l) == 0)
        zck_log(ZCK_LOG_DEBUG, "No boundary detected");

    if(dl->header_cb)
        return dl->header_cb(b, l, c, dl->hdrdata);
    return c*l;
}


int zck_zero_bytes(zckDL *dl, size_t bytes, size_t start, size_t *buffer_len) {
    char buf[BUF_SIZE] = {0};
    if(start + bytes > *buffer_len) {
        zck_log(ZCK_LOG_DEBUG, "Seeking to end of temporary file\n");
        if(lseek(dl->zck->fd, 0, SEEK_END) == -1) {
            zck_log(ZCK_LOG_ERROR, "Seek to end of temporary file failed: %s\n",
                    strerror(errno));
            return False;
        }
        size_t write = *buffer_len;
        while(write < start + bytes) {
            size_t wb = BUF_SIZE;
            if(write + wb > start + bytes)
                wb = (start + bytes) - write;
            if(!write_data(dl->zck->fd, buf, wb))
                return False;
            write += wb;
        }
        zck_log(ZCK_LOG_DEBUG, "Wrote %lu zeros at position %lu\n", start+bytes-*buffer_len, *buffer_len);
        *buffer_len = start+bytes;
        zck_log(ZCK_LOG_DEBUG, "Seeking to position %lu\n", start);
        if(lseek(dl->zck->fd, start, SEEK_SET) == -1) {
            zck_log(ZCK_LOG_ERROR,
                    "Seek to byte %lu of temporary file failed: %s\n", start,
                    strerror(errno));
            return False;
        }
    }
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

/* Free zckDL header regex used for downloading ranges */
void zck_dl_clear_regex(zckDL *dl) {
    if(dl == NULL || dl->priv == NULL)
        return;

    free_dl_regex(dl);
    if(dl->priv->hdr_regex) {
        regfree(dl->priv->hdr_regex);
        free(dl->priv->hdr_regex);
        dl->priv->hdr_regex = NULL;
    }
}

/* Initialize zckDL.  When finished, zckDL *must* be freed by zck_dl_free() */
zckDL PUBLIC *zck_dl_init(zckCtx *zck) {
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
    dl->zck = zck;
    return dl;
}

/* Free zckDL and set pointer to NULL */
void PUBLIC zck_dl_reset(zckDL *dl) {
    if(!dl)
        return;
    if(dl->priv) {
        if(dl->priv->mp) {
            if(dl->priv->mp->buffer)
                free(dl->priv->mp->buffer);
            memset(dl->priv->mp, 0, sizeof(zckMP));
        }
        dl->priv->dl_chunk_data = 0;
        zck_dl_clear_regex(dl);
        if(dl->priv->boundary)
            free(dl->priv->boundary);
        dl->priv->boundary = NULL;
    }
    if(dl->range)
        zck_range_free(&(dl->range));

    zckCtx *zck = dl->zck;
    size_t db = dl->dl;
    size_t ub = dl->ul;
    zckDLPriv *priv = dl->priv;
    memset(dl, 0, sizeof(zckDL));
    dl->priv = priv;
    dl->zck = zck;
    dl->dl = db;
    dl->ul = ub;
}

/* Free zckDL and set pointer to NULL */
void PUBLIC zck_dl_free(zckDL **dl) {
    zck_dl_reset(*dl);
    if((*dl)->priv->mp)
        free((*dl)->priv->mp);
    if((*dl)->priv)
        free((*dl)->priv);
    free(*dl);
    *dl = NULL;
}
