/**
*********************************************************************************************************
*               Copyright(c) 2018, Realtek Semiconductor Corporation. All rights reserved.
*********************************************************************************************************
* @file     io_lpc.h
* @brief
* @details
* @author   yuan
* @date     2018-12-07
* @version  v1.0
*********************************************************************************************************
*/

#ifndef __IO_LPC_H
#define __IO_LPC_H

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include "rtl876x_lpc.h"
#include "rtl876x_nvic.h"
#include "rtl876x_pinmux.h"
#include "rtl876x_rcc.h"
#include "trace.h"


/* Defines ------------------------------------------------------------------*/
/** LPC voltage detect channel config.
  * PIN select:P2_2 P2_3 P2_6 and VBAT.
  * LPC channel select: #define LPC_CHANNEL_P2_2                ((uint32_t)0)
  *                     #define LPC_CHANNEL_P2_3                ((uint32_t)1)
  *                     #define LPC_CHANNEL_P2_6                ((uint32_t)2)
  *                     #define LPC_CHANNEL_VBAT                ((uint32_t)3)
  * If LPC_CAPTURE_PIN = P2_2, then LPC_CAPTURE_CHANNEL = LPC_CHANNEL_P2_2;
  */
#define LPC_CAPTURE_PIN                 P2_2
#define LPC_CAPTURE_CHANNEL             LPC_CHANNEL_P2_2

#define LPC_VOLTAGE_DETECT_EDGE         LPC_Vin_Below_Vth
#define LPC_VOLTAGE_DETECT_THRESHOLD    LPC_2000_mV

void board_lpc_init(void);
void driver_lpc_init(void);
void nvic_lpc_init(void);


#ifdef __cplusplus
}
#endif

#endif

