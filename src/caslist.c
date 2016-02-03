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
#include <stdbool.h>
#include "caslist.h"

// caslist constructor
caslist* caslist_new (uint64_t begin, uint64_t end)
{
    if (begin > end || (begin == 0 && end > 0))
        return NULL;

    caslist* list = (caslist*) malloc (sizeof (caslist));
    if (!list)
        return list;
    list->begin = begin;
    list->end = end;
    if (begin == 0 && end == 0)
        list->size = 0;
    else
        list->size = end - begin + 1;
    list->next = NULL;
    pthread_mutex_init (&list->mutex, NULL);
    return list;
}

// returns one element from ranges or 0 if there's none available
uint8_t caslist_pop (caslist* list, uint64_t* val)
{
    if (!list || !val)
        return 1;

    pthread_mutex_lock (&list->mutex);

    if (list->begin == 0 && list->end == 0) {
        *val = 0;
        pthread_mutex_unlock(&list->mutex);
        return 1;
    }

    *val = list->begin;
    list->begin++;

    // check if we depleted current range
    if (list->begin > list->end) {
        // we depleted current range
        if (list->next) {
            // next range is available, get copy boundaries and next range form
            // it and deallocate it
            caslist_link* next = list->next;
            list->begin = next->begin;
            list->end = next->end;
            list->next = next->next;
            free(next);
        } else {
            // next range is not available
            list->begin = 0;
            list->end = 0;
        }
    }

    list->size--;
    pthread_mutex_unlock(&list->mutex);
    return 0;
}

static bool _caslist_put (caslist_link* link, uint64_t val)
{
    if (val >= link->begin && val <= link->end)
        return false;

    if (val == link->begin - 1) {
        // it's just before current range, so just decrease it
        link->begin--;
        return true;
    }

    if (val == link->end + 1) {
        // it's just after current range, so expand it and check if merge with
        // another range is possible
        link->end++;
        if (link->next) {
            // try to merge current current range with next
            if (link->end + 1 == link->next->begin) {
                caslist_link* rm_link = link->next;
                link->end = rm_link->end;
                link->next = rm_link->next;
                free (rm_link);
            }
        }

        return true;
    }

    if (!link->next) {
        // add new range after current
        caslist_link* new_link = (caslist_link*) malloc (sizeof (caslist_link));
        new_link->begin = val;
        new_link->end = val;
        new_link->next = NULL;
        link->next = new_link;
        return true;
    }

    // check next range
    if (val < link->next->begin - 1) {
        // link with existing ranges
        caslist_link* new_link = (caslist_link*) malloc (sizeof (caslist_link));
        new_link->begin = val;
        new_link->end = val;
        new_link->next = link->next;
        link->next = new_link;
        return true;
    }

    // val is in the next range
    return _caslist_put(link->next, val);
}

// expands ranges with val
void caslist_push (caslist* list, uint64_t val)
{
    if (!list || val == 0)
        return;

    pthread_mutex_lock (&list->mutex);

    if(list->begin <= val && list->end >= val)
        goto end;

    // check if main range is empty
    if (list->begin == 0 && list->end == 0) {
        list->begin = val;
        list->end = val;
        list->size++;
        goto end;
    }

    if (val == list->begin - 1) {
        // it's just before current range, so just decrease it
        list->begin--;
        list->size++;
        goto end;
    }

    if (val == list->end + 1) {
        // it's just after current range, so expand it and check if merge with
        // another range is possible
        list->end++;
        if (list->next) {
            // try to merge current current range with next
            if (list->end + 1 == list->next->begin) {
                caslist_link* rm_link = list->next;
                list->end = rm_link->end;
                list->next = rm_link->next;
                free(rm_link);
            }
        }

        list->size++;
        goto end;
    }

    // val could be:
    // - before current range but not mergable
    // - after current range and there's no range after it
    // - between current range an the next range
    // - in the next range
    if (val < list->begin - 1) {
        // prepend new range with value
        caslist_link* new_link = (caslist_link*) malloc (sizeof (caslist_link));
        new_link->begin = list->begin;
        new_link->end = list->end;
        new_link->next = list->next;
        list->next = new_link;
        list->begin = val;
        list->end = val;
        list->size++;
        goto end;
    }

    if (!list->next) {
        // add new range after current
        caslist_link *new_link = (caslist_link *) malloc(sizeof(caslist_link));
        new_link->begin = val;
        new_link->end = val;
        new_link->next = NULL;
        list->next = new_link;
        list->size++;
    } else {
        // check next range
        if (val < list->next->begin - 1) {
            // link with existing ranges
            caslist_link *new_link = (caslist_link *) malloc(sizeof(caslist_link));
            new_link->begin = val;
            new_link->end = val;
            new_link->next = list->next;
            list->next = new_link;
            list->size++;
        } else {
            // val is in the next range
            if (_caslist_put(list->next, val))
                list->size++;
        }
    }

end:
    pthread_mutex_unlock (&list->mutex);
    return;
}

// caslist destructor
void caslist_free (caslist* list)
{
    if (!list)
        return;

    pthread_mutex_lock(&list->mutex);
    if (list->next) {
        caslist_link* link = list->next;
        caslist_link* next;
        while (link) {
            next = link->next;
            free(link);
            link = next;
        }
    }

    pthread_mutex_unlock (&list->mutex);
    pthread_mutex_destroy (&list->mutex);
    free (list);
}

// getter for size
ssize_t caslist_size (caslist* list)
{
    if (!list)
        return 0;

    ssize_t size;
    pthread_mutex_lock (&list->mutex);
    size = list->size;
    pthread_mutex_unlock (&list->mutex);

    return size;
}
