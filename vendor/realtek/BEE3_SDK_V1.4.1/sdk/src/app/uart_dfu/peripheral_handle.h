/*****************************************************************************************
*     Copyright(c) 2020, Realtek Semiconductor Corporation. All rights reserved.
*****************************************************************************************
* @file    peripheral_handle.h
* @brief   This file contains all the constants and functions prototypes for peripheral handle.
* @details
* @author  mandy
* @date    2022-01-04
* @version v1.0
* *************************************************************************************
*/

#ifndef _PERIPHERAL_HANDLE_H_
#define _PERIPHERAL_HANDLE_H_

#ifdef __cplusplus
extern "C" {
#endif

/*============================================================================*
 *                              Header Files
 *============================================================================*/
#include <stdint.h>
#include <stdbool.h>
#include <board.h>
/*============================================================================*
 *                              Macro Definitions
 *============================================================================*/
/* Configure peripheral packet buffer length */
#define HEADER_SIZE                           5

/* Protocol header define */
#define PACKET_CMD_START_BYTE_VALUE           ((uint8_t)0x03)
#define PACKET_CMD_HIGH_OPCODE_VALUE          ((uint8_t)0x12)
#define PACKET_CMD_START_BYTE_LEN             ((uint8_t)0x01)
#define PACKET_CMD_OPCODE_LEN                 ((uint8_t)0x02)
#define PACKET_CMD_LENGTH_LEN                 ((uint8_t)0x02)
#define PACKET_CMD_CRC_DATA_LEN               ((uint16_t)0x02)

#define NO_ACTION_EXIT_DFU_TIMEOUT            10000  /* 10s */
/*============================================================================*
 *                              Types
 *============================================================================*/
typedef void *TimerHandle_t;

/* Packet decode status */
typedef enum
{
    PERIPHERAL_STATE_WAIT_HEADER,
    PERIPHERAL_STATE_WAIT_OPCODE,
    PERIPHERAL_STATE_WAIT_LENGTH,
    PERIPHERAL_STATE_WAIT_PARAMS,
    PERIPHERAL_STATE_WAIT_CRC,
} T_PERIPHERAL_STATE;

/* packet data structure */
typedef struct t_peripheral_packet_def
{
    uint8_t  header_buf[HEADER_SIZE];
    bool is_payload_len_error;
    uint16_t buf_index;
    uint16_t opcode;
    uint16_t payload_len;
    uint16_t crc_value;
    T_PERIPHERAL_STATE peripheral_status;
} T_PERIPHERAL_PACKET_DEF;

/*============================================================================*
*                           Export Global Variables
*============================================================================*/

/*============================================================================*
 *                          Functions
 *============================================================================*/
void peripheral_handle_allow_to_enter_dlps(void);
void peripheral_handle_disallow_to_enter_dlps(void);
bool peripheral_handle_check_dlps(void);
#if FEATURE_SUPPORT_NO_ACTION_EXIT_DFU
void peripheral_handle_timer_init(void);
void peripheral_handle_no_act_timer_restart(void);
void peripheral_handle_no_act_timer_stop(void);
#endif
void peripheral_handle_send_data(uint8_t *buf, uint32_t length);
void peripheral_handle_init_packet_struct(T_PERIPHERAL_PACKET_DEF *p_packet);
bool peripheral_handle_decode_packet(T_PERIPHERAL_PACKET_DEF *p_packet, uint8_t *payload_buf);

#ifdef __cplusplus
}
#endif

#endif
