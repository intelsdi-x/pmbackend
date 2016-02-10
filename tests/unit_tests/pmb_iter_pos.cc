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

#include "unit_test_utils.h"

/*
 * Succesfully get iterator position
 */
TEST(IterPos, Succes) {
	pmb_handle *handle = create_handle();
	pmb_pair to_put = generate_put_input();
	uint64_t tx_slot;
	EXPECT_EQ(PMB_OK, pmb_tx_begin(handle, &tx_slot));
	EXPECT_EQ(PMB_OK, pmb_tput(handle,  tx_slot, &to_put));
	EXPECT_EQ(PMB_OK, pmb_tx_commit(handle, tx_slot));
	EXPECT_EQ(PMB_OK, pmb_tx_execute(handle, tx_slot));
	EXPECT_EQ(PMB_OK, pmb_close(handle));
	handle = NULL;
	EXPECT_EQ(PMB_OK, open_handle(handle, 4, "single_thread.pool"));
	EXPECT_TRUE(NULL!=handle);
	EXPECT_EQ(1, count(handle, PMB_DATA));

	pmb_iter *iter = pmb_iter_open(handle, 0); // get iterator

	uint64_t position =	pmb_iter_pos(iter); // get position
	EXPECT_GT(position, 0); // position cant point on superblock or tx_log
	EXPECT_EQ(position, to_put.blk_id);  // position is equal blk_id cause there is only one item in handle

	EXPECT_EQ(PMB_OK, pmb_iter_close(iter));

	remove_handle(handle);
}
