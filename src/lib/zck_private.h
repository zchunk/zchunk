#ifndef ZCK_PRIVATE_H
#define ZCK_PRIVATE_H

#include <stdarg.h>
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <regex.h>
#include "buzhash/buzhash.h"
#include "uthash.h"
#include "zck.h"

#define BUF_SIZE 32768
/* Maximum string length for a compressed size_t */
#define MAX_COMP_SIZE (((sizeof(size_t) * 8) / 7) + 1)

#define ZCK_MODE_READ 0
#define ZCK_MODE_WRITE 1

#define DEFAULT_BUZHASH_WIDTH 48
#define DEFAULT_BUZHASH_BITS 15
#define CHUNK_DEFAULT_MIN 1
#define CHUNK_DEFAULT_MAX 10485760 // 10MB

#define PUBLIC __attribute__((visibility("default")))

#define zck_log(...) zck_log_wf(__func__, __VA_ARGS__)

#define set_error(zck, ...) set_error_wf(zck, 0, __func__, __VA_ARGS__)
#define set_fatal_error(zck, ...) set_error_wf(zck, 1, __func__, __VA_ARGS__)

#define ALLOCD_BOOL(z, f)   if(!f) { \
                                set_error(z, \
                                        "Object not initialized"); \
                                return false; \
                            }
#define ALLOCD_INT(z, f)    if(!f) { \
                                set_error(z, \
                                        "Object not initialized"); \
                                return -1; \
                            }
#define ALLOCD_PTR(z, f)    if(!f) { \
                                set_error(z, \
                                        "Object not initialized"); \
                                return NULL; \
                            }
#define VALIDATE_BOOL(f)    ALLOCD_BOOL(f, f) \
                            if((f)->error_state > 0) return false;
#define VALIDATE_INT(f)     ALLOCD_INT(f, f) \
                            if((f)->error_state > 0) return -1;
#define VALIDATE_PTR(f)     ALLOCD_PTR(f, f) \
                            if((f)->error_state > 0) return NULL;

#define VALIDATE_READ_BOOL(f)   VALIDATE_BOOL(f); \
                                if(f->mode != ZCK_MODE_READ) { \
                                    set_error(f, \
                                        "zckCtx not opened for reading"); \
                                    return false; \
                                }
#define VALIDATE_READ_INT(f)    VALIDATE_INT(f); \
                                if(f->mode != ZCK_MODE_READ) { \
                                    set_error(f, \
                                        "zckCtx not opened for reading"); \
                                    return -1; \
                                }
#define VALIDATE_READ_PTR(f)    VALIDATE_PTR(f); \
                                if(f->mode != ZCK_MODE_READ) { \
                                    set_error(f, \
                                        "zckCtx not opened for reading"); \
                                    return NULL; \
                                }

#define VALIDATE_WRITE_BOOL(f)  VALIDATE_BOOL(f); \
                                if(f->mode != ZCK_MODE_WRITE) { \
                                    set_error(f, \
                                        "zckCtx not opened for writing"); \
                                    return false; \
                                }
#define VALIDATE_WRITE_INT(f)   VALIDATE_INT(f); \
                                if(f->mode != ZCK_MODE_WRITE) { \
                                    set_error(f, \
                                        "zckCtx not opened for writing"); \
                                    return -1; \
                                }
#define VALIDATE_WRITE_PTR(f)   VALIDATE_PTR(f); \
                                if(f->mode != ZCK_MODE_WRITE) { \
                                    set_error(f, \
                                        "zckCtx not opened for writing"); \
                                    return NULL; \
                                }

typedef struct zckComp zckComp;

typedef bool (*finit)(zckCtx *zck, zckComp *comp);
typedef bool (*fparam)(zckCtx *zck,zckComp *comp, int option, const void *value);
typedef bool (*fccompend)(zckCtx *zck, zckComp *comp, char **dst,
                          size_t *dst_size, bool use_dict);
typedef ssize_t (*fcomp)(zckCtx *zck, zckComp *comp, const char *src,
                         const size_t src_size, char **dst, size_t *dst_size,
                         bool use_dict);
typedef bool (*fdecomp)(zckCtx *zck, zckComp *comp, const bool use_dict);
typedef bool (*fdcompend)(zckCtx *zck, zckComp *comp, const bool use_dict,
                          const size_t fd_size);
typedef bool (*fcclose)(zckCtx *zck, zckComp *comp);

typedef struct zckHashType {
    int type;
    int digest_size;
} zckHashType;

struct zckHash {
    zckHashType *type;
    void *ctx;
};

typedef void CURL;

typedef struct zckMP {
    int state;
    size_t length;
    char *buffer;
    size_t buffer_len;
} zckMP;

struct zckDL {
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
};

/* Contains an index item pointing to a chunk */
struct zckChunk {
    char *digest;
    int digest_size;
    int valid;
    size_t number;
    size_t start;
    size_t comp_length;
    size_t length;
    struct zckChunk *next;
    struct zckChunk *src;
    zckCtx *zck;
    UT_hash_handle hh;
};

/* Contains everything about an index and a pointer to the first index item */
struct zckIndex {
    size_t count;
    size_t length;
    int hash_type;
    size_t digest_size;
    zckChunk *first;
    zckChunk *last;
    zckChunk *current;
    zckChunk *ht;
};

/* Contains a single range */
typedef struct zckRangeItem {
    size_t start;
    size_t end;
    struct zckRangeItem *next;
    struct zckRangeItem *prev;
} zckRangeItem;

/* Contains a series of ranges, information about them, a link to the first
 * range item, and an index describing what information is in the ranges */
struct zckRange {
    unsigned int count;
    zckRangeItem *first;
    zckIndex index;
};

struct zckComp {
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
};

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

struct zckCtx {
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
    int has_optional_elems;

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
    int chunk_auto_min;
    int chunk_auto_max;
    int chunk_min_size;
    int chunk_max_size;
    int manual_chunk;

    char *msg;
    int error_state;
};

int get_tmp_fd()
    __attribute__ ((warn_unused_result));
bool import_dict(zckCtx *zck)
    __attribute__ ((warn_unused_result));
void *zmalloc(size_t size)
    __attribute__ ((warn_unused_result));
void *zrealloc(void *ptr, size_t size)
    __attribute__ ((warn_unused_result));


/* hash/hash.h */
bool hash_setup(zckCtx *zck, zckHashType *ht, int h)
    __attribute__ ((warn_unused_result));
bool hash_init(zckCtx *zck, zckHash *hash, zckHashType *hash_type)
    __attribute__ ((warn_unused_result));
bool hash_update(zckCtx *zck, zckHash *hash, const char *message,
                 const size_t size)
    __attribute__ ((warn_unused_result));
char *hash_finalize(zckCtx *zck, zckHash *hash)
    __attribute__ ((warn_unused_result));
void hash_close(zckHash *hash);
void hash_reset(zckHashType *ht);
int validate_chunk(zckChunk *idx, zck_log_type bad_checksum)
    __attribute__ ((warn_unused_result));
int validate_file(zckCtx *zck, zck_log_type bad_checksums)
    __attribute__ ((warn_unused_result));
int validate_current_chunk(zckCtx *zck)
    __attribute__ ((warn_unused_result));
int validate_header(zckCtx *zck)
    __attribute__ ((warn_unused_result));
bool set_full_hash_type(zckCtx *zck, int hash_type)
    __attribute__ ((warn_unused_result));
bool set_chunk_hash_type(zckCtx *zck, int hash_type)
    __attribute__ ((warn_unused_result));
int get_max_hash_size()
    __attribute__ ((warn_unused_result));
char *get_digest_string(const char *digest, int size)
    __attribute__ ((warn_unused_result));


/* index/index.c */
bool index_read(zckCtx *zck, char *data, size_t size, size_t max_length)
    __attribute__ ((warn_unused_result));
bool index_create(zckCtx *zck)
    __attribute__ ((warn_unused_result));
bool index_new_chunk(zckCtx *zck, zckIndex *index, char *digest, int digest_size,
                     size_t comp_size, size_t orig_size, zckChunk *src, bool valid)
    __attribute__ ((warn_unused_result));
bool index_add_to_chunk(zckCtx *zck, char *data, size_t comp_size,
                        size_t orig_size)
    __attribute__ ((warn_unused_result));
bool index_finish_chunk(zckCtx *zck)
    __attribute__ ((warn_unused_result));
void index_clean(zckIndex *index);
void index_free(zckCtx *zck);
void clear_work_index(zckCtx *zck);
bool write_index(zckCtx *zck)
    __attribute__ ((warn_unused_result));


/* io.c */
int seek_data(zckCtx *zck, off_t offset, int whence)
    __attribute__ ((warn_unused_result));
ssize_t tell_data(zckCtx *zck)
    __attribute__ ((warn_unused_result));
ssize_t read_data(zckCtx *zck, char *data, size_t length)
    __attribute__ ((warn_unused_result));
int write_data(zckCtx *zck, int fd, const char *data, size_t length)
    __attribute__ ((warn_unused_result));
int chunks_from_temp(zckCtx *zck)
    __attribute__ ((warn_unused_result));

/* header.c */
bool header_create(zckCtx *zck)
    __attribute__ ((warn_unused_result));
bool write_header(zckCtx *zck)
    __attribute__ ((warn_unused_result));

/* comp/comp.c */
bool comp_init(zckCtx *zck)
    __attribute__ ((warn_unused_result));
bool comp_close(zckCtx *zck)
    __attribute__ ((warn_unused_result));
bool comp_reset(zckCtx *zck)
    __attribute__ ((warn_unused_result));
bool comp_add_to_dc(zckCtx *zck, zckComp *comp, const char *src, size_t src_size)
    __attribute__ ((warn_unused_result));
ssize_t comp_read(zckCtx *zck, char *dst, size_t dst_size, bool use_dict)
    __attribute__ ((warn_unused_result));
bool comp_ioption(zckCtx *zck, zck_ioption option, ssize_t value)
    __attribute__ ((warn_unused_result));
bool comp_soption(zckCtx *zck, zck_soption option, const void *value,
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
int compint_from_int(zckCtx *zck, char *compint, int val, size_t *length)
    __attribute__ ((warn_unused_result));
void compint_from_size(char *compint, size_t val, size_t *length);
int compint_to_int(zckCtx *zck, int *val, const char *compint, size_t *length,
                   size_t max_length)
    __attribute__ ((warn_unused_result));
int compint_to_size(zckCtx *zck, size_t *val, const char *compint,
                    size_t *length, size_t max_length)
    __attribute__ ((warn_unused_result));

/* log.c */
void zck_log_v(const char *function, zck_log_type lt, const char *format,
     va_list args);
void zck_log_wf(const char *function, zck_log_type lt, const char *format, ...);

/* error.c */
void set_error_wf(zckCtx *zck, int fatal, const char *function,
                  const char *format, ...);

#endif
