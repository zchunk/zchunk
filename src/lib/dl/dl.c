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

/* Free zckDL header regex used for downloading ranges */
static void clear_dl_regex(zckDL *dl) {
    if(dl == NULL || dl == NULL)
        return;

    if(dl->hdr_regex) {
        regfree(dl->hdr_regex);
        free(dl->hdr_regex);
        dl->hdr_regex = NULL;
    }
    if(dl->dl_regex) {
        regfree(dl->dl_regex);
        free(dl->dl_regex);
        dl->dl_regex = NULL;
    }
    if(dl->end_regex) {
        regfree(dl->end_regex);
        free(dl->end_regex);
        dl->end_regex = NULL;
    }
}

/* Write zeros to tgt->fd in location of tgt_idx */
static int zero_chunk(zckCtx *tgt, zckChunk *tgt_idx) {
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

/* Check whether last downloaded chunk is valid and zero it out if it isn't */
static int set_chunk_valid(zckDL *dl) {
    VALIDATE(dl);
    VALIDATE(dl);

    int retval = validate_chunk(dl->zck, dl->tgt_check, ZCK_LOG_WARNING,
                                dl->tgt_number);
    if(retval < 1) {
        if(!zero_chunk(dl->zck, dl->tgt_check))
            return False;
        dl->tgt_check->valid = -1;
        if(retval == 0)
            return False;
    } else {
        dl->tgt_check->valid = 1;
    }
    dl->tgt_check = NULL;
    return True;
}

/* Write length or to end of current chunk, whichever comes first */
static int dl_write(zckDL *dl, const char *at, size_t length) {
    VALIDATE(dl);
    VALIDATE(dl);
    int wb = 0;
    if(dl->write_in_chunk > 0) {
        if(dl->write_in_chunk < length)
            wb = dl->write_in_chunk;
        else
            wb = length;
        if(!write_data(dl->zck->fd, at, wb))
            return -1;
        dl->write_in_chunk -= wb;
        if(!hash_update(&(dl->zck->check_chunk_hash), at, wb))
            return -1;
        zck_log(ZCK_LOG_DEBUG, "Writing %lu bytes\n", wb);
        dl->dl_chunk_data += wb;
    }
    return wb;
}

/* Copy chunk identified by src_idx into location specified by tgt_idx */
static int write_and_verify_chunk(zckCtx *src, zckCtx *tgt,
                                  zckChunk *src_idx,
                                  zckChunk *tgt_idx) {
    static char buf[BUF_SIZE] = {0};

    size_t to_read = src_idx->comp_length;
    if(!seek_data(src->fd, src->data_offset + src_idx->start, SEEK_SET))
        return False;
    if(!seek_data(tgt->fd, tgt->data_offset + tgt_idx->start, SEEK_SET))
        return False;
    zckHash check_hash = {0};
    if(!hash_init(&check_hash, &(src->chunk_hash_type)))
        return False;
    while(to_read > 0) {
        int rb = BUF_SIZE;
        if(rb > to_read)
            rb = to_read;
        if(!read_data(src->fd, buf, rb))
            return False;
        if(!hash_update(&check_hash, buf, rb))
            return False;
        if(!write_data(tgt->fd, buf, rb))
            return False;
        to_read -= rb;
    }
    char *digest = hash_finalize(&check_hash);
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

/* Split current read into the appropriate chunks and write appropriately */
int dl_write_range(zckDL *dl, const char *at, size_t length) {
    VALIDATE(dl);
    VALIDATE(dl);
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
    if(dl->write_in_chunk == 0) {
        /* Check whether we just finished downloading a chunk and verify it */
        if(dl->tgt_check && !set_chunk_valid(dl))
            return False;

        for(zckChunk *idx = dl->range->index.first; idx; idx = idx->next) {
            if(dl->dl_chunk_data == idx->start) {
                int count = 0;
                for(zckChunk *tgt_idx = dl->zck->index.first; tgt_idx;
                    tgt_idx = tgt_idx->next, count++) {
                    if(tgt_idx->valid == 1)
                        continue;
                    if(idx->comp_length == tgt_idx->comp_length &&
                       memcmp(idx->digest, tgt_idx->digest,
                              idx->digest_size) == 0) {
                        dl->tgt_check = tgt_idx;
                        dl->tgt_number = count;
                        if(!hash_init(&(dl->zck->check_chunk_hash),
                                          &(dl->zck->chunk_hash_type)))
                            return 0;
                        dl->write_in_chunk = idx->comp_length;
                        if(!seek_data(dl->zck->fd,
                                      dl->zck->data_offset + tgt_idx->start,
                                      SEEK_SET))
                            return 0;
                        idx = NULL;
                        tgt_idx = NULL;
                        break;
                    }
                }
            }
            if(!idx)
                break;
        }
    }
    int wb2 = 0;
    /* We've still got data, call recursively */
    if(dl->write_in_chunk > 0 && wb < length) {
        wb2 = dl_write_range(dl, at+wb, length-wb);
        if(wb2 == 0)
            return 0;
    }
    return wb + wb2;
}

int PUBLIC zck_copy_chunks(zckCtx *src, zckCtx *tgt) {
    zckIndex *tgt_info = &(tgt->index);
    zckIndex *src_info = &(src->index);
    zckChunk *tgt_idx = tgt_info->first;
    zckChunk *src_idx = src_info->first;
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

size_t PUBLIC zck_dl_get_bytes_downloaded(zckDL *dl) {
    VALIDATE(dl);
    return dl->dl;
}

size_t PUBLIC zck_dl_get_bytes_uploaded(zckDL *dl) {
    VALIDATE(dl);
    return dl->ul;
}

/* Initialize zckDL.  When finished, zckDL *must* be freed by zck_dl_free() */
zckDL PUBLIC *zck_dl_init(zckCtx *zck) {
    zckDL *dl = zmalloc(sizeof(zckDL));
    if(!dl) {
        zck_log(ZCK_LOG_ERROR, "Unable to allocate %lu bytes for zckDL\n",
                sizeof(zckDL));
        return NULL;
    }
    dl->mp = zmalloc(sizeof(zckMP));
    if(!dl->mp) {
        zck_log(ZCK_LOG_ERROR,
                "Unable to allocate %lu bytes for dl->mp\n",
                sizeof(zckMP));
        return NULL;
    }
    dl->zck = zck;
    return dl;
}

/* Reset dl while maintaining download statistics and private information */
void PUBLIC zck_dl_reset(zckDL *dl) {
    if(!dl)
        return;
    reset_mp(dl->mp);
    dl->dl_chunk_data = 0;
    clear_dl_regex(dl);
    if(dl->boundary)
        free(dl->boundary);
    dl->boundary = NULL;

    zckCtx *zck = dl->zck;
    size_t db = dl->dl;
    size_t ub = dl->ul;
    zckMP *mp = dl->mp;
    memset(dl, 0, sizeof(zckDL));
    dl->zck = zck;
    dl->dl = db;
    dl->ul = ub;
    dl->mp = mp;
}

/* Free zckDL and set pointer to NULL */
void PUBLIC zck_dl_free(zckDL **dl) {
    zck_dl_reset(*dl);
    if((*dl)->mp)
        free((*dl)->mp);
    free(*dl);
    *dl = NULL;
}

zckCtx PUBLIC *zck_dl_get_zck(zckDL *dl) {
    if(dl == NULL)
        return NULL;
    return dl->zck;
}

int PUBLIC zck_dl_set_zck(zckDL *dl, zckCtx *zck) {
    if(dl == NULL)
        return False;
    dl->zck = zck;
    return True;
}
int PUBLIC zck_dl_set_range(zckDL *dl, zckRange *range) {
    if(dl == NULL)
        return False;
    dl->range = range;
    return True;
}

zckRange PUBLIC *zck_dl_get_range(zckDL *dl) {
    if(dl == NULL)
        return NULL;
    return dl->range;
}

int PUBLIC zck_dl_set_header_cb(zckDL *dl, zck_wcb func) {
    if(dl == NULL)
        return False;
    dl->header_cb = func;
    return True;
}

int PUBLIC zck_dl_set_header_data(zckDL *dl, void *data) {
    if(dl == NULL)
        return False;
    dl->header_data = data;
    return True;
}

int PUBLIC zck_dl_set_write_cb(zckDL *dl, zck_wcb func) {
    if(dl == NULL)
        return False;
    dl->write_cb = func;
    return True;
}

int PUBLIC zck_dl_set_write_data(zckDL *dl, void *data) {
    if(dl == NULL)
        return False;
    dl->write_data = data;
    return True;
}

/*******************************************************************
 * Callbacks
 *******************************************************************/

size_t PUBLIC zck_header_cb(char *b, size_t l, size_t c, void *dl_v) {
    if(dl_v == NULL)
        return 0;
    zckDL *dl = (zckDL*)dl_v;

    if(multipart_get_boundary(dl, b, c*l) == 0)
        zck_log(ZCK_LOG_DEBUG, "No boundary detected");

    if(dl->header_cb)
        return dl->header_cb(b, l, c, dl->header_data);
    return c*l;
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
        return dl->write_cb(ptr, l, c, dl->write_data);
    return wb;
}

size_t PUBLIC zck_write_chunk_cb(void *ptr, size_t l, size_t c, void *dl_v) {
    if(dl_v == NULL)
        return 0;
    zckDL *dl = (zckDL*)dl_v;
    size_t wb = 0;
    dl->dl += l*c;
    if(dl->boundary != NULL) {
        int retval = multipart_extract(dl, ptr, l*c);
        if(retval == 0)
            wb = 0;
        else
            wb = l*c;
    } else {
        int retval = dl_write_range(dl, ptr, l*c);
        if(retval == 0)
            wb = 0;
        else
            wb = l*c;
    }
    if(dl->write_cb)
        return dl->write_cb(ptr, l, c, dl->write_data);
    return wb;
}
