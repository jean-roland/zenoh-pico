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

#ifndef ZENOH_PICO_PROTOCOL_MSG_H
#define ZENOH_PICO_PROTOCOL_MSG_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "zenoh-pico/api/constants.h"
#include "zenoh-pico/collections/array.h"
#include "zenoh-pico/collections/bytes.h"
#include "zenoh-pico/collections/element.h"
#include "zenoh-pico/collections/string.h"
#include "zenoh-pico/link/endpoint.h"
#include "zenoh-pico/net/query.h"
#include "zenoh-pico/protocol/core.h"
#include "zenoh-pico/protocol/ext.h"

#define _Z_DEFAULT_BATCH_SIZE 65535
#define _Z_DEFAULT_RESOLUTION_SIZE 2

#define _Z_DECLARE_CLEAR(layer, name) void _z_##layer##_msg_clear_##name(_z_##name##_t *m, uint8_t header)
#define _Z_DECLARE_CLEAR_NOH(layer, name) void _z_##layer##_msg_clear_##name(_z_##name##_t *m)

// NOTE: 16 bits (2 bytes) may be prepended to the serialized message indicating the total length
//       in bytes of the message, resulting in the maximum length of a message being 65_535 bytes.
//       This is necessary in those stream-oriented transports (e.g., TCP) that do not preserve
//       the boundary of the serialized messages. The length is encoded as little-endian.
//       In any case, the length of a message must not exceed 65_535 bytes.
#define _Z_MSG_LEN_ENC_SIZE 2

/*=============================*/
/*         Message IDs         */
/*=============================*/
/* Scouting Messages */
#define _Z_MID_SCOUT 0x01
#define _Z_MID_HELLO 0x02

/* Transport Messages */
#define _Z_MID_T_OAM 0x00
#define _Z_MID_T_INIT 0x01
#define _Z_MID_T_OPEN 0x02
#define _Z_MID_T_CLOSE 0x03
#define _Z_MID_T_KEEP_ALIVE 0x04
#define _Z_MID_T_FRAME 0x05
#define _Z_MID_T_FRAGMENT 0x06
#define _Z_MID_T_JOIN 0x07

/* Network Messages */
#define _Z_MID_N_OAM 0x1f
#define _Z_MID_N_DECLARE 0x1e
#define _Z_MID_N_PUSH 0x1d
#define _Z_MID_N_REQUEST 0x1c
#define _Z_MID_N_RESPONSE 0x1b
#define _Z_MID_N_RESPONSE_FINAL 0x1a

/* Zenoh Messages */
#define _Z_MID_Z_DATA 0x0c
#define _Z_MID_Z_QUERY 0x0d
#define _Z_MID_Z_PULL 0x0e
#define _Z_MID_Z_UNIT 0x0f
#define _Z_MID_Z_LINK_STATE_LIST 0x10

/*=============================*/
/*        Message flags        */
/*=============================*/
#define _Z_FLAG_T_Z 0x80  // 1 << 7

// Scout message flags:
//      I ZenohID          if I==1 then the ZenohID is present
//      Z Extensions       if Z==1 then Zenoh extensions are present
#define _Z_FLAG_T_SCOUT_I 0x08  // 1 << 3

// Hello message flags:
//      L Locators         if L==1 then Locators are present
//      Z Extensions       if Z==1 then Zenoh extensions are present
#define _Z_FLAG_T_HELLO_L 0x20  // 1 << 5

// Join message flags:
//      T Lease period     if T==1 then the lease period is in seconds else in milliseconds
//      S Size params      if S==1 then size parameters are exchanged
//      Z Extensions       if Z==1 then Zenoh extensions are present
#define _Z_FLAG_T_JOIN_T 0x40  // 1 << 6
#define _Z_FLAG_T_JOIN_S 0x20  // 1 << 5

// Init message flags:
//      A Ack              if A==1 then the message is an acknowledgment (aka InitAck), otherwise InitSyn
//      S Size params      if S==1 then size parameters are exchanged
//      Z Extensions       if Z==1 then Zenoh extensions are present
#define _Z_FLAG_T_INIT_A 0x20  // 1 << 5
#define _Z_FLAG_T_INIT_S 0x40  // 1 << 6

// Open message flags:
//      A Ack              if A==1 then the message is an acknowledgment (aka OpenAck), otherwise OpenSyn
//      T Lease period     if T==1 then the lease period is in seconds else in milliseconds
//      Z Extensions       if Z==1 then Zenoh extensions are present
#define _Z_FLAG_T_OPEN_A 0x20  // 1 << 5
#define _Z_FLAG_T_OPEN_T 0x40  // 1 << 6

// Frame message flags:
//      R Reliable         if R==1 it concerns the reliable channel, else the best-effort channel
//      Z Extensions       if Z==1 then Zenoh extensions are present
#define _Z_FLAG_T_FRAME_R 0x20  // 1 << 5

// Frame message flags:
//      R Reliable         if R==1 it concerns the reliable channel, else the best-effort channel
//      M More             if M==1 then other fragments will follow
//      Z Extensions       if Z==1 then Zenoh extensions are present
#define _Z_FLAG_T_FRAGMENT_R 0x20  // 1 << 5
#define _Z_FLAG_T_FRAGMENT_M 0x40  // 1 << 6

// Close message flags:
//      S Session Close    if S==1 Session close or S==0 Link close
//      Z Extensions       if Z==1 then Zenoh extensions are present
#define _Z_FLAG_T_CLOSE_S 0x20  // 1 << 5

/*=============================*/
/*        Network flags        */
/*=============================*/
#define _Z_FLAG_N_Z 0x80  // 1 << 7

// PUSH message flags:
//      N Named            if N==1 then the key expr has name/suffix
//      M Mapping          if M==1 then keyexpr mapping is the one declared by the sender, otherwise by the receiver
//      Z Extensions       if Z==1 then Zenoh extensions are present
#define _Z_FLAG_N_PUSH_N 0x20  // 1 << 5
#define _Z_FLAG_N_PUSH_M 0x40  // 1 << 6

// REQUEST message flags:
//      N Named            if N==1 then the key expr has name/suffix
//      M Mapping          if M==1 then keyexpr mapping is the one declared by the sender, otherwise by the receiver
//      Z Extensions       if Z==1 then Zenoh extensions are present
#define _Z_FLAG_N_REQUEST_N 0x20  // 1 << 5
#define _Z_FLAG_N_REQUEST_M 0x40  // 1 << 6

// RESPONSE message flags:
//      N Named            if N==1 then the key expr has name/suffix
//      M Mapping          if M==1 then keyexpr mapping is the one declared by the sender, otherwise by the receiver
//      Z Extensions       if Z==1 then Zenoh extensions are present
#define _Z_FLAG_N_RESPONSE_N 0x20  // 1 << 5
#define _Z_FLAG_N_RESPONSE_M 0x40  // 1 << 6

// RESPONSE FINAL message flags:
//      Z Extensions       if Z==1 then Zenoh extensions are present
// #define _Z_FLAG_N_RESPONSE_X 0x20  // 1 << 5
// #define _Z_FLAG_N_RESPONSE_X 0x40  // 1 << 6

/* Zenoh message flags */
#define _Z_FLAG_Z_Z 0x80
#define _Z_FLAG_Z_B 0x40  // 1 << 6 | QueryPayload      if B==1 then QueryPayload is present
#define _Z_FLAG_Z_D 0x20  // 1 << 5 | Dropping          if D==1 then the message can be dropped
#define _Z_FLAG_Z_F \
    0x20  // 1 << 5 | Final             if F==1 then this is the final message (e.g., ReplyContext, Pull)
#define _Z_FLAG_Z_I 0x40  // 1 << 6 | DataInfo          if I==1 then DataInfo is present
#define _Z_FLAG_Z_K 0x80  // 1 << 7 | ResourceKey       if K==1 then keyexpr is string
#define _Z_FLAG_Z_N 0x40  // 1 << 6 | MaxSamples        if N==1 then the MaxSamples is indicated
#define _Z_FLAG_Z_P 0x80  // 1 << 7 | Period            if P==1 then a period is present
#define _Z_FLAG_Z_Q 0x40  // 1 << 6 | QueryableKind     if Q==1 then the queryable kind is present
#define _Z_FLAG_Z_R \
    0x20  // 1 << 5 | Reliable          if R==1 then it concerns the reliable channel, best-effort otherwise
#define _Z_FLAG_Z_S 0x40  // 1 << 6 | SubMode           if S==1 then the declaration SubMode is indicated
#define _Z_FLAG_Z_T 0x20  // 1 << 5 | QueryTarget       if T==1 then the query target is present
#define _Z_FLAG_Z_X 0x00  // Unused flags are set to zero

/*=============================*/
/*       Message header        */
/*=============================*/
#define _Z_MID_MASK 0x1f
#define _Z_FLAGS_MASK 0xe0

/*=============================*/
/*       Message helpers       */
/*=============================*/
#define _Z_MID(h) (_Z_MID_MASK & (h))
#define _Z_FLAGS(h) (_Z_FLAGS_MASK & (h))
#define _Z_HAS_FLAG(h, f) (((h) & (f)) != 0)
#define _Z_SET_FLAG(h, f) (h |= f)

/*=============================*/
/*       Declaration IDs       */
/*=============================*/
#define _Z_DECL_RESOURCE 0x01
#define _Z_DECL_PUBLISHER 0x02
#define _Z_DECL_SUBSCRIBER 0x03
#define _Z_DECL_QUERYABLE 0x04
#define _Z_DECL_FORGET_RESOURCE 0x11
#define _Z_DECL_FORGET_PUBLISHER 0x12
#define _Z_DECL_FORGET_SUBSCRIBER 0x13
#define _Z_DECL_FORGET_QUERYABLE 0x14

/*=============================*/
/*        Close reasons        */
/*=============================*/
#define _Z_CLOSE_GENERIC 0x00
#define _Z_CLOSE_UNSUPPORTED 0x01
#define _Z_CLOSE_INVALID 0x02
#define _Z_CLOSE_MAX_TRANSPORTS 0x03
#define _Z_CLOSE_MAX_LINKS 0x04
#define _Z_CLOSE_EXPIRED 0x05

/*=============================*/
/*       DataInfo flags        */
/*=============================*/
#define _Z_DATA_INFO_SLICED 0x01  // 1 << 0
#define _Z_DATA_INFO_KIND 0x02    // 1 << 1
#define _Z_DATA_INFO_ENC 0x04     // 1 << 2
#define _Z_DATA_INFO_TSTAMP 0x08  // 1 << 3
// Reserved: bits 4-6
#define _Z_DATA_INFO_SRC_ID 0x80   // 1 << 7
#define _Z_DATA_INFO_SRC_SN 0x100  // 1 << 8

/*------------------ Payload field ------------------*/
//  7 6 5 4 3 2 1 0
// +-+-+-+-+-+-+-+-+
// ~    Length     ~
// +---------------+
// ~    Buffer     ~
// +---------------+
//
typedef _z_bytes_t _z_payload_t;
void _z_payload_clear(_z_payload_t *p);

/*------------------ Locators Field ------------------*/
//  7 6 5 4 3 2 1 0
// +-+-+-+-+-+-+-+-+
// ~  Num of Locs  ~
// +---------------+
// ~   [Locator]   ~
// +---------------+
//
// NOTE: Locators is an array of strings and are encoded as such

/*=============================*/
/*     Message decorators      */
/*=============================*/

// -- Priority decorator
//
// The Priority is a message decorator containing
// informations related to the priority of the frame/zenoh message.
//
//  7 6 5 4 3 2 1 0
// +-+-+-+-+-+-+-+-+
// | ID  |  Prio   |
// +-+-+-+---------+
//
// WARNING: zenoh-pico does not support priorities and QoS.

/*=============================*/
/*       Zenoh Messages        */
/*=============================*/
/*------------------ ResKey Field ------------------*/
//  7 6 5 4 3 2 1 0
// +-+-+-+-+-+-+-+-+
// ~      RID      ~
// +---------------+
// ~    Suffix     ~ if K==1
// +---------------+
//
void _z_keyexpr_clear(_z_keyexpr_t *rk);
void _z_keyexpr_free(_z_keyexpr_t **rk);

/*------------------ Resource Declaration ------------------*/
//  7 6 5 4 3 2 1 0
// +-+-+-+-+-+-+-+-+
// |K|X|X|   RES   |
// +---------------+
// ~      RID      ~
// +---------------+
// ~    ResKey     ~ if K==1 then keyexpr is string
// +---------------+
//
typedef struct {
    _z_keyexpr_t _key;
    _z_zint_t _id;
} _z_res_decl_t;
void _z_declaration_clear_resource(_z_res_decl_t *dcl);

/*------------------ Forget Resource Declaration ------------------*/
//  7 6 5 4 3 2 1 0
// +-+-+-+-+-+-+-+-+
// |X|X|X|  F_RES  |
// +---------------+
// ~      RID      ~
// +---------------+
//
typedef struct {
    _z_zint_t _rid;
} _z_forget_res_decl_t;
void _z_declaration_clear_forget_resource(_z_forget_res_decl_t *dcl);

/*------------------ Publisher Declaration ------------------*/
//  7 6 5 4 3 2 1 0
// +-+-+-+-+-+-+-+-+
// |K|X|X|   PUB   |
// +---------------+
// ~    ResKey     ~ if K==1 then keyexpr is string
// +---------------+
//
typedef struct {
    _z_keyexpr_t _key;
} _z_pub_decl_t;
void _z_declaration_clear_publisher(_z_pub_decl_t *dcl);

/*------------------ Forget Publisher Declaration ------------------*/
//  7 6 5 4 3 2 1 0
// +-+-+-+-+-+-+-+-+
// |K|X|X|  F_PUB  |
// +---------------+
// ~    ResKey     ~ if K==1 then keyexpr is string
// +---------------+
//
typedef struct {
    _z_keyexpr_t _key;
} _z_forget_pub_decl_t;
void _z_declaration_clear_forget_publisher(_z_forget_pub_decl_t *dcl);

/*------------------ SubInfo Field ------------------*/
//  7 6 5 4 3 2 1 0
// +-+-+-+---------+
// |P|X|X|  SM_ID  |
// +---------------+
// ~    Period     ~ if P==1
// +---------------+
//
void _z_subinfo_clear(_z_subinfo_t *si);

/*------------------ Subscriber Declaration ------------------*/
//  7 6 5 4 3 2 1 0
// +-+-+-+-+-+-+-+-+
// |K|S|R|   SUB   |
// +---------------+
// ~    ResKey     ~ if K==1 then keyexpr is string
// +---------------+
// ~    SubInfo    ~ if S==1. Otherwise: SubMode=Push
// +---------------+
//
// - if R==1 then the subscription is reliable, best-effort otherwise.
//
typedef struct {
    _z_keyexpr_t _key;
    _z_subinfo_t _subinfo;
} _z_sub_decl_t;
void _z_declaration_clear_subscriber(_z_sub_decl_t *dcl);

/*------------------ Forget Subscriber Message ------------------*/
//  7 6 5 4 3 2 1 0
// +-+-+-+-+-+-+-+-+
// |K|X|X|  F_SUB  |
// +---------------+
// ~    ResKey     ~ if K==1 then keyexpr is string
// +---------------+
//
typedef struct {
    _z_keyexpr_t _key;
} _z_forget_sub_decl_t;
void _z_declaration_clear_forget_subscriber(_z_forget_sub_decl_t *dcl);

/*------------------ Queryable Declaration ------------------*/
//  7 6 5 4 3 2 1 0
// +-+-+-+-+-+-+-+-+
// |K|Q|X|  QABLE  |
// +---------------+
// ~     ResKey    ~ if K==1 then keyexpr is string
// +---------------+
// ~   QablInfo    ~ if Q==1
// +---------------+
//
typedef struct {
    _z_keyexpr_t _key;
    _z_zint_t _complete;
    _z_zint_t _distance;
} _z_qle_decl_t;
void _z_declaration_clear_queryable(_z_qle_decl_t *dcl);

/*------------------ Forget Queryable Declaration ------------------*/
//  7 6 5 4 3 2 1 0
// +-+-+-+-+-+-+-+-+
// |K|X|X| F_QABLE |
// +---------------+
// ~    ResKey     ~ if K==1 then keyexpr is string
// +---------------+
//
typedef struct {
    _z_keyexpr_t _key;
} _z_forget_qle_decl_t;
void _z_declaration_clear_forget_queryable(_z_forget_qle_decl_t *dcl);

typedef struct {
    union {
        _z_res_decl_t _res;
        _z_forget_res_decl_t _forget_res;
        _z_pub_decl_t _pub;
        _z_forget_pub_decl_t _forget_pub;
        _z_sub_decl_t _sub;
        _z_forget_sub_decl_t _forget_sub;
        _z_qle_decl_t _qle;
        _z_forget_qle_decl_t _forget_qle;
    } _body;
    uint8_t _header;
} _z_declaration_t;
void _z_declaration_clear(_z_declaration_t *dcl);

/*------------------ Timestamp Field ------------------*/
//  7 6 5 4 3 2 1 0
// +-+-+-+---------+
// ~     Time      ~  Encoded as _z_zint_t
// +---------------+
// ~      ID       ~
// +---------------+
//
void _z_timestamp_clear(_z_timestamp_t *ts);

/*------------------ Data Info Field ------------------*/
//  7 6 5 4 3 2 1 0
// +-+-+-+-+-+-+-+-+
// ~    options    ~
// +---------------+
// ~      kind     ~ if options & (1 << 1)
// +---------------+
// ~   encoding    ~ if options & (1 << 2)
// +---------------+
// ~   timestamp   ~ if options & (1 << 3)
// +---------------+
// ~   source_id   ~ if options & (1 << 7)
// +---------------+
// ~   source_sn   ~ if options & (1 << 8)
// +---------------+
//
// - if options & (1 << 0) then the payload is sliced
typedef struct {
    _z_bytes_t _source_id;
    _z_timestamp_t _tstamp;
    _z_zint_t _flags;
    _z_zint_t _source_sn;
    _z_encoding_t _encoding;
    uint8_t _kind;
} _z_data_info_t;
void _z_data_info_clear(_z_data_info_t *di);
typedef struct {
    _z_id_t _id;
    uint32_t _entity_id;
    uint32_t _source_sn;
} _z_source_info_t;

/*------------------ Data Message ------------------*/
//  7 6 5 4 3 2 1 0
// +-+-+-+-+-+-+-+-+
// |K|I|D|  DATA   |
// +-+-+-+---------+
// ~    ResKey     ~ if K==1 then keyexpr is string
// +---------------+
// ~    DataInfo   ~ if I==1
// +---------------+
// ~    Payload    ~
// +---------------+
//
// - if D==1 then the message can be dropped for congestion control reasons.
//
typedef struct {
    _z_data_info_t _info;
    _z_keyexpr_t _key;
    _z_payload_t _payload;
} _z_msg_data_t;
void _z_msg_clear_data(_z_msg_data_t *msg);

/*------------------ Unit Message ------------------*/
//  7 6 5 4 3 2 1 0
// +-+-+-+-+-+-+-+-+
// |X|X|D|  UNIT   |
// +-+-+-+---------+
//
// - if D==1 then the message can be dropped for congestion control reasons.
//
typedef struct {
    uint8_t __dummy;  // Just to avoid empty structures that might cause undefined behavior
} _z_msg_unit_t;
void _z_msg_clear_unit(_z_msg_unit_t *unt);

/*------------------ Pull Message ------------------*/

/*------------------ Query Message ------------------*/
//   7 6 5 4 3 2 1 0
//  +-+-+-+-+-+-+-+-+
//  |Z|C|P|  QUERY  |
//  +-+-+-+---------+
//  ~ params        ~  if P==1 -- <utf8;z32>
//  +---------------+
//  ~ consolidation ~  if C==1 -- u8
//  +---------------+
//  ~ [qry_exts]    ~  if Z==1
//  +---------------+
#define _Z_FLAG_Z_Q_P 0x20  // 1 << 6 | Period            if P==1 then a period is present
typedef struct {
    _z_bytes_t _parameters;
    _z_source_info_t _info;
    _z_value_t _value;
    z_consolidation_mode_t _consolidation;
} _z_msg_query_t;
typedef struct {
    _Bool info;
    _Bool body;
    _Bool consolidation;
} _z_msg_query_reqexts_t;
_z_msg_query_reqexts_t _z_msg_query_required_extensions(const _z_msg_query_t *msg);
void _z_msg_clear_query(_z_msg_query_t *msg);

/*------------------ Zenoh Message ------------------*/
typedef union {
    _z_msg_data_t _data;
    _z_msg_query_t _query;
    _z_msg_unit_t _unit;
} _z_zenoh_body_t;

// TODO[remove]
typedef struct {
    _z_id_t _replier_id;
    _z_zint_t _qid;
    uint8_t _header;
} _z_reply_context_t;
_z_reply_context_t *_z_msg_make_reply_context(_z_zint_t qid, _z_id_t replier_id, _Bool is_final);
// TODO[remove end]

typedef struct {
    uint8_t _header;
    _z_zenoh_body_t _body;
} _z_zenoh_message_t;
void _z_msg_clear(_z_zenoh_message_t *m);
void _z_msg_free(_z_zenoh_message_t **m);
_Z_ELEM_DEFINE(_z_zenoh_message, _z_zenoh_message_t, _z_noop_size, _z_msg_clear, _z_noop_copy)
_Z_VEC_DEFINE(_z_zenoh_message, _z_zenoh_message_t)

/*------------------ Builders ------------------*/
_z_declaration_t _z_msg_make_declaration_resource(_z_zint_t id, _z_keyexpr_t key);
_z_declaration_t _z_msg_make_declaration_forget_resource(_z_zint_t rid);
_z_declaration_t _z_msg_make_declaration_publisher(_z_keyexpr_t key);
_z_declaration_t _z_msg_make_declaration_forget_publisher(_z_keyexpr_t key);
_z_declaration_t _z_msg_make_declaration_subscriber(_z_keyexpr_t key, _z_subinfo_t subinfo);
_z_declaration_t _z_msg_make_declaration_forget_subscriber(_z_keyexpr_t key);
_z_declaration_t _z_msg_make_declaration_queryable(_z_keyexpr_t key, _z_zint_t complete, _z_zint_t distance);
_z_declaration_t _z_msg_make_declaration_forget_queryable(_z_keyexpr_t key);
_z_zenoh_message_t _z_msg_make_data(_z_keyexpr_t key, _z_data_info_t info, _z_payload_t payload, _Bool can_be_dropped);
_z_zenoh_message_t _z_msg_make_unit(_Bool can_be_dropped);
_z_zenoh_message_t _z_msg_make_pull(_z_keyexpr_t key, _z_zint_t pull_id, _z_zint_t max_samples, _Bool is_final);
_z_zenoh_message_t _z_msg_make_query(_z_keyexpr_t key, _z_bytes_t parameters, _z_zint_t qid,
                                     z_consolidation_mode_t consolidation, _z_value_t value);
_z_zenoh_message_t _z_msg_make_reply(_z_keyexpr_t key, _z_data_info_t info, _z_payload_t payload, _Bool can_be_dropped);

/*=============================*/
/*      Network Messages       */
/*=============================*/
/*------------------ Declaration  Message ------------------*/
// Flags:
// - X: Reserved
// - X: Reserved
// - Z: Extension      if Z==1 then at least one extension is present
//
// 7 6 5 4 3 2 1 0
// +-+-+-+-+-+-+-+-+
// |Z|X|X| DECLARE |
// +-+-+-+---------+
// ~  [decl_exts]  ~ -- if Flag(Z)==1
// +---------------+
// ~  declaration  ~
// +---------------+
//
typedef struct {
    _z_declaration_t _declaration;
} _z_n_msg_declare_t;
void _z_n_msg_clear_declare(_z_n_msg_declare_t *dcl);

/*------------------ Push Message ------------------*/

typedef struct {
    _z_timestamp_t _timestamp;
    _z_source_info_t _source_info;
} _z_m_push_commons_t;

typedef struct {
    _z_m_push_commons_t _commons;
} _z_m_del_t;
#define _Z_M_DEL_ID 0x02
#define _Z_FLAG_Z_D_T 0x20

typedef struct {
    _z_m_push_commons_t _commons;
    _z_bytes_t _payload;
    _z_encoding_t _encoding;
} _z_m_put_t;
#define _Z_M_PUT_ID 0x01
#define _Z_FLAG_Z_P_E 0x40
#define _Z_FLAG_Z_P_T 0x20

typedef struct {
    _z_m_push_commons_t _commons;
    _Bool _is_put;
    union {
        _z_m_del_t _del;
        _z_m_put_t _put;
    } _union;
} _z_push_body_t;
void _z_push_body_clear(_z_push_body_t *msg);

typedef struct {
    uint8_t _val;
} _z_n_qos_t;

#define _z_n_qos_make(express, nodrop, priority) \
    (_z_n_qos_t) { ._val = ((express << 4) | (nodrop << 3) | priority) }
#define _Z_N_QOS_DEFAULT _z_n_qos_make(0, 0, 5)

// Flags:
// - N: Named          if N==1 then the keyexpr has name/suffix
// - M: Mapping        if M==1 then keyexpr mapping is the one declared by the sender, otherwise by the receiver
// - Z: Extension      if Z==1 then at least one extension is present
//
//  7 6 5 4 3 2 1 0
// +-+-+-+-+-+-+-+-+
// |Z|M|N|  PUSH   |
// +-+-+-+---------+
// ~ key_scope:z?  ~
// +---------------+
// ~  key_suffix   ~  if N==1 -- <u8;z16>
// +---------------+
// ~  [push_exts]  ~  if Z==1
// +---------------+
// ~ ZenohMessage  ~
// +---------------+
//
typedef struct {
    _z_keyexpr_t _key;
    _z_timestamp_t _timestamp;
    _z_n_qos_t _qos;
    _z_push_body_t _body;
} _z_n_msg_push_t;
void _z_n_msg_clear_push(_z_n_msg_push_t *msg);

/*------------------ Request Message ------------------*/
typedef union {
} _z_request_body_t;
void _z_request_body_clear(_z_request_body_t *msg);

// Flags:
// - N: Named          if N==1 then the keyexpr has name/suffix
// - M: Mapping        if M==1 then keyexpr mapping is the one declared by the sender, otherwise by the receiver
// - Z: Extension      if Z==1 then at least one extension is present
//
//  7 6 5 4 3 2 1 0
// +-+-+-+-+-+-+-+-+
// |Z|M|N| REQUEST |
// +-+-+-+---------+
// ~ request_id:z32~
// +---------------+
// ~ key_scope:z16 ~
// +---------------+
// ~  key_suffix   ~  if N==1 -- <u8;z16>
// +---------------+
// ~   [req_exts]  ~  if Z==1
// +---------------+
// ~ ZenohMessage  ~
// +---------------+
//
typedef struct {
    _z_zint_t _rid;
    _z_keyexpr_t _key;
    _z_request_body_t _body;
} _z_n_msg_request_t;
void _z_n_msg_clear_request(_z_n_msg_request_t *msg);

/*------------------ Response Message ------------------*/
typedef union {
} _z_response_body_t;
void _z_response_body_clear(_z_response_body_t *msg);

// Flags:
// - T: Timestamp      If T==1 then the timestamp if present
// - E: Encoding       If E==1 then the encoding is present
// - Z: Extension      If Z==1 then at least one extension is present
//
//   7 6 5 4 3 2 1 0
//  +-+-+-+-+-+-+-+-+
//  |Z|E|T|  REPLY  |
//  +-+-+-+---------+
//  ~ ts: <u8;z16>  ~  if T==1
//  +---------------+
//  ~   encoding    ~  if E==1
//  +---------------+
//  ~  [repl_exts]  ~  if Z==1
//  +---------------+
//  ~ pl: <u8;z32>  ~  -- Payload
//  +---------------+
typedef struct {
    _z_timestamp_t _timestamp;
    _z_value_t _value;
    _z_source_info_t _source_info;
    z_consolidation_mode_t _consolidation;
} _z_msg_reply_t;
void _z_msg_reply_clear(_z_msg_reply_t *msg);
#define _Z_FLAG_Z_R_T 0x20
#define _Z_FLAG_Z_R_E 0x40

// Flags:
// - T: Timestamp      If T==1 then the timestamp if present
// - I: Infrastructure If I==1 then the error is related to the infrastructure else to the user
// - Z: Extension      If Z==1 then at least one extension is present
//
//   7 6 5 4 3 2 1 0
//  +-+-+-+-+-+-+-+-+
//  |Z|I|T|   ERR   |
//  +-+-+-+---------+
//  %   code:z16    %
//  +---------------+
//  ~ ts: <u8;z16>  ~  if T==1
//  +---------------+
//  ~  [err_exts]   ~  if Z==1
//  +---------------+
#define _Z_FLAG_Z_E_T 0x20
#define _Z_FLAG_Z_E_I 0x40
typedef struct {
    uint16_t _code;
    _Bool _is_infrastructure;
    _z_timestamp_t _timestamp;
    _z_source_info_t _ext_source_info;
    _z_value_t _ext_value;
} _z_msg_err_t;
void _z_msg_err_clear(_z_msg_err_t *err);

/// Flags:
/// - T: Timestamp      If T==1 then the timestamp if present
/// - X: Reserved
/// - Z: Extension      If Z==1 then at least one extension is present
///
///   7 6 5 4 3 2 1 0
///  +-+-+-+-+-+-+-+-+
///  |Z|X|T|   ACK   |
///  +-+-+-+---------+
///  ~ ts: <u8;z16>  ~  if T==1
///  +---------------+
///  ~  [err_exts]   ~  if Z==1
///  +---------------+
typedef struct {
    _z_timestamp_t _timestamp;
    _z_source_info_t _ext_source_info;
} _z_msg_ack_t;
#define _Z_FLAG_Z_A_T 0x20

/// Flags:
/// - T: Timestamp      If T==1 then the timestamp if present
/// - X: Reserved
/// - Z: Extension      If Z==1 then at least one extension is present
///
///   7 6 5 4 3 2 1 0
///  +-+-+-+-+-+-+-+-+
///  |Z|X|X|  PULL   |
///  +---------------+
///  ~  [pull_exts]  ~  if Z==1
///  +---------------+
typedef struct {
    _z_source_info_t _ext_source_info;
} _z_msg_pull_t;

/*------------------ Response Final Message ------------------*/
// Flags:
// - Z: Extension      if Z==1 then at least one extension is present
//
//  7 6 5 4 3 2 1 0
// +-+-+-+-+-+-+-+-+
// |Z|M|N| ResFinal|
// +-+-+-+---------+
// ~ request_id:z32~
// +---------------+
// ~  [reply_exts] ~  if Z==1
// +---------------+
//
typedef struct {
    _z_zint_t _rid;
} _z_n_msg_response_final_t;
void _z_n_msg_clear_response_final(_z_n_msg_response_final_t *msg);

/*------------------ Zenoh Message ------------------*/
typedef union {
    _z_n_msg_declare_t _declare;
    _z_n_msg_push_t _push;
    _z_n_msg_request_t _request;
    _z_msg_reply_t _response;
    _z_n_msg_response_final_t _response_f;
} _z_network_body_t;
typedef struct {
    _z_network_body_t _body;
    _z_msg_ext_vec_t _extensions;
    uint8_t _header;
} _z_network_message_t;
void _z_n_msg_clear(_z_network_message_t *m);
void _z_n_msg_free(_z_network_message_t **m);
_Z_ELEM_DEFINE(_z_network_message, _z_network_message_t, _z_noop_size, _z_n_msg_clear, _z_noop_copy)
_Z_VEC_DEFINE(_z_network_message, _z_network_message_t)

/*------------------ Builders ------------------*/
_z_network_message_t _z_n_msg_make_declare(_z_declaration_t declarations);
_z_network_message_t _z_n_msg_make_push(_z_keyexpr_t key, _z_push_body_t body, _Bool is_remote_mapping);
_z_network_message_t _z_n_msg_make_request(_z_zint_t rid, _z_keyexpr_t key, _z_request_body_t body,
                                           _Bool is_remote_mapping);
_z_network_message_t _z_n_msg_make_response(_z_zint_t rid, _z_keyexpr_t key, _z_response_body_t body,
                                            _Bool is_remote_mapping);
_z_network_message_t _z_n_msg_make_response_final(_z_zint_t rid);

/*=============================*/
/*     Transport Messages      */
/*=============================*/
/*------------------ Scout Message ------------------*/
// NOTE: 16 bits (2 bytes) may be prepended to the serialized message indicating the total length
//       in bytes of the message, resulting in the maximum length of a message being 65_535 bytes.
//       This is necessary in those stream-oriented transports (e.g., TCP) that do not preserve
//       the boundary of the serialized messages. The length is encoded as little-endian.
//       In any case, the length of a message must not exceed 65_535 bytes.
//
// The SCOUT message can be sent at any point in time to solicit HELLO messages from matching parties.
//
//  7 6 5 4 3 2 1 0
// +-+-+-+-+-+-+-+-+
// |Z|X|X|  SCOUT  |
// +-+-+-+---------+
// |    version    |
// +---------------+
// |zid_len|I| what| (#)(*)
// +-+-+-+-+-+-+-+-+
// ~      [u8]     ~ if Flag(I)==1 -- ZenohID
// +---------------+
//
// (#) ZID length. If Flag(I)==1 it indicates how many bytes are used for the ZenohID bytes.
//     A ZenohID is minimum 1 byte and maximum 16 bytes. Therefore, the actual lenght is computed as:
//         real_zid_len := 1 + zid_len
//
// (*) What. It indicates a bitmap of WhatAmI interests.
//    The valid bitflags are:
//    - 0b001: Router
//    - 0b010: Peer
//    - 0b100: Client
//
typedef struct {
    _z_id_t _zid;
    z_what_t _what;
    uint8_t _version;
} _z_s_msg_scout_t;
void _z_s_msg_clear_scout(_z_s_msg_scout_t *msg);

/*------------------ Hello Message ------------------*/
// NOTE: 16 bits (2 bytes) may be prepended to the serialized message indicating the total length
//       in bytes of the message, resulting in the maximum length of a message being 65_535 bytes.
//       This is necessary in those stream-oriented transports (e.g., TCP) that do not preserve
//       the boundary of the serialized messages. The length is encoded as little-endian.
//       In any case, the length of a message must not exceed 65_535 bytes.
//
// The HELLO message is sent in any of the following three cases:
//     1) in response to a SCOUT message;
//     2) to (periodically) advertise (e.g., on multicast) the Peer and the locators it is reachable at;
//     3) in a already established session to update the corresponding peer on the new capabilities
//        (i.e., whatmai) and/or new set of locators (i.e., added or deleted).
// Locators are expressed as:
// <code>
//  udp/192.168.0.2:1234
//  tcp/192.168.0.2:1234
//  udp/239.255.255.123:5555
// <code>
//
//  7 6 5 4 3 2 1 0
// +-+-+-+-+-+-+-+-+
// |Z|X|L|  HELLO  |
// +-+-+-+---------+
// |    version    |
// +---------------+
// |zid_len|X|X|wai| (*)
// +-+-+-+-+-+-+-+-+
// ~     [u8]      ~ -- ZenohID
// +---------------+
// ~   <utf8;z8>   ~ if Flag(L)==1 -- List of locators
// +---------------+
//
// (*) WhatAmI. It indicates the role of the zenoh node sending the HELLO message.
//    The valid WhatAmI values are:
//    - 0b00: Router
//    - 0b01: Peer
//    - 0b10: Client
//    - 0b11: Reserved
//
typedef struct {
    _z_id_t _zid;
    _z_locator_array_t _locators;
    z_whatami_t _whatami;
    uint8_t _version;
} _z_s_msg_hello_t;
void _z_s_msg_clear_hello(_z_s_msg_hello_t *msg);

/*------------------ Join Message ------------------*/
// # Join message
//
// NOTE: 16 bits (2 bytes) may be prepended to the serialized message indicating the total length
//       in bytes of the message, resulting in the maximum length of a message being 65_535 bytes.
//       This is necessary in those stream-oriented transports (e.g., TCP) that do not preserve
//       the boundary of the serialized messages. The length is encoded as little-endian.
//       In any case, the length of a message must not exceed 65_535 bytes.
//
// The JOIN message is sent on a multicast Locator to advertise the transport parameters.
//
//  7 6 5 4 3 2 1 0
// +-+-+-+-+-+-+-+-+
// |O|S|T|   JOIN  |
// +-+-+-+-+-------+
// ~             |Q~ if O==1
// +---------------+
// | v_maj | v_min | -- Protocol Version VMaj.VMin
// +-------+-------+
// ~    whatami    ~ -- Router, Peer or a combination of them
// +---------------+
// ~   zenoh_id    ~ -- PID of the sender of the JOIN message
// +---------------+
// ~     lease     ~ -- Lease period of the sender of the JOIN message(*)
// +---------------+
// ~ seq_num_res ~ if S==1(*) -- Otherwise 2^28 is assumed(**)
// +---------------+
// ~   [next_sn]   ~ (***)
// +---------------+
//
// - if Q==1 then the sender supports QoS.
//
// (*)   if T==1 then the lease period is expressed in seconds, otherwise in milliseconds
// (**)  if S==0 then 2^28 is assumed.
// (***) if Q==1 then 8 sequence numbers are present: one for each priority.
//       if Q==0 then only one sequence number is present.
//
typedef struct {
    _z_zint_t _reliable;
    _z_zint_t _best_effort;
} _z_coundit_sn_t;
typedef struct {
    union {
        _z_coundit_sn_t _plain;
        _z_coundit_sn_t _qos[Z_PRIORITIES_NUM];
    } _val;
    _Bool _is_qos;
} _z_conduit_sn_list_t;
typedef struct {
    _z_id_t _zid;
    _z_zint_t _lease;
    _z_conduit_sn_list_t _next_sn;
    uint16_t _batch_size;
    z_whatami_t _whatami;
    uint8_t _req_id_res;
    uint8_t _seq_num_res;
    uint8_t _version;
} _z_t_msg_join_t;
void _z_t_msg_clear_join(_z_t_msg_join_t *msg);

/*------------------ Init Message ------------------*/
// # Init message
//
// NOTE: 16 bits (2 bytes) may be prepended to the serialized message indicating the total length
//       in bytes of the message, resulting in the maximum length of a message being 65_535 bytes.
//       This is necessary in those stream-oriented transports (e.g., TCP) that do not preserve
//       the boundary of the serialized messages. The length is encoded as little-endian.
//       In any case, the length of a message must not exceed 65_535 bytes.
//
// The INIT message is sent on a specific Locator to initiate a session with the peer associated
// with that Locator. The initiator MUST send an INIT message with the A flag set to 0.  If the
// corresponding peer deems appropriate to initialize a session with the initiator, the corresponding
// peer MUST reply with an INIT message with the A flag set to 1.
//
// Flags:
// - A: Ack          if A==0 then the message is an InitSyn else it is an InitAck
// - S: Size params  if S==1 then size parameters are exchanged
// - Z: Extensions   if Z==1 then zenoh extensions will follow.
//
//  7 6 5 4 3 2 1 0
// +-+-+-+-+-+-+-+-+
// |Z|S|A|   INIT  |
// +-+-+-+---------+
// |    version    |
// +---------------+
// |zid_len|x|x|wai| (#)(*)
// +-------+-+-+---+
// ~      [u8]     ~ -- ZenohID of the sender of the INIT message
// +---------------+
// |x|x|kid|rid|fsn| \                -- SN/ID resolution (+)
// +---------------+  | if Flag(S)==1
// |      u16      |  |               -- Batch Size ($)
// |               | /
// +---------------+
// ~    <u8;z16>   ~ -- if Flag(A)==1 -- Cookie
// +---------------+
// ~   [InitExts]  ~ -- if Flag(Z)==1
// +---------------+
//
// If A==1 and S==0 then size parameters are (ie. S flag) are accepted.
//
// (*) WhatAmI. It indicates the role of the zenoh node sending the INIT
// message.
//    The valid WhatAmI values are:
//    - 0b00: Router
//    - 0b01: Peer
//    - 0b10: Client
//    - 0b11: Reserved
//
// (#) ZID length. It indicates how many bytes are used for the ZenohID bytes.
//     A ZenohID is minimum 1 byte and maximum 16 bytes. Therefore, the actual
//     lenght is computed as:
//         real_zid_len := 1 + zid_len
//
// (+) Sequence Number/ID resolution. It indicates the resolution and
// consequently the wire overhead
//     of various SN and ID in Zenoh.
//     - fsn: frame/fragment sequence number resolution. Used in Frame/Fragment
//     messages.
//     - rid: request ID resolution. Used in Request/Response messages.
//     - kid: key expression ID resolution. Used in Push/Request/Response
//     messages. The valid SN/ID resolution values are:
//     - 0b00: 8 bits
//     - 0b01: 16 bits
//     - 0b10: 32 bits
//     - 0b11: 64 bits
//
// ($) Batch Size. It indicates the maximum size of a batch the sender of the
//
typedef struct {
    _z_id_t _zid;
    _z_bytes_t _cookie;
    uint16_t _batch_size;
    z_whatami_t _whatami;
    uint8_t _req_id_res;
    uint8_t _seq_num_res;
    uint8_t _version;
} _z_t_msg_init_t;
void _z_t_msg_clear_init(_z_t_msg_init_t *msg);

/*------------------ Open Message ------------------*/
// NOTE: 16 bits (2 bytes) may be prepended to the serialized message indicating the total lenght
//       in bytes of the message, resulting in the maximum lenght of a message being 65_535 bytes.
//       This is necessary in those stream-oriented transports (e.g., TCP) that do not preserve
//       the boundary of the serialized messages. The length is encoded as little-endian.
//       In any case, the lenght of a message must not exceed 65_535 bytes.
//
// The OPEN message is sent on a link to finally open an initialized session with the peer.
//
// Flags:
// - A Ack           if A==1 then the message is an acknowledgment (aka OpenAck), otherwise OpenSyn
// - T Lease period  if T==1 then the lease period is in seconds else in milliseconds
// - Z Extensions    if Z==1 then Zenoh extensions are present
//
//  7 6 5 4 3 2 1 0
// +-+-+-+-+-+-+-+-+
// |Z|T|A|   OPEN  |
// +-+-+-+---------+
// %     lease     % -- Lease period of the sender of the OPEN message
// +---------------+
// %  initial_sn   % -- Initial SN proposed by the sender of the OPEN(*)
// +---------------+
// ~    <u8;z16>   ~ if Flag(A)==0 (**) -- Cookie
// +---------------+
// ~   [OpenExts]  ~ if Flag(Z)==1
// +---------------+
//
// (*)     The initial sequence number MUST be compatible with the sequence number resolution agreed in the
//         [`super::InitSyn`]-[`super::InitAck`] message exchange
// (**)    The cookie MUST be the same received in the [`super::InitAck`]from the corresponding zenoh node
//
typedef struct {
    _z_zint_t _lease;
    _z_zint_t _initial_sn;
    _z_bytes_t _cookie;
} _z_t_msg_open_t;
void _z_t_msg_clear_open(_z_t_msg_open_t *msg);

/*------------------ Close Message ------------------*/
// NOTE: 16 bits (2 bytes) may be prepended to the serialized message indicating the total length
//       in bytes of the message, resulting in the maximum length of a message being 65_535 bytes.
//       This is necessary in those stream-oriented transports (e.g., TCP) that do not preserve
//       the boundary of the serialized messages. The length is encoded as little-endian.
//       In any case, the length of a message must not exceed 65_535 bytes.
//
// The CLOSE message is sent in any of the following two cases:
//     1) in response to an OPEN message which is not accepted;
//     2) at any time to arbitrarly close the session with the corresponding peer.
//
// Flags:
// - S: Session Close  if S==1 Session close or S==0 Link close
// - X: Reserved
// - Z: Extensions     if Z==1 then zenoh extensions will follow.
//
//  7 6 5 4 3 2 1 0
// +-+-+-+-+-+-+-+-+
// |Z|X|S|  CLOSE  |
// +-+-+-+---------+
// |    Reason     |
// +---------------+
// ~  [CloseExts]  ~ if Flag(Z)==1
// +---------------+
//
typedef struct {
    uint8_t _reason;
} _z_t_msg_close_t;
void _z_t_msg_clear_close(_z_t_msg_close_t *msg);

/*------------------ Keep Alive Message ------------------*/
// NOTE: 16 bits (2 bytes) may be prepended to the serialized message indicating the total length
//       in bytes of the message, resulting in the maximum length of a message being 65_535 bytes.
//       This is necessary in those stream-oriented transports (e.g., TCP) that do not preserve
//       the boundary of the serialized messages. The length is encoded as little-endian.
//       In any case, the length of a message must not exceed 65_535 bytes.
//
// The KEEP_ALIVE message can be sent periodically to avoid the expiration of the session lease
// period in case there are no messages to be sent.
//
// Flags:
// - X: Reserved
// - X: Reserved
// - Z: Extensions     If Z==1 then Zenoh extensions will follow.
//
//  7 6 5 4 3 2 1 0
// +-+-+-+-+-+-+-+-+
// |Z|X|X| KALIVE  |
// +-+-+-+---------+
// ~  [KAliveExts] ~ if Flag(Z)==1
// +---------------+
//
typedef struct {
    uint8_t __dummy;  // Just to avoid empty structures that might cause undefined behavior
} _z_t_msg_keep_alive_t;
void _z_t_msg_clear_keep_alive(_z_t_msg_keep_alive_t *msg);

/*------------------ Frame Message ------------------*/
// NOTE: 16 bits (2 bytes) may be prepended to the serialized message indicating the total length
//       in bytes of the message, resulting in the maximum length of a message being 65_535 bytes.
//       This is necessary in those stream-oriented transports (e.g., TCP) that do not preserve
//       the boundary of the serialized messages. The length is encoded as little-endian.
//       In any case, the length of a message must not exceed 65_535 bytes.
//
// Flags:
// - R: Reliable       If R==1 it concerns the reliable channel, else the best-effort channel
// - X: Reserved
// - Z: Extensions     If Z==1 then zenoh extensions will follow.
//
//  7 6 5 4 3 2 1 0
// +-+-+-+-+-+-+-+-+
// |Z|X|R|  FRAME  |
// +-+-+-+---------+
// %    seq num    %
// +---------------+
// ~  [FrameExts]  ~ if Flag(Z)==1
// +---------------+
// ~  [NetworkMsg] ~
// +---------------+
//
// - if R==1 then the FRAME is sent on the reliable channel, best-effort otherwise.
//
typedef struct {
    _z_network_message_vec_t _messages;
    _z_zint_t _sn;
} _z_t_msg_frame_t;
void _z_t_msg_clear_frame(_z_t_msg_frame_t *msg);

/*------------------ Fragment Message ------------------*/
// The Fragment message is used to transmit on the wire large Zenoh Message that require fragmentation
// because they are larger thatn the maximum batch size (i.e. 2^16-1) and/or the link MTU.
//
// The [`Fragment`] message flow is the following:
//
// Flags:
// - R: Reliable       if R==1 it concerns the reliable channel, else the best-effort channel
// - M: More           if M==1 then other fragments will follow
// - Z: Extensions     if Z==1 then zenoh extensions will follow.
//
//  7 6 5 4 3 2 1 0
// +-+-+-+-+-+-+-+-+
// |Z|M|R| FRAGMENT|
// +-+-+-+---------+
// %    seq num    %
// +---------------+
// ~   [FragExts]  ~ if Flag(Z)==1
// +---------------+
// ~      [u8]     ~
// +---------------+
//
typedef struct {
    _z_payload_t _payload;
    _z_zint_t _sn;
} _z_t_msg_fragment_t;
void _z_t_msg_clear_fragment(_z_t_msg_fragment_t *msg);

/*------------------ Transport Message ------------------*/
typedef union {
    _z_t_msg_join_t _join;
    _z_t_msg_init_t _init;
    _z_t_msg_open_t _open;
    _z_t_msg_close_t _close;
    _z_t_msg_keep_alive_t _keep_alive;
    _z_t_msg_frame_t _frame;
    _z_t_msg_fragment_t _fragment;
} _z_transport_body_t;

typedef struct {
    _z_transport_body_t _body;
    uint8_t _header;
} _z_transport_message_t;
void _z_t_msg_clear(_z_transport_message_t *msg);

/*------------------ Builders ------------------*/
_z_transport_message_t _z_t_msg_make_join(z_whatami_t whatami, _z_zint_t lease, _z_id_t zid,
                                          _z_conduit_sn_list_t next_sn);
_z_transport_message_t _z_t_msg_make_init_syn(z_whatami_t whatami, _z_id_t zid);
_z_transport_message_t _z_t_msg_make_init_ack(z_whatami_t whatami, _z_id_t zid, _z_bytes_t cookie);
_z_transport_message_t _z_t_msg_make_open_syn(_z_zint_t lease, _z_zint_t initial_sn, _z_bytes_t cookie);
_z_transport_message_t _z_t_msg_make_open_ack(_z_zint_t lease, _z_zint_t initial_sn);
_z_transport_message_t _z_t_msg_make_close(uint8_t reason, _Bool link_only);
_z_transport_message_t _z_t_msg_make_keep_alive(void);
_z_transport_message_t _z_t_msg_make_frame(_z_zint_t sn, _z_network_message_vec_t messages, _Bool is_reliable);
_z_transport_message_t _z_t_msg_make_frame_header(_z_zint_t sn, _Bool is_reliable);
_z_transport_message_t _z_t_msg_make_fragment(_z_zint_t sn, _z_payload_t messages, _Bool is_reliable, _Bool is_last);

/*------------------ Copy ------------------*/
void _z_t_msg_copy(_z_transport_message_t *clone, _z_transport_message_t *msg);
void _z_t_msg_copy_join(_z_t_msg_join_t *clone, _z_t_msg_join_t *msg);
void _z_t_msg_copy_init(_z_t_msg_init_t *clone, _z_t_msg_init_t *msg);
void _z_t_msg_copy_open(_z_t_msg_open_t *clone, _z_t_msg_open_t *msg);
void _z_t_msg_copy_close(_z_t_msg_close_t *clone, _z_t_msg_close_t *msg);
void _z_t_msg_copy_keep_alive(_z_t_msg_keep_alive_t *clone, _z_t_msg_keep_alive_t *msg);
void _z_t_msg_copy_frame(_z_t_msg_frame_t *clone, _z_t_msg_frame_t *msg);

typedef union {
    _z_s_msg_scout_t _scout;
    _z_s_msg_hello_t _hello;
} _z_scouting_body_t;

typedef struct {
    _z_scouting_body_t _body;
    uint8_t _header;
} _z_scouting_message_t;
void _z_s_msg_clear(_z_scouting_message_t *msg);

_z_scouting_message_t _z_s_msg_make_scout(z_what_t what, _z_id_t zid);
_z_scouting_message_t _z_s_msg_make_hello(z_whatami_t whatami, _z_id_t zid, _z_locator_array_t locators);

void _z_s_msg_copy(_z_scouting_message_t *clone, _z_scouting_message_t *msg);
void _z_s_msg_copy_scout(_z_s_msg_scout_t *clone, _z_s_msg_scout_t *msg);
void _z_s_msg_copy_hello(_z_s_msg_hello_t *clone, _z_s_msg_hello_t *msg);

#endif /* ZENOH_PICO_PROTOCOL_MSG_H */
