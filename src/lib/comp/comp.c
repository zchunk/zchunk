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
#define VALIDATE(f)     if(!f) { \
                            zck_log(ZCK_LOG_ERROR, \
                                    "zckCtx not initialized\n"); \
                            return False; \
                        }

#define VALIDATE_READ(f)    VALIDATE(f); \
                            if(f->mode != ZCK_MODE_READ) { \
                                zck_log(ZCK_LOG_ERROR, \
                                        "zckCtx not opened for reading\n"); \
                                return False; \
                            }

#define VALIDATE_WRITE(f)   VALIDATE(f); \
                            if(f->mode != ZCK_MODE_WRITE) { \
                                zck_log(ZCK_LOG_ERROR, \
                                        "zckCtx not opened for writing\n"); \
                                return False; \
                            }

#define VALIDATE_SIZE(f)    if(!f) { \
                                zck_log(ZCK_LOG_ERROR, \
                                        "zckComp not initialized\n"); \
                                return -1; \
                            }

#define VALIDATE_READ_SIZE(f)   VALIDATE_SIZE(f); \
                                if(f->mode != ZCK_MODE_READ) { \
                                    zck_log(ZCK_LOG_ERROR, \
                                            "zckComp not opened for reading\n"); \
                                    return -1; \
                                }

#define VALIDATE_WRITE_SIZE(f)  VALIDATE_SIZE(f); \
                                if(f->mode != ZCK_MODE_WRITE) { \
                                    zck_log(ZCK_LOG_ERROR, \
                                            "zckComp not opened for writing\n"); \
                                    return -1; \
                                }

static char unknown[] = "Unknown(\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0";

const static char *COMP_NAME[] = {
    "no",
    "Unknown (1)",
    "zstd"
};

int comp_init(zckCtx *zck) {
    VALIDATE(zck);

    zckComp *comp = &(zck->comp);

    if(zck->comp.started) {
        zck_log(ZCK_LOG_ERROR, "Compression already initialized\n");
        return False;
    }
    if((zck->comp.dict && zck->comp.dict_size == 0) ||
       (zck->comp.dict == NULL && zck->comp.dict_size > 0)) {
        zck_log(ZCK_LOG_ERROR, "Invalid dictionary configuration\n");
        return False;
    }
    zck_log(ZCK_LOG_DEBUG, "Initializing %s compression\n",
            zck_comp_name_from_type(comp->type));
    if(!zck->comp.init(&(zck->comp)))
        return False;
    if(zck->temp_fd) {
        if(zck->comp.dict) {
            char *dst = NULL;
            size_t dst_size = 0;

            if(zck->comp.compress(comp, zck->comp.dict, zck->comp.dict_size,
                                  &dst, &dst_size, 0) < 0)
                return False;
            if(!write_data(zck->temp_fd, dst, dst_size)) {
                free(dst);
                return False;
            }
            if(!index_add_to_chunk(zck, dst, dst_size,
                                       zck->comp.dict_size)) {
                free(dst);
                return False;
            }
            free(dst);
            dst = NULL;
            dst_size = 0;

            if(!zck->comp.end_cchunk(comp, &dst, &dst_size, 0))
                return False;
            if(!write_data(zck->temp_fd, dst, dst_size)) {
                free(dst);
                return False;
            }
            if(!index_add_to_chunk(zck, dst, dst_size, 0) ||
               !index_finish_chunk(zck)) {
                free(dst);
                return False;
            }
            free(dst);
        } else {
            if(!index_finish_chunk(zck))
                return False;
        }
    }
    free(zck->comp.dict);
    zck->comp.dict = NULL;
    zck->comp.dict_size = 0;
    zck->comp.started = True;
    return True;
}

int comp_reset(zckCtx *zck) {
    VALIDATE(zck);

    zck->comp.started = 0;
    if(zck->comp.dc_data) {
        free(zck->comp.dc_data);
        zck->comp.dc_data = NULL;
        zck->comp.dc_data_loc = 0;
        zck->comp.dc_data_size = 0;
    }
    if(zck->comp.close == NULL)
        return True;
    return zck->comp.close(&(zck->comp));
}

int comp_close(zckCtx *zck) {
    VALIDATE(zck);
    zck_log(ZCK_LOG_DEBUG, "Closing compression\n");
    if(zck->comp.data) {
        free(zck->comp.data);
        zck->comp.data = NULL;
        zck->comp.data_size = 0;
        zck->comp.data_loc = 0;
        zck->comp.data_idx = NULL;
    }
    return comp_reset(zck);
}

static int set_comp_type(zckCtx *zck, ssize_t type) {
    VALIDATE(zck);

    zckComp *comp = &(zck->comp);

    /* Cannot change compression type after compression has started */
    if(comp->started) {
        zck_log(ZCK_LOG_ERROR,
                "Unable to set compression type after initialization\n");
        return False;
    }

    /* Set all values to 0 before setting compression type */
    char *dc_data = comp->dc_data;
    size_t dc_data_loc = comp->dc_data_loc;
    size_t dc_data_size = comp->dc_data_size;
    memset(comp, 0, sizeof(zckComp));
    comp->dc_data = dc_data;
    comp->dc_data_loc = dc_data_loc;
    comp->dc_data_size = dc_data_size;

    zck_log(ZCK_LOG_DEBUG, "Setting compression to %s\n",
            zck_comp_name_from_type(type));
    if(type == ZCK_COMP_NONE) {
        return nocomp_setup(comp);
#ifdef ZCHUNK_ZSTD
    } else if(type == ZCK_COMP_ZSTD) {
        return zstd_setup(comp);
#endif
    } else {
        zck_log(ZCK_LOG_ERROR, "Unsupported compression type: %s\n",
                zck_comp_name_from_type(type));
        return False;
    }
    return True;
}

int comp_ioption(zckCtx *zck, zck_ioption option, ssize_t value) {
    VALIDATE(zck);

    /* Cannot change compression parameters after compression has started */
    if(zck && zck->comp.started) {
        zck_log(ZCK_LOG_ERROR,
                "Unable to set compression parameters after initialization\n");
        return False;
    }
    if(option == ZCK_COMP_TYPE) {
        return set_comp_type(zck, value);
    } else {
        if(zck && zck->comp.set_parameter)
            return zck->comp.set_parameter(&(zck->comp), option, &value);

        zck_log(ZCK_LOG_ERROR, "Unsupported compression parameter: %i\n",
                option);
        return False;
    }
    return True;
}

int comp_soption(zckCtx *zck, zck_soption option, const void *value,
                 size_t length) {
    VALIDATE(zck);

    /* Cannot change compression parameters after compression has started */
    if(zck && zck->comp.started) {
        zck_log(ZCK_LOG_ERROR,
                "Unable to set compression parameters after initialization\n");
        return False;
    }
    if(option == ZCK_COMP_DICT) {
        char *dict = zmalloc(length);
        if(dict == NULL) {
            zck_log(ZCK_LOG_ERROR, "Unable to allocate %lu bytes\n",
                    length);
            return False;
        }
        memcpy(dict, value, length);
        zck->comp.dict = dict;
        zck->comp.dict_size = length;
    } else {
        if(zck && zck->comp.set_parameter)
            return zck->comp.set_parameter(&(zck->comp), option, value);

        zck_log(ZCK_LOG_ERROR, "Unsupported compression parameter: %i\n",
                option);
        return False;
    }
    return True;
}

const char PUBLIC *zck_comp_name_from_type(int comp_type) {
    if(comp_type > 2) {
        snprintf(unknown+8, 21, "%i)", comp_type);
        return unknown;
    }
    return COMP_NAME[comp_type];
}

size_t comp_read_from_dc(zckComp *comp, char *dst, size_t dst_size) {
    VALIDATE_SIZE(comp);
    VALIDATE_SIZE(dst);

    size_t dl_size = dst_size;
    if(dl_size > comp->dc_data_size - comp->dc_data_loc)
        dl_size = comp->dc_data_size - comp->dc_data_loc;
    memcpy(dst, comp->dc_data+comp->dc_data_loc, dl_size);
    comp->dc_data_loc += dl_size;
    if(dl_size > 0)
        zck_log(ZCK_LOG_DEBUG, "Reading %lu bytes from decompressed buffer\n",
                dl_size);
    return dl_size;
}

int comp_add_to_dc(zckComp *comp, const char *src, size_t src_size) {
    VALIDATE(comp);
    VALIDATE(src);

    /* Get rid of any already read data and allocate space for new data */
    char *temp = zmalloc(comp->dc_data_size - comp->dc_data_loc + src_size);
    if(temp == NULL) {
        zck_log(ZCK_LOG_ERROR, "Unable to allocate %lu bytes\n",
                comp->dc_data_size - comp->dc_data_loc + src_size);
        return False;
    }
    if(comp->dc_data_loc != 0)
        zck_log(ZCK_LOG_DEBUG, "Freeing %lu bytes from decompressed buffer\n",
                comp->dc_data_loc);
    zck_log(ZCK_LOG_DEBUG, "Adding %lu bytes to decompressed buffer\n",
            src_size);
    memcpy(temp, comp->dc_data + comp->dc_data_loc,
           comp->dc_data_size - comp->dc_data_loc);
    free(comp->dc_data);
    comp->dc_data_size -= comp->dc_data_loc;
    comp->dc_data_loc = 0;
    comp->dc_data = temp;

    /* Copy new uncompressed data into comp */
    memcpy(comp->dc_data + comp->dc_data_size, src, src_size);
    comp->dc_data_size += src_size;
    return True;
}

int comp_add_to_data(zckComp *comp, const char *src, size_t src_size) {
    VALIDATE(comp);
    VALIDATE(src);
    comp->data = realloc(comp->data, comp->data_size + src_size);
    if(comp->data == NULL) {
        zck_log(ZCK_LOG_ERROR, "Unable to reallocate %lu bytes\n",
                comp->data_size + src_size);
        return False;
    }
    zck_log(ZCK_LOG_DEBUG, "Adding %lu bytes to compressed buffer\n",
        src_size);
    memcpy(comp->data + comp->data_size, src, src_size);
    comp->data_size += src_size;
    comp->data_loc += src_size;
    return True;
}

ssize_t PUBLIC zck_write(zckCtx *zck, const char *src, const size_t src_size) {
    VALIDATE_WRITE_SIZE(zck);

    if(!zck->comp.started && !comp_init(zck))
        return -1;

    if(src_size == 0)
        return 0;

    char *dst = NULL;
    size_t dst_size = 0;
    if(zck->comp.compress(&(zck->comp), src, src_size, &dst, &dst_size, 1) < 0)
        return -1;
    if(dst_size > 0 && !write_data(zck->temp_fd, dst, dst_size)) {
        free(dst);
        return -1;
    }
    if(!index_add_to_chunk(zck, dst, dst_size, src_size)) {
        free(dst);
        return -1;
    }
    free(dst);
    return src_size;
}

ssize_t PUBLIC zck_end_chunk(zckCtx *zck) {
    VALIDATE_WRITE_SIZE(zck);

    if(!zck->comp.started && !comp_init(zck))
        return -1;

    /* No point in compressing empty data */
    if(zck->comp.dc_data_size == 0)
        return 0;

    size_t data_size = zck->comp.dc_data_size;
    char *dst = NULL;
    size_t dst_size = 0;
    if(!zck->comp.end_cchunk(&(zck->comp), &dst, &dst_size, 1))
        return -1;
    if(dst_size > 0 && !write_data(zck->temp_fd, dst, dst_size)) {
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
    free(dst);
    return data_size;
}

ssize_t comp_end_dchunk(zckCtx *zck, int use_dict, size_t fd_size) {
    ssize_t rb = zck->comp.end_dchunk(&(zck->comp), use_dict, fd_size);
    if(validate_current_chunk(zck) < 1)
        return -1;
    zck->comp.data_loc = 0;
    zck->comp.data_idx = zck->comp.data_idx->next;
    if(!hash_init(&(zck->check_chunk_hash), &(zck->chunk_hash_type)))
        return -1;
    return rb;
}

ssize_t comp_read(zckCtx *zck, char *dst, size_t dst_size, int use_dict) {
    VALIDATE_READ_SIZE(zck);

    if(!zck->comp.started) {
        zck_log(ZCK_LOG_ERROR, "Compression hasn't been initialized yet\n");
        return -1;
    }

    if(dst_size == 0)
        return 0;

    /* Read dictionary if it exists and hasn't been read yet */
    if(use_dict && !zck->comp.data_eof && zck->comp.data_idx == NULL &&
       zck->index.first->length > 0 && !import_dict(zck))
        return -1;

    size_t dc = 0;
    char *src = zmalloc(dst_size - dc);
    if(src == NULL) {
        zck_log(ZCK_LOG_ERROR, "Unable to allocate %lu bytes\n", dst_size-dc);
        return False;
    }
    int finished_rd = False;
    int finished_dc = False;
    zck_log(ZCK_LOG_DEBUG, "Trying to read %lu bytes\n", dst_size);
    while(dc < dst_size) {
        /* Get bytes from decompressed buffer */
        ssize_t rb = comp_read_from_dc(&(zck->comp), dst+dc, dst_size-dc);
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
        if(!zck->comp.decompress(&(zck->comp), use_dict))
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
            if(!hash_init(&(zck->check_chunk_hash), &(zck->chunk_hash_type)))
                goto hash_error;
            if(zck->comp.data_loc > 0) {
                if(!hash_update(&(zck->check_full_hash), zck->comp.data,
                                    zck->comp.data_loc))
                    goto hash_error;
                if(!hash_update(&(zck->check_chunk_hash), zck->comp.data,
                                    zck->comp.data_loc))
                    goto hash_error;
            }
            if(zck->comp.data_idx == NULL) {
                free(src);
                return 0;
            }
        }
        if(zck->comp.data_loc == zck->comp.data_idx->comp_length) {
            if(!comp_end_dchunk(zck, use_dict, zck->comp.data_idx->length))
                return -1;
            if(zck->comp.data_idx == NULL)
                zck->comp.data_eof = True;
            continue;
        }

        /* If we finished reading and we've reached here, we're done
         * decompressing */
        if(finished_rd) {
            finished_dc = True;
            continue;
        }

        /* Make sure we don't read beyond current chunk length */
        size_t rs = dst_size;
        if(zck->comp.data_loc + rs > zck->comp.data_idx->comp_length)
            rs = zck->comp.data_idx->comp_length - zck->comp.data_loc;

        /* Decompressed buffer is empty, so read data from file and fill
         * compressed buffer */
        rb = read_data(zck->fd, src, rs);
        if(rb < 0)
            goto read_error;
        if(rb < rs) {
            zck_log(ZCK_LOG_DEBUG, "EOF\n");
            finished_rd = True;
        }
        if(!hash_update(&(zck->check_full_hash), src, rb) ||
           !hash_update(&(zck->check_chunk_hash), src, rb) ||
           !comp_add_to_data(&(zck->comp), src, rb))
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

ssize_t PUBLIC zck_read(zckCtx *zck, char *dst, size_t dst_size) {
    VALIDATE_READ_SIZE(zck);

    return comp_read(zck, dst, dst_size, 1);
}
