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
#include <sys/types.h>
#include <unistd.h>
#include <errno.h>
#include <zck.h>

#include "zck_private.h"

#define VALIDATE(f)     if(!f) { \
                            zck_log(ZCK_LOG_ERROR, "zckCtx not initialized\n"); \
                            return False; \
                        }

int zck_close(zckCtx *zck) {
    VALIDATE(zck);

    if(zck->mode == ZCK_MODE_WRITE) {
        zck_index_finalize(zck);
        zck_log(ZCK_LOG_DEBUG, "Writing header\n");
        if(!zck_write_header(zck))
            return False;
        zck_log(ZCK_LOG_DEBUG, "Writing index\n");
        if(!zck_write_index(zck))
            return False;
        zck_log(ZCK_LOG_DEBUG, "Writing chunks\n");
        if(!chunks_from_temp(zck))
            return False;
        zck_log(ZCK_LOG_DEBUG, "Finished writing file, cleaning up\n");
        zck_index_free(zck);
        zck_comp_close(zck);
        if(zck->temp_fd) {
            close(zck->temp_fd);
            zck->temp_fd = 0;
        }
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
    zck_comp_close(zck);
    zck_hash_close(&(zck->full_hash));
    zck_hash_close(&(zck->check_full_hash));
    zck_hash_close(&(zck->check_chunk_hash));
    zck_clear_work_index(zck);
    if(zck->full_hash_digest) {
        free(zck->full_hash_digest);
        zck->full_hash_digest = NULL;
    }
    if(zck->index_digest) {
        free(zck->index_digest);
        zck->index_digest = NULL;
    }
    if(zck->temp_fd) {
        close(zck->temp_fd);
        zck->temp_fd = 0;
    }
}

void zck_free(zckCtx **zck) {
    if(*zck == NULL)
        return;
    zck_clear(*zck);
    free(*zck);
    *zck = NULL;
}

zckCtx *zck_create() {
    zckCtx *zck = zmalloc(sizeof(zckCtx));
    if(zck == NULL) {
        zck_log(ZCK_LOG_ERROR, "Unable to allocate %lu bytes\n",
                sizeof(zckCtx));
        return False;
    }
    return zck;
}

zckCtx *zck_init_adv_read (int src_fd) {
    zckCtx *zck = zck_create();
    if(zck == NULL)
        return NULL;

    zck->mode = ZCK_MODE_READ;
    zck->fd = src_fd;
    return zck;
}

zckCtx *zck_init_read (int src_fd) {
    zckCtx *zck = zck_init_adv_read(src_fd);
    if(zck == NULL)
        return NULL;

    if(!zck_read_header(zck))
        return False;

    return zck;
}

zckCtx *zck_init_write (int dst_fd) {
    zckCtx *zck = zck_create();
    if(zck == NULL)
        return NULL;

    zck->mode = ZCK_MODE_WRITE;
    zck->temp_fd = zck_get_tmp_fd();
    if(zck->temp_fd < 0)
        goto iw_error;

    /* Set defaults */
#ifdef ZCHUNK_ZSTD
    if(!zck_set_compression_type(zck, ZCK_COMP_ZSTD))
        goto iw_error;
#else
    if(!zck_set_compression_type(zck, ZCK_COMP_NONE))
        goto iw_error;
#endif
    if(!zck_set_full_hash_type(zck, ZCK_HASH_SHA256))
        goto iw_error;
    if(!zck_set_chunk_hash_type(zck, ZCK_HASH_SHA1))
        goto iw_error;
    zck->fd = dst_fd;

    return zck;
iw_error:
    free(zck);
    return NULL;
}

int zck_set_full_hash_type(zckCtx *zck, int hash_type) {
    VALIDATE(zck);
    zck_log(ZCK_LOG_INFO, "Setting full hash to %s\n",
            zck_hash_name_from_type(hash_type));
    if(!zck_hash_setup(&(zck->hash_type), hash_type)) {
        zck_log(ZCK_LOG_ERROR, "Unable to set full hash to %s\n",
                zck_hash_name_from_type(hash_type));
        return False;
    }
    zck_hash_close(&(zck->full_hash));
    if(!zck_hash_init(&(zck->full_hash), &(zck->hash_type))) {
        zck_log(ZCK_LOG_ERROR, "Unable initialize full hash\n");
        return False;
    }
    return True;
}

int zck_set_chunk_hash_type(zckCtx *zck, int hash_type) {
    VALIDATE(zck);
    memset(&(zck->chunk_hash_type), 0, sizeof(zckHashType));
    zck_log(ZCK_LOG_INFO, "Setting chunk hash to %s\n",
            zck_hash_name_from_type(hash_type));
    if(!zck_hash_setup(&(zck->chunk_hash_type), hash_type)) {
        zck_log(ZCK_LOG_ERROR, "Unable to set chunk hash to %s\n",
                zck_hash_name_from_type(hash_type));
        return False;
    }
    zck->index.hash_type = zck->chunk_hash_type.type;
    zck->index.digest_size = zck->chunk_hash_type.digest_size;
    return True;
}

int zck_get_full_digest_size(zckCtx *zck) {
    if(zck == NULL)
        return -1;
    return zck->hash_type.digest_size;
}

int zck_get_chunk_digest_size(zckCtx *zck) {
    if(zck == NULL || zck->index.digest_size == 0)
        return -1;
    return zck->index.digest_size;
}

int zck_get_full_hash_type(zckCtx *zck) {
    if(zck == NULL)
        return -1;
    return zck->hash_type.type;
}

int zck_get_chunk_hash_type(zckCtx *zck) {
    if(zck == NULL)
        return -1;
    return zck->index.hash_type;
}

ssize_t zck_get_index_count(zckCtx *zck) {
    if(zck == NULL)
        return -1;
    return zck->index.count;
}

zckIndex *zck_get_index(zckCtx *zck) {
    if(zck == NULL)
        return NULL;
    return &(zck->index);
}

char *zck_get_index_digest(zckCtx *zck) {
    if(zck == NULL)
        return NULL;
    return zck->index_digest;
}

char *zck_get_data_digest(zckCtx *zck) {
    if(zck == NULL)
        return NULL;
    return zck->full_hash_digest;
}

ssize_t zck_get_header_length(zckCtx *zck) {
    if(zck == NULL)
        return -1;
    return zck->header_size + zck->index_size;
}

ssize_t zck_get_data_length(zckCtx *zck) {
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
    strncpy(fname+strlen(tmpdir), "/", 1);
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
    zck_log(ZCK_LOG_DEBUG, "setting dict 1\n");
    if(!zck_set_comp_parameter(zck, ZCK_COMMON_DICT, data))
        return False;
    zck_log(ZCK_LOG_DEBUG, "setting dict 2\n");
    if(!zck_set_comp_parameter(zck, ZCK_COMMON_DICT_SIZE, &size))
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
    zck_log(ZCK_LOG_DEBUG, "Checking chunk checksum\n");
    zck_log(ZCK_LOG_DEBUG, "Expected chunk checksum: ");
    for(int i=0; i<zck->chunk_hash_type.digest_size; i++)
        zck_log(ZCK_LOG_DEBUG, "%02x",
                (unsigned char)zck->comp.data_idx->digest[i]);
    zck_log(ZCK_LOG_DEBUG, "\n");
    zck_log(ZCK_LOG_DEBUG, "Calculated chunk checksum: ");
    for(int i=0; i<zck->chunk_hash_type.digest_size; i++)
        zck_log(ZCK_LOG_DEBUG, "%02x", (unsigned char)digest[i]);
    zck_log(ZCK_LOG_DEBUG, "\n");
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
    zck_log(ZCK_LOG_INFO, "Excpected data checksum: ");
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
