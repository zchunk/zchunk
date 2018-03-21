#ifndef ZCK_H
#define ZCK_H

#define ZCK_VERSION "0.0.1"
#define ZCK_VER_MAJOR 0
#define ZCK_VER_MINOR 0
#define ZCK_VER_REVISION 1
#define ZCK_VER_SUBREVISION 0

#define ZCK_COMP_NONE 0
#define ZCK_COMP_ZSTD 2

#define ZCK_HASH_SHA1   0
#define ZCK_HASH_SHA256 1

#define ZCK_COMMON_DICT 1
#define ZCK_COMMON_DICT_SIZE 2

#define ZCK_ZCK_COMP_LEVEL 20

#define True 1
#define False 0

typedef enum log_type { ZCK_LOG_DEBUG, ZCK_LOG_INFO, ZCK_LOG_WARNING,
                        ZCK_LOG_ERROR } log_type;

typedef struct zckIndex {
    char *digest;
    int digest_size;
    int finished;
    size_t start;
    size_t length;
    struct zckIndex *next;
} zckIndex;

typedef struct zckIndexInfo {
    size_t count;
    size_t length;
    int hash_type;
    size_t digest_size;
    zckIndex *first;
} zckIndexInfo;

typedef struct zckRange {
    size_t start;
    size_t end;
    struct zckRange *next;
    struct zckRange *prev;
} zckRange;

typedef struct zckRangeInfo {
    unsigned int count;
    unsigned int segments;
    unsigned int max_ranges;
    zckRange *first;
    zckIndexInfo index;
} zckRangeInfo;

typedef struct zckDLPriv zckDLPriv;
typedef struct zckCtx zckCtx;
typedef struct zckHash zckHash;

typedef struct zckDL {
    size_t dl;
    size_t ul;
    size_t write_in_chunk;
    size_t dl_chunk_data;
    int dst_fd;
    char *boundary;
    zckRangeInfo info;
    zckDLPriv *priv;
    struct zckCtx *zck;
    zckIndex *tgt_check;
    zckHash *chunk_hash;
} zckDL;


/* Get a zchunk context that can be used for creating or reading a zchunk
 * file.  Must be freed using zck_free */
zckCtx *zck_create();
/* Free a zchunk context.  You must pass the address of the context, and the
 * context will automatically be set to null after it is freed */
void zck_free(zckCtx **zck);
/* Clear a zchunk context so it may be reused */
void zck_clear(zckCtx *zck);

/* No idea, but probably important */
int zck_init_write (zckCtx *zck, int dst_fd);

/* Set compression type */
int zck_set_compression_type(zckCtx *zck, int comp_type);
/* Get name of compression type */
const char *zck_comp_name_from_type(int comp_type);
/* Set compression parameter */
int zck_set_comp_parameter(zckCtx *zck, int option, void *value);
/* Initialize compression.  Compression type and parameters *must* be done
 * before this is called */
int zck_comp_init(zckCtx *zck);
/* Release compression resources.  After this is run, you may change compression
 * type and parameters */
int zck_comp_close(zckCtx *zck);
/* Compress data src of size src_size, and write to chunk */
int zck_compress(zckCtx *zck, const char *src, const size_t src_size);
/* Decompress data src of size src_size, and write to dst, while setting
 * dst_size */
int zck_decompress(zckCtx *zck, const char *src, const size_t src_size,
                   char **dst, size_t *dst_size);

/* Write everything to disk */
int zck_write_file(zckCtx *zck);

/* Read zchunk header from src_fd */
int zck_read_header(zckCtx *zck, int src_fd);

/* Get index count */
ssize_t zck_get_index_count(zckCtx *zck);
/* Get index */
zckIndexInfo *zck_get_index(zckCtx *zck);

/* Decompress zchunk file pointed to by src_fd into dst_fd */
int zck_decompress_to_file (zckCtx *zck, int src_fd, int dst_fd);

/* Set overall hash type */
int zck_set_full_hash_type(zckCtx *zck, int hash_type);
/* Get overall hash type */
int zck_get_full_hash_type(zckCtx *zck);
/* Get digest size of overall hash type */
int zck_get_full_digest_size(zckCtx *zck);
/* Get index digest (uses overall hash type) */
char *zck_get_index_digest(zckCtx *zck);
/* Get index digest (uses overall hash type) */
char *zck_get_data_digest(zckCtx *zck);
/* Set chunk hash type */
int zck_set_chunk_hash_type(zckCtx *zck, int hash_type);
/* Get chunk hash type */
int zck_get_chunk_hash_type(zckCtx *zck);
/* Get digest size of chunk hash type */
int zck_get_chunk_digest_size(zckCtx *zck);
/* Get name of hash type */
const char *zck_hash_name_from_type(int hash_type);

/* Get header length (header + index) */
ssize_t zck_get_header_length(zckCtx *zck);

/* Get temporary fd */
int zck_get_tmp_fd();


int zck_range_calc_segments(zckRangeInfo *info, unsigned int max_ranges);
int zck_range_get_need_dl(zckRangeInfo *info, zckCtx *zck_src, zckCtx *zck_tgt);
int zck_dl_copy_src_chunks(zckRangeInfo *info, zckCtx *src, zckCtx *tgt);
int zck_range_get_array(zckRangeInfo *info, char **ra);
void zck_range_close(zckRangeInfo *info);
void zck_set_log_level(log_type ll);
void zck_dl_global_init();
void zck_dl_global_cleanup();
zckDL *zck_dl_init();
void zck_dl_free(zckDL **dl);
void zck_dl_free_regex(zckDL *dl);
int zck_dl_get_header(zckCtx *zck, zckDL *dl, char *url);
size_t zck_dl_get_bytes_downloaded(zckDL *dl);
size_t zck_dl_get_bytes_uploaded(zckDL *dl);
int zck_dl_range(zckDL *dl, char *url, int is_chunk);
char *zck_dl_get_range(unsigned int start, unsigned int end);
int zck_hash_check_full_file(zckCtx *zck, int dst_fd);
#endif
