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

#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdarg.h>
#include <unistd.h>
#include <zck.h>

#include "zck_private.h"

static zck_log_type log_level = ZCK_LOG_ERROR;
#ifdef _WIN32
static int log_fd = 2;
#else
static int log_fd = STDERR_FILENO;
#endif

static logcallback callback = NULL;

void ZCK_PUBLIC_API zck_set_log_level(zck_log_type ll) {
    log_level = ll;
}

void ZCK_PUBLIC_API zck_set_log_fd(int fd) {
    log_fd = fd;
}

void ZCK_PUBLIC_API zck_set_log_callback(logcallback function) {
    if (!function)
        return;
    callback = function;
}

void zck_log_v(const char *function, zck_log_type lt, const char *format,
     va_list args) {
    if(lt < log_level || log_level == ZCK_LOG_ERROR)
        return;

    if (callback) {
        callback(function, lt, format, args);
    } else {
#ifdef _WIN32
        if (log_fd == 2)
        {
            fprintf(stderr, "%s: ", function);
            vfprintf(stderr, format, args);
            fprintf(stderr, "\n");
        }
        else
        {
            FILE *fstream = _fdopen(log_fd, "a+");
            fprintf(fstream, "%s: ", function);
            vfprintf(fstream, format, args);
            fprintf(fstream, "\n");
            _close(fstream);
        }
#else
        dprintf(log_fd, "%s: ", function);
        vdprintf(log_fd, format, args);
        dprintf(log_fd, "\n");
#endif
    }
}

void zck_log_wf(const char *function, zck_log_type lt, const char *format, ...) {
    va_list args;
    va_start(args, format);
    zck_log_v(function, lt, format, args);
    va_end(args);
}
