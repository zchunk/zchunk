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

int zck_write_file(zckCtx *zck) {
    VALIDATE(zck);
    zck_index_finalize(zck);
    zck_log(ZCK_LOG_DEBUG, "Writing header\n");
    if(!zck_write_header(zck))
        return False;
    zck_log(ZCK_LOG_DEBUG, "Writing index\n");
    if(!zck_write_index(zck))
        return False;
    zck_log(ZCK_LOG_DEBUG, "Writing chunks\n");
    if(!zck_chunks_from_temp(zck))
        return False;
    zck_log(ZCK_LOG_DEBUG, "Finished writing file, cleaning up\n");
    zck_index_free(zck);
    zck_comp_close(zck);
    if(zck->temp_fd) {
        close(zck->temp_fd);
        zck->temp_fd = 0;
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

int zck_init_write (zckCtx *zck, int dst_fd) {
    VALIDATE(zck);
    zck_clear(zck);
    memset(zck, 0, sizeof(zckCtx));

    zck->temp_fd = zck_get_tmp_fd();
    if(zck->temp_fd < 0)
        return False;

    /* Set defaults */
#ifdef ZCHUNK_ZSTD
    zck_set_compression_type(zck, ZCK_COMP_ZSTD);
#else
    zck_set_compression_type(zck, ZCK_COMP_NONE);
#endif
    if(!zck_set_full_hash_type(zck, ZCK_HASH_SHA256))
        return False;
    if(!zck_set_chunk_hash_type(zck, ZCK_HASH_SHA1))
        return False;
    zck->fd = dst_fd;

    return True;
}

int zck_import_dict(zckCtx *zck, char *data, size_t size) {
    VALIDATE(zck);
    if(data == NULL || size == 0) {
        zck_log(ZCK_LOG_ERROR,
                "Attempting to initialize an empty compression dictionary\n");
        return False;
    }
    if(!zck_comp_close(zck))
        return False;
    if(!zck_set_comp_parameter(zck, ZCK_COMMON_DICT, data))
        return False;
    if(!zck_set_comp_parameter(zck, ZCK_COMMON_DICT_SIZE, &size))
        return False;
    if(!zck_comp_init(zck))
        return False;
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

int zck_validate_file(zckCtx *zck) {
    VALIDATE(zck);
    char *digest = zck_hash_finalize(&(zck->check_full_hash));
    if(digest == NULL) {
        zck_log(ZCK_LOG_ERROR,
                "Unable to calculate %s checksum for full file\n");
        return False;
    }
    if(memcmp(digest, zck->full_hash_digest, zck->hash_type.digest_size) != 0) {
        free(digest);
        zck_log(ZCK_LOG_ERROR, "Final file failed checksum\n");
        return False;
    }
    free(digest);
    return True;
}

int zck_decompress_to_file(zckCtx *zck, int src_fd, int dst_fd) {
    VALIDATE(zck);
    if(!zck_read_header(zck, src_fd))
        return False;
    if(!zck_hash_init(&(zck->check_full_hash), &(zck->hash_type)))
        return False;

    int start = lseek(src_fd, 0, SEEK_CUR);
    if(start == -1) {
        zck_log(ZCK_LOG_ERROR, "Unable to set starting point source file: %s\n",
                strerror(errno));
        return False;
    }

    zckIndex *index = zck_get_index(zck);
    zckIndexItem *idx = index->first;

    /* Check if zck file is empty */
    for(int count=0; idx; count++) {
        size_t csize = idx->comp_length;
        size_t size = idx->length;
        char *cdata;

        if(csize == 0)
            continue;

        cdata = zmalloc(csize);
        if(cdata == NULL) {
            zck_log(ZCK_LOG_ERROR, "Unable to allocate %lu bytes\n", csize);
            return False;
        }
        if(!zck_seek(src_fd, start + idx->start, SEEK_SET))
            return False;
        if(!zck_read(src_fd, cdata, csize)) {
            free(cdata);
            zck_log(ZCK_LOG_ERROR, "Error reading chunk %i\n", count);
            return False;
        }
        if(!zck_validate_chunk(zck, cdata, csize, idx, count)) {
            free(cdata);
            return False;
        }

        char *data = NULL;
        if(!zck_decompress(zck, cdata, csize, &data, size)) {
            free(cdata);
            zck_log(ZCK_LOG_ERROR, "Unable to decompress chunk %i\n", count);
            return False;
        }
        free(cdata);
        if(count == 0) {
            if(!zck_import_dict(zck, data, size)) {
                free(data);
                return False;
            }
        } else {
            if(!zck_write(dst_fd, data, size)) {
                free(data);
                zck_log(ZCK_LOG_ERROR, "Unable to write chunk %i\n", count);
                return False;
            }
        }
        free(data);
        idx = idx->next;
    }
    if(!zck_validate_file(zck))
        return False;
    return True;
}
