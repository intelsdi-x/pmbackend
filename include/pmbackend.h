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

#ifndef PMBACKEND_H
#define PMBACKEND_H 1

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <sys/types.h>
#include <pthread.h>

#define PMB_OK        0
#define PMB_ERR       1 // generic error
#define PMB_ENOENT    2 // requested object don't exists
#define PMB_ENOSPC    3 // no space left on device
#define PMB_ECREAT    4 // cannot create handle
#define PMB_ESBWR     5 // cannot write superblock
#define PMB_ESBCR     6 // superblock corrupted
#define PMB_ESBIN     7 // superblock invalid
#define PMB_ESIZE     8 // key or value exceeded max
#define PMB_EWRGID    9 // ID of data data block to update with meta function or
                         // ID of meta block to update with data function

//#define PMB_ITER_READ		 0
//#define PMB_ITER_CREATE_LST  1

#define PMB_DATA 0
#define PMB_META 1

/*
 * Type of syncing technique
 */
#define PMB_SYNC     0
#define PMB_ASYNC    1
#define PMB_SELSYNC  2
#define PMB_THSYNC   3
#define PMB_NOSYNC   4

/*
 * pmb_handle
 *
 * Main handle to object handle, used by all other functions to put/get/delete
 * objects from pool.
 */
typedef struct _pmb_handle pmb_handle;

/*
 * Structure with key and value related data, used to pass key and values between
 * library and client.
 */
typedef struct {
    uint64_t obj_id;
    uint32_t offset; // for update - start of an update region
    void*    key;
    uint32_t key_len;
    void*    val;
    uint32_t val_len;
} pmb_pair;

/*
 * Sturucture with pmb_handle options.
 * write_log_entries, max_key_len and max_val_len are saved in superblock. When
 * handle is open function checks if provided data matches saved.
 */
typedef struct {
    const char* path;
    uint64_t    data_size;
    uint64_t    meta_size;
    uint8_t     write_log_entries;
    uint32_t    max_key_len;
    uint32_t    max_val_len;
    uint32_t    meta_max_key_len;
	uint32_t    meta_max_val_len;
	uint8_t     sync_type;
} pmb_opts;

/*
 * Parameters:
 * path              - path to handle
 * handle_size        - size of handle in bytes
 * write_log_entries - number of blocks reserved for write log entries
 * max_key_len       - maximal length of handled keys, used to compute block size
 * max_val_len       - maximal length of handled values, used to compute block size
 *
 * Returns:
 * - non-NULL pointer to handle on success
 * - NULL pointer and set error code if operation failed
 */
pmb_handle* pmb_open(pmb_opts* opts, uint8_t* error);

/*
 * Closes handle, dealocates memory used for runtime data.
 *
 * Returns:
 * - PMEMKV_OK if operation succeed
 */
uint8_t pmb_close(pmb_handle* handle);

/*
 * Starts transaction in the handle:
 * - gets free transaction slot,
 * - writes initial metadata to it,
 * - returns transaction id in *tx_slot_id
 */
uint8_t pmb_tx_begin(pmb_handle* handle, uint64_t* tx_slot);

/*
 * Finishes transaction:
 * - changes transaction state from 'processing' to 'commited',
 * - checksums whole transaction slot.
 */
uint8_t pmb_tx_commit(pmb_handle* handle, uint64_t tx_slot);

/*
 * Perform transaction:
 * - clears deleted objects,
 * - clears old objects from update.
 */
uint8_t pmb_tx_execute(pmb_handle* handle, uint64_t tx_slot);

/*
 * Revert changes done by transaction:
 * - clears newly written objects,
 * - removes new objects from update.
 */
uint8_t pmb_tx_abort(pmb_handle* handle, uint64_t tx_slot);

/*
 * If key from key-value pair exists then returns PMEMKV_OK and sets value
 * related fields, otherwise returns error code.
 *
 * Function expects those fields of pmb_pair to be set:
 * key
 * key_len
 *
 * Return:
 * - PMB_OK on success, also sets val and val_len fields in pmb_pair structure
 * - PMB_ENOENT if there's no such key-value pair in handle
 */
uint8_t pmb_get(pmb_handle* handle, uint64_t obj_id, pmb_pair* pair);

/*
 * Functions writing data to the store:
 * - tput: transactional write to data region
 * - tput_meta: transactional wirte to the meta region
 *
 * Function returns PMB_OK and stores written object id in pmb_pair or returns error code.
 *
 * Function expects those fields of pmb_pair to be set:
 * key
 * key_len
 *
 * If obj_id is set to non-zero value in pmb_pair and pmb_tput* verion is used then write will
 * be performed transactionally.
 *
 * val_len is allowed to be 0 only when val is set to NULL
 *
 * Returns:
 * - PMB_OK if operation succeeded and object id is set in pmb_pair
 * - PMB_ERR if arguments are invalid
 * - PMB_ESIZE if write is
 * - PMB_ENOENT
 */
uint8_t pmb_tput(pmb_handle* handle, uint64_t tx_slot, pmb_pair* pair);

uint8_t pmb_tput_meta(pmb_handle* handle, uint64_t tx_slot, pmb_pair* pair);

/*
 * Removes object from pmb_handle, at success returns PMB_OK, otherwise error code.
 *
 * When there's no object for given obj_id then function returns PMB_OK, there's no side
 * effects.
 */
uint8_t pmb_tdel(pmb_handle* handle, uint64_t tx_slot, uint64_t obj_id);

/*
 * Prints information about how many items can be handled in each bucket.
 */
int64_t pmb_nfree(pmb_handle* handle, uint8_t region);

/*
 * Returns number addresable blocks in region.
 */
uint64_t pmb_ntotal(pmb_handle* handle, uint8_t region);

/*
 * For debug purposes only, prints to stdout object,s data and metadata.
 */
void pmb_inspect(pmb_handle* handle, uint64_t obj_id);

uint64_t pmb_resolve_conflict(pmb_handle* handle, uint64_t obj_id1, uint64_t obj_id2);

/*
 * Return error message associated with given error code.
 */
const char* pmb_strerror(int error);

/*
 * Functions related to iterator. Iterator is designed to enumerate all objects
 * on openning time and should be considered as "read only".
 */

/*
 * Structure with runtime data for iterator.
 */
typedef struct pmb_iter pmb_iter;

/*
 * Returns iterator over requested region.
 */
pmb_iter* pmb_iter_open(pmb_handle* handle, uint8_t region);

/*
 * Closes iterator, releases associated data
 */
uint8_t pmb_iter_close(pmb_iter* iter);

/*
 * Returns pmb_pair for current iterator position
 */
uint8_t pmb_iter_get(pmb_iter* iter, pmb_pair* pair);

/*
 * Advances one position in vector and returns pmb_pair, or NULL if was at last pos in vector, work in ro mode.
 */
uint8_t pmb_iter_next(pmb_iter* iter);

/*
 *
 */
uint64_t pmb_iter_valid(pmb_iter* iter);

uint64_t pmb_iter_pos(pmb_iter* iter);

#ifdef __cplusplus
}
#endif
#endif //PMBACKEND_H
