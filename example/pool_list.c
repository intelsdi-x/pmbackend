/*
 * Copyright (c) 2014-2015, Intel Corporation
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in
 *       the documentation and/or other materials provided with the
 *       distribution.
 *
 *     * Neither the name of Intel Corporation nor the names of its
 *       contributors may be used to endorse or promote products derived
 *       from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY LOG OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <errno.h>
#include <string.h>
#include "pmbackend.h"

int main(int argc, const char *argv[]) {
    if (argc != 5) {
        printf("Usage %s <key len> <val len> <handle size> <filename>\n", argv[0]);
        exit(1);
    }

    pmb_opts opts;
    opts.max_key_len = atoi(argv[1]);
    opts.max_val_len = atoi(argv[2]);
    opts.write_log_entries = 32;
    opts.path = argv[4];
    opts.data_size = strtol(argv[3], NULL, 10);
    uint8_t error;
    pmb_handle *handle = pmb_open(&opts, &error);
    if (error != PMB_OK) {
        printf("!!! ERROR %d errno %s !!!\n", error, strerror(errno));

        exit(1);
    }

    pmb_pair p;

    // get itrator
    pmb_iter *iter = pmb_iter_open(handle, 0);
    while(pmb_iter_valid(iter)) {
        pmb_iter_get(iter, &p);
        if (p.obj_id != 0)
            printf("saved object: id %zu key %s key_len %d\n", p.obj_id, (char *)p.key, p.key_len);
        pmb_iter_next(iter);
        p.obj_id = 0;
    }
    pmb_iter_close(iter);
    pmb_close(handle);

    return 0;
}
