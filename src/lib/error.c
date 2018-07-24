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
#include <zck.h>

#include "zck_private.h"

void set_fatal_error(zckCtx *zck, const char *msg) {
    assert(zck != NULL && zck->msg == NULL && msg != NULL);

    zck->fatal_msg = zmalloc(strlen(msg)+1);
    strncpy(zck->fatal_msg, msg, strlen(msg));
}

void set_error(zckCtx *zck, const char *msg) {
    assert(zck != NULL && zck->msg == NULL && msg != NULL);

    zck->msg = zmalloc(strlen(msg)+1);
    strncpy(zck->msg, msg, strlen(msg));
}

char PUBLIC *zck_get_error(zckCtx *zck) {
    assert(zck != NULL);

    if(zck->fatal_msg)
        return zck->fatal_msg;
    return zck->msg;
}

int PUBLIC zck_clear_error(zckCtx *zck) {
    assert(zck != NULL);

    if(zck->fatal_msg)
        return False;

    free(zck->msg);
    zck->msg = NULL;
    return True;
}
