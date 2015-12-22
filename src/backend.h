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

#ifndef _BACKEND_H_
#define _BACKEND_H_

#include <stdint.h>
#include <sys/types.h>

#define BACKEND_OK         0
#define BACKEND_NO_BACKEND 1
#define BACKEND_ENOENT     2
#define BACKEND_EMPTY      3
#define BACKEND_FULL       4
#define BACKEND_INV_ID     5

typedef struct _backend backend;

backend* backend_open(const char* path, size_t data_size, size_t meta_size,
         size_t tx_slots, size_t tx_slot_size,
         uint32_t max_key_len, uint32_t max_val_len,
         uint32_t meta_max_key_len, uint32_t meta_max_val_len,
		 uint8_t sync_type);

backend* backend_create(const char* path, size_t data_size, size_t meta_size,
         size_t tx_slots, size_t tx_slot_size,
         uint32_t max_key_len, uint32_t max_val_len,
		 uint32_t meta_max_key_len, uint32_t meta_max_val_len,
         mode_t mode, uint8_t sync_type);

uint8_t backend_get_sync_type(struct _backend* backend);

void* backend_memcpy(struct _backend* backend, void* dest, const void* src, size_t num);

void  backend_close(struct _backend* backend);

void* backend_direct(struct _backend* backend, uint64_t obj_id);

void* backend_get(struct _backend* backend, uint64_t obj_id, uint8_t* error);

uint8_t backend_persist(struct _backend* backend, void* obj_ptr, size_t size);

uint8_t backend_persist_all(struct _backend* backend);

uint8_t backend_persist_weak(struct _backend* backend, void* obj_ptr, size_t size);

uint8_t backend_set_zero(struct _backend* backend, void* obj_ptr);

uint8_t backend_tx_set_zero(struct _backend* backend, void* slot_ptr);

void* backend_tx_direct(struct _backend* backen, uint8_t tx_id);

uint8_t backend_tx_persist(struct _backend* backend, uint8_t tx_id, size_t size);

size_t backend_nblock(struct _backend* backend, int meta);

#endif//_BACKEND_H
