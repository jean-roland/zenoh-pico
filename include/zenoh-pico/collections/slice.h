//
// Copyright (c) 2022 ZettaScale Technology
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

#ifndef ZENOH_PICO_COLLECTIONS_SLICE_H
#define ZENOH_PICO_COLLECTIONS_SLICE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "zenoh-pico/utils/result.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    void (*deleter)(void *data, void *context);
    void *context;
} _z_delete_context_t;

// Warning: None of the sub-types require a non-0 initialization. Add a init function if it changes.
static inline _z_delete_context_t _z_delete_context_null(void) { return (_z_delete_context_t){0}; }

static inline _z_delete_context_t _z_delete_context_create(void (*deleter)(void *context, void *data), void *context) {
    _z_delete_context_t ret;
    ret.deleter = deleter;
    ret.context = context;
    return ret;
}
static inline bool _z_delete_context_is_null(const _z_delete_context_t *c) { return c->deleter == NULL; }
static inline void _z_delete_context_delete(_z_delete_context_t *c, void *data) {
    if (!_z_delete_context_is_null(c)) {
        c->deleter(data, c->context);
    }
}
_z_delete_context_t _z_delete_context_default(void);
_z_delete_context_t _z_delete_context_static(void);

/*-------- Slice --------*/
/**
 * An array of bytes.
 *
 * Members:
 *   size_t len: The length of the bytes array.
 *   uint8_t *start: A pointer to the bytes array.
 *   _z_delete_context_t delete_context - context used to delete the data.
 */
typedef struct {
    size_t len;
    const uint8_t *start;
    _z_delete_context_t _delete_context;
} _z_slice_t;

static inline _z_slice_t _z_slice_null(void) { return (_z_slice_t){0}; }
static inline void _z_slice_reset(_z_slice_t *bs) { *bs = _z_slice_null(); }
static inline bool _z_slice_is_empty(const _z_slice_t *bs) { return bs->len == 0; }
static inline bool _z_slice_check(const _z_slice_t *slice) { return slice->start != NULL; }
static inline _z_slice_t _z_slice_alias(const _z_slice_t bs) {
    _z_slice_t ret;
    ret.len = bs.len;
    ret.start = bs.start;
    ret._delete_context = _z_delete_context_null();
    return ret;
}
static inline _z_slice_t _z_slice_from_buf_custom_deleter(const uint8_t *p, size_t len, _z_delete_context_t dc) {
    _z_slice_t bs;
    bs.start = p;
    bs.len = len;
    bs._delete_context = dc;
    return bs;
}
static inline _z_slice_t _z_slice_alias_buf(const uint8_t *p, size_t len) {
    return _z_slice_from_buf_custom_deleter(p, len, _z_delete_context_null());
}
static inline void _z_slice_clear(_z_slice_t *bs) {
    if (bs->start != NULL) {
        _z_delete_context_delete(&bs->_delete_context, (void *)bs->start);
        bs->len = 0;
        bs->start = NULL;
    }
}

z_result_t _z_slice_init(_z_slice_t *bs, size_t capacity);
_z_slice_t _z_slice_make(size_t capacity);
_z_slice_t _z_slice_copy_from_buf(const uint8_t *bs, size_t len);
_z_slice_t _z_slice_steal(_z_slice_t *b);
z_result_t _z_slice_copy(_z_slice_t *dst, const _z_slice_t *src);
z_result_t _z_slice_n_copy(_z_slice_t *dst, const _z_slice_t *src, size_t offset, size_t len);
_z_slice_t _z_slice_duplicate(const _z_slice_t *src);
z_result_t _z_slice_move(_z_slice_t *dst, _z_slice_t *src);
bool _z_slice_eq(const _z_slice_t *left, const _z_slice_t *right);
void _z_slice_free(_z_slice_t **bs);
bool _z_slice_is_alloced(const _z_slice_t *s);

/*-------- QSlice --------*/
typedef struct {
    size_t len;
    const uint8_t *start;
    bool _is_alloced;
} _z_qslice_t;

static inline _z_qslice_t _z_qslice_null(void) { return (_z_qslice_t){0}; }
static inline bool _z_qslice_is_empty(const _z_qslice_t *bs) { return bs->len == 0; }
static inline bool _z_qslice_check(const _z_qslice_t *slice) { return slice->start != NULL; }
static inline bool _z_qslice_is_alloced(const _z_qslice_t *slice) { return slice->_is_alloced; }
static inline _z_qslice_t _z_qslice_alias(const _z_qslice_t bs) {
    _z_qslice_t ret;
    ret.len = bs.len;
    ret.start = bs.start;
    ret._is_alloced = false;
    return ret;
}
static inline _z_qslice_t _z_qslice_alias_buf(const uint8_t *p, size_t len) {
    _z_qslice_t bs;
    bs.len = len;
    bs.start = p;
    bs._is_alloced = false;
    return bs;
}

static inline _z_qslice_t _z_qslice_steal_buf(const uint8_t *p, size_t len) {
    _z_qslice_t bs;
    bs.len = len;
    bs.start = p;
    bs._is_alloced = true;
    return bs;
}

static inline _z_qslice_t _z_qslice_from_slice(_z_slice_t *bs) {
    _z_qslice_t qs;
    qs.len = bs->len;
    qs.start = bs->start;
    qs._is_alloced = _z_slice_is_alloced(bs);
    return qs;
}

static inline _z_qslice_t _z_qslice_steal_slice(_z_slice_t *bs) {
    _z_qslice_t qs = _z_qslice_from_slice(bs);
    *bs = _z_slice_null();
    return qs;
}

z_result_t _z_qslice_init(_z_qslice_t *bs, size_t capacity);
_z_qslice_t _z_qslice_make(size_t capacity);
_z_qslice_t _z_qslice_copy_from_buf(const uint8_t *bs, size_t len);
_z_qslice_t _z_qslice_steal(_z_qslice_t *b);
z_result_t _z_qslice_copy(_z_qslice_t *dst, const _z_qslice_t *src);
z_result_t _z_qslice_n_copy(_z_qslice_t *dst, const _z_qslice_t *src, size_t offset, size_t len);
_z_qslice_t _z_qslice_duplicate(const _z_qslice_t *src);
z_result_t _z_qslice_move(_z_qslice_t *dst, _z_qslice_t *src);
bool _z_qslice_eq(const _z_qslice_t *left, const _z_qslice_t *right);
void _z_qslice_clear(_z_qslice_t *bs);
void _z_qslice_free(_z_qslice_t **bs);

static inline _z_slice_t _z_slice_from_qslice(_z_qslice_t *qs) {
    _z_slice_t bs;
    bs.len = qs->len;
    bs.start = qs->start;
    if (qs->_is_alloced) {
        bs._delete_context = _z_delete_context_default();
    } else {
        bs._delete_context = _z_delete_context_null();
    }
    return bs;
}

static inline _z_slice_t _z_slice_steal_qslice(_z_qslice_t *qs) {
    _z_slice_t bs = _z_slice_from_qslice(qs);
    *qs = _z_qslice_null();
    return bs;
}

#ifdef __cplusplus
}
#endif

#endif /* ZENOH_PICO_COLLECTIONS_SLICE_H */
