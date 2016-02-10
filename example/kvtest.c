/*
 * Copyright (c) 2015, Intel Corporation
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

/*
 * kvtest.c -- simple example for the libpmemkv put and get methods
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

#include "pmbackend.h"

#include "kv.h"

#define NUM 128
#define KEY_LEN 128
#define VAL_LEN  64UL * 1024
#define STORE_SIZE 2UL * 1024 * 1024 * 1024

int main(int argc, const char *argv[]) {
    if (argc != 2) {
        printf("Usage %s <filename>\n", argv[0]);
        exit(1);
    }

    pmb_opts opts;
    opts.max_key_len = KEY_LEN;
    opts.max_val_len = VAL_LEN;
    opts.write_log_entries = 32;
    opts.path = argv[1];
    opts.data_size = STORE_SIZE;
    opts.meta_size= STORE_SIZE / 10;
    opts.meta_max_key_len = KEY_LEN;
    opts.meta_max_val_len = VAL_LEN;
    opts.sync_type = PMB_NOSYNC;
    uint8_t error;
    pmb_handle *store = pmb_open(&opts, &error);
    if (error != PMB_OK) {
        printf("!!! ERROR %s !!!\n", pmb_strerror(error));

        exit(1);
    }

    pmb_pair p;
    p.blk_id = 0;
    void *key = malloc(opts.max_key_len);
    void *val = malloc(VAL_LEN / 4);
    uint64_t ids[NUM];

    // get itrator
    pmb_iter *iter = pmb_iter_open(store, 0);
    if (iter != NULL) {
        while(pmb_iter_valid(iter)) {
            pmb_iter_get(iter, &p);
            if (p.blk_id != 0)
                printf("saved object: blk_id %zu id %zu key %s\n", p.blk_id, p.id, (char *)p.key);
            pmb_iter_next(iter);
            p.blk_id = 0;
        }
        pmb_iter_close(iter);
    }

    p.offset = 0;
    p.key = key;
    p.val = val;
    p.key_len = KEY_LEN;
    p.val_len = VAL_LEN / 4;

    uint8_t status;
    uint64_t tx_id;
    pmb_tx_begin(store, &tx_id);
    for(int i = 0; i < NUM; i++) {
        snprintf(key, KEY_LEN, "%0126d", i);
        snprintf(val, VAL_LEN, "%0131d", i);
        p.id = i;
        status = pmb_tput(store, tx_id, &p);
        if (status == PMB_OK) {
            ids[i] = p.blk_id;
            p.blk_id = 0;
        } else {
            ids[i] = 0;
            printf("put failed for: %d: %s\n", i, pmb_strerror(status));
        }
    }
    pmb_tx_commit(store, tx_id);
    pmb_tx_execute(store, tx_id);

    for(int i = 0; i < NUM; i++) {
        if (ids[i] != 0) {
            pmb_get(store, ids[i], &p);
            printf("blk_id: %zu id: %zu key %s blk_id %zu\n", ids[i], p.id, (char *)p.key, p.blk_id);
        } else {
            printf("!!! id 0 for %d !!!\n", i);
        }
    }

    free(key);
    free(val);

    pmb_close(store);

    return 0;
}
