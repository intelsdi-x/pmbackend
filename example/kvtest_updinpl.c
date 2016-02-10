/*
 * Copyright (c) 2014, Intel Corporation
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
 * kvtest_updinpl.c -- simple example for the libvmemkv update in place
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

#include "pmbackend.h"

#include "kv.h"

#define NUM 128
#define KEY_LEN 128
#define VAL_LEN  4UL * 1024
#define MAX_VAL_LEN  128UL * 1024
#define STORE_SIZE 1UL * 1024 * 1024 * 1024

int main(int argc, const char *argv[]) {
    if (argc != 2) {
        printf("Usage %s <filename>\n", argv[0]);
        exit(1);
    }

    pmb_opts opts;
    opts.max_key_len = KEY_LEN;
    opts.max_val_len = MAX_VAL_LEN;
    opts.write_log_entries = 32;
    opts.path = argv[1];
    opts.data_size = STORE_SIZE;
    opts.meta_size= STORE_SIZE;
    opts.meta_max_key_len = KEY_LEN;
    opts.meta_max_val_len = VAL_LEN;
    uint8_t error;
    pmb_handle *handle = pmb_open(&opts, &error);
    if (error != PMB_OK) {
        printf("!!! ERROR %d !!!\n", error);

        exit(1);
    }

    pmb_pair p, m;
    p.blk_id = 0;
    void *key = malloc(opts.max_key_len);
    void *val = malloc(MAX_VAL_LEN);
    memset(val, 0, MAX_VAL_LEN);

    // get itrator
    pmb_iter *iter = pmb_iter_open(handle, 0);
    if (iter != NULL) {
        while(pmb_iter_valid(iter)) {
            pmb_iter_get(iter, &p);
            if (p.blk_id != 0)
                printf("saved object: id %zu key %s\n", p.blk_id, (char *)p.key);
            pmb_iter_next(iter);
            p.blk_id = 0;
        }
        pmb_iter_close(iter);
    }

    p.offset = 0;
    p.key = key;
    p.val = val;
    p.key_len = KEY_LEN;
    p.val_len = VAL_LEN;

    m.blk_id = 0;
    m.offset = 0;
    m.key = key;
    m.val = val;
    m.key_len = KEY_LEN;
    m.val_len = VAL_LEN;

    uint64_t blk_id;
    uint64_t m_blk_id;
    uint64_t tx_id;
    pmb_tx_begin(handle, &tx_id);
    for(int i = 0; i < (MAX_VAL_LEN) / (VAL_LEN); i++) {
        snprintf(key, KEY_LEN, "%0126d", i);
        snprintf(val, VAL_LEN, "%0131d", i);
        if (pmb_tput(handle, tx_id, &p) == PMB_OK) {
            blk_id = p.blk_id;
            printf("saved data to blk_id: %zu offset %u\n", p.blk_id, p.offset);
            p.offset += VAL_LEN;
        } else {
            blk_id = 0;
            printf("put data failed for: %d\n", i);
        }
        if (pmb_tput_meta(handle, tx_id, &m) == PMB_OK) {
            printf("saved meta to blk_id: %zu offset %u\n", p.blk_id, p.offset);
            m.offset += VAL_LEN;
            m_blk_id = m.blk_id;
        } else {
            printf("put meta failed for: %d\n", i);
        }
    }
    pmb_tx_commit(handle, tx_id);
    pmb_tx_execute(handle, tx_id);

    pmb_get(handle, blk_id, &p);
    printf("id: %zu key %s val_len %u val %s\n", blk_id, (char *)p.key, p.val_len, (char*)p.val);

    pmb_get(handle, m_blk_id, &m);
    printf("id: %zu key %s val_len %u val %s\n", m_blk_id, (char *)m.key, m.val_len, (char*)m.val);

    free(key);
    free(val);

    pmb_close(handle);

    return 0;
}
