/*
 * Copyright (c) 2015-2016, Intel Corporation
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


TEST(TPutMeta, ReturnErrorCauseValLenIsTooBig) {
	pmb_handle *handle = create_handle();
	pmb_pair to_put = generate_put_input(0, 0, (void *)"5", (void *)"6", MAX_KEY_LEN, MAX_VAL_LEN+100);
	uint64_t tx_slot;
	EXPECT_EQ(PMB_OK, pmb_tx_begin(handle, &tx_slot));
	EXPECT_EQ(PMB_ESIZE, pmb_tput_meta(handle, tx_slot, &to_put));
	EXPECT_EQ(0, count(handle, PMB_META));
	remove_handle(handle);
}


TEST(TPutMeta, ReturnErrorCauseKeyLenIsTooBig) {
	pmb_handle *handle = create_handle();
	pmb_pair to_put = generate_put_input(0, 0, (void *)"5", (void *)"6", MAX_KEY_LEN+100, MAX_VAL_LEN);
	uint64_t tx_slot;

	EXPECT_EQ(PMB_OK, pmb_tx_begin(handle, &tx_slot));
	EXPECT_EQ(PMB_ESIZE, pmb_tput_meta(handle, tx_slot, &to_put));
	EXPECT_EQ(0, count(handle, PMB_META));

	remove_handle(handle);
}


TEST(TPutMeta, ReturnErrorCauseKeyLenIs0) {
	pmb_handle *handle = create_handle();
	pmb_pair to_put = generate_put_input(0, 0, (void *)"5", (void *)"6", 0, MAX_VAL_LEN);
	uint64_t tx_slot;

	EXPECT_EQ(PMB_OK, pmb_tx_begin(handle, &tx_slot));
	EXPECT_EQ(PMB_ERR, pmb_tput_meta(handle, tx_slot, &to_put));
	EXPECT_EQ(0, count(handle, PMB_META));

	remove_handle(handle);
}

TEST(TPutMeta, ReturnErrorCauseValueIsNullAndItsNotTouch) {
	pmb_handle *handle = create_handle();
	pmb_pair to_put = generate_put_input(0, 0, (void *)"5", NULL, MAX_KEY_LEN, MAX_VAL_LEN);
	uint64_t tx_slot;

	EXPECT_EQ(PMB_OK, pmb_tx_begin(handle, &tx_slot));
	EXPECT_EQ(PMB_ERR, pmb_tput_meta(handle, tx_slot, &to_put));
	EXPECT_EQ(0, count(handle, PMB_META));

	remove_handle(handle);
}

TEST(TPutMeta, ReturnErrorCauseKeyIsNull) {
	pmb_handle *handle = create_handle();
	pmb_pair to_put = generate_put_input(0, 0, NULL, (void *)"6", MAX_KEY_LEN, MAX_VAL_LEN);
	uint64_t tx_slot;

	EXPECT_EQ(PMB_OK, pmb_tx_begin(handle, &tx_slot));
	EXPECT_EQ(PMB_ERR, pmb_tput_meta(handle, tx_slot, &to_put));
	EXPECT_EQ(0, count(handle, PMB_META));

	remove_handle(handle);
}

TEST(TPutMeta, ReturnErrorCauseThereIsNoFreeObjectLeft) {
  	pmb_handle *handle = create_handle();
	pmb_pair to_put = generate_put_input();
	uint64_t tx_slot;

	EXPECT_EQ(PMB_OK, pmb_tx_begin(handle, &tx_slot));
	handle->meta_free_list->begin = 0;
	handle->meta_free_list->end = 0;
	handle->meta_free_list->size = 0; // lest empty free list

	EXPECT_EQ(PMB_ENOSPC, pmb_tput_meta(handle, tx_slot, &to_put));

	remove_handle(handle);
}

TEST(TPutMeta, SuccessfullyWriteNewData) {
	pmb_handle *handle = create_handle();
	pmb_pair to_put = generate_put_input();
	uint64_t tx_slot;

	EXPECT_EQ(PMB_OK, pmb_tx_begin(handle, &tx_slot));
	EXPECT_EQ(PMB_OK, pmb_tput_meta(handle, tx_slot, &to_put));
	EXPECT_EQ(PMB_OK, pmb_tx_commit(handle, tx_slot));
	EXPECT_EQ(PMB_OK, pmb_tx_execute(handle, tx_slot));

	validate_save(handle, to_put.obj_id, to_put);
	EXPECT_EQ(1, count(handle, PMB_META));

	remove_handle(handle);
}

TEST(TPutMeta, ReturnErrorCauseObjectToUpdateIsDataObject) {
	pmb_handle *handle = create_handle();
	pmb_pair to_put = generate_put_input();
	to_put.obj_id = 1030;
	uint64_t tx_slot;

	EXPECT_EQ(PMB_OK, pmb_tx_begin(handle, &tx_slot));
	EXPECT_EQ(PMB_EWRGID, pmb_tput_meta(handle, tx_slot, &to_put));
	EXPECT_EQ(0, count(handle, PMB_META));

	remove_handle(handle);
}

TEST(TPutMeta, ReturnErrorCauseObjectToUpdateDoesntHavePreviousVersion) {
	pmb_handle *handle = create_handle();
	pmb_pair to_put = generate_put_input();
	to_put.obj_id = 983038;
	uint64_t tx_slot;

	EXPECT_EQ(PMB_OK, pmb_tx_begin(handle, &tx_slot));
	EXPECT_EQ(PMB_ENOENT, pmb_tput_meta(handle, tx_slot, &to_put));
	EXPECT_EQ(0, count(handle, PMB_META));

	remove_handle(handle);
}

TEST(TPutMeta, SuccessfullyUpdateObjectWithObjectOfTheSameSize) {
	pmb_handle *handle = create_handle();
	pmb_pair to_put = generate_put_input();
	pmb_pair readed;

	uint64_t tx_slot;
	EXPECT_EQ(PMB_OK, pmb_tx_begin(handle, &tx_slot));
	EXPECT_EQ(PMB_OK, pmb_tput_meta(handle, tx_slot, &to_put));
	EXPECT_EQ(PMB_OK, pmb_tx_commit(handle, tx_slot));
	EXPECT_EQ(PMB_OK, pmb_tx_execute(handle, tx_slot));
	validate_save(handle, to_put.obj_id, to_put);
	EXPECT_EQ(1, count(handle, PMB_META));
	EXPECT_GT(to_put.obj_id, 0);
	uint64_t obj_id = to_put.obj_id;

	to_put.val = (void *)"1234321";
	EXPECT_EQ(PMB_OK, pmb_tx_begin(handle, &tx_slot));

	EXPECT_EQ(PMB_OK, pmb_tput_meta(handle, tx_slot, &to_put));
	EXPECT_EQ(PMB_OK, pmb_tx_commit(handle, tx_slot));
	EXPECT_EQ(PMB_OK, pmb_tx_execute(handle, tx_slot));

	validate_save(handle, to_put.obj_id, to_put);
    EXPECT_NE(to_put.obj_id, obj_id);
	EXPECT_EQ(PMB_ENOENT, pmb_get(handle, obj_id, &readed));
	EXPECT_EQ(1, count(handle, PMB_META));

	remove_handle(handle);
}

TEST(TPutMeta, SuccessfullyUpdateObjectWithObjectOfTheSmallerSize) {
	std::string oval1 = std::string(1024, '1');
	std::string oval2 = std::string(768, '2');
	void *val1 = (void *)oval1.c_str();
	void *val2 = (void *)oval2.c_str();

	pmb_handle *handle = create_handle();
	pmb_pair to_put = generate_put_input(0, 0, (void *)"key", val1);
	pmb_pair readed;

	uint64_t tx_slot;
	EXPECT_EQ(PMB_OK, pmb_tx_begin(handle, &tx_slot));
	EXPECT_EQ(PMB_OK, pmb_tput_meta(handle, tx_slot, &to_put));
	EXPECT_EQ(PMB_OK, pmb_tx_commit(handle, tx_slot));
	EXPECT_EQ(PMB_OK, pmb_tx_execute(handle, tx_slot));
	validate_save(handle, to_put.obj_id, to_put);
	EXPECT_EQ(1, count(handle, PMB_META));
	EXPECT_GT(to_put.obj_id, 0);
	uint64_t obj_id = to_put.obj_id;

	to_put.val_len = MAX_VAL_LEN - 256;
	to_put.val = val2;
	EXPECT_EQ(PMB_OK, pmb_tx_begin(handle, &tx_slot));

	EXPECT_EQ(PMB_OK, pmb_tput_meta(handle, tx_slot, &to_put));
	EXPECT_EQ(PMB_OK, pmb_tx_commit(handle, tx_slot));
	EXPECT_EQ(PMB_OK, pmb_tx_execute(handle, tx_slot));

	EXPECT_NE(to_put.obj_id, obj_id);
	EXPECT_EQ(PMB_ENOENT, pmb_get(handle, obj_id, &readed));
	EXPECT_EQ(1, count(handle, PMB_META));
	validate_save(handle, to_put.obj_id, to_put);

	remove_handle(handle);
}

TEST(TPutMeta, SuccessfullyUpdateObjectWithObjectOfTheBiggerSize) {
	pmb_handle *handle = create_handle();
	pmb_pair to_put = generate_put_input(0, 0, (void *)"120", (void *)"632", MAX_KEY_LEN, MAX_VAL_LEN-256);
	pmb_pair readed;

	uint64_t tx_slot;
	EXPECT_EQ(PMB_OK, pmb_tx_begin(handle, &tx_slot));
	EXPECT_EQ(PMB_OK, pmb_tput_meta(handle, tx_slot, &to_put));
	EXPECT_EQ(PMB_OK, pmb_tx_commit(handle, tx_slot));
	EXPECT_EQ(PMB_OK, pmb_tx_execute(handle, tx_slot));
	validate_save(handle, to_put.obj_id, to_put);
	EXPECT_EQ(1, count(handle, PMB_META));
	EXPECT_GT(to_put.obj_id, 0);
	uint64_t obj_id = to_put.obj_id;

	to_put.val_len = MAX_VAL_LEN;
	to_put.val = (void *)"1234321";
	EXPECT_EQ(PMB_OK, pmb_tx_begin(handle, &tx_slot));

	EXPECT_EQ(PMB_OK, pmb_tput_meta(handle, tx_slot, &to_put));
	EXPECT_EQ(PMB_OK, pmb_tx_commit(handle, tx_slot));
	EXPECT_EQ(PMB_OK, pmb_tx_execute(handle, tx_slot));

	validate_save(handle, to_put.obj_id, to_put);
	EXPECT_NE(to_put.obj_id, obj_id);
	EXPECT_EQ(PMB_ENOENT, pmb_get(handle, obj_id, &readed));
	EXPECT_EQ(1, count(handle, PMB_META));

	remove_handle(handle);
}
