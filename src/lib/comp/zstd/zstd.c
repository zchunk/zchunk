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
#include <zstd.h>
#include <zck.h>

#include "zck_private.h"

static int init(zckCtx *zck, zckComp *comp) {
    VALIDATE_BOOL(zck);
    _VALIDATE_BOOL(comp);

    comp->cctx = ZSTD_createCCtx();
    comp->dctx = ZSTD_createDCtx();
    if(comp->dict && comp->dict_size > 0) {
        comp->cdict_ctx = ZSTD_createCDict(comp->dict, comp->dict_size,
                                           comp->level);
        if(comp->cdict_ctx == NULL) {
            set_fatal_error(zck,
                            "Unable to create zstd compression dict context");
            return False;
        }
        comp->ddict_ctx = ZSTD_createDDict(comp->dict, comp->dict_size);
        if(comp->ddict_ctx == NULL) {
            set_fatal_error(zck,
                            "Unable to create zstd decompression dict context");
            return False;
        }
    }
    return True;
}

/* The zstd compression format doesn't allow streaming compression with a dict
 * unless you statically link to it.  If we have a dict, we do pseudo-streaming
 * compression where we buffer the data until the chunk ends. */
static ssize_t compress(zckCtx *zck, zckComp *comp, const char *src,
                        const size_t src_size, char **dst, size_t *dst_size,
                        int use_dict) {
    VALIDATE_TRI(zck);
    _VALIDATE_TRI(comp);

    comp->dc_data = realloc(comp->dc_data, comp->dc_data_size + src_size);
    if(comp->dc_data == NULL) {
        set_fatal_error(zck, "Unable to allocate %lu bytes",
                        comp->dc_data_size + src_size);
        return -1;
    }
    memcpy(comp->dc_data + comp->dc_data_size, src, src_size);
    *dst = NULL;
    *dst_size = 0;
    comp->dc_data_size += src_size;
    return 0;
}

static int end_cchunk(zckCtx *zck, zckComp *comp, char **dst, size_t *dst_size,
                      int use_dict) {
    VALIDATE_BOOL(zck);
    _VALIDATE_BOOL(comp);

    size_t max_size = ZSTD_compressBound(comp->dc_data_size);
    if(ZSTD_isError(max_size)) {
        set_fatal_error(zck, "zstd compression error: %s",
                        ZSTD_getErrorName(max_size));
        return False;
    }

    *dst = zmalloc(max_size);
    if(dst == NULL) {
        set_fatal_error(zck, "Unable to allocate %lu bytes", max_size);
        return False;
    }

    if(use_dict && comp->cdict_ctx) {
        *dst_size = ZSTD_compress_usingCDict(comp->cctx, *dst, max_size,
                                             comp->dc_data, comp->dc_data_size,
                                             comp->cdict_ctx);
    } else {
        *dst_size = ZSTD_compressCCtx(comp->cctx, *dst, max_size, comp->dc_data,
                                      comp->dc_data_size, comp->level);
    }
    free(comp->dc_data);
    comp->dc_data = NULL;
    comp->dc_data_size = 0;
    comp->dc_data_loc = 0;
    if(ZSTD_isError(*dst_size)) {
        set_fatal_error(zck, "zstd compression error: %s",
                        ZSTD_getErrorName(*dst_size));
        return False;
    }
    return True;
}

static int decompress(zckCtx *zck, zckComp *comp, const int use_dict) {
    VALIDATE_BOOL(zck);
    _VALIDATE_BOOL(comp);

    return True;
}

static int end_dchunk(zckCtx *zck, zckComp *comp, const int use_dict,
                      const size_t fd_size) {
    VALIDATE_BOOL(zck);
    _VALIDATE_BOOL(comp);

    char *src = comp->data;
    size_t src_size = comp->data_size;
    comp->data = NULL;
    comp->data_size = 0;

    char *dst = zmalloc(fd_size);
    if(dst == NULL) {
        set_fatal_error(zck, "Unable to allocate %lu bytes", fd_size);
        goto decomp_error_1;
    }

    size_t retval;
    zck_log(ZCK_LOG_DEBUG, "Decompressing %lu bytes to %lu bytes", src_size,
            fd_size);
    if(use_dict && comp->ddict_ctx) {
        zck_log(ZCK_LOG_DEBUG, "Running decompression using dict");
        retval = ZSTD_decompress_usingDDict(comp->dctx, dst, fd_size, src,
                                            src_size, comp->ddict_ctx);
    } else {
        zck_log(ZCK_LOG_DEBUG, "Running decompression");
        retval = ZSTD_decompressDCtx(comp->dctx, dst, fd_size, src, src_size);
    }

    if(ZSTD_isError(retval)) {
        set_fatal_error(zck, "zstd decompression error: %s",
                        ZSTD_getErrorName(retval));
        goto decomp_error_2;
    }
    if(!comp_add_to_dc(zck, comp, dst, fd_size))
        goto decomp_error_2;
    free(dst);
    free(src);
    return True;
decomp_error_2:
    free(dst);
decomp_error_1:
    free(src);
    return False;
}

static int close(zckCtx *zck, zckComp *comp) {
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

static int set_parameter(zckCtx *zck, zckComp *comp, int option,
                         const void *value) {
    if(option == ZCK_ZSTD_COMP_LEVEL) {
        if(*(int*)value >= 0 && *(int*)value <= ZSTD_maxCLevel()) {
            comp->level = *(int*)value;
            return True;
        }
    }
    set_error(zck, "Invalid compression parameter for ZCK_COMP_ZSTD");
    return False;
}

static int set_default_parameters(zckCtx *zck, zckComp *comp) {
    /* Set default compression level to 16 */
    int level=16;
    return set_parameter(zck, comp, ZCK_ZSTD_COMP_LEVEL, &level);
}

int zstd_setup(zckCtx *zck, zckComp *comp) {
    comp->init = init;
    comp->set_parameter = set_parameter;
    comp->compress = compress;
    comp->end_cchunk = end_cchunk;
    comp->decompress = decompress;
    comp->end_dchunk = end_dchunk;
    comp->close = close;
    comp->type = ZCK_COMP_ZSTD;
    return set_default_parameters(zck, comp);
}
