/*****************************************************************************************
*     Copyright(c) 2022, Realtek Semiconductor Corporation. All rights reserved.
*****************************************************************************************
* @file    uart_driver.h
* @brief   This file contains all the constants and functions prototypes for uart driver.
* @details
* @author  mandy
* @date    2022-01-04
* @version v1.0
* *************************************************************************************
*/

#ifndef _UART_DRIVER_H_
#define _UART_DRIVER_H_

#ifdef __cplusplus
extern "C" {
#endif

/*============================================================================*
 *                        Header Files
 *============================================================================*/
#include <stdint.h>
#include <stdbool.h>
#include <rtl876x.h>
#include "loop_queue.h"
#include "board.h"
/*============================================================================*
 *                         Macros
 *============================================================================*/
#define UART_DFU                    UART0
#define UART_DFU_TX                 UART0_TX
#define UART_DFU_RX                 UART0_RX
#define UART_DFU_IRQn               UART0_IRQn
#define UART_DFU_APBPERIPH          APBPeriph_UART0
#define UART_DFU_APBPERIPH_CLK      APBPeriph_UART0_CLOCK
#define UART_DFU_Handler            UART0_Handler

#define RECEIVE_BUF_MAX_LENGTH      2500
#define UART_RX_TRIGGER_VALUE       4

/*============================================================================*
 *                         Types
 *============================================================================*/
/**
 * @brief  UART global data struct definition.
 */
typedef struct
{
    bool is_allowed_to_enter_dlps;  /* to indicate whether to allow to enter dlps or not */
    uint32_t  baudrate;
} T_UART_GLOBAL_DATA;

/*============================================================================*
*                        Export Global Variables
*============================================================================*/
extern T_UART_GLOBAL_DATA uart_global_data;
extern T_LOOP_QUEUE_DEF *p_uart_queue;
/*============================================================================*
 *                         Functions
 *============================================================================*/
void uart_disable_wakeup_config(void);
void uart_pinmux_config(void);
void uart_init_data(void);
void uart_nvic_config(void);
void uart_init_driver(void);
void uart_init_pad_config(void);
void uart_enter_dlps_config(void);
void uart_exit_dlps_config(void);
void uart_send_data(uint8_t *buf, uint32_t length);

#ifdef __cplusplus
}
#endif

#endif

