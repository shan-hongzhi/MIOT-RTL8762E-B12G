/**
*****************************************************************************************
*     Copyright(c) 2021, Realtek Semiconductor Corporation. All rights reserved.
*****************************************************************************************
* @file      lpc.c
* @brief
* @author    yuan
* @date      2021-05-04
* @version   v0.1
**************************************************************************************
* @attention
* <h2><center>&copy; COPYRIGHT 2021 Realtek Semiconductor Corporation</center></h2>
**************************************************************************************
*/

/*============================================================================*
 *                              Header Files
 *============================================================================*/
#include "rtl876x_gpio.h"
#include "rtl876x_lpc.h"
#include "rtl876x_nvic.h"
#include "rtl876x_pinmux.h"
#include "rtl876x_rcc.h"
#include "rtl876x_rtc.h"

#include "board.h"
#include "platform_utils.h"
#include "trace.h"

#define LPC_CAPTURE_PIN                 P2_2
#define LPC_CAPTURE_CHANNEL             LPC_CHANNEL_P2_2

#define LPC_VOLTAGE_DETECT_EDGE         LPC_Vin_Over_Vth;
#define LPC_VOLTAGE_DETECT_THRESHOLD    LPC_2000_mV;

#define LPC_COMP_VALUE                  10

#define GPIO_OUTPUT_PIN_0               P2_4
#define GPIO_PIN_OUTPUT_0               GPIO_GetPin(GPIO_OUTPUT_PIN_0)

/**
  * @brief  Initialization of pinmux settings and pad settings.
  * @param  No parameter.
  * @return None.
  */
void board_lpc_init(void)
{
    Pad_Config(LPC_CAPTURE_PIN, PAD_SW_MODE, PAD_IS_PWRON, PAD_PULL_NONE, PAD_OUT_DISABLE,
               PAD_OUT_HIGH);
    Pinmux_Config(LPC_CAPTURE_PIN, IDLE_MODE);
}

/**
  * @brief  Initialize RTC peripheral.
  * @param  No parameter.
  * @return None.
  */
void driver_lpc_init(void)
{
    LPC_DeInit();
    LPC_InitTypeDef LPC_InitStruct;
    LPC_StructInit(&LPC_InitStruct);

    LPC_InitStruct.LPC_Channel   = LPC_CAPTURE_CHANNEL;
    LPC_InitStruct.LPC_Edge      = LPC_VOLTAGE_DETECT_EDGE;
    LPC_InitStruct.LPC_Threshold = LPC_VOLTAGE_DETECT_THRESHOLD;
    LPC_Init(&LPC_InitStruct);
    LPC_Cmd(ENABLE);

    LPC_ResetCounter();
    LPC_SetCompValue(LPC_COMP_VALUE);
    LPC_INTConfig(LPC_INT_LPCOMP_CNT, ENABLE);

    /* Config LPC interrupt */
    NVIC_InitTypeDef NVIC_InitStruct;
    NVIC_InitStruct.NVIC_IRQChannel = LPCOMP_IRQn;
    NVIC_InitStruct.NVIC_IRQChannelPriority = 2;
    NVIC_InitStruct.NVIC_IRQChannelCmd = ENABLE;
    NVIC_Init(&NVIC_InitStruct);

    LPC_INTCmd(ENABLE);
    LPC_CounterCmd(ENABLE);
}

/**
  * @brief  Initialization of pinmux settings and pad settings.
  * @param  No parameter.
  * @return None.
*/
void board_gpio_init(void)
{
    Pad_Config(GPIO_OUTPUT_PIN_0, PAD_PINMUX_MODE, PAD_IS_PWRON, PAD_PULL_NONE, PAD_OUT_ENABLE,
               PAD_OUT_HIGH);

    Pinmux_Config(GPIO_OUTPUT_PIN_0, DWGPIO);
}

/**
  * @brief  Initialize GPIO peripheral.
  * @param  No parameter.
  * @return void
*/
void driver_gpio_init(void)
{
    /* Initialize GPIO peripheral */
    RCC_PeriphClockCmd(APBPeriph_GPIO, APBPeriph_GPIO_CLOCK, ENABLE);

    GPIO_InitTypeDef GPIO_InitStruct;
    GPIO_StructInit(&GPIO_InitStruct);

    GPIO_InitStruct.GPIO_Pin  = GPIO_PIN_OUTPUT_0;
    GPIO_InitStruct.GPIO_Mode = GPIO_Mode_OUT;
    GPIO_Init(&GPIO_InitStruct);

    GPIO_WriteBit(GPIO_PIN_OUTPUT_0, (BitAction)(1));
}

void lpc_demo(void)
{
    board_lpc_init();
    driver_lpc_init();

    /* GPIO output is only used to simulate the input signal, only for demo debugging. */
    board_gpio_init();
    driver_gpio_init();
    while (1)
    {
        /* Simulate GPIO trigger signal */
        for (uint32_t i = 0; i < 100000; i++);
        GPIO_WriteBit(GPIO_PIN_OUTPUT_0, (BitAction)(1));
        for (uint32_t i = 0; i < 100000; i++);
        GPIO_WriteBit(GPIO_PIN_OUTPUT_0, (BitAction)(0));
    }
}

/**
 * @brief    Entry of APP code
 * @return   int (To avoid compile warning)
 */
int main(void)
{
    __enable_irq();

    lpc_demo();

    while (1)
    {
        __nop();
        __nop();
        __nop();
        __nop();
        __nop();
        __nop();
    }
}

/**
  * @brief  LPC battery detection interrupt handle function.
  * @param  None.
  * @return None.
  */
void LPCOMP_Handler(void)
{
    APP_PRINT_INFO0("LPCOMP_Handler\r\n");
    //Add Application code here
    /* LPC counter comparator interrupt */
    if (LPC_GetINTStatus(LPC_INT_LPCOMP_CNT) == SET)
    {
        APP_PRINT_INFO2("LPC_INT_LPCOMP_CNT: counter = %d comp_value = %d\r\n", LPC_GetCounter(),
                        LPC_GetCompValue());
        LPC_SetCompValue(LPC_GetCounter() + LPC_COMP_VALUE);
        LPC_ClearINTPendingBit(LPC_INT_LPCOMP_CNT);
    }
}
