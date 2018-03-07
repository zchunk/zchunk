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
#include <zck.h>

#include "zck_private.h"
#include "comp/nocomp/nocomp.h"
#ifdef ZCHUNK_ZSTD
#include "comp/zstd/zstd.h"
#endif

#define BLK_SIZE 32768

static char unknown[] = "Unknown(\0\0\0\0\0";

const static char *COMP_NAME[] = {
    "no",
    "zstd"
};

int zck_comp_init(zckCtx *zck) {
    zckComp *comp = &(zck->comp);
    char *dst = NULL;
    size_t dst_size = 0;

    if(zck->comp.started) {
        zck_log(ZCK_LOG_ERROR, "Compression already initialized\n");
        return False;
    }
    if((zck->comp.dict && zck->comp.dict_size == 0) ||
       (zck->comp.dict == NULL && zck->comp.dict_size > 0)) {
        zck_log(ZCK_LOG_ERROR, "Invalid dictionary configuration\n");
        return False;
    }
    if(!zck->comp.init(&(zck->comp)))
        return False;

    if(zck->comp.dict && zck->temp_fd) {
        if(!zck->comp.compress(comp, zck->comp.dict, zck->comp.dict_size, &dst,
                               &dst_size, 0))
            return False;

        if(!zck_write(zck->temp_fd, dst, dst_size)) {
            free(dst);
            return False;
        }
        zck_index_add_chunk(zck, dst, dst_size);
        free(dst);
    }
    zck->comp.dict = NULL;
    zck->comp.dict_size = 0;
    zck->comp.started = True;
    return True;
}

int zck_compress(zckCtx *zck, const char *src, const size_t src_size) {
    zckComp *comp = &(zck->comp);
    char *dst;
    size_t dst_size;

    if(!zck->comp.started) {
        zck_log(ZCK_LOG_ERROR, "Compression hasn't been initialized yet\n");
        return False;
    }

    if(src_size == 0)
        return True;

    if(!zck->comp.compress(comp, src, src_size, &dst, &dst_size, 1))
        return False;

    if(!zck_write(zck->temp_fd, dst, dst_size)) {
        free(dst);
        return False;
    }
    zck_index_add_chunk(zck, dst, dst_size);
    free(dst);
    return True;
}

int zck_decompress(zckCtx *zck, const char *src, const size_t src_size, char **dst, size_t *dst_size) {
    zckComp *comp = &(zck->comp);
    *dst = NULL;
    *dst_size = 0;

    if(!zck->comp.started) {
        zck_log(ZCK_LOG_ERROR, "Compression hasn't been initialized yet\n");
        return False;
    }

    if(src_size == 0)
        return True;

    if(!zck->comp.decompress(comp, src, src_size, dst, dst_size, 1))
        return False;

    return True;
}

int zck_comp_close(zckCtx *zck) {
    if(zck->comp.cctx) {
        zck->comp.started = 0;
        return zck->comp.close(&(zck->comp));
    }
    return True;
}

int zck_set_compression_type(zckCtx *zck, uint8_t type) {
    zckComp *comp = &(zck->comp);

    /* Cannot change compression type after compression has started */
    if(comp->started) {
        zck_log(ZCK_LOG_ERROR,
                "Unable to set compression type after initialization\n");
        return False;
    }

    /* Set all values to 0 before setting compression type */
    memset(comp, 0, sizeof(zckComp));
    if(type == ZCK_COMP_NONE) {
        return zck_nocomp_setup(comp);
#ifdef ZCHUNK_ZSTD
    } else if(type == ZCK_COMP_ZSTD) {
        return zck_zstd_setup(comp);
#endif
    } else {
        zck_log(ZCK_LOG_ERROR, "Unsupported compression type: %s\n",
                zck_comp_name_from_type(type));
        return False;
    }
    zck_log(ZCK_LOG_DEBUG, "Setting compression to %s\n",
            zck_comp_name_from_type(type));
    return True;
}

int zck_set_comp_parameter(zckCtx *zck, int option, void *value) {
    /* Cannot change compression parameters after compression has started */
    if(zck && zck->comp.started) {
        zck_log(ZCK_LOG_ERROR,
                "Unable to set compression parameters after initialization\n");
        return False;
    }
    if(option == ZCK_COMMON_DICT) {
        zck->comp.dict = value;
    }else if(option == ZCK_COMMON_DICT_SIZE) {
        zck->comp.dict_size = *(size_t*)value;
    }else {
        if(zck && zck->comp.set_parameter)
            return zck->comp.set_parameter(&(zck->comp), option, value);

        zck_log(ZCK_LOG_ERROR, "Unsupported compression parameter: %i\n",
                option);
        return False;
    }
    return True;
}

const char *zck_comp_name_from_type(uint8_t comp_type) {
    if(comp_type > 1) {
        snprintf(unknown+8, 4, "%i)", comp_type);
        return unknown;
    }
    return COMP_NAME[comp_type];
}
