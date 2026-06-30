#ifndef MIBLE_H_
#define MIBLE_H_
#include <zephyr/types.h>
#include <stdbool.h>
#include <stdint.h>

#include <bluetooth/bluetooth.h>
#include "mible_dfu.h"
#include "mible_spec.h"

/* The following code defines three constants to represent the major, minor, and revision numbers of
 * the MIBLE library.
 */
#if defined(CONFIG_MIBLE_PSK) && (CONFIG_MIBLE_PSK)
#include "mible_psk_auth.h"

#define MIBLE_LIB_MAJOR                 3 // Represents the major version number.
#define MIBLE_LIB_MINOR                 0 // Represents the minor version number.
#define MIBLE_LIB_REVISION              0 // Represents the revision number.

#define MIBLE_AUTH_MODE                 3 // 2: Standard auth, 3: Psk auth
#else
#include "mesh_vnd_lpn.h"

#define MIBLE_LIB_MAJOR                 3 // Represents the major version number.
#define MIBLE_LIB_MINOR                 0 // Represents the minor version number.
#define MIBLE_LIB_REVISION              4 // Represents the revision number.
#endif

#define STR_VAL(str)                    #str
#define CONCAT_DEVELOPER_VERSION(x)     STR_VAL(x)
#define CONCAT_LIB_VERSION(x, y, z)     STR_VAL(x) "." STR_VAL(y) "." STR_VAL(z) "_"
#define CONCAT_VERSION(x, y, z, u)      STR_VAL(x) "." STR_VAL(y) "." STR_VAL(z) "_" STR_VAL(u)

/**< The UUID of the Xiaomi Service. */
#define BLE_UUID_MI_SERVICE             0xFE95
/**< The UUID of the Mi Service Version Characteristic. */
#define BLE_UUID_MI_VERS                0x0004
/**< The UUID of the WiFi Status Characteristic. */
#define BLE_UUID_MI_WIFI_CFG            0x0005
/**< The UUID of the Control Point Characteristic. */
#define BLE_UUID_MI_CTRLP               0x0010
/**< The UUID of the Secure Auth Characteristic. */
#define BLE_UUID_MI_SECURE              0x0016
/**< The UUID of the dfu Control Point Characteristic. */
#define BLE_UUID_MI_DFU_CTRL            0x0017
/**< The UUID of the dfu Data Characteristic. */
#define BLE_UUID_MI_DFU_DATA            0x0018
/**< The UUID of the Standard Auth Characteristic. */
#define BLE_UUID_MI_STANDARD            0x0019
/**< The UUID of the MIOT Spec RX Characteristic. */
#define BLE_UUID_MI_SPEC_RD             0x001A
/**< The UUID of the MIOT Spec TX Characteristic. */
#define BLE_UUID_MI_SPEC_WR             0x001B
/**< The UUID of the System info Characteristic. */
#define BLE_UUID_MI_SYS_INFO            0x001C

/* ATT MTU must be equal for RX and TX, so select the smallest value */
/* DMTU MIN(BT_L2CAP_TX_MTU, BT_L2CAP_TX_MTU) - ATT Header(3) - SN(2) - L2CAP Header(4, for Huawei Compatible) */
#define DMTU                            (MIN(CONFIG_BT_BUF_ACL_TX_SIZE - 4, CONFIG_BT_BUF_ACL_RX_SIZE - 4)  - 3 - 2 - 4)
#define MAX_CONSECTIVE_INTERVAL         400
#define MAX_RETRY_CNT                   (6)
#define MAX_LOST_RETRY_CNT              (3)
/*  SDU = EVT(255) - Field(26) - (Header(2) + Mode(1) + ExtHead(3) + CRC(3) + Resv(4)) 
        - (TypeLen(2) + NetHead(9) + NetMIC(4) + TransHead(1))
    DMTU = SDU - VendorOP(3) - TransMIC(4) - Tid/Handle(3) - PacketNum(2) */
#define DMTU_EXTADV                     ((CONFIG_BT_BUF_EVT_DISCARDABLE_SIZE - 39 - 16) - 3 - 4 - 3 - 2)
#define DMTU_MAX                        (MAX(DMTU, DMTU_EXTADV))
#define MAX_CONSECTIVE_INTERVAL_EXTADV  3000
#define NET_BUF_EXT_LEN                 2 //tid + handle

#define PASS_THROUGH                    0x00
#define DEV_CERT                        0x01
#define DEV_MANU_CERT                   0x02
#define ECC_PUBKEY                      0x03
#define DEV_SIGNATURE                   0x04
#define DEV_LOGIN_INFO                  0x05
#define DEV_SHARE_INFO                  0x06
#define SERVER_CERT                     0x07
#define SERVER_SIGN                     0x08
#define MESH_CONFIG                     0x09
#define APP_CONFIRMATION                0x0A
#define APP_RANDOM                      0x0B
#define DEV_CONFIRMATION                0x0C
#define DEV_RANDOM                      0x0D
#define BIND_KEY                        0x0E
#define WIFI_CONFIG                     0x0F

#define STATE_NONE                      0x00
#define STATE_WAIT_CMD                  0x01
#define STATE_WAIT_ACK                  0x02
#define STATE_TXD                       0x03
#define STATE_RXD                       0x04
#define STATE_ERROR                     0xFF

#define SEG_CMD                         0
#define SEG_ACK                         1
#define SGL_CMD                         2
#define SGL_ACK                         3
#define MNG_CMD                         4
#define MNG_ACK                         5
#define NIL_TYPE                        0xFF
#define FRAME_CTRL                      0

#define A_SUCCESS                       0
#define A_READY                         1
#define A_BUSY                          2
#define A_TIMEOUT                       3
#define A_CANCEL                        4
#define A_LOST                          5

#define M_FEATURE                       0
#define M_LENGTH                        1

struct rxfer_cb {
    void (*sent)(int err, uint8_t cmd, void *user_data);
    void (*recv)(int err, uint8_t cmd, uint8_t *val, uint16_t len, void *user_data);
    void (*mng)(int err, uint8_t cmd, const uint8_t val[]);
};

struct k_work_vendor{
    void *user_data;
    struct k_work work;
};

struct k_work_delayable_vendor{
    void *user_data;
    struct k_work_delayable work_delay;
};

enum {
    MIBLE_STATUS_BINDED,
    MIBLE_STATUS_LOGINED,
    MIBLE_STATUS_ADV_DISABLED,
    MIBLE_STATUS_BIND_CONFIRMED,
};

enum {
    MIBLE_TASK_USED,
};

typedef enum {
    MI_SUCCESS = 0x00,
    MI_ERR_INTERNAL,
    MI_ERR_NOT_FOUND,
    MI_ERR_NO_EVENT,
    MI_ERR_NO_MEM,
    MI_ERR_INVALID_ADDR,  // Invalid pointer supplied
    MI_ERR_INVALID_PARAM, // Invalid parameter(s) supplied.
    MI_ERR_INVALID_STATE, // Invalid state to perform operation.
    MI_ERR_INVALID_LENGTH,
    MI_ERR_DATA_SIZE,
    MI_ERR_TIMEOUT,
    MI_ERR_BUSY,
    MI_ERR_RESOURCES,
    MIBLE_ERR_INVALID_CONN_HANDLE,
    MIBLE_ERR_ATT_INVALID_ATT_HANDLE,
    MIBLE_ERR_GAP_INVALID_BLE_ADDR,
    MIBLE_ERR_GATT_INVALID_ATT_TYPE,
    MIBLE_ERR_UNKNOWN, // other ble stack errors
} mible_status_t;

typedef enum {
    MIBLE_DEV_INFO_SUPPORT = 0x00,
    MIBLE_DEV_INFO_MCU_VERSION,
    MIBLE_DEV_INFO_DEVICE_SN, // Max 18 byte
    MIBLE_DEV_INFO_HARDWARE_VERSION,
    MIBLE_DEV_INFO_LATITUDE,
    MIBLE_DEV_INFO_LONGITUDE,
    MIBLE_DEV_INFO_VENDOR1,
    MIBLE_DEV_INFO_VENDOR2,
    MIBLE_DEV_INFO_NEW_SN, // Max 64 byte
} mible_dev_info_type_t;

typedef struct {
    int code;
    uint8_t *buff;
    uint8_t len;
} mible_dev_info_t;

typedef void (*mible_dev_info_cb_t)(mible_dev_info_type_t type, mible_dev_info_t *buf);

typedef enum {
    MIBLE_USER_PROV_CB_TYPE_UNPROV = 0,
    MIBLE_USER_PROV_CB_TYPE_PROVED,
    MIBLE_USER_PROV_CB_TYPE_START,
    MIBLE_USER_PROV_CB_TYPE_COMPLETE,
    MIBLE_USER_PROV_CB_TYPE_FAIL,
    MIBLE_USER_PROV_CB_TYPE_RESET,
    MIBLE_USER_PROV_CB_TYPE_RECOVERY,
    MIBLE_USER_LOGIN_CB_TYPE_START,
    MIBLE_USER_LOGIN_CB_TYPE_COMPLETE,
    MIBLE_USER_LOGIN_CB_TYPE_FAIL,
    MIBLE_USER_GAP_CB_CONNECTED,
    MIBLE_USER_GAP_CB_DISCONNECTED,
    MIBLE_USER_SCENE_CB_ADD,
    MIBLE_USER_SCENE_CB_DELETE,
    MIBLE_USER_SCENE_CB_RECALL,
    MIBLE_USER_MIBEACON_SEND,
    MIBLE_USER_OOB_GENERATE,
    MIBLE_USER_OOB_INPUT,
} mible_user_event_type_t;

typedef void (*mible_user_event_cb_t)(mible_user_event_type_t event, void *user_data);

typedef bool (*mible_dfu_low_power_req_cb_t)(void);
typedef bool (*mible_dfu_pre_check_req_cb_t)(void);
typedef void (*mible_dfu_event_cb_t)(mible_dfu_state_t state, const mible_dfu_param_t *param);

typedef int (*mible_config_init_t)(void);

struct mible_config {
    uint32_t magic;
    struct {
        uint32_t pid;
        uint8_t model_name[BT_GAP_ADV_MAX_ADV_DATA_LEN - 2 + sizeof((char)'\0')];
        uint16_t dev_version;
        uint8_t *mcu_version;
        uint16_t io;
        uint8_t have_reset_button;
        uint8_t have_confirm_button;

        struct {
            mible_config_init_t gatt_spec_init;
            mible_config_init_t psk_init;
            mible_config_init_t path_init;
            mible_config_init_t lpn_init;
            mible_config_init_t mcu_ota_init;
        } init;
        struct {
            void *rx_buf;
            uint16_t max_rx_buf_len;

            uint16_t max_tx_buf_len;

            uint16_t max_tx_pool_num;
            struct mible_gatt_spec_tx *tx_pool;
        } gatt_spec;
    } cfg;

    struct {
        struct {
            mible_user_event_cb_t event;
        } event;

        struct {
            mible_dev_info_cb_t req;
        } dev_info;

        struct {
            mible_dfu_event_cb_t event;
            mible_dfu_pre_check_req_cb_t pre_check_req;
            mible_dfu_low_power_req_cb_t low_power_req;
        } dfu_info;

        struct {
            mible_property_operation_cb_t get;
            mible_property_operation_cb_t set;
            mible_action_operation_cb_t invoke;
        } spec;
#if defined(CONFIG_BT_PER_ADV_SYNC)
        struct {
            sync_callback_t sync;
        } pa_lpn;
#endif
    } cb;
};

typedef void (*mible_handler_t)(void *arg);

struct mible_task_item {
    sys_snode_t node;

    atomic_t flags;

    mible_handler_t handler;
    void *arg;
};


int mible_service_init(void);

int mible_otp_init(void);
int mible_record_delete(uint16_t record_id);
int mible_record_read(uint16_t record_id, uint8_t *p_data, uint16_t len);
int mible_record_read_ext(uint16_t record_id, uint8_t *p_data, uint16_t len, uint16_t *rd_len);
int mible_record_write(uint16_t record_id, const uint8_t *p_data, uint16_t len);

uint16_t mible_get_conn_handle(void);
mible_status_t mible_gap_disconnect(uint16_t conn_handle);

void mible_rxfer_reset(void);
void set_slave_mtu_intvl(uint16_t mtu, uint16_t intvl);
uint16_t get_exchanged_mtu(void);

void mible_single_task_post(struct mible_task_item *item, mible_handler_t handler, void *arg);
void evt_run_callback(uint8_t event, void *user_data);
void defer_task_process(void *args);
void mible_events_process(void);
void mible_core_enable_mcu_get_info(void);
void mible_mesh_enable_mcu_get_info(void);
#endif /* MIBLE_H_ */
