/****************************************************************************
 * arch/arm/src/rtl/rtl_i2c.c
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

#include <nuttx/config.h>

#include <assert.h>
#include <errno.h>
#include <debug.h>

#include "rtl876x_i2c.h"
#include "rtl876x_gdma.h"
#include "rtl876x_rcc.h"
#include "rtl876x_pinmux.h"

#include <nuttx/irq.h>
#include <nuttx/arch.h>
#include <nuttx/mutex.h>
#include <nuttx/kmalloc.h>
#include <nuttx/i2c/i2c_master.h>
#include <arch/board/board.h>

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

#ifdef CONFIG_I2C_POLLED
#error "CONFIG_I2C_POLLED is not supported"
#endif

#ifndef BUILD_ASSERT
#define BUILD_ASSERT(EXPR, MSG...) _Static_assert(EXPR, "" MSG)
#endif

#ifndef CONCAT
#define _CONCAT(_a, _b) _a ## _b
#define CONCAT(a, b) _CONCAT(a, b)
#endif

#define GDMA_PREFIX GDMA_Channel

#if defined(CONFIG_RTL876x_I2C0_MASTER)
#define I2C0_GDMA_CHANNEL_TX \
        CONCAT(GDMA_PREFIX, CONFIG_RTL876x_I2C0_MASTER_DMA_TX_CH)
#define I2C0_GDMA_CHANNEL_RX \
        CONCAT(GDMA_PREFIX, CONFIG_RTL876x_I2C0_MASTER_DMA_RX_CH)

BUILD_ASSERT((I2C0_GDMA_CHANNEL_TX != I2C0_GDMA_CHANNEL_RX));
#endif /* CONFIG_RTL876x_I2C0_MASTER */

/****************************************************************************
 * Private Types
 ****************************************************************************/

/* I2C Device Private Data */

struct rtl_i2c_priv_s
{
  const struct i2c_ops_s *ops;      /* Standard I2C operations */
  I2C_TypeDef            *base;     /* TWI base address */
  I2C_InitTypeDef         Init;      /* Initialization structure */
  uint8_t                 scl_pin;  /* SCL pin configuration */
  uint8_t                 sda_pin;  /* SDA pin configuration */
  uint8_t                 scl_mux;  /* SCL pin mux */
  uint8_t                 sda_mux;  /* SDA pin mux */
  uint32_t                apb_peripheral; /* Peripheral */
  uint32_t                apb_clock;      /* Clock */
  uint32_t                irq;      /* IRQ number */

  GDMA_ChannelTypeDef    *gdma_tx;     /* GDMA channel */
  GDMA_ChannelTypeDef    *gdma_rx;     /* GDMA channel */
  uint8_t                 gdma_channel_tx;  /* DMA Channel */
  uint8_t                 gdma_channel_rx;  /* DMA Channel */
  uint8_t                 gdma_handshake_tx;/* GDMA Handshake */
  uint8_t                 gdma_handshake_rx;/* GDMA Handshake */

  int                     refs;     /* Reference count */
  int                     _errno;   /* I2C transfer status */

  struct i2c_msg_s       *msgv;     /* Message list */
  uint8_t                 idx;      /* Current message buffer */
  uint8_t                 msgc;     /* Message count */
  uint16_t                flags;     /* Current message flags */

  mutex_t                 lock;      /* Mutual exclusion mutex */
  sem_t                   sem_isr;   /* Interrupt wait semaphore */
};

/****************************************************************************
 * Private Function Prototypes
 ****************************************************************************/
static int rtl_i2c_transfer(struct i2c_master_s *dev,
                            struct i2c_msg_s *msgs,
                            int count);
#ifdef CONFIG_I2C_RESET
static int rtl_i2c_reset(struct i2c_master_s *dev);
#endif
#ifndef CONFIG_I2C_POLLED
static int rtl_i2c_isr(int irq, void *context, void *arg);
#endif
static int rtl_i2c_deinit(struct rtl_i2c_priv_s *priv);
static int rtl_i2c_init(struct rtl_i2c_priv_s *priv);

/****************************************************************************
 * Private Data
 ****************************************************************************/

/* I2C operations */

static const struct i2c_ops_s g_rtl_i2c_ops =
{
  .transfer = rtl_i2c_transfer
#ifdef CONFIG_I2C_RESET
  , .reset  = rtl_i2c_reset
#endif
};

/* I2C0 (TWI0) device */

#ifdef CONFIG_RTL876x_I2C0_MASTER
static struct rtl_i2c_priv_s g_rtl_i2c0_priv =
{
  .ops     = &g_rtl_i2c_ops,
  .base    = I2C0,
  .scl_pin = CONFIG_RTL876x_I2C0_MASTER_SCL_PIN,
  .sda_pin = CONFIG_RTL876x_I2C0_MASTER_SDA_PIN,
  .scl_mux = I2C0_CLK,
  .sda_mux = I2C0_DAT,

  .apb_peripheral  = APBPeriph_I2C0,
  .apb_clock       = APBPeriph_I2C0_CLOCK,

  .gdma_tx           = I2C0_GDMA_CHANNEL_TX,
  .gdma_rx           = I2C0_GDMA_CHANNEL_RX,
  .gdma_channel_tx   = CONFIG_RTL876x_I2C0_MASTER_DMA_TX_CH,
  .gdma_channel_rx   = CONFIG_RTL876x_I2C0_MASTER_DMA_RX_CH,
  .gdma_handshake_tx = GDMA_Handshake_I2C0_TX,
  .gdma_handshake_rx = GDMA_Handshake_I2C0_RX,

  .irq     = I2C0_VECTORn,
  .lock    = NXMUTEX_INITIALIZER,
};
#endif

/****************************************************************************
 * Private Functions
 ****************************************************************************/

static void gdma_i2c_master_send(struct rtl_i2c_priv_s *priv, uint16_t *dma_tx_buffer)
{
  int left = priv->msgv->length - 1;
  GDMA_InitTypeDef GDMA_InitStruct;

  /* Initialize data buffer */

  for (uint32_t i = 0; i < left; i++)
    {
      dma_tx_buffer[i] = priv->msgv->buffer[i];
    }

  dma_tx_buffer[left] = priv->msgv->buffer[left] | (0x0001 << 9);

  /* GDMA init */

  GDMA_StructInit(&GDMA_InitStruct);

  GDMA_InitStruct.GDMA_ChannelNum      = priv->gdma_channel_tx;
  GDMA_InitStruct.GDMA_DIR             = GDMA_DIR_MemoryToPeripheral;
  GDMA_InitStruct.GDMA_BufferSize      = priv->msgv->length;
  GDMA_InitStruct.GDMA_SourceInc       = DMA_SourceInc_Inc;
  GDMA_InitStruct.GDMA_DestinationInc  = DMA_DestinationInc_Fix;
  GDMA_InitStruct.GDMA_SourceDataSize  = GDMA_DataSize_HalfWord;
  GDMA_InitStruct.GDMA_DestinationDataSize = GDMA_DataSize_HalfWord;
  GDMA_InitStruct.GDMA_SourceMsize         = GDMA_Msize_1;
  GDMA_InitStruct.GDMA_DestinationMsize    = GDMA_Msize_1;
  GDMA_InitStruct.GDMA_SourceAddr          = (uint32_t)dma_tx_buffer;
  GDMA_InitStruct.GDMA_DestinationAddr     = (uint32_t)(&(priv->base->IC_DATA_CMD));
  GDMA_InitStruct.GDMA_DestHandshake       = priv->gdma_handshake_tx;

  GDMA_Init(priv->gdma_tx, &GDMA_InitStruct);

  /* Start to send data */

  GDMA_Cmd(priv->gdma_channel_tx, ENABLE);
}

static void gdma_i2c_master_read(struct rtl_i2c_priv_s *priv, uint16_t *dma_tx_buffer)
{
  GDMA_InitTypeDef GDMA_InitStruct;

  /* Initialize data buffer */

  for (uint32_t i = 0; i < priv->msgv->length; i++)
    {
      priv->msgv->buffer[i] = 0;
    }

  GDMA_StructInit(&GDMA_InitStruct);

  GDMA_InitStruct.GDMA_ChannelNum      = priv->gdma_channel_rx;;
  GDMA_InitStruct.GDMA_DIR             = GDMA_DIR_PeripheralToMemory;
  GDMA_InitStruct.GDMA_BufferSize      = priv->msgv->length;
  GDMA_InitStruct.GDMA_SourceInc       = DMA_SourceInc_Fix;
  GDMA_InitStruct.GDMA_DestinationInc  = DMA_DestinationInc_Inc;
  GDMA_InitStruct.GDMA_SourceDataSize  = GDMA_DataSize_Byte;
  GDMA_InitStruct.GDMA_DestinationDataSize = GDMA_DataSize_Byte;
  GDMA_InitStruct.GDMA_SourceMsize         = GDMA_Msize_1;
  GDMA_InitStruct.GDMA_DestinationMsize    = GDMA_Msize_1;
  GDMA_InitStruct.GDMA_SourceAddr          = (uint32_t)(&(priv->base->IC_DATA_CMD));
  GDMA_InitStruct.GDMA_DestinationAddr     = (uint32_t)(priv->msgv->buffer);
  GDMA_InitStruct.GDMA_SourceHandshake     = priv->gdma_handshake_rx;

  GDMA_Init(priv->gdma_rx, &GDMA_InitStruct);

  /* Start to receive data */

  GDMA_Cmd(priv->gdma_channel_rx, ENABLE);

  /* Initialize data buffer */

  for (uint32_t i = 0; i < (priv->msgv->length - 1); i++)
    {
      dma_tx_buffer[i] = (0x01 << 8);
    }

  dma_tx_buffer[priv->msgv->length - 1] = (0x0003 << 8);

  /* Initialize GDMA I2C Send Cmd */

  GDMA_StructInit(&GDMA_InitStruct);

  GDMA_InitStruct.GDMA_ChannelNum      = priv->gdma_channel_tx;
  GDMA_InitStruct.GDMA_DIR             = GDMA_DIR_MemoryToPeripheral;
  GDMA_InitStruct.GDMA_BufferSize      = priv->msgv->length;
  GDMA_InitStruct.GDMA_SourceInc       = DMA_SourceInc_Inc;
  GDMA_InitStruct.GDMA_DestinationInc  = DMA_DestinationInc_Fix;
  GDMA_InitStruct.GDMA_SourceDataSize  = GDMA_DataSize_HalfWord;
  GDMA_InitStruct.GDMA_DestinationDataSize = GDMA_DataSize_HalfWord;
  GDMA_InitStruct.GDMA_SourceMsize         = GDMA_Msize_1;
  GDMA_InitStruct.GDMA_DestinationMsize    = GDMA_Msize_1;
  GDMA_InitStruct.GDMA_SourceAddr          = (uint32_t)dma_tx_buffer;
  GDMA_InitStruct.GDMA_DestinationAddr     = (uint32_t)(&(priv->base->IC_DATA_CMD));
  GDMA_InitStruct.GDMA_DestHandshake       = priv->gdma_handshake_tx;

  GDMA_Init(priv->gdma_tx, &GDMA_InitStruct);

  GDMA_Cmd(priv->gdma_channel_tx, ENABLE);
}

/****************************************************************************
 * Name: rtl_i2c_transfer
 *
 * Description:
 *   Generic I2C transfer function
 *
 ****************************************************************************/

static int rtl_i2c_transfer(struct i2c_master_s *dev,
                            struct i2c_msg_s *msgs,
                            int count)
{
  struct rtl_i2c_priv_s *priv = (struct rtl_i2c_priv_s *)dev;
  const struct timespec ts = {
        .tv_sec  = CONFIG_RTL876x_I2C_TRANSFER_TIMEOUT,
  };
  uint16_t *dma_tx_buffer = NULL;
  uint16_t length;
  int ret = OK;

  ret = nxmutex_lock(&priv->lock);
  if (ret < 0)
    {
      return ret;
    }

  priv->idx  = 0;
  priv->msgv = msgs;
  priv->msgc = count;

  /* Reset I2C transfer status */

  priv->_errno = OK;

  /* Initialize semaphore */

  nxsem_init(&priv->sem_isr, 0, 0);

  i2cinfo("I2C TRANSFER count=%d\n", count);

  /* Do we need change I2C bus frequency ? */

  if (priv->msgv->frequency != priv->Init.I2C_ClockSpeed ||
      priv->msgv->addr != priv->Init.I2C_SlaveAddress)
    {
      /* Save the new I2C frequency */

      priv->Init.I2C_ClockSpeed = priv->msgv->frequency;

      /* Save the new I2C slave address */

      priv->Init.I2C_SlaveAddress = priv->msgv->addr;

      priv->Init.I2C_Ack     = I2C_Ack_Enable;
      priv->Init.I2C_RxDmaEn = ENABLE;
      priv->Init.I2C_TxDmaEn = ENABLE;
      priv->Init.I2C_RxWaterlevel = 8;
      priv->Init.I2C_TxWaterlevel = 8;

      /* Set I2C address mode */

      if (priv->msgv->flags & I2C_M_TEN)
        {
          priv->Init.I2C_AddressMode = I2C_AddressMode_10BIT;
        }
      else
        {
          priv->Init.I2C_AddressMode = I2C_AddressMode_7BIT;
        }

      i2cinfo("I2C TRANSFER frequency=%ld addr=0x%x\n",
               priv->msgv->frequency, priv->msgv->addr);

      /* I2C init */

      I2C_Init(priv->base, &priv->Init);

      I2C_ClearINTPendingBit(priv->base, I2C_INT_STOP_DET);
      I2C_ClearINTPendingBit(priv->base, I2C_INT_TX_ABRT);
      I2C_ClearINTPendingBit(priv->base, I2C_INT_RX_OVER);

      I2C_INTConfig(priv->base, I2C_INT_STOP_DET |
                                I2C_INT_TX_ABRT |
                                I2C_INT_RX_OVER, ENABLE);

      /* Enable I2C */

      I2C_Cmd(priv->base, ENABLE);
    }

  /* I2C transfer */

  do
    {
      length = priv->msgv->length;
      if (length == 0)
        {
          i2cerr("I2C TRANSFER length=0\n");
          priv->_errno = -EINVAL;
          goto errout;
        }

      /* Allocate DMA buffer */

      dma_tx_buffer = kmm_malloc(length * sizeof(uint16_t));
      if (dma_tx_buffer == NULL)
        {
          i2cerr("I2C TRANSFER kmm_malloc failed\n");
          priv->_errno = -ENOMEM;
          goto errout;
        }

      /* Get current message data */

      priv->flags = priv->msgv->flags;

      if (priv->flags & I2C_M_READ)
        {
          i2cinfo("I2C TRANSFER read length=%d\n", length);

          gdma_i2c_master_read(priv, dma_tx_buffer);

          /* Wait for last TX event */

          ret = nxsem_tickwait_uninterruptible(&priv->sem_isr, timespec_to_tick(&ts));
          if (ret != OK)
            {
              i2cerr("I2C TRANSFER gdma_i2c_master_read timeout\n");
              goto errout;
            }

          if (priv->_errno != OK)
            {
              i2cerr("I2C TRANSFER gdma_i2c_master_read failed\n");
              goto errout;
            }

          i2cinfo("I2C TRANSFER read successful\n");
        }
      else
        {
          i2cinfo("I2C TRANSFER write length=%d\n", length);

          gdma_i2c_master_send(priv, dma_tx_buffer);

          /* Wait for last TX event */

          ret = nxsem_tickwait_uninterruptible(&priv->sem_isr, timespec_to_tick(&ts));
          if (ret != OK)
            {
              i2cerr("I2C TRANSFER gdma_i2c_master_send timeout\n");
              goto errout;
            }

          if (priv->_errno != OK)
            {
              i2cerr("I2C TRANSFER gdma_i2c_master_send failed\n");
              goto errout;
            }

          i2cinfo("I2C TRANSFER write successful\n");
        }

      kmm_free(dma_tx_buffer);
      dma_tx_buffer = NULL;

      /* Next message */

      priv->idx  += 1;
      priv->msgv += 1;
    }
  while (priv->idx < priv->msgc);

errout:
  if (dma_tx_buffer != NULL)
    {
      kmm_free(dma_tx_buffer);
    }

  if (priv->_errno != OK)
    {
      ret = priv->_errno;
    }

  nxmutex_unlock(&priv->lock);

  return ret;
}

/****************************************************************************
 * Name: rtl_i2c_reset
 *
 * Description:
 *   Perform an I2C bus reset in an attempt to break loose stuck I2C devices.
 *
 * Input Parameters:
 *   dev   - Device-specific state data
 *
 * Returned Value:
 *   Zero (OK) on success; a negated errno value on failure.
 *
 ****************************************************************************/

#ifdef CONFIG_I2C_RESET
static int rtl_i2c_reset(struct i2c_master_s *dev)
{
#error not implemented
}
#endif

static const char * const i2c_abort_reasons[] =
{
  "Success",
  "Arbitration Lost",
  "Master Disabled",
  "Data TX No Ack",
  "10bit Address 2 No Ack",
  "10bit Address 1 No Ack",
  "7bit Address No Ack",
  "I2C Error Timeout"
};

/****************************************************************************
 * Name: rtl_i2c_isr
 *
 * Description:
 *   Common I2C interrupt service routine
 *
 ****************************************************************************/

static int rtl_i2c_isr(int irq, void *context, void *arg)
{
  struct rtl_i2c_priv_s *priv = (struct rtl_i2c_priv_s *)arg;

  /* Reset I2C status */

  if(I2C_GetINTStatus(priv->base, I2C_INT_TX_ABRT) == SET)
    {
      I2C_Status status = I2C_CheckAbortStatus(priv->base);

      if (status == I2C_ARB_LOST)
        {
          priv->_errno = -ENOTCONN;
        }
      else if (status == I2C_ABRT_TXDATA_NOACK ||
               status == I2C_ABRT_10ADDR2_NOACK ||
               status == I2C_ABRT_10ADDR1_NOACK ||
               status == I2C_ABRT_7B_ADDR_NOACK)
        {
          priv->_errno = -EAGAIN;
        }
      else
        {
          priv->_errno = -EIO;
        }

      GDMA_Cmd(priv->gdma_channel_tx, DISABLE);
      GDMA_Cmd(priv->gdma_channel_rx, DISABLE);

      i2cerr("I2C ABRT: %s\n", i2c_abort_reasons[status]);

      I2C_ClearINTPendingBit(priv->base, I2C_INT_TX_ABRT);
    }
  else if (I2C_GetINTStatus(priv->base, I2C_INT_RX_OVER) == SET)
    {
      priv->_errno = -EAGAIN;

      i2cerr("I2C RX OVER\n");

      I2C_ClearINTPendingBit(priv->base, I2C_INT_RX_OVER);
    }
  else if (I2C_GetINTStatus(priv->base, I2C_INT_STOP_DET) == SET)
    {
      i2cinfo("I2C STOP DETECT\n");

      nxsem_post(&priv->sem_isr);

      I2C_ClearINTPendingBit(priv->base, I2C_INT_STOP_DET);
    }
  else
    {
      i2cerr("I2C UND(%08lx)\n", priv->base->IC_INTR_STAT);
      I2C_ClearAllINT(priv->base);
    }

  return OK;
}

/****************************************************************************
 * Name: rtl_i2c_init
 *
 * Description:
 *   Setup the I2C hardware, ready for operation with defaults
 *
 ****************************************************************************/

static int rtl_i2c_init(struct rtl_i2c_priv_s *priv)
{
  RCC_PeriphClockCmd(APBPeriph_GDMA, APBPeriph_GDMA_CLOCK, ENABLE);

  RCC_PeriphClockCmd(priv->apb_peripheral, priv->apb_clock, ENABLE);

  Pad_Config(priv->scl_pin, PAD_PINMUX_MODE, PAD_IS_PWRON, PAD_PULL_UP, PAD_OUT_ENABLE,
             PAD_OUT_HIGH);
  Pad_Config(priv->sda_pin, PAD_PINMUX_MODE, PAD_IS_PWRON, PAD_PULL_UP, PAD_OUT_ENABLE,
             PAD_OUT_HIGH);

  Pinmux_Config(priv->scl_pin, priv->scl_mux);
  Pinmux_Config(priv->sda_pin, priv->sda_mux);

  I2C_StructInit(&priv->Init);
  priv->Init.I2C_Ack     = I2C_Ack_Enable;
  priv->Init.I2C_RxDmaEn = ENABLE;
  priv->Init.I2C_TxDmaEn = ENABLE;
  priv->Init.I2C_RxWaterlevel = 8;
  priv->Init.I2C_TxWaterlevel = 8;

  I2C_Init(priv->base, &priv->Init);

  I2C_ClearINTPendingBit(priv->base, I2C_INT_STOP_DET);
  I2C_ClearINTPendingBit(priv->base, I2C_INT_TX_ABRT);
  I2C_ClearINTPendingBit(priv->base, I2C_INT_RX_OVER);

  I2C_INTConfig(priv->base, I2C_INT_STOP_DET |
                            I2C_INT_TX_ABRT |
                            I2C_INT_RX_OVER, ENABLE);

  I2C_Cmd(priv->base, ENABLE);

  irq_attach(priv->irq, rtl_i2c_isr, priv);

  up_enable_irq(priv->irq);

  return OK;
}

/****************************************************************************
 * Name: rtl_i2c_deinit
 *
 * Description:
 *   Shutdown the I2C hardware
 *
 ****************************************************************************/

static int rtl_i2c_deinit(struct rtl_i2c_priv_s *priv)
{
  GDMA_Cmd(priv->gdma_channel_tx, DISABLE);
  GDMA_Cmd(priv->gdma_channel_rx, DISABLE);

  RCC_PeriphClockCmd(APBPeriph_GDMA, APBPeriph_GDMA_CLOCK, DISABLE);

  Pad_Config(priv->scl_pin, PAD_SW_MODE, PAD_IS_PWRON, PAD_PULL_NONE, PAD_OUT_DISABLE,
             PAD_OUT_LOW);
  Pad_Config(priv->sda_pin, PAD_SW_MODE, PAD_IS_PWRON, PAD_PULL_NONE, PAD_OUT_DISABLE,
             PAD_OUT_LOW);

  Pinmux_Deinit(priv->scl_pin);
  Pinmux_Deinit(priv->sda_pin);

  I2C_INTConfig(priv->base, I2C_INT_STOP_DET |
                            I2C_INT_TX_ABRT |
                            I2C_INT_RX_OVER, DISABLE);

  I2C_StructInit(&priv->Init);
  I2C_Cmd(priv->base, DISABLE);

  RCC_PeriphClockCmd(priv->apb_peripheral, priv->apb_clock, DISABLE);

  up_disable_irq(priv->irq);

  irq_detach(priv->irq);

  return OK;
}

/****************************************************************************
 * Public Functions
 ****************************************************************************/

/****************************************************************************
 * Name: rtl_i2cbus_initialize
 *
 * Description:
 *   Initialize one I2C bus
 *
 ****************************************************************************/

struct i2c_master_s *rtl_i2cbus_initialize(int port)
{
  struct rtl_i2c_priv_s *priv = NULL;

  i2cinfo("I2C INITIALIZE port=%d\n", port);

  /* Get interface */

  switch (port)
    {
#ifdef CONFIG_RTL876x_I2C0_MASTER
      case 0:
        {
          priv = (struct rtl_i2c_priv_s *)&g_rtl_i2c0_priv;
          break;
        }
#endif

      default:
        {
          return NULL;
        }
    }

  /* Initialize private data for the first time, increment reference count,
   * power-up hardware and configure GPIOs.
   */

  nxmutex_lock(&priv->lock);
  if (priv->refs++ == 0)
    {
      /* Initialize I2C */
      rtl_i2c_init(priv);
    }

  nxmutex_unlock(&priv->lock);
  return (struct i2c_master_s *)priv;
}

/****************************************************************************
 * Name: rtl_i2cbus_uninitialize
 *
 * Description:
 *   Uninitialize an I2C bus
 *
 ****************************************************************************/

int rtl_i2cbus_uninitialize(struct i2c_master_s *dev)
{
  struct rtl_i2c_priv_s *priv = (struct rtl_i2c_priv_s *)dev;

  DEBUGASSERT(dev);

  /* Decrement reference count and check for underflow */

  if (priv->refs == 0)
    {
      return ERROR;
    }

  nxmutex_lock(&priv->lock);
  if (--priv->refs)
    {
      nxmutex_unlock(&priv->lock);
      return OK;
    }

  /* Disable power and other HW resource (GPIO's) */

  rtl_i2c_deinit(priv);
  nxmutex_unlock(&priv->lock);

  return OK;
}
