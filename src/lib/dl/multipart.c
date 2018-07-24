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
#include <stdio.h>
#include <regex.h>
#include <zck.h>

#include "zck_private.h"

static char *add_boundary_to_regex(zckCtx *zck, const char *regex,
                                   const char *boundary) {
    VALIDATE_CHAR(zck);

    if(regex == NULL || boundary == NULL)
        return NULL;
    char *regex_b = zmalloc(strlen(regex) + strlen(boundary) + 1);
    if(regex_b == NULL) {
        set_fatal_error(zck,
                        "Unable to reallocate %lu bytes for regular expression",
                        strlen(regex) + strlen(boundary) - 2);
        return NULL;
    }
    if(snprintf(regex_b, strlen(regex) + strlen(boundary), regex,
                boundary) != strlen(regex) + strlen(boundary) - 2) {
        free(regex_b);
        set_error(zck, "Unable to build regular expression");
        return NULL;
    }
    return regex_b;
}

static int create_regex(zckCtx *zck, regex_t *reg, const char *regex) {
    VALIDATE_BOOL(zck);

    if(reg == NULL || regex == NULL) {
        set_error(zck, "Regular expression not initialized");
        return False;
    }
    if(regcomp(reg, regex, REG_ICASE | REG_EXTENDED) != 0) {
        set_error(zck, "Unable to compile regular expression");
        return False;
    }
    return True;
}

static int gen_regex(zckDL *dl) {
    _VALIDATE_BOOL(dl);
    VALIDATE_BOOL(dl->zck);

    char *next = "\r\n--%s\r\ncontent-type:.*\r\n" \
                 "content-range: *bytes *([0-9]+) *- *([0-9]+) */.*\r\n\r";
    char *end =  "\r\n--%s--\r\n\r";
    char *regex_n = add_boundary_to_regex(dl->zck, next, dl->boundary);
    if(regex_n == NULL)
        return False;
    char *regex_e = add_boundary_to_regex(dl->zck, end, dl->boundary);
    if(regex_n == NULL)
        return False;
    dl->dl_regex = zmalloc(sizeof(regex_t));
    if(!create_regex(dl->zck, dl->dl_regex, regex_n)) {
        free(regex_n);
        return False;
    }
    free(regex_n);
    dl->end_regex = zmalloc(sizeof(regex_t));
    if(!create_regex(dl->zck, dl->end_regex, regex_e)) {
        free(regex_e);
        return False;
    }
    free(regex_e);
    return True;
}

void reset_mp(zckMP *mp) {
    if(mp == NULL)
        return;
    if(mp->buffer)
        free(mp->buffer);
    memset(mp, 0, sizeof(zckMP));
}

size_t multipart_extract(zckDL *dl, char *b, size_t l) {
    _VALIDATE_BOOL(dl);
    VALIDATE_BOOL(dl->zck);

    if(dl == NULL || dl->mp == NULL)
        return 0;
    zckMP *mp = dl->mp;
    char *buf = b;
    int alloc_buf = False;

    /* Add new data to stored buffer */
    if(mp->buffer) {
        buf = realloc(mp->buffer, mp->buffer_len + l);
        if(buf == NULL) {
            set_fatal_error(dl->zck, "Unable to reallocate %lu bytes for zckDL",
                            mp->buffer_len + l);
            return 0;
        }
        memcpy(buf + mp->buffer_len, b, l);
        l = mp->buffer_len + l;
        mp->buffer = NULL;  // No need to free, buf holds realloc'd buffer
        mp->buffer_len = 0;
        alloc_buf = True;
    }

    /* If regex hasn't been created, create it */
    if(dl->dl_regex == NULL && !gen_regex(dl))
        return 0;

    char *header_start = buf;
    char *i = buf;
    while(i) {
        char *end = buf + l;
        /* If we're in data writing state, then write data until end of buffer
         * or end of range, whichever comes first */
        if(mp->state != 0) {
            if(i >= end)
                break;
            size_t size = end - i;
            if(mp->length <= size) {
                size = mp->length;
                mp->length = 0;
                mp->state = 0;
                header_start = i + size;
            } else {
                mp->length -= size;
            }
            if(dl_write_range(dl, i, size) != size)
                return 0;
            i += size;
            continue;
        }

        /* If we've reached the end of the buffer without finishing, save it
         * and leave loop */
        if(i >= end) {
            size_t size = buf + l - header_start;
            if(size > 0) {
                mp->buffer = malloc(size);
                memcpy(mp->buffer, header_start, size);
                mp->buffer_len = size;
            }
            break;
        }

        /* Find double newline and replace final \n with \0, so it's a zero-
         * terminated string */
        for(char *j=i; j<end; j++) {
            if(j + 4 >= end) {
                i = j+4;
                break;
            }
            if(memcmp(j, "\r\n\r\n", 4) == 0) {
                j[3] = '\0';
                break;
            }
        }
        if(i >= end)
            continue;

        /* Run regex against download range string */
        regmatch_t match[4] = {{0}};
        if(regexec(dl->dl_regex, i, 3, match, 0) != 0) {
            if(regexec(dl->end_regex, i, 3, match, 0) != 0)
                set_error(dl->zck, "Unable to find multipart download range");
            goto end;
        }

        /* Get range start from regex */
        size_t rstart = 0;
        for(char *c=i + match[1].rm_so; c < i + match[1].rm_eo; c++)
            rstart = rstart*10 + (size_t)(c[0] - 48);

        /* Get range end from regex */
        size_t rend = 0;
        for(char *c=i + match[2].rm_so; c < i + match[2].rm_eo; c++)
            rend = rend*10 + (size_t)(c[0] - 48);

        i += match[0].rm_eo + 1;
        zck_log(ZCK_LOG_DEBUG, "Download range: %lu-%lu", rstart, rend);
        mp->length = rend-rstart+1;
        mp->state = 1;
    }
end:
    if(alloc_buf)
        free(buf);
    return l;
}

size_t multipart_get_boundary(zckDL *dl, char *b, size_t size) {
    _VALIDATE_BOOL(dl);
    VALIDATE_BOOL(dl->zck);

    if(dl == NULL)
        return 0;

    /* Create regex to find boundary */
    if(dl->hdr_regex == NULL) {
        char *regex = "boundary *= *([0-9a-fA-F]+)";
        dl->hdr_regex = zmalloc(sizeof(regex_t));
        if(!create_regex(dl->zck, dl->hdr_regex, regex))
            return 0;
    }

    /* Copy buffer to null-terminated string because POSIX regex requires null-
     * terminated string */
    char *buf = zmalloc(size+1);
    if(buf == NULL) {
        set_fatal_error(dl->zck, "Unable to allocate %lu bytes for header",
                        size+1);
        return 0;
    }
    buf[size] = '\0';
    memcpy(buf, b, size);

    /* Check whether this header contains the boundary and set it if it does */
    regmatch_t match[2] = {{0}};
    if(regexec(dl->hdr_regex, buf, 2, match, 0) == 0) {
        reset_mp(dl->mp);
        char *boundary = zmalloc(match[1].rm_eo - match[1].rm_so + 1);
        memcpy(boundary, buf + match[1].rm_so, match[1].rm_eo - match[1].rm_so);
        zck_log(ZCK_LOG_DEBUG, "Multipart boundary: %s", boundary);
        dl->boundary = boundary;
    }
    if(buf)
        free(buf);
    return size;
}
