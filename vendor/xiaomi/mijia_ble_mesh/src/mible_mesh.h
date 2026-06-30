#ifndef MIBLE_MESH_H_
#define MIBLE_MESH_H_

#include <zephyr/types.h>
#include <stdbool.h>
#include <stdint.h>
#include <sys/atomic.h>
#include <bluetooth/bluetooth.h>

#include "mible.h"

#define RECORD_ID_KEY_INFO                          0x10
#define RECORD_ID_REC_INFO                          0x11
#define RECORD_ID_MESH_BASE                         0x20

#define MIBLE_MESH_DEVICE_GROUP                     0xFEFE
#define MIBLE_MESH_GATEWAY_GROUP                    0xFEFF
#define MIBLE_MESH_COMPANY_ID_SIG                   0xFFFF
#define MIBLE_MESH_COMPANY_ID_XIAOMI                0x038F

/*SIG Generic model ID */
#define MIBLE_MESH_MODEL_ID_CONFIGURATION_SERVER    0x0000
#define MIBLE_MESH_MODEL_ID_GENERIC_ONOFF_SERVER    0x1000
#define MIBLE_MESH_MODEL_ID_LIGHTNESS_SERVER        0x1300
#define MIBLE_MESH_MODEL_ID_CTL_TEMPEATURE_SERVER   0x1306

/** Mesh vendor models **/
#define MIBLE_MESH_MIOT_SPEC_SERVER_MODEL           0000 // 0x038f0000
#define MIBLE_MESH_MIOT_SPEC_CLIENT_MODEL           0001 // 0x038f0001
#define MIBLE_MESH_MIJIA_SERVER_MODEL               0002 // 0x038f0002
#define MIBLE_MESH_MIJIA_CLIENT_MODEL               0003 // 0x038f0003

#define MI_MESH_STATE_UNREG                         0
#define MI_MESH_STATE_UNPROV                        1
#define MI_MESH_STATE_UNCONFIG                      2
#define MI_MESH_STATE_AVAIL                         3

typedef struct {
    uint32_t scene_id;
    uint8_t  pos;
    uint8_t  num;
    uint16_t delay;
    uint8_t  siid;
    uint8_t  piid;
    uint8_t  value[4];
} mible_mesh_scene_store_t;

typedef struct {
    uint32_t scene_id;
    uint8_t  pos;
} mible_mesh_scene_delete_t;

typedef struct {
    uint32_t scene_id;
    uint8_t pos;
    uint8_t rsv;
    uint16_t delay;
} mible_mesh_scene_recall_t;

struct mible_mesh_model_spec {
    uint8_t siid;
    uint8_t piid;
    uint8_t elem;
    uint8_t vnd;
    struct bt_mesh_model *mod;
};

struct mible_mesh_msg_sent_ctx {
    uint8_t retry_times;    //mesh 2.0: dfu msg retry
    uint8_t sub_opcode;     //mesh 2.0: dfu sub opcode
    uint8_t mod_idx;
    uint16_t opcode;
    uint16_t dst;
    uint64_t delay;
};

struct mible_mesh_msg_recv {
    sys_snode_t node;

    uint16_t mod_idx;
    uint16_t opcode;

    struct bt_mesh_msg_ctx ctx;

    // uint8_t data[BT_MESH_RX_SDU_MAX];
    uint8_t *pdata;
    uint8_t data[15];       //#define BT_MESH_APP_UNSEG_SDU_MAX 15
    uint8_t data_len;
    uint8_t sub_opcode;     //mesh 2.0: dfu sub opcode
};

extern atomic_t mible_status;

int mible_mesh_start(void);
int mible_mesh_restore(bool is_manu);
int mible_adv_start(void);
int mible_adv_resume(void);
int mible_adv_suspend(void);
int mibeacon_mesh_adv_data_init(uint8_t mesh_stat);
int mible_mesh_key_restore(void);
int mible_psk_auth_enable(void);
int mible_path_ota_enable(void);
int mible_pa_lpn_enable(void);

int mible_mesh_send_property_changed(uint16_t siid, uint16_t piid, mible_spec_property_value_t *newValue);
int mible_mesh_send_property_report(uint16_t siid, uint16_t piid, mible_spec_property_value_t *newValue,
                                    uint32_t delay_ms, bool retrans);
int mible_mesh_send_property_request(uint8_t siid, uint8_t piid);
int mible_mesh_send_property_indication(uint16_t siid, uint16_t piid, mible_spec_property_value_t *newValue,
                                uint32_t retry_intvl, uint8_t retry_cnt);
int mible_mesh_send_event_occurred(uint16_t siid, uint16_t eiid, mible_spec_arguments_t *newArgs);
int mible_mesh_send_event_report(uint16_t siid, uint16_t eiid, mible_spec_arguments_t *newArgs,
                                uint32_t delay_ms, bool retrans);
int mible_mesh_pub_add(uint8_t siid, uint8_t piid, uint32_t period);
int mod_sub_groups(uint16_t *sub_addr, uint8_t sub_cnt);

int mible_mesh_device_scan_set(uint8_t level);
uint8_t mible_mesh_device_scan_get(void);

uint16_t get_path_origin(void);
int mi_scene_record_del(uint16_t scene_id, uint8_t siid, uint8_t piid);
int mi_scene_record_add(uint16_t scene_id, uint8_t siid, uint8_t piid, uint8_t *val);

int mible_rec_info_store(void);

int mible_mesh_msg_send(struct mible_mesh_msg_sent_ctx *ctx, struct net_buf_simple *msg);

void mibeacon_mesh_pa_adv_data_update(uint8_t pa_stat);
#endif //MIBLE_MESH_H_