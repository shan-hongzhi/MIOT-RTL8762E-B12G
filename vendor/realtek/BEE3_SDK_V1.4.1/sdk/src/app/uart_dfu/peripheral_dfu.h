/**
*********************************************************************************************************
*               Copyright(c) 2022, Realtek Semiconductor Corporation. All rights reserved.
*********************************************************************************************************
* @file    peripheral_dfu.h
* @brief   This file contains all the constants and functions prototypes for peripheral dfu.
* @details
* @author  mandy
* @date    2022-01-05
* @version v1.0
* *********************************************************************************************************
*/

#ifndef _PERIPHERAL_DFU_H_
#define _PERIPHERAL_DFU_H_

#ifdef __cplusplus
extern "C" {
#endif
/*============================================================================*
 *                              Header Files
 *============================================================================*/
#include <app_msg.h>
#include <profile_server.h>
#include "patch_header_check.h"
#include "dfu_api.h"
/*============================================================================*
 *                              Macro Definitions
 *============================================================================*/
#define SOCV_CFG    0
#define SYS_CFG     1
#define OTA_HEADER  2
#define SECURE_BOOT 3
#define ROM_PATCH   4
#define APP_IMG     5
#define APP_DATA1   6
#define APP_DATA2   7
#define APP_DATA3   8
#define APP_DATA4   9
#define APP_DATA5   10
#define APP_DATA6   11

#define IMAGE_NOEXIST         0
#define IMAGE_LOCATION_BANK0  1
#define IMAGE_LOCATION_BANK1  2
#define IMAGE_FIX_BANK_EXIST  3

#define PERIP_DFU_OPCODE_EVENT        0x04
#define PERIP_DFU_OPCODE_EVENT_HEADER 0x12

#define HW_AES_KEY 0
#define DFU_UPDATE_RETRY_MAX_NUM 5

//event payload length
#define PERIP_DFU_PAYLOAD_LEN_START_DFU             0x1
#define PERIP_DFU_PAYLOAD_LEN_WRITE_IMAGE           0x5
#define PERIP_DFU_PAYLOAD_LEN_VALID_IMAGE           0x1
#define PERIP_DFU_PAYLOAD_LEN_ACTIVE_IMAGE_RESET    0x1
#define PERIP_DFU_PAYLOAD_LEN_SYSTEM_RESET          0x1
#define PERIP_DFU_PAYLOAD_LEN_READ_DEVICE_INFO      0x9//sizeof(T_PERIP_DFU_DEVICE_INFO)
#define PERIP_DFU_PAYLOAD_LEN_READ_VERSION_INFO     44//The max value is (IMAGE_MAX - OTA) *sizeof(uint32_t)

#define PERIP_DFU_MAX_EVENT_PACKET_LEN              52//1+2+1+2+MAX_PAYLOAD_LEN+2
/*============================================================================*
 *                              Types
 *============================================================================*/
typedef enum
{
    PERIP_DFU_START_FALG          = 0x00,
    PERIP_DFU_STOP_FALG           = 0x01,
} T_PERIP_DFU_FLAG;

/*each control point procedure,can't modify exist value*/
typedef enum
{
    PERIP_DFU_OPCODE_MIN                   = 0x00, /*control point opcode min*/
    PERIP_DFU_OPCODE_START_DFU             = 0x01,
    PERIP_DFU_OPCODE_WRITE_IMAGE           = 0x02,
    PERIP_DFU_OPCODE_VALID_IMAGE           = 0x03,
    PERIP_DFU_OPCODE_ACTIVE_IMAGE_RESET    = 0x04,
    PERIP_DFU_OPCODE_SYSTEM_RESET          = 0x05,
    PERIP_DFU_OPCODE_READ_DEVICE_INFO      = 0x06,
    PERIP_DFU_OPCODE_READ_VERSION_INFO     = 0x07,
    PERIP_DFU_OPCODE_MAX                   = 0x08, /*control point opcode max*/
} T_PERIP_DFU_CP_OPCODE;

typedef enum
{
    PERIP_DFU_PAYLOAD_SUCCESS              = 0x00,
    PERIP_DFU_PAYLOAD_IC_TYPE_ERROR        = 0x01,
    PERIP_DFU_PAYLOAD_IMAGE_ID_ERROR       = 0x02,
    PERIP_DFU_PAYLOAD_LENGTH_ERROR         = 0x03,
    PERIP_DFU_PAYLOAD_UPDATE_ERROR         = 0x04,
    PERIP_DFU_PAYLOAD_CRC_CHECK_ERROR      = 0x05,
} T_PERIP_DFU_PAYLOAD_STATUS;

typedef enum
{
    PERIP_DFU_FRAME_NO_ERROR               = 0x00,
    PERIP_DFU_FRAME_OPCODE_ERROR           = 0x01,
    PERIP_DFU_FRAME_LENGTH_ERROR           = 0x02,
    PERIP_DFU_FRAME_CRC_ERROR              = 0x03,
} T_PERIP_DFU_FRAME_ERROR_CODE;

typedef struct
{
    uint8_t image_num;
    uint16_t image_exist;
    uint32_t image_indicator;
} T_PERIP_DFU_ACTIVE_BANK_IMG_INFO;

typedef struct _T_PERIP_DFU_DEVICE_INFO
{
    uint8_t ic_type;
    uint8_t ota_version;
    uint8_t secure_version;
    T_OTA_MODE ota_mode;

    uint16_t max_buffer_size;
    uint8_t temp_bank_size; //Unit:4K, 0x00:No Limitation of OTA Temp Buffer Size,Only available when Updating Multi Image at a time  is supported.
    uint8_t rsvd;
    uint32_t img_indicator;
} T_PERIP_DFU_DEVICE_INFO;

typedef struct
{
    T_IMG_ID image_id;
    uint32_t image_size;
    uint32_t image_offset;
} T_PERIP_DFU_TEMP_IMAGE_INFO;

typedef struct _T_PERIP_DFU_CTRL_POINT
{
    uint8_t opcode;
    T_IMG_CTRL_HEADER_FORMAT start_dfu;
} T_PERIP_DFU_CTRL_POINT, * P_PERIP_DFU_CTRL_POINT;

typedef struct
{
    T_IMG_CTRL_HEADER_FORMAT ctrl_header;
    uint32_t image_total_length;
    uint32_t origin_image_version;
    uint32_t cur_offset;
    uint8_t mtu_size;
    uint8_t dfu_update_retry_count;
} T_PERIP_DFU_PARA;
/*============================================================================*
*                           Export Global Variables
*============================================================================*/

/*============================================================================*
 *                          Functions
 *============================================================================*/
void peripheral_dfu_deinit(uint8_t dfu_stop_flag);
void peripheral_dfu_handle(void);

#ifdef __cplusplus
}
#endif

#endif

