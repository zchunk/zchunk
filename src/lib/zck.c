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
#include <sys/types.h>
#include <unistd.h>
#include <errno.h>
#include <zck.h>

#include "zck_private.h"

#define VALIDATE(f)     if(!f) { \
                            zck_log(ZCK_LOG_ERROR, "zckCtx not initialized\n"); \
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

int PUBLIC zck_set_ioption(zckCtx *zck, zck_ioption option, ssize_t value) {
    /* Set hash type */
    if(option == ZCK_HASH_FULL_TYPE) {
        VALIDATE_WRITE(zck);
        return set_full_hash_type(zck, value);
    } else if(option == ZCK_HASH_CHUNK_TYPE) {
        VALIDATE_WRITE(zck);
        return set_chunk_hash_type(zck, value);

    /* Validation options */
    } else if(option == ZCK_VAL_HEADER_HASH_TYPE) {
        VALIDATE_READ(zck);
        if(value < 0) {
            zck_log(ZCK_LOG_ERROR,
                    "Header hash type can't be less than zero: %li\n",
                    value);
            return False;
        }
        /* Make sure that header hash type is set before the header digest,
         * otherwise we run the risk of a buffer overflow */
        if(zck->prep_digest != NULL) {
            zck_log(ZCK_LOG_ERROR,
                    "For validation, you must set the header hash type "
                    "*before* the header digest itself\n");
            return False;
        }
        zck->prep_hash_type = value;
    } else if(option == ZCK_VAL_HEADER_LENGTH) {
        VALIDATE_READ(zck);
        if(value < 0) {
            zck_log(ZCK_LOG_ERROR,
                    "Header size validation can't be less than zero: %li\n",
                    value);
            return False;
        }
        zck->prep_hdr_size = value;

    /* Hash options */
    } else if(option < 100) {
        /* Currently no hash options other than setting hash type, so bail */
        zck_log(ZCK_LOG_ERROR, "Unknown option %lu\n", value);
        return False;

    /* Compression options */
    } else if(option < 2000) {
        VALIDATE_WRITE(zck);
        return comp_ioption(zck, option, value);

    /* Unknown options */
    } else {
        zck_log(ZCK_LOG_ERROR, "Unknown integer option %i\n", option);
        return False;
    }
    return True;
}

int hex_to_int (char c) {
    if (c >= 97)
        c = c - 32;
    int result = (c / 16 - 3) * 10 + (c % 16);
    if (result > 9)
        result--;
    return result;
}

char *ascii_checksum_to_bin (char *checksum) {
    int cl = strlen(checksum);
    char *raw_checksum = zmalloc(cl/2);
    if(raw_checksum == NULL) {
        zck_log(ZCK_LOG_ERROR, "Unable to allocate %lu bytes\n", cl/2);
        return NULL;
    }
    char *rp = raw_checksum;
    int buf = 0;
    for (int i=0; i<cl; i++) {
        if (i % 2 == 0)
            buf = hex_to_int(checksum[i]);
        else {
            rp[0] = buf*16 + hex_to_int(checksum[i]);
            rp++;
        }
    }
    return raw_checksum;
}

int PUBLIC zck_set_soption(zckCtx *zck, zck_soption option, const char *value,
                           size_t length) {
    char *data = zmalloc(length);
    if(data == NULL) {
        zck_log(ZCK_LOG_ERROR, "Unable to allocate %lu bytes\n", length);
        return False;
    }
    memcpy(data, value, length);

    /* Validation options */
    if(option == ZCK_VAL_HEADER_DIGEST) {
        VALIDATE_READ(zck);
        zckHashType chk_type = {0};
        if(zck->prep_hash_type < 0) {
            free(data);
            zck_log(ZCK_LOG_ERROR,
                    "For validation, you must set the header hash type "
                    "*before* the header digest itself\n");
            return False;
        }
        if(!zck_hash_setup(&chk_type, zck->prep_hash_type)) {
            free(data);
            return False;
        }
        if(chk_type.digest_size*2 != length) {
            free(data);
            zck_log(ZCK_LOG_ERROR, "Hash digest size mismatch for header "
                    "validation\n"
                    "Expected: %lu\nProvided: %lu\n", chk_type.digest_size*2,
                    length);
            return False;
        }
        zck->prep_digest = ascii_checksum_to_bin(data);
        free(data);

    /* Compression options */
    } else if(option < 2000) {
        VALIDATE_WRITE(zck);
        return comp_soption(zck, option, data, length);

    /* Unknown options */
    } else {
        free(data);
        zck_log(ZCK_LOG_ERROR, "Unknown string option %i\n", option);
        return False;
    }
    return True;
}
int PUBLIC zck_close(zckCtx *zck) {
    VALIDATE(zck);

    if(zck->mode == ZCK_MODE_WRITE) {
        if(zck_end_chunk(zck) < 0)
            return False;
        if(!zck_header_create(zck))
            return False;
        if(!zck_write_header(zck))
            return False;
        zck_log(ZCK_LOG_DEBUG, "Writing chunks\n");
        if(!chunks_from_temp(zck))
            return False;
        zck_log(ZCK_LOG_DEBUG, "Finished writing file, cleaning up\n");
        if(!zck_comp_close(zck))
            return False;
        if(zck->temp_fd) {
            close(zck->temp_fd);
            zck->temp_fd = 0;
        }
    } else {
        if(!zck_validate_file(zck))
            return False;
    }

    return True;
}

void zck_clear_work_index(zckCtx *zck) {
    if(zck == NULL)
        return;

    zck_hash_close(&(zck->work_index_hash));
    if(zck->work_index_item)
        zck_index_free_item(&(zck->work_index_item));
}

void zck_clear(zckCtx *zck) {
    if(zck == NULL)
        return;
    zck_index_free(zck);
    if(zck->header)
        free(zck->header);
    zck->header = NULL;
    zck->header_size = 0;
    if(!zck_comp_close(zck))
        zck_log(ZCK_LOG_WARNING, "Unable to close compression\n");
    zck_hash_close(&(zck->full_hash));
    zck_hash_close(&(zck->check_full_hash));
    zck_hash_close(&(zck->check_chunk_hash));
    zck_clear_work_index(zck);
    if(zck->full_hash_digest) {
        free(zck->full_hash_digest);
        zck->full_hash_digest = NULL;
    }
    if(zck->header_digest) {
        free(zck->header_digest);
        zck->header_digest = NULL;
    }
    if(zck->temp_fd) {
        close(zck->temp_fd);
        zck->temp_fd = 0;
    }
}

void PUBLIC zck_free(zckCtx **zck) {
    if(*zck == NULL)
        return;
    zck_clear(*zck);
    free(*zck);
    *zck = NULL;
}

zckCtx PUBLIC *zck_create() {
    zckCtx *zck = zmalloc(sizeof(zckCtx));
    if(zck == NULL) {
        zck_log(ZCK_LOG_ERROR, "Unable to allocate %lu bytes\n",
                sizeof(zckCtx));
        return False;
    }
    zck->prep_hash_type = -1;
    zck->prep_hdr_size = -1;
    return zck;
}

zckCtx PUBLIC *zck_init_adv_read (int src_fd) {
    zckCtx *zck = zck_create();
    if(zck == NULL)
        return NULL;

    zck->mode = ZCK_MODE_READ;
    zck->fd = src_fd;
    return zck;
}

zckCtx PUBLIC *zck_init_read (int src_fd) {
    zckCtx *zck = zck_init_adv_read(src_fd);
    if(zck == NULL)
        return NULL;

    if(!zck_read_header(zck)) {
        zck_free(&zck);
        return NULL;
    }

    return zck;
}

zckCtx PUBLIC *zck_init_write (int dst_fd) {
    zckCtx *zck = zck_create();
    if(zck == NULL)
        return NULL;

    zck->mode = ZCK_MODE_WRITE;
    zck->temp_fd = zck_get_tmp_fd();
    if(zck->temp_fd < 0)
        goto iw_error;

    /* Set defaults */
#ifdef ZCHUNK_ZSTD
    if(!zck_set_ioption(zck, ZCK_COMP_TYPE, ZCK_COMP_ZSTD))
        goto iw_error;
#else
    if(!zck_set_ioption(zck, ZCK_COMP_TYPE, ZCK_COMP_NONE))
        goto iw_error;
#endif
    if(!zck_set_ioption(zck, ZCK_HASH_FULL_TYPE, ZCK_HASH_SHA256))
        goto iw_error;
    if(!zck_set_ioption(zck, ZCK_HASH_CHUNK_TYPE, ZCK_HASH_SHA1))
        goto iw_error;
    zck->fd = dst_fd;

    return zck;
iw_error:
    free(zck);
    return NULL;
}

int PUBLIC zck_get_full_hash_type(zckCtx *zck) {
    if(zck == NULL)
        return -1;
    return zck->hash_type.type;
}

int PUBLIC zck_get_chunk_hash_type(zckCtx *zck) {
    if(zck == NULL)
        return -1;
    return zck->index.hash_type;
}

ssize_t PUBLIC zck_get_index_count(zckCtx *zck) {
    if(zck == NULL)
        return -1;
    return zck->index.count;
}

zckIndex PUBLIC *zck_get_index(zckCtx *zck) {
    if(zck == NULL)
        return NULL;
    return &(zck->index);
}

char *get_digest_string(const char *digest, int size) {
    char *str = zmalloc(size*2+1);

    for(int i=0; i<size; i++)
        snprintf(str + i*2, 3, "%02x", (unsigned char)digest[i]);
    return str;
}

char PUBLIC *zck_get_header_digest(zckCtx *zck) {
    if(zck == NULL)
        return NULL;
    return get_digest_string(zck->header_digest, zck->hash_type.digest_size);
}

char PUBLIC *zck_get_data_digest(zckCtx *zck) {
    if(zck == NULL)
        return NULL;
    return get_digest_string(zck->full_hash_digest, zck->hash_type.digest_size);
}

char PUBLIC *zck_get_chunk_digest(zckIndexItem *item) {
    if(item == NULL)
        return NULL;
    return get_digest_string(item->digest, item->digest_size);
}

ssize_t PUBLIC zck_get_header_length(zckCtx *zck) {
    if(zck == NULL)
        return -1;
    return zck->data_offset;
}

ssize_t PUBLIC zck_get_data_length(zckCtx *zck) {
    zckIndexItem *idx = zck->index.first;
    while(idx->next != NULL)
        idx = idx->next;
    return idx->start + idx->comp_length;
}

int zck_get_tmp_fd() {
    int temp_fd;
    char *fname = NULL;
    char template[] = "zcktempXXXXXX";
    char *tmpdir = getenv("TMPDIR");

    if(tmpdir == NULL) {
        tmpdir = "/tmp/";
    }
    fname = zmalloc(strlen(template) + strlen(tmpdir) + 2);
    if(fname == NULL) {
        zck_log(ZCK_LOG_ERROR, "Unable to allocate %lu bytes\n",
                strlen(template) + strlen(tmpdir) + 2);
        return -1;
    }
    strncpy(fname, tmpdir, strlen(tmpdir));
    strncpy(fname+strlen(tmpdir), "/", 2);
    strncpy(fname+strlen(tmpdir)+1, template, strlen(template));

    temp_fd = mkstemp(fname);
    if(temp_fd < 0) {
        free(fname);
        zck_log(ZCK_LOG_ERROR, "Unable to create temporary file\n");
        return -1;
    }
    if(unlink(fname) < 0) {
        free(fname);
        zck_log(ZCK_LOG_ERROR, "Unable to delete temporary file\n");
        return -1;
    }
    free(fname);
    return temp_fd;
}

int zck_import_dict(zckCtx *zck) {
    VALIDATE(zck);

    size_t size = zck->index.first->length;

    /* No dict */
    if(size == 0)
        return True;

    zck_log(ZCK_LOG_DEBUG, "Reading compression dict\n");
    char *data = zmalloc(size);
    if(data == NULL) {
        zck_log(ZCK_LOG_ERROR, "Unable to allocate %lu bytes\n", size);
        return False;
    }
    if(comp_read(zck, data, size, 0) != size) {
        zck_log(ZCK_LOG_ERROR, "Error reading compressed dict\n");
        return False;
    }
    zck_log(ZCK_LOG_DEBUG, "Resetting compression\n");
    if(!zck_comp_reset(zck))
        return False;
    zck_log(ZCK_LOG_DEBUG, "Setting dict\n");
    if(!comp_soption(zck, ZCK_COMP_DICT, data, size))
        return False;
    if(!zck_comp_init(zck))
        return False;
    free(data);

    return True;
}

int zck_validate_chunk(zckCtx *zck, zckIndexItem *idx,
                       zck_log_type bad_checksum) {
    VALIDATE(zck);
    if(idx == NULL) {
        zck_log(ZCK_LOG_ERROR, "Index not initialized\n");
        return -1;
    }

    char *digest = zck_hash_finalize(&(zck->check_chunk_hash));
    if(digest == NULL) {
        zck_log(ZCK_LOG_ERROR,
                "Unable to calculate %s checksum for chunk\n");
        return -1;
    }
    if(idx->comp_length == 0)
        memset(digest, 0, idx->digest_size);
    zck_log(ZCK_LOG_DEBUG, "Checking chunk checksum\n");
    char *pdigest = zck_get_chunk_digest(idx);
    zck_log(ZCK_LOG_DEBUG, "Expected chunk checksum:   %s\n", pdigest);
    free(pdigest);
    pdigest = get_digest_string(digest, idx->digest_size);
    zck_log(ZCK_LOG_DEBUG, "Calculated chunk checksum: %s\n", pdigest);
    free(pdigest);
    if(memcmp(digest, idx->digest, idx->digest_size) != 0) {
        free(digest);
        zck_log(bad_checksum, "Chunk checksum failed!\n");
        return 0;
    }
    zck_log(ZCK_LOG_DEBUG, "Chunk checksum valid\n");
    free(digest);
    return 1;
}

int zck_validate_current_chunk(zckCtx *zck) {
    VALIDATE(zck);

    return zck_validate_chunk(zck, zck->comp.data_idx, ZCK_LOG_ERROR);
}

int zck_validate_file(zckCtx *zck) {
    VALIDATE(zck);
    char *digest = zck_hash_finalize(&(zck->check_full_hash));
    if(digest == NULL) {
        zck_log(ZCK_LOG_ERROR,
                "Unable to calculate %s checksum for full file\n");
        return -1;
    }
    zck_log(ZCK_LOG_DEBUG, "Checking data checksum\n");
    char *cks = get_digest_string(zck->full_hash_digest,
                                  zck->hash_type.digest_size);
    zck_log(ZCK_LOG_INFO, "Expected data checksum:   %s\n", cks);
    free(cks);
    cks = get_digest_string(digest, zck->hash_type.digest_size);
    zck_log(ZCK_LOG_INFO, "Calculated data checksum: %s\n", cks);
    free(cks);
    if(memcmp(digest, zck->full_hash_digest, zck->hash_type.digest_size) != 0) {
        free(digest);
        zck_log(ZCK_LOG_ERROR, "Data checksum failed!\n");
        return 0;
    }
    zck_log(ZCK_LOG_DEBUG, "Data checksum valid\n");
    free(digest);
    return 1;
}

int zck_validate_header(zckCtx *zck) {
    VALIDATE(zck);
    char *digest = zck_hash_finalize(&(zck->check_full_hash));
    if(digest == NULL) {
        zck_log(ZCK_LOG_ERROR,
                "Unable to calculate %s checksum for header\n");
        return -1;
    }
    zck_log(ZCK_LOG_DEBUG, "Checking header checksum\n");
    char *cks = get_digest_string(zck->header_digest,
                                  zck->hash_type.digest_size);
    zck_log(ZCK_LOG_INFO, "Expected header checksum:   %s\n", cks);
    free(cks);
    cks = get_digest_string(digest, zck->hash_type.digest_size);
    zck_log(ZCK_LOG_INFO, "Calculated header checksum: %s\n", cks);
    free(cks);
    if(memcmp(digest, zck->header_digest, zck->hash_type.digest_size) != 0) {
        free(digest);
        zck_log(ZCK_LOG_ERROR, "Header checksum failed!\n");
        return 0;
    }
    zck_log(ZCK_LOG_DEBUG, "Header checksum valid\n");
    free(digest);

    if(!zck_hash_init(&(zck->check_full_hash), &(zck->hash_type)))
        return -1;

    return 1;
}

int PUBLIC zck_validate_checksums(zckCtx *zck) {
    VALIDATE_READ(zck);
    char buf[BUF_SIZE];

    if(zck->data_offset == 0) {
        zck_log(ZCK_LOG_ERROR, "Header hasn't been read yet\n");
        return -1;
    }

    zck_hash_close(&(zck->check_full_hash));
    if(!zck_hash_init(&(zck->check_full_hash), &(zck->hash_type)))
        return -1;

    if(!seek_data(zck->fd, zck->data_offset, SEEK_SET))
        return -1;

    /* Check each chunk checksum */
    zckIndexItem *idx = zck->index.first;
    int all_good = True;
    while(idx) {
        if(idx == zck->index.first && idx->length == 0) {
            idx->valid = 1;
            continue;
        }

        if(!zck_hash_init(&(zck->check_chunk_hash), &(zck->chunk_hash_type)))
            return -1;

        size_t rlen = 0;
        while(rlen < idx->comp_length) {
            size_t rsize = BUF_SIZE;
            if(BUF_SIZE > idx->comp_length - rlen)
                rsize = idx->comp_length - rlen;
            if(read_data(zck->fd, buf, rsize) != rsize)
                return 0;
            if(!zck_hash_update(&(zck->check_chunk_hash), buf, rsize))
                return -1;
            if(!zck_hash_update(&(zck->check_full_hash), buf, rsize))
                return -1;
            rlen += rsize;
        }
        int valid_chunk = zck_validate_chunk(zck, idx, ZCK_LOG_ERROR);
        if(valid_chunk < 0)
            return -1;
        idx->valid = valid_chunk;
        if(all_good && !valid_chunk)
            all_good = False;
        idx = idx->next;
    }
    if(!all_good)
        return 0;

    /* Check data checksum */
    int valid_file = zck_validate_file(zck);
    if(valid_file < 0)
        return -1;

    /* Go back to beginning of data section */
    if(!seek_data(zck->fd, zck->data_offset, SEEK_SET))
        return -1;

    /* Reinitialize data checksum */
    if(!zck_hash_init(&(zck->check_full_hash), &(zck->hash_type)))
        return -1;

    return valid_file;
}
