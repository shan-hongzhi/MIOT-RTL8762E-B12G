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

/*******************************************************
*                 Keyboard Keyscan Config
*******************************************************/
/* keypad row and column */
#define KEYPAD_ROW_SIZE       4
#define KEYPAD_COLUMN_SIZE    4
#define ROW0                  P3_0
#define ROW1                  P3_1
#define ROW2                  P1_0
#define ROW3                  P1_1

#define COLUMN0               P4_0
#define COLUMN1               P4_1
#define COLUMN2               P4_2
#define COLUMN3               P4_3

/*******************************************************
*                 Keyboard Button Config
*******************************************************/
#define PAIR_BUTTON           P2_4

/*******************************************************
*                 Keyboard LED Config
*******************************************************/
#define ADV_LED               P0_1
#define CAPS_LOCK_LED         P2_5

#define DFU_BUFFER_CHECK_ENABLE     1  /* set 1 to enable buffer check feature */
#define DFU_TEMP_BUFFER_SIZE        2048  /* dfu max buffer size */

/** @defgroup IO Driver Config
  * @note user must config it firstly!! Do not change macro names!!
  * @{
  */

/* if use user define dlps enter/dlps exit callback function */
#define USE_USER_DEFINE_DLPS_ENTER_CB       1
#define USE_USER_DEFINE_DLPS_EXIT_CB        1

/* if use any peripherals below, #define it 1 */
#define USE_ADC_DLPS                0
#define USE_CTC_DLPS                0

#define USE_GPIO_DLPS               1
#define USE_I2C0_DLPS               0
#define USE_I2C1_DLPS               0
#define USE_IR_DLPS                 0
#define USE_KEYSCAN_DLPS            1




#define USE_QDECODER_DLPS           0

#define USE_SPI0_DLPS               0
#define USE_SPI1_DLPS               0
#define USE_SPI2W_DLPS              0
#define USE_TIM_DLPS                0
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
#define DLPS_EN                     1



#ifdef __cplusplus
}
#endif

#endif  /* _BOARD_H_ */

