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



/* If lead format changes, this needs to be changed */
int PUBLIC zck_get_min_download_size() {
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
    return result;
}

static char *ascii_checksum_to_bin (zckCtx *zck, char *checksum) {
    int cl = strlen(checksum);
    char *raw_checksum = zmalloc(cl/2);
    if(raw_checksum == NULL) {
        set_error(zck, "Unable to allocate %lu bytes", cl/2);
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

int get_tmp_fd(zckCtx *zck) {
    VALIDATE_BOOL(zck);

    int temp_fd;
    char *fname = NULL;
    char template[] = "zcktempXXXXXX";
    char *tmpdir = getenv("TMPDIR");

    if(tmpdir == NULL) {
        tmpdir = "/tmp/";
    }
    fname = zmalloc(strlen(template) + strlen(tmpdir) + 2);
    if(fname == NULL) {
        set_error(zck, "Unable to allocate %lu bytes",
                  strlen(template) + strlen(tmpdir) + 2);
        return -1;
    }
    strncpy(fname, tmpdir, strlen(tmpdir));
    strncpy(fname+strlen(tmpdir), "/", 2);
    strncpy(fname+strlen(tmpdir)+1, template, strlen(template));

    temp_fd = mkstemp(fname);
    if(temp_fd < 0) {
        free(fname);
        set_error(zck, "Unable to create temporary file");
        return -1;
    }
    if(unlink(fname) < 0) {
        free(fname);
        set_error(zck, "Unable to delete temporary file");
        return -1;
    }
    free(fname);
    return temp_fd;
}

int import_dict(zckCtx *zck) {
    VALIDATE_BOOL(zck);

    size_t size = zck->index.first->length;

    /* No dict */
    if(size == 0)
        return True;

    zck_log(ZCK_LOG_DEBUG, "Reading compression dict");
    char *data = zmalloc(size);
    if(data == NULL) {
        set_error(zck, "Unable to allocate %lu bytes", size);
        return False;
    }
    if(comp_read(zck, data, size, 0) != size) {
        set_error(zck, "Error reading compressed dict");
        return False;
    }
    zck_log(ZCK_LOG_DEBUG, "Resetting compression");
    if(!comp_reset(zck))
        return False;
    zck_log(ZCK_LOG_DEBUG, "Setting dict");
    if(!comp_soption(zck, ZCK_COMP_DICT, data, size))
        return False;
    if(!comp_init(zck))
        return False;

    return True;
}

int PUBLIC zck_set_soption(zckCtx *zck, zck_soption option, const char *value,
                           size_t length) {
    VALIDATE_BOOL(zck);
    char *data = zmalloc(length);
    if(data == NULL) {
        set_error(zck, "Unable to allocate %lu bytes", length);
        return False;
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
            return False;
        }
        if(!hash_setup(zck, &chk_type, zck->prep_hash_type)) {
            free(data);
            return False;
        }
        if(chk_type.digest_size*2 != length) {
            free(data);
            set_fatal_error(zck, "Hash digest size mismatch for header "
                                 "validation\n"
                                 "Expected: %lu\nProvided: %lu",
                                 chk_type.digest_size*2, length);
            return False;
        }
        zck_log(ZCK_LOG_DEBUG, "Setting expected hash to (%s)%s",
                zck_hash_name_from_type(zck->prep_hash_type), data);
        zck->prep_digest = ascii_checksum_to_bin(zck, data);
        free(data);

    /* Compression options */
    } else if(option < 2000) {
        VALIDATE_WRITE_BOOL(zck);
        return comp_soption(zck, option, data, length);

    /* Unknown options */
    } else {
        free(data);
        set_error(zck, "Unknown string option %i", option);
        return False;
    }
    return True;
}

int PUBLIC zck_set_ioption(zckCtx *zck, zck_ioption option, ssize_t value) {
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
            set_error(zck, "Header hash type can't be less than zero: %li",
                      value);
            return False;
        }
        /* Make sure that header hash type is set before the header digest,
         * otherwise we run the risk of a buffer overflow */
        if(zck->prep_digest != NULL) {
            set_error(zck, "For validation, you must set the header hash type "
                           "*before* the header digest itself");
            return False;
        }
        zck->prep_hash_type = value;
    } else if(option == ZCK_VAL_HEADER_LENGTH) {
        VALIDATE_READ_BOOL(zck);
        if(value < 0) {
            set_error(zck,
                      "Header size validation can't be less than zero: %li",
                      value);
            return False;
        }
        zck->prep_hdr_size = value;

    /* Hash options */
    } else if(option < 100) {
        /* Currently no hash options other than setting hash type, so bail */
        set_error(zck, "Unknown option %lu", value);
        return False;

    /* Compression options */
    } else if(option < 2000) {
        VALIDATE_WRITE_BOOL(zck);
        return comp_ioption(zck, option, value);

    /* Unknown options */
    } else {
        set_error(zck, "Unknown integer option %i", option);
        return False;
    }
    return True;
}

int PUBLIC zck_close(zckCtx *zck) {
    VALIDATE_BOOL(zck);

    if(zck->mode == ZCK_MODE_WRITE) {
        if(zck_end_chunk(zck) < 0)
            return False;
        if(!header_create(zck))
            return False;
        if(!write_header(zck))
            return False;
        zck_log(ZCK_LOG_DEBUG, "Writing chunks");
        if(!chunks_from_temp(zck))
            return False;
        zck_log(ZCK_LOG_DEBUG, "Finished writing file, cleaning up");
        if(!comp_close(zck))
            return False;
        if(zck->temp_fd) {
            close(zck->temp_fd);
            zck->temp_fd = 0;
        }
    } else {
        if(validate_file(zck, ZCK_LOG_WARNING) < 1)
            return False;
    }

    return True;
}

void PUBLIC zck_free(zckCtx **zck) {
    if(zck == NULL || *zck == NULL)
        return;
    zck_clear(*zck);
    free(*zck);
    *zck = NULL;
}

zckCtx PUBLIC *zck_create() {
    zckCtx *zck = zmalloc(sizeof(zckCtx));
    if(zck == NULL) {
        zck_log(ZCK_LOG_NONE, "Unable to allocate %lu bytes",
                sizeof(zckCtx));
        return NULL;
    }
    zck->prep_hash_type = -1;
    zck->prep_hdr_size = -1;
    return zck;
}

int PUBLIC zck_init_adv_read (zckCtx *zck, int src_fd) {
    VALIDATE_BOOL(zck);

    zck->mode = ZCK_MODE_READ;
    zck->fd = src_fd;
    return True;
}

int PUBLIC zck_init_read (zckCtx *zck, int src_fd) {
    VALIDATE_BOOL(zck);

    if(!zck_init_adv_read(zck, src_fd)) {
        set_fatal_error(zck, "Unable to read file");
        return False;
    }

    if(!zck_read_lead(zck)) {
        set_fatal_error(zck, "Unable to read lead");
        return False;
    }

    if(!zck_read_header(zck)) {
        set_fatal_error(zck, "Unable to read header");
        return False;
    }

    return True;
}

int PUBLIC zck_init_write (zckCtx *zck, int dst_fd) {
    VALIDATE_BOOL(zck);

    zck->mode = ZCK_MODE_WRITE;
    zck->temp_fd = get_tmp_fd(zck);
    if(zck->temp_fd < 0)
        return False;

    /* Set defaults */
#ifdef ZCHUNK_ZSTD
    if(!zck_set_ioption(zck, ZCK_COMP_TYPE, ZCK_COMP_ZSTD))
        return False;
#else
    if(!zck_set_ioption(zck, ZCK_COMP_TYPE, ZCK_COMP_NONE))
        return False;
#endif
    if(!zck_set_ioption(zck, ZCK_HASH_FULL_TYPE, ZCK_HASH_SHA256))
        return False;
    if(!zck_set_ioption(zck, ZCK_HASH_CHUNK_TYPE, ZCK_HASH_SHA512_128))
        return False;
    zck->fd = dst_fd;

    return True;
}

int PUBLIC zck_get_fd(zckCtx *zck) {
    VALIDATE_BOOL(zck);
    return zck->fd;
}

int PUBLIC zck_set_fd(zckCtx *zck, int fd) {
    VALIDATE_BOOL(zck);
    zck->fd = fd;
    return True;
}
