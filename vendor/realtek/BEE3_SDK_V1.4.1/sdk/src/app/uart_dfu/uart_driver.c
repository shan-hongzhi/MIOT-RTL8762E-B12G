/**
*********************************************************************************************************
*               Copyright(c) 2022, Realtek Semiconductor Corporation. All rights reserved.
**********************************************************************************************************
* @file     uart_driver.c
* @brief    uart module driver
* @details
* @author   mandy
* @date     2022-01-04
* @version  v1.0
*********************************************************************************************************
*/

/*============================================================================*
 *                              Header Files
 *============================================================================*/
#include <board.h>
#include <string.h>
#include <trace.h>
#include <os_msg.h>
#include <app_msg.h>
#include <os_timer.h>
#include <uart_driver.h>
#include <rtl876x_rcc.h>
#include <rtl876x_uart.h>
#include <rtl876x_pinmux.h>
#include <rtl876x_nvic.h>
#include <app_task.h>
#include <app_section.h>
#include "loop_queue.h"

/*============================================================================*
 *                              Local Variables
 *============================================================================*/

/*============================================================================*
 *                              Global Variables
 *============================================================================*/
T_UART_GLOBAL_DATA uart_global_data;
T_LOOP_QUEUE_DEF *p_uart_queue;

/*============================================================================*
 *                              Functions Declaration
 *============================================================================*/
void uart_enter_dlps_config(void) DATA_RAM_FUNCTION;
void uart_exit_dlps_config(void) DATA_RAM_FUNCTION;
void uart_nvic_config(void) DATA_RAM_FUNCTION;
void UART_DFU_Handler(void) DATA_RAM_FUNCTION;
void uart_send_data(uint8_t *buf, uint32_t length) DATA_RAM_FUNCTION;

/*============================================================================*
 *                              Local Functions
 *============================================================================*/
/******************************************************************
 * @brief  uart enable wakeup config function
 * @param  none
 * @return none
 * @retval void
 */
void uart_enable_wakeup_config(void)
{
    System_WakeUpPinEnable(UART_RX, PAD_WAKEUP_POL_LOW, PAD_WK_DEBOUNCE_DISABLE);
}
/*============================================================================*
 *                              Global Functions
 *============================================================================*/
/******************************************************************
 * @brief  uart disable wakeup config function
 * @param  none
 * @return none
 * @retval void
 */
void uart_disable_wakeup_config(void)
{
    System_WakeUpPinDisable(UART_RX);
}

/******************************************************************
 * @brief  initialize uart driver data
 * @param  none
 * @return none
 * @retval void
 */
void uart_init_data(void)
{
    APP_PRINT_INFO0("uart_init_data");
    memset(&uart_global_data, 0, sizeof(uart_global_data));
    uart_global_data.baudrate = UART_BANDRATE;
    uart_global_data.is_allowed_to_enter_dlps = true;
    p_uart_queue = loop_queue_init(RECEIVE_BUF_MAX_LENGTH, 1, RAM_TYPE_DATA_ON);
}

/******************************************************************
 * @brief    uart nvic config
 * @param    none
 * @return   none
 * @retval   void
 */
void uart_nvic_config(void)
{
    NVIC_InitTypeDef NVIC_InitStruct;

    NVIC_InitStruct.NVIC_IRQChannel = UART_DFU_IRQn;
    NVIC_InitStruct.NVIC_IRQChannelPriority = 3;
    NVIC_InitStruct.NVIC_IRQChannelCmd = ENABLE;

    NVIC_Init(&NVIC_InitStruct);
}

/******************************************************************
 * @fn       uart_init_driver
 * @brief    uart module initial
 * @param    none
 * @return   none
 * @retval   void
 */
void uart_init_driver(void)
{
    RCC_PeriphClockCmd(UART_DFU_APBPERIPH, UART_DFU_APBPERIPH_CLK, ENABLE);

    UART_InitTypeDef UART_InitStruct;
    UART_StructInit(&UART_InitStruct);

    UART_InitStruct.rxTriggerLevel = UART_RX_TRIGGER_VALUE;

    switch (uart_global_data.baudrate)
    {
    case 2400:
        UART_InitStruct.div = 1295;
        UART_InitStruct.ovsr = 7;
        UART_InitStruct.ovsr_adj = 0x7F7;
        break;
    case 4800:
        UART_InitStruct.div = 542;
        UART_InitStruct.ovsr = 10;
        UART_InitStruct.ovsr_adj = 0x24A;
        break;
    case 9600:
        UART_InitStruct.div = 271;
        UART_InitStruct.ovsr = 10;
        UART_InitStruct.ovsr_adj = 0x24A;
        break;
    case 19200:
        UART_InitStruct.div = 165;
        UART_InitStruct.ovsr = 7;
        UART_InitStruct.ovsr_adj = 0x5AD;
        break;
    case 38400:
        UART_InitStruct.div = 85;
        UART_InitStruct.ovsr = 7;
        UART_InitStruct.ovsr_adj = 0x222;
        break;
    case 57600:
        UART_InitStruct.div = 55;
        UART_InitStruct.ovsr = 7;
        UART_InitStruct.ovsr_adj = 0x5AD;
        break;
    case 115200:
        UART_InitStruct.div = 20;
        UART_InitStruct.ovsr = 12;
        UART_InitStruct.ovsr_adj = 0x252;
        break;
    case 921600:
        UART_InitStruct.div = 3;
        UART_InitStruct.ovsr = 9;
        UART_InitStruct.ovsr_adj = 0x2AA;
        break;
    case 1000000:
        UART_InitStruct.div = 4;
        UART_InitStruct.ovsr = 5;
        UART_InitStruct.ovsr_adj = 0;
        break;
    case 2000000:
        UART_InitStruct.div = 2;
        UART_InitStruct.ovsr = 5;
        UART_InitStruct.ovsr_adj = 0;
        break;
    case 3000000:
        UART_InitStruct.div = 1;
        UART_InitStruct.ovsr = 8;
        UART_InitStruct.ovsr_adj = 0x292;
        break;
    default:
        UART_InitStruct.div = 271;//9600
        UART_InitStruct.ovsr = 10;
        UART_InitStruct.ovsr_adj = 0x24A;
        break;
    }

    UART_Init(UART_DFU, &UART_InitStruct);
    /*  enable line status interrupt and rx data avaliable interrupt    */
    UART_INTConfig(UART_DFU, UART_INT_RD_AVA | UART_INT_LINE_STS | UART_INT_RX_IDLE,  ENABLE);

    uart_nvic_config();
}

/******************************************************************
 * @brief    uart pinmux config
 * @param    none
 * @return   none
 * @retval   void
 */
void uart_pinmux_config(void)
{
    Pinmux_Config(UART_TX, UART_DFU_TX);
    Pinmux_Config(UART_RX, UART_DFU_RX);
}

/******************************************************************
 * @brief    uart init pad config
 * @param    none
 * @return   none
 * @retval   void
 */
void uart_init_pad_config(void)
{
    Pad_Config(UART_TX, PAD_PINMUX_MODE, PAD_IS_PWRON, PAD_PULL_UP, PAD_OUT_DISABLE, PAD_OUT_HIGH);
    Pad_Config(UART_RX, PAD_PINMUX_MODE, PAD_IS_PWRON, PAD_PULL_UP, PAD_OUT_DISABLE, PAD_OUT_HIGH);
}

/******************************************************************
 * @brief    uart enter DLPS config
 * @param    none
 * @return   none
 * @retval   void
 */
void uart_enter_dlps_config(void)
{
    Pad_Config(UART_TX, PAD_SW_MODE, PAD_IS_PWRON, PAD_PULL_UP, PAD_OUT_DISABLE, PAD_OUT_HIGH);
    Pad_Config(UART_RX, PAD_SW_MODE, PAD_IS_PWRON, PAD_PULL_UP, PAD_OUT_DISABLE, PAD_OUT_HIGH);
    uart_enable_wakeup_config();
}

/******************************************************************
 * @brief    uart exit DLPS config
 * @param    none
 * @return   none
 * @retval   void
 */
void uart_exit_dlps_config(void)
{
    Pad_Config(UART_TX, PAD_PINMUX_MODE, PAD_IS_PWRON, PAD_PULL_UP, PAD_OUT_DISABLE, PAD_OUT_HIGH);
    Pad_Config(UART_RX, PAD_PINMUX_MODE, PAD_IS_PWRON, PAD_PULL_UP, PAD_OUT_DISABLE, PAD_OUT_HIGH);
}

/******************************************************************
 * @brief    uart interrupt handler
 * @param    none
 * @return   none
 * @retval   void
 */
void UART_DFU_Handler(void)
{
    uint32_t int_status;
    T_IO_MSG uart_msg;

    if (true == uart_global_data.is_allowed_to_enter_dlps)
    {
        uart_global_data.is_allowed_to_enter_dlps = false;
    }

    /* read interrupt id */
    int_status = UART_GetIID(UART_DFU);

    if (UART_GetFlagState(UART_DFU, UART_FLAG_RX_IDLE) == SET)
    {
        //clear Flag
        UART_INTConfig(UART_DFU, UART_INT_RX_IDLE, DISABLE);
        UART_ClearRxFIFO(UART_DFU);
        uart_msg.type = IO_MSG_TYPE_UART;
        uart_msg.subtype = IO_MSG_UART_RX;
        if (false == app_send_msg_to_app(&uart_msg))
        {
            APP_PRINT_WARN0("[UART_DFU_Handler] Send IO_MSG_TYPE_UART failed!");
        }

        //enable idle interrupt again
        UART_INTConfig(UART_DFU, UART_INT_RX_IDLE, ENABLE);
    }

    /* disable interrupt */
    UART_INTConfig(UART_DFU, UART_INT_RD_AVA | UART_INT_LINE_STS, DISABLE);

    switch (int_status)
    {
    /* rx data valiable */
    case UART_INT_ID_RX_LEVEL_REACH:
        {
            uint8_t temp_buf[UART_RX_TRIGGER_VALUE];
            UART_ReceiveData(UART_DFU, temp_buf, UART_RX_TRIGGER_VALUE);
            if (!loop_queue_is_full(p_uart_queue, UART_RX_TRIGGER_VALUE))
            {
                loop_queue_write_buf(p_uart_queue, temp_buf, UART_RX_TRIGGER_VALUE, true);
            }
        }
        break;

    /* rx time out */
    case UART_INT_ID_RX_TMEOUT:
        {
            uint8_t temp_data;
            while (UART_GetFlagStatus(UART_DFU, UART_FLAG_RX_DATA_RDY) == SET)
            {
                UART_ReceiveData(UART_DFU, &temp_data, 1);
                if (!loop_queue_is_full(p_uart_queue, 1))
                {
                    loop_queue_write_buf(p_uart_queue, &temp_data, 1, true);
                }
            }
        }
        break;
    /* receive line status interrupt */
    case UART_INT_ID_LINE_STATUS:
        APP_PRINT_INFO1("Line status error!=0x%x", UART_DFU->LSR);
        break;

    default:
        break;
    }

    UART_INTConfig(UART_DFU, UART_INT_RD_AVA | UART_INT_LINE_STS, ENABLE);
}

/******************************************************************
 * @brief    uart send data
 * @param    buf - point to data buf.
 * @param    length - length of data to be sent.
 * @return   none
 * @retval   void
 */
void uart_send_data(uint8_t *buf, uint32_t length)
{
    uint32_t i;
    uint32_t count = length / 16;
    uint32_t remainder = length % 16;
    for (i = 0; i < count; i++)
    {
        UART_SendData(UART_DFU, &buf[16 * i], 16);
        while (UART_GetFlagState(UART_DFU, UART_FLAG_THR_TSR_EMPTY) != SET);
    }
    /* send left bytes */
    UART_SendData(UART_DFU, &buf[16 * i], remainder);
    while (UART_GetFlagState(UART_DFU, UART_FLAG_THR_TSR_EMPTY) != SET);
}

/******************* (C) COPYRIGHT 2022 Realtek Semiconductor Corporation *****END OF FILE****/
