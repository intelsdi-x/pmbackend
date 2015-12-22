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

#include <iostream>

#include <gtest/gtest.h>

#include "pmbackend.h"
#include "kv.h"

#define ARRAY_SIZE(x) sizeof(x)/sizeof(*x)

TEST(Fuzzing, pmb_close) {
    pmb_handle handle;
    pmb_handle *params[] = {
            NULL
    };
    for (int set = 0; set < ARRAY_SIZE(params); set++) {
        std::cout << "\n running pmb_close with handle: " << params[set] << std::endl;
        EXPECT_EQ(PMB_ERR, pmb_close(params[set]));
    }
}

TEST(Fuzzing, pmb_get) {
    struct func_params {
        pmb_handle *handle;
        uint64_t obj_id;
        pmb_pair *kv;
    };
    pmb_pair readed;
    pmb_handle handle;
    struct func_params params[] = {
            {.handle = NULL, .obj_id = 0, .kv = NULL},
            {.handle = NULL, .obj_id = 0, .kv = &readed},
            {.handle = NULL, .obj_id = 10, .kv = NULL},
            {.handle = NULL, .obj_id = 10, .kv = &readed},
            {.handle = &handle, .obj_id = 0, .kv = NULL},
            {.handle = &handle, .obj_id = 10190980980, .kv = NULL},
    };
    for (int set = 0; set < ARRAY_SIZE(params); set++) {
        std::cout << "\n running pmb_get with handle: " << params[set].handle << " obj_id: " << params[set].obj_id <<
        " kv: " << params[set].kv << std::endl;
        EXPECT_EQ(PMB_ERR, pmb_get(params[set].handle, params[set].obj_id, params[set].kv));
    }
}


TEST(Fuzzing, pmb_iter) {
    struct func_params {
        pmb_handle *handle;
        int region;
    };
    pmb_handle handle;
    struct func_params params[] = {
            {.handle = NULL, .region = 0},
            {.handle = NULL, .region = 1},
            {.handle = NULL, .region = -1},
    };
    for (int set = 0; set < ARRAY_SIZE(params); set++) {
        std::cout << "\n running pmb_iter with handle: " << params[set].handle << " region: " <<
        params[set].region << std::endl;
        pmb_iter* iter = pmb_iter_open(params[set].handle, params[set].region);
        EXPECT_TRUE(iter == NULL);
        if (iter != NULL) {
            pmb_iter_close(iter);
        }
    }
}


TEST(Fuzzing, pmb_iter_close) {
    pmb_iter iter;
    pmb_iter *params[] = {
            NULL
    };
    for (int set = 0; set < ARRAY_SIZE(params); set++) {
        std::cout << "\n running pmb_close with iter: " << params[set] << std::endl;
        EXPECT_EQ(PMB_ERR, pmb_iter_close(params[set]));
    }
}


TEST(Fuzzing, pmb_iter_get) {
    struct func_params {
        pmb_iter *iter;
        pmb_pair *pair;
    };
    pmb_iter iter;
    pmb_pair pair;
    struct func_params params[] = {
            {.iter = NULL, .pair = NULL},
            {.iter = NULL, .pair = &pair},
            {.iter = &iter, .pair = NULL},
    };
    for (int set = 0; set < ARRAY_SIZE(params); set++) {
        std::cout << "\n running pmb_iter_get with iter: " << params[set].iter << " kv: " << params[set].pair <<
        std::endl;
        EXPECT_EQ(PMB_ERR, pmb_iter_get(params[set].iter, params[set].pair));
    }
}


TEST(Fuzzing, pmb_iter_next) {
    pmb_iter iter;
    pmb_iter *params[] = {
            NULL
    };
    for (int set = 0; set < ARRAY_SIZE(params); set++) {
        std::cout << "\n running pmb_iter_next with iter: " << params[set] << std::endl;
        EXPECT_EQ(PMB_ERR, pmb_iter_next(params[set]));
    }
}


TEST(Fuzzing, pmb_iter_pos) {
    pmb_iter iter;
    pmb_iter *params[] = {
            NULL
    };
    for (int set = 0; set < ARRAY_SIZE(params); set++) {
        std::cout << "\n running pmb_iter_pos with iter: " << params[set] << std::endl;
        EXPECT_EQ(0, pmb_iter_pos(params[set]));
    }
}


TEST(Fuzzing, pmb_iter_valid) {
    pmb_iter iter;
    pmb_iter *params[] = {
            NULL
    };
    for (int set = 0; set < ARRAY_SIZE(params); set++) {
        std::cout << "\n running pmb_iter_valid with iter: " << params[set] << std::endl;
        EXPECT_FALSE(pmb_iter_valid(params[set]));
    }
}


TEST(Fuzzing, pmb_nfree) {
    struct func_params {
        pmb_handle *handle;
        int region;
    };
    pmb_handle handle;
    struct func_params params[] = {
            {.handle = NULL, .region = 0},
            {.handle = NULL, .region = 1},
            {.handle = NULL, .region = -1},
    };
    for (int set = 0; set < ARRAY_SIZE(params); set++) {
        std::cout << "\n running pmb_nfree with handle: " << params[set].handle << " region: " <<
                params[set].region << std::endl;
        EXPECT_EQ(0, pmb_nfree(params[set].handle, params[set].region));
    }
}


TEST(Fuzzing, pmb_ntotal) {
    struct func_params {
        pmb_handle *handle;
        int meta_size;
    };
    pmb_handle handle;
    struct func_params params[] = {
            {.handle = NULL, .meta_size = 0},
            {.handle = NULL, .meta_size = 1},
            {.handle = NULL, .meta_size = -1},
    };
    for (int set = 0; set < ARRAY_SIZE(params); set++) {
        std::cout << "\n running pmb_ntotal with handle: " << params[set].handle << " meta_size: " <<
        params[set].meta_size << std::endl;
        EXPECT_EQ(0, pmb_ntotal(params[set].handle, params[set].meta_size));
    }
}


//TODO: pmb_open

TEST(Fuzzing, pmb_tdel) {
    struct func_params {
        pmb_handle *handle;
        uint64_t obj_id;
        uint64_t tx_slot;
    };
    pmb_handle invalid_handle;
    struct func_params params[] = {
            {.handle = NULL, .obj_id = 0, .tx_slot = 0},
            {.handle = NULL, .obj_id = 0, .tx_slot = 1},
            {.handle = NULL, .obj_id = 0, .tx_slot = -1},
            {.handle = NULL, .obj_id = 10, .tx_slot = 0},
            {.handle = NULL, .obj_id = 10, .tx_slot = 1},
            {.handle = NULL, .obj_id = 10, .tx_slot = -1},
            {.handle = &invalid_handle, .obj_id = 0, .tx_slot = 0},
            {.handle = &invalid_handle, .obj_id = 0, .tx_slot = -1},
            {.handle = &invalid_handle, .obj_id = 10, .tx_slot = 0},
            {.handle = &invalid_handle, .obj_id = 10, .tx_slot = -1},
    };
    for (int set = 0; set < ARRAY_SIZE(params); set++) {
        std::cout << "\n running pmb_tdel with handle: " << params[set].handle << " obj_id: " << params[set].obj_id <<
                " tx_slot: " << params[set].tx_slot << std::endl;
        EXPECT_EQ(PMB_ERR, pmb_tdel(params[set].handle, params[set].tx_slot, params[set].obj_id));
    }
}

TEST(Fuzzing, pmb_tput) {
    struct func_params {
        pmb_handle *handle;
        pmb_pair *kv;
        uint64_t tx_slot;
    };
    pmb_pair to_put;
    pmb_handle invalid_handle;
    struct func_params params[] = {
            {.handle = NULL, .kv = NULL, .tx_slot = 0},
            {.handle = NULL, .kv = &to_put, .tx_slot = 0},
            {.handle = &invalid_handle, .kv = NULL, .tx_slot = 0},
            {.handle = &invalid_handle, .kv = &to_put, .tx_slot = 0},
            {.handle = NULL, .kv = NULL, .tx_slot = 1},
            {.handle = NULL, .kv = &to_put, .tx_slot = 1},
            {.handle = &invalid_handle, .kv = NULL, .tx_slot = 1},
            {.handle = NULL, .kv = NULL, .tx_slot = -1},
            {.handle = NULL, .kv = &to_put, .tx_slot = -1},
            {.handle = &invalid_handle, .kv = NULL, .tx_slot = -1},
    };
    for (int set = 0; set < ARRAY_SIZE(params); set++) {
        std::cout << "\n running pmb_tput with handle: " << params[set].handle << " kv: " << params[set].kv <<
                " tx_slot: " << params[set].tx_slot << std::endl;
        EXPECT_EQ(PMB_ERR, pmb_tput(params[set].handle, params[set].tx_slot, params[set].kv));
    }
}

TEST(Fuzzing, pmb_tx_abort) {
    struct func_params {
        pmb_handle *handle;
        uint64_t tx_slot;
    };
    pmb_handle invalid_handle;
    struct func_params params[] = {
            {.handle = NULL, .tx_slot = 0},
            {.handle = NULL, .tx_slot = 1},
            {.handle = NULL, .tx_slot = -1},
            {.handle = &invalid_handle, .tx_slot = 0},
    };
    for (int set = 0; set < ARRAY_SIZE(params); set++) {
        std::cout << "\n running pmb_tx_abort with handle: " << params[set].handle << " tx_slot: " <<
                params[set].tx_slot << std::endl;
        EXPECT_EQ(PMB_ERR, pmb_tx_abort(params[set].handle, params[set].tx_slot));
    }
}

TEST(Fuzzing, pmb_tx_begin) {
    struct func_params {
        pmb_handle *handle;
        uint64_t *tx_slot;
    };
    uint64_t *tx_slot_ptr;
    pmb_handle invalid_handle;
    struct func_params params[] = {
            {.handle = NULL, .tx_slot = NULL},
            {.handle = NULL, .tx_slot = tx_slot_ptr},
            {.handle = &invalid_handle, .tx_slot = NULL},
    };
    for (int set = 0; set < ARRAY_SIZE(params); set++) {
        std::cout << "\n running pmb_tx_begin with handle: " << params[set].handle << " tx_slot: " <<
                params[set].tx_slot << std::endl;
        EXPECT_EQ(PMB_ERR, pmb_tx_begin(params[set].handle, params[set].tx_slot));
    }
}

TEST(Fuzzing, pmb_tx_commit) {
    struct func_params {
        pmb_handle *handle;
        uint64_t tx_slot;
    };
    pmb_handle invalid_handle;
    struct func_params params[] = {
            {.handle = NULL, .tx_slot = 0},
            {.handle = NULL, .tx_slot = 1},
            {.handle = NULL, .tx_slot = -1},
            {.handle = &invalid_handle, .tx_slot = 0},
    };
    for (int set = 0; set < ARRAY_SIZE(params); set++) {
        std::cout << "\n running pmb_tx_commit with handle: " << params[set].handle << " tx_slot: " <<
                params[set].tx_slot << std::endl;
        EXPECT_EQ(PMB_ERR, pmb_tx_commit(params[set].handle, params[set].tx_slot));
    }
}

TEST(Fuzzing, pmb_tx_execute) {
    struct func_params {
        pmb_handle *handle;
        uint64_t tx_slot;
    };
    pmb_handle invalid_handle;
    struct func_params params[] = {
            {.handle = NULL, .tx_slot = 0},
            {.handle = NULL, .tx_slot = 1},
            {.handle = NULL, .tx_slot = -1},
            {.handle = &invalid_handle, .tx_slot = 0},
    };
    for (int set = 0; set < ARRAY_SIZE(params); set++) {
        std::cout << "\n running pmb_tx_execute with handle: " << params[set].handle << " tx_slot: " <<
                params[set].tx_slot << std::endl;
        EXPECT_EQ(PMB_ERR, pmb_tx_execute(params[set].handle, params[set].tx_slot));
    }
}

