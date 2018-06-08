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
#include <string.h>
#include <math.h>
#include <errno.h>
#include <zck.h>

#include "zck_private.h"

static zckRangeItem *range_insert_new(zckRangeItem *prev, zckRangeItem *next,
                                      uint64_t start, uint64_t end,
                                      zckRange *info, zckChunk *idx,
                                      int add_index) {
    zckRangeItem *new = zmalloc(sizeof(zckRangeItem));
    if(!new) {
        zck_log(ZCK_LOG_ERROR, "Unable to allocate %lu bytes\n",
                sizeof(zckRangeItem));
        return NULL;
    }
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
        if(!index_new_chunk(&(info->index), idx->digest, idx->digest_size,
                                end-start+1, end-start+1, False, NULL)) {
            free(new);
            return NULL;
        }
    return new;
}

static void range_remove(zckRangeItem *range) {
    if(range->prev)
        range->prev->next = range->next;
    if(range->next)
        range->next->prev = range->prev;
    free(range);
}

static void range_merge_combined(zckRange *info) {
    if(!info) {
        zck_log(ZCK_LOG_ERROR, "zckRange not allocated\n");
        return;
    }
    for(zckRangeItem *ptr=info->first; ptr;) {
        if(ptr->next && ptr->end >= ptr->next->start-1) {
            if(ptr->end < ptr->next->end)
                ptr->end = ptr->next->end;
            range_remove(ptr->next);
            info->count -= 1;
        } else {
            ptr = ptr->next;
        }
    }
}

static int range_add(zckRange *info, zckChunk *idx, zckCtx *zck) {
    if(info == NULL || idx == NULL) {
        zck_log(ZCK_LOG_ERROR, "zckRange or zckChunk not allocated\n");
        return False;
    }
    size_t header_len = 0;
    int add_index = False;
    if(zck) {
        header_len = zck_get_header_length(zck);
        add_index = True;
    }

    size_t start = idx->start + header_len;
    size_t end = idx->start + header_len + idx->comp_length - 1;
    zckRangeItem *prev = info->first;
    for(zckRangeItem *ptr=info->first; ptr;) {
        prev = ptr;
        if(start > ptr->start) {
            ptr = ptr->next;
            continue;
        } else if(start < ptr->start) {

            if(range_insert_new(ptr->prev, ptr, start, end, info, idx,
                                add_index) == NULL)
                return False;
            if(info->first == ptr) {
                info->first = ptr->prev;
            }
            info->count += 1;
            range_merge_combined(info);
            return True;
        } else { // start == ptr->start
            if(end > ptr->end)
                ptr->end = end;
            info->count += 1;
            range_merge_combined(info);
            return True;
        }
    }
    /* We've only reached here if we should be last item */
    zckRangeItem *new = range_insert_new(prev, NULL, start, end, info, idx,
                                         add_index);
    if(new == NULL)
        return False;
    if(info->first == NULL)
        info->first = new;
    info->count += 1;
    range_merge_combined(info);
    return True;
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

char PUBLIC *zck_get_range_char(zckRange *range) {
    int buf_size=BUF_SIZE;
    char *output=malloc(buf_size);
    if(!output) {
        zck_log(ZCK_LOG_ERROR, "Unable to allocate %lu bytes\n", buf_size);
        return NULL;
    }

    int loc = 0;
    int count = 0;
    zckRangeItem *ri = range->first;
    while(ri) {
        int length = snprintf(output+loc, buf_size-loc, "%lu-%lu,",
                              (long unsigned)ri->start,
                              (long unsigned)ri->end);
        if(length < 0) {
            zck_log(ZCK_LOG_ERROR, "Unable to get range: %s\n",
                    strerror(errno));
            free(output);
            return NULL;
        }
        if(length > buf_size-loc) {
            buf_size = (int)(buf_size * 1.5);
            output = realloc(output, buf_size);
            if(output == NULL) {
                zck_log(ZCK_LOG_ERROR, "Unable to allocate %lu bytes\n",
                        buf_size);
                return NULL;
            }
            continue;
        }
        loc += length;
        count++;
        ri = ri->next;
    }
    output[loc-1]='\0'; // Remove final comma
    output = realloc(output, loc);
    if(output == NULL) {
        zck_log(ZCK_LOG_ERROR, "Unable to shrink range to %lu bytes\n",
                loc);
        free(output);
        return NULL;
    }
    return output;
}

zckRange PUBLIC *zck_get_dl_range(zckCtx *zck, int max_ranges) {
    zckRange *range = zmalloc(sizeof(zckRange));
    if(range == NULL) {
        zck_log(ZCK_LOG_ERROR, "Unable to allocate %lu bytes\n",
                sizeof(zckRange));
        return NULL;
    }
    zckChunk *idx = zck->index.first;
    while(idx) {
        if(idx->valid) {
            idx = idx->next;
            continue;
        }
        if(!range_add(range, idx, zck)) {
            zck_range_free(&range);
            return NULL;
        }
        if(max_ranges >= 0 && range->count >= max_ranges)
            break;
        idx = idx->next;
    }
    return range;
}

char PUBLIC *zck_get_range(size_t start, size_t end) {
    zckRange range = {0};
    zckRangeItem ri = {0};
    range.first = &ri;
    ri.start = start;
    ri.end = end;
    return zck_get_range_char(&range);
}

int PUBLIC zck_get_range_count(zckRange *range) {
    if(range == NULL)
        return -1;
    return range->count;
}
