//
// Copyright (c) 2024 ZettaScale Technology
//
// This program and the accompanying materials are made available under the
// terms of the Eclipse Public License 2.0 which is available at
// http://www.eclipse.org/legal/epl-2.0, or the Apache License, Version 2.0
// which is available at https://www.apache.org/licenses/LICENSE-2.0.
//
// SPDX-License-Identifier: EPL-2.0 OR Apache-2.0
//
// Contributors:
//   ZettaScale Zenoh Team, <zenoh@zettascale.tech>
//

#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "zenoh-pico/collections/hashmap_jr.h"
#include "zenoh-pico/collections/list.h"
#include "zenoh-pico/collections/string.h"
#include "zenoh-pico/system/common/platform.h"

#undef NDEBUG
#include <assert.h>

#define HMAP_CAPACITY 10

typedef struct _dummy_t {
    int foo;
} _dummy_t;

static inline void _dummy_elem_clear(void *e) { _z_noop_clear(e); }

_Z_HASHMAP_JR_DEFINE(test, _z_string, _dummy, _z_string_t, _dummy_t)

void test_hashmap_init(void) {
    test_hashmap_t hmap = test_hashmap_jr_init();
    assert(hmap._capacity == _Z_DEFAULT_HASHMAP_JR_CAPACITY);
    assert(hmap._vals == NULL);
}

void test_hashmap_insert(void) {
    test_hashmap_t hmap = test_hashmap_jr_init();

    _z_string_t k0 = _z_string_alias_str("key0");
    _dummy_t v0 = {0};
    assert(hmap._vals == NULL);
    assert(test_hashmap_jr_get(&hmap, &k0) == NULL);
    assert(test_hashmap_jr_insert(&hmap, &k0, &v0) == _Z_RES_OK);
    assert(hmap._vals != NULL);
    _dummy_t *res = test_hashmap_jr_get(&hmap, &k0);
    assert(res != NULL);
    assert(res->foo == v0.foo);

    _dummy_t data[_Z_DEFAULT_HASHMAP_CAPACITY + 1] = {0};
    _z_string_t keys[_Z_DEFAULT_HASHMAP_CAPACITY + 1] = {0};
    char *key_name[_Z_DEFAULT_HASHMAP_CAPACITY + 1] = {"key0",  "key1",  "key2",  "key3",  "key4",  "key5",
                                                       "key6",  "key7",  "key8",  "key9",  "key10", "key11",
                                                       "key12", "key13", "key14", "key15", "key16"};
    for (size_t i = 1; i < _Z_DEFAULT_HASHMAP_CAPACITY + 1; i++) {
        keys[i] = _z_string_alias_str(key_name[i]);
        data[i].foo = (int)i;
        assert(test_hashmap_jr_insert(&hmap, &keys[i], &data[i]) == _Z_RES_OK);
    }
    for (size_t i = 1; i < _Z_DEFAULT_HASHMAP_CAPACITY + 1; i++) {
        res = test_hashmap_jr_get(&hmap, &keys[i]);
        assert(res != NULL);
        assert(res->foo == data[i].foo);
    }
    test_hashmap_jr_delete(&hmap);
}

void test_hashmap_clear(void) {
    test_hashmap_t hmap = test_hashmap_jr_init();

    _dummy_t data[HMAP_CAPACITY] = {0};
    _z_string_t keys[HMAP_CAPACITY] = {0};
    char *key_name[HMAP_CAPACITY] = {"key0", "key1", "key2", "key3", "key4", "key5", "key6", "key7", "key8", "key9"};
    for (size_t i = 0; i < HMAP_CAPACITY; i++) {
        data[i].foo = (int)i;
        keys[i] = _z_string_alias_str(key_name[i]);
        assert(test_hashmap_jr_insert(&hmap, &keys[i], &data[i]) == _Z_RES_OK);
    }
    test_hashmap_jr_clear(&hmap);
    assert(hmap._capacity == _Z_DEFAULT_HASHMAP_JR_CAPACITY);
    assert(hmap._len == 0);
    for (size_t i = 0; i < HMAP_CAPACITY; i++) {
        assert(test_hashmap_jr_get(&hmap, &keys[i]) == NULL);
    }
    test_hashmap_jr_delete(&hmap);
}

void test_hashmap_remove(void) {
    test_hashmap_t hmap = test_hashmap_jr_init();

    _dummy_t data[HMAP_CAPACITY] = {0};
    _z_string_t keys[HMAP_CAPACITY] = {0};
    char *key_name[HMAP_CAPACITY] = {"key0", "key1", "key2", "key3", "key4", "key5", "key6", "key7", "key8", "key9"};

    // Insert values
    for (size_t i = 0; i < HMAP_CAPACITY; i++) {
        keys[i] = _z_string_alias_str(key_name[i]);
        data[i].foo = (int)i;
        assert(test_hashmap_jr_insert(&hmap, &keys[i], &data[i]) == _Z_RES_OK);
    }
    // Remove value and check
    test_hashmap_jr_remove(&hmap, &keys[0]);
    assert(test_hashmap_jr_get(&hmap, &keys[0]) == NULL);
    assert(hmap._len == HMAP_CAPACITY - 1);

    // Check remaining values
    for (size_t i = 1; i < HMAP_CAPACITY; i++) {
        _dummy_t *res = test_hashmap_jr_get(&hmap, &keys[i]);
        assert(res != NULL);
        assert(res->foo == data[i].foo);
    }
    test_hashmap_jr_delete(&hmap);
}

#if 0
#define BENCH_THRESHOLD 10000000

#define LCG_A 1664525
#define LCG_C 0
#define LCG_M 4294967296  // 2^32

typedef struct {
    uint32_t state;
} lcg_state;

void lcg_seed(lcg_state *state, uint32_t seed) { state->state = seed; }

uint32_t lcg_next(lcg_state *state) {
    state->state = (LCG_A * state->state + LCG_C) % LCG_M;
    return state->state;
}

void generate_kv(_z_string_t *key, _dummy_t *val, lcg_state *state, size_t len) {
    assert(key->_slice.start != NULL);
    uint32_t rand_nb = 0;
    for (size_t i = 0; i < len; i++) {
        rand_nb = lcg_next(state);
        ((uint8_t *)key->_slice.start)[i] = (uint8_t)(32 + (rand_nb % 95));  // Random ascii characters
    }
    if (val != NULL) {
        val->foo = (int)rand_nb;
    }
}

void test_op_benchmark(size_t capacity) {
    test_hashmap_t hmap = test_hashmap_jr_init();
    _dummy_t *data = (_dummy_t *)malloc(capacity * sizeof(_dummy_t));
    _z_string_t *keys = (_z_string_t *)malloc(capacity * sizeof(_z_string_t));
    _z_string_t *bad_keys = (_z_string_t *)malloc(capacity * sizeof(_z_string_t));
    assert(bad_keys != NULL);
    assert(keys != NULL);
    assert(data != NULL);

    memset(data, 0, capacity * sizeof(_dummy_t));
    memset(keys, 0, capacity * sizeof(_z_string_t));
    memset(bad_keys, 0, capacity * sizeof(_z_string_t));

    lcg_state _lcg_state;
    lcg_seed(&_lcg_state, 0x55);
    srand(0x55);

    // Generate keys and values
    size_t key_len = 8;
    for (size_t i = 0; i < capacity; i++) {
        keys[i] = _z_string_preallocate(key_len);
        bad_keys[i] = _z_string_preallocate(key_len);
        generate_kv(&keys[i], &data[i], &_lcg_state, key_len);
        generate_kv(&bad_keys[i], NULL, &_lcg_state, key_len);
    }
    // Insert data
    z_clock_t measure_start = z_clock_now();
    for (size_t i = 0; i < capacity; i++) {
        assert(test_hashmap_jr_insert(&hmap, &keys[i], &data[i]) == _Z_RES_OK);
    }
    unsigned long elapsed_us = z_clock_elapsed_us(&measure_start);
    printf("%ld\n", elapsed_us);
    // Test: Get existing keys
    measure_start = z_clock_now();
    for (size_t get_cnt = 0; get_cnt <= BENCH_THRESHOLD; get_cnt++) {
        size_t key_idx = (size_t)rand() % capacity;
        _dummy_t *res = test_hashmap_jr_get(&hmap, &keys[key_idx]);
        if (res == NULL)
            printf("Key not found! %ld %ld '%.*s'\n", capacity, key_idx, (int)keys[key_idx]._slice.len,
                   (char *)keys[key_idx]._slice.start);
        fflush(stdout);
        assert(res != NULL);
    }
    elapsed_us = z_clock_elapsed_us(&measure_start);
    printf("%ld\n", elapsed_us);

    // Test: Get non-existing keys
    measure_start = z_clock_now();
    for (size_t get_cnt = 0; get_cnt <= BENCH_THRESHOLD; get_cnt++) {
        size_t key_idx = (size_t)rand() % capacity;
        _dummy_t *res = test_hashmap_jr_get(&hmap, &bad_keys[key_idx]);
        assert(res == NULL);  // Ensure the key doesn't exist
    }
    elapsed_us = z_clock_elapsed_us(&measure_start);
    printf("%ld\n", elapsed_us);

    // Test: Remove keys
    measure_start = z_clock_now();
    for (size_t get_cnt = 0; get_cnt <= BENCH_THRESHOLD; get_cnt++) {
        size_t key_idx = (size_t)rand() % capacity;
        test_hashmap_jr_remove(&hmap, &keys[key_idx]);
    }
    elapsed_us = z_clock_elapsed_us(&measure_start);
    printf("%ld\n", elapsed_us);
    free(data);
    test_hashmap_jr_delete(&hmap);
    for (size_t i = 0; i < capacity; i++) {
        _z_string_clear(&keys[i]);
        _z_string_clear(&bad_keys[i]);
    }
    free(keys);
    free(bad_keys);
}

void test_benchmark(void) {
    for (size_t i = 1; i <= BENCH_THRESHOLD; i *= 10) {
        test_op_benchmark(i);
    }
}
#endif

int main(void) {
    test_hashmap_init();
    test_hashmap_insert();
    test_hashmap_clear();
    test_hashmap_remove();
#if 0
    test_benchmark();
#endif
    return 0;
}
