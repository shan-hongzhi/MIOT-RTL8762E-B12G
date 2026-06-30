#ifndef MIBLE_DFU_H_
#define MIBLE_DFU_H_

#include <zephyr/types.h>
#include <stdbool.h>
#include <stdint.h>

#define RECORD_ID_DFU_INFO                      0x05

#define MIBLE_DFU_STATUS_SUCC                   0x00
#define MIBLE_DFU_STATUS_ERR_NO_CMD             0x01
#define MIBLE_DFU_STATUS_ERR_AUTH_FAIL          0x02
#define MIBLE_DFU_STATUS_ERR_INVALID            0x03
#define MIBLE_DFU_STATUS_ERR_NO_MEM             0x04
#define MIBLE_DFU_STATUS_ERR_BUSY               0x05
#define MIBLE_DFU_STATUS_ERR_UNSIGNED           0x06
#define MIBLE_DFU_STATUS_ERR_RX_FAIL            0x07
#define MIBLE_DFU_STATUS_ERR_LOW_BATTERY        0x08
#define MIBLE_DFU_STATUS_ERR_INVALID_VERSION    0x09
#define MIBLE_DFU_STATUS_ERR_UNKNOWN            0xFF

typedef enum {
    MIBLE_DFU_STATE_START,
    MIBLE_DFU_STATE_UPGRADE_STATUS,
    MIBLE_DFU_STATE_TRANSFER,
    MIBLE_DFU_STATE_VERIFY,
    MIBLE_DFU_STATE_SWITCH,
    MIBLE_DFU_STATE_TRANSFER_MCU,
    MIBLE_DFU_STATE_VERIFY_MCU,
    MIBLE_DFU_STATE_SWITCH_MCU,
    MIBLE_DFU_STATE_PROCESS,
    MIBLE_DFU_STATE_CANCEL,
} mible_dfu_state_t;

typedef struct {
    uint8_t major;
    uint8_t minor;
    uint8_t revision;
    uint16_t build;
}__packed mible_dfu_ver_t;

typedef struct {
    uint16_t fragment_size;
} mible_dfu_start_param_t;

typedef struct {
    uint16_t last_index;
} mible_dfu_trans_param_t;

typedef struct {
    uint8_t value;
    mible_dfu_ver_t version;
} mible_dfu_verify_param_t;

typedef struct {
    uint8_t up_to_app;
} mible_dfu_switch_param_t;

typedef struct {
    union {
        struct {
            uint8_t install     :1;
            uint8_t silent      :1;
            uint8_t reserved    :6;
        }_req;
        uint8_t  req;
    };

    union {
        struct {
            uint8_t install     :1;
            uint8_t silent      :1;
            uint8_t reserved    :6;
        }_rsp;
        uint8_t  rsp;
    };
} mible_dfu_upgrade_status_param_t;

typedef union {
    mible_dfu_start_param_t start;
    mible_dfu_trans_param_t trans;
    mible_dfu_verify_param_t verify;
    mible_dfu_switch_param_t switch_plan;
    mible_dfu_upgrade_status_param_t upgrade_status;
} mible_dfu_param_t;

typedef struct {
    uint16_t last_index;
    uint16_t rfu;
    uint32_t crc32;
    uint32_t recv_bytes;
} __packed mible_dfu_info_t;

void mible_dfu_init(void);
void set_dfu_switch_flag(void);
uint8_t mible_dfu_verify_process(void);
int dfu_info_check(uint16_t frag_idx, uint32_t crc32);
void dfu_try_switch_process(void *args);
int mcu_dfu_info_check(uint16_t frag_idx, uint32_t crc32);
int mcu_dfu_try_activate_new_fw(void);
int mible_dfu_mcu_ota_enable(void);
#endif //MIBLE_DFU_H_
