/**
*****************************************************************************************
*     Copyright(c) 2021, Realtek Semiconductor Corporation. All rights reserved.
*****************************************************************************************
* @file
* @brief
* @author
* @date      2021-07-06
* @version   v1.0
**************************************************************************************
* @attention
* <h2><center>&copy; COPYRIGHT 2021 Realtek Semiconductor Corporation</center></h2>
**************************************************************************************
*/

/*============================================================================*
 *                              Header Files
 *============================================================================*/
#include "board.h"
#include "rtl876x_adc.h"
#include "rtl876x_captouch.h"
#include "rtl876x_nvic.h"
#include "trace.h"

/* Globals -------------------------------------------------------------------*/

/**
  * @brief  Initialization of pinmux settings and pad settings.
  * @param  No parameter.
  * @return void
*/
void board_ctc_init(void)
{

}

/**
  * @brief  Initialize tim peripheral.
  * @param   No parameter.
  * @return  void
  */
void driver_ctc_init(void)
{
//    RCC_PeriphClockCmd(APBPeriph_CTC, APBPeriph_CTC_CLOCK, ENABLE);

    CTC_RCCConfig(CTC_CLOCK_SOURCE_1M);

    CTC_InitTypeDef CTC_InitStruct;
    CTC_StructInit(&CTC_InitStruct);
    CTC_InitStruct.CTC_ETCEn = ENABLE;
    CTC_Init(CTC, &CTC_InitStruct);

//    AUXADC_PowerOn(ENABLE);

    CTC_ChannelInitTypeDef CTC_ChannelInitStruct;
    CTC_ChannelStructInit(&CTC_ChannelInitStruct);
    CTC_ChannelInitStruct.CTC_DifferenceTouchThd = 0x00;
    CTC_ChannelInitStruct.CTC_MBias = CTC_MBIAS_0p25uA;
    CTC_ChannelInitStruct.CTC_ChannelEn = ENABLE;
    CTC_ChannelInit(CTC_Channel0, &CTC_ChannelInitStruct);
    CTC_ChannelInit(CTC_Channel1, &CTC_ChannelInitStruct);

    /*  Enable IRQ */
    NVIC_InitTypeDef NVIC_InitStruct;
    NVIC_InitStruct.NVIC_IRQChannel = CAP_TOUCH_IRQn;
    NVIC_InitStruct.NVIC_IRQChannelPriority = 3;
    NVIC_InitStruct.NVIC_IRQChannelCmd = ENABLE;
    NVIC_Init(&NVIC_InitStruct);

    CTC_INTConfig(CTC_INT_FALSE_TOUCH_CH0 | CTC_INT_TOUCH_RELEASE_CH0 | CTC_INT_TOUCH_PRESS_CH0,
                  ENABLE);
    CTC_INTConfig(CTC_INT_FALSE_TOUCH_CH1 | CTC_INT_TOUCH_RELEASE_CH1 | CTC_INT_TOUCH_PRESS_CH1,
                  ENABLE);
}

/**
  * @brief  Demo code of Cap Touch.
  * @param  No parameter.
  * @return void
*/
void ctc_demo(void)
{
    /* Initialize Cap Touch peripheral */
    driver_ctc_init();

    /* Set scan interval */
    if (!CTC_SetScanInterval(0x3C, CTC_SLOW_MODE))
    {
        APP_PRINT_ERROR0(" Slow mode scan interval overange!\r\n");
    }
    if (!CTC_SetScanInterval(0x10, CTC_FAST_MODE))
    {
        APP_PRINT_ERROR0("Fast mode scan interval overange!\r\n");
    }

    /* Cap Touch start */
    CTC_Cmd(CTC, ENABLE);
    while (CTC_GetBaselineInistatus());
    APP_PRINT_INFO0("Cap Touch Baseline init done!\r\n");
}

/**
 * @brief    Entry of APP code
 * @return   int (To avoid compile warning)
 */
int main(void)
{
    __enable_irq();

    ctc_demo();

    while (1)
    {
        ;
    }
}

/**
  * @brief  CTC Interrupt Handler.
  * @retval None
  */
void CAP_Touch_Handler(void)
{
    /*Printed information is only used for debugging, and removed in actual applications.*/
    APP_PRINT_INFO3("ch0: [%4d, %5d, %4d]\r\n",
                    CTC_GetChannelAveData(CTC_Channel0),
                    CTC_GetChannelAveData(CTC_Channel0) - CTC_GetChannelBaseline(CTC_Channel0),
                    CTC_GetChannelTouchCount(CTC_CH0));
    APP_PRINT_INFO3("ch1: [%4d, %5d, %4d]\r\n",
                    CTC_GetChannelAveData(CTC_Channel1),
                    CTC_GetChannelAveData(CTC_Channel1) - CTC_GetChannelBaseline(CTC_Channel1),
                    CTC_GetChannelTouchCount(CTC_CH1));
    APP_PRINT_INFO3("fs:%d, touch0_status:%d, touch1_status:%d\r\n",
                    CTC_IsFastMode(),
                    CTC_GetChannelTouchStatus(CTC_CH0),
                    CTC_GetChannelTouchStatus(CTC_CH1));

    /* Add corresponding interrupt processing according to application needs.*/
    if (CTC_GetINTStatus(CTC, CTC_INT_FALSE_TOUCH_CH1))
    {
        CTC_ClearINTPendingBit(CTC, CTC_INT_FALSE_TOUCH_CH1);
        APP_PRINT_INFO0("CTC_INT_FALSE_TOUCH_CH1\r\n");
    }
    if (CTC_GetINTStatus(CTC, CTC_INT_FALSE_TOUCH_CH0))
    {
        //Add user code here.
        CTC_ClearINTPendingBit(CTC, CTC_INT_FALSE_TOUCH_CH0);
        APP_PRINT_INFO0("CTC_INT_FALSE_TOUCH_CH0\r\n");
    }
    if (CTC_GetINTStatus(CTC, CTC_INT_TOUCH_RELEASE_CH1))
    {
        //Add user code here.
        CTC_ClearINTPendingBit(CTC, CTC_INT_TOUCH_RELEASE_CH1);
        APP_PRINT_INFO0("CTC_INT_TOUCH_RELEASE_CH1\r\n");
    }
    if (CTC_GetINTStatus(CTC, CTC_INT_TOUCH_RELEASE_CH0))
    {
        //Add user code here.
        CTC_ClearINTPendingBit(CTC, CTC_INT_TOUCH_RELEASE_CH0);
        APP_PRINT_INFO0("CTC_INT_TOUCH_RELEASE_CH0\r\n");
    }
    if (CTC_GetINTStatus(CTC, CTC_INT_TOUCH_PRESS_CH1))
    {
        //Add user code here.
        CTC_ClearINTPendingBit(CTC, CTC_INT_TOUCH_PRESS_CH1);
        APP_PRINT_INFO0("CTC_INT_TOUCH_PRESS_CH1\r\n");
    }
    if (CTC_GetINTStatus(CTC, CTC_INT_TOUCH_PRESS_CH0))
    {
        //Add user code here.
        CTC_ClearINTPendingBit(CTC, CTC_INT_TOUCH_PRESS_CH0);
        APP_PRINT_INFO0("CTC_INT_TOUCH_PRESS_CH0\r\n");
    }
    if (CTC_GetINTStatus(CTC, CTC_INT_N_NOISE_THD))
    {
        //Add user code here.
        CTC_ClearINTPendingBit(CTC, CTC_INT_N_NOISE_THD);
        APP_PRINT_INFO0("CTC_INT_N_NOISE_THD\r\n");
    }
    if (CTC_GetINTStatus(CTC, CTC_INT_P_NOISE_THD))
    {
        //Add user code here.
        CTC_Cmd(CTC, DISABLE);
        CTC_Cmd(CTC, ENABLE);
        CTC_ClearINTPendingBit(CTC, CTC_INT_P_NOISE_THD);
        APP_PRINT_INFO0("CTC_INT_P_NOISE_THD\r\n");
    }
    if (CTC_GetINTStatus(CTC, CTC_INT_FIFO_OVERFLOW))
    {
        //Add user code here.
        CTC_ClearINTPendingBit(CTC, CTC_INT_FIFO_OVERFLOW);
        APP_PRINT_INFO0("CTC_INT_FIFO_OVERFLOW\r\n");
    }
}
