/**
*********************************************************************************************************
*               Copyright(c) 2018, Realtek Semiconductor Corporation. All rights reserved.
**********************************************************************************************************
* @file     uart_interrupt_demo.c
* @brief    uart demo interrupt
* @details
* @author   yuan
* @date     2018-06-28
* @version  v0.1
*********************************************************************************************************
*/

/* Includes ------------------------------------------------------------------*/
#include <string.h>
#include <stdlib.h>
#include "rtl876x_nvic.h"
#include "rtl876x_pinmux.h"
#include "rtl876x_rcc.h"
#include "rtl876x_uart.h"
#include "trace.h"
#include "otp.h"
#include "platform_utils.h"
#include "os_sync.h"
#include "dlps.h"
#include "rtl876x_io_dlps.h"

/* Defines ------------------------------------------------------------------*/


/* Globals ------------------------------------------------------------------*/
typedef struct
{
    uint16_t div;
    uint16_t ovsr;
    uint16_t ovsr_adj;
} UART_BaudRate_TypeDef;

typedef enum
{
    BAUD_RATE_9600,
    BAUD_RATE_19200,
    BAUD_RATE_115200,
    BAUD_RATE_230400,
    BAUD_RATE_256000,
    BAUD_RATE_384000,
    BAUD_RATE_460800,
    BAUD_RATE_921600,
    BAUD_RATE_1000000,
    BAUD_RATE_2000000,
    BAUD_RATE_3000000
} UartBaudRate_TypeDef;

const UART_BaudRate_TypeDef BaudRate_Table[11] =
{
    {271, 10, 0x24A}, // BAUD_RATE_9600
    {150, 8,  0x3EF}, // BAUD_RATE_19200
    {20, 12,  0x252}, // BAUD_RATE_115200
    {11,  10, 0x3BB}, // BAUD_RATE_230400
    {11,  9,  0x084}, // BAUD_RATE_256000
    {7,   9,  0x3EF}, // BAUD_RATE_384000
    {6,   9,  0x0AA}, // BAUD_RATE_460800
    {3,   9,  0x0AA}, // BAUD_RATE_921600
    {4,   5,  0},     // BAUD_RATE_1000000
    {2,   5,  0},     // BAUD_RATE_2000000
    {1,   8,  0x292}, // BAUD_RATE_3000000
};

uint8_t String_Buf[100];
uint8_t UART_Recv_Buf[256];
uint8_t UART_Send_Buf[256];
uint16_t UART_Recv_Buf_Lenth = 0;
bool receive_flag = false;

void data_print_buffer(const uint8_t *source, uint16_t size);

void board_uart_init_0(void)
{
    Pad_Config(P4_1, PAD_PINMUX_MODE, PAD_IS_PWRON, PAD_PULL_UP, PAD_OUT_DISABLE, PAD_OUT_HIGH);
    Pinmux_Config(P4_1, UART0_TX);
}

/**
  * @brief  Initialize uart peripheral.
  * @param  No parameter.
  * @return void
*/
void driver_uart_init_0(void)
{
    RCC_PeriphClockCmd(APBPeriph_UART0, APBPeriph_UART0_CLOCK, ENABLE);

    /* uart init */
    UART_InitTypeDef UART_InitStruct;
    UART_StructInit(&UART_InitStruct);

    UART_Init(UART0, &UART_InitStruct);
}

/**
  * @brief  Initialization of pinmux settings and pad settings.
  * @param  No parameter.
  * @return void
*/


/**
  * @brief  Initialize uart peripheral.
  * @param  No parameter.
  * @return void
*/

void driver_uart_init(void)
{
    UART_INTConfig(UART1, UART_INT_RX_IDLE, DISABLE);
    UART_ClearRxFIFO(UART1);
    UART_INTConfig(UART1, UART_INT_RX_IDLE, ENABLE);

    UART_INTConfig(UART1, UART_INT_RD_AVA, ENABLE);

    /*  Enable UART IRQ  */
    NVIC_InitTypeDef NVIC_InitStruct;
    NVIC_InitStruct.NVIC_IRQChannel         = UART1_IRQn;
    NVIC_InitStruct.NVIC_IRQChannelCmd      = (FunctionalState)ENABLE;
    NVIC_InitStruct.NVIC_IRQChannelPriority = 3;
    NVIC_Init(&NVIC_InitStruct);
}

/**
  * @brief  UARt send data continuous.
  * @param  No parameter.
  * @return void
*/
void uart_senddata_continuous(UART_TypeDef *UARTx, const uint8_t *pSend_Buf, uint16_t vCount)
{
    uint8_t count;

    while (vCount / UART_TX_FIFO_SIZE > 0)
    {
        while (UART_GetFlagStatus(UARTx, UART_FLAG_TX_FIFO_EMPTY) == 0);
        for (count = UART_TX_FIFO_SIZE; count > 0; count--)
        {
            UARTx->RB_THR = *pSend_Buf++;
        }
        vCount -= UART_TX_FIFO_SIZE;
    }

    while (UART_GetFlagStatus(UARTx, UART_FLAG_TX_FIFO_EMPTY) == 0);
    while (vCount--)
    {
        UARTx->RB_THR = *pSend_Buf++;
    }
}

/**
  * @brief  Demo code of uart.
  * @param  No parameter.
  * @return void
*/
#include "os_task.h"
#include "os_sched.h"

const uint8_t output[10] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9};

void uart_demo(void *p_param)
{
    uint16_t demoStrLen = 0;

    // init uart0 only for debug, baud rate 115200
    board_uart_init_0();
    driver_uart_init_0();

    // only configure rx irq
//    driver_uart_init();

    char *demoStr = "DATA UART\r\n";

    /* Loop rx and tx */
    while (1)
    {
        os_delay(1000);
        APP_PRINT_INFO0("SEND BUFF");
        os_delay(1000);

        demoStr = "DATA UART\r\n";
        demoStrLen = strlen(demoStr);
        memcpy(String_Buf, demoStr, demoStrLen);
        data_print_buffer(String_Buf, demoStrLen);
    }
}

void *app_task_handle;

void app_task_init()
{
    os_task_create(&app_task_handle, "app", uart_demo, 0, 4 * 1024, 1);
}

/**
 * @brief    System_Handler
 * @note     system handle to judge which pin is wake source
 * @return   void
 */
void System_Handler(void)
{
    APP_PRINT_INFO0("System_Handler");
    NVIC_DisableIRQ(System_IRQn);

    NVIC_ClearPendingIRQ(System_IRQn);
}
/**
 * @brief    PxpEnterDlpsSet
 * @note     enter dlps to set pad&wake up pin
 * @return   void
 */
void PxpEnterDlpsSet(void)
{
//    char *demoStr = "DLPS ENTER\r\n";
//    uint32_t demoStrLen = strlen(demoStr);
//    memcpy(String_Buf, demoStr, demoStrLen);
//    uart_senddata_continuous(UART0, String_Buf, demoStrLen);

#if (AON_WDG_ENABLE == 1)
    aon_wdg_enable();
#endif
}
/**
 * @brief    PxpExitDlpsInit
 * @note     Exit dlps to configure pad
 * @return   void
 */
void PxpExitDlpsInit(void)
{
//    char *demoStr = "DLPS EXIT\r\n";
//    uint32_t demoStrLen = strlen(demoStr);
//    memcpy(String_Buf, demoStr, demoStrLen);
//    uart_senddata_continuous(UART0, String_Buf, demoStrLen);

#if (AON_WDG_ENABLE == 1)
    aon_wdg_disable();
#endif
}
/**
 * @brief    DLPS_PxpCheck
 * @note     check app enter dlps flg.
 * @return   bool
 */
bool DLPS_PxpCheck(void)
{
    return true;
}

/**
 * @brief    Contains the power mode settings
 * @return   void
 */
void pwr_mgr_init(void)
{
    if (false == dlps_check_cb_reg(DLPS_PxpCheck))
    {
        DBG_DIRECT("Error: dlps_check_cb_reg(DLPS_RcuCheck) failed!\n");
    }
    DLPS_IORegUserDlpsEnterCb(PxpEnterDlpsSet);
    DLPS_IORegUserDlpsExitCb(PxpExitDlpsInit);
    DLPS_IORegister();
    lps_mode_set(PLATFORM_DLPS_PFM);
}

/**
  * @brief  main()
  * @param  No parameter.
  * @return void
*/


int main(void)
{
    pwr_mgr_init();
    app_task_init();
    os_sched_start();
}

/**
  * @brief  UART_Handler.
  * @param  No parameter.
  * @return void
*/
//void UART1_Handler()
//{
//    uint16_t lenth = 0;
//    uint32_t int_status = UART_GetIID(UART1);

//    UART_INTConfig(UART1, UART_INT_RD_AVA, DISABLE);

//    if (UART_GetFlagStatus(UART1, UART_FLAG_RX_IDLE) == SET)
//    {
//        char *demoStr = "idle\r\n";
//        uint32_t demoStrLen = strlen(demoStr);
//        memcpy(String_Buf, demoStr, demoStrLen);
//        uart_senddata_continuous(UART0, String_Buf, demoStrLen);

//        UART_INTConfig(UART1, UART_INT_RX_IDLE, DISABLE);

//        lenth = UART_GetRxFIFODataLen(UART1);
//        UART_ReceiveData(UART1, UART_Recv_Buf, lenth);
//        uart_senddata_continuous(UART0, UART_Recv_Buf, lenth);

//        UART_ClearRxFIFO(UART1);
//        UART_INTConfig(UART1, UART_INT_RX_IDLE, ENABLE);
//        receive_flag = true;
//    }

//    switch (int_status & 0x0E)
//    {
//    case UART_INT_ID_RX_DATA_TIMEOUT:
//        {
//            char *demoStr = "RX TIMEOUT\r\n";
//            uint32_t demoStrLen = strlen(demoStr);
//            memcpy(String_Buf, demoStr, demoStrLen);
//            uart_senddata_continuous(UART0, String_Buf, demoStrLen);

//            lenth = UART_GetRxFIFODataLen(UART1);
//            UART_ReceiveData(UART1, UART_Recv_Buf, lenth);
//            uart_senddata_continuous(UART0, UART_Recv_Buf, lenth);
//            break;
//        }
//    case UART_INT_ID_LINE_STATUS:
//        break;

//    case UART_INT_ID_RX_LEVEL_REACH:
//        {
//            char *demoStr = "RX LEVEL REACH\r\n";
//            uint32_t demoStrLen = strlen(demoStr);
//            memcpy(String_Buf, demoStr, demoStrLen);
//            uart_senddata_continuous(UART0, String_Buf, demoStrLen);

//            lenth = UART_GetRxFIFODataLen(UART1);
//            UART_ReceiveData(UART1, UART_Recv_Buf, lenth);
//            uart_senddata_continuous(UART0, UART_Recv_Buf, lenth);
//            break;
//        }
//    case UART_INT_ID_TX_EMPTY:
//        break;

//    case UART_INT_ID_MODEM_STATUS:
//        break;

//    default:
//        break;
//    }

//    UART_INTConfig(UART1, UART_INT_RD_AVA, ENABLE);
//}

