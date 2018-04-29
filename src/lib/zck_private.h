#ifndef ZCK_PRIVATE_H
#define ZCK_PRIVATE_H
#include <stdarg.h>
#include <stdint.h>
#include <stddef.h>
#include <regex.h>

#define BUF_SIZE 32768
/* Maximum string length for a compressed size_t */
#define MAX_COMP_SIZE (((sizeof(size_t) * 8) / 7) + 1)

#define ZCK_MODE_READ 0
#define ZCK_MODE_WRITE 1

#define zmalloc(x) calloc(1, x)

#define PUBLIC __attribute__((visibility("default")))

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

typedef struct zckDLPriv {
    CURL *curl_ctx;
    zckMP *mp;
    int parser_started;
    int is_chunk;
    size_t write_in_chunk;
    size_t dl_chunk_data;
    regex_t *dl_regex;
    regex_t *end_regex;
    regex_t *hdr_regex;
    zckIndexItem *tgt_check;
    zckHash *chunk_hash;
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

    char *data;
    size_t data_size;
    size_t data_loc;
    zckIndexItem *data_idx;
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
    zckIndexItem *work_index_item;
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
} zckCtx;

const char *zck_hash_name_from_type(int hash_type)
    __attribute__ ((warn_unused_result));
int zck_get_tmp_fd()
    __attribute__ ((warn_unused_result));
int zck_import_dict(zckCtx *zck)
    __attribute__ ((warn_unused_result));
int zck_validate_file(zckCtx *zck)
    __attribute__ ((warn_unused_result));
int zck_validate_current_chunk(zckCtx *zck)
    __attribute__ ((warn_unused_result));
int zck_validate_header(zckCtx *zck)
    __attribute__ ((warn_unused_result));
void zck_clear_work_index(zckCtx *zck);
char *get_digest_string(const char *digest, int size)
    __attribute__ ((warn_unused_result));

/* hash/hash.h */
int zck_hash_setup(zckHashType *ht, int h)
    __attribute__ ((warn_unused_result));
int zck_hash_init(zckHash *hash, zckHashType *hash_type)
    __attribute__ ((warn_unused_result));
int zck_hash_update(zckHash *hash, const char *message, const size_t size)
    __attribute__ ((warn_unused_result));
char *zck_hash_finalize(zckHash *hash)
    __attribute__ ((warn_unused_result));
void zck_hash_close(zckHash *hash);
const char *zck_hash_get_printable(const char *digest, zckHashType *type)
    __attribute__ ((warn_unused_result));
int set_full_hash_type(zckCtx *zck, int hash_type)
    __attribute__ ((warn_unused_result));
int set_chunk_hash_type(zckCtx *zck, int hash_type)
    __attribute__ ((warn_unused_result));

/* index/index.c */
int zck_index_read(zckCtx *zck, char *data, size_t size, size_t max_length)
    __attribute__ ((warn_unused_result));
int index_create(zckCtx *zck)
    __attribute__ ((warn_unused_result));
int zck_index_new_chunk(zckIndex *index, char *digest, int digest_size,
                        size_t comp_size, size_t orig_size, int finished)
    __attribute__ ((warn_unused_result));
int zck_index_add_to_chunk(zckCtx *zck, char *data, size_t comp_size,
                        size_t orig_size)
    __attribute__ ((warn_unused_result));
int zck_index_finish_chunk(zckCtx *zck)
    __attribute__ ((warn_unused_result));
void zck_index_clean(zckIndex *index);
void zck_index_free(zckCtx *zck);
void zck_index_free_item(zckIndexItem **item);
int zck_write_index(zckCtx *zck)
    __attribute__ ((warn_unused_result));
zckIndexItem *zck_get_index_of_loc(zckIndex *index, size_t loc)
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
int write_comp_size(int fd, size_t val)
    __attribute__ ((warn_unused_result));
int read_comp_size(int fd, size_t *val, size_t *length)
    __attribute__ ((warn_unused_result));
int chunks_from_temp(zckCtx *zck)
    __attribute__ ((warn_unused_result));

/* header.c */
int read_lead_1(zckCtx *zck)
    __attribute__ ((warn_unused_result));
int read_lead_2(zckCtx *zck)
    __attribute__ ((warn_unused_result));
int validate_header(zckCtx *zck)
    __attribute__ ((warn_unused_result));
int read_preface(zckCtx *zck)
    __attribute__ ((warn_unused_result));
int read_index(zckCtx *zck)
    __attribute__ ((warn_unused_result));
int read_sig(zckCtx *zck)
    __attribute__ ((warn_unused_result));
int zck_read_header(zckCtx *zck)
    __attribute__ ((warn_unused_result));
int zck_header_create(zckCtx *zck)
    __attribute__ ((warn_unused_result));
int zck_sig_create(zckCtx *zck)
    __attribute__ ((warn_unused_result));
int zck_write_header(zckCtx *zck)
    __attribute__ ((warn_unused_result));
int zck_write_sigs(zckCtx *zck)
    __attribute__ ((warn_unused_result));

/* comp/comp.c */
int zck_comp_add_to_dc(zckComp *comp, const char *src, size_t src_size)
    __attribute__ ((warn_unused_result));
int zck_comp_add_to_data(zckComp *comp, const char *src, size_t src_size)
    __attribute__ ((warn_unused_result));
size_t zck_comp_read_from_dc(zckComp *comp, char *dst, size_t dst_size)
    __attribute__ ((warn_unused_result));
ssize_t comp_read(zckCtx *zck, char *dst, size_t dst_size, int use_dict)
    __attribute__ ((warn_unused_result));
int comp_ioption(zckCtx *zck, zck_ioption option, ssize_t value)
    __attribute__ ((warn_unused_result));
int comp_soption(zckCtx *zck, zck_soption option, const void *value)
    __attribute__ ((warn_unused_result));

/* dl/range.c */
char *zck_range_get_char(zckRangeItem **range, int max_ranges)
    __attribute__ ((warn_unused_result));
int zck_range_add(zckRange *info, zckIndexItem *idx, zckCtx *zck)
    __attribute__ ((warn_unused_result));

/* dl/multipart.c */
size_t zck_multipart_extract(zckDL *dl, char *b, size_t l)
    __attribute__ ((warn_unused_result));
size_t zck_multipart_get_boundary(zckDL *dl, char *b, size_t size)
    __attribute__ ((warn_unused_result));

/* dl/dl.c */
int zck_dl_write_range(zckDL *dl, const char *at, size_t length)
    __attribute__ ((warn_unused_result));
int zck_dl_range_chk_chunk(zckDL *dl, char *url, int is_chunk)
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
void zck_log(zck_log_type lt, const char *format, ...);
#endif
