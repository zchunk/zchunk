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
#include <zstd.h>
#include <zck.h>

#include "zck_private.h"

static int zck_zstd_init(zckComp *comp) {
    comp->cctx = ZSTD_createCCtx();
    comp->dctx = ZSTD_createDCtx();
    if(comp->dict && comp->dict_size > 0) {
        comp->cdict_ctx = ZSTD_createCDict(comp->dict, comp->dict_size,
                                           comp->level);
        if(comp->cdict_ctx == NULL) {
            zck_log(ZCK_LOG_ERROR,
                    "Unable to create zstd compression dict context\n");
            return False;
        }
        comp->ddict_ctx = ZSTD_createDDict(comp->dict, comp->dict_size);
        if(comp->ddict_ctx == NULL) {
            zck_log(ZCK_LOG_ERROR,
                    "Unable to create zstd decompression dict context\n");
            return False;
        }
    }
    return True;
}

static int zck_zstd_compress(zckComp *comp, const char *src,
                             const size_t src_size, char **dst,
                             size_t *dst_size, int use_dict) {
    size_t max_size = ZSTD_compressBound(src_size);
    if(ZSTD_isError(max_size)) {
        zck_log(ZCK_LOG_ERROR, "zstd compression error: %s\n",
                ZSTD_getErrorName(max_size));
        return False;
    }

    *dst = zmalloc(max_size);
    if(dst == NULL) {
        zck_log(ZCK_LOG_ERROR, "Unable to allocate %lu bytes\n", max_size);
        return False;
    }

    if(use_dict && comp->cdict_ctx) {
        *dst_size = ZSTD_compress_usingCDict(comp->cctx, *dst, max_size, src,
                                             src_size, comp->cdict_ctx);
    } else {
        *dst_size = ZSTD_compressCCtx(comp->cctx, *dst, max_size, src, src_size,
                                      comp->level);
    }
    if(ZSTD_isError(*dst_size)) {
        zck_log(ZCK_LOG_ERROR, "zstd compression error: %s\n",
                ZSTD_getErrorName(*dst_size));
        return False;
    }
    return True;
}

static int zck_zstd_decompress(zckComp *comp, const char *src,
                               const size_t src_size, char **dst,
                               size_t *dst_size, int use_dict) {
    unsigned long long frame_size;
    size_t retval;

    frame_size = ZSTD_getFrameContentSize(src, src_size);
    if(frame_size == ZSTD_CONTENTSIZE_UNKNOWN ||
       frame_size == ZSTD_CONTENTSIZE_ERROR) {
        zck_log(ZCK_LOG_ERROR, "Unknown content size\n");
        return False;
    }
    *dst_size = (size_t)frame_size;
    *dst = zmalloc(*dst_size);
    if(dst == NULL) {
        zck_log(ZCK_LOG_ERROR, "Unable to allocate %lu bytes\n", *dst_size);
        return False;
    }

    if(use_dict && comp->ddict_ctx) {
        retval = ZSTD_decompress_usingDDict(comp->dctx, *dst, *dst_size, src,
                                            src_size, comp->ddict_ctx);
    } else {
        retval = ZSTD_decompressDCtx(comp->dctx, *dst, *dst_size, src,
                                     src_size);
    }
    if(ZSTD_isError(retval)) {
        free(*dst);
        *dst = NULL;
        zck_log(ZCK_LOG_ERROR, "zstd decompression error: %s\n",
                ZSTD_getErrorName(retval));
        return False;
    }
    return True;
}

static int zck_zstd_close(zckComp *comp) {
    if(comp->cdict_ctx) {
        ZSTD_freeCDict(comp->cdict_ctx);
        comp->cdict_ctx = NULL;
    }
    if(comp->ddict_ctx) {
        ZSTD_freeDDict(comp->ddict_ctx);
        comp->ddict_ctx = NULL;
    }
    if(comp->cctx) {
        ZSTD_freeCCtx(comp->cctx);
        comp->cctx = NULL;
    }
    if(comp->dctx) {
        ZSTD_freeDCtx(comp->dctx);
        comp->dctx = NULL;
    }
    return True;
}

static int zck_zstd_set_parameter(zckComp *comp, int option, void *value) {
    if(option == ZCK_ZCK_COMP_LEVEL) {
        if(*(int*)value >= 0 && *(int*)value <= ZSTD_maxCLevel()) {
            comp->level = *(int*)value;
            return True;
        }
    }
    zck_log(ZCK_LOG_ERROR, "Invalid compression parameter for ZCK_COMP_ZSTD\n");
    return False;
}

static int zck_zstd_set_default_parameters(zckComp *comp) {
    /* Set default compression level to 16 */
    int level=16;
    return zck_zstd_set_parameter(comp, ZCK_ZCK_COMP_LEVEL, &level);
}

int zck_zstd_setup(zckComp *comp) {
    comp->init = zck_zstd_init;
    comp->set_parameter = zck_zstd_set_parameter;
    comp->compress = zck_zstd_compress;
    comp->decompress = zck_zstd_decompress;
    comp->close = zck_zstd_close;
    comp->type = ZCK_COMP_ZSTD;
    return zck_zstd_set_default_parameters(comp);
}
