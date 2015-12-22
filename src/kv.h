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

#ifndef KV_H
#define KV_H 1

#include <time.h>

#include "caslist.h"
#include "backend.h"

#ifdef DEBUG
#define logprintf(args...) fprintf(stderr, args)
#else
#define logprintf(args...)
#endif

typedef struct {
    uint64_t flch64;
    uint32_t max_key_len;
    uint64_t max_val_len;
    uint8_t  tx_slots_count;
} pmb_super_block;

typedef struct {
	uint64_t flch64;
    uint32_t version;
    uint32_t key_len;
    uint32_t val_len;
} pmb_obj_meta;

typedef struct {
    uint64_t* obj_ids;
    size_t*   sizes;
    size_t    count;
} tx_flush;

typedef struct _tx_meta tx_meta;

struct _tx_meta {
	struct _tx_meta* next;
	uint64_t id;
	uint32_t version;
	uint32_t val_len;
	uint32_t offset;
};

typedef struct {
	size_t count;
	tx_meta *list;
} tx_metalist;

/*
 * Structure defining transaction log. Before actual write first new data will be saved
 * to write log, and then persisted to actual block. Each block will be guarded
 * by compare-and-swap based spinlock. Number of blocks can be configured at handle
 * creation, it's recommended to set number of write log entries equal to the
 * number of write threads.
 */
typedef struct {
    uint8_t      tx_slots_count;   // number of available write log entries
    uint32_t     tx_slot_capacity; // number of entries single transaction slot could keep
    caslist*     tx_slots_list;    // list with available tx_slots
    tx_flush*    hard_flush_list;  // list with id's for hard flush
    tx_metalist* upd_id_list;      // list with blocks ids to metadata update
} tx_log;

/*
 * Main handle to object handle, used by all other functions to put/get/delete
 * objects from pool.
 */
struct _pmb_handle {
    struct _backend*     backend;  // handle to backend
    uint64_t     total_objs_count; // statistic from pmemblk
    uint64_t     meta_objs_count;
    uint32_t     max_key_len;      // from superblock
    uint32_t     max_val_len;      // from superblock
    uint32_t     meta_max_key_len; // from superblock
    uint32_t     meta_max_val_len;
    uint64_t     border;
    caslist*     objs_list;        // list with objects found at initial scan
    caslist*     free_list;        // list with available blocks to write
    caslist*     meta_objs_list;   // list with objects found at initial scan
	caslist*     meta_free_list;
    tx_log       op_log;           // for secure in-place data writes/updates
    pthread_t    sync_thread;      // thread for syncs
};

struct pmb_iter {
    struct _pmb_handle* handle;
    int				    region;
    uint64_t            vector_len;
    uint64_t            vector_pos;
};

/*
 * Handles failed transactions recovery, creates object list and free list
 */
uint8_t recovery(struct _pmb_handle* handle);

void populate_free_list(struct _pmb_handle* handle);

/*
 * Prototypes related to transactions handling.
 */

/*
 * State of transaction:
 * - EMPTY:      there's no transaction in block
 * - PROCESSING: before commit
 * - COMMITED:   after commit, before execute
 * - ABORTED:    after triggering pmb_tx_abort, all ops will be reverted
 */
typedef enum {
    EMPTY,
    PROCESSING,
    COMMITED,
    ABORTED
} tx_status;

/*
 * Types of operations registerd in transaction log.
 */
typedef enum {
    WRITE,
    UPDATE,
    UPDINPLACE,
    REMOVE,
} tx_op;

/*
 * Single operation,
 * obj_id2 :
 * - is not used by WRITE and DELETE operations,
 * - is used by UPDATE as new object id,
 * - is used by UPDINPLACE as size of following databuffer
 */
typedef struct {
    tx_op    type;
    uint64_t obj_id1;
    uint64_t obj_id2;
} tx_entry;

/*
 * State of transaction
 */
typedef struct {
    uint64_t  flch64;
    tx_status status;
    size_t    size;
} tx_slot;

void tx_log_init(struct _pmb_handle *handle, uint8_t tx_log_slots);

void tx_log_check(struct _pmb_handle *handle);

void tx_log_free(struct _pmb_handle *handle);

uint64_t tx_log_get_slot(struct _pmb_handle *handle, uint64_t *tx_slot_pt);

void tx_log_free_slot(struct _pmb_handle *handle, uint64_t tx_slot);

uint8_t tx_slot_init(struct _pmb_handle *handle, uint64_t tx_slot);

uint8_t tx_slot_commit(struct _pmb_handle *handle, uint64_t tx_slot);

uint8_t tx_slot_execute(struct _pmb_handle *handle, uint64_t tx_slot);

uint8_t tx_slot_abort(struct _pmb_handle *handle, uint64_t tx_slot);

uint8_t tx_slot_op_write(struct _pmb_handle *handle, uint64_t tx_slot, uint64_t obj_id, uint32_t size);

uint8_t tx_slot_op_update(struct _pmb_handle *handle, uint64_t tx_slot, uint64_t old_obj_id, uint64_t new_obj_id, uint32_t size);

uint8_t tx_slot_op_small_update(struct _pmb_handle *handle, uint64_t tx_slot, uint64_t obj_id, void *data, uint32_t offset, uint32_t size);

uint8_t tx_slot_op_remove(struct _pmb_handle *handle, uint64_t tx_slot, uint64_t obj_id);

uint32_t get_block_size(uint32_t key_len, uint32_t val_len);

#endif //KV_H
