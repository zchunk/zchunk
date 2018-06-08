#ifndef ZCK_H
#define ZCK_H

#define ZCK_VERSION "0.6.3"
#define ZCK_VER_MAJOR 0
#define ZCK_VER_MINOR 6
#define ZCK_VER_REVISION 3
#define ZCK_VER_SUBREVISION 0

#define True 1
#define False 0

typedef enum zck_hash {
    ZCK_HASH_SHA1,
    ZCK_HASH_SHA256,
    ZCK_HASH_UNKNOWN
} zck_hash;

typedef enum zck_comp {
    ZCK_COMP_NONE,
    ZCK_COMP_GZIP, /* Not implemented yet */
    ZCK_COMP_ZSTD
} zck_comp;

typedef enum zck_ioption {
    ZCK_HASH_FULL_TYPE = 0,     /* Set full file hash type, using zck_hash */
    ZCK_HASH_CHUNK_TYPE,        /* Set chunk hash type using zck_hash */
    ZCK_VAL_HEADER_HASH_TYPE,   /* Set what the header hash type *should* be */
    ZCK_VAL_HEADER_LENGTH,      /* Set what the header length *should* be */
    ZCK_COMP_TYPE = 100,        /* Set compression type using zck_comp */
    ZCK_ZSTD_COMP_LEVEL = 1000  /* Set zstd compression level */
} zck_ioption;

typedef enum zck_soption {
    ZCK_VAL_HEADER_DIGEST = 0,  /* Set what the header hash *should* be */
    ZCK_COMP_DICT = 100         /* Set compression dictionary */
} zck_soption;

typedef enum zck_log_type {
    ZCK_LOG_DDEBUG = -1,
    ZCK_LOG_DEBUG,
    ZCK_LOG_INFO,
    ZCK_LOG_WARNING,
    ZCK_LOG_ERROR,
    ZCK_LOG_NONE
} zck_log_type;

typedef struct zckCtx zckCtx;
typedef struct zckHash zckHash;
typedef struct zckChunk zckChunk;
typedef struct zckIndex zckIndex;
typedef struct zckRange zckRange;
typedef struct zckDL zckDL;

typedef size_t (*zck_wcb)(void *ptr, size_t l, size_t c, void *dl_v);

/*******************************************************************
 * Reading a zchunk file
 *******************************************************************/
/* Initialize zchunk for reading */
zckCtx *zck_init_read (int src_fd)
    __attribute__ ((warn_unused_result));
/* Decompress dst_size bytes from zchunk file to dst, while verifying hashes */
ssize_t zck_read(zckCtx *zck, char *dst, size_t dst_size)
    __attribute__ ((warn_unused_result));


/*******************************************************************
 * Writing a zchunk file
 *******************************************************************/
/* Initialize zchunk for writing */
zckCtx *zck_init_write (int dst_fd)
    __attribute__ ((warn_unused_result));
/* Compress data src of size src_size, and write to zchunk file
 * Due to the nature of zchunk files and how they are built, no data will
 * actually appear in the zchunk file until zck_close() is called */
ssize_t zck_write(zckCtx *zck, const char *src, const size_t src_size)
    __attribute__ ((warn_unused_result));
/* Create a chunk boundary */
ssize_t zck_end_chunk(zckCtx *zck)
    __attribute__ ((warn_unused_result));


/*******************************************************************
 * Common functions for finishing a zchunk file
 *******************************************************************/
/* Close a zchunk file so it may no longer be read from or written to. The
 * context still contains information about the file */
int zck_close(zckCtx *zck)
    __attribute__ ((warn_unused_result));
/* Free a zchunk context.  You must pass the address of the context, and the
 * context will automatically be set to NULL after it is freed */
void zck_free(zckCtx **zck);


/*******************************************************************
 * Options
 *******************************************************************/
/* Set string option */
int zck_set_soption(zckCtx *zck, zck_soption option, const char *value,
                    size_t length)
    __attribute__ ((warn_unused_result));
/* Set integer option */
int zck_set_ioption(zckCtx *zck, zck_ioption option, ssize_t value)
    __attribute__ ((warn_unused_result));


/*******************************************************************
 * Miscellaneous utilities
 *******************************************************************/
/* Set logging level */
void zck_set_log_level(zck_log_type ll);
/* Validate the chunk and data checksums for the current file.
 * Returns -1 for error, 0 for invalid checksum and 1 for valid checksum */
int zck_validate_checksums(zckCtx *zck)
    __attribute__ ((warn_unused_result));
/* Validate just the data checksum for the current file */
int zck_validate_data_checksum(zckCtx *zck)
    __attribute__ ((warn_unused_result));
/* Go through file and mark valid chunks as valid */
int zck_find_valid_chunks(zckCtx *zck)
    __attribute__ ((warn_unused_result));

/* Get a zckRange of ranges that need to still be downloaded.
 * max_ranges is the maximum number of ranges supported in a single request
 *     by the server.  If the server supports unlimited ranges, set to -1
 * Returns NULL if there's an error */
zckRange *zck_get_dl_range(zckCtx *zck, int max_ranges)
    __attribute__ ((warn_unused_result));
/* Get a string representation of a zckRange */
char *zck_get_range_char(zckRange *range)
    __attribute__ ((warn_unused_result));
/* Get file descriptor attached to zchunk context */
int zck_get_fd(zckCtx *zck)
    __attribute__ ((warn_unused_result));

/* Return number of missing chunks (-1 if error) */
int zck_missing_chunks(zckCtx *zck)
    __attribute__ ((warn_unused_result));
/* Return number of failed chunks (-1 if error) */
int zck_failed_chunks(zckCtx *zck)
    __attribute__ ((warn_unused_result));
/* Reset failed chunks to become missing */
void zck_reset_failed_chunks(zckCtx *zck);

/*******************************************************************
 * The functions should be all you need to read and write a zchunk
 * file.  After this point are advanced functions with an unstable
 * API, so use them with care.
 *******************************************************************/


/*******************************************************************
 * Advanced miscellaneous zchunk functions
 *******************************************************************/
/* Initialize zchunk context */
zckCtx *zck_create()
    __attribute__ ((warn_unused_result));
/* Get lead length */
ssize_t zck_get_lead_length(zckCtx *zck)
    __attribute__ ((warn_unused_result));
/* Get header length (lead + preface + index + sigs) */
ssize_t zck_get_header_length(zckCtx *zck)
    __attribute__ ((warn_unused_result));
/* Get data length */
ssize_t zck_get_data_length(zckCtx *zck)
    __attribute__ ((warn_unused_result));
/* Get file length */
ssize_t zck_get_length(zckCtx *zck)
    __attribute__ ((warn_unused_result));
/* Get index digest */
char *zck_get_header_digest(zckCtx *zck)
    __attribute__ ((warn_unused_result));
/* Get data digest */
char *zck_get_data_digest(zckCtx *zck)
    __attribute__ ((warn_unused_result));


/*******************************************************************
 * Advanced compression functions
 *******************************************************************/
/* Get name of compression type */
const char *zck_comp_name_from_type(int comp_type)
    __attribute__ ((warn_unused_result));
/* Initialize compression.  Compression type and parameters *must* be done
 * before this is called */


/*******************************************************************
 * Advanced zchunk reading functions
 *******************************************************************/
/* Initialize zchunk for reading using advanced options */
zckCtx *zck_init_adv_read (int src_fd)
    __attribute__ ((warn_unused_result));
/* Read zchunk lead */
int zck_read_lead(zckCtx *zck)
    __attribute__ ((warn_unused_result));
/* Read zchunk header */
int zck_read_header(zckCtx *zck)
    __attribute__ ((warn_unused_result));


/*******************************************************************
 * Indexes
 *******************************************************************/
/* Get chunk count */
ssize_t zck_get_chunk_count(zckCtx *zck)
    __attribute__ ((warn_unused_result));
/* Get first chunk */
zckChunk *zck_get_first_chunk(zckCtx *zck)
    __attribute__ ((warn_unused_result));
/* Get next chunk */
zckChunk *zck_get_next_chunk(zckChunk *idx)
    __attribute__ ((warn_unused_result));
/* Get chunk starting location */
ssize_t zck_get_chunk_start(zckChunk *idx)
    __attribute__ ((warn_unused_result));
/* Get uncompressed chunk size */
ssize_t zck_get_chunk_size(zckChunk *idx)
    __attribute__ ((warn_unused_result));
/* Get compressed chunk size */
ssize_t zck_get_chunk_comp_size(zckChunk *idx)
    __attribute__ ((warn_unused_result));
/* Get validity of current chunk - 1 = valid, 0 = missing, -1 = invalid */
int zck_get_chunk_valid(zckChunk *idx)
    __attribute__ ((warn_unused_result));
/* Get chunk digest */
char *zck_get_chunk_digest(zckChunk *item)
    __attribute__ ((warn_unused_result));
/* Find out if two chunk digests are the same */
int zck_compare_chunk_digest(zckChunk *a, zckChunk *b)
    __attribute__ ((warn_unused_result));

/*******************************************************************
 * Advanced hash functions
 *******************************************************************/
/* Get overall hash type */
int zck_get_full_hash_type(zckCtx *zck)
    __attribute__ ((warn_unused_result));
/* Get digest size of overall hash type */
ssize_t zck_get_full_digest_size(zckCtx *zck)
    __attribute__ ((warn_unused_result));
/* Get chunk hash type */
int zck_get_chunk_hash_type(zckCtx *zck)
    __attribute__ ((warn_unused_result));
/* Get digest size of chunk hash type */
ssize_t zck_get_chunk_digest_size(zckCtx *zck)
    __attribute__ ((warn_unused_result));
/* Get name of hash type */
const char *zck_hash_name_from_type(int hash_type)
    __attribute__ ((warn_unused_result));



/*******************************************************************
 * Downloading (should this go in a separate header and library?)
 *******************************************************************/

/*******************************************************************
 * Ranges
 *******************************************************************/
/* Get any matching chunks from src and put them in the right place in tgt */
int zck_copy_chunks(zckCtx *src, zckCtx *tgt)
    __attribute__ ((warn_unused_result));
/* Free zckRange */
void zck_range_free(zckRange **info);
/* Get range string from start and end location */
char *zck_get_range(size_t start, size_t end)
    __attribute__ ((warn_unused_result));
/* Get the minimum size needed to download in order to know how large the header
 * is */
int zck_get_min_download_size()
    __attribute__ ((warn_unused_result));
/* Get the number of separate range items in the range */
int zck_get_range_count(zckRange *range)
    __attribute__ ((warn_unused_result));

/*******************************************************************
 * Downloading
 *******************************************************************/
/* Initialize zchunk download context */
zckDL *zck_dl_init(zckCtx *zck)
    __attribute__ ((warn_unused_result));
/* Reset zchunk download context for reuse */
void zck_dl_reset(zckDL *dl);
/* Free zchunk download context */
void zck_dl_free(zckDL **dl);
/* Get zchunk context from download context */
zckCtx *zck_dl_get_zck(zckDL *dl)
    __attribute__ ((warn_unused_result));
/* Clear regex used for extracting download ranges from multipart download */
void zck_dl_clear_regex(zckDL *dl);
/* Download and process the header from url */
int zck_dl_get_header(zckCtx *zck, zckDL *dl, char *url)
    __attribute__ ((warn_unused_result));
/* Get number of bytes downloaded using download context */
size_t zck_dl_get_bytes_downloaded(zckDL *dl)
    __attribute__ ((warn_unused_result));
/* Get number of bytes uploaded using download context */
size_t zck_dl_get_bytes_uploaded(zckDL *dl)
    __attribute__ ((warn_unused_result));
/* Set download ranges for zchunk download context */
int zck_dl_set_range(zckDL *dl, zckRange *range)
    __attribute__ ((warn_unused_result));
/* Get download ranges from zchunk download context */
int zck_dl_set_range(zckDL *dl, zckRange *range)
    __attribute__ ((warn_unused_result));

/* Set header callback function */
int zck_dl_set_header_cb(zckDL *dl, zck_wcb func)
    __attribute__ ((warn_unused_result));
/* Set header userdata */
int zck_dl_set_header_data(zckDL *dl, void *data)
    __attribute__ ((warn_unused_result));
/* Set write callback function */
int zck_dl_set_write_cb(zckDL *dl, zck_wcb func)
    __attribute__ ((warn_unused_result));
/* Set write userdata */
int zck_dl_set_write_data(zckDL *dl, void *data)
    __attribute__ ((warn_unused_result));

/* Write callback.  You *must* pass this and your initialized zchunk download
 * context to the downloader when downloading a zchunk file.  If you have your
 * own callback, set dl->write_cb to your callback and dl->wdata to your
 * callback data. */
size_t zck_write_chunk_cb(void *ptr, size_t l, size_t c, void *dl_v);
size_t zck_write_zck_header_cb(void *ptr, size_t l, size_t c, void *dl_v);
size_t zck_header_cb(char *b, size_t l, size_t c, void *dl_v);

#endif
