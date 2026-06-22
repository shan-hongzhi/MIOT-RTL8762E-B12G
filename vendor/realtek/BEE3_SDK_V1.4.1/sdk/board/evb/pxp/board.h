/**
*********************************************************************************************************
*               Copyright(c) 2015, Realtek Semiconductor Corporation. All rights reserved.
*********************************************************************************************************
* @file      board.h
* @brief     header file of Keypad demo.
* @details
* @author    tifnan_ge
* @date      2015-06-26
* @version   v0.1
* *********************************************************************************************************
*/


#ifndef _BOARD_H_
#define _BOARD_H_

#ifdef __cplusplus
extern "C" {
#endif

#include "otp_config.h"
#include "rtl876x_pinmux.h"

/** @defgroup IO Driver Config
  * @note user must config it firstly!! Do not change macro names!!
  * @{
  */
#define DEVICE_NAME           'B', 'L', 'B', '_', 'P', 'R', 'O', 'X'
#define DEVICE_NAME_LEN       (8+1)  /* sizeof(DEVICE_NAME) + 1 */

#define DLPS_EN              1

#define EVB_87x2CJ_1BIT      0
#define EVB_87x2CK           1
#define EVB_87x2CJF          0

#if EVB_87x2CJ_1BIT
#define LED      P1_3       //LED2 EVB QFN40 FLASH 1bit
#define BEEP     P1_4       //LED3 EVB QFN40 FLASH 1bit
#else
#define LED      P0_1       //LED0 EVB QFN48
#define BEEP     P0_2       //LED1 EVB QFN48
#endif

#define KEY      P2_4       //KEY2 EVB QFN48&QFN40
#define KEY_IRQ    GPIO20_IRQn
#define KEY_INT_Handle  GPIO20_Handler

/* if use user define dlps enter/dlps exit callback function */
#define USE_USER_DEFINE_DLPS_ENTER_CB       1
#define USE_USER_DEFINE_DLPS_EXIT_CB        1

/* if use any peripherals below, #define it 1 */
#define USE_ADC_DLPS                0
#define USE_CTC_DLPS                0
#define USE_GPIO_DLPS               1
#define USE_I2C0_DLPS               0
#define USE_I2C1_DLPS               0
#if (ROM_WATCH_DOG_ENABLE == 1)
#define USE_TIM_DLPS                1 //must be 1 if enable watch dog
#else
#define USE_TIM_DLPS                0
#endif
#define USE_IR_DLPS                 0
#define USE_KEYSCAN_DLPS            0
#define USE_QDECODER_DLPS           0
#define USE_SPI0_DLPS               0
#define USE_SPI1_DLPS               0
#define USE_SPI2W_DLPS              0
#define USE_UART0_DLPS              0
#define USE_UART1_DLPS              0
#define USE_I2S0_DLPS               0
#define USE_ENHTIM_DLPS             0
#define USE_CODEC_DLPS              0

/* do not modify USE_IO_DRIVER_DLPS macro */
#define USE_IO_DRIVER_DLPS  (USE_I2C0_DLPS | USE_I2C1_DLPS | USE_TIM_DLPS | USE_QDECODER_DLPS\
                             | USE_IR_DLPS | USE_ADC_DLPS | USE_CTC_DLPS | USE_SPI0_DLPS\
                             | USE_SPI1_DLPS | USE_SPI2W_DLPS | USE_KEYSCAN_DLPS\
                             | USE_GPIO_DLPS | USE_CODEC_DLPS | USE_I2S0_DLPS\
                             | USE_ENHTIM_DLPS | USE_UART0_DLPS | USE_UART1_DLPS\
                             | USE_USER_DEFINE_DLPS_ENTER_CB\
                             | USE_USER_DEFINE_DLPS_EXIT_CB)


#ifdef __cplusplus
}
#endif

#endif  /* _BOARD_H_ */

