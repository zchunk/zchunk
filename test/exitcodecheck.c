/*
 * Copyright 2018, 2023 Jonathan Dieter <jdieter@gmail.com>
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
#ifndef _WIN32
#include <sys/wait.h>
#endif
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
        printf("Usage: %s <command> <expected exit code> [args]\n",
               argv[0]);
        exit(1);
    }

    char *cmd = untaint(argv[1]);
    int exit_code = atoi(argv[2]);
    char **args = calloc(argc-1, sizeof(void*));
    int return_code = 0;

    args[0] = cmd;
    for(int i=1; i<argc-2; i++)
        args[i] = untaint(argv[i+2]);

    int status;
#ifdef _WIN32
    char* fullcmd = malloc(2000);
    if (!fullcmd)
    {
        printf("Unable to allocate 2000 bytes\n");
        exit(97);
    }
    strcpy_s(fullcmd, 2000, args[0]);
    for(int i=1; i<argc-2; i++)
    {
        strcat_s(fullcmd, 2000, " ");
        strcat_s(fullcmd, 2000, args[i]);
    }
    status = system(fullcmd);
    free(fullcmd);
#else
    pid_t child_pid;

    child_pid = fork();
    if (child_pid == -1) {
        perror("fork failed");
        exit(97);
    } else if(child_pid == 0) {
        execv(cmd, args);
        perror("Unable to run command");
        exit(98);
    } else {
        waitpid(child_pid, &status, 0);
    }
    if (!WIFEXITED(status)) {
        printf("Application didn't exit normally\n");
        /* Prevent next error message from showing */
        status = exit_code;
        return_code = 1;
    } else {
        status = WEXITSTATUS(status);
    }
#endif
    if (status != exit_code) {
        printf("Exit code %i doesn't match expected exit code %i\n", status, exit_code);
        return_code = 1;
    }

    for(int i=0; i<argc-2; i++)
        free(args[i]);
    free(args);
    return return_code;
}
