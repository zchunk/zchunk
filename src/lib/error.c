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
#include <assert.h>
#include <string.h>
#include <stdarg.h>
#include <zck.h>

#include "zck_private.h"

char *snprintf_error = "String conversion error";

void set_error_wf(zckCtx *zck, int fatal, const char *format, ...) {
    va_list args;
    int size = 0;
    assert(zck != NULL && zck->msg == NULL && format != NULL);
    zck->error_state = 1 + (fatal > 0 ? 1 : 0);

    va_start(args, format);
    size = vsnprintf(NULL, 0, format, args);
    if(size < 0)
        return;
    zck->msg = zmalloc(size+1);
    vsnprintf(zck->msg, size+1, format, args);
    va_end(args);
}

char PUBLIC *zck_get_error(zckCtx *zck) {
    assert(zck != NULL);

    return zck->msg;
}

int PUBLIC zck_clear_error(zckCtx *zck) {
    assert(zck != NULL);

    if(zck->error_state > 1)
        return False;

    free(zck->msg);
    zck->msg = NULL;
    zck->error_state = 0;
    return True;
}
