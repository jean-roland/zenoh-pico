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
#include "zenoh-pico/utils/result.h"

#ifdef __cplusplus
extern "C" {
#endif

#define _Z_DEFAULT_HASHMAP_JR_CAPACITY 16  // Must be power of 2

// Hashmap entry struct: {key; value}
typedef void _z_hashmap_jr_entry_t;

/**
 * A hashmap with generic keys. WARNING key ~0 is considered empty and will not be accepted.
 *
 * Members:
 *   bool resizable: indicates if the hashmap can resize when load factor is exceeded
 *   size_t _capacity: the number of buckets available in the hashmap
 *   size_t _len: the number of entries in the hashmap
 *   _z_hashmap_jr_entry_t *_vals: the array containing the values
 */
typedef struct {
    bool resizable;
    size_t _capacity;
    size_t _len;
    _z_hashmap_jr_entry_t *_vals;
} _z_hashmap_jr_t;

_z_hashmap_jr_t _z_hashmap_jr_init(size_t capacity, bool resizable);
z_result_t _z_hashmap_jr_insert(_z_hashmap_jr_t *map, void *key, void *val, z_element_hash_f f_hash,
                                z_element_eq_f f_equals, size_t key_size, size_t val_size);
void *_z_hashmap_jr_get(const _z_hashmap_jr_t *map, const void *key, z_element_hash_f f_hash, z_element_eq_f f_equals,
                        size_t key_size, size_t val_size);
void _z_hashmap_jr_remove(_z_hashmap_jr_t *map, const void *k, z_element_hash_f f_hash, z_element_eq_f f_equals,
                          z_element_clear_f key_f_clear, z_element_clear_f val_f_clear, size_t key_size,
                          size_t val_size);
void _z_hashmap_jr_clear(_z_hashmap_jr_t *map, z_element_clear_f key_f_clear, z_element_clear_f val_f_clear,
                         size_t key_size, size_t val_size);
void _z_hashmap_jr_delete(_z_hashmap_jr_t *map);

#define _Z_HASHMAP_JR_DEFINE(map_name, key_name, val_name, key_type, val_type)                                    \
    typedef _z_hashmap_jr_t map_name##_hashmap_t;                                                                 \
    static inline _z_hashmap_jr_t map_name##_hashmap_jr_init(size_t capacity, bool resizable) {                   \
        return _z_hashmap_jr_init(capacity, resizable);                                                           \
    }                                                                                                             \
    static inline z_result_t map_name##_hashmap_jr_insert(map_name##_hashmap_t *m, key_type *k, val_type *v) {    \
        return _z_hashmap_jr_insert(m, k, v, key_name##_elem_hash, key_name##_elem_eq, sizeof(key_type),          \
                                    sizeof(val_type));                                                            \
    }                                                                                                             \
    static inline val_type *map_name##_hashmap_jr_get(const map_name##_hashmap_t *m, const key_type *k) {         \
        return (val_type *)_z_hashmap_jr_get(m, k, key_name##_elem_hash, key_name##_elem_eq, sizeof(key_type),    \
                                             sizeof(val_type));                                                   \
    }                                                                                                             \
    static inline void map_name##_hashmap_jr_remove(map_name##_hashmap_t *m, const key_type *k) {                 \
        _z_hashmap_jr_remove(m, k, key_name##_elem_hash, key_name##_elem_eq, key_name##_elem_clear,               \
                             val_name##_elem_clear, sizeof(key_type), sizeof(val_type));                          \
    }                                                                                                             \
    static inline void map_name##_hashmap_jr_clear(map_name##_hashmap_t *m) {                                     \
        _z_hashmap_jr_clear(m, key_name##_elem_clear, val_name##_elem_clear, sizeof(key_type), sizeof(val_type)); \
    }                                                                                                             \
    static inline void map_name##_hashmap_jr_delete(map_name##_hashmap_t *m) { _z_hashmap_jr_delete(m); }

#ifdef __cplusplus
}
#endif

#endif /* ZENOH_PICO_COLLECTIONS_HASHMAP_H */
