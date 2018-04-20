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
#include <unistd.h>
#include <zck.h>

#include "zck_private.h"

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

int check_flags(zckCtx *zck, char *header, size_t *length) {
    zck->has_streams = header[8] & 0x01;
    if(zck->has_streams)
        zck_log(ZCK_LOG_INFO, "Archive has streams\n");
    if((header[8] & 0xfe) != 0 || header[7] != 0 || header[6] != 0 ||
       header[5] != 0) {
        zck_log(ZCK_LOG_ERROR, "Unknown flags(s) set\n");
        return False;
    }
    *length += 4;
    return True;
}

int add_to_header_string(zckCtx *zck, char *data, size_t length) {
    VALIDATE(zck);

    zck->header_string = realloc(zck->header_string, zck->header_size + length);
    if(zck->header_string == NULL) {
        zck_log(ZCK_LOG_ERROR, "Unable to allocate %lu bytes\n",
                zck->header_size + length);
        return False;
    }
    memcpy(zck->header_string + zck->header_size, data, length);
    zck->header_size += length;
    return True;
}

int add_to_sig_string(zckCtx *zck, char *data, size_t length) {
    VALIDATE(zck);

    zck->sig_string = realloc(zck->sig_string, zck->sig_size + length);
    if(zck->sig_string == NULL) {
        zck_log(ZCK_LOG_ERROR, "Unable to allocate %lu bytes\n",
                zck->sig_size + length);
        return False;
    }
    memcpy(zck->sig_string + zck->sig_size, data, length);
    zck->sig_size += length;
    return True;
}

int zck_read_initial(zckCtx *zck) {
    VALIDATE_READ(zck);

    char *header = NULL;
    size_t length = 0;

    zck_log(ZCK_LOG_DEBUG, "Reading magic, flags and hash type\n");
    if(read_header(zck, &header, 9 + MAX_COMP_SIZE) < 9 + MAX_COMP_SIZE)
        return False;

    if(memcmp(header, "\0ZCK1", 5) != 0) {
        free(header);
        zck_log(ZCK_LOG_ERROR,
                "Invalid header, perhaps this is not a zck file?\n");
        return False;
    }
    length += 5;

    if(!check_flags(zck, header, &length))
        return False;
    int hash_type = 0;
    if(!compint_to_int(&hash_type, header+length, &length))
        return False;
    if(!zck_hash_setup(&(zck->hash_type), hash_type))
        return False;

    /* Return any unused bytes from read_header */
    if(!read_header_unread(zck, 9 + MAX_COMP_SIZE - length))
        return False;

    return add_to_header_string(zck, header, length);
}

int zck_read_header_hash(zckCtx *zck) {
    VALIDATE_READ(zck);

    if(zck->header_string == NULL) {
        zck_log(ZCK_LOG_ERROR,
                "Reading index hash before initial bytes are read\n");
        return False;
    }

    char *header = NULL;

    char *digest = zmalloc(zck->hash_type.digest_size);
    if(digest == NULL) {
        zck_log(ZCK_LOG_ERROR, "Unable to allocate %lu bytes\n",
                zck->hash_type.digest_size);
        return False;
    }
    zck_log(ZCK_LOG_DEBUG, "Reading header hash\n");
    if(read_header(zck, &header, zck->hash_type.digest_size)
                                  < zck->hash_type.digest_size) {
        free(digest);
        return False;
    }
    memcpy(digest, header, zck->hash_type.digest_size);
    zck->header_digest = digest;
    return True;
}

int zck_read_ct_is(zckCtx *zck) {
    VALIDATE_READ(zck);

    if(zck->header_string == NULL) {
        zck_log(ZCK_LOG_ERROR,
                "Reading compression type before hash type is read\n");
        return False;
    }

    char *header = NULL;
    size_t length = 0;

    zck_log(ZCK_LOG_DEBUG, "Reading compression type and index size\n");
    if(read_header(zck, &header, MAX_COMP_SIZE*2) < MAX_COMP_SIZE*2)
        return False;

    int tmp = 0;

    /* Read and initialize compression type */
    if(!compint_to_int(&tmp, header, &length))
        return False;
    if(!zck_set_ioption(zck, ZCK_COMP_TYPE, tmp))
        return False;
    if(!zck_comp_init(zck))
        return False;

    /* Read and initialize index size */
    if(!compint_to_int(&tmp, header + length, &length))
        return False;
    zck->index_size = tmp;

    /* Return any unused bytes from read_header */
    if(!read_header_unread(zck, MAX_COMP_SIZE*2 - length))
        return False;

    return add_to_header_string(zck, header, length);
}

int zck_header_hash(zckCtx *zck) {
    /* Calculate checksum to this point */
    if(!zck_hash_init(&(zck->check_full_hash), &(zck->hash_type)))
        return False;
    if(!zck_hash_update(&(zck->check_full_hash), zck->header_string,
                        zck->header_size))
        return False;
    return True;
}

int zck_read_index(zckCtx *zck) {
    VALIDATE_READ(zck);

    char *header = NULL;
    zck_log(ZCK_LOG_DEBUG, "Reading index\n");
    if(!read_header(zck, &header, zck->index_size))
        return False;

    if(!zck_index_read(zck, header, zck->index_size))
        return False;

    return True;
}

int zck_read_sig(zckCtx *zck) {
    VALIDATE_READ(zck);

    if(zck->header_string == NULL) {
        zck_log(ZCK_LOG_ERROR,
                "Reading signatures before hash type is read\n");
        return False;
    }

    char *header = NULL;
    size_t length = 0;

    /* Get signature size */
    ssize_t rd = read_header(zck, &header, MAX_COMP_SIZE);
    if(rd < 0)
        return False;

    if(!compint_to_int(&(zck->sigs.count), header, &length))
        return False;

    /* We don't actually support signatures yet, so bail if there is one */
    zck_log(ZCK_LOG_DEBUG, "Signature count: %i\n", zck->sigs.count);
    if(zck->sigs.count > 0) {
        zck_log(ZCK_LOG_ERROR, "Signatures aren't supported yet\n");
        return False;
    }

    if(!zck_hash_update(&(zck->check_full_hash), header,
                        length))
        return False;

    /* Return any unused bytes from read_header */
    if(!read_header_unread(zck, rd - length))
        return False;

    zck->data_offset = zck->hdr_buf_size;
    return add_to_sig_string(zck, header, length);
}

int zck_read_header(zckCtx *zck) {
    VALIDATE_READ(zck);

    if(!zck_read_initial(zck))
        return False;
    if(!zck_read_header_hash(zck))
        return False;
    if(!zck_read_ct_is(zck))
        return False;
    if(!zck_header_hash(zck))
        return False;
    if(!zck_read_index(zck))
        return False;
    if(!zck_read_sig(zck))
        return False;
    if(!close_read_header(zck))
        return False;
    if(!zck_validate_header(zck))
        return False;
    if(!zck_import_dict(zck))
        return False;
    return True;
}

int zck_header_create(zckCtx *zck) {
    int header_malloc = 9 + MAX_COMP_SIZE + zck->hash_type.digest_size +
                        MAX_COMP_SIZE*2;

    char *header = zmalloc(header_malloc);
    if(header == NULL) {
        zck_log(ZCK_LOG_ERROR, "Unable to allocate %lu bytes\n", header_malloc);
        return False;
    }
    size_t start = 0;
    size_t length = 0;
    memcpy(header, "\0ZCK1", 5);
    length += 5;
    /* First three bytes of flags are always 0 */
    length += 3;
    /* Final byte for flags */
    if(zck->has_streams)
        header[length] &= 1;
    length += 1;
    compint_from_size(header+length, zck->hash_type.type, &length);
    if(!add_to_header_string(zck, header, length)) {
        free(header);
        return False;
    }
    start = length;

    /* If we have the digest, write it in, otherwise write zeros */
    if(zck->header_digest)
        memcpy(header+length, zck->header_digest, zck->hash_type.digest_size);
    else
        memset(header+length, 0, zck->hash_type.digest_size);
    length += zck->hash_type.digest_size;
    start = length;

    /* Write out compression type and index size */
    if(!compint_from_int(header+length, zck->comp.type, &length)) {
        free(header);
        return False;
    }
    compint_from_size(header+length, zck->index_size, &length);
    if(!add_to_header_string(zck, header+start, length-start)) {
        free(header);
        return False;
    }
    start = length;

    /* Shrink header to actual size */
    header = realloc(header, length);
    if(header == NULL) {
        zck_log(ZCK_LOG_ERROR, "Unable to reallocate %lu bytes\n", length);
        return False;
    }
    if(zck->hdr_buf)
        free(zck->hdr_buf);
    zck->hdr_buf = header;
    zck->hdr_buf_size = length;
    return True;
}

int zck_sig_create(zckCtx *zck) {
    char *header = zmalloc(MAX_COMP_SIZE);
    if(header == NULL) {
        zck_log(ZCK_LOG_ERROR, "Unable to allocate %lu bytes\n", MAX_COMP_SIZE);
        return False;
    }
    size_t length = 0;

    zck_log(ZCK_LOG_DEBUG, "Calculating %i signatures\n", zck->sigs.count);

    /* Write out signature count and signatures */
    if(!compint_from_int(header+length, zck->sigs.count, &length)) {
        free(header);
        return False;
    }
    for(int i=0; i<zck->sigs.count; i++) {
        // TODO: Add signatures
    }
    zck->sig_string = header;
    zck->sig_size = length;
    return True;
}

int zck_write_header(zckCtx *zck) {
    VALIDATE_WRITE(zck);

    if(!write_data(zck->fd, zck->hdr_buf, zck->hdr_buf_size))
        return False;
    return True;
}


int zck_write_sigs(zckCtx *zck) {
    VALIDATE_WRITE(zck);

    if(!write_data(zck->fd, zck->sig_string, zck->sig_size))
        return False;
    return True;
}
