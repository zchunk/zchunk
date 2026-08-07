/*
 * Copyright 2018, 2020 Jonathan Dieter <jdieter@gmail.com>
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

#define _GNU_SOURCE
#define STDERR_FILENO 2

#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include <sys/types.h>
#include <sys/stat.h>
#ifndef _WIN32
#include <sys/wait.h>
#endif
#include <fcntl.h>
#ifndef _WIN32
#include <libgen.h>
#endif
#include <dirent.h>
#include <unistd.h>
#ifndef _AIX
#include <argp.h>
#endif
#include <zck.h>

#if defined(stdout)
#undef stdout
#endif

#include "util_common.h"

static char doc[] = "zck_gen_zdict - Generate a zdict for a zchunk file";

static char args_doc[] = "<file>";

static struct argp_option options[] = {
    {"verbose", 'v', 0,        0,
     "Increase verbosity (can be specified more than once for debugging)"},
    /*{"stdout",  'c', 0,        0, "Direct output to stdout"},*/
    {"dir",     'd', "DIRECTORY", 0,
     "Write individual chunks to DIRECTORY (defaults to temporary directory)"},
    {"version", 'V', 0,        0, "Show program version"},
    { 0 }
};

struct arguments {
  char *args[1];
  char *dir;
  zck_log_type log_level;
  bool stdout;
  bool exit;
};

static error_t parse_opt (int key, char *arg, struct argp_state *state) {
    struct arguments *arguments = state->input;

    if(arguments->exit)
        return 0;

    switch (key) {
        case 'v':
            arguments->log_level--;
            if(arguments->log_level < ZCK_LOG_DDEBUG)
                arguments->log_level = ZCK_LOG_DDEBUG;
            break;
        /*case 'c':
            arguments->stdout = true;
            break;*/
        case 'd':
            arguments->dir = arg;
            break;
        case 'V':
            version();
            arguments->exit = true;
            break;
        case ARGP_KEY_ARG:
            if (state->arg_num >= 1) {
                argp_usage (state);
                return EINVAL;
            }
            arguments->args[state->arg_num] = arg;

            break;

        case ARGP_KEY_END:
            if (state->arg_num < 1) {
                argp_usage (state);
                return EINVAL;
            }
            break;

        default:
            return ARGP_ERR_UNKNOWN;
    }
    return 0;
}

static struct argp argp = {options, parse_opt, args_doc, doc};

char *get_tmp_dir(char *old_dir) {
    char *dir = NULL;
    if(old_dir == NULL) {
        char* template = "zcktempXXXXXX";
        #ifdef _WIN32
        char *tmpdir = getenv("TEMP");
        #else
        char *tmpdir = getenv("TMPDIR");
        #endif

        if(tmpdir == NULL) {
            tmpdir = "/tmp/";
        } else if(strlen(tmpdir) > 1024) {
            printf("TMPDIR environmental variable is > 1024 bytes\n");
            return NULL;
        }

#ifdef _WIN32
        char* prev_cwd;

        // Get the current working directory:
        if ((prev_cwd = _getcwd( NULL, 0 )) == NULL)
        {
            LOG_ERROR("Could not find current workdir");
            return NULL;
        }
        if (chdir(tmpdir) != 0)
        {
            LOG_ERROR("Could not change to Temp Dir");
            return NULL;
        }
        printf("generating temp name: ... \n");
        errno_t err = _mktemp_s(template, 14);
        if (err)
        {
            LOG_ERROR("Could not generate temporary name");
            return NULL;
        }
        if (_mkdir(template) != 0)
        {
            LOG_ERROR("Could not create temp folder");
            return NULL;
        }
        assert(chdir(template) == 0);
        char* res = _getcwd(NULL, 0);
        assert(chdir(prev_cwd));
        return res;
#else
        char *base_dir = calloc(strlen(template) + strlen(tmpdir) + 2, 1);
        assert(base_dir);
        int i=0;
        for(i=0; i<strlen(tmpdir); i++)
            base_dir[i] = tmpdir[i];
        int offset = i;
        base_dir[offset] = '/';
        offset++;
        for(i=0; i<strlen(template); i++)
            base_dir[offset + i] = template[i];
        offset += i;
        base_dir[offset] = '\0';
        dir = mkdtemp(base_dir);
        if(dir == NULL) {
            perror("ERROR: ");
            return NULL;
        }
#endif
    } else {
        dir = calloc(strlen(old_dir) + 1, 1);
        assert(dir);
        int i=0;
        for(i=0; i<strlen(old_dir); i++)
            dir[i] = old_dir[i];
        dir[i] = '\0';
    }
    return dir;
}

int main (int argc, char *argv[]) {
    struct arguments arguments = {0};

    /* Defaults */
    arguments.log_level = ZCK_LOG_ERROR;

    int retval = argp_parse (&argp, argc, argv, 0, 0, &arguments);
    if(retval || arguments.exit)
        exit(retval);

    zck_set_log_level(arguments.log_level);

    int src_fd = open(arguments.args[0], O_RDONLY | O_BINARY);
    if(src_fd < 0) {
        LOG_ERROR("Unable to open %s\n", arguments.args[0]);
        perror("");
        exit(1);
    }
    char *base_name = basename(arguments.args[0]);
    // len .zck -> .zdict = +2 + \0 = +3
    char *out_name = calloc(strlen(base_name) + 3, 1);
    assert(out_name);
    snprintf(out_name, strlen(base_name) - 3, "%s", base_name); //Strip off .zck

    char *dir = get_tmp_dir(arguments.dir);
    if(dir == NULL) {
        free(out_name);
        exit(1);
    }
    bool good_exit = false;

    char *data = NULL;
    zckCtx *zck = zck_create();
    if(!zck_init_read(zck, src_fd)) {
        LOG_ERROR("%s", zck_get_error(zck));
        goto error2;
    }

    int ret = zck_validate_data_checksum(zck);
    if(ret < 1) {
        if(ret == -1)
            LOG_ERROR("Data checksum failed verification\n");
        goto error2;
    }

    for(zckChunk *idx=zck_get_first_chunk(zck); idx!=NULL;
        idx=zck_get_next_chunk(idx)) {
        // Skip dictionary
        if(idx == zck_get_first_chunk(zck))
            continue;
        ssize_t chunk_size = zck_get_chunk_size(idx);
        if(chunk_size < 0) {
            LOG_ERROR("%s", zck_get_error(zck));
            goto error2;
        }
        data = calloc(chunk_size, 1);
        assert(data);
        ssize_t read_size = zck_get_chunk_data(idx, data, chunk_size);
        if(read_size != chunk_size) {
            if(read_size < 0)
                LOG_ERROR("%s", zck_get_error(zck));
            else
                LOG_ERROR(
                        "Chunk %lli size doesn't match expected size: %lli != %lli\n",
                        (long long) zck_get_chunk_number(idx),
                        (long long) read_size,
                        (long long) chunk_size
                );
            goto error2;
        }

        char *dict_block = calloc(strlen(dir) + strlen(out_name) + 12, 1);
        assert(dict_block);
        snprintf(dict_block, strlen(dir) + strlen(out_name) + 12, "%s/%s.%lli",
                 dir, out_name, (long long) zck_get_chunk_number(idx));
        int dst_fd = open(dict_block, O_TRUNC | O_WRONLY | O_CREAT | O_BINARY, 0666);
        if(dst_fd < 0) {
            LOG_ERROR("Unable to open %s", dict_block);
            perror("");
            free(dict_block);
            goto error2;
        }
        if(write(dst_fd, data, chunk_size) != chunk_size) {
            LOG_ERROR("Error writing to %s\n", dict_block);
            free(dict_block);
            goto error2;
        }
        free(data);
        close(dst_fd);
        free(dict_block);
    }
    snprintf(out_name + strlen(base_name) - 4, 7, ".zdict");

    if(!zck_close(zck)) {
        LOG_ERROR("%s", zck_get_error(zck));
        goto error2;
    }

    /* Create dictionary */
#ifdef _WIN32
    char buf[5000];
    sprintf(buf, "zstd --train '%s' -r -o '%s'", dir, out_name);
    int w = system(buf);
    if (w < 0)
    {
        LOG_ERROR("Error generating dict\n");
        goto error2;
    }
#else
    int pid = fork();
    if(pid == 0) {
        execl("/usr/bin/zstd", "zstd", "--train", dir, "-r", "-o", out_name, NULL);
        LOG_ERROR("Unable to find /usr/bin/zstd\n");
        exit(1);
    }
    int wstatus = 0;
    int w = waitpid(pid, &wstatus, 0);

    if (w == -1) {
        LOG_ERROR("Error waiting for zstd\n");
        perror("");
        goto error2;
    }
    if(WEXITSTATUS(wstatus) != 0) {
        LOG_ERROR("Error generating dict\n");
        goto error2;
    }
#endif
    /* Clean up temporary directory */
    if(!arguments.dir) {
        struct dirent *dp;
        DIR *dfd;

        if ((dfd = opendir(dir)) == NULL) {
            LOG_ERROR("Unable to read %s\n", dir);
            goto error2;
        }

        bool err = false;
        while((dp = readdir(dfd)) != NULL) {
            if(dp->d_name[0] == '.')
                continue;
            char *full_path = calloc(strlen(dir) + strlen(dp->d_name) + 2, 1);
            snprintf(full_path, strlen(dir) + strlen(dp->d_name) + 2, "%s/%s",
                     dir, dp->d_name);
            if(unlink(full_path) != 0) {
                LOG_ERROR("Unable to remove %s\n", full_path);
                perror("");
                err = true;
            } else {
                if(arguments.log_level <= ZCK_LOG_INFO)
                    LOG_ERROR("Removed %s\n", full_path);
            }
            free(full_path);
        }
        closedir(dfd);
        if(!err) {
            if(rmdir(dir) != 0) {
                LOG_ERROR("Unable to remove %s\n", dir);
                perror("");
            }
        } else {
            LOG_ERROR("Errors encountered, not removing %s\n",
                    dir);
        }
    }
    good_exit = true;
error2:
    free(dir);
    zck_free(&zck);
    if(!good_exit)
        unlink(out_name);
    free(out_name);
    close(src_fd);
    if(!good_exit)
        exit(1);
    exit(0);
}
