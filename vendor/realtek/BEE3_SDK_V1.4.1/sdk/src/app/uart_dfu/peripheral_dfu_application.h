/**
*********************************************************************************************************
*               Copyright(c) 2022, Realtek Semiconductor Corporation. All rights reserved.
**********************************************************************************************************
* @file     dataTrans_uart.h
* @brief    Data uart operations for testing profiles.
* @details  Data uart init and print data through data uart.
* @author
* @date     2022-01-24
* @version  v0.1
*********************************************************************************************************
*/

#ifndef _PERIPHERAL_DFU_APPLICATION_H_
#define _PERIPHERAL_DFU_APPLICATION_H_

#include "rtl876x.h"
#include "app_msg.h"

void app_handle_io_msg(T_IO_MSG io_driver_msg_recv);
void app_nvic_config(void);
void driver_init(void);
#endif

