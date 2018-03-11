#ifndef ZCK_PRIVATE_H
#define ZCK_PRIVATE_H
#include <stdarg.h>
#include <stdint.h>
#include <stddef.h>

#define zmalloc(x) calloc(1, x)

struct zckComp;

typedef int (*finit)(struct zckComp *comp);
typedef int (*fparam)(struct zckComp *comp, int option, void *value);
typedef int (*fcomp)(struct zckComp *comp, const char *src,
                     const size_t src_size, char **dst, size_t *dst_size,
                     int use_dict);
typedef int (*fdecomp)(struct zckComp *comp, const char *src,
                       const size_t src_size, char **dst, size_t *dst_size,
                       int use_dict);
typedef int (*fcclose)(struct zckComp *comp);

typedef enum log_type log_type;

typedef struct zckHashType {
    uint8_t type;
    int digest_size;
} zckHashType;

typedef struct {
    zckHashType *type;
    void *ctx;
} zckHash;

/*typedef struct zckIndex zckIndex;*/
typedef void CURL;

typedef struct zckMP {
    int state;
    size_t length;
    char *buffer;
    size_t buffer_len;
} zckMP;

typedef struct zckDLPriv {
    CURL *curl_ctx;
    zckMP *mp;
    int parser_started;
} zckDLPriv;

typedef struct zckComp {
    int started;

    uint8_t type;
    int level;

    void *cctx;
    void *dctx;
    void *cdict_ctx;
    void *ddict_ctx;
    void *dict;
    size_t dict_size;
    finit init;
    fparam set_parameter;
    fcomp compress;
    fdecomp decompress;
    fcclose close;
} zckComp;

typedef struct zckCtx {
    int temp_fd;
    int fd;

    char *full_hash_digest;
    char *comp_index;
    size_t comp_index_size;
    zckIndexInfo index;
    char *index_digest;
    zckHash full_hash;
    zckHash check_full_hash;
    zckComp comp;
    zckHashType hash_type;
} zckCtx;

const char *zck_hash_name_from_type(uint8_t hash_type);
int zck_get_tmp_fd();

/* comp/comp.c */
int zck_comp_init(zckCtx *zck);
int zck_compress(zckCtx *zck, const char *src, const size_t src_size);
int zck_decompress(zckCtx *zck, const char *src, const size_t src_size, char **dst, size_t *dst_size);
int zck_comp_close(zckCtx *zck);
int zck_set_compression_type(zckCtx *zck, uint8_t type);
int zck_set_comp_parameter(zckCtx *zck, int option, void *value);

/* hash/hash.h */
int zck_hash_setup(zckHashType *ht, int h);
int zck_hash_init(zckHash *hash, zckHashType *hash_type);
int zck_hash_update(zckHash *hash, const char *message, const size_t size);
char *zck_hash_finalize(zckHash *hash);
void zck_hash_close(zckHash *hash);

/* index/index.c */
int zck_index_read(zckCtx *zck, char *data, size_t size);
int zck_index_finalize(zckCtx *zck);
int zck_index_add_chunk(zckCtx *zck, char *data, size_t size);
int zck_index_free(zckCtx *zck);
int zck_write_index(zckCtx *zck);

/* io.c */
int zck_read(int fd, char *data, size_t length);
int zck_write(int fd, const char *data, size_t length);
int zck_chunks_from_temp(zckCtx *zck);

/* header.c */
int zck_read_initial(zckCtx *zck, int src_fd);
int zck_read_index_hash(zckCtx *zck, int src_fd);
int zck_read_comp_type(zckCtx *zck, int src_fd);
int zck_read_index_size(zckCtx *zck, int src_fd);
int zck_read_index(zckCtx *zck, int src_fd);
int zck_read_header(zckCtx *zck, int src_fd);
int zck_write_header(zckCtx *zck);

/* dl/range.c */
char *zck_range_get_char(zckRange **range, int max_ranges);
int zck_range_add(zckRangeInfo *info, uint64_t start, uint64_t end);

/* log.c */
void zck_log(log_type lt, const char *format, ...);
#endif
