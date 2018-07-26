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

static int init(zckCtx *zck, zckComp *comp) {
    VALIDATE_BOOL(zck);
    ALLOCD_BOOL(comp);

    return True;
}

static ssize_t compress(zckCtx *zck, zckComp *comp, const char *src,
                        const size_t src_size, char **dst, size_t *dst_size,
                        int use_dict) {
    VALIDATE_INT(zck);
    ALLOCD_INT(comp);

    *dst = zmalloc(src_size);
    if(dst == NULL) {
        set_fatal_error(zck, "Unable to allocate %lu bytes", src_size);
        return -1;
    }

    memcpy(*dst, src, src_size);
    *dst_size = src_size;

    return *dst_size;
}

static int end_cchunk(zckCtx *zck, zckComp *comp, char **dst, size_t *dst_size,
                     int use_dict) {
    VALIDATE_BOOL(zck);
    ALLOCD_BOOL(comp);

    *dst = NULL;
    *dst_size = 0;

    return True;
}

static int decompress(zckCtx *zck, zckComp *comp, const int use_dict) {
    VALIDATE_BOOL(zck);
    ALLOCD_BOOL(comp);

    char *src = comp->data;
    size_t src_size = comp->data_size;
    comp->data = NULL;
    comp->data_size = 0;
    if(!comp_add_to_dc(zck, comp, src, src_size)) {
        free(src);
        return False;
    }
    free(src);
    return True;
}

static int end_dchunk(zckCtx *zck, zckComp *comp, const int use_dict,
                      const size_t fd_size) {
    return True;
}

static int close(zckCtx *zck, zckComp *comp) {
    return True;
}

/* Nocomp doesn't support any parameters, so return error if setting a parameter
 * was attempted */
static int set_parameter(zckCtx *zck, zckComp *comp, int option,
                         const void *value) {
    set_error(zck, "Invalid compression parameter for ZCK_COMP_NONE");
    return False;
}

/* No default parameters to set when there's no compression */
static int set_default_parameters(zckCtx *zck, zckComp *comp) {
    return True;
}

int nocomp_setup(zckCtx *zck, zckComp *comp) {
    comp->init = init;
    comp->set_parameter = set_parameter;
    comp->compress = compress;
    comp->end_cchunk = end_cchunk;
    comp->decompress = decompress;
    comp->end_dchunk = end_dchunk;
    comp->close = close;
    comp->type = ZCK_COMP_NONE;
    return set_default_parameters(zck, comp);
}
