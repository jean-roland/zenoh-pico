//
// Copyright (c) 2025 ZettaScale Technology
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

#include <assert.h>
#include <stddef.h>
#include <string.h>

#include "zenoh-pico/collections/jr_hashmap.h"
#include "zenoh-pico/utils/logging.h"

#define EXPAND_LOAD_FACTOR 8  // 8/10th to avoid float

static inline _z_hashmap_jr_t _z_hashmap_jr_null(void) { return (_z_hashmap_jr_t){0}; }

static void _z_hashmap_jr_elem_free(void **e) {
    _z_hashmap_entry_jr_t *ptr = (_z_hashmap_entry_jr_t *)*e;
    if (ptr != NULL) {
        z_free(ptr);
        *e = NULL;
    }
}

static z_result_t _z_hashmap_jr_expand(_z_hashmap_jr_t *map, z_element_hash_f f_hash, z_element_eq_f f_equals) {
    // Expand table if load factor exceeded
    size_t old_capacity = map->_capacity;
    _z_list_t **old_vals = map->_vals;

    map->_capacity *= 2;
    size_t len = map->_capacity * sizeof(_z_list_t *);
    map->_vals = (_z_list_t **)z_malloc(len);
    if (map->_vals == NULL) {
        map->_vals = old_vals;
        map->_capacity = old_capacity;
        _Z_ERROR_RETURN(_Z_ERR_SYSTEM_OUT_OF_MEMORY);
    }
    (void)memset(map->_vals, 0, len);

    // Rehash entries
    for (size_t i = 0; i < old_capacity; i++) {
        _z_list_t *curr_bucket = old_vals[i];
        while (curr_bucket != NULL) {
            _z_hashmap_entry_jr_t *entry = (_z_hashmap_entry_jr_t *)_z_list_value(curr_bucket);
            _Z_RETURN_IF_ERR(_z_hashmap_jr_insert(map, entry->_key, entry->_val, f_hash, f_equals));
            curr_bucket = _z_list_next(curr_bucket);
        }
        _z_list_free(&old_vals[i], _z_hashmap_jr_elem_free);
    }
    z_free(old_vals);
    return _Z_RES_OK;
}

_z_hashmap_jr_t _z_hashmap_jr_init(size_t capacity) {
    _z_hashmap_jr_t map = _z_hashmap_jr_null();
    map._capacity = capacity;
    return map;
}

z_result_t _z_hashmap_jr_insert(_z_hashmap_jr_t *map, void *k, void *v, z_element_hash_f f_hash,
                                z_element_eq_f f_equals) {
    // Lazily allocate and initialize the table
    if (map->_vals == NULL) {
        size_t len = map->_capacity * sizeof(_z_list_t *);
        map->_vals = (_z_list_t **)z_malloc(len);
        if (map->_vals == NULL) {
            _Z_ERROR_RETURN(_Z_ERR_SYSTEM_OUT_OF_MEMORY);
        }
        (void)memset(map->_vals, 0, len);
    } else if (map->_len * 10 >= map->_capacity * EXPAND_LOAD_FACTOR) {
        _Z_RETURN_IF_ERR(_z_hashmap_jr_expand(map, f_hash, f_equals));
    }
    // Retrieve bucket
    size_t idx = f_hash(k) & (map->_capacity - 1);
    _z_list_t *curr_bucket = map->_vals[idx];

    // Check if key already exists
    _z_hashmap_entry_jr_t e = {._key = k, ._val = NULL};
    _z_list_t *curr_entry = _z_list_find(curr_bucket, f_equals, &e);

    // Replace value if key exists
    if (curr_entry != NULL) {
        _z_hashmap_entry_jr_t *h = (_z_hashmap_entry_jr_t *)_z_list_value(curr_entry);
        h->_val = v;
    } else {
        // Insert the new element
        _z_hashmap_entry_jr_t *entry = (_z_hashmap_entry_jr_t *)z_malloc(sizeof(_z_hashmap_entry_jr_t));
        if (entry == NULL) {
            _Z_ERROR_RETURN(_Z_ERR_SYSTEM_OUT_OF_MEMORY);
        }
        entry->_key = k;
        entry->_val = v;
        map->_vals[idx] = _z_list_push(map->_vals[idx], entry);
        map->_len++;
    }
    return _Z_RES_OK;
}

void *_z_hashmap_jr_get(const _z_hashmap_jr_t *map, const void *k, z_element_hash_f f_hash, z_element_eq_f f_equals) {
    if (map->_vals == NULL) {
        return NULL;
    }
    void *ret = NULL;
    size_t idx = f_hash(k) & (map->_capacity - 1);

    _z_hashmap_entry_jr_t e = {._key = (void *)k, ._val = NULL};  // k will not be mutated by this operation
    _z_list_t *xs = _z_list_find(map->_vals[idx], f_equals, &e);
    if (xs != NULL) {
        _z_hashmap_entry_jr_t *h = (_z_hashmap_entry_jr_t *)_z_list_value(xs);
        ret = h->_val;
    }
    return ret;
}

void _z_hashmap_jr_remove(_z_hashmap_jr_t *map, const void *k, z_element_hash_f f_hash, z_element_eq_f f_equals) {
    if (map->_vals == NULL) {
        return;
    }
    size_t idx = f_hash(k) & (map->_capacity - 1);
    _z_hashmap_entry_jr_t e = {._key = (void *)k, ._val = NULL};  // k will not be mutated by this operation
    size_t curr_len = _z_list_len(map->_vals[idx]);
    map->_vals[idx] = _z_list_drop_filter(map->_vals[idx], _z_hashmap_jr_elem_free, f_equals, &e);
    if (curr_len > _z_list_len(map->_vals[idx])) {
        map->_len--;
    }
}

void _z_hashmap_jr_clear(_z_hashmap_jr_t *map) {
    if (map->_vals == NULL) {
        return;
    }
    for (size_t idx = 0; idx < map->_capacity; idx++) {
        _z_list_free(&map->_vals[idx], _z_hashmap_jr_elem_free);
    }
    z_free(map->_vals);
    map->_vals = NULL;
    map->_len = 0;
}

void _z_hashmap_jr_delete(_z_hashmap_jr_t *map) {
    _z_hashmap_jr_clear(map);
    // z_free(map->_vals);
    // map->_vals = NULL;
}