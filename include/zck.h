#ifndef ZCK_H
#define ZCK_H

#define ZCK_VERSION "0.2.0"
#define ZCK_VER_MAJOR 0
#define ZCK_VER_MINOR 2
#define ZCK_VER_REVISION 0
#define ZCK_VER_SUBREVISION 0

#define True 1
#define False 0

typedef enum zck_hash {
    ZCK_HASH_SHA1,
    ZCK_HASH_SHA256
} zck_hash;

typedef enum zck_comp {
    ZCK_COMP_NONE,
    ZCK_COMP_GZIP, /* Not implemented yet */
    ZCK_COMP_ZSTD
} zck_comp;

typedef enum zck_ioption {
    ZCK_HASH_FULL_TYPE = 0,     /* Set full file hash type, using zck_hash */
    ZCK_HASH_CHUNK_TYPE,        /* Set chunk hash type using zck_hash */
    ZCK_COMP_TYPE = 100,        /* Set compression type using zck_comp */
    ZCK_COMP_DICT_SIZE,         /* Set compression dictionary size */
    ZCK_ZSTD_COMP_LEVEL = 1000  /* Set zstd compression level */
} zck_ioption;

typedef enum zck_soption {
    ZCK_COMP_DICT = 100         /* Set compression dictionary */
} zck_soption;

typedef enum zck_log_type {
    ZCK_LOG_DEBUG,
    ZCK_LOG_INFO,
    ZCK_LOG_WARNING,
    ZCK_LOG_ERROR
} zck_log_type;

typedef struct zckCtx zckCtx;
typedef struct zckHash zckHash;

/* Contains an index item pointing to a chunk */
typedef struct zckIndexItem {
    char *digest;
    int digest_size;
    int finished;
    size_t start;
    size_t comp_length;
    size_t length;
    struct zckIndexItem *next;
} zckIndexItem;

/* Contains everything about an index and a pointer to the first index item */
typedef struct zckIndex {
    size_t count;
    size_t length;
    int hash_type;
    size_t digest_size;
    zckIndexItem *first;
} zckIndex;


/*******************************************************************
 * Reading a zchunk file
 *******************************************************************/
/* Initialize zchunk for reading */
zckCtx *zck_init_read (int src_fd);
/* Decompress dst_size bytes from zchunk file to dst, while verifying hashes */
ssize_t zck_read(zckCtx *zck, char *dst, size_t dst_size);


/*******************************************************************
 * Writing a zchunk file
 *******************************************************************/
/* Initialize zchunk for writing */
zckCtx *zck_init_write (int dst_fd);
/* Compress data src of size src_size, and write to zchunk file
 * Due to the nature of zchunk files and how they are built, no data will
 * actually appear in the zchunk file until zck_close() is called */
ssize_t zck_write(zckCtx *zck, const char *src, const size_t src_size);
/* Create a chunk boundary */
ssize_t zck_end_chunk(zckCtx *zck);


/*******************************************************************
 * Common functions for finishing a zchunk file
 *******************************************************************/
/* Close a zchunk file so it may no longer be read from or written to. The
 * context still contains information about the file */
int zck_close(zckCtx *zck);
/* Free a zchunk context.  You must pass the address of the context, and the
 * context will automatically be set to NULL after it is freed */
void zck_free(zckCtx **zck);


/*******************************************************************
 * Options
 *******************************************************************/
/* Set string option */
int zck_set_soption(zckCtx *zck, zck_soption option, const void *value);
/* Set integer option */
int zck_set_ioption(zckCtx *zck, zck_ioption option, ssize_t value);


/*******************************************************************
 * Miscellaneous utilities
 *******************************************************************/
/* Set logging level */
void zck_set_log_level(zck_log_type ll);


/*******************************************************************
 * The functions should be all you need to read and write a zchunk
 * file.  After this point are advanced functions with an unstable
 * API, so use them with care.
 *******************************************************************/


/*******************************************************************
 * Advanced miscellaneous zchunk functions
 *******************************************************************/
/* Initialize zchunk context */
zckCtx *zck_create();
/* Get header length (header + index) */
ssize_t zck_get_header_length(zckCtx *zck);
/* Get data length */
ssize_t zck_get_data_length(zckCtx *zck);
/* Get index digest */
char *zck_get_header_digest(zckCtx *zck);
/* Get data digest */
char *zck_get_data_digest(zckCtx *zck);
/* Get temporary fd that will disappear when fd is closed */
int zck_get_tmp_fd();


/*******************************************************************
 * Advanced compression functions
 *******************************************************************/
/* Get name of compression type */
const char *zck_comp_name_from_type(int comp_type);
/* Initialize compression.  Compression type and parameters *must* be done
 * before this is called */
int zck_comp_init(zckCtx *zck);
/* Release compression resources.  After this is run, you may change compression
 * type and parameters */
int zck_comp_close(zckCtx *zck);
/* Reset compression configuration without losing buffered compressed data.
 * After this is run, you may change compression type and parameters */
int zck_comp_reset(zckCtx *zck);


/*******************************************************************
 * Advanced zchunk reading functions
 *******************************************************************/
/* Initialize zchunk for reading using advanced options */
zckCtx *zck_init_adv_read (int src_fd);
/* Read zchunk header */
int zck_read_header(zckCtx *zck);


/*******************************************************************
 * Indexes
 *******************************************************************/
/* Get index count */
ssize_t zck_get_index_count(zckCtx *zck);
/* Get index */
zckIndex *zck_get_index(zckCtx *zck);
/* Get chunk digest */
char *zck_get_chunk_digest(zckIndexItem *item);


/*******************************************************************
 * Advanced hash functions
 *******************************************************************/
/* Get overall hash type */
int zck_get_full_hash_type(zckCtx *zck);
/* Get digest size of overall hash type */
int zck_get_full_digest_size(zckCtx *zck);
/* Get chunk hash type */
int zck_get_chunk_hash_type(zckCtx *zck);
/* Get digest size of chunk hash type */
int zck_get_chunk_digest_size(zckCtx *zck);
/* Get name of hash type */
const char *zck_hash_name_from_type(int hash_type);
/* Check data hash */
int zck_hash_check_data(zckCtx *zck, int dst_fd);


/*******************************************************************
 * Downloading (should this go in a separate header and library?)
 *******************************************************************/
/* Contains a single range */
typedef struct zckRangeItem {
    size_t start;
    size_t end;
    struct zckRangeItem *next;
    struct zckRangeItem *prev;
} zckRangeItem;

/* Contains a series of ranges, information about them, a link to the first
 * range item, and an index describing what information is in the ranges */
typedef struct zckRange {
    unsigned int count;
    unsigned int segments;
    unsigned int max_ranges;
    zckRangeItem *first;
    zckIndex index;
} zckRange;

typedef struct zckDLPriv zckDLPriv;

/* Contains a zchunk download context */
typedef struct zckDL {
    size_t dl;
    size_t ul;
    int dst_fd;
    char *boundary;
    zckRange info;
    zckDLPriv *priv;
    struct zckCtx *zck;
} zckDL;

/*******************************************************************
 * Ranges
 *******************************************************************/
/* Update info with the maximum number of ranges in a single request */
int zck_range_calc_segments(zckRange *info, unsigned int max_ranges);
/* Get any matching chunks from src and put them in the right place in tgt */
int zck_dl_copy_src_chunks(zckRange *info, zckCtx *src, zckCtx *tgt);
/* Get index of chunks not available in src, and put them in info */
int zck_range_get_need_dl(zckRange *info, zckCtx *zck_src, zckCtx *zck_tgt);
/* Get array of range request strings.  ra must be allocated to size
 * info->segments, and the strings must be freed by the caller after use */
int zck_range_get_array(zckRange *info, char **ra);
/* Free any resources in zckRange */
void zck_range_close(zckRange *info);


/*******************************************************************
 * Downloading
 *******************************************************************/
/* Initialize curl stuff, should be run at beginning of any program using any
 * following functions */
void zck_dl_global_init();
/* Clean up curl stuff, should be run at end of any program using any following
 * functions */
void zck_dl_global_cleanup();

/* Initialize zchunk download context */
zckDL *zck_dl_init();
/* Free zchunk download context */
void zck_dl_free(zckDL **dl);
/* Clear regex used for extracting download ranges from multipart download */
void zck_dl_clear_regex(zckDL *dl);
/* Download and process the header from url */
int zck_dl_get_header(zckCtx *zck, zckDL *dl, char *url);
/* Get number of bytes downloaded using download context */
size_t zck_dl_get_bytes_downloaded(zckDL *dl);
/* Get number of bytes uploaded using download context */
size_t zck_dl_get_bytes_uploaded(zckDL *dl);
/* Download ranges specified in dl->info from url */
int zck_dl_range(zckDL *dl, char *url);
/* Return string with range request from start to end (inclusive) */
char *zck_dl_get_range(unsigned int start, unsigned int end);
#endif
