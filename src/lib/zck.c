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

#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <errno.h>
#include <zck.h>
#ifdef _WIN32
#include <fcntl.h>
#endif

#include "zck_private.h"



/* If lead format changes, this needs to be changed */
int ZCK_PUBLIC_API zck_get_min_download_size() {
    /* Magic + hash type + hash digest + header size */
    return 5 + MAX_COMP_SIZE*2 + get_max_hash_size();
}



static void zck_clear(zckCtx *zck) {
    if(zck == NULL)
        return;
    index_free(zck);
    if(zck->header)
        free(zck->header);
    zck->header = NULL;
    zck->header_size = 0;
    if(!comp_close(zck))
        zck_log(ZCK_LOG_WARNING, "Unable to close compression");
    hash_close(&(zck->full_hash));
    hash_close(&(zck->check_full_hash));
    hash_close(&(zck->check_chunk_hash));
    clear_work_index(zck);
    if(zck->full_hash_digest) {
        free(zck->full_hash_digest);
        zck->full_hash_digest = NULL;
    }
    if(zck->header_digest) {
        free(zck->header_digest);
        zck->header_digest = NULL;
    }
    if(zck->prep_digest) {
        free(zck->prep_digest);
        zck->prep_digest = NULL;
    }
    if(zck->temp_fd) {
        close(zck->temp_fd);
        zck->temp_fd = 0;
    }
    if(zck->msg) {
        free(zck->msg);
        zck->msg = NULL;
    }
    zck->error_state = 0;
    zck->fd = -1;
}

static int hex_to_int (char c) {
    if (c >= 97)
        c = c - 32;
    int result = (c / 16 - 3) * 10 + (c % 16);
    if (result > 9)
        result--;
    if (result < 0 || result > 15)
        return -1;
    return result;
}


static char *ascii_checksum_to_bin (zckCtx *zck, char *checksum,
                                    int checksum_length) {
    char *raw_checksum = zmalloc(checksum_length/2);
    char *rp = raw_checksum;
    int buf = 0;
    if (!raw_checksum) {
       zck_log(ZCK_LOG_ERROR, "OOM in %s", __func__);
       return NULL;
    }
    for (int i=0; i<checksum_length; i++) {
        // Get integer value of hex checksum character.  If -1 is returned, then
        // the character wasn't actually hex, so return NULL
        int cksum = hex_to_int(checksum[i]);
        if (cksum < 0) {
            free(raw_checksum);
            return NULL;
        }
        if (i % 2 == 0)
            buf = cksum;
        else {
            rp[0] = buf*16 + cksum;
            rp++;
        }
    }
    return raw_checksum;
}

void *zmalloc(size_t size) {
    void *ret = calloc(1, size);
    return ret;
}

void *zrealloc(void *ptr, size_t size) {
    /* Handle requested size of zero */
    if(size == 0) {
        if(ptr != NULL)
            free(ptr);
        return NULL;
    }

    void *ret = realloc(ptr, size);
    /*
     * Realloc does not touch the original block if fails.
     * Policy is to free memory and returns with error (Null)
     */
    if (!ret && ptr)
        free(ptr);
    return ret;
}

int get_tmp_fd(zckCtx *zck) {
    VALIDATE_BOOL(zck);

    int temp_fd;
    char *fname = NULL;
    char template[] = "zcktempXXXXXX";

    #ifdef _WIN32
    char *tmpdir = getenv("TEMP");
    #else
    char *tmpdir = getenv("TMPDIR");
    #endif

    if(tmpdir == NULL) {
        tmpdir = "/tmp/";
    } else if(strlen(tmpdir) > 1024) {
        set_error(zck, "TMPDIR environmental variable is > 1024 bytes");
        return -1;
    }

    fname = zmalloc(strlen(template) + strlen(tmpdir) + 2);
    if (!fname) {
       zck_log(ZCK_LOG_ERROR, "OOM in %s", __func__);
       return -ENOMEM;
    }
    int i=0;
    for(i=0; i<strlen(tmpdir); i++)
        fname[i] = tmpdir[i];
    int offset = i;
    #ifdef _WIN32
    fname[offset] = '\\';
    #else
    fname[offset] = '/';
    #endif
    offset++;
    for(i=0; i<strlen(template); i++)
        fname[offset + i] = template[i];
    offset += i;
    fname[offset] = '\0';

    typedef int mode_t;
    mode_t old_mode_mask;

    #ifdef _WIN32
    errno_t out = _mktemp_s(
        fname,
        offset + 1
    );
    temp_fd = open(fname, O_CREAT | O_EXCL | O_RDWR | O_BINARY);
    #else
    old_mode_mask = umask (S_IXUSR | S_IRWXG | S_IRWXO);
    temp_fd = mkstemp(fname);
    umask(old_mode_mask);
    #endif
    if(temp_fd < 0) {
        free(fname);
        set_error(zck, "Unable to create temporary file");
        return -1;
    }
#ifndef _WIN32
    // Files with open file handle cannot be removed on Windows
    if(unlink(fname) < 0) {
        free(fname);
        set_error(zck, "Unable to delete temporary file");
        return -1;
    }
#endif
    free(fname);
    return temp_fd;
}

bool import_dict(zckCtx *zck) {
    VALIDATE_BOOL(zck);

    size_t size = zck->index.first->length;

    /* No dict */
    if(size == 0)
        return true;

    zck_log(ZCK_LOG_DEBUG, "Reading compression dict");
    char *data = zmalloc(size);
    if (!data) {
       zck_log(ZCK_LOG_ERROR, "OOM in %s", __func__);
       return false;
    }
    if(comp_read(zck, data, size, 0) != size) {
        set_error(zck, "Error reading compressed dict");
        return false;
    }
    zck_log(ZCK_LOG_DEBUG, "Resetting compression");
    if(!comp_reset(zck))
        return false;
    zck_log(ZCK_LOG_DEBUG, "Setting dict");
    if(!comp_soption(zck, ZCK_COMP_DICT, data, size))
        return false;
    if(!comp_init(zck))
        return false;

    return true;
}

bool ZCK_PUBLIC_API zck_set_soption(zckCtx *zck, zck_soption option, const char *value,
                            size_t length) {
    VALIDATE_BOOL(zck);
    char *data = zmalloc(length);
    if (!data) {
       zck_log(ZCK_LOG_ERROR, "OOM in %s", __func__);
       return false;
    }
    memcpy(data, value, length);

    /* Validation options */
    if(option == ZCK_VAL_HEADER_DIGEST) {
        VALIDATE_READ_BOOL(zck);
        zckHashType chk_type = {0};
        if(zck->prep_hash_type < 0) {
            free(data);
            set_error(zck, "For validation, you must set the header hash type "
                           "*before* the header digest itself");
            return false;
        }
        if(!hash_setup(zck, &chk_type, zck->prep_hash_type)) {
            free(data);
            return false;
        }
        if(chk_type.digest_size*2 != length) {
            free(data);
            set_fatal_error(
                zck,
                "Hash digest size mismatch for header validation\n"
                "Expected: %i\nProvided: %llu",
                chk_type.digest_size*2,
                (long long unsigned) length
            );
            return false;
        }
        zck_log(ZCK_LOG_DEBUG, "Setting expected hash to (%s)%.*s",
                zck_hash_name_from_type(zck->prep_hash_type), length, data);
        zck->prep_digest = ascii_checksum_to_bin(zck, data, length);
        free(data);
        if(zck->prep_digest == NULL) {
            set_fatal_error(zck, "Non-hex character found in supplied digest");
            return false;
        }

    /* Compression options */
    } else if(option < 2000) {
        VALIDATE_WRITE_BOOL(zck);
        return comp_soption(zck, option, data, length);

    /* Unknown options */
    } else {
        free(data);
        set_error(zck, "Unknown string option %i", option);
        return false;
    }
    return true;
}

bool ZCK_PUBLIC_API zck_set_ioption(zckCtx *zck, zck_ioption option, ssize_t value) {
    VALIDATE_BOOL(zck);

    /* Set hash type */
    if(option == ZCK_HASH_FULL_TYPE) {
        VALIDATE_WRITE_BOOL(zck);
        return set_full_hash_type(zck, value);
    } else if(option == ZCK_HASH_CHUNK_TYPE) {
        VALIDATE_WRITE_BOOL(zck);
        return set_chunk_hash_type(zck, value);

    /* Validation options */
    } else if(option == ZCK_VAL_HEADER_HASH_TYPE) {
        VALIDATE_READ_BOOL(zck);
        if(value < 0) {
            set_error(zck, "Header hash type can't be less than zero: %lli",
                      (long long) value);
            return false;
        }
        /* Make sure that header hash type is set before the header digest,
         * otherwise we run the risk of a buffer overflow */
        if(zck->prep_digest != NULL) {
            set_error(zck, "For validation, you must set the header hash type "
                           "*before* the header digest itself");
            return false;
        }
        zck->prep_hash_type = value;
    } else if(option == ZCK_VAL_HEADER_LENGTH) {
        VALIDATE_READ_BOOL(zck);
        if(value < 0) {
            set_error(zck,
                      "Header size validation can't be less than zero: %lli",
                      (long long) value);
            return false;
        }
        zck->prep_hdr_size = value;

    } else if(option == ZCK_UNCOMP_HEADER) {
        zck->has_uncompressed_source = 1;
        /* Uncompressed source requires chunk checksums to be a minimum of SHA-256 */
        if(zck->chunk_hash_type.type == ZCK_HASH_SHA1 ||
           zck->chunk_hash_type.type == ZCK_HASH_SHA512_128) {
            if(!set_chunk_hash_type(zck, ZCK_HASH_SHA256))
                return false;
        }
    } else if(option == ZCK_NO_WRITE) {
        if(value == 0) {
            if(zck->no_write == 1) {
                set_error(zck, "Unable to enable write after it's been disabled");
                return false;
            }
            zck->no_write = 0;
        } else if(value == 1) {
            zck->no_write = 1;
            if(zck->temp_fd) {
                close(zck->temp_fd);
                zck->temp_fd = 0;
            }
        } else {
            set_error(zck, "Unknown value %lli for ZCK_NO_WRITE", (long long) value);
            return false;
        }

    /* Hash options */
    } else if(option < 100) {
        /* Currently no hash options other than setting hash type, so bail */
        set_error(
            zck,
            "Unknown option %llu",
            (long long unsigned) value
        );
        return false;

    /* Compression options */
    } else if(option < 2000) {
        VALIDATE_WRITE_BOOL(zck);
        return comp_ioption(zck, option, value);

    /* Unknown options */
    } else {
        set_error(zck, "Unknown integer option %i", option);
        return false;
    }
    return true;
}

bool ZCK_PUBLIC_API zck_close(zckCtx *zck) {
    VALIDATE_BOOL(zck);

    if(zck->mode == ZCK_MODE_WRITE) {
        if(zck_end_chunk(zck) < 0)
            return false;
        if(!header_create(zck))
            return false;
        if(!write_header(zck))
            return false;
        zck_log(ZCK_LOG_DEBUG, "Writing chunks");
        if(!chunks_from_temp(zck))
            return false;
        zck_log(ZCK_LOG_DEBUG, "Finished writing file, cleaning up");
        if(!comp_close(zck))
            return false;
        if(zck->temp_fd) {
            close(zck->temp_fd);
            zck->temp_fd = 0;
        }
    } else {
        if(validate_file(zck, ZCK_LOG_WARNING) < 1)
            return false;
    }

    return true;
}

void ZCK_PUBLIC_API zck_free(zckCtx **zck) {
    if(zck == NULL || *zck == NULL)
        return;
    zck_clear(*zck);
    free(*zck);
    *zck = NULL;
}

zckCtx ZCK_PUBLIC_API *zck_create() {
    zckCtx *zck = zmalloc(sizeof(zckCtx));
    if (!zck) {
       zck_log(ZCK_LOG_ERROR, "OOM in %s", __func__);
       return false;
    }
    zck_clear_error(NULL);
    zck->prep_hash_type = -1;
    zck->prep_hdr_size = -1;
    return zck;
}

bool ZCK_PUBLIC_API zck_init_adv_read (zckCtx *zck, int src_fd) {
    VALIDATE_BOOL(zck);

    zck->mode = ZCK_MODE_READ;
    zck->fd = src_fd;
    return true;
}

bool ZCK_PUBLIC_API zck_init_read (zckCtx *zck, int src_fd) {
    VALIDATE_BOOL(zck);

    if(!zck_init_adv_read(zck, src_fd)) {
        set_fatal_error(zck, "Unable to read file");
        return false;
    }

    if(!zck_read_lead(zck)) {
        set_fatal_error(zck, "Unable to read lead");
        return false;
    }

    if(!zck_read_header(zck)) {
        set_fatal_error(zck, "Unable to read header");
        return false;
    }

    return true;
}

bool ZCK_PUBLIC_API zck_init_write (zckCtx *zck, int dst_fd) {
    VALIDATE_BOOL(zck);

    zck->mode = ZCK_MODE_WRITE;
    zck->temp_fd = get_tmp_fd(zck);
    if(zck->temp_fd < 0)
        return false;

    /* Set defaults */
#ifdef ZCHUNK_ZSTD
    if(!zck_set_ioption(zck, ZCK_COMP_TYPE, ZCK_COMP_ZSTD))
        return false;
#else
    if(!zck_set_ioption(zck, ZCK_COMP_TYPE, ZCK_COMP_NONE))
        return false;
#endif
    if(!zck_set_ioption(zck, ZCK_HASH_FULL_TYPE, ZCK_HASH_SHA256))
        return false;
    if(!zck_set_ioption(zck, ZCK_HASH_CHUNK_TYPE, ZCK_HASH_SHA512_128))
        return false;
    zck->fd = dst_fd;

    return true;
}

zckCtx ZCK_PUBLIC_API *zck_get_ctx(zckChunk *chunk) {
    return chunk->zck;
}

int ZCK_PUBLIC_API zck_get_fd(zckCtx *zck) {
    VALIDATE_BOOL(zck);
    return zck->fd;
}

bool ZCK_PUBLIC_API zck_set_fd(zckCtx *zck, int fd) {
    VALIDATE_BOOL(zck);
    zck->fd = fd;
    return true;
}
