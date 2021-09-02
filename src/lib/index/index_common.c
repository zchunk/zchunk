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
#include <zck.h>

#include "zck_private.h"

static void index_free_item(zckChunk **item) {
    if(*item == NULL)
        return;

    if((*item)->digest)
        free((*item)->digest);
    free(*item);
    *item = NULL;
    return;
}

void index_clean(zckIndex *index) {
    if(index == NULL)
        return;

    HASH_CLEAR(hh, index->ht);
    if(index->first) {
        zckChunk *next;
        zckChunk *tmp=index->first;
        while(tmp != NULL) {
            next = tmp->next;
            index_free_item(&tmp);
            tmp = next;
        }
    }
    memset(index, 0, sizeof(zckIndex));
}

void index_free(zckCtx *zck) {
    index_clean(&(zck->index));
    if(zck->full_hash_digest) {
        free(zck->full_hash_digest);
        zck->full_hash_digest = NULL;
    }
    if(zck->full_hash.ctx) {
        free(zck->full_hash.ctx);
        zck->full_hash.ctx = NULL;
    }
    zck->lead_string = NULL;
    zck->lead_size = 0;
    zck->preface_string = NULL;
    zck->preface_size = 0;
    zck->index_string = NULL;
    zck->index_size = 0;
    zck->sig_string = NULL;
    zck->sig_size = 0;
    if(zck->header_digest) {
        free(zck->header_digest);
        zck->header_digest = NULL;
    }
}

void clear_work_index(zckCtx *zck) {
    if(zck == NULL)
        return;

    hash_close(&(zck->work_index_hash));
    hash_close(&(zck->work_index_hash_uncomp));
    if(zck->work_index_item)
        index_free_item(&(zck->work_index_item));
}
