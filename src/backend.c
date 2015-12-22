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


#include "backend.h"
#include "libpmem.h"
#include "kv.h"

// NVML's internals
#include "out.h"
#include "util.h"

#include <sys/param.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <assert.h>
#include <sys/mman.h>
#include <inttypes.h>

#ifdef WITH_LTTNG
#define TRACEPOINT_CREATE_PROBES
#define TRACEPOINT_DEFINE
#include "backend-lttng.h"
#else
#define tracepoint(...)
#endif

#define PMB_MIN_POOL          20 * 1024 * 1024
#define PMB_FORMAT_DATA_ALIGN 4096
#define PMB_HDR_SIG           "PMBACKEN"
#define PMB_FORMAT_MAJOR      0x0001
#define PMB_FORMAT_INCOMPAT   0x0000
#define PMB_FORMAT_RO_COMPAT  0x0000
#define PMB_FORMAT_COMPAT     0x0000

typedef void (*persist_fn)(void *, size_t);
typedef void (*flush_fn)(void *, size_t);
typedef void (*drain_fn)(void);
typedef void *(*copy_fn)(void *, const void *, size_t);

extern unsigned long Pagesize;

struct _backend {
    struct pool_hdr hdr;
    uint32_t        bsize;
    uint32_t        meta_bsize;
    size_t          size;			/* size of mapped region */
    int             is_pmem;			/* true if pool is PMEM */
    size_t          datasize;		/* size of data area */
    size_t	    	metasize;
    size_t          data_nlba;
    size_t          meta_nlba;
    uint32_t        max_key_len;
    uint32_t        max_val_len;
    uint32_t        meta_max_key_len;
    uint32_t        meta_max_val_len;
    uint8_t         tx_slots_count;
    size_t          tx_slot_size;
    uint8_t         sync_type;
    persist_fn      persist;
    persist_fn      weak_persist;
    flush_fn        flush;
    drain_fn        drain;
    copy_fn         memcpy;
    void           *addr;    // beggining of the file
    void           *tx_log;  // start of transaction log
    void           *data;    // start of data area
    void           *meta;    // start of metadata area
    uint64_t        flch64;
};

/*
 * drain_empty -- (internal) empty function for drain on non-pmem memory
 */
static void
drain_empty(void)
{
	/* do nothing */
}

/*
 * msync_weak -- (internal) asynchronous msync for non-pmem
 */
static void
msync_weak(void *addr, size_t length)
{
	/* increase len by the amount we gain when we round addr down */
	length += (uintptr_t)addr & (Pagesize - 1);

	/* round addr down to page boundary */
	uintptr_t uptr = (uintptr_t)addr & ~(Pagesize - 1);

	if(msync((void *)uptr, length, MS_ASYNC)) {
        printf("msync_weak: %s %" PRIxPTR  "%lu\n", strerror(errno), uptr, length);
        assert(0);
    }
}

/*
 * empty_weak_persist -- (internal) empty weak_persist on pmem memory
 */
static void
empty_weak_persist(void *addr, size_t length)
{

}

/*
 * _backendk_map_common -- (internal) map a block memory pool
 *
 * This routine does all the work, but takes a rdonly flag so internal
 * calls can map a read-only pool if required.
 *
 * If empty flag is set, the file is assumed to be a new memory pool, and
 * a new pool header is created.  Otherwise, a valid pool header must exist.
 *
 * Passing in bsize == 0 means a valid pool header must exist (which
 * will supply the block size).
 */
static struct _backend*
_backend_map_common(struct pool_set* set, size_t poolsize, size_t meta_poolsize,
        size_t bsize, size_t meta_bsize, int rdonly, int initialize,
        uint8_t tx_slots_count, size_t tx_slot_size,
        uint32_t max_key_len, uint32_t max_val_len,
		uint32_t meta_max_key_len, uint32_t meta_max_val_len,
        uint8_t sync_type)
{
	LOG(3, "poolsize %zu meta_poolsize %zu bsize %zu meta_bsize %zu rdonly %d initialize %d",
			poolsize, meta_poolsize, bsize, meta_bsize, rdonly, initialize);


    struct pool_replica* rep = set->replica[0];
    struct _backend* backend = rep->part[0].addr;
    backend->addr = backend;

	/* check if the mapped region is located in persistent memory */
	int is_pmem = pmem_is_pmem(backend->addr, poolsize + meta_poolsize);

	/* opaque info lives at the beginning of mapped memory pool */
	//struct _backend *pbp = addr;

	bsize = roundup(bsize, PMB_FORMAT_DATA_ALIGN);
	meta_bsize = roundup(meta_bsize, PMB_FORMAT_DATA_ALIGN);
	tx_slot_size = roundup(tx_slot_size, PMB_FORMAT_DATA_ALIGN);

	if (!initialize) {
		size_t hdr_bsize = le32toh(backend->bsize);
		if (bsize && bsize != hdr_bsize) {
			LOG(1, "wrong bsize (%zu), pool created with bsize %zu",
					bsize, hdr_bsize);
			errno = EINVAL;
			goto err;
		}
		bsize = hdr_bsize;
		LOG(3, "using block size from header: %zu", bsize);

		size_t meta_hdr_bsize = le32toh(backend->meta_bsize);
		if (meta_bsize && meta_bsize != meta_hdr_bsize) {
			LOG(1, "wrong meta_bsize (%zu), pool created with meta_bsize %zu",
					meta_bsize, meta_hdr_bsize);
			errno = EINVAL;
			goto err;
		}
		meta_bsize = meta_hdr_bsize;
		LOG(3, "using meta block size from header: %zu", meta_bsize);
	} else {
		LOG(3, "creating new memory pool");

		ASSERTeq(rdonly, 0);

		/* check if bsize is valid */
		if (bsize == 0) {
			LOG(1, "Invalid block size %zu", bsize);
			errno = EINVAL;
			goto err;
		}

		/* check if meta_bsize is valid */
		if (meta_bsize == 0) {
			LOG(1, "Invalid meta block size %zu", meta_bsize);
			errno = EINVAL;
			goto err;
		}

		/* create the required metadata first */
		backend->bsize = htole32(bsize);
		pmem_msync(&backend->bsize, sizeof (bsize));

		backend->meta_bsize = htole32(meta_bsize);
		pmem_msync(&backend->meta_bsize, sizeof (meta_bsize));

		backend->max_key_len = htole32(max_key_len);
		pmem_msync(&backend->max_key_len, sizeof (max_key_len));

		backend->max_val_len = htole32(max_val_len);
		pmem_msync(&backend->max_val_len, sizeof (max_val_len));

		backend->meta_max_key_len = htole32(meta_max_key_len);
		pmem_msync(&backend->meta_max_key_len, sizeof (meta_max_key_len));

		backend->meta_max_val_len = htole32(meta_max_val_len);
		pmem_msync(&backend->meta_max_val_len, sizeof (meta_max_val_len));

		backend->tx_slot_size = htole32(tx_slot_size);
		pmem_msync(&backend->tx_slot_size, sizeof(backend->tx_slot_size));

		backend->tx_slots_count = tx_slots_count;
		pmem_msync(&backend->tx_slots_count, sizeof(backend->tx_slots_count));

		/* store pool's header */
		pmem_msync(backend, sizeof (*backend));
	}

	/*
	 * Use some of the memory pool area for run-time info.  This
	 * run-time state is never loaded from the file, it is always
	 * created here, so no need to worry about byte-order.
	 */
	backend->size = poolsize + meta_poolsize;
	backend->is_pmem = is_pmem;
	backend->tx_log = backend->addr + roundup(sizeof (*backend), PMB_FORMAT_DATA_ALIGN);
	backend->data = backend->tx_log + tx_slots_count * tx_slot_size;
	backend->datasize = (backend->addr + poolsize) - backend->data;
	backend->data_nlba = backend->datasize / backend->bsize;
	backend->meta = backend->data + backend->data_nlba * backend->bsize;
	backend->metasize = (backend->addr + poolsize + meta_poolsize) - backend->meta;
	backend->meta_nlba = backend->metasize / backend->meta_bsize;
	backend->sync_type = sync_type;

	if (backend->is_pmem) {
		backend->persist = pmem_persist;
		backend->weak_persist = empty_weak_persist;
		backend->flush = pmem_flush;
		backend->drain = pmem_drain;
		backend->memcpy = pmem_memcpy_persist;
	} else {
		backend->persist = (persist_fn)pmem_msync;
		backend->weak_persist = msync_weak;
	    backend->flush = (flush_fn)pmem_msync;
		backend->drain = drain_empty;
		backend->memcpy = memcpy;
	}

	LOG(4, "data area %p data size %zu bsize %zu",
		backend->data, backend->datasize, bsize);
	LOG(4, "meta area %p meta size %zu meta_bsize %zu",
			backend->meta, backend->metasize, meta_bsize);

    printf("address %p tx_log %p data area %p data size %zu bsize %u, meta area\
            %p meta size %zu meta_bsize %u\n", backend->addr, backend->tx_log,
            backend->data, backend->datasize, backend->bsize, backend->meta,
            backend->metasize, backend->meta_bsize);

	/*
	 * If possible, turn off all permissions on the pool header page.
	 *
	 * The prototype PMFS doesn't allow this when large pages are in
	 * use. It is not considered an error if this fails.
	 */
	util_range_ro(backend->addr, sizeof (struct _backend));
	mlock(backend->addr, sizeof (struct _backend));

	LOG(3, "backend %p", backend);
	return backend;

err:
	LOG(4, "error clean up");
	int oerrno = errno;
	errno = oerrno;
	return NULL;
}

struct _backend*
backend_create(const char *path, size_t data_size, size_t meta_size,
        size_t tx_slots, size_t tx_slot_size,
        uint32_t max_key_len, uint32_t max_val_len,
		uint32_t meta_max_key_len, uint32_t meta_max_val_len,
        mode_t mode, uint8_t sync_type)
{
    size_t bsize = sizeof(pmb_obj_meta) + max_key_len + max_val_len;
    size_t meta_bsize = sizeof(pmb_obj_meta) + meta_max_key_len + meta_max_val_len;

    LOG(3, "path %s bsize %zu poolsize %zu mode %d",
			path, bsize, data_size, mode);
    LOG(3, "path %s meta_bsize %zu poolsize %zu mode %d",
            path, meta_bsize, meta_bsize, mode);

	int created = 0;
	int ret;
	size_t total_size = data_size + meta_size;
    struct pool_set* set;
	if (total_size != 0) {
		/* create a new memory pool file XXX descriptor is returned in set struct */
		ret = util_pool_create(&set, path, total_size, PMB_MIN_POOL,
                roundup(sizeof(struct _backend), PMB_FORMAT_DATA_ALIGN),
                PMB_HDR_SIG, PMB_FORMAT_MAJOR, PMB_FORMAT_COMPAT,
                PMB_FORMAT_INCOMPAT, PMB_FORMAT_RO_COMPAT);
		created = 1;
	} else {
		/* open an existing file XXX descriptor is returned in set struct */
		ret = util_pool_open(&set, path, 0, PMB_MIN_POOL,
                roundup(sizeof(struct _backend), PMB_FORMAT_DATA_ALIGN),
                PMB_HDR_SIG, PMB_FORMAT_MAJOR, PMB_FORMAT_COMPAT,
                PMB_FORMAT_INCOMPAT, PMB_FORMAT_RO_COMPAT);
	}

	if (ret != 0) {
		return NULL;	/* errno set by util_pool_create/open() */
    }

	struct _backend* backend = _backend_map_common(set, data_size, meta_size,
            bsize, meta_bsize, 0, created, tx_slots, tx_slot_size,
            max_key_len, max_val_len, meta_max_key_len, meta_max_val_len,
            sync_type);

    if (created) {
        util_poolset_chmod(set, mode);
    }

    util_poolset_fdclose(set);
    util_poolset_free(set);

    return backend;
}

struct _backend*
backend_open(const char *path, size_t data_size, size_t meta_size,
        size_t tx_slots, size_t tx_slot_size,
        uint32_t max_key_len, uint32_t max_val_len,
		uint32_t meta_max_key_len, uint32_t meta_max_val_len,
        uint8_t sync_type)
{
    size_t bsize = sizeof(pmb_obj_meta) + max_key_len + max_val_len;
    size_t meta_bsize = sizeof(pmb_obj_meta) + meta_max_key_len + meta_max_val_len;

    LOG(3, "path %s bsize %zu", path, bsize);
    LOG(3, "path %s meta_bsize %zu", path, meta_bsize);

	int ret;

    struct pool_set* set;
	if ((ret = util_pool_open(&set, path, 0, PMB_MIN_POOL,
                    roundup(sizeof (struct _backend), PMB_FORMAT_DATA_ALIGN),
                    PMB_HDR_SIG, PMB_FORMAT_MAJOR, PMB_FORMAT_COMPAT,
                    PMB_FORMAT_INCOMPAT, PMB_FORMAT_RO_COMPAT)) == -1) {
		return NULL;	/* errno set by util_pool_open() */
    }

	struct _backend* backend = _backend_map_common(set, data_size, meta_size,
            bsize, meta_bsize, 0, 0, tx_slots, tx_slot_size,
            max_key_len, max_val_len, meta_max_key_len, meta_max_val_len,
            sync_type);

    util_poolset_fdclose(set);
    util_poolset_free(set);

    return backend;
}

void
backend_close(struct _backend *backend)
{
    if (backend != NULL) {
        munlock(backend, sizeof(*backend));
        util_unmap(backend->addr, backend->size);
    }
}


inline uint8_t
backend_get_sync_type(struct _backend* backend)
{
    return backend->sync_type;
}

void*
backend_direct(struct _backend* backend, uint64_t obj_id)
{
	tracepoint(pmem_backend, backend_direct_enter);
    if (backend == NULL) {
        tracepoint(pmem_backend, backend_direct_exit);
        return NULL;
    }

    if (obj_id < backend->data_nlba) {
        tracepoint(pmem_backend, backend_direct_exit);
        return backend->data + obj_id * backend->bsize;
    } else if (obj_id < backend->data_nlba + backend->meta_nlba) {
        tracepoint(pmem_backend, backend_direct_exit);
        return backend->meta + (obj_id - backend->data_nlba -1) * backend->meta_bsize;
    }
    tracepoint(pmem_backend, backend_direct_exit);
    return NULL;
}


void*
backend_get(struct _backend* backend, uint64_t obj_id, uint8_t *error)
{
	tracepoint(pmem_backend, backend_get_enter);
	void *obj_ptr;
    if (backend == NULL || !obj_id) {
        printf("backend: Error for %zu empty backend\n", obj_id);
        *error = BACKEND_NO_BACKEND;
        tracepoint(pmem_backend, backend_get_exit);
        return NULL;
    }

    obj_ptr = backend_direct(backend, obj_id);

    if (obj_ptr == NULL) {
        *error = BACKEND_INV_ID;
		logprintf("backend: Error for %zu: is higher than nlba %zu\n", obj_id, backend->data_nlba);
		tracepoint(pmem_backend, backend_get_exit);
		return NULL;
    }

	if (((pmb_obj_meta *)obj_ptr)->flch64 == 0) {
		*error = BACKEND_ENOENT;
//		logprintf("backend: Error for %zu: 0 checksum %p\n", obj_id, obj_ptr);
		tracepoint(pmem_backend, backend_get_exit);
		return NULL;
	}

	tracepoint(pmem_backend, backend_get_exit);
	return obj_ptr;
}

inline uint8_t
backend_persist(struct _backend* backend, void *obj_ptr, size_t size)
{
	tracepoint(pmem_backend, backend_persist_enter, obj_ptr, size);
    if (backend == NULL) {
        tracepoint(pmem_backend, backend_persist_exit, obj_ptr, size);
        return BACKEND_NO_BACKEND;
    }

    if (obj_ptr == NULL) {
	    tracepoint(pmem_backend, backend_persist_exit, obj_ptr, size);
        return BACKEND_ENOENT;
    }

/*    if (backend->sync_type == 0) {
        backend->persist(backend->tx_log, backend->datasize + backend->metasize); // SYNC
    } else*/
    if (backend->sync_type == 2) {
        backend->persist(obj_ptr, size); // SELSYNC
    } else if (backend->sync_type == 1) {
        backend->weak_persist(obj_ptr, size); // ASYNC
    }
    // NOSYNC if sync_type != 0 | 1

    tracepoint(pmem_backend, backend_persist_exit, obj_ptr, size);
    return BACKEND_OK;
}

inline uint8_t
backend_persist_all(struct _backend* backend)
{
	tracepoint(pmem_backend, backend_persist_enter, obj_ptr, size);
    if (backend == NULL) {
        tracepoint(pmem_backend, backend_persist_exit, obj_ptr, size);
        return BACKEND_NO_BACKEND;
    }

    backend->persist(backend->tx_log, backend->datasize + backend->metasize);

    tracepoint(pmem_backend, backend_persist_exit, obj_ptr, size);
    return BACKEND_OK;
}

uint8_t
backend_persist_weak(struct _backend* backend, void *obj_ptr, size_t size)
{
    if (backend == NULL)
        return BACKEND_NO_BACKEND;

    if (obj_ptr == NULL) {
        return BACKEND_ENOENT;
    }

    backend->weak_persist(obj_ptr, size);

    return BACKEND_OK;
}

void*
backend_tx_direct(struct _backend* backend, uint8_t tx_id)
{
	tracepoint(pmem_backend, backend_tx_direct_enter);
    if (backend == NULL) {
        tracepoint(pmem_backend, backend_tx_direct_exit);
        return NULL;
    }

    if (tx_id < backend->tx_slots_count) {
        tracepoint(pmem_backend, backend_tx_direct_exit);
        return backend->tx_log + tx_id * backend->tx_slot_size;
    }

    tracepoint(pmem_backend, backend_tx_direct_exit);
    return NULL;
}

uint8_t
backend_tx_persist(struct _backend* backend, uint8_t tx_id, size_t size)
{
	tracepoint(pmem_backend, backend_tx_persist_enter, tx_id, size);
    if (backend == NULL) {
        tracepoint(pmem_backend, backend_tx_persist_exit, tx_id, size);
        return BACKEND_NO_BACKEND;
    }

    if (backend->sync_type == 0) {
        backend_persist_all(backend);
        tracepoint(pmem_backend, backend_tx_persist_exit, tx_id, size);
        return BACKEND_OK;
    }

    void *obj = backend_tx_direct(backend, tx_id);
    if (obj == NULL) {
        tracepoint(pmem_backend, backend_tx_persist_exit, tx_id, size);
        return BACKEND_ENOENT;
    }

    backend_persist(backend, obj, size);

    tracepoint(pmem_backend, backend_tx_persist_exit, tx_id, size);
    return BACKEND_OK;
}

uint8_t
backend_set_zero(struct _backend* backend, void *obj_ptr)
{
	tracepoint(pmem_backend, backend_set_zero_enter);
    if (backend == NULL) {
        tracepoint(pmem_backend, backend_set_zero_exit);
        return BACKEND_NO_BACKEND;
    }

    if (obj_ptr == NULL) {
        tracepoint(pmem_backend, backend_set_zero_exit);
        return BACKEND_ENOENT;
    }

	uint64_t size = sizeof(pmb_obj_meta) + ((pmb_obj_meta *)obj_ptr)->key_len + ((pmb_obj_meta *)obj_ptr)->val_len;

	tracepoint(pmem_backend, memset_enter);
	memset(obj_ptr, 0, size);

//	tracepoint(pmem_backend, weak_persist_enter);
//	backend->weak_persist(obj_ptr, size);
//	tracepoint(pmem_backend, weak_persist_exit);

	tracepoint(pmem_backend, backend_set_zero_exit);
    return 0;
}

uint8_t
backend_tx_set_zero(struct _backend *backend, void *slot_ptr)
{
	tracepoint(pmem_backend, backend_tx_set_zero_enter);
    if (backend == NULL) {
        tracepoint(pmem_backend, backend_tx_set_zero_exit);
        return BACKEND_NO_BACKEND;
    }

    if (slot_ptr == NULL) {
        tracepoint(pmem_backend, backend_tx_set_zero_exit);
        return BACKEND_ENOENT;
    }

	// we're zeroing only 2 first entrances of slot: flch64 and txstatus
	uint64_t size = sizeof(uint64_t) + sizeof(tx_status);

	tracepoint(pmem_backend, memset_enter);
	memset(slot_ptr, 0, size);
	tracepoint(pmem_backend, memset_exit);

	tracepoint(pmem_backend, persist_enter);
	if (backend->sync_type == 0) {
	    backend_persist_all(backend);
	} else {
	    backend_persist(backend, slot_ptr, size);
	}
	tracepoint(pmem_backend, persist_exit);

	tracepoint(pmem_backend, backend_tx_set_zero_exit);
    return 0;
}

size_t
backend_nblock(struct _backend *backend, int meta_store)
{
    if (backend == NULL)
        return 0;
    if (meta_store){
        return backend->meta_nlba;
    } else {
        return backend->data_nlba;
    }
}

void *
backend_memcpy(struct _backend *backend, void *dest, const void *src, size_t num)
{
	return backend->memcpy(dest, src, num);
}
