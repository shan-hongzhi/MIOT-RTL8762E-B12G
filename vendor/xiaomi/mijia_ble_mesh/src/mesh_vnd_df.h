#ifndef __MESH_VND_DF_H
#define __MESH_VND_DF_H

#include <bluetooth/mesh.h>

// TODO: rename or redefine those opcodes
#define ALL_DF_NODES           0xFEDF

#define DF_REQ  0x00DA
#define DF_RSP  0x00DB
#define DF_CFM  0x00DC
#define DF_DEL  0x00DD
#define DF_DCFM 0x00DE

enum path_status {
    PATH_TO_TARGET_FOUND,
    PATH_TO_ORIGIN_FOUND,
    PATH_DISCOVER_END,
    PATH_EXPIRED,
    PATH_TARGET_EXPIRED,

    // DEBUG ONLY
    NODE_SELECTED_AS_PATH,
    NODE_PATH_REVOKE,
    GROUP_PATH_TO_TARGET_FOUND,     //fn -> uni_addr[0], lane_id uni_addr[1]
    GROUP_PATH_TO_ORIGIN_FOUND,
};

struct path_param {
    uint8_t path_life        :2;    // lifetime = ( path_life + 1 ) * 15 min
    uint8_t path_rssi        :2;    // rssi_thershold = -80 + path_rssi * 5
    uint8_t nettx_double     :1;
    uint8_t reserved         :3;
};

typedef void (*path_callback_t)(enum path_status, uint32_t path, uint8_t fn, uint8_t lane_id, uint8_t result);

int vnd_miot_path_request(struct bt_mesh_model *model,
        struct bt_mesh_msg_ctx *ctx,
        struct net_buf_simple *buf);

int vnd_miot_path_reply(struct bt_mesh_model *model,
        struct bt_mesh_msg_ctx *ctx,
        struct net_buf_simple *buf);

int vnd_miot_path_confirm(struct bt_mesh_model *model,
        struct bt_mesh_msg_ctx *ctx,
        struct net_buf_simple *buf);

int vnd_miot_path_delete(struct bt_mesh_model *model,
        struct bt_mesh_msg_ctx *ctx,
        struct net_buf_simple *buf);

int vnd_miot_path_delcfm(struct bt_mesh_model *model,
        struct bt_mesh_msg_ctx *ctx,
        struct net_buf_simple *buf);

int mesh_path_enable(path_callback_t cb);

int discover_path_to_target(struct path_param *param, uint16_t target_addr, uint8_t fn, uint8_t lane_id);

int is_path_msg(struct bt_mesh_msg_ctx *ctx);

int is_not_relayable_path_msg(struct bt_mesh_msg_ctx *ctx);

int bt_mesh_path_relay_retransmit_get(struct bt_mesh_msg_ctx *ctx);

int del_one_path(uint16_t path_origin, uint16_t path_target, uint16_t delays, void **path_find);

void del_all_path(void);

#endif
