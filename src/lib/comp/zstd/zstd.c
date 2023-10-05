/*
 * Copyright 2018, 2021 Jonathan Dieter <jdieter@gmail.com>
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
#include <stdbool.h>
#include <string.h>
#include <zstd.h>
#include <zck.h>

#include "zck_private.h"

static bool init(zckCtx *zck, zckComp *comp) {
    VALIDATE_BOOL(zck);
    ALLOCD_BOOL(zck, comp);

#ifndef OLD_ZSTD
    size_t retval = 0;
#endif

    comp->cctx = ZSTD_createCCtx();
#ifndef OLD_ZSTD
    retval = ZSTD_CCtx_setParameter(comp->cctx, ZSTD_c_compressionLevel, comp->level);
    if(ZSTD_isError(retval)) {
        set_fatal_error(zck, "Unable to set compression level to %i", comp->level);
        return false;
    }
    // This seems to be the only way to make the compression deterministic across
    // architectures with zstd 1.5.0
    retval = ZSTD_CCtx_setParameter(comp->cctx, ZSTD_c_strategy, ZSTD_btopt);
    if(ZSTD_isError(retval)) {
        set_fatal_error(zck, "Unable to set compression strategy");
        return false;
    }
#endif //OLD_ZSTD
    comp->dctx = ZSTD_createDCtx();
    if(comp->dict && comp->dict_size > 0) {
#ifdef OLD_ZSTD
        comp->cdict_ctx = ZSTD_createCDict(comp->dict, comp->dict_size,
                                           comp->level);
        if(comp->cdict_ctx == NULL) {
            set_fatal_error(zck, "Unable to create zstd compression dict context");
            return false;
        }
#else
        retval = ZSTD_CCtx_loadDictionary(comp->cctx, comp->dict, comp->dict_size);
        if(ZSTD_isError(retval)) {
            set_fatal_error(zck, "Unable to add zdict to compression context");
            return false;
        }
#endif //OLD_ZSTD
        comp->ddict_ctx = ZSTD_createDDict(comp->dict, comp->dict_size);
        if(comp->ddict_ctx == NULL) {
            set_fatal_error(zck,
                            "Unable to create zstd decompression dict context");
            return false;
        }
    }
    return true;
}

static bool close(zckCtx *zck, zckComp *comp) {
    ALLOCD_BOOL(zck, zck);
    ALLOCD_BOOL(zck, comp);

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
    return true;
}

/* The zstd compression format doesn't allow streaming compression with a dict
 * unless you statically link to it.  If we have a dict, we do pseudo-streaming
 * compression where we buffer the data until the chunk ends. */
static ssize_t compress(zckCtx *zck, zckComp *comp, const char *src,
                        const size_t src_size, char **dst, size_t *dst_size,
                        bool use_dict) {
    VALIDATE_INT(zck);
    ALLOCD_INT(zck, dst);
    ALLOCD_INT(zck, src);
    ALLOCD_INT(zck, dst_size);
    ALLOCD_INT(zck, comp);

    if((comp->dc_data_size > comp->dc_data_size + src_size) ||
       (src_size > comp->dc_data_size + src_size)) {
        zck_log(ZCK_LOG_ERROR, "Integer overflow when reading decompressed data");
        return false;
    }

    comp->dc_data = zrealloc(comp->dc_data, comp->dc_data_size + src_size);
    if (!comp->dc_data) {
        zck_log(ZCK_LOG_ERROR, "OOM in %s", __func__);
        return -1;
    }

    memcpy(comp->dc_data + comp->dc_data_size, src, src_size);
    *dst = NULL;
    *dst_size = 0;
    return 0;
}

static bool end_cchunk(zckCtx *zck, zckComp *comp, char **dst, size_t *dst_size,
                       bool use_dict) {
    VALIDATE_BOOL(zck);
    ALLOCD_BOOL(zck, dst);
    ALLOCD_BOOL(zck, dst_size);
    ALLOCD_BOOL(zck, comp);

    size_t max_size = ZSTD_compressBound(comp->dc_data_size);
    if(ZSTD_isError(max_size)) {
        set_fatal_error(zck, "zstd compression error: %s",
                        ZSTD_getErrorName(max_size));
        return false;
    }

    *dst = zmalloc(max_size);
    if (!dst) {
        zck_log(ZCK_LOG_ERROR, "OOM in %s", __func__);
        return false;
    }
#ifdef OLD_ZSTD
    /* Currently, compression isn't deterministic when using contexts in
     * zstd 1.3.5, so this works around it */
    if(use_dict && comp->cdict_ctx) {
        if(comp->cctx)
            ZSTD_freeCCtx(comp->cctx);
        comp->cctx = ZSTD_createCCtx();

        *dst_size = ZSTD_compress_usingDict(comp->cctx, *dst, max_size,
                                            comp->dc_data, comp->dc_data_size,
                                            comp->dict, comp->dict_size,
                                            comp->level);
    } else {
        *dst_size = ZSTD_compress(*dst, max_size, comp->dc_data,
                                  comp->dc_data_size, comp->level);
    }
#else
    if(!use_dict && comp->dict_size > 0) {
        size_t retval = ZSTD_CCtx_loadDictionary(comp->cctx, NULL, 0);
        if(ZSTD_isError(retval)) {
            set_fatal_error(zck, "Unable to add zdict to compression context");
            return false;
        }
        *dst_size = ZSTD_compress2(comp->cctx, *dst, max_size, comp->dc_data,
                                   comp->dc_data_size);
        retval = ZSTD_CCtx_loadDictionary(comp->cctx, comp->dict, comp->dict_size);
        if(ZSTD_isError(retval)) {
            set_fatal_error(zck, "Unable to add zdict to compression context");
            return false;
        }
    } else {
        *dst_size = ZSTD_compress2(comp->cctx, *dst, max_size, comp->dc_data,
                                   comp->dc_data_size);
    }
#endif //OLD_ZSTD

    free(comp->dc_data);
    comp->dc_data = NULL;
    comp->dc_data_loc = 0;
    if(ZSTD_isError(*dst_size)) {
        set_fatal_error(zck, "zstd compression error: %s",
                        ZSTD_getErrorName(*dst_size));
        return false;
    }
    return true;
}

static bool decompress(zckCtx *zck, zckComp *comp, const bool use_dict) {
    VALIDATE_BOOL(zck);
    ALLOCD_BOOL(zck, comp);

    return true;
}

static bool end_dchunk(zckCtx *zck, zckComp *comp, const bool use_dict,
                       const size_t fd_size) {
    VALIDATE_BOOL(zck);
    ALLOCD_BOOL(zck, comp);

    char *src = comp->data;
    size_t src_size = comp->data_size;
    comp->data = NULL;
    comp->data_size = 0;

    char *dst = zmalloc(fd_size);
    if (!dst) {
        zck_log(ZCK_LOG_ERROR, "OOM in %s", __func__);
        return false;
    }
    size_t retval = 0;
    zck_log(ZCK_LOG_DEBUG, "Decompressing %llu bytes to %llu bytes",
            (long long unsigned) src_size,
            (long long unsigned) fd_size
    );
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
    return true;
decomp_error_2:
    free(dst);
    free(src);
    return false;
}

static bool set_parameter(zckCtx *zck, zckComp *comp, int option,
                          const void *value) {
    VALIDATE_BOOL(zck);
    ALLOCD_BOOL(zck, comp);

    if(option == ZCK_ZSTD_COMP_LEVEL) {
        if(*(int*)value >= 0 && *(int*)value <= ZSTD_maxCLevel()) {
            comp->level = *(int*)value;
            return true;
        }
    }
    set_error(zck, "Invalid compression parameter for ZCK_COMP_ZSTD");
    return false;
}

static bool set_default_parameters(zckCtx *zck, zckComp *comp) {
    VALIDATE_BOOL(zck);
    ALLOCD_BOOL(zck, comp);

    /* Set default compression level to 9 */
    int level=9;
    return set_parameter(zck, comp, ZCK_ZSTD_COMP_LEVEL, &level);
}

bool zstd_setup(zckCtx *zck, zckComp *comp) {
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
