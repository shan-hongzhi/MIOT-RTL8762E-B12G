/*
 * Copyright (c) 2024 Xiaomi Corporation
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef MIBLE_TRACE_H_
#define MIBLE_TRACE_H_

#include <zephyr/types.h>
#include <stdbool.h>
#include <stdint.h>

#include "mible_mesh.h"

/*
* Trace points
* call api with the definitions when need to report corresponding info
*/

#define TRACE_BT_MESH_POWERON           0

#define TRACE_BT_MESH_POWERON_IVU       1

#define TRACE_BT_MESH_IV_CHANGE         2

#define TRACE_BT_MESH_IV_STAT           3
/* mesh rpl (5Byte * 32) */
#define TRACE_BT_MESH_RPL               4

#define TRACE_BT_MESH_RPL_RECENT        5

/* mesh mod_idx[2],appd_id(2) * n */
#define TRACE_BT_MESH_BIND_SUB          6

#define TRACE_BT_DEVICE_INFO            7

#define TRACE_BT_ADV_INFO               8

#define TRACE_BT_CONNECTED              9

#define TRACE_BT_DISCONNECTED           10

#define TRACE_BT_SCAN_UPDATE            11

#define TRACE_BT_SOFT_CNT1              12

#define TRACE_BT_SOFT_CNT2              13

#define TRACE_ROM_CRC                   14

#define TRACE_BT_NUM_MAX                15

/*
* Trace report levels
* Control what infomation to report to the gateway
*/

/* only report important short messages <= 8Byte */
#define TACEE_BT_REPORT_LEVEL_0     0

/* report short messages and expand broadcast messages */
#define TACEE_BT_REPORT_LEVEL_1     1

#define TACEE_BT_REPORT_LEVEL_MAX   2

#define TRACE_IV_UPDATE_FLAG(x)     ((x) & BIT(0))
#define TRACE_IV_RECOVERY_FLAG(x)   (((x) << 1) & BIT(1)) 

/**
 * @brief Report infomation to the gateway
 *
 * Trace how the device behave 
 * 
 * @param trace_point what to report
 *
 * @return response code.
 */
int mible_trace_report(uint16_t trace_point);

/** @brief init trace module.
 */
void mible_trace_init(void);

/** @brief Recive gateway control msg.
 *
 *  @param rv   Gateway msg.
 *
 * @return response code.
 */
int mible_trace_control(struct mible_mesh_msg_recv *rv);

int mible_trace_update_adv_param(struct bt_le_adv_param *param);

int mible_trace_update_scan_param(struct bt_hci_ext_scan_phy *param);

int mible_trace_update_user_adv_cnt(uint16_t cnt);

int mible_trace_update_reboot_reason(uint8_t reason);

int mible_trace_update_iv(uint8_t flag);

int mible_trace_clear_power_cnt();

int mible_trace_update_report_cnt();

#endif /* MIBLE_TRACE_H_ */
