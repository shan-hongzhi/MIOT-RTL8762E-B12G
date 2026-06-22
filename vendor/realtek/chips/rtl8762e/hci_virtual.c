/**
  *************************************************************************************************
  * @file    hci_virtual.c
  * @author
  * @version V0.0.1
  * @date    2016-11-9
  * @brief   This file contains all the functions regarding flash device.
  *          This file is to re_rewrite ext_flash module, so use FLASH_DEVICE_ENABLE to use new
  *          flash api, EXT_FLASH_EN will still exist until new api is verified OK.
  *************************************************************************************************
  * @attention
  *
  * This module is a confidential and proprietary property of RealTek and
  * possession or use of this module requires written permission of RealTek.
  *
  * Copyright(c) 2016, Realtek Semiconductor Corporation. All rights reserved.
  *************************************************************************************************/

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include <stdint.h>
#include <trace.h>
#include "hci_if.h"
#include <vhci.h>

bool hci_if_open(P_HCI_IF_CALLBACK p_callback)
{
    return __rtl_vhci_open(p_callback);
}

bool hci_if_close(void)
{
    return vhci_close();
}

bool hci_if_write(uint8_t *p_buf, uint32_t len)
{
    return __rtl_vhci_send(p_buf, len);
}

bool hci_if_confirm(uint8_t *p_buf)
{
    return __rtl_vhci_ack(p_buf);
}
