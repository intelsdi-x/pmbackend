/*
 * Copyright (c) 2014-2016, Intel Corporation
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
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <stdlib.h>
#include <errno.h>
#include <sys/stat.h>
#include <unistd.h>

#include "pmbackend.h"
#include "backend.h"
#include "kv.h"
#include "util.h"

#ifdef WITH_LTTNG
#define TRACEPOINT_CREATE_PROBES
#define TRACEPOINT_DEFINE
#include "pmbackend-lttng.h"
#else
#define tracepoint(...)
#endif

char *INVALID_INPUT = "%s error: passed params are invalid\n";

/*
 * libpmemobj_init -- load-time initialization for obj
 *
 * Called automatically by the run-time loader.
 */
__attribute__((constructor))
static void
pmb_init(void)
{
	util_init();
}

uint32_t
get_block_size(uint32_t key_len, uint32_t val_len) {
    // checksum: uint64_t 8B
    // version:  uint32_t 4B
    // key_len:  uint32_t 4B
    // val_len:  uint32_t 4B
    // data:     uint8_t * (key_len + val_len)

    return val_len + sizeof(pmb_obj_meta) + key_len;
}

static void*
_sync_thread(void *args)
{
    backend *bck = (backend *) args;
    while(1) {
        sleep(5); // TODO: add to opts
        backend_persist_all(bck);
    }

    return NULL;
}

#define TX_LOG_SIZE 128UL * 1024 * 1024

pmb_handle*
pmb_open(pmb_opts* opts, uint8_t* error)
{
    *error = 0;
    // first try open
    // then create
    // initialize and process write log
    // populate free list and validate saved data
    uint8_t empty;
    pmb_handle* handle = NULL;

    tracepoint(pmbackend, pmb_open_enter, opts);
    handle = malloc(sizeof(pmb_handle));
    if (handle == NULL) {
        *error = PMB_ERR;
        return NULL;
    }
    // Fails when trying open existing store with changed params
    handle->backend = backend_open(opts->path, opts->data_size, opts->meta_size,
            opts->write_log_entries, TX_LOG_SIZE / opts->write_log_entries,
            opts->max_key_len, opts->max_val_len,
            opts->meta_max_key_len, opts->meta_max_val_len,
			opts->sync_type);

    if (handle->backend == NULL) {
        // try create if cannot open
        handle->backend = backend_create(opts->path, opts->data_size, opts->meta_size,
                opts->write_log_entries, TX_LOG_SIZE / opts->write_log_entries,
                opts->max_key_len, opts->max_val_len,
				opts->meta_max_key_len, opts->meta_max_val_len,
                S_IRWXU, opts->sync_type);
        if (handle->backend == NULL) {
            *error = PMB_ECREAT;
            logprintf("pmb_open: cannot create store: %s\n", strerror(errno));
            free(handle);
            tracepoint(pmbackend, pmb_open_exit, "NULL");
            return NULL;
        } else {
            // empty store -> set 1
            logprintf("pmb_open: empty store\n");
            empty = 1;
        }
    } else {
        logprintf("pmb_open: nonempty store\n");
        empty = 0;
    }

    handle->max_key_len = opts->max_key_len;
    handle->max_val_len = opts->max_val_len;
    handle->meta_max_key_len = opts->meta_max_key_len;
	handle->meta_max_val_len = opts->meta_max_val_len;

    // initialize and process write log
    tx_log_init(handle, opts->write_log_entries);

    handle->total_objs_count = backend_nblock(handle->backend, PMB_DATA);
    handle->meta_objs_count = backend_nblock(handle->backend, PMB_META);

    if (empty) {
        // empty store, skip recovery
        // initialize freelist, free list will be populated, when data will be checked
        // with iterator
        handle->free_list = caslist_new(1, handle->total_objs_count);
        handle->meta_free_list = caslist_new(1, handle->meta_objs_count);
        handle->objs_list = NULL;
        handle->meta_objs_list = NULL;
        populate_free_list(handle);
    } else {
        // there was write to store, perform full recovery
        tx_log_check(handle);  // recover transactions
        handle->free_list = caslist_new(0, 0);
        handle->meta_free_list = caslist_new(0, 0);
        handle->objs_list = caslist_new(0, 0);
        handle->meta_objs_list = caslist_new(0, 0);
        recovery(handle);
    }

    if (opts->sync_type == PMB_THSYNC) {
        // init sync thread
        pthread_create(&handle->sync_thread, NULL, _sync_thread, (void*) handle->backend);
    }
    tracepoint(handle, pmb_open_exit, "OK");
    return handle;
}


uint8_t
pmb_close(pmb_handle* handle)
{
	tracepoint(pmbackend, pmb_close_enter);
	if (handle == NULL) {
		logprintf(INVALID_INPUT, "pmb_close");
		tracepoint(pmbackend, pmb_close_exit, "Store NULL");
		return PMB_ERR;
	}

    if (handle->objs_list != NULL) {
        caslist_free(handle->objs_list);
    }

    if (handle->meta_objs_list != NULL) {
        caslist_free(handle->meta_objs_list);
    }

    caslist_free(handle->free_list);
    caslist_free(handle->meta_free_list);
    tx_log_free(handle);

    if (backend_get_sync_type(handle->backend) == PMB_THSYNC) {
        pthread_cancel(handle->sync_thread);
        pthread_join(handle->sync_thread, NULL);
    }

    backend_close(handle->backend);

    free(handle);

    tracepoint(pmbackend, pmb_close_exit, "OK");
    return PMB_OK;
}

uint8_t
pmb_tx_begin(pmb_handle* handle, uint64_t* tx_slot)
{
	uint8_t ret;
	tracepoint(pmbackend, pmb_tx_begin_enter, handle);

	if (handle == NULL || tx_slot == NULL) {
		logprintf(INVALID_INPUT, "pmb_tx_begin");
		tracepoint(pmbackend, pmb_tx_begin_exit, handle, 0, "ErrNull");
		return PMB_ERR;
	}
    if (tx_log_get_slot(handle, tx_slot) != 0) {
        tracepoint(pmbackend, pmb_tx_begin_exit, handle, 0, "ErrSlot");
        return PMB_ERR;
    }
    ret = tx_slot_init(handle, *tx_slot);
    tracepoint(pmbackend, pmb_tx_begin_exit, handle, 0, "OK");
    return ret;
}

uint8_t
pmb_tx_commit(pmb_handle* handle, uint64_t tx_slot)
{
	uint8_t ret;
	tracepoint(pmbackend, pmb_tx_commit_enter, handle, tx_slot);
	if (handle == NULL || tx_slot == 0 || tx_slot > handle->op_log.tx_slots_count) {
		logprintf(INVALID_INPUT, "pmb_tx_commit");
		tracepoint(pmbackend, pmb_tx_commit_exit, handle, tx_slot, PMB_ERR);
		return PMB_ERR;
	}
    ret =  tx_slot_commit(handle, tx_slot);
    tracepoint(pmbackend, pmb_tx_commit_exit, handle, tx_slot, ret);
    return ret;
}

uint8_t
pmb_tx_execute(pmb_handle* handle, uint64_t tx_slot)
{
	tracepoint(pmbackend, pmb_tx_execute_enter, handle, tx_slot);
    if (handle == NULL || tx_slot == 0 || tx_slot > handle->op_log.tx_slots_count) {
		logprintf(INVALID_INPUT, "pmb_tx_execute");
		tracepoint(pmbackend, pmb_tx_execute_exit, handle, tx_slot, PMB_ERR);
		return PMB_ERR;
	}
    uint8_t ret = tx_slot_execute(handle, tx_slot);
    tx_log_free_slot(handle, tx_slot);
    tracepoint(pmbackend, pmb_tx_execute_exit, handle, tx_slot, ret);
    return ret;
}

uint8_t
pmb_tx_abort(pmb_handle* handle, uint64_t tx_slot)
{
	tracepoint(pmbackend, pmb_tx_abort_enter, handle, tx_slot);
    if (handle == NULL || tx_slot == 0 || tx_slot > handle->op_log.tx_slots_count) {
		logprintf(INVALID_INPUT, "pmb_tx_abort");
		tracepoint(pmbackend, pmb_tx_abort_exit, handle, tx_slot, PMB_ERR);
		return PMB_ERR;
	}
    uint8_t ret = tx_slot_abort(handle, tx_slot);
    tx_log_free_slot(handle, tx_slot);
    tracepoint(pmbackend, pmb_tx_abort_exit, handle, tx_slot, ret);
    return ret;
}

uint8_t
pmb_get(pmb_handle* handle, uint64_t obj_id, pmb_pair* kv)
{
	tracepoint(pmbackend, pmb_get_enter, handle, obj_id);
	if (handle == NULL || kv == NULL) {
		logprintf(INVALID_INPUT, "pmb_get");
	    tracepoint(pmbackend, pmb_get_exit, handle, obj_id, PMB_ERR);
		return PMB_ERR;
	}

    uint8_t error;
	void* obj = backend_get(handle->backend, obj_id, &error);
	if (obj == NULL) {
	    tracepoint(pmbackend, pmb_get_exit, handle, obj_id, PMEMKV_ENOENT);
		return PMB_ENOENT;
	}

    logprintf("pmb_get get obj_id: %zu\n", obj_id);

    kv->obj_id = obj_id;
    kv->key_len = ((pmb_obj_meta *) obj)->key_len;
    kv->key = obj + sizeof(pmb_obj_meta);
    kv->val_len = ((pmb_obj_meta *) obj)->val_len;
    if (kv->val_len == 0) {
        kv->val = NULL;
    } else {
        if (obj_id < handle->total_objs_count) {
            /* in "data" region meta + max_key_len are aligned to 4k,
             * so we need to add this to the beggining of region */
            kv->val = obj + sizeof(pmb_obj_meta) + handle->max_key_len;
        } else {
            /* in "meta" region meta + max_key_len + max_val_len are aligned to the 4k, value is written just
             * after end of actual key, not after max_key_len */
            kv->val = obj + sizeof(pmb_obj_meta) + kv->key_len;
        }
    }
    kv->offset = 0;

    tracepoint(pmbackend, pmb_get_exit, handle, obj_id, PMB_OK);
    return PMB_OK;
}

uint8_t
pmb_tput(pmb_handle* handle, uint64_t tx_slot, pmb_pair* kv)
{
	tracepoint(pmbackend, pmb_tput_enter, handle, kv, tx_slot);
    // validate input parameters
	if (handle == NULL || kv == NULL || tx_slot == 0 || tx_slot > handle->op_log.tx_slots_count ||
		kv->key_len > handle->max_key_len ||
		kv->key_len == 0 || kv->key == NULL ||
		(kv->val == NULL && kv->val_len != 0 ) ||
		(kv->val_len == 0 && kv->offset != 0)) {
		logprintf(INVALID_INPUT, "pmb_tput");
		tracepoint(pmbackend, pmb_tput_exit, handle, kv, tx_slot, __LINE__);
		return PMB_ERR;
	}

    // check if write with offset doesn't exceed maximum space for value
    if (kv->offset + kv->val_len > handle->max_val_len) {
        tracepoint(pmbackend, pmb_tput_exit, handle, kv, tx_slot, __LINE__);
        return PMB_ESIZE;
    }

    uint64_t obj_id;
    uint64_t status;
    uint8_t error;
    void *old_obj = NULL;

    if (kv->obj_id && (kv->val_len < (handle->max_val_len / 2))) {
        //  we're performing "small" update
        return tx_slot_op_small_update(handle, tx_slot, kv->obj_id, kv->val,
                                       kv->offset, kv->val_len);
    } else {
        if (kv->obj_id) {
            old_obj = backend_get(handle->backend, kv->obj_id, &error);
            if (old_obj == NULL) {
                tracepoint(pmbackend, pmb_tput_exit, handle, kv, tx_slot, __LINE__);
                return PMB_ENOENT;
            }
        }
        // get new empty block
        status = caslist_pop(handle->free_list, &obj_id);
    }

    if (status != 0) {
        logprintf("pmb_tput: free objects %zu\n", handle->free_list->counter);
        tracepoint(pmbackend, pmb_tput_exit, handle, kv, tx_slot, __LINE__);
        return PMB_ENOSPC; // error: no free objects!
    }

    void *obj = backend_direct(handle->backend, obj_id);

    if (obj == NULL) {
        tracepoint(pmbackend, pmb_tput_exit, handle, kv, tx_slot, __LINE__);
        return PMB_ERR;
    }

    size_t data_len = kv->val_len + kv->offset;
    uint32_t obj_size = sizeof(pmb_obj_meta) + handle->max_key_len + data_len;

    logprintf("tx slot id: %zu\n", tx_slot);

    // register operation in tx log and assign write-cache entry
    logprintf("tx write to obj_id: %zu\n", obj_id);

    if (kv->obj_id) {
        status = tx_slot_op_update(handle, tx_slot, kv->obj_id, obj_id, obj_size);
    } else {
        status = tx_slot_op_write(handle, tx_slot, obj_id, obj_size);
    }

    if (status != PMB_OK) {
        tracepoint(pmbackend, pmb_tput_exit, handle, kv, tx_slot, __LINE__);
        return status;
    }

    pmb_obj_meta *meta = obj;
    pmb_obj_meta *old_meta = NULL;
    meta->key_len = kv->key_len;

    // increment version number if needed
    if (kv->obj_id) {
        // update, increment version number
        old_meta = (pmb_obj_meta *)old_obj;
        meta->version = old_meta->version + 1;
    } else {
        // new write, set version number to "1"
        meta->version = 1;
    }

    void *key = obj + sizeof(pmb_obj_meta);
    void *value = key + handle->max_key_len;

    backend_memcpy(handle->backend, key, kv->key, kv->key_len);

    meta->val_len = data_len;

    if (kv->val_len) {
        if (kv->obj_id && old_meta->val_len > 0) {
            // update to existing block, need to copy old data
            // clean write without offset
            old_obj = old_obj + sizeof(pmb_obj_meta) + handle->max_key_len;
            size_t beginning = kv->offset > old_meta->val_len ? old_meta->val_len : kv->offset;
            backend_memcpy(handle->backend, value, old_obj, beginning);

            if (old_meta->val_len > data_len) {
                uint64_t size_diff = old_meta->val_len - data_len;
                backend_memcpy(handle->backend, value + data_len, old_obj + data_len, size_diff);
                obj_size = obj_size + size_diff;
                meta->val_len = old_meta->val_len;
            }
        }
        backend_memcpy(handle->backend, value + kv->offset, kv->val, kv->val_len);
    }

    util_checksum(obj, obj_size, &meta->flch64, 1);

    logprintf("pmb_put before write obj_id: %zu\n", obj_id);

    kv->obj_id = obj_id;
    tracepoint(pmbackend, pmb_tput_exit, handle, kv, tx_slot, __LINE__);
    return PMB_OK;
}

/*
 * Writes new meta block, offset is ignored, also there's no copying on update.
 *
 * This is transaction-only version.
 */
uint8_t
pmb_tput_meta(pmb_handle* handle, uint64_t tx_slot, pmb_pair* kv)
{
	tracepoint(pmbackend, pmb_tput_enter, handle, kv, tx_slot);
    // validate input parameters
	if (handle == NULL || tx_slot == 0 || tx_slot > handle->op_log.tx_slots_count ||
        kv == NULL || kv->key_len == 0 || kv->key == NULL ||
		kv->val == NULL || kv->val_len == 0 ) {
		logprintf(INVALID_INPUT, "pmb_tput");
		tracepoint(pmbackend, pmb_tput_exit, handle, kv, tx_slot, __LINE__);
		return PMB_ERR;
	}

    if (kv->key_len + kv->val_len > handle->meta_max_key_len + handle->meta_max_val_len) {
        tracepoint(pmbackend, pmb_tput_exit, handle, kv, tx_slot, __LINE__);
        return PMB_ESIZE;
    }

    if (kv->obj_id < handle->total_objs_count && kv->obj_id > 0) {
        tracepoint(pmbackend, pmb_tput_exit, handle, kv, tx_slot, __LINE__);
        return PMB_EWRGID;
    }

    uint8_t status;

    // get old meta version for update
	pmb_obj_meta *old_meta = NULL;
    if (kv->obj_id) {
        old_meta = (pmb_obj_meta *)backend_get(handle->backend, kv->obj_id, &status);
        if (old_meta == NULL) {
            tracepoint(pmbackend, pmb_tput_exit, handle, kv, tx_slot, __LINE__);
            return PMB_ENOENT;
        }
    }

    // get new empty block
    uint64_t obj_id;
    status = caslist_pop(handle->meta_free_list, &obj_id);

    if (status != 0) {
        logprintf("pmb_tput: free objects %zu\n", handle->free_list->counter);
        tracepoint(pmbackend, pmb_tput_exit, handle, kv, tx_slot, __LINE__);
        return PMB_ENOSPC; // error: no free objects!
    }

    void *obj = backend_direct(handle->backend, obj_id);

    if (obj == NULL) {
        tracepoint(pmbackend, pmb_tput_exit, handle, kv, tx_slot, __LINE__);
        return PMB_ERR;
    }

    const uint32_t obj_size = sizeof(pmb_obj_meta) + kv->key_len + kv->val_len;

    // register operation in transaction log
    if (kv->obj_id) {
        // update
        status = tx_slot_op_update(handle, tx_slot, kv->obj_id, obj_id, obj_size);
    } else {
        // write
        status = tx_slot_op_write(handle, tx_slot, obj_id, obj_size);
    }

    if (status != PMB_OK) {
        caslist_push(handle->meta_free_list, obj_id);
        tracepoint(pmbackend, pmb_tput_exit, handle, kv, tx_slot, __LINE__);
        return status;
    }

    pmb_obj_meta *meta = obj;
    meta->key_len = kv->key_len;
    meta->val_len = kv->val_len;

    // increment version number if needed
    if (kv->obj_id) {
        // update, increment version number
        meta->version = old_meta->version + 1;
    } else {
        // new write, set version number to "1"
        meta->version = 1;
    }

    // the actual write of key nad value byte arrays
    void *key = obj + sizeof(pmb_obj_meta);
    void *value = key + kv->key_len;

    backend_memcpy(handle->backend, key,   kv->key, kv->key_len);
    backend_memcpy(handle->backend, value, kv->val, kv->val_len);

    util_checksum(obj, obj_size, &meta->flch64, 1);

    kv->obj_id = obj_id;

    tracepoint(pmbackend, pmb_tput_exit, handle, kv, tx_slot, __LINE__);
    return PMB_OK;
}

uint8_t
pmb_tdel(pmb_handle* handle, uint64_t tx_slot, uint64_t obj_id)
{
	uint8_t ret = 0;
	tracepoint(pmbackend, pmb_tdel_enter, handle, obj_id, tx_slot);
	if (handle == NULL || tx_slot == 0 || tx_slot > handle->op_log.tx_slots_count ||
            obj_id == 0) {
		logprintf(INVALID_INPUT, "pmb_tdel");
		tracepoint(pmbackend, pmb_tdel_exit, handle, obj_id, tx_slot, PMB_ERR);
		return PMB_ERR;
	}

    logprintf("tx del obj_id: %zu\n", obj_id);
    ret = tx_slot_op_remove(handle, tx_slot, obj_id);
    tracepoint(pmbackend, pmb_tdel_exit, handle, obj_id, tx_slot, ret);
	return ret;
}

int64_t
pmb_nfree(pmb_handle* handle, uint8_t region)
{
	tracepoint(pmbackend, pmb_nfree_enter, handle);
	if (handle == NULL) {
		tracepoint(pmbackend, pmb_nfree_exit, handle, 0, __LINE__);
		return 0;
	}
	tracepoint(pmbackend, pmb_nfree_exit, handle, handle->free_list->counter, __LINE__);
	if (region) {
		return caslist_size(handle->meta_free_list);
	}
	return caslist_size(handle->free_list);
}

uint64_t
pmb_ntotal(pmb_handle* handle, uint8_t region)
{
	tracepoint(pmbackend, pmb_ntotal_enter, handle);
	if (handle == NULL) {
		tracepoint(pmbackend, pmb_ntotal_exit, handle, 0,  __LINE__);
		return 0;
	}
	tracepoint(pmbackend, pmb_ntotal_exit, handle, handle->total_objs_count - 1, __LINE__);
	if (region) {
		return handle->meta_objs_count;
	}
	return handle->total_objs_count - 1;
}

/*
 * Iterator handlers
 */

pmb_iter*
pmb_iter_open(pmb_handle* handle, uint8_t region)
{
	caslist* list;
	uint64_t obj = 0;
	tracepoint(pmbackend, pmb_iter_enter, handle);
	if (handle == NULL) {
		tracepoint(pmbackend, pmb_iter_exit, handle, __LINE__);
		return NULL;
	}

	if (region) {
		if (handle->meta_objs_list == NULL) {
			tracepoint(pmbackend, pmb_iter_exit, handle, __LINE__);
			return NULL;
		}
		list = handle->meta_objs_list;
	} else {
		if (handle->objs_list == NULL) {
			tracepoint(pmbackend, pmb_iter_exit, handle, __LINE__);
			return NULL;
		}
		list = handle->objs_list;
	}

    if (caslist_pop(list, &obj) == 0) {
        pmb_iter* iter = (pmb_iter *) malloc (sizeof(pmb_iter));
        if (iter == NULL) {
			tracepoint(pmbackend, pmb_iter_exit, handle, __LINE__);
			return NULL;
		}
        iter->region = region;
        iter->handle = handle;
        iter->vector_pos = obj;
        tracepoint(pmbackend, pmb_iter_exit, handle, __LINE__);
        return iter;
    }
    tracepoint(pmbackend, pmb_iter_exit, handle, __LINE__);
    return NULL;
}

uint64_t
pmb_iter_pos(pmb_iter* iter)
{
    if (iter == NULL) {
        return 0;
    }
    return iter->vector_pos;
}

uint8_t
pmb_iter_close(pmb_iter* iter)
{
	tracepoint(pmbackend, pmb_iter_close_enter, iter);

	if (iter == NULL) {
		return PMB_ERR;
	}

    // relese obj_list
	if (iter->region) {
		if (iter->handle->meta_objs_list != NULL) {
			caslist_free(iter->handle->meta_objs_list);
			iter->handle->meta_objs_list = NULL;
		}
	} else {
		if (iter->handle->objs_list != NULL) {
			caslist_free(iter->handle->objs_list);
			iter->handle->objs_list = NULL;
		}
	}

    free(iter);
    tracepoint(pmbackend, pmb_iter_close_exit, iter);
    return PMB_OK;
}

uint8_t
pmb_iter_get(pmb_iter* iter, pmb_pair* pair)
{
	uint8_t ret = 0;
	if (iter == NULL || pair == NULL) {
		logprintf(INVALID_INPUT, "pmb_iter_get");
		return PMB_ERR;
	}
	tracepoint(pmbackend, pmb_iter_get_enter, iter);
	if (!pmb_iter_valid(iter)) {
		logprintf("pmb_iter_get error: iterator is not valid\n");
		tracepoint(pmbackend, pmb_iter_get_exit, iter, __LINE__);
		return PMB_ERR;
	}
    ret = pmb_get(iter->handle, iter->vector_pos, pair);
    tracepoint(pmbackend, pmb_iter_get_exit, iter, __LINE__);
    return ret;
}

uint8_t
pmb_iter_next(pmb_iter* iter)
{
	if (iter == NULL) {
		logprintf(INVALID_INPUT, "pmb_iter_next");
		return PMB_ERR;
	}
	tracepoint(pmbackend, pmb_iter_next_enter, iter);
    caslist* list;
	if (iter->region) {
		list = iter->handle->meta_objs_list;
	} else {
		list = iter->handle->objs_list;
	}
	if(caslist_pop(list, &(iter->vector_pos)) != 0) {
		tracepoint(pmbackend, pmb_iter_next_exit, iter, __LINE__);
		return PMB_ERR;
	}
	tracepoint(pmbackend, pmb_iter_next_exit, iter, __LINE__);
    return PMB_OK;
}

uint64_t
pmb_iter_valid(pmb_iter* iter)
{
	if (iter == NULL || iter->vector_pos == 0) {
		return 0;
	} else {
        return 1;
    }
}

typedef struct {
    pmb_handle* handle;
    uint64_t recovery_start;
    uint64_t recovery_stop;
} rc_args;

void*
recovery_thread(void* args)
{
    rc_args* rcargs = (rc_args *)args;
    uint64_t border = pmb_ntotal(rcargs->handle, 0);
    uint8_t error;
    caslist* free_list = NULL;
    caslist* obj_list = NULL;
    size_t object_size;
    for (uint64_t pos = rcargs->recovery_start; pos < rcargs->recovery_stop; pos++) {
        if (pos > border) {
			free_list = rcargs->handle->meta_free_list;
			obj_list = rcargs->handle->meta_objs_list;
		} else {
			free_list = rcargs->handle->free_list;
			obj_list = rcargs->handle->objs_list;
		}

        void* obj = backend_get(rcargs->handle->backend, pos, &error);
        if (obj == NULL) {
            caslist_push(free_list, pos);
            continue;
        }

        if (pos > border){
            object_size = sizeof(pmb_obj_meta) + ((pmb_obj_meta *) obj)->key_len + ((pmb_obj_meta *) obj)->val_len;
        } else {
			object_size = sizeof(pmb_obj_meta) + rcargs->handle->max_key_len + ((pmb_obj_meta *) obj)->val_len;
        }

        if (util_checksum(obj, object_size, &((pmb_obj_meta *) obj)->flch64, 0)) {
            // if checksum is correct it belongs to obj_list
            caslist_push(obj_list, pos);
        } else {
            // if checksum is corrupted add it to free list
            caslist_push(free_list, pos);
        }
    }

    return NULL;
}

uint8_t
recovery(pmb_handle* handle)
{
    const uint8_t threads_num = 8; // TODO: option? or cpu_num?
    pthread_t recovery_threads[threads_num];
    rc_args rcargs[threads_num];
    uint64_t part = (pmb_ntotal(handle, 0) + pmb_ntotal(handle, 1)) / threads_num;
    for(int i = 0; i < threads_num; i++) {
        rcargs[i].handle = handle;
        rcargs[i].recovery_start = part * i;
        if (i == 0)
            rcargs[i].recovery_start++; // skip '0' block
        if (i < threads_num - 1) {
            rcargs[i].recovery_stop = part * (i + 1);
        } else {
            rcargs[i].recovery_stop = handle->total_objs_count + handle->meta_objs_count;
        }

        pthread_create(&recovery_threads[i], NULL, recovery_thread, &rcargs[i]);
    }

    for(int i = 0; i < threads_num; i++) {
        pthread_join(recovery_threads[i], NULL);
    }

    return PMB_OK;
}

void
populate_free_list(pmb_handle* handle)
{
    for(uint64_t pos = handle->total_objs_count + handle->meta_objs_count - 1; pos > 0; pos--) {
        if (pos <= handle->total_objs_count - 1) {
            caslist_push(handle->free_list, pos);
        } else {
            caslist_push(handle->meta_free_list, pos);
        }
    }
}

void
pmb_inspect(pmb_handle* handle, uint64_t obj_id)
{
    void* obj = backend_direct(handle->backend, obj_id);
    if (obj == NULL) {
        printf("Object corrupted: NULL obj\n");
        return;
    }
    pmb_obj_meta* meta = obj;
    printf("Object: %zu\n flch64: %zu\nversion: %u\nkey_len: %u\nval_len: %u\n",
            obj_id, meta->flch64, meta->version, meta->key_len, meta->val_len);

    if (meta->key_len > handle->max_key_len) {
        printf("Obejct corrupted: wrong key len\n");
        return;
    }

    printf("key: ");
    char* key = (char*)(obj + sizeof(pmb_obj_meta));

    for(uint32_t i = 0; i < meta->key_len; i++) {
        printf("%c", key[i]);
    }
    printf("\n");

    if (meta->val_len > handle->max_val_len) {
        printf("Obejct corrupted: wrong val len\n");
        return;
    }

    printf("val:\n");
    uint8_t* val = (uint8_t*)(obj + sizeof(pmb_obj_meta) + meta->key_len);
    for(uint64_t i = 0; i < meta->val_len; i++) {
        printf("%2x ", val[i]);
    }
}

uint64_t
pmb_resolve_conflict(pmb_handle* handle, uint64_t obj_id1, uint64_t obj_id2)
{
	void *obj1, *obj2, *delete_ptr;
	uint8_t error;
	uint64_t return_id, delete_id;

	obj1 = backend_get(handle->backend, obj_id1, &error);
	if (obj1 == NULL) {
		printf("Backend error: %d for obj_id1\n", error);
		return PMB_ERR;
	}

	obj2 = backend_get(handle->backend, obj_id2, &error);
	if (obj2 == NULL) {
		printf("Backend error: %d for obj_id2\n", error);
		return PMB_ERR;
	}

	if (((pmb_obj_meta *)obj1)->version > ((pmb_obj_meta *)obj2)->version) {
		return_id = obj_id2;
		delete_id = obj_id1;
		delete_ptr = obj1;
	} else {
		return_id = obj_id1;
		delete_id = obj_id2;
		delete_ptr = obj2;
	}

	backend_set_zero(handle->backend, delete_ptr);
	caslist_push(handle->free_list, delete_id);

	return return_id;
}

const char*
pmb_strerror(int error)
{
    switch(error) {
        case 1: return "generic error\0";
        case 2: return "object doesn't exist\0";
        case 3: return "no free space\0";
        case 4: return "cannot create handle\0";
        case 5: return "cannot write superblock\0";
        case 6: return "superblock corrupted\0";
        case 7: return "superblock invalid\0";
        case 8: return "key or value length invalid\0";
        case 9: return "update object with obeject from different region\0";
        default: return "Invalid error code!\0";
    }
}
