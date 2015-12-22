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

#include <stdlib.h>
#include <stdio.h>

#include "caslist.h"

#ifdef WITH_LTTNG
#define TRACEPOINT_CREATE_PROBES
#define TRACEPOINT_DEFINE
#include "caslist-lttng.h"
#else
#define tracepoint(...)
#endif

caslist *
caslist_new(uint64_t size)
{
    if (size == 0)
        return NULL;

    caslist *list = malloc(sizeof(caslist));
    if (list == NULL) {
        return NULL;
    }
    list->size = size;
    list->data = (uint64_t *)malloc(size * sizeof(uint64_t));

    pthread_mutex_init(&list->lock_mtx, NULL);

    list->counter = 0;
    return list;
}

void
caslist_free(caslist *list)
{
    pthread_mutex_lock(&list->lock_mtx);

    free(list->data);

    pthread_mutex_unlock(&list->lock_mtx);
    pthread_mutex_destroy(&list->lock_mtx);

    free(list);
}

/*
 * Put element at the end of the list
 */
void
caslist_push(caslist *list, uint64_t data)
{
	tracepoint(caslist, caslist_push_enter);
    pthread_mutex_lock(&list->lock_mtx);

    if (list->counter < list->size) {
        list->data[list->counter] = data;
        list->counter++;
    }

    pthread_mutex_unlock(&list->lock_mtx);
    tracepoint(caslist, caslist_push_exit);
}

/*
 * Get element from beginning of the list
 */
uint8_t
caslist_pop(caslist *list, uint64_t *obj_id_pt)
{
    pthread_mutex_lock(&list->lock_mtx);
    if (list->counter > 0) {
        list->counter--;
        *obj_id_pt = list->data[list->counter];
        list->data[list->counter] = 0;
    } else {
        *obj_id_pt = 0;
    }

    pthread_mutex_unlock(&list->lock_mtx);

    if (*obj_id_pt == 0) {
        return 1;
    }
    return 0;
}
