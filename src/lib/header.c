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

#define MAX_HEADER_IN_MEM 10*1024*1024

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

int check_flags(zckCtx *zck, char *header, size_t *length, size_t max_length) {
    if(max_length < 4) {
        zck_log(ZCK_LOG_ERROR, "Read past end of header\n");
        return False;
    }
    zck->has_streams = header[3] & 0x01;
    if(zck->has_streams)
        zck_log(ZCK_LOG_INFO, "Archive has streams\n");
    if((header[3] & 0xfe) != 0 || header[2] != 0 || header[1] != 0 ||
       header[0] != 0) {
        zck_log(ZCK_LOG_ERROR, "Unknown flags(s) set\n");
        return False;
    }
    *length += 4;
    return True;
}

int read_lead_1(zckCtx *zck) {
    VALIDATE_READ(zck);

    int lead = 5 + 2*MAX_COMP_SIZE;

    char *header = zmalloc(lead);
    if(header == NULL) {
        zck_log(ZCK_LOG_ERROR, "Unable to allocate %lu bytes\n", lead);
        return False;
    }
    size_t length = 0;

    if(read_data(zck->fd, header, lead) < lead)
        return False;

    if(memcmp(header, "\0ZCK1", 5) != 0) {
        free(header);
        zck_log(ZCK_LOG_ERROR,
                "Invalid lead, perhaps this is not a zck file?\n");
        return False;
    }
    length += 5;

    /* Read hash type for header and full digest and initialize check hash */
    int hash_type = 0;
    if(!compint_to_int(&hash_type, header+length, &length, lead))
        return False;
    if(zck->prep_hash_type > -1 && zck->prep_hash_type != hash_type) {
        zck_log(ZCK_LOG_ERROR,
                "Hash type (%i) doesn't match requested hash type "
                "(%i)\n", hash_type, zck->prep_hash_type);
        return False;
    }
    if(!zck_hash_setup(&(zck->hash_type), hash_type))
        return False;
    zck_log(ZCK_LOG_DEBUG, "Setting header and full digest hash type to %s\n",
            zck_hash_name_from_type(hash_type));

    /* Read header size */
    size_t header_length = 0;
    if(!compint_to_size(&header_length, header+length, &length, lead))
        return False;
    if(zck->prep_hdr_size > -1 && (size_t)zck->prep_hdr_size != header_length) {
        zck_log(ZCK_LOG_ERROR,
                "Header length (%lu) doesn't match requested header length "
                "(%lu)\n", header_length, zck->prep_hdr_size);
        return False;
    }
    zck->header_length = header_length;

    zck->header = header;
    zck->header_size = lead;
    zck->lead_string = header;
    zck->lead_size = length;
    zck->hdr_digest_loc = length;
    return True;
}

int read_lead_2(zckCtx *zck) {
    VALIDATE_READ(zck);

    if(zck->lead_string == NULL || zck->lead_size == 0) {
        zck_log(ZCK_LOG_ERROR,
                "Reading lead step 2 before lead step 1 is read\n");
        return False;
    }

    char *header = zck->lead_string;
    size_t length = zck->lead_size;
    size_t lead = zck->header_size;

    /* Read header digest */
    zck_log(ZCK_LOG_DEBUG, "Reading header digest\n");
    header = realloc(header, length + zck->hash_type.digest_size);
    if(header == NULL) {
        zck_log(ZCK_LOG_ERROR, "Unable to re-allocate %lu bytes\n",
                length + zck->hash_type.digest_size);
        return False;
    }
    size_t to_read = 0;
    if(lead < length + zck->hash_type.digest_size)
        to_read = length + zck->hash_type.digest_size - lead;
    if(read_data(zck->fd, header + lead, to_read) < to_read)
        return False;
    lead += to_read;

    if(zck->prep_digest &&
       memcmp(zck->prep_digest, header + length, zck->hash_type.digest_size) != 0) {
        zck_log(ZCK_LOG_ERROR,
                "Header digest doesn't match requested header digest\n");
        return False;
    }
    zck->header_digest = zmalloc(zck->hash_type.digest_size);
    if(zck->header_digest == NULL) {
        zck_log(ZCK_LOG_ERROR, "Unable to allocate %lu bytes\n",
                zck->hash_type.digest_size);
        return False;
    }
    memcpy(zck->header_digest, header + length, zck->hash_type.digest_size);
    length += zck->hash_type.digest_size;

    /* Store pre-header */
    zck->header = header;
    zck->header_size = lead;
    zck->lead_string = header;
    zck->lead_size = length;
    zck_log(ZCK_LOG_DEBUG, "Parsed lead: %lu bytes\n", length);
    return True;
}

int validate_header(zckCtx *zck) {
    if(zck->header_length > MAX_HEADER_IN_MEM) {

    }

    /* Allocate header and store any extra bytes at beginning of header */
    zck->header = realloc(zck->header, zck->lead_size + zck->header_length);
    if(zck->header == NULL) {
        zck_log(ZCK_LOG_ERROR, "Unable to reallocate %lu bytes\n",
                zck->lead_size + zck->header_length);
        return False;
    }
    zck->lead_string = zck->header;
    char *header = zck->header + zck->lead_size;
    size_t loaded = 0;

    if(zck->header_length < zck->header_size - zck->lead_size) {
        zck_log(ZCK_LOG_ERROR, "Header size is too small for actual data\n");
        return False;
    }
    if(zck->lead_size < zck->header_size)
        loaded = zck->header_size - zck->lead_size;

    /* Read header from file */
    zck_log(ZCK_LOG_DEBUG, "Reading the rest of the header: %lu bytes\n",
            zck->header_length);
    if(loaded < zck->header_length) {
        if(!read_data(zck->fd, header + loaded, zck->header_length - loaded))
            return False;
        zck->header_size = zck->lead_size + zck->header_length;
    }

    if(!zck_hash_init(&(zck->check_full_hash), &(zck->hash_type)))
        return False;
    if(!zck_hash_update(&(zck->check_full_hash), zck->header,
                        zck->hdr_digest_loc))
        return False;
    if(!zck_hash_update(&(zck->check_full_hash), header, zck->header_length))
        return False;
    if(!zck_validate_header(zck))
        return False;
    return True;
}

int read_preface(zckCtx *zck) {
    VALIDATE_READ(zck);

    if(zck->header_digest == NULL) {
        zck_log(ZCK_LOG_ERROR,
                "Reading preface before lead is read\n");
        return False;
    }

    char *header = zck->header + zck->lead_size;
    size_t length = 0;
    size_t max_length = zck->header_length;

    /* Read data digest */
    zck_log(ZCK_LOG_DEBUG, "Reading data digest\n");
    if(length + zck->hash_type.digest_size > max_length) {
        zck_log(ZCK_LOG_ERROR, "Read past end of header\n");
        return False;
    }
    zck->full_hash_digest = zmalloc(zck->hash_type.digest_size);
    if(!zck->full_hash_digest) {
        zck_log(ZCK_LOG_ERROR, "Unable to allocate %lu bytes\n",
                zck->hash_type.digest_size);
        return False;
    }
    memcpy(zck->full_hash_digest, header+length, zck->hash_type.digest_size);
    length += zck->hash_type.digest_size;

    /* Read flags */
    if(!check_flags(zck, header+length, &length, max_length-length))
        return False;

    /* Setup for reading compression type */
    zck_log(ZCK_LOG_DEBUG, "Reading compression type and index size\n");
    int tmp = 0;

    /* Read and initialize compression type */
    if(!compint_to_int(&tmp, header+length, &length, max_length))
        return False;
    if(!comp_ioption(zck, ZCK_COMP_TYPE, tmp))
        return False;
    if(!zck_comp_init(zck))
        return False;

    /* Read and initialize index size */
    if(!compint_to_int(&tmp, header+length, &length, max_length))
        return False;
    zck->index_size = tmp;

    zck->preface_string = header;
    zck->preface_size = length;
    return True;
}

int read_index(zckCtx *zck) {
    VALIDATE_READ(zck);

    if(zck->preface_string == NULL) {
        zck_log(ZCK_LOG_ERROR,
                "Reading index before preface is read\n");
        return False;
    }

    char *header = NULL;
    zck_log(ZCK_LOG_DEBUG, "Reading index\n");
    if(zck->lead_size + zck->preface_size + zck->index_size >
       zck->header_size) {
        zck_log(ZCK_LOG_ERROR, "Read past end of header\n");
        return False;
    }
    header = zck->header + zck->lead_size + zck->preface_size;
    int max_length = zck->header_size - (zck->lead_size + zck->preface_size);
    if(!zck_index_read(zck, header, zck->index_size, max_length))
        return False;

    zck->index_string = header;
    return True;
}

int read_sig(zckCtx *zck) {
    VALIDATE_READ(zck);

    if(zck->index_string == NULL) {
        zck_log(ZCK_LOG_ERROR,
                "Reading signatures before index is read\n");
        return False;
    }

    char *header = zck->header + zck->lead_size + zck->preface_size +
                   zck->index_size;
    size_t max_length = zck->header_size - (zck->lead_size + zck->preface_size +
                                            zck->index_size);
    size_t length = 0;

    if(!compint_to_int(&(zck->sigs.count), header, &length, max_length))
        return False;

    /* We don't actually support signatures yet, so bail if there is one */
    zck_log(ZCK_LOG_DEBUG, "Signature count: %i\n", zck->sigs.count);
    if(zck->sigs.count > 0) {
        zck_log(ZCK_LOG_ERROR, "Signatures aren't supported yet\n");
        return False;
    }

    /* Set data_offset */
    zck->data_offset = zck->lead_size + zck->header_length;

    if(zck->header_size >
       zck->lead_size + zck->preface_size + zck->index_size + length)
        zck_log(ZCK_LOG_WARNING, "There are %lu unused bytes in the header\n");

    zck->sig_size = length;
    zck->sig_string = header;
    return True;
}

int PUBLIC zck_read_header(zckCtx *zck) {
    VALIDATE_READ(zck);

    if(!read_lead_1(zck))
        return False;
    if(!read_lead_2(zck))
        return False;
    if(!validate_header(zck))
        return False;
    if(!read_preface(zck))
        return False;
    if(!read_index(zck))
        return False;
    if(!read_sig(zck))
        return False;
    return True;
}

int preface_create(zckCtx *zck) {
    int header_malloc = zck->hash_type.digest_size + 4 + 2*MAX_COMP_SIZE;

    char *header = zmalloc(header_malloc);
    if(header == NULL) {
        zck_log(ZCK_LOG_ERROR, "Unable to allocate %lu bytes\n", header_malloc);
        return False;
    }
    size_t length = 0;

    /* Write out the full data digest */
    memcpy(header + length, zck->full_hash_digest, zck->hash_type.digest_size);
    length += zck->hash_type.digest_size;

    /* Write out flags */
    memset(header + length, 0, 3);
    length += 3;
    /* Final byte for flags */
    if(zck->has_streams)
        header[length] &= 1;
    length += 1;

    /* Write out compression type and index size */
    if(!compint_from_int(header+length, zck->comp.type, &length)) {
        free(header);
        return False;
    }
    compint_from_size(header+length, zck->index_size, &length);

    /* Shrink header to actual size */
    header = realloc(header, length);
    if(header == NULL) {
        zck_log(ZCK_LOG_ERROR, "Unable to reallocate %lu bytes\n", length);
        return False;
    }

    zck->preface_string = header;
    zck->preface_size = length;
    zck_log(ZCK_LOG_DEBUG, "Generated preface: %lu bytes\n", zck->preface_size);
    return True;
}

int sig_create(zckCtx *zck) {
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
    zck_log(ZCK_LOG_DEBUG, "Generated signatures: %lu bytes\n", zck->sig_size);
    return True;
}

int lead_create(zckCtx *zck) {
    int phs = 5 + 2*MAX_COMP_SIZE + zck->hash_type.digest_size;
    char *header = zmalloc(phs);
    if(header == NULL) {
        zck_log(ZCK_LOG_ERROR, "Unable to allocate %lu bytes\n", phs);
        return False;
    }
    size_t length = 0;
    memcpy(header, "\0ZCK1", 5);
    length += 5;

    /* Write out full data and header hash type */
    compint_from_size(header + length, zck->hash_type.type, &length);
    /* Write out header length */
    zck->header_length = zck->preface_size + zck->index_size + zck->sig_size;
    compint_from_size(header + length, zck->header_length, &length);
    /* Skip header digest; we'll fill it in later */
    zck->hdr_digest_loc = length;
    length += zck->hash_type.digest_size;

    header = realloc(header, length);
    if(header == NULL) {
        zck_log(ZCK_LOG_ERROR, "Unable to reallocate %lu bytes\n", length);
        return False;
    }

    zck->lead_string = header;
    zck->lead_size = length;
    zck_log(ZCK_LOG_DEBUG, "Generated lead: %lu bytes\n", zck->lead_size);
    return True;
}

int zck_header_create(zckCtx *zck) {
    /* Rebuild header without header hash */
    if(zck->header_digest) {
        free(zck->header_digest);
        zck->header_digest = NULL;
    }

    /* Generate index */
    if(!index_create(zck))
        return False;

    /* Generate preface */
    if(!preface_create(zck))
        return False;

    /* Rebuild signatures */
    if(!sig_create(zck))
        return False;

    /* Rebuild pre-header */
    if(!lead_create(zck))
        return False;

    /* Calculate data offset */
    zck->data_offset = zck->lead_size + zck->preface_size +
                       zck->index_size + zck->sig_size;

    /* Merge everything into one large string */
    zck_log(ZCK_LOG_DEBUG, "Merging into header: %lu bytes\n",
            zck->data_offset);
    zck->header = zmalloc(zck->data_offset);
    if(zck->header == NULL) {
        zck_log(ZCK_LOG_ERROR, "Unable to allocate %lu bytes\n",
                zck->data_offset);
        return False;
    }
    size_t offs = 0;
    memcpy(zck->header + offs, zck->lead_string, zck->lead_size);
    free(zck->lead_string);
    zck->lead_string = zck->header + offs;
    offs += zck->lead_size;
    memcpy(zck->header + offs, zck->preface_string, zck->preface_size);
    free(zck->preface_string);
    zck->preface_string = zck->header + offs;
    offs += zck->preface_size;
    memcpy(zck->header + offs, zck->index_string, zck->index_size);
    free(zck->index_string);
    zck->index_string = zck->header + offs;
    offs += zck->index_size;
    memcpy(zck->header + offs, zck->sig_string, zck->sig_size);
    free(zck->sig_string);
    zck->sig_string = zck->header + offs;
    zck->header_size = zck->data_offset;

    zckHash header_hash = {0};

    /* Calculate hash of header */
    if(!zck_hash_init(&header_hash, &(zck->hash_type)))
        return False;
    zck_log(ZCK_LOG_DEBUG, "Hashing lead\n");
    /* Hash lead up to header digest */
    if(!zck_hash_update(&header_hash, zck->lead_string,
                        zck->hdr_digest_loc))
        return False;
    zck_log(ZCK_LOG_DEBUG, "Hashing the rest\n");
    /* Hash rest of header */
    if(!zck_hash_update(&header_hash, zck->preface_string, zck->header_length))
        return False;
    zck->header_digest = zck_hash_finalize(&header_hash);
    if(zck->header_digest == NULL) {
        zck_log(ZCK_LOG_ERROR,
                "Unable to calculate %s checksum for index\n",
                zck_hash_name_from_type(zck->hash_type.type));
        return False;
    }
    /* Write digest to header */
    memcpy(zck->lead_string+zck->hdr_digest_loc, zck->header_digest,
           zck->hash_type.digest_size);

    return True;
}

int zck_write_header(zckCtx *zck) {
    VALIDATE_WRITE(zck);

    zck_log(ZCK_LOG_DEBUG, "Writing header: %lu bytes\n",
            zck->lead_size);
    if(!write_data(zck->fd, zck->header, zck->header_size))
        return False;
    return True;
}
