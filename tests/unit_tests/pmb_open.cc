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

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

/*
 * Fail on creating new handle cause path is null
 */
TEST(OpenHandle, CreateFailNullPath) {
	pmb_handle *handle;
	pmb_opts opts;
	uint8_t error;

	opts.max_key_len = MAX_KEY_LEN;
	opts.max_val_len = MAX_VAL_LEN;
	opts.meta_max_key_len = MAX_KEY_LEN;
	opts.meta_max_val_len = MAX_VAL_LEN;
	opts.write_log_entries = 16;
	opts.path = NULL;
	opts.data_size = 4 * 1024 * 1024;

	handle = pmb_open(&opts, &error);

    EXPECT_EQ(PMB_ECREAT, error);
    EXPECT_EQ(NULL, handle);
}

/*
 * Fail on creating new handle cause path to handle doesn't exist
 */
TEST(OpenHandle, CreateFailPathDoesntExist) {
    pmb_handle *handle;

    EXPECT_EQ(PMB_ECREAT, open_handle(handle, 10, "alibaba/failed.pool"));
    EXPECT_TRUE(NULL==handle);
}

/*
 * Fail on creating new handle cause user doesn't have permissions to write in given path
 */
TEST(OpenHandle, CreateFailPermisionDenied) {
    pmb_handle *handle;
    int fd = creat("/tmp/ro.pool", S_IRUSR);
    close(fd);
    EXPECT_EQ(PMB_ECREAT, open_handle(handle, 10, "/tmp/ro.pool"));
    EXPECT_TRUE(NULL==handle);
    unlink("/tmp/ro.pool");
}

/*
 * Fail on creating new handle cause max key len in opts is 0
 */
TEST(DISABLED_OpenHandle, CreateFailWrongMaxKeyLen) {
    pmb_handle *handle;

    EXPECT_EQ(PMB_ECREAT, open_handle(handle, 10, "failed.pool", 0));
    EXPECT_TRUE(NULL==handle);
    if (handle != NULL) {
        remove_handle(handle);
    }
}

/*
 * Fail on creating new handle cause max val len in opts is 0
 */
TEST(DISABLED_OpenHandle, CreateFailWrongMaxValLen) {
    pmb_handle *handle;

    EXPECT_EQ(PMB_ECREAT, open_handle(handle, 10, "failed.pool", MAX_KEY_LEN, 0));
    EXPECT_TRUE(NULL==handle);
}

/*
 * Fail on creating new handle cause max_val_len is 0 and max_key_len is 0
 */
TEST(DISABLED_OpenHandle, CreateFailWrongMaxValLenWrongMaxKeyLen) {
    pmb_handle *handle;

    EXPECT_EQ(PMB_ECREAT, open_handle(handle, 10, "failed.pool", 0, 0));
    EXPECT_TRUE(NULL==handle);
}

/*
 * Fail on creating new handle cause write_log_entries is 0
 */
TEST(DISABLED_OpenHandle, CreateFailWronWriteLogLen) {
    pmb_handle *handle;

    EXPECT_EQ(PMB_ECREAT, open_handle(handle, 10, "failed.pool", MAX_KEY_LEN, MAX_VAL_LEN, 0));
    EXPECT_TRUE(NULL==handle);
}

/*
 * Fail on creating new handle cause size is 0
 */
TEST(OpenHandle, CreateFailSize0) {
    pmb_handle *handle;

    EXPECT_EQ(PMB_ECREAT, open_handle(handle, 0, "failed.pool"));
    EXPECT_TRUE(NULL==handle);
}

/*
 * Open handle with success
 */
TEST(OpenHandle, OpenSuccess) {
	pmb_handle *handle = create_handle();

	EXPECT_EQ(PMB_OK, pmb_close(handle));

	// try to open handle created before
	handle = create_handle();

	remove_handle(handle);
}

/*
 * Success
 */
TEST(OpenHandle, SuccessOpenNotEmpty) {
	pmb_handle *handle = create_handle();

	pmb_pair to_put = generate_put_input();

	uint64_t tx_slot;
	EXPECT_EQ(PMB_OK, pmb_tx_begin(handle, &tx_slot));
	EXPECT_EQ(PMB_OK, pmb_tput(handle,  tx_slot, &to_put));
	EXPECT_EQ(PMB_OK, pmb_tx_commit(handle, tx_slot));
	EXPECT_EQ(PMB_OK, pmb_tx_execute(handle, tx_slot));

	EXPECT_EQ(PMB_OK, pmb_close(handle));

	EXPECT_EQ(PMB_OK, open_handle(handle, 4, "single_thread.pool"));
	EXPECT_TRUE(NULL != handle);
	EXPECT_EQ(1, count(handle, PMB_DATA));

	remove_handle(handle);
}

//TODO: checking restored values

TEST(DISABLED_OpenHandle, SuccessOpenNotEmptyObjAndMeta) {
	pmb_handle *handle = create_handle();

	pmb_pair to_put = generate_put_input();

	uint64_t tx_slot;
	EXPECT_EQ(PMB_OK, pmb_tx_begin(handle, &tx_slot));
	EXPECT_EQ(PMB_OK, pmb_tput(handle,  tx_slot, &to_put));
	to_put.blk_id = 0;
	EXPECT_EQ(PMB_OK, pmb_tput(handle, tx_slot, &to_put));
	EXPECT_EQ(PMB_OK, pmb_tx_commit(handle, tx_slot));
	EXPECT_EQ(PMB_OK, pmb_tx_execute(handle, tx_slot));

	EXPECT_EQ(PMB_OK, pmb_close(handle));

	EXPECT_EQ(PMB_OK, open_handle(handle, 4, "single_thread.pool"));
	EXPECT_TRUE(NULL != handle);
	EXPECT_EQ(2, count(handle, PMB_DATA));

	remove_handle(handle);
}

/*
 * Fail on creating new handle cause of superblock write error
 */
TEST(DISABLED_OpenHandle, CreateFailSBWriteError) {
    EXPECT_TRUE("TODO"=="NIMA");
}

/*
 * Fail on creating new handle cause superblock checksum is incorrect
 */
TEST(DISABLED_OpenHandle, OpenFailSBChecksumError) {
    EXPECT_TRUE("TODO"=="NIMA");
}

// TODO: add validation to open
/*
 * Fail on opening handle cause max_key_len changed
 */
TEST(DISABLED_OpenHandle, OpenFailSlotsLenChanged) {
	pmb_handle *handle = create_handle();

	EXPECT_EQ(PMB_OK, pmb_close(handle));

	// try to open handle created before
	EXPECT_EQ(PMB_ESBIN, open_handle(handle, 4, "single_thread.pool", MAX_KEY_LEN, MAX_VAL_LEN, 22));
	EXPECT_TRUE(NULL==handle);

	EXPECT_EQ(0, remove("/nvml/single_thread.pool"));
}
