/**
*********************************************************************************************************
*               Copyright(c) 2020, Realtek Semiconductor Corporation. All rights reserved.
**********************************************************************************************************
* @file     rtl876x_driver_sample.c
* @brief    This file provides samples
* @author   Yuan
* @date     2020-10-13
* @version  v1.0.0
*********************************************************************************************************
*/
/* Includes ------------------------------------------------------------------*/
#include "rtl876x.h"
#include "arm_internal.h"
#include <nuttx/config.h>
#include <nuttx/irq.h>
#include <nuttx/serial/serial.h>
#include <sys/types.h>
#include <stdint.h>
#include <stdbool.h>
#include <sched.h>
#include <stdbool.h>
#include <syslog.h>

#include "rtl876x_nvic.h"
#include "rtl876x_pinmux.h"
#include "rtl876x_rcc.h"
#ifdef CONFIG_RTL876x_DRIVER_SAMPLE

#ifdef CONFIG_RTL876x_DRIVER_SAMPLE_GPIO
#include "rtl876x_gpio.h"
#define GPIO_INPUT_PIN_0        P4_0
#define GPIO_PIN_INPUT          GPIO_GetPin(GPIO_INPUT_PIN_0)
#define GPIO_PIN_INPUT_VECTORn  GPIO_Group1_VECTORn
#define GPIO_INPUT_HANDLER      gpio_group1_handler
static int GPIO_INPUT_HANDLER(int irq, void *context, void *arg);

static void board_gpio_init(void)
{
    Pad_Config(GPIO_INPUT_PIN_0, PAD_PINMUX_MODE, PAD_IS_PWRON, PAD_PULL_UP, PAD_OUT_DISABLE,
               PAD_OUT_HIGH);

    Pinmux_Config(GPIO_INPUT_PIN_0, DWGPIO);
}

static void driver_gpio_init(void)
{
    RCC_PeriphClockCmd(APBPeriph_GPIO, APBPeriph_GPIO_CLOCK, ENABLE);

    GPIO_InitTypeDef GPIO_InitStruct;
    GPIO_StructInit(&GPIO_InitStruct);
    GPIO_InitStruct.GPIO_Pin        = GPIO_PIN_INPUT;
    GPIO_InitStruct.GPIO_Mode       = GPIO_Mode_IN;
    GPIO_InitStruct.GPIO_ITCmd      = ENABLE;
    GPIO_InitStruct.GPIO_ITTrigger  = GPIO_INT_Trigger_EDGE;
    GPIO_InitStruct.GPIO_ITPolarity = GPIO_INT_POLARITY_ACTIVE_LOW;
    GPIO_InitStruct.GPIO_ITDebounce = GPIO_INT_DEBOUNCE_ENABLE;
    GPIO_InitStruct.GPIO_DebounceTime = 10;/* unit:ms , can be 1~64 ms */
    GPIO_Init(&GPIO_InitStruct);

    up_enable_irq(GPIO_PIN_INPUT_VECTORn);
    irq_attach(GPIO_PIN_INPUT_VECTORn, GPIO_INPUT_HANDLER, NULL);

    GPIO_MaskINTConfig(GPIO_PIN_INPUT, DISABLE);
    GPIO_INTConfig(GPIO_PIN_INPUT, ENABLE);
}

static void gpio_demo(void)
{
    syslog(0,"[__rtl_driver_sample] gpio\n");
    board_gpio_init();
    driver_gpio_init();
}

static int GPIO_INPUT_HANDLER(int irq, void *context, void *arg)
{
    syslog(0,"[__rtl_driver_sample] GPIO_Input_Handler\n");
    if (GPIO_GetINTStatus(GPIO_PIN_INPUT))
    {
        GPIO_INTConfig(GPIO_PIN_INPUT, DISABLE);
        GPIO_MaskINTConfig(GPIO_PIN_INPUT, ENABLE);
        //Add user code here
        GPIO_ClearINTPendingBit(GPIO_PIN_INPUT);
        GPIO_MaskINTConfig(GPIO_PIN_INPUT, DISABLE);
        GPIO_INTConfig(GPIO_PIN_INPUT, ENABLE);
    }
    
    return OK;
}

#endif

#ifdef CONFIG_RTL876x_DRIVER_SAMPLE_TIM
#include "rtl876x_tim.h"
#define TIMER_NUM           TIM4
#define TIMING_TIME         1000000    //uint: us
#define TIMER_PERIOD        ((TIMING_TIME)*40-1)
#define TIMER_VECTORn       Timer4_VECTORn
#define TIM_INPUT_HANDLER   timer4_handler
static int TIM_INPUT_HANDLER(int irq, void *context, void *arg);

static void driver_timer_init(void)
{
    RCC_PeriphClockCmd(APBPeriph_TIMER, APBPeriph_TIMER_CLOCK, ENABLE);

    TIM_TimeBaseInitTypeDef TIM_InitStruct;
    TIM_StructInit(&TIM_InitStruct);

    TIM_InitStruct.TIM_PWM_En = PWM_DISABLE;
    TIM_InitStruct.TIM_Period = TIMER_PERIOD ;
    TIM_InitStruct.TIM_Mode = TIM_Mode_UserDefine;
    TIM_TimeBaseInit(TIMER_NUM, &TIM_InitStruct);

    up_enable_irq(TIMER_VECTORn);
    irq_attach(TIMER_VECTORn, TIM_INPUT_HANDLER, NULL);

    TIM_ClearINT(TIMER_NUM);
    TIM_INTConfig(TIMER_NUM, ENABLE);
    TIM_Cmd(TIMER_NUM, ENABLE);

}

static void tim_demo(void)
{
    syslog(0,"[__rtl_driver_sample] tim\n");
    driver_timer_init();
}

static int TIM_INPUT_HANDLER(int irq, void *context, void *arg)
{
    syslog(0,"[__rtl_driver_sample] Timer_Handler\n");

    TIM_ClearINT(TIMER_NUM);
    TIM_Cmd(TIMER_NUM, DISABLE);
    //Add user code here
    TIM_Cmd(TIMER_NUM, ENABLE);
    return OK;
}

#endif

#ifdef CONFIG_RTL876x_DRIVER_SAMPLE_PWM
#include "rtl876x_tim.h"
#define PWM_OUT_PIN             P0_1
#define PWM_OUT_PIN_PINMUX      TIM_PWM2//timer_pwm2
#define PWM_PERIOD              100000 //uint:us
#define PWM_DUTY_CYCLE          50      //uint:percent
#define PWM_HIGH_COUNT          (((PWM_PERIOD)*((PWM_DUTY_CYCLE*40)/100))-1)    //PWM CLOCK = 40000000
#define PWM_LOW_COUNT           (((PWM_PERIOD)*(((100-PWM_DUTY_CYCLE)*40)/100))-1)

static void board_pwm_init(void)
{
    Pad_Config(PWM_OUT_PIN, PAD_PINMUX_MODE, PAD_IS_PWRON, PAD_PULL_NONE, PAD_OUT_ENABLE, PAD_OUT_HIGH);
    Pinmux_Config(PWM_OUT_PIN, PWM_OUT_PIN_PINMUX);
}

static void driver_pwm_init(void)
{
    RCC_PeriphClockCmd(APBPeriph_TIMER, APBPeriph_TIMER_CLOCK, ENABLE);

    TIM_TimeBaseInitTypeDef TIM_InitStruct;

    TIM_StructInit(&TIM_InitStruct);
    TIM_InitStruct.TIM_Mode             = TIM_Mode_UserDefine;
    TIM_InitStruct.TIM_PWM_En           = PWM_ENABLE;
    TIM_InitStruct.TIM_PWM_High_Count   = PWM_HIGH_COUNT;
    TIM_InitStruct.TIM_PWM_Low_Count    = PWM_LOW_COUNT;
    TIM_TimeBaseInit(TIM2, &TIM_InitStruct);

    TIM_Cmd(TIM2, ENABLE);
}

static void pwm_demo(void)
{
    syslog(0,"[__rtl_driver_sample] pwm\n");
    board_pwm_init();
    driver_pwm_init();
}
#endif

#ifdef CONFIG_RTL876x_DRIVER_SAMPLE_RTC
#include "rtl876x_rtc.h"
#define RTC_PRESCALER_VALUE     (3200-1)//f = 10Hz
/* RTC has 4 comparators,comparator0~3 . */
#define RTC_COMP_INDEX          RTC_COMP1
#define RTC_COMP_INDEX_INT      RTC_INT_COMP1
#define RTC_COMP_VALUE          (10)
#define RTC_HANDLER             rtc_handler
static int RTC_HANDLER(int irq, void *context, void *arg);

static void driver_rtc_init(void)
{
    RTC_DeInit();

    RTC_SetPrescaler(RTC_PRESCALER_VALUE);
    RTC_SetCompValue(RTC_COMP_INDEX, RTC_COMP_VALUE);

    RTC_INTConfig(RTC_COMP_INDEX_INT, ENABLE);

    up_enable_irq(RTC_VECTORn);
    irq_attach(RTC_VECTORn, RTC_HANDLER, NULL);

    RTC_NvCmd(ENABLE);
    /* Start RTC */
    RTC_ResetCounter();
    RTC_Cmd(ENABLE);
}

static void rtc_demo(void)
{
    syslog(0,"[__rtl_driver_sample] rtc\n");
    driver_rtc_init();
}

static int RTC_HANDLER(int irq, void *context, void *arg)
{
    if (RTC_GetINTStatus(RTC_COMP_INDEX_INT) == SET)
    {
        syslog(0,"[__rtl_driver_sample] RTC_Handler\n");
        RTC_SetCompValue(RTC_COMP_INDEX, RTC_GetCounter() + RTC_COMP_VALUE);
        RTC_ClearCompINT(RTC_COMP_INDEX);
        RTC_ClearINTPendingBit(RTC_COMP_INDEX);
    }
    return OK;
}

#endif

#ifdef CONFIG_RTL876x_DRIVER_SAMPLE_ADC
#include "rtl876x_adc.h"

#define ADC_DIVIDE_MODE                     0
#define ADC_BYPASS_MODE                     1
#define ADC_DATA_HW_AVERAGE         0
#define ADC_DATA_OUTPUT_TO_FIFO     0

#define ADC_Channel_Index_0         0
#define ADC_Channel_Index_1         1
#define ADC_Channel_Index_2         2
#define ADC_Channel_Index_3         3

#define ADC_Schedule_Index_0         0
#define ADC_Schedule_Index_1         1
#define ADC_Schedule_Index_2         2
#define ADC_Schedule_Index_3         3
#define ADC_Schedule_Index_4         4
#define ADC_Schedule_Index_5         5
#define ADC_Schedule_Index_6         6
#define ADC_Schedule_Index_7         7
#define ADC_Schedule_Index_8         8
#define ADC_Schedule_Index_9         9
#define ADC_Schedule_Index_10        10
#define ADC_Schedule_Index_11        11
#define ADC_Schedule_Index_12        12
#define ADC_Schedule_Index_13        13
#define ADC_Schedule_Index_14        14
#define ADC_Schedule_Index_15        15
/* Change the ADC sampling mode here! */
#define ADC_MODE_DIVIDE_OR_BYPASS           ADC_DIVIDE_MODE
#define ADC_SAMPLE_PIN_0                    P2_2
#define ADC_SAMPLE_CHANNEL_0                ADC_Channel_Index_0
#define ADC_HANDLER                         adc_handler
static int ADC_HANDLER(int irq, void *context, void *arg);

static void board_adc_init(void)
{
    Pad_Config(ADC_SAMPLE_PIN_0, PAD_SW_MODE, PAD_IS_PWRON, PAD_PULL_NONE, PAD_OUT_DISABLE,
               PAD_OUT_LOW);
}

static void driver_adc_init(void)
{
    RCC_PeriphClockCmd(APBPeriph_ADC, APBPeriph_ADC_CLOCK, ENABLE);

    ADC_InitTypeDef ADC_InitStruct;
    ADC_StructInit(&ADC_InitStruct);

    /* Configure the ADC sampling schedule0 */
    ADC_InitStruct.ADC_SchIndex[0]      = EXT_SINGLE_ENDED(ADC_SAMPLE_CHANNEL_0);
    /* Set the bitmap corresponding to schedule0*/
    ADC_InitStruct.ADC_Bitmap           = 0x01;

#if (ADC_DATA_HW_AVERAGE && ADC_DATA_OUTPUT_TO_FIFO)
    syslog(0,"[__rtl_driver_sample] driver_adc_init: ADC config error !\n");
#elif (ADC_DATA_HW_AVERAGE )
    ADC_InitStruct.ADC_DataAvgEn        = ADC_DATA_AVERAGE_ENABLE;
    ADC_InitStruct.ADC_DataAvgSel       = ADC_DATA_AVERAGE_OF_4;
#elif (ADC_DATA_OUTPUT_TO_FIFO)
    ADC_InitStruct.ADC_DataWriteToFifo  = ADC_DATA_WRITE_TO_FIFO_ENABLE;
    ADC_InitStruct.ADC_FifoThdLevel     = 0x0A;
#endif

    ADC_InitStruct.ADC_PowerAlwaysOnEn  = ADC_POWER_ALWAYS_ON_ENABLE;
    /* Fixed 255 in OneShot mode. */
    ADC_InitStruct.ADC_SampleTime       = 255;

    ADC_Init(ADC, &ADC_InitStruct);

#if (ADC_MODE_DIVIDE_OR_BYPASS == ADC_BYPASS_MODE)
    /* High bypass resistance mode config, please notice that the input voltage of
      adc channel using high bypass mode should not be over 0.9V */
    ADC_BypassCmd(ADC_SAMPLE_CHANNEL_0, ENABLE);
#else
    ADC_BypassCmd(ADC_SAMPLE_CHANNEL_0, DISABLE);
#endif

#if (!ADC_DATA_OUTPUT_TO_FIFO)
    ADC_INTConfig(ADC, ADC_INT_ONE_SHOT_DONE, ENABLE);
#else
    ADC_INTConfig(ADC, ADC_INT_FIFO_THD, ENABLE);
#endif

    up_enable_irq(ADC_VECTORn);
    irq_attach(ADC_VECTORn, ADC_HANDLER, NULL);

    /* When ADC is enabled, sampling will be done quickly and interruption will occur.
       After initialization, ADC can be enabled when sampling is needed.*/
    ADC_Cmd(ADC, ADC_ONE_SHOT_MODE, ENABLE);
}

static void adc_demo(void)
{
    syslog(0,"[__rtl_driver_sample] adc\n");
    board_adc_init();
    driver_adc_init();
}

static int ADC_HANDLER(int irq, void *context, void *arg)
{
#if (!ADC_DATA_OUTPUT_TO_FIFO)
    if (ADC_GetINTStatus(ADC, ADC_INT_ONE_SHOT_DONE) == SET)
    {
        ADC_ClearINTPendingBit(ADC, ADC_INT_ONE_SHOT_DONE);

        uint16_t sample_data = 0;
        sample_data = ADC_ReadRawData(ADC, ADC_Schedule_Index_0);
        syslog(0,"[__rtl_driver_sample] ADC sample_data = 0x%x \n", sample_data);

        ADC_ClearINTPendingBit(ADC, ADC_INT_ONE_SHOT_DONE);
    }
#else
    if (ADC_GetIntFlagStatus(ADC, ADC_INT_FIFO_THD) == SET)
    {
        typedef struct
        {
            uint16_t RawData[32];
            uint8_t RawDataLen;
        } ADC_Data_TypeDef;
        ADC_Data_TypeDef ADC_Global_Data;
        ADC_Global_Data.RawDataLen = ADC_GetFifoLen(ADC);
        ADC_GetFifoData(ADC, ADC_Global_Data.RawData, ADC_Global_Data.RawDataLen);

        for (uint8_t i = 0; i < ADC_Global_Data.RawDataLen; i++)
        {
            DBG_DIRECT("ADC_INT_FIFO_THD: data[%d] = %d", i, ADC_Global_Data.RawData[i]);
        }

        ADC_ClearINTPendingBit(ADC, ADC_INT_FIFO_THD);
        ADC_ClearFifo(ADC);
        TIM_Cmd(TIM7, ENABLE);
    }
#endif
    return OK;
}

#endif

void __rtl_driver_sample(void)
{
#ifdef CONFIG_RTL876x_DRIVER_SAMPLE_GPIO
    gpio_demo();
#endif
#ifdef CONFIG_RTL876x_DRIVER_SAMPLE_TIM
    tim_demo();
#endif
#ifdef CONFIG_RTL876x_DRIVER_SAMPLE_PWM
    pwm_demo();
#endif
#ifdef CONFIG_RTL876x_DRIVER_SAMPLE_RTC
    rtc_demo();
#endif
#ifdef CONFIG_RTL876x_DRIVER_SAMPLE_ADC
    adc_demo();
#endif

}

#endif /* CONFIG_RTL876x_DRIVER_SAMPLE */
/******************* (C) COPYRIGHT 2020 Realtek Semiconductor Corporation *****END OF FILE****/

