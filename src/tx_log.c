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

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include "pmbackend.h"
#include "kv.h"
#include "util.h"

#ifdef WITH_LTTNG
#define TRACEPOINT_CREATE_PROBES
#define TRACEPOINT_DEFINE
#include "tx_log-lttng.h"
#else
#define tracepoint(...)
#endif

static void tx_update_block(struct _pmb_handle *store, uint8_t tx_id,
        uint64_t obj_id, const void *payload, size_t offset, size_t len);

static void tx_slot_meta_upd_add(struct _pmb_handle *store, uint8_t tx_id,
        uint64_t obj_id, uint32_t size);

static void tx_slot_meta_upd_process(struct _pmb_handle *store, uint8_t tx_id);

void
tx_log_init(struct _pmb_handle *store, uint8_t tx_slots_count)
{
    store->op_log.tx_slots_count = tx_slots_count;
    store->op_log.tx_slots_list = caslist_new(1, tx_slots_count);
    store->op_log.tx_slot_capacity =
            (get_block_size(store->max_key_len, store->max_val_len) - sizeof(tx_slot))
            / sizeof(tx_entry);

    store->op_log.upd_id_list = (tx_metalist *) malloc(tx_slots_count * sizeof(tx_metalist));

    for (uint8_t i = 1; i <= tx_slots_count; i++) {
        caslist_push(store->op_log.tx_slots_list, i);

        // very dummy way, check maximal number of ops per slot
        store->op_log.upd_id_list[i - 1].list = (tx_meta *) malloc(128 * sizeof(tx_meta));
        store->op_log.upd_id_list[i - 1].count = 0;
    }
}

void
tx_log_free(struct _pmb_handle *store)
{
    for (uint8_t i = 0; i < store->op_log.tx_slots_count; i++) {
        free(store->op_log.upd_id_list[i].list);
    }
    free(store->op_log.upd_id_list);
    store->op_log.tx_slots_count = 0;
    caslist_free(store->op_log.tx_slots_list);
}

uint64_t
tx_log_get_slot(struct _pmb_handle *store, uint64_t *tx_slot_pt)
{
    return caslist_pop(store->op_log.tx_slots_list, tx_slot_pt);
}

void
tx_log_free_slot(struct _pmb_handle *store, uint64_t tx_slot_id)
{
    caslist_push(store->op_log.tx_slots_list, tx_slot_id);
}

static void
tx_slot_checksum(struct _pmb_handle *store, tx_slot *slot,
        uint8_t tx_slot_id)
{
	tracepoint(tx_log, tx_slot_checksum_enter);
    // compute slot checksum without space reserved for checksum
    util_checksum(slot, slot->size, &(slot->flch64), 1);
    backend_tx_persist(store->backend, tx_slot_id, slot->size);
    tracepoint(tx_log, tx_slot_checksum_exit);
}

uint8_t
tx_slot_init(struct _pmb_handle *store, uint64_t tx_slot_id)
{
    if (tx_slot_id == 0 || tx_slot_id > store->op_log.tx_slots_count) {
        return PMB_ERR;
    }

    tx_slot_id--;
    void *slot_ptr = backend_tx_direct(store->backend, tx_slot_id);

    if (slot_ptr == NULL) {
        return PMB_ERR;
    }

    tx_slot *slot = slot_ptr;
    slot->flch64 = 0;
    slot->status = PROCESSING;
    slot->size = sizeof(tx_slot);

    return PMB_OK;
}

uint8_t
tx_slot_commit(struct _pmb_handle *store, uint64_t tx_slot_id)
{
	tracepoint(tx_log, tx_slot_commit_enter);
    if (tx_slot_id == 0 || tx_slot_id > store->op_log.tx_slots_count) {
        tracepoint(tx_log, tx_slot_commit_exit);
        return PMB_ERR;
    }

    tx_slot_id--;
    void *slot_ptr = backend_tx_direct(store->backend, tx_slot_id);

    if (slot_ptr == NULL) {
        tracepoint(tx_log, tx_slot_commit_exit);
        return PMB_ERR;
    }

    tx_slot *slot = slot_ptr;
    if (slot->status != PROCESSING) {
        tracepoint(tx_log, tx_slot_commit_exit);
        return PMB_ERR;
    }
    slot->status = COMMITED;

    tx_slot_checksum(store, slot, tx_slot_id);

    tracepoint(tx_log, tx_slot_commit_exit);
    return PMB_OK;
}
static void
tx_slot_meta_upd_add(struct _pmb_handle *store, uint8_t tx_id,
        uint64_t obj_id, uint32_t size)
{
	tracepoint(tx_log, tx_slot_meta_upd_add_enter);
    size_t i = 0;
    tx_metalist *metalist = &store->op_log.upd_id_list[tx_id];
    tx_meta *meta;
    while (i < metalist->count) {
        if (metalist->list[i].id == obj_id || metalist->list[i].id == 0)
            break;
        i++;
    }

    meta = &metalist->list[i];
    if (meta->id == 0) {
        meta->id = obj_id;
        pmb_obj_meta *obj_meta = backend_direct(store->backend, obj_id);
        meta->version = obj_meta->version;
        meta->val_len = obj_meta->val_len;
        metalist->count++;
    }

    meta->version += 1;

    if (meta->val_len < size) {
        meta->val_len = size;
    }

    tracepoint(tx_log, tx_slot_meta_upd_add_exit);
}

static void
tx_update_block(struct _pmb_handle *store, uint8_t tx_id, uint64_t obj_id,
        const void *payload, size_t offset, size_t len)
{
	tracepoint(tx_log, tx_update_block_enter);

    void *obj = backend_direct(store->backend, obj_id);
    tracepoint(tx_log, backend_memcpy_enter);
    backend_memcpy(store->backend,
            obj + sizeof(pmb_obj_meta) + store->max_key_len + offset, payload,
            len);
	tracepoint(tx_log, backend_memcpy_exit);

    tx_slot_meta_upd_add(store, tx_id, obj_id, offset + len);

    tracepoint(tx_log, tx_update_block_exit);
}

uint8_t
tx_slot_execute(struct _pmb_handle *store, uint64_t tx_slot_id)
{
	tracepoint(tx_log, tx_slot_execute_enter);

    tx_slot_id--;
    void *slot_ptr = backend_tx_direct(store->backend, tx_slot_id);

    if (slot_ptr == NULL) {
        printf("tx_execute: error lane\n");
        tracepoint(tx_log, tx_slot_execute_exit);
        return PMB_ERR;
    }
    tx_slot *slot = slot_ptr;

    if (slot->status != COMMITED) {
        tracepoint(tx_log, tx_slot_execute_exit);
        return PMB_ERR;
    }

    void *entries = slot_ptr + sizeof(tx_slot);
    void *slot_end = slot_ptr + slot->size;
    void *obj;
    tx_entry *txe;
    uint32_t offset;
    uint32_t size;
    uint8_t error = 0;
    while (entries < slot_end) {
        txe = entries;
        switch (txe->type) {
            case UPDATE:
            case REMOVE:
                obj = backend_get(store->backend, txe->obj_id1, &error);
                if (obj) {
                    backend_set_zero(store->backend, obj);
                    if (txe->obj_id1 < store->total_objs_count) {
                        caslist_push(store->free_list, txe->obj_id1);
                    } else {
                        caslist_push(store->meta_free_list, txe->obj_id1);
                    }

                    logprintf("tx_log: releasing obj_id: %zu\n", txe->obj_id1);
                }
                entries += sizeof(tx_entry);
                break;
            case UPDINPLACE:
                // copy to the existing object and sync
                size = txe->obj_id2 >> 32;
                offset = txe->obj_id2 & 0xffffffff;
                obj = entries + sizeof(tx_entry);
                tx_update_block(store, tx_slot_id, txe->obj_id1, obj, offset,
                        size);
                entries = obj + size;
                break;
            default:
                entries += sizeof(tx_entry);
        }
    }

    // Chaining transaction slots not yet supported

    tx_slot_meta_upd_process(store, tx_slot_id);

    backend_tx_set_zero(store->backend, slot_ptr);

    tracepoint(tx_log, tx_slot_execute_exit);
    return PMB_OK;
}

uint8_t
tx_slot_abort(struct _pmb_handle *store, uint64_t tx_slot_id)
{
     tx_slot_id--;
     void *slot_ptr = backend_tx_direct(store->backend, tx_slot_id);

     if (slot_ptr == NULL)
        return PMB_ERR;

     printf("tx_abort: %p\n", slot_ptr);
     tx_slot *slot = slot_ptr;
     tx_entry *entries = slot_ptr + sizeof(tx_slot);

     if(slot->status != PROCESSING && slot->status != COMMITED) {
        return PMB_ERR;
     }

     slot->status = ABORTED;

     tx_slot_checksum(store, slot, tx_slot_id);
     void *update_clear_ptr, *write_clear_ptr;
     void *slot_end = slot_ptr + slot->size;
     tx_entry *txe;
     // process entries, all new blocks (obj_id1 from WRITE and obj_id2 from UPDATE
     // should be zeroed and returned to the free list

     for(void *position_ptr = entries; position_ptr < slot_end;
             position_ptr += sizeof(tx_entry)) {
         txe = (tx_entry *)position_ptr;
         switch(txe->type) {
             case UPDATE:
                 update_clear_ptr = backend_direct(store->backend, txe->obj_id2);
                 backend_set_zero(store->backend, update_clear_ptr);
                 if (txe->obj_id2 < store->total_objs_count) {
                    caslist_push(store->free_list, txe->obj_id2);
                 } else {
                    caslist_push(store->meta_free_list, txe->obj_id2);
                 }
                 break;
             case WRITE:
                 write_clear_ptr = backend_direct(store->backend, txe->obj_id1);
                 backend_set_zero(store->backend, write_clear_ptr);
                 if (txe->obj_id1 < store->total_objs_count) {
                    caslist_push(store->free_list, txe->obj_id1);
                 } else {
                    caslist_push(store->meta_free_list, txe->obj_id1);
                 }
                 break;
             default:
                break;
         }
     }

     slot->status = EMPTY;
     slot->size = 0;

     tx_slot_checksum(store, slot, tx_slot_id);

     backend_tx_set_zero(store->backend, slot_ptr);

     return PMB_OK;
}

uint8_t
tx_slot_op_write(struct _pmb_handle *store, uint64_t tx_slot_id,
        uint64_t obj_id, uint32_t size)
{
    tx_slot_id--;
    void *slot_ptr = backend_tx_direct(store->backend, tx_slot_id);

    if (slot_ptr == NULL)
        return PMB_ERR;

    tx_slot *slot = slot_ptr;
    tx_entry *entry = slot_ptr + slot->size;

    if (slot->status != PROCESSING) {
        return PMB_ERR;
    }

    entry->type = WRITE;
    entry->obj_id1 = obj_id;
    slot->size += sizeof(tx_entry);

    return PMB_OK;
}

uint8_t
tx_slot_op_small_update(struct _pmb_handle *store, uint64_t tx_slot_id,
        uint64_t obj_id, void *data, uint32_t offset, uint32_t size)
{
    tx_slot_id--;
    void *slot_ptr = backend_tx_direct(store->backend, tx_slot_id);

    if (slot_ptr == NULL)
        return PMB_ERR;

    tx_slot *slot = slot_ptr;
    tx_entry *entry = slot_ptr + slot->size;

    if (slot->status != PROCESSING) {
        return PMB_ERR;
    }

    entry->type = UPDINPLACE;
    entry->obj_id1 = obj_id;
    entry->obj_id2 = ((uint64_t) size << 32) | offset;
    backend_memcpy(store->backend, (void *) entry + sizeof(tx_entry), data,
            size);
    slot->size += sizeof(tx_entry) + size;

    return PMB_OK;
}

uint8_t
tx_slot_op_update(struct _pmb_handle *store, uint64_t tx_slot_id,
        uint64_t old_obj_id, uint64_t new_obj_id, uint32_t size)
{
    tx_slot_id--;
    void *slot_ptr = backend_tx_direct(store->backend, tx_slot_id);

    if (slot_ptr == NULL)
        return PMB_ERR;

    tx_slot *slot = slot_ptr;
    tx_entry *entry = slot_ptr + slot->size;

    if (slot->status != PROCESSING) {
        return PMB_ERR;
    }

    entry->type = UPDATE;
    entry->obj_id1 = old_obj_id;
    entry->obj_id2 = new_obj_id;
    slot->size += sizeof(tx_entry);

    return PMB_OK;
}

uint8_t
tx_slot_op_remove(struct _pmb_handle *store, uint64_t tx_slot_id,
        uint64_t obj_id)
{
    tx_slot_id--;
    void *slot_ptr = backend_tx_direct(store->backend, tx_slot_id);

    if (slot_ptr == NULL)
        return PMB_ERR;

    tx_slot *slot = slot_ptr;
    tx_entry *entry = slot_ptr + slot->size;

    if (slot->status != PROCESSING) {
        return PMB_ERR;
    }

    entry->type = REMOVE;
    entry->obj_id1 = obj_id;
    slot->size += sizeof(tx_entry);

    return PMB_OK;
}

void tx_log_check(struct _pmb_handle *store)
{
    printf("TX_LOG_CHECK START\n");
    void *slot_ptr = NULL;
    void *obj = NULL;
    uint32_t offset;
    uint32_t size;

    for(uint8_t tx_slot_id = 1; tx_slot_id <= store->op_log.tx_slots_count; tx_slot_id++) {
        slot_ptr = backend_tx_direct(store->backend, tx_slot_id);

        if (slot_ptr == NULL)
            continue;

        tx_slot *slot = slot_ptr;
        tx_entry *entries = slot_ptr + sizeof(tx_slot);

        if (slot->status == EMPTY) {
            continue;
        }

        int commit = util_checksum(slot, sizeof(tx_slot) + slot->size,
                &(slot->flch64), 0) && (slot->status == COMMITED);

        for(void *position_ptr = entries; position_ptr < slot_ptr + slot->size;
                position_ptr += sizeof(tx_entry)) {
            tx_entry *entry = (tx_entry *) position_ptr;
            if (!entry->obj_id1 && !entry->obj_id2) {
                break;
            }

            switch (entry->type) {
                case WRITE:
                    if (!commit) {
                        obj = backend_direct(store->backend, entry->obj_id1);
                        backend_set_zero(store->backend, obj);
                    }
                    break;
                case REMOVE:
                    if (commit) {
                        obj = backend_direct(store->backend, entry->obj_id1);
                        backend_set_zero(store->backend, obj);
                    }
                    break;
                case UPDATE:
                    if (commit) {
                        obj = backend_direct(store->backend, entry->obj_id1);
                        backend_set_zero(store->backend, obj);
                    } else {
                        obj = backend_direct(store->backend, entry->obj_id2);
                        backend_set_zero(store->backend, obj);
                    }
                    break;
                case UPDINPLACE:
                    if (commit){
                        size = entry->obj_id2 >> 32;
                        offset = entry->obj_id2 & 0xffffffff;
                        obj = (void *) entries + sizeof(tx_entry);
                        tx_update_block(store, tx_slot_id, entry->obj_id1, obj,
                                offset, size);
                    }
                default:
                    break;
            }
        }
        slot->status = EMPTY;
        slot->size = 0;
        tx_slot_checksum(store, slot, tx_slot_id);
        backend_tx_set_zero(store->backend, slot_ptr);
    }
    printf("TX_LOG_CHECK END\n");
}

static void
tx_slot_meta_upd_process(struct _pmb_handle *store, uint8_t tx_id)
{
	tracepoint(tx_log, tx_slot_meta_upd_process_enter);
    tx_metalist *metalist = &(store->op_log.upd_id_list[tx_id]);
    tx_meta *meta;
    for (size_t i = 0; i < metalist->count; i++) {
        meta = &(metalist->list[i]);
        pmb_obj_meta *obj_meta = backend_direct(store->backend, meta->id);
        obj_meta->version = meta->version;
        obj_meta->val_len = meta->val_len;

        util_checksum(obj_meta,
                sizeof(pmb_obj_meta) + store->max_key_len + obj_meta->val_len,
                &obj_meta->flch64, 1);
    }

    memset((void *) metalist->list, 0, metalist->count * sizeof(tx_meta));
    metalist->count = 0;

    tracepoint(tx_log, tx_slot_meta_upd_process_exit);
}
