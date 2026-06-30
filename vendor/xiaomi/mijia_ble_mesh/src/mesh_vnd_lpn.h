#ifndef __MESH_VND_LPN_H
#define __MESH_VND_LPN_H

#include <bluetooth/mesh.h>

#define PA_REQ  0x00D7
#define PA_RSP  0x00D8
#define PA_CFM  0x00D9

enum sync_status {
    SYNC_ESTABLISH,
    SYNC_LOST,
    SYNC_FAILED,
};

typedef struct {
    uint8_t req_node_feature_map :1;
    uint8_t req_gw_feature_map   :1;
    uint8_t rfu:6;
} __packed feature_map_req_t;

typedef struct {
    uint8_t bitmap[7];
} __packed feature_map_rsp_t;

typedef struct {
    uint8_t sid;
    uint8_t addr[6];
}__packed per_adv_sync_req_t;

typedef struct {
    uint8_t result;
}__packed per_adv_sync_rsp_t;

typedef struct{
    uint16_t net_idx;
    uint16_t app_idx;
    uint16_t addr;
}__packed per_adv_sync_gw_info_t;

typedef enum {
    PER_ADV_SYNC_IDLE,
    PER_ADV_SYNC_INIT,
    PER_ADV_SYNC_REQ_GW_GROUP_ADDR,
    PER_ADV_SYNC_SELECT_GW_NODE,
    PER_ADV_SYNC_GW_NODE_FIND_NONE,
    PER_ADV_SYNC_REQ_GW_UNICAST_ADDR,
    PER_ADV_SYNC_START,
    PER_ADV_SYNC_FAIL,
    PER_ADV_SYNC_ESTABLISH,
    PER_ADV_SYNC_LOST,
    PER_ADV_SYNC_SUCCESS,
    PER_ADV_SYNC_STOP
}per_adv_sync_dev_state_t;

typedef void (*sync_callback_t)(per_adv_sync_dev_state_t status, uint16_t node_addr, void * data);
int mesh_vnd_per_adv_respond(struct bt_mesh_model *model,
        struct bt_mesh_msg_ctx *ctx,
        struct net_buf_simple *buf);

int mesh_vnd_per_adv_sync_stop(void);
int mesh_vnd_per_adv_sync_recv_enable(bool enable);


// GATEWAY SIDE
int mesh_vnd_per_adv_enable(sync_callback_t cb);

// NODE SIDE
int mesh_vnd_per_adv_sync_enable(sync_callback_t cb);

int mesh_vnd_per_adv_sync_request(void);

void mesh_vnd_per_adv_set_gw_info(uint16_t gw_addr, uint16_t net_idx, uint16_t app_idx);
void mesh_vnd_per_adv_get_gw_info(per_adv_sync_gw_info_t* gw_info);
uint8_t mesh_vnd_is_per_adv_running(void);
#endif
