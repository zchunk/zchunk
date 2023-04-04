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
#include <stdbool.h>
#include <string.h>
#include <unistd.h>
#include <zck.h>

#include "zck_private.h"

static bool check_flags(zckCtx *zck, size_t flags) {
    zck->has_streams = flags & 1;
    if(zck->has_streams) {
        flags -= 1;
        set_fatal_error(zck,
                        "This version of zchunk doesn't support streams");
        return false;
    }
    zck->has_optional_elems = flags & 2;
    if(zck->has_optional_elems)
        flags -= 2;
    zck->has_uncompressed_source = flags & 4;
    if(zck->has_uncompressed_source)
        flags -= 4;

    flags = flags & (SIZE_MAX - 1);
    if(flags != 0) {
        set_fatal_error(zck, "Unknown flags(s) set: %lu", flags);
        return false;
    }
    return true;
}

static ssize_t get_flags(zckCtx *zck) {
    size_t flags = 0;
    if(zck->has_streams)
        flags |= 1;
    if(zck->has_optional_elems)
        flags |= 2;
    if(zck->has_uncompressed_source)
        flags |= 4;
    return flags;
}

static bool read_optional_element(zckCtx *zck, size_t id, size_t data_size,
                                  char *data) {
    zck_log(ZCK_LOG_WARNING, "Unknown optional element id %i set", id);
    return true;
}

static bool read_header_from_file(zckCtx *zck) {
    /* Verify that lead_size and header_length have been set */
    if(zck->lead_size == 0 || zck->header_length == 0) {
        set_error(zck, "Lead and header sizes are both 0.  Have you run zck_read_lead() yet?");
        return false;
    }

    /* Allocate header and store any extra bytes at beginning of header */
    zck->header = zrealloc(zck->header, zck->lead_size + zck->header_length);
    if (!zck->header) {
        zck_log(ZCK_LOG_ERROR, "OOM in %s", __func__);
        return false;
    }
    zck->lead_string = zck->header;
    char *header = zck->header + zck->lead_size;
    size_t loaded = 0;

    if(zck->header_length < zck->header_size - zck->lead_size) {
        set_fatal_error(zck, "Header size is too small for actual data");
        return false;
    }
    if(zck->lead_size < zck->header_size)
        loaded = zck->header_size - zck->lead_size;

    /* Read header from file */
    zck_log(ZCK_LOG_DEBUG, "Reading the rest of the header: %llu bytes",
            (long long unsigned) zck->header_length);
    if(loaded < zck->header_length) {
        if(read_data(zck, header + loaded, zck->header_length - loaded) < zck->header_length - loaded) {
            set_fatal_error(zck, "Unable to read %llu bytes from the file", zck->header_length - loaded);
            return false;
        }
        zck->header_size = zck->lead_size + zck->header_length;
    }

    if(!hash_init(zck, &(zck->check_full_hash), &(zck->hash_type)))
        return false;
    /* If we're reading a detached zchunk header, first five bytes will be
     * different, breaking the header digest, so let's make things simple
     * by forcing the first five bytes to be static */
    if(!hash_update(zck, &(zck->check_full_hash), "\0ZCK1", 5))
        return false;
    /* Now hash the remaining lead */
    if(!hash_update(zck, &(zck->check_full_hash), zck->header+5,
                    zck->hdr_digest_loc-5))
        return false;
    /* And the remaining header */
    if(!hash_update(zck, &(zck->check_full_hash), header, zck->header_length))
        return false;
    int ret = validate_header(zck);
    if(ret < 1) {
        if(ret == -1)
            set_fatal_error(zck, "Header checksum failed verification");
        return false;
    }
    return true;
}

static bool read_preface(zckCtx *zck) {
    VALIDATE_READ_BOOL(zck);

    if(zck->header_digest == NULL) {
        set_error(zck, "Reading preface before lead is read");
        return false;
    }

    char *header = zck->header + zck->lead_size;
    size_t length = 0;
    size_t max_length = zck->header_length;

    /* Read data digest */
    zck_log(ZCK_LOG_DEBUG, "Reading data digest");
    if(length + zck->hash_type.digest_size > max_length) {
        set_fatal_error(zck, "Read past end of header");
        return false;
    }
    zck->full_hash_digest = zmalloc(zck->hash_type.digest_size);
    if (!zck->full_hash_digest) {
	    zck_log(ZCK_LOG_ERROR, "OOM in %s", __func__);
	    return false;
    }
    memcpy(zck->full_hash_digest, header+length, zck->hash_type.digest_size);
    length += zck->hash_type.digest_size;

    /* Read flags */
    size_t flags = 0;
    if(!compint_to_size(zck, &flags, header+length, &length, max_length))
        return false;
    if(!check_flags(zck, flags))
        return false;

    /* Setup for reading compression type */
    zck_log(ZCK_LOG_DEBUG, "Reading compression type and index size");
    int tmp = 0;

    /* Read and initialize compression type */
    if(!compint_to_int(zck, &tmp, header+length, &length, max_length))
        return false;
    if(!comp_ioption(zck, ZCK_COMP_TYPE, tmp))
        return false;
    if(!comp_init(zck))
        return false;

    /* Read optional flags */
    if(zck->has_optional_elems) {
        size_t opt_count = 0;
        if(!compint_to_size(zck, &opt_count, header+length, &length,
                            max_length))
            return false;
        for(size_t i=0; i<opt_count; i++) {
            size_t id = 0;
            size_t data_size = 0;
            if(!compint_to_size(zck, &id, header+length, &length, max_length))
                return false;
            if(!compint_to_size(zck, &data_size, header+length, &length,
                                max_length))
                return false;
            if(!read_optional_element(zck, id, data_size, header+length))
                return false;
            length += data_size;
        }
    }

    /* Read and initialize index size */
    if(!compint_to_int(zck, &tmp, header+length, &length, max_length))
        return false;
    zck->index_size = tmp;

    zck->preface_string = header;
    zck->preface_size = length;
    return true;
}

static bool read_index(zckCtx *zck) {
    VALIDATE_READ_BOOL(zck);

    if(zck->preface_string == NULL) {
        set_error(zck, "Reading index before preface is read");
        return false;
    }

    char *header = NULL;
    if(zck->lead_size + zck->preface_size + zck->index_size >
       zck->header_size) {
        set_fatal_error(zck, "Read past end of header");
        return false;
    }
    header = zck->header + zck->lead_size + zck->preface_size;
    zck_log(ZCK_LOG_DEBUG, "Reading index at 0x%x", (unsigned long)(zck->lead_size + zck->preface_size));
    int max_length = zck->header_size - (zck->lead_size + zck->preface_size);
    if(!index_read(zck, header, zck->index_size, max_length))
        return false;

    zck->index_string = header;
    return true;
}

static bool read_sig(zckCtx *zck) {
    VALIDATE_READ_BOOL(zck);

    if(zck->index_string == NULL) {
        set_error(zck, "Reading signatures before index is read");
        return false;
    }

    char *header = zck->header + zck->lead_size + zck->preface_size +
                   zck->index_size;
    size_t max_length = zck->header_size - (zck->lead_size + zck->preface_size +
                                            zck->index_size);
    size_t length = 0;

    if(!compint_to_int(zck, &(zck->sigs.count), header, &length, max_length))
        return false;

    /* We don't actually support signatures yet, so bail if there is one */
    zck_log(ZCK_LOG_DEBUG, "Signature count: %i", zck->sigs.count);
    if(zck->sigs.count > 0) {
        set_fatal_error(zck, "Signatures aren't supported yet");
        return false;
    }

    /* Set data_offset */
    zck->data_offset = zck->lead_size + zck->header_length;

    if(zck->header_size >
       zck->lead_size + zck->preface_size + zck->index_size + length)
        zck_log(ZCK_LOG_WARNING, "There are unused bytes in the header");

    zck->sig_size = length;
    zck->sig_string = header;
    return true;
}

static bool preface_create(zckCtx *zck) {
    VALIDATE_WRITE_BOOL(zck);

    int header_malloc = zck->hash_type.digest_size + 4 + 2*MAX_COMP_SIZE;

    char *header = zmalloc(header_malloc);
    if (!header) {
	    zck_log(ZCK_LOG_ERROR, "OOM in %s", __func__);
	    return false;
    }
    size_t length = 0;

    /* Write out the full data digest */
    memcpy(header + length, zck->full_hash_digest, zck->hash_type.digest_size);
    length += zck->hash_type.digest_size;

    /* Write out flags */
    compint_from_size(header+length, get_flags(zck), &length);

    /* Write out compression type and index size */
    if(!compint_from_int(zck, header+length, zck->comp.type, &length)) {
        free(header);
        return false;
    }
    compint_from_size(header+length, zck->index_size, &length);

    /* Shrink header to actual size */
    header = zrealloc(header, length);
    if (!header) {
        zck_log(ZCK_LOG_ERROR, "OOM in %s", __func__);
        return false;
    }

    zck->preface_string = header;
    zck->preface_size = length;
    zck_log(
        ZCK_LOG_DEBUG,
        "Generated preface: %llu bytes",
        (long long unsigned) zck->preface_size
    );
    return true;
}

static bool sig_create(zckCtx *zck) {
    char *header = zmalloc(MAX_COMP_SIZE);
    size_t length = 0;

    if (!header) {
	    zck_log(ZCK_LOG_ERROR, "OOM in %s", __func__);
	    return false;
    }
    zck_log(ZCK_LOG_DEBUG, "Calculating %i signatures", zck->sigs.count);

    /* Write out signature count and signatures */
    if(!compint_from_int(zck, header+length, zck->sigs.count, &length)) {
        free(header);
        return false;
    }
    for(int i=0; i<zck->sigs.count; i++) {
        // TODO: Add signatures
    }
    zck->sig_string = header;
    zck->sig_size = length;
    zck_log(
        ZCK_LOG_DEBUG,
        "Generated signatures: %llu bytes",
        (long long unsigned) zck->sig_size
    );
    return true;
}

static bool lead_create(zckCtx *zck) {
    int phs = 5 + 2*MAX_COMP_SIZE + zck->hash_type.digest_size;
    char *header = zmalloc(phs);
    size_t length = 0;
    memcpy(header, "\0ZCK1", 5);
    length += 5;

    if (!header) {
	    zck_log(ZCK_LOG_ERROR, "OOM in %s", __func__);
	    return false;
    }
    /* Write out full data and header hash type */
    compint_from_size(header + length, zck->hash_type.type, &length);
    /* Write out header length */
    zck->header_length = zck->preface_size + zck->index_size + zck->sig_size;
    compint_from_size(header + length, zck->header_length, &length);
    /* Skip header digest; we'll fill it in later */
    zck->hdr_digest_loc = length;
    length += zck->hash_type.digest_size;

    header = zrealloc(header, length);
    if (!header) {
        zck_log(ZCK_LOG_ERROR, "OOM in %s", __func__);
        return false;
    }

    zck->lead_string = header;
    zck->lead_size = length;
    zck_log(
        ZCK_LOG_DEBUG,
        "Generated lead: %llu bytes",
        (long long unsigned) zck->lead_size
    );
    return true;
}

bool header_create(zckCtx *zck) {
    VALIDATE_WRITE_BOOL(zck);

    /* Rebuild header without header hash */
    if(zck->header_digest) {
        free(zck->header_digest);
        zck->header_digest = NULL;
    }

    /* Generate index */
    if(!index_create(zck))
        return false;

    /* Generate preface */
    if(!preface_create(zck))
        return false;

    /* Rebuild signatures */
    if(!sig_create(zck))
        return false;

    /* Rebuild pre-header */
    if(!lead_create(zck))
        return false;

    /* Calculate data offset */
    zck->data_offset = zck->lead_size + zck->preface_size +
                       zck->index_size + zck->sig_size;

    /* Merge everything into one large string */
    zck_log(
        ZCK_LOG_DEBUG,
        "Merging into header: %llu bytes",
        (long long unsigned) zck->data_offset
    );
    zck->header = zmalloc(zck->data_offset);
    if (!zck->header) {
	    zck_log(ZCK_LOG_ERROR, "OOM in %s", __func__);
	    return false;
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
    if(!hash_init(zck, &header_hash, &(zck->hash_type)))
        return false;
    zck_log(ZCK_LOG_DEBUG, "Hashing lead");
    /* Hash lead up to header digest */
    if(!hash_update(zck, &header_hash, zck->lead_string,
                    zck->hdr_digest_loc))
        return false;
    zck_log(ZCK_LOG_DEBUG, "Hashing the rest");
    /* Hash rest of header */
    if(!hash_update(zck, &header_hash, zck->preface_string, zck->header_length))
        return false;
    zck->header_digest = hash_finalize(zck, &header_hash);
    if(zck->header_digest == NULL)
        return false;

    /* Write digest to header */
    memcpy(zck->lead_string+zck->hdr_digest_loc, zck->header_digest,
           zck->hash_type.digest_size);

    return true;
}

bool write_header(zckCtx *zck) {
    VALIDATE_WRITE_BOOL(zck);

    zck_log(
        ZCK_LOG_DEBUG,
        "Writing header: %llu bytes",
        (long long unsigned) zck->lead_size
    );
    if(zck->no_write == 0 && !write_data(zck, zck->fd, zck->header, zck->header_size))
        return false;
    return true;
}

static bool read_lead(zckCtx *zck) {
    VALIDATE_READ_BOOL(zck);

    int lead = 5 + 2*MAX_COMP_SIZE;

    char *header = zmalloc(lead);
    if (!header) {
	    zck_log(ZCK_LOG_ERROR, "OOM in %s", __func__);
	    return false;
    }
    size_t length = 0;

    if(read_data(zck, header, lead) < lead) {
        free(header);
        set_error(zck, "Short read");
        return false;
    }

    if(memcmp(header, "\0ZHR1", 5) == 0) {
        zck->header_only = true;
    } else if(memcmp(header, "\0ZCK1", 5) != 0) {
        free(header);
        set_error(zck, "Invalid lead, perhaps this is not a zck file?");
        return false;
    }
    length += 5;

    /* Read hash type for header and full digest and initialize check hash */
    int hash_type = 0;
    if(!compint_to_int(zck, &hash_type, header+length, &length, lead)) {
        free(header);
        return false;
    }
    if(zck->prep_hash_type > -1 && zck->prep_hash_type != hash_type) {
        free(header);
        set_error(zck, "Hash type (%i) doesn't match requested hash type "
                  "(%i)", hash_type, zck->prep_hash_type);
        return false;
    }
    if(!hash_setup(zck, &(zck->hash_type), hash_type)) {
        free(header);
        return false;
    }
    zck_log(ZCK_LOG_DEBUG, "Setting header and full digest hash type to %s",
            zck_hash_name_from_type(hash_type));

    /* Read header size */
    size_t header_length = 0;
    if(!compint_to_size(zck, &header_length, header+length, &length, lead)) {
        free(header);
        hash_reset(&(zck->hash_type));
        return false;
    }
    if(header_length > SIZE_MAX) {
        free(header);
        set_error(zck, "Header length of %li invalid", header_length);
        hash_reset(&(zck->hash_type));
        return false;
    }
    zck->header_length = header_length;

    /* Set header digest location */
    zck->hdr_digest_loc = length;

    /* Read header digest */
    zck_log(ZCK_LOG_DEBUG, "Reading header digest");
    header = zrealloc(header, length + zck->hash_type.digest_size);
    if (!header) {
        zck_log(ZCK_LOG_ERROR, "OOM in %s", __func__);
        return false;
    }
    size_t to_read = 0;
    if(lead < length + zck->hash_type.digest_size)
        to_read = length + zck->hash_type.digest_size - lead;
    if(read_data(zck, header + lead, to_read) < to_read) {
        free(header);
        zck->header_length = 0;
        zck->hdr_digest_loc = 0;
        hash_reset(&(zck->hash_type));
        return false;
    }
    lead += to_read;

    if(zck->prep_digest &&
       memcmp(zck->prep_digest, header + length, zck->hash_type.digest_size) != 0) {
        zck->header_length = 0;
        zck->hdr_digest_loc = 0;
        hash_reset(&(zck->hash_type));
        set_error(zck,
                  "Header digest doesn't match requested header digest"
                  "Expected: %sActual: %s",
                  get_digest_string(zck->prep_digest,
                                    zck->hash_type.digest_size),
                  get_digest_string(header + length,
                                    zck->hash_type.digest_size));
        free(header);
        return false;
    }
    zck->header_digest = zmalloc(zck->hash_type.digest_size);
    if (!zck->header_digest) {
	    zck_log(ZCK_LOG_ERROR, "OOM in %s", __func__);
	    free(header);
	    return false;
    }
    memcpy(zck->header_digest, header + length, zck->hash_type.digest_size);
    length += zck->hash_type.digest_size;

    /* Check whether full header length matches specified header length */
    if(zck->prep_hdr_size > -1 &&
       (size_t)zck->prep_hdr_size != zck->header_length + length) {
        free(header);
        zck->header_length = 0;
        zck->hdr_digest_loc = 0;
        hash_reset(&(zck->hash_type));
        free(zck->header_digest);
        zck->header_digest = NULL;
        set_error(
            zck,
            "Header length (%llu) doesn't match requested header length (%llu)",
            (long long unsigned) zck->header_length + length,
            (long long unsigned) zck->prep_hdr_size
        );
        return false;
    }
    /* Store pre-header */
    zck->header = header;
    zck->header_size = lead;
    zck->lead_string = header;
    zck->lead_size = length;
    zck_log(
        ZCK_LOG_DEBUG,
        "Parsed lead: %llu bytes",
        (long long unsigned) length
    );
    return true;
}

bool ZCK_PUBLIC_API zck_read_lead(zckCtx *zck) {
    VALIDATE_BOOL(zck);

    return read_lead(zck);
}

bool ZCK_PUBLIC_API zck_validate_lead(zckCtx *zck) {
    VALIDATE_BOOL(zck);

    int retval = read_lead(zck);
    if(!zck_clear_error(zck))
        return false;
    free(zck->header);
    free(zck->header_digest);
    zck->header = NULL;
    zck->header_size = 0;
    zck->header_length = 0;
    zck->hdr_digest_loc = 0;
    zck->lead_string = NULL;
    zck->lead_size = 0;
    zck->header_digest = NULL;
    zck->hdr_digest_loc = 0;
    hash_reset(&(zck->hash_type));
    if(!seek_data(zck, 0, SEEK_SET))
        return false;
    return retval;
}

bool ZCK_PUBLIC_API zck_read_header(zckCtx *zck) {
    VALIDATE_READ_BOOL(zck);

    if(!read_header_from_file(zck))
        return false;
    if(!read_preface(zck))
        return false;
    if(!read_index(zck))
        return false;
    if(!read_sig(zck))
        return false;
    return true;
}

ssize_t ZCK_PUBLIC_API zck_get_header_length(zckCtx *zck) {
    VALIDATE_INT(zck);
    return zck->lead_size + zck->header_length;
}

ssize_t ZCK_PUBLIC_API zck_get_lead_length(zckCtx *zck) {
    VALIDATE_INT(zck);
    return zck->lead_size;
}

ssize_t ZCK_PUBLIC_API zck_get_data_length(zckCtx *zck) {
    VALIDATE_INT(zck);
    zckChunk *idx = zck->index.first;
    while(idx->next != NULL)
        idx = idx->next;
    return idx->start + idx->comp_length;
}

ssize_t ZCK_PUBLIC_API zck_get_length(zckCtx *zck) {
    VALIDATE_INT(zck);
    return zck_get_header_length(zck) + zck_get_data_length(zck);
}

ssize_t ZCK_PUBLIC_API zck_get_flags(zckCtx *zck) {
    VALIDATE_INT(zck);
    return get_flags(zck);
}

bool ZCK_PUBLIC_API zck_is_detached_header(zckCtx *zck) {
    VALIDATE_BOOL(zck);
    return zck->header_only;
}
