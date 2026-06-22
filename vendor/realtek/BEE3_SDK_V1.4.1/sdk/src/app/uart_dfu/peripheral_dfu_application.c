/**
*********************************************************************************************************
*               Copyright(c) 2022, Realtek Semiconductor Corporation. All rights reserved.
**********************************************************************************************************
* @file     dataTrans_uart.c
* @brief    Data uart operations for testing profiles.
* @details  Data uart init and print data through data uart.
* @author
* @date     2022-01-24
* @version  v0.1
*********************************************************************************************************
*/
#include "peripheral_dfu_application.h"
#include "peripheral_dfu.h"
#include "rtl876x.h"
#include "trace.h"
#include "app_msg.h"
#include "os_msg.h"
#include <app_task.h>
#include "rtl876x_wdg.h"

/******************************************************************
 * @fn       app_handle_io_msg
 * @brief    All the application events are pre-handled in this function.
 *           All the IO MSGs are sent to this function, Then the event handling function
 *           shall be called according to the MSG type.
 *
 * @param    io_driver_msg_recv  - bee io msg data
 * @return   void
 */
void app_handle_io_msg(T_IO_MSG io_driver_msg_recv)
{
    uint16_t msg_type = io_driver_msg_recv.type;

    switch (msg_type)
    {
    case IO_MSG_TYPE_BT_STATUS:
        {
        }
        break;
    case IO_MSG_TYPE_UART:
        {
            peripheral_dfu_handle();
            APP_PRINT_INFO0("IO_MSG_TYPE_UART");
        }
        break;
    case IO_MSG_TYPE_RESET_WDG_TIMER:
        {
            APP_PRINT_INFO0("[WDG] Watch Dog Rset Timer");
            WDG_Restart();
        }
        break;
    default:
        break;
    }
}

/******************* (C) COPYRIGHT 2022 Realtek Semiconductor Corporation *****END OF FILE****/
