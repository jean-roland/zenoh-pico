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

#include "zenoh-pico/collections/hashmap_jr.h"

#include <assert.h>
#include <stddef.h>
#include <string.h>

#include "zenoh-pico/utils/logging.h"
#include "zenoh-pico/utils/pointers.h"

#define EXPAND_LOAD_FACTOR 9  // Expand at 90% load
#define INDEX_WRAP(idx, capacity) ((idx) & (capacity - 1))

static inline void *_z_hashmap_jr_entry_key(_z_hashmap_jr_entry_t *entry) { return (void *)entry; }

static inline void *_z_hashmap_jr_entry_value(_z_hashmap_jr_entry_t *entry, size_t key_size) {
    return (void *)_z_ptr_u8_offset((uint8_t *)entry, (ptrdiff_t)key_size);
}

static inline _z_hashmap_jr_t _z_hashmap_jr_null(void) { return (_z_hashmap_jr_t){0}; }

static inline bool _z_hashmap_jr_entry_is_empty(_z_hashmap_jr_entry_t *entry, size_t key_size) {
    uint8_t *bytes = (uint8_t *)_z_hashmap_jr_entry_key(entry);
    for (size_t i = 0; i < key_size; i++) {
        if (bytes[i] != 0xFF) {
            return false;
        }
    }
    return true;
}

static inline z_result_t _z_hashmap_jr_expand(_z_hashmap_jr_t *map, z_element_hash_f f_hash, z_element_eq_f f_equals,
                                              size_t key_size, size_t val_size) {
    // Expand table if load factor exceeded
    size_t old_capacity = map->_capacity;
    _z_hashmap_jr_entry_t *old_vals = map->_vals;

    map->_capacity *= 2;
    size_t map_size = map->_capacity * (key_size + val_size);
    map->_vals = z_malloc(map_size);
    if (map->_vals == NULL) {
        map->_vals = old_vals;
        map->_capacity = old_capacity;
        _Z_ERROR_RETURN(_Z_ERR_SYSTEM_OUT_OF_MEMORY);
    }
    (void)memset(map->_vals, 0xff, map_size);
    map->_len = 0;
    // Reinsert old entries
    for (size_t idx = 0; idx < old_capacity; idx++) {
        _z_hashmap_jr_entry_t *old_entry =
            (void *)_z_ptr_u8_offset((uint8_t *)old_vals, (ptrdiff_t)(idx * (key_size + val_size)));
        if (_z_hashmap_jr_entry_is_empty(old_entry, key_size)) {
            continue;
        }
        // Reinsert entry
        void *re_key = _z_hashmap_jr_entry_key(old_entry);
        void *re_val = _z_hashmap_jr_entry_value(old_entry, key_size);
        _z_hashmap_jr_insert(map, re_key, re_val, f_hash, f_equals, key_size, val_size);
    }
    z_free(old_vals);
    return _Z_RES_OK;
}

_z_hashmap_jr_t _z_hashmap_jr_init(size_t capacity) {
    _z_hashmap_jr_t map = _z_hashmap_jr_null();
    map._capacity = capacity;
    return map;
}

z_result_t _z_hashmap_jr_insert(_z_hashmap_jr_t *map, void *key, void *val, z_element_hash_f f_hash,
                                z_element_eq_f f_equals, size_t key_size, size_t val_size) {
    // Cannot insert "empty" key
    assert(!_z_hashmap_jr_entry_is_empty(key, key_size));

    // Lazily allocate and initialize the table
    if (map->_vals == NULL) {
        size_t map_size = map->_capacity * (key_size + val_size);
        map->_vals = z_malloc(map_size);
        if (map->_vals == NULL) {
            _Z_ERROR_RETURN(_Z_ERR_SYSTEM_OUT_OF_MEMORY);
        }
        (void)memset(map->_vals, 0xff, map_size);
    } else if (map->_len * 10 >= map->_capacity * EXPAND_LOAD_FACTOR) {
        _Z_RETURN_IF_ERR(_z_hashmap_jr_expand(map, f_hash, f_equals, key_size, val_size));
    }
    // Retrieve bucket
    size_t idx = INDEX_WRAP(f_hash(key), map->_capacity);
    _z_hashmap_jr_entry_t *curr_bucket =
        (void *)_z_ptr_u8_offset((uint8_t *)map->_vals, (ptrdiff_t)(idx * (key_size + val_size)));

    // Handle collision (linear probing)
    while (!_z_hashmap_jr_entry_is_empty(curr_bucket, key_size)) {
        // Check if same key
        if (f_equals(_z_hashmap_jr_entry_key(curr_bucket), key)) {
            // Replace value
            memcpy(_z_hashmap_jr_entry_value(curr_bucket, key_size), val, val_size);
            return _Z_RES_OK;
        }
        // Move to next bucket
        idx = INDEX_WRAP((idx + 1), map->_capacity);
        curr_bucket = (void *)_z_ptr_u8_offset((uint8_t *)map->_vals, (ptrdiff_t)(idx * (key_size + val_size)));
    }
    // Insert the new element
    memcpy(_z_hashmap_jr_entry_key(curr_bucket), key, key_size);
    memcpy(_z_hashmap_jr_entry_value(curr_bucket, key_size), val, val_size);
    map->_len++;
    return _Z_RES_OK;
}

void *_z_hashmap_jr_get(const _z_hashmap_jr_t *map, const void *key, z_element_hash_f f_hash, z_element_eq_f f_equals,
                        size_t key_size, size_t val_size) {
    if (map->_vals == NULL) {
        return NULL;
    }
    // Retrieve bucket
    size_t idx = INDEX_WRAP(f_hash(key), map->_capacity);
    _z_hashmap_jr_entry_t *curr_bucket =
        (void *)_z_ptr_u8_offset((uint8_t *)map->_vals, (ptrdiff_t)(idx * (key_size + val_size)));

    while (!_z_hashmap_jr_entry_is_empty(curr_bucket, key_size)) {
        // Check if same key
        if (f_equals(_z_hashmap_jr_entry_key(curr_bucket), key)) {
            return _z_hashmap_jr_entry_value(curr_bucket, key_size);
        }
        // Move to next bucket
        idx = INDEX_WRAP((idx + 1), map->_capacity);
        curr_bucket = (void *)_z_ptr_u8_offset((uint8_t *)map->_vals, (ptrdiff_t)(idx * (key_size + val_size)));
    }
    return NULL;
}

void _z_hashmap_jr_remove(_z_hashmap_jr_t *map, const void *k, z_element_hash_f f_hash, z_element_eq_f f_equals,
                          size_t key_size, size_t val_size) {
    if (map->_vals == NULL) {
        return;
    }
    // Retrieve bucket
    size_t idx = INDEX_WRAP(f_hash(k), map->_capacity);
    _z_hashmap_jr_entry_t *curr_bucket =
        (void *)_z_ptr_u8_offset((uint8_t *)map->_vals, (ptrdiff_t)(idx * (key_size + val_size)));

    while (!_z_hashmap_jr_entry_is_empty(curr_bucket, key_size)) {
        // Check if same key
        if (f_equals(_z_hashmap_jr_entry_key(curr_bucket), k)) {
            // Clear entry
            memset(curr_bucket, 0xFF, key_size + val_size);
            map->_len--;
            // Reinsert following entries to avoid search breakage
            size_t del_idx = idx;
            while (true) {
                // Move to next bucket
                idx = INDEX_WRAP((idx + 1), map->_capacity);
                curr_bucket = (void *)_z_ptr_u8_offset((uint8_t *)map->_vals, (ptrdiff_t)(idx * (key_size + val_size)));
                if (_z_hashmap_jr_entry_is_empty(curr_bucket, key_size)) {
                    break;  // Reached an empty slot
                }
                // Reinsert entry
                size_t re_idx = INDEX_WRAP(f_hash(_z_hashmap_jr_entry_key(curr_bucket)), map->_capacity);
                // Find new location
                bool should_move = false;
                if (idx > del_idx) {
                    should_move = (re_idx <= del_idx) || (re_idx > idx);
                } else {
                    should_move = (re_idx <= del_idx) && (re_idx > idx);
                }
                if (should_move) {
                    // Move entry
                    _z_hashmap_jr_entry_t *new_bucket =
                        (void *)_z_ptr_u8_offset((uint8_t *)map->_vals, (ptrdiff_t)(del_idx * (key_size + val_size)));
                    memcpy(_z_hashmap_jr_entry_key(new_bucket), _z_hashmap_jr_entry_key(curr_bucket),
                           key_size + val_size);
                    // Clear old entry
                    memset(curr_bucket, 0xFF, key_size + val_size);
                    del_idx = idx;  // Update the deleted slot index
                }
            }
            return;
        }
        // Move to next bucket
        idx = INDEX_WRAP((idx + 1), map->_capacity);
        curr_bucket = (void *)_z_ptr_u8_offset((uint8_t *)map->_vals, (ptrdiff_t)(idx * (key_size + val_size)));
    }
}

void _z_hashmap_jr_clear(_z_hashmap_jr_t *map, size_t key_size, size_t val_size) {
    if (map->_vals == NULL) {
        return;
    }
    memset(map->_vals, 0xFF, map->_capacity * (key_size + val_size));
    map->_len = 0;
}

void _z_hashmap_jr_delete(_z_hashmap_jr_t *map) {
    if (map->_vals == NULL) {
        return;
    }
    map->_len = 0;
    z_free(map->_vals);
    map->_vals = NULL;
}