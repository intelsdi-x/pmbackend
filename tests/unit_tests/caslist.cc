/*
 * Copyright (c) 2016, Intel Corporation
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

#include <gtest/gtest.h>
#include <caslist.h>

/*
 * Unit tests for caslist, interface:
 * - caslist* caslist_new (uint64_t begin, uint64_t end)
 * - uint8_t caslist_pop (caslist* list, uint64_t* val)
 * - void caslist_push (caslist* list, uint64_t val);
 * - void caslist_free (caslist* list);
 * - ssize_t caslist_size (caslist* list);
 *
 * Test plan:
 * - caslist_new:
 *   - <0, 0> -> begin = 0, end = 0, size = 0
 *   - <1, 0> -> NULL
 *   - <-1, 0> -> NULL
 *   - <1, 5> -> NULL
 *   - <1, 5> -> begin = 1, end = 5, size = 5
 *   - <5, 10> -> begin = 5, end = 10, size = 6
 *
 * - caslist_pop
 *   - list NULL, pop -> 0
 *   - list <1, 5> (size 5), pop 2 times -> 1, 2;             size = 3, begin = 3, end = 5
 *   - list <1, 5> (size 5), pop all     -> 1, 2, 3, 4, 5;    size = 0, begin = 0, end = 0
 *   - list <1, 5> (size 5), pop all + 1 -> 1, 2, 3, 4, 5, 0; size = 0, begin = 0, end = 0
 *
 * - caslist_push
 *   - list <1, 5> (size 5), push 0 -> begin = 1, end = 5, size = 5
 *   - list <1, 5> (size 5), push 6 -> begin = 1, end = 6, size = 6
 *   - list <1, 5> (size 5), push 7 -> size = 6, n1: begin = 1, end = 5; n2: begin = 7, end = 7
 *   - list <1, 5> (size 5), push 7, 8 -> size = 7, n1: begin = 1, end = 5; n2: begin = 7, end = 8
 *   - list <1, 5> (size 5), push 7, 9 -> size = 7, n1: begin = 1, end = 5; n2: begin = 7, end = 7; n3: begin = 9, end = 9
 *   - list <1, 5> (size 5), push 7, 8, 9 -> size = 8, n1: begin = 1, end = 5; n2: begin = 7, end = 9; n3: NULL
 *   - list <1, 5> (size 5), push 7, 9, 8 -> size = 8, n1: begin = 1, end = 5; n2: begin = 7, end = 9; n3: NULL
 *   - list <1, 5> (size 5), push 7, 9, 8, 11, 13, 12, 10 -> size = 12, n1: begin = 1, end = 5; n2: begin = 7, end = 13; n3: NULL
 *
 * - caslist_free
 *   - destructor, tested with other methods and validated with valgrind
 *
 * - caslist_size
 *   - simple getter, tested with _new/_push/_pop
 */

TEST(caslist, new_empty) {
    caslist *list = caslist_new (0, 0);

    EXPECT_TRUE(NULL != list);
    EXPECT_EQ(0, list->begin);
    EXPECT_EQ(0, list->end);
    EXPECT_EQ(0, list->size);
    caslist_free (list);
}

TEST(caslist, new_inv1) {
    caslist *list = caslist_new (1, 0);

    EXPECT_TRUE(NULL == list);
}

TEST(caslist, new_inv2) {
    caslist *list = caslist_new (-1, 0);

    EXPECT_TRUE(NULL == list);
}

TEST(caslist, new_inv3) {
    caslist *list = caslist_new (0, 5);

    EXPECT_TRUE(NULL == list);
}

TEST(caslist, new_ok1) {
    caslist *list = caslist_new (1, 5);

    EXPECT_TRUE(NULL != list);
    EXPECT_EQ(1, list->begin);
    EXPECT_EQ(5, list->end);
    EXPECT_EQ(5, list->size);
    caslist_free (list);
}

TEST(caslist, new_ok2) {
    caslist *list = caslist_new (5, 10);

    EXPECT_TRUE(NULL != list);
    EXPECT_EQ(5, list->begin);
    EXPECT_EQ(10, list->end);
    EXPECT_EQ(6, list->size);
    caslist_free (list);
}

TEST(caslist, pop1) {
    uint8_t ret = 2;
    uint64_t output = 0;
    caslist *list = caslist_new (1, 5);

    ret = caslist_pop (list, &output);
    EXPECT_EQ(ret, 0);
    EXPECT_EQ(output, 1);
    ret = 2;
    output = 0;
    ret = caslist_pop (list, &output);
    EXPECT_EQ(ret, 0);
    EXPECT_EQ(output, 2);
    EXPECT_EQ(list->begin, 3);
    EXPECT_EQ(list->end, 5);
    EXPECT_EQ(list->size, 3);

    caslist_free (list);
}

TEST(caslist, pop2) {
    uint8_t ret = 2;
    uint64_t output = 0;
    caslist *list = caslist_new (1, 5);

    for (int i = 0; i < 5; i++) {
        ret = caslist_pop (list, &output);
        EXPECT_EQ(ret, 0);
        EXPECT_EQ(output, i + 1);
        ret = 2;
    }
    EXPECT_EQ(list->size, 0);
    EXPECT_EQ(list->begin, 0);
    EXPECT_EQ(list->end, 0);

    caslist_free (list);
}

TEST(caslist, pop3) {
    uint8_t ret = 2;
    uint64_t output = 0;
    caslist *list = caslist_new (1, 5);
    for (int i = 0; i < 6; i++) {
        ret = caslist_pop (list, &output);
        if (i == 5) {
            EXPECT_EQ(ret, 1);
            EXPECT_EQ(output, 0);
        } else {
            EXPECT_EQ(ret, 0);
            EXPECT_EQ(output, i + 1);
        }
        ret = 2;
    }
    EXPECT_EQ(list->size, 0);
    EXPECT_EQ(list->begin, 0);
    EXPECT_EQ(list->end, 0);

    caslist_free (list);
}

TEST(caslist, pop_null) {
    uint8_t ret = 2;
    uint64_t output = 0;
    ret = caslist_pop (NULL, &output);
    EXPECT_EQ(output, 0);
    EXPECT_EQ(ret, 1);
}

TEST(caslist, push0) {
    caslist *list = caslist_new(1, 5);
    caslist_push(list, 0);

    EXPECT_EQ(list->begin, 1);
    EXPECT_EQ(list->end, 5);
    EXPECT_EQ(list->size, 5);

    caslist_free(list);
}

TEST(caslist, push4) {
    caslist *list = caslist_new(1, 5);
    caslist_push(list, 4);

    EXPECT_EQ(list->begin, 1);
    EXPECT_EQ(list->end, 5);
    EXPECT_EQ(list->size, 5);

    caslist_free(list);
}

TEST(caslist, push6) {
    caslist *list = caslist_new(1, 5);
    caslist_push(list, 6);

    EXPECT_EQ(list->begin, 1);
    EXPECT_EQ(list->end, 6);
    EXPECT_EQ(list->size, 6);

    caslist_free(list);
}

TEST(caslist, push7) {
    caslist *list = caslist_new(1, 5);
    caslist_push(list, 7);

    EXPECT_EQ(list->begin, 1);
    EXPECT_EQ(list->end, 5);
    EXPECT_EQ(list->size, 6);
    EXPECT_TRUE(list->next != NULL);

    caslist_link* link = list->next;
    EXPECT_EQ(link->begin, 7);
    EXPECT_EQ(link->end, 7);
    EXPECT_TRUE(link->next == NULL);

    caslist_free(list);
}

TEST(caslist, push78) {
    caslist *list = caslist_new(1, 5);
    caslist_push(list, 7);
    caslist_push(list, 8);

    EXPECT_EQ(list->begin, 1);
    EXPECT_EQ(list->end, 5);
    EXPECT_EQ(list->size, 7);
    EXPECT_TRUE(list->next != NULL);

    caslist_link* link = list->next;
    EXPECT_EQ(link->begin, 7);
    EXPECT_EQ(link->end, 8);
    EXPECT_TRUE(link->next == NULL);

    caslist_free(list);
}

TEST(caslist, push79) {
    caslist *list = caslist_new(1, 5);
    caslist_push(list, 7);
    caslist_push(list, 9);

    EXPECT_EQ(list->begin, 1);
    EXPECT_EQ(list->end, 5);
    EXPECT_EQ(list->size, 7);
    EXPECT_TRUE(list->next != NULL);

    caslist_link* link = list->next;
    EXPECT_EQ(link->begin, 7);
    EXPECT_EQ(link->end, 7);
    EXPECT_TRUE(link->next != NULL);

    link = link->next;
    EXPECT_EQ(link->begin, 9);
    EXPECT_EQ(link->end, 9);
    EXPECT_TRUE(link->next == NULL);

    caslist_free(list);
}

TEST(caslist, push789) {
    caslist *list = caslist_new(1, 5);
    caslist_push(list, 7);
    caslist_push(list, 8);
    caslist_push(list, 9);

    EXPECT_EQ(list->begin, 1);
    EXPECT_EQ(list->end, 5);
    EXPECT_EQ(list->size, 8);
    EXPECT_TRUE(list->next != NULL);

    caslist_link* link = list->next;
    EXPECT_EQ(link->begin, 7);
    EXPECT_EQ(link->end, 9);
    EXPECT_TRUE(link->next == NULL);

    caslist_free(list);
}

TEST(caslist, push798) {
    caslist *list = caslist_new(1, 5);
    caslist_push(list, 7);
    caslist_push(list, 9);
    caslist_push(list, 8);

    EXPECT_EQ(list->begin, 1);
    EXPECT_EQ(list->end, 5);
    EXPECT_EQ(list->size, 8);
    EXPECT_TRUE(list->next != NULL);

    caslist_link* link = list->next;
    EXPECT_EQ(link->begin, 7);
    EXPECT_EQ(link->end, 9);
    EXPECT_TRUE(link->next == NULL);

    caslist_free(list);
}

TEST(caslist, push713) {
    caslist *list = caslist_new(1, 5);
    caslist_push(list, 7);
    caslist_push(list, 9);
    caslist_push(list, 8);
    caslist_push(list, 11);
    caslist_push(list, 13);
    caslist_push(list, 12);
    caslist_push(list, 10);

    EXPECT_EQ(list->begin, 1);
    EXPECT_EQ(list->end, 5);
    EXPECT_EQ(list->size, 12);
    EXPECT_TRUE(list->next != NULL);

    caslist_link* link = list->next;
    EXPECT_EQ(link->begin, 7);
    EXPECT_EQ(link->end, 13);
    EXPECT_TRUE(link->next == NULL);

    caslist_free(list);
}
