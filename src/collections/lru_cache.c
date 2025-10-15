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

// #define USE_HASH

#if defined USE_HASH
#define OVERSIZE_FACTOR 12 / 10  // 1.2 but in integer
#else
#define OVERSIZE_FACTOR 1
#endif

// Nodes are chained as double linked list for lru insertion/deletion.
typedef struct _z_lru_cache_node_data_t {
    _z_lru_cache_node_t *prev;  // List previous node
    _z_lru_cache_node_t *next;  // List next node
#if defined USE_HASH
    size_t probe_length;  // Robin hood probing
#endif
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

static _z_lru_cache_node_t *_z_lru_cache_node_create(void *value, size_t value_size) {
    size_t node_size = NODE_DATA_SIZE + value_size;
    _z_lru_cache_node_t *node = (_z_lru_cache_node_t *)z_malloc(node_size);
    if (node == NULL) {
        return node;
    }
    memset(node, 0, NODE_DATA_SIZE);
    memcpy(_z_lru_cache_node_value(node), value, value_size);
    return node;
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

static void _z_lru_cache_update_list(_z_lru_cache_t *cache, _z_lru_cache_node_t *node) {
    _z_lru_cache_remove_list_node(cache, node);
    _z_lru_cache_insert_list_node(cache, node);
}

static void _z_lru_cache_clear_list(_z_lru_cache_t *cache, z_element_clear_f clear) {
    _z_lru_cache_node_data_t *node = cache->head;
    while (node != NULL) {
        _z_lru_cache_node_t *tmp = node;
        _z_lru_cache_node_data_t *node_data = _z_lru_cache_node_data(node);
        void *node_value = _z_lru_cache_node_value(node);
        node = node_data->next;
        clear(node_value);
        z_free(tmp);
    }
}

#if defined USE_HASH
static inline size_t incr_wrap_idx(size_t idx, size_t max) {
    if (++idx >= max) {
        idx -= max;
    }
    return idx;
}

static inline size_t wrap_idx(size_t idx, size_t max) {
    if (idx >= max) {
        idx -= max;
    }
    return idx;
}

static _z_lru_cache_node_t *_z_lru_cache_search_hlist(_z_lru_cache_t *cache, void *value, _z_lru_val_cmp_f compare,
                                                      size_t *idx, z_element_hash_f elem_hash) {
    if (cache->slist == NULL) {
        return NULL;
    }
    size_t curr_idx = elem_hash(value) % cache->slist_len;
    size_t try_count = 0;

    while ((cache->slist[curr_idx] != NULL) && (try_count < cache->slist_len)) {
        int res = compare(_z_lru_cache_node_value(cache->slist[curr_idx]), value);
        if (res == 0) {
            *idx = curr_idx;
            return cache->slist[curr_idx];
        } else {
            // Linear probing
            curr_idx = incr_wrap_idx(curr_idx, cache->slist_len);
        }
        try_count++;
    }
    return NULL;  // Not found
}

// // Warning: not protected against value duplicate
// static void _z_lru_cache_insert_hlist(_z_lru_cache_t *cache, _z_lru_cache_node_t *new_node, z_element_hash_f elem_hash) {
//     size_t curr_idx = elem_hash(_z_lru_cache_node_value(new_node)) % cache->slist_len;
//     size_t curr_probe_length = 0;
//     _z_lru_cache_node_t *curr_node = cache->slist[curr_idx];

//     // Find free slot with Robin Hood adressing
//     while (curr_node != NULL) {
//         // If existing node is "poorer", swap nodes
//         if (_z_lru_cache_node_data(curr_node)->probe_length < curr_probe_length) {
//             _z_lru_cache_node_data(new_node)->probe_length = curr_probe_length;
//             cache->slist[curr_idx] = new_node;
//             new_node = curr_node;
//             curr_probe_length = _z_lru_cache_node_data(new_node)->probe_length;
//         }
//         // Move to next slot
//         curr_probe_length++;
//         curr_idx = incr_wrap_idx(curr_idx, cache->slist_len);
//         curr_node = cache->slist[curr_idx];
//     }
//     // Insert node
//     _z_lru_cache_node_data(new_node)->probe_length = curr_probe_length;
//     cache->slist[curr_idx] = new_node;
// }

// static void _z_lru_cache_delete_hlist(_z_lru_cache_t *cache, _z_lru_cache_node_t *node, _z_lru_val_cmp_f compare,
//                                       z_element_hash_f elem_hash) {
//     size_t del_idx = 0;
//     _z_lru_cache_node_t *del_node =
//         _z_lru_cache_search_hlist(cache, _z_lru_cache_node_value(node), compare, &del_idx, elem_hash);
//     if (del_node == NULL) {
//         _Z_ERROR("Failed to delete node");
//         assert(false);
//     }
//     // Clear location
//     cache->slist[del_idx] = NULL;
//     // Robin Hood backward shift 
//     size_t idx = del_idx;
//     while (true) {
//         idx = incr_wrap_idx(idx, cache->slist_len);
//         if (cache->slist[idx] == NULL) {
//             break;  // Reached an empty slot
//         }
//         _z_lru_cache_node_t *current_node = cache->slist[idx];
//         size_t ideal_idx = elem_hash(_z_lru_cache_node_value(current_node)) % cache->slist_len;
//         // Node is in its ideal position
//         if (idx == ideal_idx) {
//             break;  
//         }
//         size_t new_probe_length = wrap_idx(cache->slist_len + del_idx - ideal_idx, cache->slist_len);
//         // Move only if probe length decreases
//         if (new_probe_length < _z_lru_cache_node_data(current_node)->probe_length) {
//             cache->slist[del_idx] = current_node;
//             cache->slist[idx] = NULL;
//             _z_lru_cache_node_data(current_node)->probe_length = new_probe_length;
//             del_idx = idx;
//         }
//     }
// }

// Warning: not protected against value duplicate
static void _z_lru_cache_insert_hlist(_z_lru_cache_t *cache, _z_lru_cache_node_t *node, z_element_hash_f elem_hash) {
    // Find insert position
    size_t curr_idx = elem_hash(_z_lru_cache_node_value(node)) % cache->slist_len;
    while (cache->slist[curr_idx] != NULL) {
        // Linear probing
        curr_idx = incr_wrap_idx(curr_idx, cache->slist_len);
    }
    // Store element
    cache->slist[curr_idx] = node;
}

static void _z_lru_cache_delete_hlist(_z_lru_cache_t *cache, _z_lru_cache_node_t *node, _z_lru_val_cmp_f compare,
                                      z_element_hash_f elem_hash) {
    // Find location
    size_t del_idx = 0;
    _z_lru_cache_node_t *del_node =
        _z_lru_cache_search_hlist(cache, _z_lru_cache_node_value(node), compare, &del_idx, elem_hash);
    if (del_node == NULL) {
        _Z_ERROR("Failed to delete node");
        assert(false);
    }
    // Clear location
    cache->slist[del_idx] = NULL;

    // Backward shift: reinsert all nodes that were probing after del_idx
    size_t idx = del_idx;
    while (true) {
        idx = incr_wrap_idx(idx, cache->slist_len);
        if (cache->slist[idx] == NULL) {
            break;  // Reached an empty slot
        }
        // Get the node at the current index
        _z_lru_cache_node_t *current_node = cache->slist[idx];
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
            cache->slist[del_idx] = current_node;
            cache->slist[idx] = NULL;
            del_idx = idx;  // Update the deleted slot index
        }
    }
}

#else
// Sorted list function
static _z_lru_cache_node_t *_z_lru_cache_search_slist(_z_lru_cache_t *cache, void *value, _z_lru_val_cmp_f compare,
                                                      size_t *idx) {
    int l_idx = 0;
    int h_idx = (int)cache->len - 1;
    while (l_idx <= h_idx) {
        int curr_idx = (l_idx + h_idx) / 2;
        int res = compare(_z_lru_cache_node_value(cache->slist[curr_idx]), value);
        if (res == 0) {
            *idx = (size_t)curr_idx;
            return cache->slist[curr_idx];
        } else if (res < 0) {
            l_idx = curr_idx + 1;
        } else {
            h_idx = curr_idx - 1;
        }
    }
    return NULL;
}

static int _z_lru_cache_find_position(_z_lru_cache_node_t **slist, _z_lru_val_cmp_f compare, void *node_val,
                                      size_t slist_size) {
    int start = 0;
    int end = (int)slist_size - 1;
    while (start <= end) {
        int mid = start + (end - start) / 2;
        if (compare(_z_lru_cache_node_value(slist[mid]), node_val) < 0) {
            start = mid + 1;
        } else {
            end = mid - 1;
        }
    }
    return start;
}

static void _z_lru_cache_move_elem_slist(_z_lru_cache_t *cache, size_t *add_idx_addr, size_t *del_idx_addr) {
    size_t del_idx = (del_idx_addr == NULL) ? cache->len : *del_idx_addr;
    size_t add_idx = *add_idx_addr;
    if (add_idx == del_idx) {
        return;
    }
    // Move elements between the indices on the right
    if (del_idx >= add_idx) {
        memmove(&cache->slist[add_idx + 1], &cache->slist[add_idx],
                (del_idx - add_idx) * sizeof(_z_lru_cache_node_t *));
    } else {  // Move them on the left
        // Rightmost value doesn't move unless we have a new maximum
        if (add_idx != cache->capacity - 1) {
            *add_idx_addr -= 1;
            add_idx -= 1;
        }
        memmove(&cache->slist[del_idx], &cache->slist[del_idx + 1],
                (add_idx - del_idx) * sizeof(_z_lru_cache_node_t *));
    }
}

static void _z_lru_cache_insert_slist(_z_lru_cache_t *cache, _z_lru_cache_node_t *node, _z_lru_val_cmp_f compare,
                                      size_t *del_idx) {
    // Find insert position:
    if (cache->len == 0) {
        cache->slist[0] = node;
        return;
    }
    void *node_val = _z_lru_cache_node_value(node);
    size_t pos = (size_t)_z_lru_cache_find_position(cache->slist, compare, node_val, cache->len);
    // Move elements
    _z_lru_cache_move_elem_slist(cache, &pos, del_idx);
    // Store element
    cache->slist[pos] = node;
}

static size_t _z_lru_cache_delete_slist(_z_lru_cache_t *cache, _z_lru_cache_node_t *node, _z_lru_val_cmp_f compare) {
    size_t del_idx = 0;
    // Don't delete, return the index
    (void)_z_lru_cache_search_slist(cache, _z_lru_cache_node_value(node), compare, &del_idx);
    return del_idx;
}
#endif

// Main static functions
static size_t _z_lru_cache_delete_last(_z_lru_cache_t *cache, _z_lru_val_cmp_f compare, z_element_hash_f elem_hash) {
    _z_lru_cache_node_t *last = cache->tail;
    assert(last != NULL);
    _z_lru_cache_remove_list_node(cache, last);
#if defined USE_HASH
    _z_lru_cache_delete_hlist(cache, last, compare, elem_hash);
    size_t del_idx = 0;  // Not used
#else
    size_t del_idx = _z_lru_cache_delete_slist(cache, last, compare);
#endif
    z_free(last);
    cache->len--;
    return del_idx;
}

static void _z_lru_cache_insert_node(_z_lru_cache_t *cache, _z_lru_cache_node_t *node, _z_lru_val_cmp_f compare,
                                     size_t *del_idx, z_element_hash_f elem_hash) {
    _z_lru_cache_insert_list_node(cache, node);
#if defined USE_HASH
    _ZP_UNUSED(del_idx);
    _ZP_UNUSED(compare);
    _z_lru_cache_insert_hlist(cache, node, elem_hash);
#else
    _z_lru_cache_insert_slist(cache, node, compare, del_idx);
#endif
    cache->len++;
}

static _z_lru_cache_node_t *_z_lru_cache_search_node(_z_lru_cache_t *cache, void *value, _z_lru_val_cmp_f compare,
                                                     z_element_hash_f elem_hash) {
    size_t idx = 0;
#if defined USE_HASH
    return _z_lru_cache_search_hlist(cache, value, compare, &idx, elem_hash);
#else
    return _z_lru_cache_search_slist(cache, value, compare, &idx);
#endif
}

// Public functions
_z_lru_cache_t _z_lru_cache_init(size_t capacity) {
    _z_lru_cache_t cache = _z_lru_cache_null();
    cache.capacity = capacity;
    cache.slist_len = capacity * OVERSIZE_FACTOR;
    return cache;
}

void *_z_lru_cache_get(_z_lru_cache_t *cache, void *value, _z_lru_val_cmp_f compare, z_element_hash_f elem_hash) {
    // Lookup if node exists.
    _z_lru_cache_node_t *node = _z_lru_cache_search_node(cache, value, compare, elem_hash);
    if (node == NULL) {
        return NULL;
    }
    // Update list with node as most recent
    _z_lru_cache_update_list(cache, node);
    return _z_lru_cache_node_value(node);
}

z_result_t _z_lru_cache_insert(_z_lru_cache_t *cache, void *value, size_t value_size, _z_lru_val_cmp_f compare,
                               z_element_hash_f elem_hash) {
    _ZP_UNUSED(elem_hash);
    assert(cache->capacity > 0);
    // Init slist
    if (cache->slist == NULL) {
        cache->slist = (_z_lru_cache_node_t **)z_malloc(cache->slist_len * sizeof(void *));
        if (cache->slist == NULL) {
            _Z_ERROR_RETURN(_Z_ERR_SYSTEM_OUT_OF_MEMORY);
        }
        memset(cache->slist, 0, cache->slist_len * sizeof(void *));
    }
    // Create node
    _z_lru_cache_node_t *node = _z_lru_cache_node_create(value, value_size);
    size_t *del_idx_addr = NULL;
    size_t del_idx = 0;
    if (node == NULL) {
        _Z_ERROR_RETURN(_Z_ERR_SYSTEM_OUT_OF_MEMORY);
    }
    // Check capacity
    if (cache->len == cache->capacity) {
        // Delete lru entry
        del_idx = _z_lru_cache_delete_last(cache, compare, elem_hash);
        del_idx_addr = &del_idx;
    }
    // Update the cache
    _z_lru_cache_insert_node(cache, node, compare, del_idx_addr, elem_hash);
    return _Z_RES_OK;
}

void _z_lru_cache_clear(_z_lru_cache_t *cache, z_element_clear_f clear) {
    // Reset slist
    if (cache->slist != NULL) {
        memset(cache->slist, 0, cache->slist_len * sizeof(void *));
    }
    // Clear list
    _z_lru_cache_clear_list(cache, clear);
    // Reset cache
    cache->len = 0;
    cache->head = NULL;
    cache->tail = NULL;
}

void _z_lru_cache_delete(_z_lru_cache_t *cache, z_element_clear_f clear) {
    _z_lru_cache_clear(cache, clear);
    z_free(cache->slist);
    cache->slist = NULL;
}
