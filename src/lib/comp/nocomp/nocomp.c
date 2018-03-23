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

static int zck_nocomp_init(zckComp *comp) {
    return True;
}

static int zck_nocomp_comp(zckComp *comp, const char *src, const size_t src_size,
                        char **dst, size_t *dst_size, int use_dict) {
    *dst = zmalloc(src_size);
    if(dst == NULL) {
        zck_log(ZCK_LOG_ERROR, "Unable to allocate %lu bytes\n", src_size);
        return False;
    }

    memcpy(*dst, src, src_size);
    *dst_size = src_size;

    return True;
}

static int zck_nocomp_decomp(zckComp *comp, const char *src, const size_t src_size,
                        char **dst, size_t dst_size, int use_dict) {
    *dst = zmalloc(src_size);
    if(dst == NULL) {
        zck_log(ZCK_LOG_ERROR, "Unable to allocate %lu bytes\n", src_size);
        return False;
    }

    memcpy(*dst, src, src_size);

    return True;
}

static int zck_nocomp_close(zckComp *comp) {
    return True;
}

/* Nocomp doesn't support any parameters, so return error if setting a parameter
 * was attempted */
static int zck_nocomp_set_parameter(zckComp *comp, int option, void *value) {
    zck_log(ZCK_LOG_ERROR, "Invalid compression parameter for ZCK_COMP_NONE\n");
    return False;
}

/* No default parameters to set when there's no compression */
static int zck_nocomp_set_default_parameters(zckComp *comp) {
    return True;
}

int zck_nocomp_setup(zckComp *comp) {
    comp->init = zck_nocomp_init;
    comp->set_parameter = zck_nocomp_set_parameter;
    comp->compress = zck_nocomp_comp;
    comp->decompress = zck_nocomp_decomp;
    comp->close = zck_nocomp_close;
    comp->type = ZCK_COMP_NONE;
    return zck_nocomp_set_default_parameters(comp);
}
