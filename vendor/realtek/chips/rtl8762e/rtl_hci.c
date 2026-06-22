/****************************************************************************
 * rtl_hci.c
 *
 * Licensed to the Apache Software Foundation (ASF) under one or more
 * contributor license agreements.  See the NOTICE file distributed with
 * this work for additional information regarding copyright ownership.  The
 * ASF licenses this file to you under the Apache License, Version 2.0 (the
 * "License"); you may not use this file except in compliance with the
 * License.  You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
 * WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.  See the
 * License for the specific language governing permissions and limitations
 * under the License.
 *
 ****************************************************************************/

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <trace.h>
#include "hci_if.h"
#include "rtl876x.h"
#include "errno.h"

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

#define H4_NONE 0x00
#define H4_CMD  0x01
#define H4_ACL  0x02
#define H4_SCO  0x03
#define H4_EVT  0x04
#define H4_ISO  0x05

/** HCI specific definitions */
#define HCI_CMD_PKT     0x01
#define HCI_ACL_PKT     0x02
#define HCI_EVT_PKT     0x04

/** Additional buffer offset for HCI communication */
#define HCI_CMD_BUF_OFFSET          7
#define HCI_EVT_BUF_OFFSET          3
#define HCI_TX_ACL_BUF_OFFSET       HCI_TX_ACL_PKT_BUF_OFFSET
#define HCI_RX_ACL_BUF_OFFSET       HCI_RX_ACL_PKT_BUF_OFFSET
#define HCI_TX_ACL_RSVD_SIZE        8

#if defined(CONFIG_RTL876x_BT_DBG)
#define _BT_SNOOP_DOWN_TRACE(...)   BT_SNOOP_DOWN_TRACE(__VA_ARGS__)
#define _BT_SNOOP_UP_TRACE(...)     BT_SNOOP_UP_TRACE(__VA_ARGS__)
#else
#define _BT_SNOOP_DOWN_TRACE(...)
#define _BT_SNOOP_UP_TRACE(...)
#endif

#ifndef BUILD_ASSERT
#define BUILD_ASSERT(EXPR, MSG...) _Static_assert(EXPR, "" MSG)
#endif

#define RTL_BUF_ALIGH_U32(len)  (((len) + 3) & (~3))

#define BT_ACL_IN_COUNT         ( 8 )
#if defined(CONFIG_BT_BUF_ACL_TX_COUNT)
BUILD_ASSERT(CONFIG_BT_BUF_ACL_TX_COUNT <= BT_ACL_IN_COUNT);
#define BT_BUF_ACL_SIZE         RTL_BUF_ALIGH_U32((CONFIG_BT_BUF_ACL_TX_SIZE + 4 + 1 + HCI_TX_ACL_RSVD_SIZE + 1 + HCI_TX_ACL_BUF_OFFSET))
#else
#define BT_BUF_ACL_SIZE         RTL_BUF_ALIGH_U32((255 + 4 + 1 + HCI_TX_ACL_RSVD_SIZE + 1 + HCI_TX_ACL_BUF_OFFSET))
#endif

#if defined(CONFIG_BT_BUF_CMD_TX_SIZE)
#define BT_CMD_SIZE             RTL_BUF_ALIGH_U32((CONFIG_BT_BUF_CMD_TX_SIZE + 3 + 1 + HCI_CMD_BUF_OFFSET + 1))
#else
#define BT_CMD_SIZE             RTL_BUF_ALIGH_U32((255 + 3 + 1 + HCI_CMD_BUF_OFFSET + 1))
#endif

#define BT_CMD_2_COUNT          ( 8 )
#if defined(CONFIG_BT_BUF_ACL_RX_COUNT)
BUILD_ASSERT(CONFIG_BT_BUF_ACL_RX_COUNT <= BT_CMD_2_COUNT);
#endif

#define BT_CMD_2_SIZE           RTL_BUF_ALIGH_U32((16 + 3 + 1 + HCI_CMD_BUF_OFFSET + 1))

struct hci_if_buf {
    uint8_t rptr;
    uint8_t wptr;
    uint8_t mask;
    uint8_t numb;
    uint16_t size;
    uint32_t  *pool;
};

__attribute__((section(".buf.dataon.bss")))
static uint32_t cmd_pool[(1 + (BT_CMD_SIZE / 4)) * 1];
static struct hci_if_buf cmd = {
    .rptr = 0,
    .wptr = 0,
    .mask = 0,
    .numb = 1,
    .size = BT_CMD_SIZE,
    .pool = cmd_pool,
};

__attribute__((section(".buf.dataon.bss")))
static uint32_t cmd2_pool[(1 + (BT_CMD_2_SIZE / 4)) * BT_CMD_2_COUNT];
static struct hci_if_buf cmd2 = {
    .rptr = 0,
    .wptr = 0,
    .mask = BT_CMD_2_COUNT - 1,
    .numb = BT_CMD_2_COUNT,
    .size = BT_CMD_2_SIZE,
    .pool = cmd2_pool,
};

__attribute__((section(".buf.dataon.bss")))
static uint32_t acl_pool[(1 + (BT_BUF_ACL_SIZE / 4)) * BT_ACL_IN_COUNT];
static struct hci_if_buf acl = {
    .rptr = 0,
    .wptr = 0,
    .mask = BT_ACL_IN_COUNT - 1,
    .numb = BT_ACL_IN_COUNT,
    .size = BT_BUF_ACL_SIZE,
    .pool = acl_pool,
};

static uint8_t * bt_buf_alloc(struct hci_if_buf *buf,
                              uint8_t flag, uint16_t size)
{
    uint32_t *p;
    uint8_t off;

    if (size > buf->size) {
        return NULL;
    }

    if ((uint8_t)(buf->wptr - buf->rptr) >= buf->numb) {
	    return NULL;
    }

    off = buf->wptr++ & buf->mask;

    p = &buf->pool[(1 + (buf->size >> 2)) * off];

    p[0] = flag;

    return (uint8_t *)&p[1];
}

static void bt_buf_free(void *buf)
{
    uint32_t *p = buf;
    switch (p[-1])
    {
    case 0x01:
        cmd.rptr++;
        break;
    case 0x02:
        cmd2.rptr++;
        break;
    case 0x03:
        acl.rptr++;
        break;
    default:
        assert_param(false);
    }
}

static uint8_t *bt_cmd_alloc(uint16_t size)
{
	return bt_buf_alloc(&cmd, 0x01, size);
}

static uint8_t *bt_cmd2_alloc(uint16_t size)
{
	return bt_buf_alloc(&cmd2, 0x02, size);
}

static uint8_t *bt_acl_alloc(uint16_t size)
{
	return bt_buf_alloc(&acl, 0x03, size);
}

/****************************************************************************
 * Private Functions
 ****************************************************************************/

extern int bt_vadapter_recv(uint8_t type, uint16_t hdr,
                            const uint8_t *data, uint16_t len);

bool hci_if_callback(T_HCI_IF_EVT evt, bool status, uint8_t *p_buf,
                     uint32_t len)
{
    bool result = true;

    HCI_PRINT_TRACE4("hci_if_callback: evt %d status %d, buf %p, len %u",
                     evt, status, p_buf, len);

    switch (evt)
    {
    case HCI_IF_EVT_DATA_IND:
        {
            switch (p_buf[0])   /* First byte is packet type */
            {
            case HCI_EVT_PKT:
                {
                    uint8_t event = p_buf[1];
                    uint8_t length = p_buf[2];

                   _BT_SNOOP_UP_TRACE(len, p_buf);

                    bt_vadapter_recv(H4_EVT, event,
                                     p_buf + HCI_EVT_BUF_OFFSET,
                                     length);
                    hci_if_confirm(p_buf);
                }
                break;

            case HCI_ACL_PKT:
                {
                    uint16_t handle;
                    uint16_t length;

                    memcpy(&handle, &p_buf[HCI_RX_ACL_BUF_OFFSET + 1], 2);
                    memcpy(&length, &p_buf[HCI_RX_ACL_BUF_OFFSET + 3], 2);

#if defined(CONFIG_RTL876x_BT_DBG)
                   p_buf[HCI_RX_ACL_BUF_OFFSET] = HCI_ACL_PKT;

                   _BT_SNOOP_UP_TRACE(len - HCI_RX_ACL_BUF_OFFSET, p_buf + HCI_RX_ACL_BUF_OFFSET);
#endif

                    bt_vadapter_recv(H4_ACL, handle,
                                     &p_buf[HCI_RX_ACL_BUF_OFFSET + 5],
                                     length);
                }
                break;

            default:
                HCI_PRINT_ERROR1("hci_if_callback: unknown pkt type %d",
                                 p_buf[0]);
                result = false; /* release the packet */
                break;
            }

            if (result == false)
            {
                hci_if_confirm(p_buf);
            }
        }
        break;

    case HCI_IF_EVT_DATA_XMIT:
        {
            uint32_t buffer_addr;
            memcpy(&buffer_addr, p_buf - 4, 4);

            bt_buf_free((void *)buffer_addr);
        }
        break;

    default:
        HCI_PRINT_ERROR1("hci_if_callback: unhandled evt type %d", evt);
        result = false;
        break;
    }

    return result;
}

/****************************************************************************
 * Name: bt_vadapter_init
 *
 * Description:
 *  bt_vadapter_init.
 *
 ****************************************************************************/

int bt_vadapter_init(void)
{
  if (hci_if_open(hci_if_callback) == false)
    {
      HCI_PRINT_ERROR0("hci_if_open: hci_if_open failed");
      return -EINVAL;
    }

  return 0;
}

#define BT_OGF_BASEBAND                         0x03
#define BT_OGF_LE                               0x08

/* Construct OpCode from OGF and OCF */
#define BT_OP(ogf, ocf)                         ((ocf) | ((ogf) << 10))

#define BT_HCI_OP_HOST_NUM_COMPLETED_PACKETS    BT_OP(BT_OGF_BASEBAND, 0x0035)
#define BT_HCI_OP_LE_CONN_PARAM_REQ_NEG_REPLY   BT_OP(BT_OGF_LE, 0x0021)
#define BT_HCI_OP_LE_CONN_PARAM_REQ_REPLY       BT_OP(BT_OGF_LE, 0x0020)
#define BT_HCI_OP_LE_LTK_REQ_NEG_REPLY          BT_OP(BT_OGF_LE, 0x001b)
#define BT_HCI_OP_LE_LTK_REQ_REPLY              BT_OP(BT_OGF_LE, 0x001a)

/****************************************************************************
 * Name: bt_vadapter_send
 *
 * Description:
 *  bt_vadapter_send.
 *
 ****************************************************************************/

int bt_vadapter_send(uint8_t type, const uint8_t *data, uint16_t len)
{
    int ret = 0;
    uint8_t *p_pkt;
    uint8_t pkt_len;
    uint8_t pkt_offset;

    switch (type)
    {
    case H4_ACL:
        {
            pkt_offset = HCI_TX_ACL_BUF_OFFSET;
            pkt_len = len + pkt_offset + 1 + HCI_TX_ACL_RSVD_SIZE;

            p_pkt = (uint8_t *)bt_acl_alloc(pkt_len);

            if (p_pkt == NULL)
            {
                ret = -EINVAL;
                return ret;
            }

            memcpy(p_pkt + pkt_offset, &type, 1);
            memcpy(p_pkt + pkt_offset + HCI_TX_ACL_RSVD_SIZE, &type, 1);
            memcpy(p_pkt + pkt_offset + HCI_TX_ACL_RSVD_SIZE + 1, data, len);

            _BT_SNOOP_DOWN_TRACE(pkt_len - pkt_offset - HCI_TX_ACL_RSVD_SIZE,
                                 p_pkt + pkt_offset + HCI_TX_ACL_RSVD_SIZE);
        }
        break;

    case H4_CMD:
        {
            uint16_t opcode = data[0] | (uint16_t)data[1] << 8;

            pkt_offset = HCI_CMD_BUF_OFFSET;
            pkt_len = len + pkt_offset + 1 ;

            switch (opcode)
            {
            case BT_HCI_OP_HOST_NUM_COMPLETED_PACKETS:
            case BT_HCI_OP_LE_CONN_PARAM_REQ_NEG_REPLY:
            case BT_HCI_OP_LE_CONN_PARAM_REQ_REPLY:
            case BT_HCI_OP_LE_LTK_REQ_NEG_REPLY:
            case BT_HCI_OP_LE_LTK_REQ_REPLY:
                p_pkt = bt_cmd2_alloc(pkt_len);
                break;
            default:
                p_pkt = bt_cmd_alloc(pkt_len);
                break;
            }

            if (p_pkt == NULL)
            {
                ret = -EINVAL;
                return ret;
            }

            memcpy(p_pkt + pkt_offset, &type, 1);
            memcpy(p_pkt + pkt_offset + 1, data, len);

            _BT_SNOOP_DOWN_TRACE(pkt_len - pkt_offset, p_pkt - pkt_offset);

        }
        break;

    default:
        ret = -EINVAL;
        return ret;
    }

    uint32_t buffer_addr = (uint32_t)p_pkt;
    memcpy(p_pkt + pkt_offset - 4, &buffer_addr, 4);

    if (hci_if_write(p_pkt + pkt_offset, pkt_len - pkt_offset) == false)
    {
        bt_buf_free(p_pkt);
        ret = -EIO;
    }

    return ret;
}
