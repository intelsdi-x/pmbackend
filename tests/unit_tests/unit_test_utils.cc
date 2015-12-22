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
 * iterate through the handle and return number of objects handled
 */
uint64_t
count(pmb_handle* handle, uint8_t region){
	return pmb_ntotal(handle, region) - pmb_nfree(handle, region);
}

/*
 * open or create handle and return error/success code
 */
int
open_handle(pmb_handle*& handle, int size, std::string path, uint32_t max_key_len, uint32_t max_val_len, uint8_t write_log_entries) {
	pmb_opts opts;
	opts.max_key_len = max_key_len;
	opts.max_val_len = max_val_len;
	opts.write_log_entries = write_log_entries;
	opts.path = path.c_str();
	opts.data_size = size * 1024UL * 1024 * 1024;
	opts.meta_size = size * 1024UL * 1024;
	opts.meta_max_key_len = max_key_len;
	opts.meta_max_val_len = max_val_len;
	opts.sync_type = PMB_SYNC;
	uint8_t error = 0;
	handle = pmb_open(&opts, &error);

	return error;
}

pmb_handle* create_handle(void) {
	pmb_handle* handle;
	EXPECT_EQ(PMB_OK, open_handle(handle, 4UL, "single_thread.pool"));
	EXPECT_TRUE(NULL!=handle);
	EXPECT_EQ(0, count(handle, PMB_DATA));
	EXPECT_EQ(0, count(handle, PMB_META));
	EXPECT_GT(pmb_nfree(handle, PMB_DATA), 0);
	EXPECT_GT(pmb_nfree(handle, PMB_META), 0);
	EXPECT_GT(pmb_ntotal(handle, PMB_DATA), 0);
	EXPECT_GT(pmb_ntotal(handle, PMB_META), 0);
	return handle;
}

void remove_handle(pmb_handle* handle) {
	EXPECT_EQ(PMB_OK, pmb_close(handle));
	EXPECT_EQ(0, remove("single_thread.pool"));
}

pmb_pair generate_put_input(uint64_t obj_id, uint32_t offset, void* key, void* val, uint32_t key_len, uint32_t val_len) {

	pmb_pair to_put;
	to_put.obj_id = obj_id;
	to_put.offset = offset;
	to_put.key = key;
	to_put.val = val;
	to_put.key_len = key_len;
	to_put.val_len = val_len;

	return to_put;
}

pmb_handle* prepare_handle_for_iter(void) {
	pmb_handle* handle = create_handle();
	pmb_pair to_put = generate_put_input();
	uint64_t tx_slot;
	EXPECT_EQ(PMB_OK, pmb_tx_begin(handle, &tx_slot));
	EXPECT_EQ(PMB_OK, pmb_tput(handle,  tx_slot, &to_put));
	EXPECT_EQ(PMB_OK, pmb_tx_commit(handle, tx_slot));
	EXPECT_EQ(PMB_OK, pmb_tx_execute(handle, tx_slot));
	EXPECT_EQ(PMB_OK, pmb_close(handle));
	EXPECT_EQ(PMB_OK, open_handle(handle,4, "single_thread.pool"));
	EXPECT_TRUE(NULL!=handle);
	EXPECT_EQ(1, count(handle, PMB_DATA));
	return handle;
}

void validate_save(pmb_handle* handle, uint64_t obj_id, pmb_pair inserted) {
	pmb_pair readed;
	printf("Validating %zu\n", obj_id);
	EXPECT_EQ(PMB_OK, pmb_get(handle, obj_id, &readed));
	EXPECT_STREQ((char *)readed.key, (char *)inserted.key);
	EXPECT_STREQ((char *)readed.val, (char *)inserted.val);
	EXPECT_EQ(readed.key_len, inserted.key_len);
	EXPECT_EQ(readed.val_len, inserted.val_len);
	EXPECT_EQ(readed.offset, 0);
	EXPECT_EQ(readed.obj_id, obj_id);
}
