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
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <math.h>
#include <errno.h>
#include <zck.h>

#include "zck_private.h"

static zckRangeItem *range_insert_new(zckCtx *zck, zckRangeItem *prev,
                                      zckRangeItem *next, uint64_t start,
                                      uint64_t end, zckRange *info,
                                      zckChunk *idx, int add_index) {
    VALIDATE_PTR(zck);

    zckRangeItem *new = zmalloc(sizeof(zckRangeItem));
    new->start = start;
    new->end = end;
    if(prev) {
        new->prev = prev;
        prev->next = new;
    }
    if(next) {
        new->next = next;
        next->prev = new;
    }
    if(add_index)
        if(!index_new_chunk(zck, &(info->index), idx->digest, idx->digest_size,
                                end-start+1, end-start+1, false)) {
            free(new);
            return NULL;
        }
    return new;
}

static void range_remove(zckCtx *zck, zckRangeItem *range) {
    if(range->prev)
        range->prev->next = range->next;
    if(range->next)
        range->next->prev = range->prev;
    free(range);
}

static void range_merge_combined(zckCtx *zck, zckRange *info) {
    if(!info) {
        set_error(zck, "zckRange not allocated");
        return;
    }
    for(zckRangeItem *ptr=info->first; ptr;) {
        if(ptr->next && ptr->end >= ptr->next->start-1) {
            if(ptr->end < ptr->next->end)
                ptr->end = ptr->next->end;
            range_remove(zck, ptr->next);
            info->count -= 1;
        } else {
            ptr = ptr->next;
        }
    }
}

static bool range_add(zckRange *info, zckChunk *chk, zckCtx *zck) {
    if(info == NULL || chk == NULL) {
        set_error(zck, "zckRange or zckChunk not allocated");
        return false;
    }
    size_t header_len = 0;
    bool add_index = false;
    if(zck) {
        header_len = zck_get_header_length(zck);
        add_index = true;
    }

    size_t start = chk->start + header_len;
    size_t end = chk->start + header_len + chk->comp_length - 1;
    zckRangeItem *prev = info->first;
    for(zckRangeItem *ptr=info->first; ptr;) {
        prev = ptr;
        if(start > ptr->start) {
            ptr = ptr->next;
            continue;
        } else if(start < ptr->start) {
            if(range_insert_new(zck, ptr->prev, ptr, start, end, info, chk,
                                add_index) == NULL)
                return false;
            if(info->first == ptr) {
                info->first = ptr->prev;
            }
            info->count += 1;
            range_merge_combined(zck, info);
            return true;
        } else { // start == ptr->start
            if(end > ptr->end)
                ptr->end = end;
            info->count += 1;
            range_merge_combined(zck, info);
            return true;
        }
    }
    /* We've only reached here if we should be last item */
    zckRangeItem *new = range_insert_new(zck, prev, NULL, start, end, info, chk,
                                         add_index);
    if(new == NULL)
        return false;
    if(info->first == NULL)
        info->first = new;
    info->count += 1;
    range_merge_combined(zck, info);
    return true;
}

void PUBLIC zck_range_free(zckRange **info) {
    zckRangeItem *next = (*info)->first;
    while(next) {
        zckRangeItem *tmp = next;
        next = next->next;
        free(tmp);
    }
    index_clean(&((*info)->index));
    free(*info);
    *info = NULL;
}

char PUBLIC *zck_get_range_char(zckCtx *zck, zckRange *range) {
    int buf_size = BUF_SIZE;
    char *output = zmalloc(buf_size);
    int loc = 0;
    int count = 0;
    zckRangeItem *ri = range->first;
    while(ri) {
        int length = snprintf(output+loc, buf_size-loc, "%lu-%lu,",
                              (long unsigned)ri->start,
                              (long unsigned)ri->end);
        if(length < 0) {
            set_fatal_error(zck, "Unable to get range: %s", strerror(errno));
            free(output);
            return NULL;
        }
        if(length > buf_size-loc) {
            buf_size = (int)(buf_size * 1.5);
            output = zrealloc(output, buf_size);
            continue;
        }
        loc += length;
        count++;
        ri = ri->next;
    }
    output[loc-1]='\0'; // Remove final comma
    output = zrealloc(output, loc);
    return output;
}

zckRange PUBLIC *zck_get_missing_range(zckCtx *zck, int max_ranges) {
    VALIDATE_PTR(zck);

    zckRange *range = zmalloc(sizeof(zckRange));
    for(zckChunk *chk = zck->index.first; chk; chk = chk->next) {
        if(chk->valid)
            continue;

        if(!range_add(range, chk, zck)) {
            zck_range_free(&range);
            return NULL;
        }
        if(max_ranges >= 0 && range->count >= max_ranges)
            break;
    }
    return range;
}

char PUBLIC *zck_get_range(size_t start, size_t end) {
    zckRange range = {0};
    zckRangeItem ri = {0};
    zckCtx *zck = zck_create();
    range.first = &ri;
    ri.start = start;
    ri.end = end;
    char *ret = zck_get_range_char(zck, &range);
    zck_free(&zck);
    return ret;
}

int PUBLIC zck_get_range_count(zckRange *range) {
    ALLOCD_INT(NULL, range);

    return range->count;
}
