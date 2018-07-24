#ifndef ZCK_PRIVATE_H
#define ZCK_PRIVATE_H
#include <stdarg.h>
#include <stdint.h>
#include <stddef.h>
#include <regex.h>
#include "buzhash/buzhash.h"

#define BUF_SIZE 32768
/* Maximum string length for a compressed size_t */
#define MAX_COMP_SIZE (((sizeof(size_t) * 8) / 7) + 1)

#define ZCK_MODE_READ 0
#define ZCK_MODE_WRITE 1

#define DEFAULT_BUZHASH_WIDTH 48
#define DEFAULT_BUZHASH_BITS 15

#define zmalloc(x) calloc(1, x)

#define PUBLIC __attribute__((visibility("default")))

#define zck_log(...) zck_log_wf(__func__, __VA_ARGS__)

#define set_error(zck, ...) set_error_wf(zck, 0, __VA_ARGS__); \
                            zck_log(__VA_ARGS__)
#define set_fatal_error(zck, ...) set_error_wf(zck, 1, __VA_ARGS__); \
                                  zck_log(__VA_ARGS__)
struct zckComp;

typedef int (*finit)(struct zckComp *comp);
typedef int (*fparam)(struct zckComp *comp, int option, const void *value);
typedef int (*fccompend)(struct zckComp *comp, char **dst, size_t *dst_size,
                         int use_dict);
typedef ssize_t (*fcomp)(struct zckComp *comp, const char *src,
                         const size_t src_size, char **dst, size_t *dst_size,
                         int use_dict);
typedef int (*fdecomp)(struct zckComp *comp, const int use_dict);
typedef int (*fdcompend)(struct zckComp *comp, const int use_dict,
                         const size_t fd_size);
typedef int (*fcclose)(struct zckComp *comp);

typedef enum zck_log_type zck_log_type;


typedef struct zckHashType {
    int type;
    int digest_size;
} zckHashType;

typedef struct zckHash {
    zckHashType *type;
    void *ctx;
} zckHash;

typedef void CURL;

typedef struct zckMP {
    int state;
    size_t length;
    char *buffer;
    size_t buffer_len;
} zckMP;

typedef struct zckDL {
    struct zckCtx *zck;
    size_t dl;
    size_t ul;
    zckRange *range;
    zckMP *mp;
    char *boundary;
    int parser_started;
    int is_chunk;
    size_t write_in_chunk;
    size_t dl_chunk_data;
    regex_t *dl_regex;
    regex_t *end_regex;
    regex_t *hdr_regex;
    zckChunk *tgt_check;
    int tgt_number;

    /* Callbacks */
    zck_wcb write_cb;
    void *write_data;
    zck_wcb header_cb;
    void *header_data;
} zckDL;

/* Contains an index item pointing to a chunk */
typedef struct zckChunk {
    char *digest;
    int digest_size;
    int valid;
    size_t start;
    size_t comp_length;
    size_t length;
    struct zckChunk *next;
    zckCtx *zck;
} zckChunk;

/* Contains everything about an index and a pointer to the first index item */
typedef struct zckIndex {
    size_t count;
    size_t length;
    int hash_type;
    size_t digest_size;
    zckChunk *first;
} zckIndex;

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
    zckRangeItem *first;
    zckIndex index;
} zckRange;

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

    char *data;
    size_t data_size;
    size_t data_loc;
    zckChunk *data_idx;
    int data_eof;
    char *dc_data;
    size_t dc_data_size;
    size_t dc_data_loc;

    finit init;
    fparam set_parameter;
    fcomp compress;
    fccompend end_cchunk;
    fdecomp decompress;
    fdcompend end_dchunk;
    fcclose close;
} zckComp;

typedef struct zckSig {
    zckHashType hash_type;
    size_t length;
    char *signature;
    void *ctx;
} zckSig;

typedef struct zckSigCollection {
    int count;
    zckSig *sig;
} zckSigCollection;

typedef struct zckCtx {
    int temp_fd;
    int fd;
    int mode;

    char *full_hash_digest;
    char *header_digest;
    size_t data_offset;
    size_t header_length;

    char *header;
    size_t header_size;
    size_t hdr_digest_loc;
    char *lead_string;
    size_t lead_size;
    char *preface_string;
    size_t preface_size;
    char *index_string;
    size_t index_size;
    char *sig_string;
    size_t sig_size;


    char *prep_digest;
    int prep_hash_type;
    ssize_t prep_hdr_size;

    zckIndex index;
    zckChunk *work_index_item;
    zckHash work_index_hash;
    size_t stream;
    int has_streams;

    char *read_buf;
    size_t read_buf_size;

    zckHash full_hash;
    zckHash check_full_hash;
    zckHash check_chunk_hash;
    zckComp comp;
    zckHashType hash_type;
    zckHashType chunk_hash_type;
    zckSigCollection sigs;

    char *data;
    size_t data_size;

    buzHash buzhash;
    int buzhash_width;
    int buzhash_match_bits;
    int buzhash_bitmask;
    int manual_chunk;

    char *msg;
    int error_state;
} zckCtx;

int get_tmp_fd()
    __attribute__ ((warn_unused_result));
int import_dict(zckCtx *zck)
    __attribute__ ((warn_unused_result));


/* hash/hash.h */
int hash_setup(zckHashType *ht, int h)
    __attribute__ ((warn_unused_result));
int hash_init(zckHash *hash, zckHashType *hash_type)
    __attribute__ ((warn_unused_result));
int hash_update(zckHash *hash, const char *message, const size_t size)
    __attribute__ ((warn_unused_result));
char *hash_finalize(zckHash *hash)
    __attribute__ ((warn_unused_result));
void hash_close(zckHash *hash);
void hash_reset(zckHashType *ht);
int validate_chunk(zckCtx *zck, zckChunk *idx, zck_log_type bad_checksum,
                   int chunk_number)
    __attribute__ ((warn_unused_result));
int validate_file(zckCtx *zck, zck_log_type bad_checksums)
    __attribute__ ((warn_unused_result));
int validate_current_chunk(zckCtx *zck)
    __attribute__ ((warn_unused_result));
int validate_header(zckCtx *zck)
    __attribute__ ((warn_unused_result));
int set_full_hash_type(zckCtx *zck, int hash_type)
    __attribute__ ((warn_unused_result));
int set_chunk_hash_type(zckCtx *zck, int hash_type)
    __attribute__ ((warn_unused_result));
int get_max_hash_size()
    __attribute__ ((warn_unused_result));
char *get_digest_string(const char *digest, int size)
    __attribute__ ((warn_unused_result));


/* index/index.c */
int index_read(zckCtx *zck, char *data, size_t size, size_t max_length)
    __attribute__ ((warn_unused_result));
int index_create(zckCtx *zck)
    __attribute__ ((warn_unused_result));
int index_new_chunk(zckIndex *index, char *digest, int digest_size,
                    size_t comp_size, size_t orig_size, int valid,
                    zckCtx *zck)
    __attribute__ ((warn_unused_result));
int index_add_to_chunk(zckCtx *zck, char *data, size_t comp_size,
                        size_t orig_size)
    __attribute__ ((warn_unused_result));
int index_finish_chunk(zckCtx *zck)
    __attribute__ ((warn_unused_result));
void index_clean(zckIndex *index);
void index_free(zckCtx *zck);
void clear_work_index(zckCtx *zck);
int write_index(zckCtx *zck)
    __attribute__ ((warn_unused_result));


/* io.c */
int seek_data(int fd, off_t offset, int whence)
    __attribute__ ((warn_unused_result));
ssize_t tell_data(int fd)
    __attribute__ ((warn_unused_result));
ssize_t read_data(int fd, char *data, size_t length)
    __attribute__ ((warn_unused_result));
int write_data(int fd, const char *data, size_t length)
    __attribute__ ((warn_unused_result));
int chunks_from_temp(zckCtx *zck)
    __attribute__ ((warn_unused_result));

/* header.c */
int header_create(zckCtx *zck)
    __attribute__ ((warn_unused_result));
int write_header(zckCtx *zck)
    __attribute__ ((warn_unused_result));

/* comp/comp.c */
int comp_init(zckCtx *zck)
    __attribute__ ((warn_unused_result));
int comp_close(zckCtx *zck)
    __attribute__ ((warn_unused_result));
int comp_reset(zckCtx *zck)
    __attribute__ ((warn_unused_result));
int comp_add_to_dc(zckComp *comp, const char *src, size_t src_size)
    __attribute__ ((warn_unused_result));
ssize_t comp_read(zckCtx *zck, char *dst, size_t dst_size, int use_dict)
    __attribute__ ((warn_unused_result));
int comp_ioption(zckCtx *zck, zck_ioption option, ssize_t value)
    __attribute__ ((warn_unused_result));
int comp_soption(zckCtx *zck, zck_soption option, const void *value,
                 size_t length)
    __attribute__ ((warn_unused_result));

/* dl/range.c */
char *range_get_char(zckRangeItem **range, int max_ranges)
    __attribute__ ((warn_unused_result));

/* dl/multipart.c */
size_t multipart_extract(zckDL *dl, char *b, size_t l)
    __attribute__ ((warn_unused_result));
size_t multipart_get_boundary(zckDL *dl, char *b, size_t size)
    __attribute__ ((warn_unused_result));
void reset_mp(zckMP *mp);

/* dl/dl.c */
int dl_write_range(zckDL *dl, const char *at, size_t length)
    __attribute__ ((warn_unused_result));

/* compint.c */
int compint_from_int(char *compint, int val, size_t *length)
    __attribute__ ((warn_unused_result));
void compint_from_size(char *compint, size_t val, size_t *length);
int compint_to_int(int *val, const char *compint, size_t *length,
                   size_t max_length)
    __attribute__ ((warn_unused_result));
int compint_to_size(size_t *val, const char *compint, size_t *length,
                    size_t max_length)
    __attribute__ ((warn_unused_result));

/* log.c */
void zck_log_wf(const char *function, zck_log_type lt, const char *format, ...);

/* error.c */
void set_error_wf(zckCtx *zck, int fatal, const char *format, ...);

#endif
