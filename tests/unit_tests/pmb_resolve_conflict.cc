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

#include <gtest/gtest.h>
#include <pmbackend.h>

#include "unit_test_utils.h"

//pmb_pair to_put = generate_put_input(0, 0, (void *)"5", (void *)"6", MAX_KEY_LEN/8, MAX_VAL_LEN/8);

TEST(ResolveConflict, MissinBothObjects) {
    pmb_handle *handle = create_handle();

    EXPECT_EQ(PMB_ERR, pmb_resolve_conflict(handle, 10, 120));

    remove_handle(handle);
}


TEST(ResolveConflict, MissingOneObject) {
    pmb_handle *handle = create_handle();
    pmb_pair to_put = generate_put_input();

    uint64_t tx_slot;
    EXPECT_EQ(PMB_OK, pmb_tx_begin(handle, &tx_slot));
    EXPECT_EQ(PMB_OK, pmb_tput(handle,  tx_slot, &to_put));
    EXPECT_EQ(PMB_OK, pmb_tx_commit(handle, tx_slot));
    EXPECT_EQ(PMB_OK, pmb_tx_execute(handle, tx_slot));

    EXPECT_EQ(PMB_ERR, pmb_resolve_conflict(handle, to_put.blk_id, 123));

    validate_save(handle, to_put.blk_id, to_put);

    remove_handle(handle);
}

TEST(DISABLED_ResolveConflict, SecondObjectVersionIsValid) {
    pmb_handle *handle = create_handle();
    pmb_pair to_put = generate_put_input();
    pmb_pair to_put2 = generate_put_input();
    pmb_pair to_read;

    uint64_t tx_slot;
    EXPECT_EQ(PMB_OK, pmb_tx_begin(handle, &tx_slot));
    EXPECT_EQ(PMB_OK, pmb_tput(handle,  tx_slot, &to_put));
    to_put.blk_id = 0;
    EXPECT_EQ(PMB_OK, pmb_tput(handle,  tx_slot, &to_put));
    EXPECT_EQ(PMB_OK, pmb_tx_commit(handle, tx_slot));
    EXPECT_EQ(PMB_OK, pmb_tx_execute(handle, tx_slot));

    EXPECT_EQ(PMB_OK, pmb_tx_begin(handle, &tx_slot));
    EXPECT_EQ(PMB_OK, pmb_tput(handle,  tx_slot, &to_put2));
    EXPECT_EQ(PMB_OK, pmb_tx_commit(handle, tx_slot));
    EXPECT_EQ(PMB_OK, pmb_tx_execute(handle, tx_slot));

    EXPECT_EQ(to_put2.blk_id, pmb_resolve_conflict(handle, to_put.blk_id, to_put2.blk_id));

//    validate_save(handle, to_put2.blk_id, to_put2);
    EXPECT_EQ(PMB_ENOENT, pmb_get(handle, to_put.blk_id, &to_read));

    remove_handle(handle);
}

TEST(DISABLED_ResolveConflict, FirstObjectVersionIsValid) {
    pmb_handle *handle = create_handle();
    pmb_pair to_put = generate_put_input();
    pmb_pair to_put2 = generate_put_input();
    pmb_pair to_read;

    uint64_t tx_slot;

    EXPECT_EQ(PMB_OK, pmb_tx_begin(handle, &tx_slot));
    EXPECT_EQ(PMB_OK, pmb_tput(handle,  tx_slot, &to_put));
    EXPECT_EQ(PMB_OK, pmb_tx_commit(handle, tx_slot));
    EXPECT_EQ(PMB_OK, pmb_tx_execute(handle, tx_slot));

    EXPECT_EQ(PMB_OK, pmb_tx_begin(handle, &tx_slot));
    EXPECT_EQ(PMB_OK, pmb_tput(handle,  tx_slot, &to_put2));
    EXPECT_EQ(PMB_OK, pmb_tx_commit(handle, tx_slot));
    EXPECT_EQ(PMB_OK, pmb_tx_execute(handle, tx_slot));

    to_put2.blk_id = 0;

    EXPECT_EQ(PMB_OK, pmb_tx_begin(handle, &tx_slot));
    EXPECT_EQ(PMB_OK, pmb_tput(handle,  tx_slot, &to_put));
    EXPECT_EQ(PMB_OK, pmb_tx_commit(handle, tx_slot));
    EXPECT_EQ(PMB_OK, pmb_tx_execute(handle, tx_slot));

    EXPECT_EQ(to_put.blk_id, pmb_resolve_conflict(handle, to_put.blk_id, to_put2.blk_id));

    validate_save(handle, to_put.blk_id, to_put2);
    EXPECT_EQ(PMB_ENOENT, pmb_get(handle, to_put2.blk_id, &to_read));

    remove_handle(handle);
}
