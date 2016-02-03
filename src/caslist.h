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

#ifndef CAS_LIST_H
#define CAS_LIST_H

#include <stdint.h>
#include <pthread.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct _caslist_link {
    uint64_t begin;
    uint64_t end;
    struct _caslist_link* next;
} caslist_link;

typedef struct _caslist {
    ssize_t size;
    uint64_t begin;
    uint64_t end;
    pthread_mutex_t mutex;
    caslist_link* next;
} caslist;

// returns pointer to the new caslist with begin and end defined or NULL
// list with range <begin > 0, end > 0>
// to create empty list: begin = 0, end = 0
// it's not allowed to create list with range: begin = 0, end > 0
caslist* caslist_new (uint64_t begin, uint64_t end);

// gets next value from caslist defined ranges, returns 0 on success, 1 on failure
// val - out value
uint8_t caslist_pop (caslist* list, uint64_t* val);

// adds new val to the caslist ranges, returns 0 on success, 1 on failure
// val - in value
void caslist_push (caslist* list, uint64_t val);

// deallocates the list and associated structures
void caslist_free (caslist* list);

// return combined length of all ranges in the list
ssize_t caslist_size (caslist* list);

#ifdef __cplusplus
}
#endif
#endif //CAS_LIST_H
