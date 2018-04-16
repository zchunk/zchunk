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
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <zck.h>

#include "memmem.h"

#define WINDOW_SIZE 4096
#define MATCH_SUM WINDOW_SIZE-1

int main (int argc, char *argv[]) {
    char *out_name;
    char *dict = NULL;
    size_t dict_size = 0;

    zck_set_log_level(ZCK_LOG_DEBUG);

    if(argc < 3 || argc > 4) {
        printf("Usage: %s <file> <split string> [optional dictionary]\n", argv[0]);
        exit(1);
    }
    out_name = malloc(strlen(argv[1]) + 5);
    snprintf(out_name, strlen(argv[1]) + 5, "%s.zck", argv[1]);
    if(argc == 4) {
        int dict_fd = open(argv[3], O_RDONLY);
        if(dict_fd < 0) {
            printf("Unable to open dictionary %s for reading", argv[3]);
            perror("");
            exit(1);
        }
        dict_size = lseek(dict_fd, 0, SEEK_END);
        if(dict_size < 0) {
            perror("Unable to seek to end of dictionary");
            exit(1);
        }
        if(lseek(dict_fd, 0, SEEK_SET) < 0) {
            perror("Unable to seek to beginning of dictionary");
            exit(1);
        }
        dict = malloc(dict_size);
        read(dict_fd, dict, dict_size);
        close(dict_fd);
    }

    int dst_fd = open(out_name, O_EXCL | O_WRONLY | O_CREAT, 0644);
    if(dst_fd < 0) {
        printf("Unable to open %s", out_name);
        perror("");
        if(dict) {
            free(dict);
            dict = NULL;
        }
        free(out_name);
        exit(1);
    }
    free(out_name);

    zckCtx *zck = zck_init_write(dst_fd);
    if(zck == NULL) {
        printf("Unable to write to %s\n", out_name);
        exit(1);
    }

    /*if(!zck_set_ioption(zck, ZCK_COMP_TYPE, ZCK_COMP_NONE)) {
        perror("Unable to set compression type\n");
        exit(1);
    }*/
    if(dict_size > 0) {
        if(!zck_set_ioption(zck, ZCK_COMP_DICT_SIZE, dict_size))
            exit(1);
        if(!zck_set_soption(zck, ZCK_COMP_DICT, dict))
            exit(1);
    }
    free(dict);

    char *data;
    int in_fd = open(argv[1], O_RDONLY);
    int in_size = 0;
    if(in_fd < 0) {
        printf("Unable to open %s for reading", argv[1]);
        perror("");
        exit(1);
    }
    in_size = lseek(in_fd, 0, SEEK_END);
    if(in_size < 0) {
        perror("Unable to seek to end of input file");
        exit(1);
    }
    if(lseek(in_fd, 0, SEEK_SET) < 0) {
        perror("Unable to seek to beginning of input file");
        exit(1);
    }
    if(in_size > 0) {
        data = malloc(in_size);
        read(in_fd, data, in_size);
        close(in_fd);

        /* Chunk based on string in argv[2] (Currently with ugly hack to group srpms together) */
        if(True) {
            char *found = data;
            char *search = found;
            char *prev_srpm = memmem(search, in_size - (search-data), "<rpm:sourcerpm", 14);
            while(search) {
                char *next = memmem(search, in_size - (search-data), argv[2], strlen(argv[2]));
                if(next) {
                    char *next_srpm = memmem(next, in_size - (next-data), "<rpm:sourcerpm", 14);

                    if(prev_srpm > next)
                        prev_srpm = NULL;
                    if(prev_srpm) {
                        int matched=0;
                        char prev = '\0';
                        for(int i=0;;i++) {
                            if(next_srpm[i] != prev_srpm[i])
                                break;
                            if(next_srpm[i] == '/' && prev == '<') {
                                matched = 1;
                                break;
                            }
                            prev = next_srpm[i];
                        }
                        if(matched) {
                            search = next + 1;
                            if(search > data + in_size)
                                search = data + in_size;
                            continue;
                        }

                    }
                    prev_srpm = next_srpm;
                    printf("Compressing %li bytes\n", next-found);
                    if(zck_write(zck, found, next-found) < 0)
                        exit(1);
                    if(zck_end_chunk(zck) < 0)
                        exit(1);
                    found = next;
                    search = next + 1;
                    if(search > data + in_size)
                        search = data + in_size;
                } else {
                    printf("Completing %li bytes\n", data+in_size-found);
                    if(zck_write(zck, found, data+in_size-found) < 0)
                        exit(1);
                    if(zck_end_chunk(zck) < 0)
                        exit(1);
                    search = NULL;
                }
            }
        /* Naive (and inefficient) rolling window */
        } else {
            char *cur_loc = data;
            char *start = data;
            char *window_loc;
            int window_sum;

            while(cur_loc < data + in_size) {
                window_sum = 0;
                window_loc = cur_loc;
                if(cur_loc + WINDOW_SIZE < data + in_size) {
                    for(int i=0; i<WINDOW_SIZE; i++) {
                        window_sum += cur_loc[i];
                    }
                    cur_loc += WINDOW_SIZE;
                    while(cur_loc < data + in_size) {
                        window_sum += cur_loc[0];
                        window_sum -= window_loc[0];
                        cur_loc++;
                        window_loc++;
                        if(((window_sum) & (WINDOW_SIZE - 1)) == 0)
                            break;
                    }
                } else {
                    cur_loc = data + in_size;
                }
                printf("Completing %li bytes\n", cur_loc-start);
                if(zck_write(zck, start, cur_loc-start) < 0)
                    exit(1);
                if(zck_end_chunk(zck) < 0)
                    exit(1);
                start = cur_loc;
            }
        }
        free(data);
    }
    if(!zck_close(zck))
        exit(1);
    zck_free(&zck);
    close(dst_fd);
}
