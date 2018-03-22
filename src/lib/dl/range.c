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

void zck_range_remove(zckRangeItem *range) {
    if(range->prev)
        range->prev->next = range->next;
    if(range->next)
        range->next->prev = range->prev;
    free(range);
}

void zck_range_close(zckRange *info) {
    zckRangeItem *next = info->first;
    while(next) {
        zckRangeItem *tmp = next;
        next = next->next;
        free(tmp);
    }
    zck_index_clean(&(info->index));
    memset(info, 0, sizeof(zckRange));
}

zckRangeItem *zck_range_insert_new(zckRangeItem *prev, zckRangeItem *next, uint64_t start,
                               uint64_t end, zckRange *info,
                               zckIndexItem *idx, int add_index) {
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
        if(!zck_index_new_chunk(&(info->index), idx->digest, idx->digest_size,
                                end-start+1, False)) {
            free(new);
            return NULL;
        }
    return new;
}

void zck_range_merge_combined(zckRange *info) {
    if(!info) {
        zck_log(ZCK_LOG_ERROR, "zckRange not allocated\n");
        return;
    }
    for(zckRangeItem *ptr=info->first; ptr;) {
        if(ptr->next && ptr->end >= ptr->next->start-1) {
            if(ptr->end < ptr->next->end)
                ptr->end = ptr->next->end;
            zck_range_remove(ptr->next);
            info->count -= 1;
        } else {
            ptr = ptr->next;
        }
    }
}

int zck_range_add(zckRange *info, zckIndexItem *idx, zckCtx *zck) {
    if(info == NULL || idx == NULL) {
        zck_log(ZCK_LOG_ERROR, "zckRange or zckIndexItem not allocated\n");
        return False;
    }
    size_t header_len = 0;
    int add_index = False;
    if(zck) {
        header_len = zck_get_header_length(zck);
        add_index = True;
    }

    size_t start = idx->start + header_len;
    size_t end = idx->start + header_len + idx->length - 1;
    zckRangeItem *prev = info->first;
    for(zckRangeItem *ptr=info->first; ptr;) {
        prev = ptr;
        if(start > ptr->start) {
            ptr = ptr->next;
            continue;
        } else if(start < ptr->start) {

            if(zck_range_insert_new(ptr->prev, ptr, start, end, info, idx, add_index) == NULL)
                return False;
            if(info->first == ptr) {
                info->first = ptr->prev;
            }
            info->count += 1;
            zck_range_merge_combined(info);
            return True;
        } else { // start == ptr->start
            if(end > ptr->end)
                ptr->end = end;
            info->count += 1;
            zck_range_merge_combined(info);
            return True;
        }
    }
    /* We've only reached here if we should be last item */
    zckRangeItem *new = zck_range_insert_new(prev, NULL, start, end, info, idx, add_index);
    if(new == NULL)
        return False;
    if(info->first == NULL)
        info->first = new;
    info->count += 1;
    zck_range_merge_combined(info);
    return True;
}

int zck_range_calc_segments(zckRange *info, unsigned int max_ranges) {
    if(max_ranges == 0)
        return False;
    info->segments = (info->count + max_ranges - 1) / max_ranges;
    info->max_ranges = max_ranges;
    return True;
}

int zck_range_get_need_dl(zckRange *info, zckCtx *zck_src, zckCtx *zck_tgt) {
    zckIndex *tgt_info = zck_get_index(zck_tgt);
    zckIndex *src_info = zck_get_index(zck_src);
    zckIndexItem *tgt_idx = tgt_info->first;
    zckIndexItem *src_idx = src_info->first;
    while(tgt_idx) {
        int found = False;
        src_idx = src_info->first;

        while(src_idx) {
            if(memcmp(tgt_idx->digest, src_idx->digest, zck_get_chunk_digest_size(zck_tgt)) == 0) {
                found = True;
                break;
            }
            src_idx = src_idx->next;
        }
        if(!found)
            if(!zck_range_add(info, tgt_idx, zck_tgt))
                return False;

        tgt_idx = tgt_idx->next;
    }
    return True;
}

char *zck_range_get_char(zckRangeItem **range, int max_ranges) {
    int buf_size=32768;
    char *output=malloc(buf_size);
    if(!output) {
        zck_log(ZCK_LOG_ERROR, "Unable to allocate %lu bytes\n", buf_size);
        return NULL;
    }

    if(snprintf(output, 20, "Range: bytes=") > 20) {
        free(output);
        zck_log(ZCK_LOG_ERROR, "Error writing range\n");
        return NULL;
    }
    int loc = strlen(output);
    int count=0;
    while(*range) {
        int length = snprintf(output+loc, buf_size-loc, "%lu-%lu,",
                              (*range)->start, (*range)->end);
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
                free(output);
                zck_log(ZCK_LOG_ERROR, "Unable to allocate %lu bytes\n",
                        buf_size);
                return NULL;
            }
            continue;
        }
        loc += length;
        count++;
        *range = (*range)->next;
        if(count == max_ranges)
            break;
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

int zck_range_get_array(zckRange *info, char **ra) {
    if(!info) {
        zck_log(ZCK_LOG_ERROR, "zckRange not allocated\n");
        return False;
    }
    zckRangeItem *tmp = info->first;
    for(int i=0; i<info->segments; i++) {
        ra[i] = zck_range_get_char(&tmp, info->max_ranges);
        if(ra[i] == NULL)
            return False;
    }
    return True;
}

int zck_range_get_max(zckRange *info, char *url) {
    return True;
}
