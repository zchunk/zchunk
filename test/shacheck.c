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
#include <stdbool.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <zck.h>
#include <sys/wait.h>
#include "zck_private.h"
#include "util.h"

char *untaint(const char *input) {
    char *output = zmalloc(strlen(input)+1);
    int i=0;
    for(i=0; i<strlen(input); i++)
        output[i] = input[i];
    output[i] = '\0';
    return output;
}

int main (int argc, char *argv[]) {
    if(argc < 4) {
        printf("Usage: %s <command> <outputfile> <expected checksum> [args]\n",
               argv[0]);
        exit(1);
    }

    char *cmd = untaint(argv[1]);
    char *outf = argv[2];
    char *echecksum = argv[3];

    char **args = calloc(argc-2, sizeof(void*));

    args[0] = cmd;
    for(int i=1; i<argc-3; i++)
        args[i] = untaint(argv[i+3]);

    int status;
    pid_t child_pid;

    child_pid = fork();
    if (child_pid == -1) {
        perror("fork failed");
        exit(1);
    } else if(child_pid == 0) {
        execv(cmd, args);
        perror("Unable to run command");
        exit(1);
    } else {
        waitpid(child_pid, &status, 0);
    }
    if (status != 0) {
        printf("Error running command\n");
        exit(1);
    }

    /* Open zchunk file and check that checksum matches */
    int in = open(outf, O_RDONLY);
    if(in < 0) {
        perror("");
        printf("Unable to open %s for reading", outf);
        exit(1);
    }
    /* Files must be smaller than 1MB  */
    char data[1024*1024] = {0};
    ssize_t len = read(in, data, 1024*1024);
    if(len < 0) {
        perror("");
        printf("Unable to read from %s", outf);
        exit(1);
    }
    char *cksum = get_hash(data, len, ZCK_HASH_SHA256);
    printf("%s:\n", outf);
    printf("Calculated checksum: (SHA-256)%s\n", cksum);
    printf("Expected checksum: (SHA-256)%s\n", echecksum);
    if(memcmp(cksum, echecksum, strlen(echecksum)) != 0) {
        printf("Checksums don't match!\n");
        exit(1);
    }
    free(cksum);
    for(int i=0; i<argc-3; i++)
        free(args[i]);
    free(args);
    return 0;
}
