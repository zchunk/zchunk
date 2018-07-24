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
#include <zck.h>
#include "zck_private.h"

void compint_from_size(char *compint, size_t val, size_t *length) {
    for(unsigned char *i = (unsigned char *)compint; ; i++) {
        i[0] = val % 128;
        val = (val - i[0]) / 128;
        (*length)++;
        if(val == 0) {
            i[0] += 128;
            break;
        }
    }
    return;
}

int compint_to_size(zckCtx *zck, size_t *val, const char *compint,
                    size_t *length, size_t max_length) {
    VALIDATE_BOOL(zck);

    *val = 0;
    size_t old_val = 0;
    const unsigned char *i = (unsigned char *)compint;
    int count = 0;
    int done = False;
    while(True) {
        size_t c = i[0];
        if(c >= 128) {
            c -= 128;
            done = True;
        }
        /* There *must* be a more elegant way of doing c * 128**count */
        for(int f=0; f<count; f++)
            c *= 128;
        *val += c;
        (*length) = (*length) + 1;
        count++;
        if(done)
            break;
        i++;
        /* Make sure we're not overflowing and fail if we do */
        if(count > MAX_COMP_SIZE || count > max_length || *val < old_val) {
            if(count > max_length)
                set_fatal_error(zck, "Read past end of header");
            else
                set_fatal_error(zck, "Number too large");
            *length -= count;
            *val = 0;
            return False;
        }
        old_val = *val;
    }
    return True;
}

int compint_from_int(zckCtx *zck, char *compint, int val, size_t *length) {
    VALIDATE_BOOL(zck);

    if(val < 0) {
        set_error(zck, "Unable to compress negative integers");
        return False;
    }

    compint_from_size(compint, (size_t)val, length);
    return True;
}

int compint_to_int(zckCtx *zck, int *val, const char *compint, size_t *length,
                   size_t max_length) {
    VALIDATE_BOOL(zck);

    size_t new = (size_t)*val;
    if(!compint_to_size(zck, &new, compint, length, max_length))
        return False;
    *val = (int)new;
    if(*val < 0) {
        set_fatal_error(zck, "Overflow error: compressed int is negative");
        return False;
    }
    return True;
}
