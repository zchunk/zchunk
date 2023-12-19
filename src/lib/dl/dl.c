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
#include <stdbool.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <errno.h>
#include <zck.h>

#include "zck_private.h"

/* Free zckDL header regex used for downloading ranges */
static void clear_dl_regex(zckDL *dl) {
    if(dl == NULL)
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
static bool zero_chunk(zckCtx *tgt, zckChunk *tgt_idx) {
    char buf[BUF_SIZE] = {0};
    size_t to_read = tgt_idx->comp_length;
    if(!seek_data(tgt, tgt->data_offset + tgt_idx->start, SEEK_SET))
        return false;
    while(to_read > 0) {
        int rb = BUF_SIZE;
        if(rb > to_read)
            rb = to_read;
        if(!write_data(tgt, tgt->fd, buf, rb))
            return false;
        to_read -= rb;
    }
    return true;
}

/* Check whether last downloaded chunk is valid and zero it out if it isn't */
static bool set_chunk_valid(zckDL *dl) {
    ALLOCD_BOOL(NULL, dl);
    VALIDATE_BOOL(dl->zck);

    int retval = validate_chunk(dl->tgt_check, ZCK_LOG_WARNING);
    if(retval < 1) {
        if(!zero_chunk(dl->zck, dl->tgt_check))
            return false;
        dl->tgt_check->valid = -1;
        return false;
    } else {
        dl->tgt_check->valid = 1;
    }
    dl->tgt_check = NULL;
    return true;
}

/* Write length or to end of current chunk, whichever comes first */
static int dl_write(zckDL *dl, const char *at, size_t length) {
    ALLOCD_INT(NULL, dl);
    VALIDATE_INT(dl->zck);

    int wb = 0;
    if(dl->write_in_chunk > 0) {
        if(dl->write_in_chunk < length)
            wb = dl->write_in_chunk;
        else
            wb = length;
        if(!write_data(dl->zck, dl->zck->fd, at, wb))
            return -1;
        dl->write_in_chunk -= wb;
        if(!hash_update(dl->zck, &(dl->zck->check_chunk_hash), at, wb))
            return -1;
        zck_log(ZCK_LOG_DEBUG, "Writing %llu bytes", (long long unsigned) wb);
        dl->dl_chunk_data += wb;
    }
    return wb;
}

/* Copy chunk identified by src_idx into location specified by tgt_idx */
static bool write_and_verify_chunk(zckCtx *src, zckCtx *tgt,
                                   zckChunk *src_idx,
                                   zckChunk *tgt_idx) {
    VALIDATE_READ_BOOL(src);
    VALIDATE_READ_BOOL(tgt);

    static char buf[BUF_SIZE] = {0};

    size_t to_read = src_idx->comp_length;
    if(!seek_data(src, src->data_offset + src_idx->start, SEEK_SET))
        return false;
    if(!seek_data(tgt, tgt->data_offset + tgt_idx->start, SEEK_SET))
        return false;
    zckHash check_hash = {0};
    if(!hash_init(tgt, &check_hash, &(src->chunk_hash_type)))
        return false;
    while(to_read > 0) {
        int rb = BUF_SIZE;
        if(rb > to_read)
            rb = to_read;
        if(!read_data(src, buf, rb))
            return false;
        if(!hash_update(tgt, &check_hash, buf, rb))
            return false;
        if(!write_data(tgt, tgt->fd, buf, rb))
            return false;
        to_read -= rb;
    }
    char *digest = hash_finalize(tgt, &check_hash);
    /* If chunk is invalid, overwrite with zeros and add to download range */
    if(memcmp(digest, src_idx->digest, src_idx->digest_size) != 0) {
        char *pdigest = zck_get_chunk_digest(src_idx);
        zck_log(ZCK_LOG_INFO, "Corrupted chunk found in file, will redownload");
        zck_log(ZCK_LOG_INFO, "Source hash: %s", pdigest);
        free(pdigest);
        pdigest = get_digest_string(digest, src_idx->digest_size);
        zck_log(ZCK_LOG_INFO, "Target hash: %s", pdigest);
        free(pdigest);
        if(!zero_chunk(tgt, tgt_idx))
            return false;
        tgt_idx->valid = -1;
    } else {
        tgt_idx->valid = 1;
        zck_log(ZCK_LOG_DEBUG, "Wrote %llu bytes at %llu",
                (long long unsigned) tgt_idx->comp_length,
                (long long unsigned) tgt_idx->start
        );
    }
    free(digest);
    return true;
}

/* Split current read into the appropriate chunks and write appropriately */
int dl_write_range(zckDL *dl, const char *at, size_t length) {
    ALLOCD_BOOL(NULL, dl);
    VALIDATE_BOOL(dl->zck);

    if(dl->range == NULL) {
        set_error(dl->zck, "zckDL range not initialized");
        return 0;
    }

    if(dl->range->index.first == NULL) {
        set_error(dl->zck, "zckDL index not initialized");
        return 0;
    }
    if(dl->zck->index.first == NULL) {
        set_error(dl->zck, "zckCtx index not initialized");
        return 0;
    }
    int wb = dl_write(dl, at, length);
    if(wb < 0)
        return 0;
    if(dl->write_in_chunk == 0) {
        /* Check whether we just finished downloading a chunk and verify it */
        if(dl->tgt_check && !set_chunk_valid(dl))
            return false;

        if(dl->range->index.current == NULL)
            dl->range->index.current = dl->range->index.first;
        for(zckChunk *chk = dl->range->index.current; chk; chk = chk->next) {
            if(dl->dl_chunk_data != chk->start)
                continue;

            int count = 0;
            zckChunk *tgt_chk = chk->src;
            if(tgt_chk->valid == 1)
                continue;
            if(chk->comp_length == tgt_chk->comp_length &&
               memcmp(chk->digest, tgt_chk->digest,
                      chk->digest_size) == 0) {
                dl->tgt_check = tgt_chk;
                dl->tgt_number = count;
                if(!hash_init(dl->zck, &(dl->zck->check_chunk_hash),
                              &(dl->zck->chunk_hash_type)))
                    return 0;
                dl->write_in_chunk = chk->comp_length;
                if(!seek_data(dl->zck,
                              dl->zck->data_offset + tgt_chk->start,
                              SEEK_SET))
                    return 0;
                dl->range->index.current = chk->next;
                chk = NULL;
                tgt_chk = NULL;
                break;
            }
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

bool ZCK_PUBLIC_API zck_copy_chunks(zckCtx *src, zckCtx *tgt) {
    VALIDATE_READ_BOOL(src);
    VALIDATE_READ_BOOL(tgt);

    zckIndex *tgt_info = &(tgt->index);
    zckIndex *src_info = &(src->index);
    zckChunk *tgt_idx = tgt_info->first;
    while(tgt_idx) {
        /* No need to copy already valid chunk */
        if(tgt_idx->valid == 1) {
            tgt_idx = tgt_idx->next;
            continue;
        }
        zckChunk *f = NULL;

        HASH_FIND(hh, src_info->ht, tgt_idx->digest, tgt_idx->digest_size, f);
        if(f && f->length == tgt_idx->length &&
           f->comp_length == tgt_idx->comp_length)
            write_and_verify_chunk(src, tgt, f, tgt_idx);
        tgt_idx = tgt_idx->next;
    }
    return true;
}

bool ZCK_PUBLIC_API zck_find_matching_chunks(zckCtx *src, zckCtx *tgt) {

    if (!src || !tgt)
        return false;

    zckIndex *src_info = &(src->index);
    zckIndex *tgt_info = &(tgt->index);
    zckChunk *tgt_idx = tgt_info->first;
    while(tgt_idx) {
        zckChunk *f = NULL;
        /*
         * This function can be called multiple time with different
         * zckCtx *src, and the resulting tgt will have a list with
         * chunks from different sources. Check first if comparison ran
         * for the chunk and it was already set as valid
         */
        if (tgt_idx->valid) {
            tgt_idx = tgt_idx->next;
            continue;
        }

        /*
         * Compare digest for compressed data if the same compressor
         * was iused
         */
        if (src->comp.type == tgt->comp.type) {
            HASH_FIND(hh, src_info->ht, tgt_idx->digest, tgt_idx->digest_size, f);
        } else if (src->has_uncompressed_source && tgt->has_uncompressed_source) {
            HASH_FIND(hhuncomp, src_info->htuncomp, tgt_idx->digest_uncompressed, tgt_idx->digest_size, f);
        } else {

        }
        if(f && f->length == tgt_idx->length) {
            tgt_idx->valid = 1;
            tgt_idx->src = f;
        } else {
            tgt_idx->src = tgt_idx;
        }
        tgt_idx = tgt_idx->next;
    }

    return true;
}

ssize_t ZCK_PUBLIC_API zck_dl_get_bytes_downloaded(zckDL *dl) {
    ALLOCD_INT(NULL, dl);

    return dl->dl;
}

ssize_t ZCK_PUBLIC_API zck_dl_get_bytes_uploaded(zckDL *dl) {
    ALLOCD_INT(NULL, dl);

    return dl->ul;
}

/* Initialize zckDL.  When finished, zckDL *must* be freed by zck_dl_free() */
zckDL ZCK_PUBLIC_API *zck_dl_init(zckCtx *zck) {
    zckDL *dl = zmalloc(sizeof(zckDL));
    if (!dl) {
       zck_log(ZCK_LOG_ERROR, "OOM in %s", __func__);
       return NULL;
    }
    dl->mp = zmalloc(sizeof(zckMP));
    dl->zck = zck;
    return dl;
}

/* Reset dl while maintaining download statistics and private information */
void ZCK_PUBLIC_API zck_dl_reset(zckDL *dl) {
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
void ZCK_PUBLIC_API zck_dl_free(zckDL **dl) {
    zck_dl_reset(*dl);
    if((*dl)->mp)
        free((*dl)->mp);
    free(*dl);
    *dl = NULL;
}

zckCtx ZCK_PUBLIC_API *zck_dl_get_zck(zckDL *dl) {
    ALLOCD_PTR(NULL, dl);

    return dl->zck;
}

bool ZCK_PUBLIC_API zck_dl_set_zck(zckDL *dl, zckCtx *zck) {
    ALLOCD_BOOL(NULL, dl);

    dl->zck = zck;
    return true;
}
bool ZCK_PUBLIC_API zck_dl_set_range(zckDL *dl, zckRange *range) {
    ALLOCD_BOOL(NULL, dl);

    dl->range = range;
    return true;
}

zckRange ZCK_PUBLIC_API *zck_dl_get_range(zckDL *dl) {
    ALLOCD_PTR(NULL, dl);

    return dl->range;
}

bool ZCK_PUBLIC_API zck_dl_set_header_cb(zckDL *dl, zck_wcb func) {
    ALLOCD_BOOL(NULL, dl);

    dl->header_cb = func;
    return true;
}

bool ZCK_PUBLIC_API zck_dl_set_header_data(zckDL *dl, void *data) {
    ALLOCD_BOOL(NULL, dl);

    dl->header_data = data;
    return true;
}

bool ZCK_PUBLIC_API zck_dl_set_write_cb(zckDL *dl, zck_wcb func) {
    ALLOCD_BOOL(NULL, dl);

    dl->write_cb = func;
    return true;
}

bool ZCK_PUBLIC_API zck_dl_set_write_data(zckDL *dl, void *data) {
    ALLOCD_BOOL(NULL, dl);

    dl->write_data = data;
    return true;
}

/*******************************************************************
 * Callbacks
 *******************************************************************/

size_t ZCK_PUBLIC_API zck_header_cb(char *b, size_t l, size_t c, void *dl_v) {
    ALLOCD_BOOL(NULL, dl_v);
    zckDL *dl = (zckDL*)dl_v;

    if(multipart_get_boundary(dl, b, c*l) == 0)
        zck_log(ZCK_LOG_DEBUG, "No boundary detected");

    if(dl->header_cb)
        return dl->header_cb(b, l, c, dl->header_data);
    return c*l;
}

size_t ZCK_PUBLIC_API zck_write_zck_header_cb(void *ptr, size_t l, size_t c,
                                      void *dl_v) {
    ALLOCD_BOOL(NULL, dl_v);
    zckDL *dl = (zckDL*)dl_v;

    size_t wb = 0;
    dl->dl += l*c;
    size_t loc = tell_data(dl->zck);
    zck_log(ZCK_LOG_DEBUG, "Downloading %llu bytes to position %llu",
            (long long unsigned) l*c,
            (long long unsigned) loc
    );
    wb = write(dl->zck->fd, ptr, l*c);
    if(dl->write_cb)
        return dl->write_cb(ptr, l, c, dl->write_data);
    return wb;
}

size_t ZCK_PUBLIC_API zck_write_chunk_cb(void *ptr, size_t l, size_t c, void *dl_v) {
    ALLOCD_BOOL(NULL, dl_v);
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
