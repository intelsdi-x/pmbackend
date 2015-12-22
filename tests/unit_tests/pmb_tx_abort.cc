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
 * Successfully abort transaction
 */
TEST(TxAbort, Success) {
	uint64_t tx_slot;
	pmb_handle *handle = create_handle();
	EXPECT_EQ(PMB_OK, pmb_tx_begin(handle, &tx_slot));
	EXPECT_TRUE(0<tx_slot && tx_slot<=handle->op_log.tx_slots_count);
	EXPECT_EQ(PMB_OK, pmb_tx_abort(handle, tx_slot));
	remove_handle(handle);
}

/*
 * Successfully abort transaction
 */
TEST(TxAbort, SuccessUpdate) {
	uint64_t tx_slot;
	pmb_handle *handle = create_handle();
	pmb_pair to_put = generate_put_input();
	pmb_pair to_update = generate_put_input();
	pmb_pair readed;

	EXPECT_EQ(PMB_OK, pmb_tx_begin(handle, &tx_slot));
	EXPECT_EQ(PMB_OK, pmb_tput(handle, tx_slot, &to_put));
	EXPECT_EQ(PMB_OK, pmb_tx_commit(handle, tx_slot));
	EXPECT_EQ(PMB_OK, pmb_tx_execute(handle, tx_slot));
	EXPECT_GT(to_put.obj_id, 0);

	EXPECT_EQ(PMB_OK, pmb_tx_begin(handle, &tx_slot));
	EXPECT_TRUE(0<tx_slot && tx_slot<=handle->op_log.tx_slots_count);

	to_update.val = (char *)"Updated value";
	to_update.obj_id = to_put.obj_id;

	EXPECT_EQ(PMB_OK, pmb_tput(handle,  tx_slot, &to_update));
	validate_save(handle, to_update.obj_id, to_update);
	validate_save(handle, to_put.obj_id, to_put);

	EXPECT_EQ(PMB_OK, pmb_tx_abort(handle, tx_slot));
	EXPECT_EQ(PMB_ENOENT, pmb_get(handle, to_update.obj_id, &readed));
	validate_save(handle, to_put.obj_id, to_put);

	remove_handle(handle);
}

/*
 * Successfully abort transaction
 */
TEST(TxAbort, SuccessWrite) {
	uint64_t tx_slot;
	pmb_pair readed;
	pmb_handle *handle = create_handle();
	EXPECT_EQ(PMB_OK, pmb_tx_begin(handle, &tx_slot));
	EXPECT_TRUE(0<tx_slot && tx_slot<=handle->op_log.tx_slots_count);
	pmb_pair to_put = generate_put_input();

	EXPECT_EQ(PMB_OK, pmb_tput(handle,  tx_slot, &to_put));
	EXPECT_EQ(PMB_OK, pmb_tx_abort(handle, tx_slot));

	EXPECT_EQ(PMB_ENOENT, pmb_get(handle, to_put.obj_id, &readed));
	remove_handle(handle);
}

/*
 * Successfully abort transaction
 */
TEST(TxAbort, SuccessDelete) {
	uint64_t tx_slot;
	pmb_handle *handle = create_handle();
	pmb_pair to_put = generate_put_input();

	EXPECT_EQ(PMB_OK, pmb_tx_begin(handle, &tx_slot));
	EXPECT_EQ(PMB_OK, pmb_tput(handle,  tx_slot, &to_put));
	EXPECT_EQ(PMB_OK, pmb_tx_commit(handle, tx_slot));
	EXPECT_EQ(PMB_OK, pmb_tx_execute(handle, tx_slot));

	EXPECT_EQ(PMB_OK, pmb_tx_begin(handle, &tx_slot));
	EXPECT_TRUE(0<tx_slot && tx_slot<=handle->op_log.tx_slots_count);

	EXPECT_EQ(PMB_OK, pmb_tdel(handle, tx_slot, to_put.obj_id));
	validate_save(handle, to_put.obj_id, to_put);

	EXPECT_EQ(PMB_OK, pmb_tx_abort(handle, tx_slot));
	validate_save(handle, to_put.obj_id, to_put);

	remove_handle(handle);
}