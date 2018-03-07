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


typedef struct zckRange {
    uint64_t start;
    uint64_t end;
    struct zckRange *next;
    struct zckRange *prev;
} zckRange;

typedef struct zckRangeInfo {
    unsigned int count;
    unsigned int segments;
    unsigned int max_ranges;
    zckRange *first;
} zckRangeInfo;

typedef struct zckIndex {
    char *digest;
    uint64_t start;
    size_t length;
    struct zckIndex *next;
} zckIndex;

typedef struct zckHashType zckHashType;

typedef struct zckIndexInfo {
    uint64_t count;
    size_t length;
    zckHashType *hash_type;
    zckIndex *first;
} zckIndexInfo;

typedef struct zckCtx zckCtx;
typedef struct zckDL zckDL;

zckCtx *zck_create();
void zck_free(zckCtx *zck);
int zck_init_write (zckCtx *zck, int dst_fd);
int zck_set_compression_type(zckCtx *zck, uint8_t comp_type);
int zck_set_comp_parameter(zckCtx *zck, int option, void *value);
int zck_comp_init(zckCtx *zck);
int zck_comp_close(zckCtx *zck);
int zck_compress(zckCtx *zck, const char *src, const size_t src_size);
int zck_decompress(zckCtx *zck, const char *src, const size_t src_size,
                   char **dst, size_t *dst_size);
int zck_write_file(zckCtx *zck);
int zck_read_header(zckCtx *zck, int src_fd);
uint64_t zck_get_index_count(zckCtx *zck);
zckIndexInfo *zck_get_index(zckCtx *zck);
int zck_decompress_to_file (zckCtx *zck, int src_fd, int dst_fd);
int zck_set_full_hash_type(zckCtx *zck, uint8_t hash_type);
int zck_set_chunk_hash_type(zckCtx *zck, uint8_t hash_type);
char *zck_get_index_digest(zckCtx *zck);
char *zck_get_full_digest(zckCtx *zck);
int zck_get_full_digest_size(zckCtx *zck);
int zck_get_chunk_digest_size(zckCtx *zck);
uint8_t zck_get_full_hash_type(zckCtx *zck);
uint8_t zck_get_chunk_hash_type(zckCtx *zck);
const char *zck_hash_name_from_type(uint8_t hash_type);
const char *zck_comp_name_from_type(uint8_t comp_type);
int zck_range_calc_segments(zckRangeInfo *info, unsigned int max_ranges);
int zck_range_get_need_dl(zckRangeInfo *info, zckCtx *zck_src, zckCtx *zck_tgt);
int zck_range_get_array(zckRangeInfo *info, char **ra);
void zck_range_close(zckRangeInfo *info);
void zck_set_log_level(log_type ll);
void zck_dl_global_init();
void zck_dl_global_cleanup();
zckDL *zck_dl_init();
void zck_dl_free(zckDL *dl);
int zck_dl_get_header(zckCtx *zck, zckDL *dl, char *url);
size_t zck_dl_get_bytes_downloaded(zckDL *dl);
size_t zck_dl_get_bytes_uploaded(zckDL *dl);
char *zck_dl_get_range(unsigned int start, unsigned int end);
#endif

