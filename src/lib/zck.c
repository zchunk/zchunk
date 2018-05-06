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

int PUBLIC zck_set_ioption(zckCtx *zck, zck_ioption option, ssize_t value) {
    /* Set hash type */
    if(option == ZCK_HASH_FULL_TYPE) {
        return set_full_hash_type(zck, value);
    } else if(option == ZCK_HASH_CHUNK_TYPE) {
        return set_chunk_hash_type(zck, value);

    /* Hash options */
    } else if(option < 100) {
        /* Currently no hash options other than setting hash type, so bail */
        zck_log(ZCK_LOG_ERROR, "Unknown option %lu\n", value);
        return False;

    /* Compression options */
    } else if(option < 2000) {
        return comp_ioption(zck, option, value);

    /* Unknown options */
    } else {
        zck_log(ZCK_LOG_ERROR, "Unknown option %lu\n", value);
        return False;
    }
}

int PUBLIC zck_set_soption(zckCtx *zck, zck_soption option, const void *value) {
    /* Hash options */
    if(option < 100) {
        /* Currently no hash options other than setting hash type, so bail */
        zck_log(ZCK_LOG_ERROR, "Unknown option %lu\n", value);
        return False;

    /* Compression options */
    } else if(option < 2000) {
        return comp_soption(zck, option, value);

    /* Unknown options */
    } else {
        zck_log(ZCK_LOG_ERROR, "Unknown option %lu\n", value);
        return False;
    }
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
    zck_log(ZCK_LOG_DEBUG, "Setting dict size\n");
    if(!zck_set_ioption(zck, ZCK_COMP_DICT_SIZE, size))
        return False;
    zck_log(ZCK_LOG_DEBUG, "Setting dict\n");
    if(!zck_set_soption(zck, ZCK_COMP_DICT, data))
        return False;
    if(!zck_comp_init(zck))
        return False;
    free(data);

    return True;
}

int zck_validate_chunk(zckCtx *zck, char *data, size_t size, zckIndexItem *idx,
                       int chk_num) {
    VALIDATE(zck);
    if(idx == NULL) {
        zck_log(ZCK_LOG_ERROR, "zckIndexItem not initialized\n");
        return False;
    }
    zckHash chunk_hash;

    /* Overall chunk checksum */
    if(!zck_hash_update(&(zck->check_full_hash), data, size)) {
        zck_log(ZCK_LOG_ERROR, "Unable to update full file checksum\n");
        return False;
    }

    /* Check chunk checksum */
    if(!zck_hash_init(&chunk_hash, &(zck->chunk_hash_type))) {
        zck_log(ZCK_LOG_ERROR,
                "Unable to initialize checksum for chunk %i\n", chk_num);
        return False;
    }
    if(!zck_hash_update(&chunk_hash, data, size)) {
        zck_log(ZCK_LOG_ERROR,
                "Unable to update checksum for chunk %i\n", chk_num);
        return False;
    }

    char *digest = zck_hash_finalize(&chunk_hash);
    if(!digest) {
        zck_log(ZCK_LOG_ERROR,
                "Unable to calculate %s checksum for chunk %i\n",
                zck_hash_name_from_type(zck->index.hash_type), chk_num);
        return False;
    }
    if(memcmp(digest, idx->digest, zck->index.digest_size) != 0) {
        free(digest);
        zck_log(ZCK_LOG_ERROR, "Chunk %i failed %s checksum\n",
                chk_num, zck_hash_name_from_type(zck->index.hash_type));
        return False;
    }
    free(digest);
    return True;
}

int zck_validate_current_chunk(zckCtx *zck) {
    VALIDATE(zck);
    char *digest = zck_hash_finalize(&(zck->check_chunk_hash));
    if(digest == NULL) {
        zck_log(ZCK_LOG_ERROR,
                "Unable to calculate %s checksum for chunk\n");
        return False;
    }
    if(zck->comp.data_idx->comp_length == 0)
        memset(digest, 0, zck->comp.data_idx->digest_size);
    zck_log(ZCK_LOG_DEBUG, "Checking chunk checksum\n");
    char *pdigest = zck_get_chunk_digest(zck->comp.data_idx);
    zck_log(ZCK_LOG_DEBUG, "Expected chunk checksum: %s\n", pdigest);
    free(pdigest);
    pdigest = get_digest_string(digest, zck->comp.data_idx->digest_size);
    zck_log(ZCK_LOG_DEBUG, "Calculated chunk checksum: %s\n", pdigest);
    free(pdigest);
    if(memcmp(digest, zck->comp.data_idx->digest,
              zck->chunk_hash_type.digest_size) != 0) {
        free(digest);
        zck_log(ZCK_LOG_ERROR, "Chunk checksum failed!\n");
        return False;
    }
    zck_log(ZCK_LOG_DEBUG, "Chunk checksum valid\n");
    free(digest);
    return True;
}

int zck_validate_file(zckCtx *zck) {
    VALIDATE(zck);
    char *digest = zck_hash_finalize(&(zck->check_full_hash));
    if(digest == NULL) {
        zck_log(ZCK_LOG_ERROR,
                "Unable to calculate %s checksum for full file\n");
        return False;
    }
    zck_log(ZCK_LOG_DEBUG, "Checking data checksum\n");
    zck_log(ZCK_LOG_INFO, "Expected data checksum: ");
    for(int i=0; i<zck->hash_type.digest_size; i++)
        zck_log(ZCK_LOG_INFO, "%02x", (unsigned char)zck->full_hash_digest[i]);
    zck_log(ZCK_LOG_INFO, "\n");
    zck_log(ZCK_LOG_INFO, "Calculated data checksum: ");
    for(int i=0; i<zck->hash_type.digest_size; i++)
        zck_log(ZCK_LOG_INFO, "%02x", (unsigned char)digest[i]);
    zck_log(ZCK_LOG_INFO, "\n");
    if(memcmp(digest, zck->full_hash_digest, zck->hash_type.digest_size) != 0) {
        free(digest);
        zck_log(ZCK_LOG_ERROR, "Data checksum failed!\n");
        return False;
    }
    zck_log(ZCK_LOG_DEBUG, "Data checksum valid\n");
    free(digest);
    return True;
}

int zck_validate_header(zckCtx *zck) {
    VALIDATE(zck);
    char *digest = zck_hash_finalize(&(zck->check_full_hash));
    if(digest == NULL) {
        zck_log(ZCK_LOG_ERROR,
                "Unable to calculate %s checksum for header\n");
        return False;
    }
    zck_log(ZCK_LOG_DEBUG, "Checking header checksum\n");
    zck_log(ZCK_LOG_INFO, "Expected header checksum: ");
    for(int i=0; i<zck->hash_type.digest_size; i++)
        zck_log(ZCK_LOG_INFO, "%02x", (unsigned char)zck->header_digest[i]);
    zck_log(ZCK_LOG_INFO, "\n");
    zck_log(ZCK_LOG_INFO, "Calculated header checksum: ");
    for(int i=0; i<zck->hash_type.digest_size; i++)
        zck_log(ZCK_LOG_INFO, "%02x", (unsigned char)digest[i]);
    zck_log(ZCK_LOG_INFO, "\n");
    if(memcmp(digest, zck->header_digest, zck->hash_type.digest_size) != 0) {
        free(digest);
        zck_log(ZCK_LOG_ERROR, "Header checksum failed!\n");
        return False;
    }
    zck_log(ZCK_LOG_DEBUG, "Header checksum valid\n");
    free(digest);

    if(!zck_hash_init(&(zck->check_full_hash), &(zck->hash_type)))
        return False;

    return True;
}
