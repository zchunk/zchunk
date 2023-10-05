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
#include <stdbool.h>
#include <string.h>
#include <math.h>
#include <zck.h>

#include "zck_private.h"
#include "comp/nocomp/nocomp.h"
#ifdef ZCHUNK_ZSTD
#include "comp/zstd/zstd.h"
#endif

#define BLK_SIZE 32768

static char unknown[] = "Unknown(\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0";

const static char *COMP_NAME[] = {
    "no",
    "Unknown (1)",
    "zstd"
};

static void update_buzhash_bits(zckCtx *zck) {
    int s=1;
    for(int i=0; i<zck->buzhash_match_bits; i++)
        s *= 2;
    s -= 1;
    zck->buzhash_bitmask = s;
}

static bool set_comp_type(zckCtx *zck, ssize_t type) {
    VALIDATE_BOOL(zck);

    zckComp *comp = &(zck->comp);

    /* Cannot change compression type after compression has started */
    if(comp->started) {
        set_error(zck, "Unable to set compression type after initialization");
        return false;
    }

    /* Set all values to 0 before setting compression type */
    char *dc_data = comp->dc_data;
    size_t dc_data_loc = comp->dc_data_loc;
    size_t dc_data_size = comp->dc_data_size;
    memset(comp, 0, sizeof(zckComp));
    comp->dc_data = dc_data;
    comp->dc_data_loc = dc_data_loc;
    comp->dc_data_size = dc_data_size;

    zck_log(ZCK_LOG_DEBUG, "Setting compression to %s",
            zck_comp_name_from_type(type));
    if(type == ZCK_COMP_NONE) {
        return nocomp_setup(zck, comp);
#ifdef ZCHUNK_ZSTD
    } else if(type == ZCK_COMP_ZSTD) {
        return zstd_setup(zck, comp);
#endif
    } else {
        set_error(zck, "Unsupported compression type: %s",
                  zck_comp_name_from_type(type));
        return false;
    }
    return true;
}

static size_t comp_read_from_dc(zckCtx *zck, zckComp *comp, char *dst,
                                size_t dst_size) {
    VALIDATE_INT(zck);
    ALLOCD_INT(zck, comp);
    ALLOCD_INT(zck, dst);

    size_t dl_size = dst_size;
    if(dl_size > comp->dc_data_size - comp->dc_data_loc)
        dl_size = comp->dc_data_size - comp->dc_data_loc;
    memcpy(dst, comp->dc_data+comp->dc_data_loc, dl_size);
    comp->dc_data_loc += dl_size;
    if(dl_size > 0)
        zck_log(ZCK_LOG_DEBUG, "Reading %llu bytes from decompressed buffer",
                (long long unsigned) dl_size);
    return dl_size;
}

static bool comp_add_to_data(zckCtx *zck, zckComp *comp, const char *src,
                             size_t src_size) {
    VALIDATE_BOOL(zck);
    ALLOCD_BOOL(zck, comp);
    ALLOCD_BOOL(zck, src);

    if((comp->data_size > comp->data_size + src_size) ||
       (src_size > comp->data_size + src_size)) {
        zck_log(ZCK_LOG_ERROR, "Integer overflow when reading data");
        return false;
    }

    comp->data = zrealloc(comp->data, comp->data_size + src_size);
    if (!comp->data) {
        zck_log(ZCK_LOG_ERROR, "OOM in %s", __func__);
        return false;
    }
    zck_log(ZCK_LOG_DEBUG, "Adding %llu bytes to compressed buffer",
        (long long unsigned) src_size);
    memcpy(comp->data + comp->data_size, src, src_size);
    comp->data_size += src_size;
    comp->data_loc += src_size;
    return true;
}

static ssize_t comp_end_dchunk(zckCtx *zck, bool use_dict, size_t fd_size) {
    VALIDATE_READ_INT(zck);

    ssize_t rb = zck->comp.end_dchunk(zck, &(zck->comp), use_dict, fd_size);
    if(validate_current_chunk(zck) < 1)
        return -1;
    zck->comp.data_loc = 0;
    zck->comp.data_idx = zck->comp.data_idx->next;
    if(!hash_init(zck, &(zck->check_chunk_hash), &(zck->chunk_hash_type)))
        return -1;
    return rb;
}

static ssize_t comp_write(zckCtx *zck, const char *src, const size_t src_size) {
    VALIDATE_WRITE_INT(zck);

    if(src_size == 0)
        return 0;

    char *dst = NULL;
    size_t dst_size = 0;
    if(zck->comp.compress(zck, &(zck->comp), src, src_size, &dst,
                          &dst_size, 1) < 0)
        return -1;
    zck->comp.dc_data_size += src_size;

    if(zck->no_write == 0 && dst_size > 0 && !write_data(zck, zck->temp_fd, dst, dst_size)) {
        free(dst);
        return -1;
    }
    if(!index_add_to_chunk(zck, dst, dst_size, src_size)) {
        free(dst);
        return -1;
    }
    if(zck->has_uncompressed_source && !hash_update(zck, &(zck->work_index_hash_uncomp), src, src_size))
        return -1;
    free(dst);
    return src_size;
}

bool comp_init(zckCtx *zck) {
    VALIDATE_BOOL(zck);

    zckComp *comp = &(zck->comp);

    if(zck->comp.started) {
        set_error(zck, "Compression already initialized");
        return false;
    }
    if((zck->comp.dict && zck->comp.dict_size == 0) ||
       (zck->comp.dict == NULL && zck->comp.dict_size > 0)) {
        set_error(zck, "Invalid dictionary configuration");
        return false;
    }
    zck_log(ZCK_LOG_DEBUG, "Initializing %s compression",
            zck_comp_name_from_type(comp->type));
    if(!zck->comp.init(zck, &(zck->comp)))
        return false;
    if(zck->mode == ZCK_MODE_WRITE) {
        if(zck->chunk_min_size == 0) {
            zck->chunk_min_size = CHUNK_DEFAULT_MIN;
            zck_log(ZCK_LOG_DEBUG, "Using default minimum chunk size of %llu",
                    (long long unsigned) zck->chunk_min_size);
        }
        if(zck->chunk_max_size == 0) {
            zck->chunk_max_size = CHUNK_DEFAULT_MAX;
            zck_log(ZCK_LOG_DEBUG, "Using default maximum chunk size of %llu",
                    (long long unsigned) zck->chunk_max_size);
        }
        if(zck->manual_chunk == 0) {
            zck_log(ZCK_LOG_DEBUG, "Using buzhash algorithm for chunking");
            zck->buzhash_width = DEFAULT_BUZHASH_WIDTH;
            zck->buzhash_match_bits = DEFAULT_BUZHASH_BITS;
            update_buzhash_bits(zck);
            zck_log(ZCK_LOG_DEBUG, "Setting average chunk size to %llu",
                    (long long unsigned) zck->buzhash_bitmask + 1);
            zck->chunk_auto_min = (zck->buzhash_bitmask + 1) / 4;
            if(zck->chunk_auto_min < zck->chunk_min_size)
                zck->chunk_auto_min = zck->chunk_min_size;
            zck_log(ZCK_LOG_DEBUG, "Setting automatic minimum chunk size to %llu",
                    (long long unsigned) zck->chunk_auto_min);
            zck->chunk_auto_max = (zck->buzhash_bitmask + 1) * 4;
            if(zck->chunk_auto_max > zck->chunk_max_size)
                zck->chunk_auto_max = zck->chunk_max_size;
            zck_log(ZCK_LOG_DEBUG, "Setting automatic maximum chunk size to %llu",
                    (long long unsigned) zck->chunk_auto_max);
        }
    }

    if(zck->temp_fd || zck->no_write) {
        if(zck->comp.dict) {
            char *dst = NULL;
            size_t dst_size = 0;

            if(zck->comp.compress(zck, comp, zck->comp.dict,
                                  zck->comp.dict_size, &dst, &dst_size, 0) < 0)
                return false;
            zck->comp.dc_data_size = zck->comp.dict_size;
            if(zck->no_write == 0 && !write_data(zck, zck->temp_fd, dst, dst_size)) {
                free(dst);
                return false;
            }
            if(!index_add_to_chunk(zck, dst, dst_size,
                                       zck->comp.dict_size)) {
                free(dst);
                return false;
            }
            free(dst);
            dst = NULL;
            dst_size = 0;

            if(!zck->comp.end_cchunk(zck, comp, &dst, &dst_size, 0))
                return false;
            zck->comp.dc_data_size = 0;
            if(zck->no_write == 0 && !write_data(zck, zck->temp_fd, dst, dst_size)) {
                free(dst);
                return false;
            }
            if(!index_add_to_chunk(zck, dst, dst_size, 0) ||
               !index_finish_chunk(zck)) {
                free(dst);
                return false;
            }
            free(dst);
        } else {
            if(!index_finish_chunk(zck))
                return false;
        }
    }
    zck->comp.started = true;
    return true;
}

bool comp_reset(zckCtx *zck) {
    ALLOCD_BOOL(zck, zck);

    zck->comp.started = 0;
    if(zck->comp.dc_data) {
        free(zck->comp.dc_data);
        zck->comp.dc_data = NULL;
        zck->comp.dc_data_loc = 0;
        zck->comp.dc_data_size = 0;
    }
    if(zck->comp.close == NULL)
        return true;
    return zck->comp.close(zck, &(zck->comp));
}

bool comp_reset_comp_data(zckCtx *zck) {
    ALLOCD_BOOL(zck, zck);

    if(zck->comp.data) {
        free(zck->comp.data);
        zck->comp.data = NULL;
        zck->comp.data_size = 0;
        zck->comp.data_loc = 0;
        zck->comp.data_idx = NULL;
    }
    return true;
}

bool comp_close(zckCtx *zck) {
    ALLOCD_BOOL(zck, zck);

    zck_log(ZCK_LOG_DEBUG, "Closing compression");
    comp_reset_comp_data(zck);
    if(zck->comp.dict)
        free(zck->comp.dict);
    zck->comp.dict = NULL;
    zck->comp.dict_size = 0;

    return comp_reset(zck);
}

bool comp_ioption(zckCtx *zck, zck_ioption option, ssize_t value) {
    VALIDATE_BOOL(zck);

    /* Cannot change compression parameters after compression has started */
    if(zck && zck->comp.started) {
        set_error(zck,
                  "Unable to set compression parameters after initialization");
        return false;
    }
    if(option == ZCK_COMP_TYPE) {
        return set_comp_type(zck, value);

    /* Manual chunking */
    } else if(option == ZCK_MANUAL_CHUNK) {
        VALIDATE_WRITE_BOOL(zck);
        if(value != 0) {
            zck_log(ZCK_LOG_DEBUG, "Disabling automatic chunking");
            zck->manual_chunk = 1;
        } else {
            zck_log(ZCK_LOG_DEBUG, "Enabling automatic chunking");
            zck->manual_chunk = 0;
        }
        return true;

    /* Minimum chunk size */
    } else if(option == ZCK_CHUNK_MIN) {
        VALIDATE_WRITE_BOOL(zck);
        if(value < 1) {
            set_error(zck, "Minimum chunk size must be > 0");
            return false;
        }
        if(value > zck->chunk_max_size) {
            set_error(zck, "Minimum chunk size must be <= maximum chunk size");
            return false;
        }
        zck->chunk_min_size = value;
        zck_log(ZCK_LOG_DEBUG, "Setting minimum chunk size to %lli", (long long) value);
        return true;

    /* Maximum chunk size */
    } else if(option == ZCK_CHUNK_MAX) {
        VALIDATE_WRITE_BOOL(zck);
        if(value < 1) {
            set_error(zck, "Maximum chunk size must be > 0");
            return false;
        }
        if(value < zck->chunk_min_size) {
            set_error(zck, "Maximum chunk size must be >= minimum chunk size");
            return false;
        }
        zck->chunk_max_size = value;
        zck_log(ZCK_LOG_DEBUG, "Setting maximum chunk size to %lli", (long long) value);
        return true;

    } else {
        if(zck && zck->comp.set_parameter)
            return zck->comp.set_parameter(zck, &(zck->comp), option, &value);

        set_error(zck, "Unsupported compression parameter: %i",
                  option);
        return false;
    }
    return true;
}

bool comp_soption(zckCtx *zck, zck_soption option, const void *value,
                  size_t length) {
    VALIDATE_BOOL(zck);

    /* Cannot change compression parameters after compression has started */
    if(zck && zck->comp.started) {
        set_error(zck,
                  "Unable to set compression parameters after initialization");
        return false;
    }
    if(option == ZCK_COMP_DICT) {
        zck_log(ZCK_LOG_DEBUG, "Adding dictionary of size %lli", (long long) length);
        zck->comp.dict = (char *)value;
        zck->comp.dict_size = length;
    } else {
        if(zck && zck->comp.set_parameter)
            return zck->comp.set_parameter(zck, &(zck->comp), option, value);

        set_error(zck, "Unsupported compression parameter: %i", option);
        return false;
    }
    return true;
}

bool comp_add_to_dc(zckCtx *zck, zckComp *comp, const char *src,
                    size_t src_size) {
    VALIDATE_BOOL(zck);
    ALLOCD_BOOL(zck, comp);
    ALLOCD_BOOL(zck, src);

    /* Get rid of any already read data and allocate space for new data */
    char *temp = zmalloc(comp->dc_data_size - comp->dc_data_loc + src_size);
    if (!temp) {
        zck_log(ZCK_LOG_ERROR, "OOM in %s", __func__);
        return false;
    }
    if(comp->dc_data_loc != 0)
        zck_log(ZCK_LOG_DEBUG, "Freeing %llu bytes from decompressed buffer",
                (long long unsigned) comp->dc_data_loc);
    zck_log(ZCK_LOG_DEBUG, "Adding %llu bytes to decompressed buffer",
            (long long unsigned) src_size);
    memcpy(temp, comp->dc_data + comp->dc_data_loc,
           comp->dc_data_size - comp->dc_data_loc);
    free(comp->dc_data);
    comp->dc_data_size -= comp->dc_data_loc;
    comp->dc_data_loc = 0;
    comp->dc_data = temp;

    /* Copy new uncompressed data into comp */
    memcpy(comp->dc_data + comp->dc_data_size, src, src_size);
    comp->dc_data_size += src_size;
    return true;
}

ssize_t comp_read(zckCtx *zck, char *dst, size_t dst_size, bool use_dict) {
    VALIDATE_READ_INT(zck);

    if(!zck->comp.started) {
        set_error(zck, "Compression hasn't been initialized yet");
        return -1;
    }

    if(dst_size == 0)
        return 0;

    /* Read dictionary if it exists and hasn't been read yet */
    if(use_dict && zck->index.first->length > 0 && zck->comp.dict == NULL &&
       !import_dict(zck))
        return -1;

    size_t dc = 0;
    char *src = zmalloc(dst_size - dc);
    if (!src) {
        zck_log(ZCK_LOG_ERROR, "OOM in %s", __func__);
        return false;
    }
    bool finished_rd = false;
    bool finished_dc = false;
    zck_log(ZCK_LOG_DEBUG, "Trying to read %llu bytes", (long long unsigned) dst_size);
    while(dc < dst_size) {
        /* Get bytes from decompressed buffer */
        ssize_t rb = comp_read_from_dc(zck, &(zck->comp), dst+dc, dst_size-dc);
        if(rb < 0)
            goto read_error;
        dc += rb;
        if(dc == dst_size)
            break;
        if(rb > 0)
            continue;
        if(finished_dc || zck->comp.data_eof)
            break;

        /* Decompress compressed buffer into decompressed buffer */
        size_t dc_data_size = zck->comp.dc_data_size;
        size_t dc_data_loc = zck->comp.dc_data_loc;
        if(zck->comp.data_size > 0 &&
           !zck->comp.decompress(zck, &(zck->comp), use_dict))
            goto read_error;

        /* Check whether we decompressed more data */
        if(zck->comp.dc_data_size != dc_data_size ||
           zck->comp.dc_data_loc != dc_data_loc)
            continue;

        /* End decompression chunk if we're on a chunk boundary */
        if(zck->comp.data_idx == NULL) {
            zck->comp.data_idx = zck->index.first;
            /* Skip first chunk if it's an empty dict */
            if(zck->comp.data_idx->comp_length == 0)
                zck->comp.data_idx = zck->comp.data_idx->next;
            if(!hash_init(zck, &(zck->check_chunk_hash),
                          &(zck->chunk_hash_type)))
                goto hash_error;
            if(zck->comp.data_loc > 0) {
                if(!zck->has_uncompressed_source) {
                    if(!hash_update(zck, &(zck->check_full_hash), zck->comp.data,
                                    zck->comp.data_loc))
                        goto hash_error;
                }
                if(!hash_update(zck, &(zck->check_chunk_hash), zck->comp.data,
                                zck->comp.data_loc))
                    goto hash_error;
            }
            if(zck->comp.data_idx == NULL) {
                free(src);
                return 0;
            }
        }
        if(zck->comp.data_loc == zck->comp.data_idx->comp_length) {
            if(!comp_end_dchunk(zck, use_dict, zck->comp.data_idx->length)) {
                free(src);
                return -1;
            }
            if(zck->comp.data_idx == NULL)
                zck->comp.data_eof = true;
            continue;
        }

        /* If we finished reading and we've reached here, we're done
         * decompressing */
        if(finished_rd) {
            finished_dc = true;
            continue;
        }

        /* Make sure we don't read beyond current chunk length */
        size_t rs = dst_size;
        if(zck->comp.data_loc + rs > zck->comp.data_idx->comp_length)
            rs = zck->comp.data_idx->comp_length - zck->comp.data_loc;

        /* Decompressed buffer is empty, so read data from file and fill
         * compressed buffer */
        rb = read_data(zck, src, rs);
        if(rb < 0)
            goto read_error;
        if(rb < rs) {
            zck_log(ZCK_LOG_DDEBUG, "EOF");
            finished_rd = true;
        }
        if(zck->check_chunk_hash.ctx == NULL)
            if(!hash_init(zck, &(zck->check_chunk_hash),
                          &(zck->chunk_hash_type)))
                goto hash_error;
        if(!zck->has_uncompressed_source) {
            if(!hash_update(zck, &(zck->check_full_hash), src, rb))
                goto read_error;
        }
        if(!hash_update(zck, &(zck->check_chunk_hash), src, rb) ||
           !comp_add_to_data(zck, &(zck->comp), src, rb))
            goto read_error;
    }
    free(src);
    return dc;
read_error:
    free(src);
    return -1;
hash_error:
    free(src);
    return -2;
}

const char ZCK_PUBLIC_API *zck_comp_name_from_type(int comp_type) {
    if(comp_type > 2) {
        snprintf(unknown+8, 21, "%i)", comp_type);
        return unknown;
    }
    return COMP_NAME[comp_type];
}

ssize_t ZCK_PUBLIC_API zck_write(zckCtx *zck, const char *src, const size_t src_size) {
    VALIDATE_WRITE_INT(zck);

    if(src_size == 0)
        return 0;

    if(!zck->comp.started && !comp_init(zck))
        return -1;

    const char *loc = src;
    size_t loc_size = src_size;
    size_t loc_written = 0;
    uint32_t buzhash_res;

    if(zck->manual_chunk) {
        while(zck->comp.dc_data_size + loc_size > zck->chunk_max_size) {
            loc_written = zck->chunk_max_size - zck->comp.dc_data_size;
            if(comp_write(zck, loc, loc_written) != loc_written)
                return -1;
            loc_size -= loc_written;
            loc += loc_written;
            zck_log(ZCK_LOG_DDEBUG,
                    "Chunk has reached maximum size, forcing a new chunk");
            if(zck_end_chunk(zck) < 0)
                return -1;
        }
        if(comp_write(zck, loc, loc_size) != loc_size)
            return -1;
        else
            return src_size;
    } else {
        for(size_t i=0; i<loc_size; ) {
            if (!buzhash_update(&(zck->buzhash), loc+i, zck->buzhash_width, &buzhash_res)) {
                zck_log(ZCK_LOG_ERROR, "OOM in buzhash_update");
                return -1;
            }

            if((buzhash_res & zck->buzhash_bitmask) == 0 ||
               zck->comp.dc_data_size + i >= zck->chunk_auto_max) {
                if(comp_write(zck, loc, i) != i)
                    return -1;
                loc += i;
                loc_size -= i;
                i = 0;
                if(zck->comp.dc_data_size >= zck->chunk_max_size)
                    zck_log(ZCK_LOG_DDEBUG,
                            "Chunk has reached maximum size, forcing a new "
                            "chunk");
                else
                    zck_log(ZCK_LOG_DDEBUG, "Automatically ending chunk");
                if(zck->comp.dc_data_size < zck->chunk_auto_min) {
                    zck_log(ZCK_LOG_DDEBUG,
                            "Chunk too small, refusing to end chunk");
                    continue;
                }
                if(zck_end_chunk(zck) < 0)
                    return -1;
            } else {
                i++;
            }
        }
        if(loc_size > 0 && comp_write(zck, loc, loc_size) != loc_size)
            return -1;
        return src_size;
    }
}

ssize_t ZCK_PUBLIC_API zck_end_chunk(zckCtx *zck) {
    VALIDATE_WRITE_INT(zck);

    if(!zck->comp.started && !comp_init(zck))
        return -1;

    if(zck->comp.dc_data_size < zck->chunk_min_size) {
        zck_log(ZCK_LOG_DDEBUG, "Chunk too small, refusing to end chunk");
        return zck->comp.dc_data_size;
    }

    buzhash_reset(&(zck->buzhash));
    /* No point in compressing empty data */
    if(zck->comp.dc_data_size == 0)
        return 0;

    size_t data_size = zck->comp.dc_data_size;
    char *dst = NULL;
    size_t dst_size = 0;
    if(!zck->comp.end_cchunk(zck, &(zck->comp), &dst, &dst_size, 1))
        return -1;
    zck->comp.dc_data_size = 0;
    if(zck->no_write == 0 && dst_size > 0 && !write_data(zck, zck->temp_fd, dst, dst_size)) {
        free(dst);
        return -1;
    }
    if(!index_add_to_chunk(zck, dst, dst_size, 0)) {
        free(dst);
        return -1;
    }
    if(!index_finish_chunk(zck)) {
        free(dst);
        return -1;
    }
    zck_log(ZCK_LOG_DDEBUG, "Finished chunk size: %llu", (long long unsigned) data_size);
    free(dst);
    return data_size;
}

ssize_t ZCK_PUBLIC_API zck_read(zckCtx *zck, char *dst, size_t dst_size) {
    VALIDATE_READ_INT(zck);
    ALLOCD_INT(zck, dst);

    return comp_read(zck, dst, dst_size, 1);
}

ssize_t ZCK_PUBLIC_API zck_get_chunk_comp_data(zckChunk *idx, char *dst,
                                       size_t dst_size) {
    zckCtx *zck = NULL;
    if(idx && idx->zck) {
        VALIDATE_INT(idx->zck);
        zck = idx->zck;
    }
    ALLOCD_INT(zck, idx);
    ALLOCD_INT(zck, dst);

    /* Make sure chunk size is valid */
    if(zck_get_chunk_size(idx) < 0)
        return -1;

    /* If the chunk is empty, we're done */
    if(zck_get_chunk_size(idx) == 0)
        return 0;

    /* Make sure requested chunk has a beginning */
    if(zck_get_chunk_start(idx) < 0)
        return -1;

    /* Seek to beginning of requested chunk */
    if(!seek_data(zck, zck_get_chunk_start(idx), SEEK_SET))
        return -1;

    /* Return read chunk */
    return read_data(zck, dst, dst_size);
}

ssize_t ZCK_PUBLIC_API zck_get_chunk_data(zckChunk *idx, char *dst,
                                  size_t dst_size) {
    zckCtx *zck = NULL;
    if(idx && idx->zck) {
        VALIDATE_INT(idx->zck);
        zck = idx->zck;
    }
    ALLOCD_INT(zck, idx);
    ALLOCD_INT(zck, dst);

    /* Make sure chunk size is valid */
    if(zck_get_chunk_size(idx) < 0)
        return -1;
    /* If the chunk is empty, we're done */
    if(zck_get_chunk_size(idx) == 0)
        return 0;
    /* Make sure requested chunk has a beginning */
    if(zck_get_chunk_start(idx) < 0)
        return -1;

    /* Read dictionary if needed */
    zckChunk *dict = zck_get_first_chunk(zck);
    if(dict == NULL)
        return -1;
    if(zck_get_chunk_size(dict) > 0 && zck->comp.dict == NULL) {
        if(zck_get_chunk_start(dict) < 0)
            return -1;
        if(!seek_data(zck, zck_get_chunk_start(dict), SEEK_SET))
            return -1;
        if(!comp_reset(zck))
            return -1;
        if(!comp_init(zck))
            return -1;
        if(!import_dict(zck))
            return -1;
    }

    /* Seek to beginning of requested chunk */
    if(!comp_reset_comp_data(zck))
        return -1;
    if(!comp_reset(zck))
        return -1;
    if(!comp_init(zck))
        return -1;
    if(!seek_data(zck, zck_get_chunk_start(idx), SEEK_SET))
        return -1;
    zck->comp.data_idx = idx;
    return comp_read(zck, dst, dst_size, 1);
}
