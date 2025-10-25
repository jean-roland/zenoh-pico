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

#ifndef ZENOH_PICO_COLLECTIONS_HASHMAP_JR_H
#define ZENOH_PICO_COLLECTIONS_HASHMAP_JR_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "zenoh-pico/collections/element.h"
#include "zenoh-pico/collections/list.h"
#include "zenoh-pico/utils/result.h"

#ifdef __cplusplus
extern "C" {
#endif

#define _Z_DEFAULT_HASHMAP_JR_CAPACITY 16  // Prime number to reduce collisions

/**
 * A hashmap entry with generic keys.
 *
 * Members:
 *   void *_key: the key of the entry
 *   void *_val: the value of the entry
 */
typedef struct {
    void *_key;
    void *_val;
} _z_hashmap_entry_jr_t;

/**
 * A hashmap with generic keys.
 *
 * Members:
 *    size_t _capacity: the number of buckets available in the hashmap
 *   _z_list_t **_vals: the linked list containing the values
 *   z_element_hash_f _f_hash: the hash function used to hash keys
 *   z_element_eq_f _f_equals: the function used to compare keys for equality
 */
typedef struct {
    size_t _capacity;
    size_t _len;
    _z_list_t **_vals;
    // _z_hashmap_entry_t *_vals;
} _z_hashmap_jr_t;

_z_hashmap_jr_t _z_hashmap_jr_init(size_t capacity);
z_result_t _z_hashmap_jr_insert(_z_hashmap_jr_t *map, void *k, void *v, z_element_hash_f f_hash,
                                z_element_eq_f f_equals);
void *_z_hashmap_jr_get(const _z_hashmap_jr_t *map, const void *k, z_element_hash_f f_hash, z_element_eq_f f_equals);
void _z_hashmap_jr_remove(_z_hashmap_jr_t *map, const void *k, z_element_hash_f f_hash, z_element_eq_f f_equals);
void _z_hashmap_jr_clear(_z_hashmap_jr_t *map);
void _z_hashmap_jr_delete(_z_hashmap_jr_t *map);

#define _Z_HASHMAP_JR_DEFINE(map_name, key_name, val_name, key_type, val_type)                                 \
    typedef _z_hashmap_entry_jr_t map_name##_hashmap_entry_t;                                                  \
    static inline bool map_name##_hashmap_jr_entry_key_eq(const void *left, const void *right) {               \
        const map_name##_hashmap_entry_t *l = (const map_name##_hashmap_entry_t *)left;                        \
        const map_name##_hashmap_entry_t *r = (const map_name##_hashmap_entry_t *)right;                       \
        return key_name##_elem_eq(l->_key, r->_key);                                                           \
    }                                                                                                          \
    typedef _z_hashmap_jr_t map_name##_hashmap_t;                                                              \
    static inline _z_hashmap_jr_t map_name##_hashmap_jr_init(void) {                                           \
        return _z_hashmap_jr_init(_Z_DEFAULT_HASHMAP_JR_CAPACITY);                                             \
    }                                                                                                          \
    static inline z_result_t map_name##_hashmap_jr_insert(map_name##_hashmap_t *m, key_type *k, val_type *v) { \
        return _z_hashmap_jr_insert(m, k, v, key_name##_elem_hash, map_name##_hashmap_jr_entry_key_eq);        \
    }                                                                                                          \
    static inline val_type *map_name##_hashmap_jr_get(const map_name##_hashmap_t *m, const key_type *k) {      \
        return (val_type *)_z_hashmap_jr_get(m, k, key_name##_elem_hash, map_name##_hashmap_jr_entry_key_eq);  \
    }                                                                                                          \
    static inline void map_name##_hashmap_jr_remove(map_name##_hashmap_t *m, const key_type *k) {              \
        _z_hashmap_jr_remove(m, k, key_name##_elem_hash, map_name##_hashmap_jr_entry_key_eq);                  \
    }                                                                                                          \
    static inline void map_name##_hashmap_jr_clear(map_name##_hashmap_t *m) { _z_hashmap_jr_clear(m); }        \
    static inline void map_name##_hashmap_jr_delete(map_name##_hashmap_t *m) { _z_hashmap_jr_delete(m); }

#ifdef __cplusplus
}
#endif

#endif /* ZENOH_PICO_COLLECTIONS_HASHMAP_H */
