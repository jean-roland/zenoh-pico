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

#include "zenoh-pico/collections/lru_cache.h"

#include <assert.h>
#include <stddef.h>
#include <string.h>

#include "zenoh-pico/system/common/platform.h"
#include "zenoh-pico/utils/logging.h"
#include "zenoh-pico/utils/pointers.h"
#include "zenoh-pico/utils/result.h"

// To avoid high load factor in hash table, we oversize it by 20%
#define OVERSIZE_FACTOR 12 / 10

// Nodes are chained as double linked list for lru insertion/deletion.
typedef struct _z_lru_cache_node_data_t {
    _z_lru_cache_node_t *prev;  // List previous node
    _z_lru_cache_node_t *next;  // List next node
} _z_lru_cache_node_data_t;

#define NODE_DATA_SIZE sizeof(_z_lru_cache_node_data_t)

// Generic static functions
static inline _z_lru_cache_t _z_lru_cache_null(void) { return (_z_lru_cache_t){0}; }

static inline _z_lru_cache_node_data_t *_z_lru_cache_node_data(_z_lru_cache_node_t *node) {
    return (_z_lru_cache_node_data_t *)node;
}

static inline void *_z_lru_cache_node_value(_z_lru_cache_node_t *node) {
    return (void *)_z_ptr_u8_offset((uint8_t *)node, (ptrdiff_t)NODE_DATA_SIZE);
}

// List functions
static void _z_lru_cache_insert_list_node(_z_lru_cache_t *cache, _z_lru_cache_node_t *node) {
    _z_lru_cache_node_data_t *node_data = _z_lru_cache_node_data(node);
    node_data->prev = NULL;
    node_data->next = cache->head;

    if (cache->head != NULL) {
        _z_lru_cache_node_data_t *head_data = _z_lru_cache_node_data(cache->head);
        head_data->prev = node;
    }
    cache->head = node;
    if (cache->tail == NULL) {
        cache->tail = node;
    }
}

static void _z_lru_cache_remove_list_node(_z_lru_cache_t *cache, _z_lru_cache_node_t *node) {
    _z_lru_cache_node_data_t *node_data = _z_lru_cache_node_data(node);

    // Nominal case
    if ((node_data->prev != NULL) && (node_data->next != NULL)) {
        _z_lru_cache_node_data_t *prev_data = _z_lru_cache_node_data(node_data->prev);
        _z_lru_cache_node_data_t *next_data = _z_lru_cache_node_data(node_data->next);
        prev_data->next = node_data->next;
        next_data->prev = node_data->prev;
    }
    if (node_data->prev == NULL) {
        assert(cache->head == node);
        cache->head = node_data->next;
        if (node_data->next != NULL) {
            _z_lru_cache_node_data_t *next_data = _z_lru_cache_node_data(node_data->next);
            next_data->prev = NULL;
        }
    }
    if (node_data->next == NULL) {
        assert(cache->tail == node);
        cache->tail = node_data->prev;
        if (node_data->prev != NULL) {
            _z_lru_cache_node_data_t *prev_data = _z_lru_cache_node_data(node_data->prev);
            prev_data->next = NULL;
        }
    }
}

static void _z_lru_cache_move_list_node(_z_lru_cache_t *cache, _z_lru_cache_node_t *node) {
    _z_lru_cache_node_data_t *moved_node_data = _z_lru_cache_node_data(node);
    if (moved_node_data->prev != NULL) {
        _z_lru_cache_node_data_t *prev_data = _z_lru_cache_node_data(moved_node_data->prev);
        prev_data->next = node;
    } else {
        cache->head = node;
    }
    if (moved_node_data->next != NULL) {
        _z_lru_cache_node_data_t *next_data = _z_lru_cache_node_data(moved_node_data->next);
        next_data->prev = node;
    } else {
        cache->tail = node;
    }
}

static void _z_lru_cache_update_list(_z_lru_cache_t *cache, _z_lru_cache_node_t *node) {
    _z_lru_cache_remove_list_node(cache, node);
    _z_lru_cache_insert_list_node(cache, node);
}

static void _z_lru_cache_clear_list(_z_lru_cache_t *cache, z_element_clear_f clear) {
    _z_lru_cache_node_data_t *node = cache->head;
    while (node != NULL) {
        _z_lru_cache_node_data_t *node_data = _z_lru_cache_node_data(node);
        void *node_value = _z_lru_cache_node_value(node);
        node = node_data->next;
        clear(node_value);
    }
}

// Hash table functions
static inline size_t incr_wrap_idx(size_t idx, size_t max) {
    if (++idx >= max) {
        idx -= max;
    }
    return idx;
}

static inline bool _z_lru_cache_value_is_empty(const void *value, size_t val_size) {
    const uint8_t *bytes = (const uint8_t *)value;
    for (size_t i = 0; i < val_size; i++) {
        if (bytes[i] != 0xFF) {
            return false;
        }
    }
    return true;
}

static _z_lru_cache_node_t *_z_lru_cache_search_hlist(_z_lru_cache_t *cache, void *value, _z_lru_val_cmp_f compare,
                                                      size_t *idx, z_element_hash_f elem_hash, size_t val_size) {
    if (cache->slist == NULL) {
        return NULL;
    }
    size_t curr_idx = elem_hash(value) % cache->slist_len;
    _z_lru_cache_node_t *curr_node =
        (void *)_z_ptr_u8_offset((uint8_t *)cache->slist, (ptrdiff_t)(curr_idx * (val_size + NODE_DATA_SIZE)));

    while (!_z_lru_cache_value_is_empty(_z_lru_cache_node_value(curr_node), val_size)) {
        int res = compare(_z_lru_cache_node_value(curr_node), value);
        if (res == 0) {
            *idx = curr_idx;
            return curr_node;
        } else {
            // Linear probing
            curr_idx = incr_wrap_idx(curr_idx, cache->slist_len);
            curr_node =
                (void *)_z_ptr_u8_offset((uint8_t *)cache->slist, (ptrdiff_t)(curr_idx * (val_size + NODE_DATA_SIZE)));
        }
    }
    return NULL;  // Not found
}

// Warning: not protected against value duplicate
static void _z_lru_cache_insert_hlist(_z_lru_cache_t *cache, _z_lru_cache_node_t **node, z_element_hash_f elem_hash,
                                      void *value, size_t val_size) {
    // Find insert position
    size_t curr_idx = elem_hash(value) % cache->slist_len;
    _z_lru_cache_node_t *curr_node =
        (void *)_z_ptr_u8_offset((uint8_t *)cache->slist, (ptrdiff_t)(curr_idx * (val_size + NODE_DATA_SIZE)));
    while (!_z_lru_cache_value_is_empty(_z_lru_cache_node_value(curr_node), val_size)) {
        // Linear probing
        curr_idx = incr_wrap_idx(curr_idx, cache->slist_len);
        curr_node =
            (void *)_z_ptr_u8_offset((uint8_t *)cache->slist, (ptrdiff_t)(curr_idx * (val_size + NODE_DATA_SIZE)));
    }
    // Store element
    _z_lru_cache_node_data(curr_node)->prev = NULL;
    _z_lru_cache_node_data(curr_node)->next = NULL;
    memcpy(_z_lru_cache_node_value(curr_node), value, val_size);
    *node = curr_node;
}

static void _z_lru_cache_delete_hlist(_z_lru_cache_t *cache, _z_lru_cache_node_t *node, _z_lru_val_cmp_f compare,
                                      z_element_hash_f elem_hash, size_t val_size) {
    // Find location
    size_t del_idx = 0;
    _z_lru_cache_node_t *del_node =
        _z_lru_cache_search_hlist(cache, _z_lru_cache_node_value(node), compare, &del_idx, elem_hash, val_size);
    if (del_node == NULL) {
        _Z_ERROR("Failed to delete node");
        assert(false);
    }
    // Clear location
    memset(del_node, 0xFF, val_size + NODE_DATA_SIZE);

    // Backward shift: reinsert all nodes that were probing after del_idx
    size_t idx = del_idx;
    while (true) {
        idx = incr_wrap_idx(idx, cache->slist_len);
        // Get the node at the current index
        _z_lru_cache_node_t *current_node =
            (void *)_z_ptr_u8_offset((uint8_t *)cache->slist, (ptrdiff_t)(idx * (val_size + NODE_DATA_SIZE)));
        if (_z_lru_cache_value_is_empty(_z_lru_cache_node_value(current_node), val_size)) {
            break;  // Reached an empty slot
        }
        size_t ideal_idx = elem_hash(_z_lru_cache_node_value(current_node)) % cache->slist_len;
        // Check if the node's ideal position is <= del_idx or if it's in a "displaced" position
        bool should_move = false;
        if (idx > del_idx) {
            should_move = (ideal_idx <= del_idx) || (ideal_idx > idx);
        } else {
            should_move = (ideal_idx <= del_idx) && (ideal_idx > idx);
        }
        if (should_move) {
            // Move the node to the deleted slot
            del_node =
                (void *)_z_ptr_u8_offset((uint8_t *)cache->slist, (ptrdiff_t)(del_idx * (val_size + NODE_DATA_SIZE)));
            memcpy(del_node, current_node, val_size + NODE_DATA_SIZE);
            _z_lru_cache_move_list_node(cache, del_node);
            memset(current_node, 0xFF, val_size + NODE_DATA_SIZE);
            // Update the deleted slot index
            del_idx = idx;
        }
    }
}

// Main static functions
static void _z_lru_cache_delete_last(_z_lru_cache_t *cache, _z_lru_val_cmp_f compare, z_element_hash_f elem_hash,
                                     z_element_clear_f clear, size_t val_size) {
    _z_lru_cache_node_t *last = cache->tail;
    assert(last != NULL);
    _z_lru_cache_remove_list_node(cache, last);
    void *last_value = _z_lru_cache_node_value(last);
    clear(last_value);
    _z_lru_cache_delete_hlist(cache, last, compare, elem_hash, val_size);
    cache->len--;
}

static void _z_lru_cache_insert_node(_z_lru_cache_t *cache, z_element_hash_f elem_hash, void *value, size_t val_size) {
    _z_lru_cache_node_t *node = NULL;
    _z_lru_cache_insert_hlist(cache, &node, elem_hash, value, val_size);
    _z_lru_cache_insert_list_node(cache, node);
    cache->len++;
}

static _z_lru_cache_node_t *_z_lru_cache_search_node(_z_lru_cache_t *cache, void *value, _z_lru_val_cmp_f compare,
                                                     z_element_hash_f elem_hash, size_t val_size) {
    size_t idx = 0;
    return _z_lru_cache_search_hlist(cache, value, compare, &idx, elem_hash, val_size);
}

// Public functions
_z_lru_cache_t _z_lru_cache_init(size_t capacity) {
    _z_lru_cache_t cache = _z_lru_cache_null();
    cache.capacity = capacity;
    cache.slist_len = capacity * OVERSIZE_FACTOR;
    return cache;
}

void *_z_lru_cache_get(_z_lru_cache_t *cache, void *value, _z_lru_val_cmp_f compare, z_element_hash_f elem_hash,
                       size_t value_size) {
    // Lookup if node exists.
    _z_lru_cache_node_t *node = _z_lru_cache_search_node(cache, value, compare, elem_hash, value_size);
    if (node == NULL) {
        return NULL;
    }
    // Update list with node as most recent
    _z_lru_cache_update_list(cache, node);
    return _z_lru_cache_node_value(node);
}

z_result_t _z_lru_cache_insert(_z_lru_cache_t *cache, void *value, size_t value_size, _z_lru_val_cmp_f compare,
                               z_element_hash_f elem_hash, z_element_clear_f elem_clear) {
    assert(cache->capacity > 0);
    assert(!_z_lru_cache_value_is_empty(value, value_size));

    // Init slist
    if (cache->slist == NULL) {
        cache->slist = (_z_lru_cache_node_t *)z_malloc(cache->slist_len * (value_size + NODE_DATA_SIZE));
        if (cache->slist == NULL) {
            _Z_ERROR_RETURN(_Z_ERR_SYSTEM_OUT_OF_MEMORY);
        }
        memset(cache->slist, 0xFF, cache->slist_len * (value_size + NODE_DATA_SIZE));
    }
    // Check capacity
    if (cache->len == cache->capacity) {
        // Delete lru entry
        _z_lru_cache_delete_last(cache, compare, elem_hash, elem_clear, value_size);
    }
    // Update the cache
    _z_lru_cache_insert_node(cache, elem_hash, value, value_size);
    return _Z_RES_OK;
}

void _z_lru_cache_clear(_z_lru_cache_t *cache, z_element_clear_f clear, size_t value_size) {
    // Clear list
    _z_lru_cache_clear_list(cache, clear);
    // Reset slist
    if (cache->slist != NULL) {
        memset(cache->slist, 0xFF, cache->slist_len * (value_size + NODE_DATA_SIZE));
    }
    // Reset cache
    cache->len = 0;
    cache->head = NULL;
    cache->tail = NULL;
}

void _z_lru_cache_delete(_z_lru_cache_t *cache, z_element_clear_f clear, size_t value_size) {
    _z_lru_cache_clear(cache, clear, value_size);
    z_free(cache->slist);
    cache->slist = NULL;
}
