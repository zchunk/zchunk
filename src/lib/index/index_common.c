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
#include <zck.h>

#include "zck_private.h"

zckIndexItem *zck_get_index_of_loc(zckIndex *index, size_t loc) {
    zckIndexItem *idx = index->first;
    while(idx != NULL) {
        if(loc >= idx->start && loc < idx->start + idx->comp_length)
            return idx;
        idx = idx->next;
    }
    zck_log(ZCK_LOG_ERROR, "Unable to find index that contains position %lu\n",
            loc);
    return NULL;
}

void zck_index_free_item(zckIndexItem **item) {
    if(*item == NULL)
        return;

    if((*item)->digest)
        free((*item)->digest);
    free(*item);
    *item = NULL;
    return;
}

void zck_index_clean(zckIndex *index) {
    if(index == NULL)
        return;

    if(index->first) {
        zckIndexItem *next;
        zckIndexItem *tmp=index->first;
        while(tmp != NULL) {
            next = tmp->next;
            zck_index_free_item(&tmp);
            tmp = next;
        }
    }
    memset(index, 0, sizeof(zckIndex));
}

void zck_index_free(zckCtx *zck) {
    zck_index_clean(&(zck->index));
    if(zck->full_hash_digest) {
        free(zck->full_hash_digest);
        zck->full_hash_digest = NULL;
    }
    if(zck->full_hash.ctx) {
        free(zck->full_hash.ctx);
        zck->full_hash.ctx = NULL;
    }
    if(zck->index_string) {
        free(zck->index_string);
        zck->index_string = NULL;
    }
    if(zck->header_string) {
        free(zck->header_string);
        zck->header_string = NULL;
    }
    if(zck->index_digest) {
        free(zck->index_digest);
        zck->index_digest = NULL;
    }
}
