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

#include <limits.h>
#include <iostream>
#include <stdint.h>
#include <string>
#include <stdio.h>

#include "pmbackend.h"
#include "kv.h"

#define MAX_KEY_LEN 64
#define MAX_VAL_LEN 1024


uint64_t count(pmb_handle* handle, uint8_t region);

int open_handle(pmb_handle*& handle, int size, std::string path,
		uint32_t max_key_len=MAX_KEY_LEN,
		uint32_t max_val_len=MAX_VAL_LEN,
		uint8_t write_log_entries=16);

pmb_handle* create_handle(void);

void remove_handle(pmb_handle* handle);

pmb_pair generate_put_input(uint64_t obj_id=0, uint32_t offset=0,
		void* key=(void *)"120", void* val=(void *)"632",
		uint32_t key_len=MAX_KEY_LEN, uint32_t val_len=MAX_VAL_LEN);

pmb_handle* prepare_handle_for_iter(void);

void validate_save(pmb_handle* handle, uint64_t obj_id, pmb_pair inserted);