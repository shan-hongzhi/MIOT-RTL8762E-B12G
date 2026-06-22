/****************************************************************************
 * arch/arm/src/rtl8762e/rtl_serial.c
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

#include "rtl_serial.h"
#if defined(CONFIG_SERIAL_RXDMA) || defined(CONFIG_SERIAL_TXDMA)
#include "rtl876x_gdma.h"
#endif
#ifdef CONFIG_PM
#include <nuttx/power/pm.h>
#include "dlps.h"
#endif

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

#ifdef CONFIG_RTL876x_UART

#define RTL876x_NUART 2
#if defined(CONFIG_RTL876x_UART0) || defined(CONFIG_RTL876x_UART1)
#define HAVE_UART 1
#endif

#if defined(CONFIG_RTL876x_UART0_CONSOLE)
#undef CONFIG_RTL876x_UART1_SERIAL_CONSOLE
#define CONSOLE_UART 0
#elif defined(CONFIG_RTL876x_UART1_CONSOLE)
#undef CONFIG_RTL876x_UART0_CONSOLE
#define CONSOLE_UART 1
#endif
#ifdef CONFIG_PM
#define SYSBLK_ACTCK_UART0DATA_EN_Pos (0UL)
#define SYSBLK_ACTCK_UART0DATA_EN_Msk \
                              (0x1UL << SYSBLK_ACTCK_UART0DATA_EN_Pos)
#define SYSBLK_SLPCK_UART0DATA_EN_Pos (1UL)
#define SYSBLK_SLPCK_UART0DATA_EN_Msk \
                              (0x1UL << SYSBLK_SLPCK_UART0DATA_EN_Pos)
#define SYSBLK_ACTCK_UART0DATA_EN_Pos (0UL)
#define SYSBLK_ACTCK_UART0DATA_EN_Msk \
                               (0x1UL << SYSBLK_ACTCK_UART0DATA_EN_Pos)
#define SYSBLK_SLPCK_UART0DATA_EN_Pos (1UL)
#define SYSBLK_SLPCK_UART0DATA_EN_Msk \
                               (0x1UL << SYSBLK_SLPCK_UART0DATA_EN_Pos)
#endif

/****************************************************************************
 * Private Types
 ****************************************************************************/

struct up_dev_s
{
  UART_TypeDef      *uart_base;   /* Base address of UART registers */
  bool              initialized;  /* Has been initialized and HW is setup. */
  uint32_t          baud;         /* Configured baud */
  uint8_t           parity;       /* 0=none, 1=odd, 2=even */
  uint8_t           bits;         /* Number of bits (7 or 8) */
  bool              stopbits2;    /* True: Configure with 2 stop bits */
  const uint32_t    irq;          /* IRQ associated with this UART */
  xcpt_t            handler;      /* Handler associated with this UART */
  bool              attached;     /* IRQ attach status bit */
  uint32_t          ie;           /* Saved interrupt mask bits value */
  uint32_t          sr;           /* Saved status bits */
  const uint32_t    tx_gpio;      /* UART TX GPIO pin configuration */
  const uint32_t    rx_gpio;      /* UART RX GPIO pin configuration */
  bool                rxdma_used;
  bool                txdma_used;
#if defined(CONFIG_SERIAL_RXDMA)
  GDMA_ChannelTypeDef *rxdma_base;
  uint8_t             rxdma_channel_num;
  xcpt_t              rxdma_handler;
  uint32_t            rxdma_irq; 
  uint32_t            rxdma_handshake;
#endif
#if defined(CONFIG_SERIAL_TXDMA)
  GDMA_ChannelTypeDef *txdma_base;
  uint8_t             txdma_channel_num;
  xcpt_t              txdma_handler;
  uint32_t            txdma_irq; 
  uint32_t            txdma_handshake;
  bool                txdma_running;
#endif
};

/****************************************************************************
 * Private Function Prototypes
 ****************************************************************************/

#if defined(RTL876x_UART_HAVE_NODMA)
static int  rtl876x_serial_setup(struct uart_dev_s *dev);
#endif

#if defined(RTL876x_UART_HAVE_RXNODMA)
static void rtl876x_serial_rxint(struct uart_dev_s *dev, bool enable);
static int  rtl876x_serial_receive(struct uart_dev_s *dev,
                                                    unsigned int *status);
#endif

#if defined(RTL876x_UART_HAVE_TXNODMA)
static void rtl876x_serial_txint(struct uart_dev_s *dev, bool enable);
#endif

static void rtl876x_serial_send(struct uart_dev_s *dev, int ch);
static void rtl876x_serial_shutdown(struct uart_dev_s *dev);
static int  rtl876x_serial_attach(struct uart_dev_s *dev);
static void rtl876x_serial_detach(struct uart_dev_s *dev);
static int  rtl876x_serial_ioctl(struct file *filep, int cmd,
                                                       unsigned long arg);
static bool rtl876x_serial_rxavailable(struct uart_dev_s *dev);
static bool rtl876x_serial_txready(struct uart_dev_s *dev);
static bool rtl876x_serial_txempty(struct uart_dev_s *dev);
static void rtl876x_serial_switch_config(struct up_dev_s *dev);
static void rtl876x_uart_int_process(struct uart_dev_s *dev);
static void rtl876x_uart_handler(struct uart_dev_s *dev);
#ifdef CONFIG_RTL876x_UART0
static int uart0_handler(int irq, void *context, void *arg);
#endif
#ifdef CONFIG_RTL876x_UART1
static int uart1_handler(int irq, void *context, void *arg);
#endif


#if !defined(RTL876x_UART_NO_DMA)
static int  rtl876x_serial_dma_setup(struct uart_dev_s *dev);
#endif

#if defined(RTL876x_UART_HAVE_RXDMA)
static void rtl876x_serial_dma_rxint(struct uart_dev_s *dev, bool enable);
static void rtl876x_serial_dma_receive(struct uart_dev_s *dev);
static void rtl876x_serial_dma_rxfree(struct uart_dev_s *dev);
static void rtl876x_uart_rxdma_handler(struct uart_dev_s *dev);
static void rtl876x_uart_int_rxdmaprocess(struct uart_dev_s *dev);
static void rtl876x_rxdma_set_block(struct uart_dev_s *dev);
#if defined(CONFIG_SERIAL_RXDMA) && defined(CONFIG_RTL876x_UART0_RXDMA)
static int uart0_rxdma_handler(int irq, void *context, void *arg);
#endif
#if defined(CONFIG_SERIAL_RXDMA) && defined(CONFIG_RTL876x_UART1_RXDMA)
static int uart1_rxdma_handler(int irq, void *context, void *arg);
#endif
#endif

#if defined(RTL876x_UART_HAVE_TXDMA)
static void rtl876x_serial_dma_txint(struct uart_dev_s *dev, bool enable);
static void rtl876x_serial_dma_send(struct uart_dev_s *dev);
static void rtl876x_serial_dma_txavail(struct uart_dev_s *dev);

static void rtl876x_uart_txdma_handler(struct uart_dev_s *dev);
static void rtl876x_txdma_set_block(struct uart_dev_s *dev);
#if defined(CONFIG_SERIAL_TXDMA) && defined(CONFIG_RTL876x_UART0_TXDMA)
static int uart0_txdma_handler(int irq, void *context, void *arg);
#endif
#if defined(CONFIG_SERIAL_TXDMA) && defined(CONFIG_RTL876x_UART1_TXDMA)
static int uart1_txdma_handler(int irq, void *context, void *arg);
#endif
#endif

#ifdef CONFIG_PM
#if defined(CONFIG_RTL876x_UART0) || defined(CONFIG_RTL876x_UART1) 
static void uart_dlps_reg_store(uint32_t *p_buf, UART_TypeDef *UARTx);
static void uart_dlps_reg_restore(uint32_t *p_buf, UART_TypeDef *UARTx);
#endif
#if defined(RTL876x_UART0_DMA_USED) || defined(RTL876x_UART1_DMA_USED)
static void dma_dlps_reg_clear(GDMA_TypeDef *GDMAx);
static void dma_dlps_reg_store(uint32_t *p_buf, GDMA_TypeDef *GDMAx);
static void dma_dlps_reg_restore(uint32_t *p_buf, GDMA_TypeDef *GDMAx);
static void dma_channel_dlps_reg_store(uint32_t *p_buf, GDMA_ChannelTypeDef *GDMA_Channelx);
static void dma_channel_dlps_reg_restore(uint32_t *p_buf, GDMA_ChannelTypeDef *GDMA_Channelx);
volatile static uint32_t gdma_storereg[7];
#endif
#ifdef CONFIG_RTL876x_UART0
#if defined(CONFIG_SERIAL_RXDMA) && defined(CONFIG_RTL876x_UART0_RXDMA)
volatile static uint32_t uart0_rxdma_storereg[6];
#endif
#if defined(CONFIG_SERIAL_TXDMA) && defined(CONFIG_RTL876x_UART0_TXDMA)
volatile static uint32_t uart0_txdma_storereg[6];
#endif
volatile static uint32_t uart0_storereg[12];
#endif

#ifdef CONFIG_RTL876x_UART1
#if defined(CONFIG_SERIAL_RXDMA) && defined(CONFIG_RTL876x_UART1_RXDMA)
volatile static uint32_t uart1_rxdma_storereg[6];
#endif
#if defined(CONFIG_SERIAL_TXDMA) && defined(CONFIG_RTL876x_UART1_TXDMA)
volatile static uint32_t uart1_txdma_storereg[6];
#endif
volatile static uint32_t uart1_storereg[12];
#endif
#endif
/****************************************************************************
 * Private Data
 ****************************************************************************/

#if defined(RTL876x_UART_HAVE_NODMA)
static const struct uart_ops_s g_uart_ops =
{
  .setup          = rtl876x_serial_setup,
  .shutdown       = rtl876x_serial_shutdown,
  .attach         = rtl876x_serial_attach,
  .detach         = rtl876x_serial_detach,
  .ioctl          = rtl876x_serial_ioctl,
  .receive        = rtl876x_serial_receive,
  .rxint          = rtl876x_serial_rxint,
  .rxavailable    = rtl876x_serial_rxavailable,
  .send           = rtl876x_serial_send,
  .txint          = rtl876x_serial_txint,
  .txready        = rtl876x_serial_txready,
  .txempty        = rtl876x_serial_txempty,
};
#endif

#if (defined(CONFIG_RTL876x_UART0_TXDMA) && \
   !defined(CONFIG_RTL876x_UART0_RXDMA)) || \
   (defined(CONFIG_RTL876x_UART1_TXDMA) && \
   !defined(CONFIG_RTL876x_UART1_RXDMA))
static const struct uart_ops_s g_uart_txdma_ops =
{
  .setup          = rtl876x_serial_dma_setup,
  .shutdown       = rtl876x_serial_shutdown,
  .attach         = rtl876x_serial_attach,
  .detach         = rtl876x_serial_detach,
  .ioctl          = rtl876x_serial_ioctl,
  .receive        = rtl876x_serial_receive,
  .rxint          = rtl876x_serial_rxint,
  .rxavailable    = rtl876x_serial_rxavailable,
  .send           = rtl876x_serial_send,
  .dmasend        = rtl876x_serial_dma_send,
  .txint          = rtl876x_serial_dma_txint,
  .dmatxavail     = rtl876x_serial_dma_txavail,
  .txready        = rtl876x_serial_txready,
  .txempty        = rtl876x_serial_txempty,
};
#endif

#if (defined(CONFIG_RTL876x_UART0_RXDMA) && \
   !defined(CONFIG_RTL876x_UART0_TXDMA)) || \
   (defined(CONFIG_RTL876x_UART1_RXDMA) && \
   !defined(CONFIG_RTL876x_UART1_TXDMA))
static const struct uart_ops_s g_uart_rxdma_ops =
{
  .setup          = rtl876x_serial_dma_setup,
  .shutdown       = rtl876x_serial_shutdown,
  .attach         = rtl876x_serial_attach,
  .detach         = rtl876x_serial_detach,
  .ioctl          = rtl876x_serial_ioctl,
  .dmareceive     = rtl876x_serial_dma_receive,
  .dmarxfree      = rtl876x_serial_dma_rxfree,
  .rxint          = rtl876x_serial_dma_rxint,
  .rxavailable    = rtl876x_serial_rxavailable,
  .send           = rtl876x_serial_send,
  .txint          = rtl876x_serial_txint,
  .txready        = rtl876x_serial_txready,
  .txempty        = rtl876x_serial_txempty,
};
#endif

#if defined(RTL876x_UART_HAVE_TXRXDMA)
static const struct uart_ops_s g_uart_txrxdma_ops =
{
  .setup          = rtl876x_serial_dma_setup,
  .shutdown       = rtl876x_serial_shutdown,
  .attach         = rtl876x_serial_attach,
  .detach         = rtl876x_serial_detach,
  .ioctl          = rtl876x_serial_ioctl,
  .dmareceive     = rtl876x_serial_dma_receive,
  .dmarxfree      = rtl876x_serial_dma_rxfree,
  .rxint          = rtl876x_serial_dma_rxint,
  .rxavailable    = rtl876x_serial_rxavailable,
  .send           = rtl876x_serial_send,
  .dmasend        = rtl876x_serial_dma_send,
  .txint          = rtl876x_serial_dma_txint,
  .dmatxavail     = rtl876x_serial_dma_txavail,
  .txready        = rtl876x_serial_txready,
  .txempty        = rtl876x_serial_txempty,
};
#endif

typedef struct
{
    uint16_t div;
    uint16_t ovsr;
    uint16_t ovsr_adj;
} uart_baudrate_t;

const uart_baudrate_t baudrate_table[11] =
{
    {271, 10, 0x24a}, /* BAUD_RATE_9600    */
    {150, 8,  0x3ef}, /* BAUD_RATE_19200   */
    {20, 12,  0x252}, /* BAUD_RATE_115200  */
    {11,  10, 0x3bb}, /* BAUD_RATE_230400  */
    {11,  9,  0x084}, /* BAUD_RATE_256000  */
    {7,   9,  0x3ef}, /* BAUD_RATE_384000  */
    {6,   9,  0x0aa}, /* BAUD_RATE_460800  */
    {3,   9,  0x0aa}, /* BAUD_RATE_921600  */
    {4,   5,  0},     /* BAUD_RATE_1000000 */
    {2,   5,  0},     /* BAUD_RATE_2000000 */
    {1,   8,  0x292}, /* BAUD_RATE_3000000 */
};

/* I/O buffers */

#ifdef CONFIG_RTL876x_UART0
#if defined(CONFIG_SERIAL_RXDMA) && defined(CONFIG_RTL876x_UART0_RXDMA)
static char g_uart0dmarxbuffer[CONFIG_RTL876x_UART0_DMA_RXBUFSIZE];
#else
static char g_uart0rxbuffer[CONFIG_RTL876x_UART0_RXBUFSIZE];
#endif
#if defined(CONFIG_SERIAL_TXDMA) && defined(CONFIG_RTL876x_UART0_TXDMA)
static char g_uart0dmatxbuffer[CONFIG_RTL876x_UART0_DMA_TXBUFSIZE];
#else
static char g_uart0txbuffer[CONFIG_RTL876x_UART0_TXBUFSIZE];
#endif
# endif

#ifdef CONFIG_RTL876x_UART1
#if defined(CONFIG_SERIAL_RXDMA) && defined(CONFIG_RTL876x_UART1_RXDMA)
static char g_uart1dmarxbuffer[CONFIG_RTL876x_UART1_DMA_RXBUFSIZE];
#else
static char g_uart1rxbuffer[CONFIG_RTL876x_UART1_RXBUFSIZE];
# endif
#if defined(CONFIG_SERIAL_TXDMA) && defined(CONFIG_RTL876x_UART1_TXDMA)
static char g_uart1dmatxbuffer[CONFIG_RTL876x_UART1_DMA_TXBUFSIZE];
#else
static char g_uart1txbuffer[CONFIG_RTL876x_UART1_TXBUFSIZE];
#endif
#endif

#ifdef CONFIG_RTL876x_UART0
static  struct up_dev_s  g_uart0priv =
{
  .uart_base     = UART0,
  .initialized   = false,
  .irq           = Uart0_VECTORn,
  .handler       = uart0_handler,
  .attached      = false,
  .ie            = 0,
  .sr            = 0,
  .tx_gpio       = CONFIG_RTL876x_UART0_TX_PIN,
  .rx_gpio       = CONFIG_RTL876x_UART0_RX_PIN,
#if defined(CONFIG_SERIAL_RXDMA) && defined(CONFIG_RTL876x_UART0_RXDMA)
  .rxdma_used    = true,
  .rxdma_channel_num = CONFIG_RTL876x_UART0_RXDMA_CH,
  .rxdma_handler     = uart0_rxdma_handler,
  .rxdma_handshake   = GDMA_Handshake_UART0_RX,
#endif
#if defined(CONFIG_SERIAL_TXDMA) && defined(CONFIG_RTL876x_UART0_TXDMA)
  .txdma_used    = true,
  .txdma_channel_num = CONFIG_RTL876x_UART0_TXDMA_CH,
  .txdma_handler     = uart0_txdma_handler,
  .txdma_handshake   = GDMA_Handshake_UART0_TX,
#endif
};

static uart_dev_t g_uart0port =
{
#if CONSOLE_UART == 0
  .isconsole = true,
#endif
#if defined(CONFIG_SERIAL_RXDMA) && defined(CONFIG_RTL876x_UART0_RXDMA)
  .dmarx     =
  {
    .buffer  = g_uart0dmarxbuffer,
    .length  = CONFIG_RTL876x_UART0_DMA_RXBUFSIZE,
    .nbuffer = NULL,
    .nlength = 0,
    .nbytes  = 0,
  },
#endif
#if defined(CONFIG_SERIAL_TXDMA) && defined(CONFIG_RTL876x_UART0_TXDMA)
  .dmatx     =
  {
    .buffer  = g_uart0dmatxbuffer,
    .length  = CONFIG_RTL876x_UART0_DMA_TXBUFSIZE,
    .nbuffer = NULL,
    .nlength = 0,
    .nbytes  = 0,
  },
#endif
  .recv      =
  {
#if defined(CONFIG_SERIAL_RXDMA) && defined(CONFIG_RTL876x_UART0_RXDMA)
    .size    = CONFIG_RTL876x_UART0_DMA_RXBUFSIZE,
    .buffer  = g_uart0dmarxbuffer,
    .head    = 0,
    .tail    = 0,
#else
    .size    = CONFIG_RTL876x_UART0_RXBUFSIZE,
    .buffer  = g_uart0rxbuffer,
    .head    = 0,
    .tail    = 0,
#endif
  },
  .xmit      =
  {
#if defined(CONFIG_SERIAL_TXDMA) && defined(CONFIG_RTL876x_UART0_TXDMA)
    .size    = CONFIG_RTL876x_UART0_DMA_TXBUFSIZE,
    .buffer  = g_uart0dmatxbuffer,
    .head    = 0,
    .tail    = 0,
#else
    .size    = CONFIG_RTL876x_UART0_TXBUFSIZE,
    .buffer  = g_uart0txbuffer,
    .head    = 0,
    .tail    = 0,
#endif
  },
#if defined(CONFIG_RTL876x_UART0_RXDMA) && \
   !defined(CONFIG_RTL876x_UART0_TXDMA)
  .ops       = &g_uart_rxdma_ops,
#elif defined(CONFIG_RTL876x_UART0_TXDMA) && \
     !defined(CONFIG_RTL876x_UART0_RXDMA)
  .ops       = &g_uart_txdma_ops,
#elif defined(CONFIG_RTL876x_UART0_TXDMA) && \
      defined(CONFIG_RTL876x_UART0_RXDMA)
  .ops       = &g_uart_txrxdma_ops,
#else
  .ops       = &g_uart_ops,
#endif
  .priv      = &g_uart0priv,
};

static int uart0_handler(int irq, void *context, void *arg)
{
    rtl876x_uart_handler(&g_uart0port);
    return OK;
}
#endif /* CONFIG_RTL876x_UART0 */

#ifdef CONFIG_RTL876x_UART1
static struct up_dev_s g_uart1priv =
{
  .uart_base     = UART1,
  .initialized   = false,
  .irq           = Uart1_VECTORn,
  .handler       = uart1_handler,
  .attached      = false,
  .ie            = 0,
  .sr            = 0,
  .tx_gpio       = CONFIG_RTL876x_UART1_TX_PIN,
  .rx_gpio       = CONFIG_RTL876x_UART1_RX_PIN,
#if defined(CONFIG_SERIAL_RXDMA) && defined(CONFIG_RTL876x_UART1_RXDMA)
  .rxdma_used    = true,
  .rxdma_channel_num = CONFIG_RTL876x_UART1_RXDMA_CH,
  .rxdma_handler     = uart1_rxdma_handler,
  .rxdma_handshake   = GDMA_Handshake_UART1_RX,
#endif
#if defined(CONFIG_SERIAL_TXDMA) && defined(CONFIG_RTL876x_UART1_TXDMA)
  .txdma_used    = true,
  .txdma_channel_num = CONFIG_RTL876x_UART1_TXDMA_CH,
  .txdma_handler     = uart1_txdma_handler,
  .txdma_handshake   = GDMA_Handshake_UART1_TX,
#endif
};

static uart_dev_t g_uart1port =
{
#if CONSOLE_UART == 1
  .isconsole = true,
#endif
#if defined(CONFIG_SERIAL_RXDMA) && defined(CONFIG_RTL876x_UART1_RXDMA)
  .dmarx     =
  {
    .buffer  = g_uart1dmarxbuffer,
    .length  = CONFIG_RTL876x_UART1_DMA_RXBUFSIZE,
    .nbuffer = NULL,
    .nlength = 0,
    .nbytes  = 0,
  },
#endif
#if defined(CONFIG_SERIAL_TXDMA) && defined(CONFIG_RTL876x_UART1_TXDMA)
  .dmatx     =
  {
    .buffer  = g_uart1dmatxbuffer,
    .length  = CONFIG_RTL876x_UART1_DMA_TXBUFSIZE,
    .nbuffer = NULL,
    .nlength = 0,
    .nbytes  = 0,
  },
#endif
  .recv      =
  {
#if defined(CONFIG_SERIAL_RXDMA) && defined(CONFIG_RTL876x_UART1_RXDMA)
    .size    = CONFIG_RTL876x_UART1_DMA_RXBUFSIZE,
    .buffer  = g_uart1dmarxbuffer,
    .head    = 0,
    .tail    = 0,
#else
    .size    = CONFIG_RTL876x_UART1_RXBUFSIZE,
    .buffer  = g_uart1rxbuffer,
    .head    = 0,
    .tail    = 0,
#endif
  },
  .xmit      =
  {
#if defined(CONFIG_SERIAL_TXDMA) && defined(CONFIG_RTL876x_UART1_TXDMA)
    .size    = CONFIG_RTL876x_UART1_DMA_TXBUFSIZE,
    .buffer  = g_uart1dmatxbuffer,
    .head    = 0,
    .tail    = 0,
#else
    .size    = CONFIG_RTL876x_UART1_TXBUFSIZE,
    .buffer  = g_uart1txbuffer,
    .head    = 0,
    .tail    = 0,
#endif
  },
#if defined(CONFIG_RTL876x_UART1_RXDMA) && \
   !defined(CONFIG_RTL876x_UART1_TXDMA)
  .ops       = &g_uart_rxdma_ops,
#elif defined(CONFIG_RTL876x_UART1_TXDMA) && \
     !defined(CONFIG_RTL876x_UART1_RXDMA)
  .ops       = &g_uart_txdma_ops,
#elif defined(CONFIG_RTL876x_UART1_TXDMA) && \
      defined(CONFIG_RTL876x_UART1_RXDMA)
  .ops       = &g_uart_txrxdma_ops,
#else
  .ops       = &g_uart_ops,
#endif
  .priv      = &g_uart1priv,
};

static int uart1_handler(int irq, void *context, void *arg)
{
    rtl876x_uart_handler(&g_uart1port);
    return OK;
}
#endif

static uart_dev_t * const
  g_uart_devs[RTL876x_NUART] =
{
#ifdef CONFIG_RTL876x_UART0
  [0] = &g_uart0port,
#endif
#ifdef CONFIG_RTL876x_UART1
  [1] = &g_uart1port,
#endif
};

/****************************************************************************
 * Private Functions
 ****************************************************************************/
static void rtl876x_uart_int_process(struct uart_dev_s *dev)
{
  struct up_dev_s *priv =
    (struct up_dev_s *)dev->priv;

  /* Get the masked USART status word. */

  priv->sr = UART_GetIID(priv->uart_base);

  switch (priv->sr & 0x0E)
  {
      case UART_INT_ID_LINE_STATUS:
      {
          (void)priv->uart_base->LSR;
          break;
      }

      case UART_INT_ID_RX_DATA_TIMEOUT:
      {
          uart_recvchars(dev);
          break;
      }

      case UART_INT_ID_RX_LEVEL_REACH:
      {
          uart_recvchars(dev);
          break;
      }

      default:
          break;
  }
}


static void rtl876x_uart_handler(struct uart_dev_s *dev)
{
  struct up_dev_s *priv =
    (struct up_dev_s *)dev->priv;

  /* Loop until there are no characters to be transferred or,
   * until we have been looping for a long time.
   */

  if (UART_GetFlagStatus(priv->uart_base, UART_FLAG_RX_IDLE) == SET)
    {
      UART_INTConfig(priv->uart_base, UART_INT_RX_IDLE, DISABLE);
#if defined(RTL876x_UART_HAVE_RXDMA)
      if (priv->rxdma_used)
        {
          rtl876x_uart_int_rxdmaprocess(dev);
        }
#endif
      UART_INTConfig(priv->uart_base, UART_INT_RX_IDLE, ENABLE);
    }

#if defined(RTL876x_UART_HAVE_RXDMA)
    if (!priv->rxdma_used)
      {
        rtl876x_uart_int_process(dev);
      }
#else
    rtl876x_uart_int_process(dev);
#endif
    if (priv->uart_base->TX_DONE_INTSR)
    {
#if defined(RTL876x_UART_HAVE_RXDMA)
      if (!priv->rxdma_used)
        {
          GDMA_SuspendCmd(priv->rxdma_base, ENABLE);
        }
#endif
    }
}

/****************************************************************************
 * Name: rtl876x_serial_switch_config
 *
 * Description:
 *   SWitch baud rate by macro.
 *
 ****************************************************************************/

static void rtl876x_serial_switch_config(struct up_dev_s *dev)
{
  uint32_t baud_rate;
  uint32_t crc_parity;
  uint32_t data_bits;
  uint32_t stop_bits;
#ifdef CONFIG_RTL876x_UART0
  if (dev->uart_base == UART0)
    {
      baud_rate  = CONFIG_RTL876x_UART0_BAUD;
      crc_parity = CONFIG_RTL876x_UART0_PARITY;
      data_bits  = CONFIG_RTL876x_UART0_BITS;
      stop_bits  = CONFIG_RTL876x_UART0_2STOP;
    }
#endif
#ifdef CONFIG_RTL876x_UART1
  if (dev->uart_base == UART1)
    {
      baud_rate  = CONFIG_RTL876x_UART1_BAUD;
      crc_parity = CONFIG_RTL876x_UART1_PARITY;
      data_bits  = CONFIG_RTL876x_UART1_BITS;
      stop_bits  = CONFIG_RTL876x_UART1_2STOP;
    }
#endif
  switch (baud_rate)
    {
      case 9600:
        dev->baud = 0;
        break;
      case 19200:
        dev->baud = 1;
        break;
      case 115200:
        dev->baud = 2;
        break;
      case 230400:
        dev->baud = 3;
        break;
      case 256000:
        dev->baud = 4;
        break;
      case 384000:
        dev->baud = 5;
        break;
      case 460800:
        dev->baud = 6;
        break;
      case 921600:
        dev->baud = 7;
        break;
      case 1000000:
        dev->baud = 8;
        break;
      case 2000000:
        dev->baud = 9;
        break;
      case 3000000:
        dev->baud = 10;
        break;
      default:
        dev->baud = 2;
        break;
    }

  switch (crc_parity)
    {
      case 0:
        dev->parity = 0;
        break;
      case 1:
        dev->parity = 8;
        break;
      case 2:
        dev->parity = 24;
        break;
      default:
        dev->parity = 0;
        break;
    }

  switch (data_bits)
    {
      case 7:
        dev->bits = 0;
        break;
      case 8:
        dev->bits = 1;
        break;
      default:
        dev->bits = 1;
        break;
    }

  switch (stop_bits)
    {
      case 1:
        dev->stopbits2 = 0;
        break;
      case 2:
        dev->stopbits2 = 4;
        break;
      default:
        dev->stopbits2 = 0;
        break;
    }
}

/****************************************************************************
 * Name: rtl876x_serial_setup
 *
 * Description:
 *   Configure the UART baud, bits, parity, etc. This method is called the
 *   first time that the serial port is opened.
 *
 ****************************************************************************/

#if defined(RTL876x_UART_HAVE_NODMA)
static int rtl876x_serial_setup(struct uart_dev_s *dev)
{
  struct up_dev_s *priv =
    (struct up_dev_s *)dev->priv;

  /* Enable UART clock , config UART pin */

  if (!priv->initialized)
    {
#ifdef CONFIG_RTL876x_UART0
      if (priv->uart_base == UART0)
        {
          RCC_PeriphClockCmd(APBPeriph_UART0, APBPeriph_UART0_CLOCK, ENABLE);
          Pad_Config(priv->tx_gpio, PAD_PINMUX_MODE, PAD_IS_PWRON,
                              PAD_PULL_NONE, PAD_OUT_ENABLE, PAD_OUT_HIGH);
          Pad_Config(priv->rx_gpio, PAD_PINMUX_MODE, PAD_IS_PWRON,
                              PAD_PULL_UP, PAD_OUT_DISABLE, PAD_OUT_LOW);
          Pinmux_Config(priv->tx_gpio, UART0_TX);
          Pinmux_Config(priv->rx_gpio, UART0_RX);
        }
#endif
#ifdef CONFIG_RTL876x_UART1
      if (priv->uart_base == UART1)
        {
          RCC_PeriphClockCmd(APBPeriph_UART1, APBPeriph_UART0_CLOCK, ENABLE);
          Pad_Config(priv->tx_gpio, PAD_PINMUX_MODE, PAD_IS_PWRON,
                              PAD_PULL_NONE, PAD_OUT_ENABLE, PAD_OUT_HIGH);
          Pad_Config(priv->rx_gpio, PAD_PINMUX_MODE, PAD_IS_PWRON,
                              PAD_PULL_UP, PAD_OUT_DISABLE, PAD_OUT_LOW);
          Pinmux_Config(priv->tx_gpio, UART1_TX);
          Pinmux_Config(priv->rx_gpio, UART1_RX);

#if defined(CONFIG_RTL876x_UART1_FLOWCONTROL)
          Pad_Config(CONFIG_RTL876x_UART1_CTS_PIN, PAD_PINMUX_MODE, PAD_IS_PWRON,
                     PAD_PULL_UP, PAD_OUT_DISABLE, PAD_OUT_HIGH);
          Pad_Config(CONFIG_RTL876x_UART1_RTS_PIN, PAD_PINMUX_MODE, PAD_IS_PWRON,
                     PAD_PULL_UP, PAD_OUT_DISABLE, PAD_OUT_HIGH);

          Pinmux_Config(CONFIG_RTL876x_UART1_CTS_PIN, UART1_CTS);
          Pinmux_Config(CONFIG_RTL876x_UART1_RTS_PIN, UART1_RTS);
#endif
        }
#endif

      rtl876x_serial_switch_config(priv);
      UART_InitTypeDef UART_InitStruct;
      UART_StructInit(&UART_InitStruct);
      UART_InitStruct.UART_OvsrAdj    = baudrate_table[priv->baud].ovsr_adj;
      UART_InitStruct.UART_Div        = baudrate_table[priv->baud].div;
      UART_InitStruct.UART_Ovsr       = baudrate_table[priv->baud].ovsr;
      UART_InitStruct.UART_WordLen    = priv->bits;
      UART_InitStruct.UART_StopBits   = priv->stopbits2;
      UART_InitStruct.UART_Parity     = priv->parity;
      UART_InitStruct.UART_IdleTime   = UART_RX_IDLE_2BYTE;
      UART_InitStruct.UART_RxThdLevel = 1;
      UART_InitStruct.UART_TxThdLevel = 16;

#ifdef CONFIG_RTL876x_UART1
#if defined(CONFIG_RTL876x_UART1_FLOWCONTROL)
      if (priv->uart_base == UART1)
        {
          UART_InitStruct.UART_HardwareFlowControl = UART_HW_FLOW_CTRL_ENABLE;
        }
#endif
#endif

      UART_Init(priv->uart_base, &UART_InitStruct);

      /* Set up the cached interrupt enables value */

      priv->ie = 0;

      /* Mark device as initialized. */

      priv->initialized = true;
    }

  return OK;
}
#endif

/****************************************************************************
 * Name: rtl876x_serial_shutdown
 *
 * Description:
 *   Disable the UART.  This method is called when the serial
 *   port is closed
 *
 ****************************************************************************/

static void rtl876x_serial_shutdown(struct uart_dev_s *dev)
{
  struct up_dev_s *priv =
    (struct up_dev_s *)dev->priv;

  /* Disable UART clock , releaase UART pin */

  if (priv->initialized)
    {
#if defined(CONFIG_SERIAL_RXDMA)
      if (priv->rxdma_used)
        {
          GDMA_Cmd(priv->rxdma_channel_num, DISABLE);
        }
#endif
#if defined(CONFIG_SERIAL_TXDMA)
      if (priv->txdma_used)
        {
          GDMA_Cmd(priv->txdma_channel_num, DISABLE);
        }
#endif
#ifdef CONFIG_RTL876x_UART0
      if (priv->uart_base == UART0)
        {
          RCC_PeriphClockCmd(APBPeriph_UART0,
                                           APBPeriph_UART0_CLOCK, DISABLE);
          Pad_Config(priv->tx_gpio, PAD_SW_MODE, PAD_IS_PWRON,
                              PAD_PULL_NONE, PAD_OUT_DISABLE, PAD_OUT_LOW);
          Pad_Config(priv->rx_gpio, PAD_SW_MODE, PAD_IS_PWRON,
                              PAD_PULL_NONE, PAD_OUT_DISABLE, PAD_OUT_LOW);
          Pinmux_Deinit(priv->tx_gpio);
          Pinmux_Deinit(priv->rx_gpio);
        }
#endif
#ifdef CONFIG_RTL876x_UART0
      if (priv->uart_base == UART1)
        {
          RCC_PeriphClockCmd(APBPeriph_UART1,
                                           APBPeriph_UART0_CLOCK, DISABLE);
          Pad_Config(priv->tx_gpio, PAD_SW_MODE, PAD_IS_PWRON,
                              PAD_PULL_NONE, PAD_OUT_DISABLE, PAD_OUT_LOW);
          Pad_Config(priv->rx_gpio, PAD_SW_MODE, PAD_IS_PWRON,
                              PAD_PULL_NONE, PAD_OUT_DISABLE, PAD_OUT_LOW);

          Pinmux_Deinit(priv->tx_gpio);
          Pinmux_Deinit(priv->rx_gpio);

#if defined(CONFIG_RTL876x_UART1_FLOWCONTROL)
          Pad_Config(CONFIG_RTL876x_UART1_CTS_PIN, PAD_SW_MODE, PAD_IS_PWRON,
                              PAD_PULL_NONE, PAD_OUT_DISABLE, PAD_OUT_LOW);
          Pad_Config(CONFIG_RTL876x_UART1_RTS_PIN, PAD_SW_MODE, PAD_IS_PWRON,
                              PAD_PULL_NONE, PAD_OUT_DISABLE, PAD_OUT_LOW);

          Pinmux_Deinit(CONFIG_RTL876x_UART1_CTS_PIN);
          Pinmux_Deinit(CONFIG_RTL876x_UART1_RTS_PIN);
#endif
        }
#endif
    }

  /* Mark device as uninitialized. */

  priv->initialized = false;
}

/****************************************************************************
 * Name: rtl876x_serial_attach
 *
 * Description:
 *   Configure the UART to operation in interrupt driven mode.  This method
 *   is called when the serial port is opened.  Normally, this is just after
 *   the setup() method is called, however, the serial console may operate in
 *   a non-interrupt driven mode during the boot phase.
 *
 *   RX and TX interrupts are not enabled when by the attach method (unless
 *   the hardware supports multiple levels of interrupt enabling).  The RX
 *   and TX interrupts are not enabled until the txint() and rxint() methods
 *   are called.
 *
 ****************************************************************************/

static int rtl876x_serial_attach(struct uart_dev_s *dev)
{
  DEBUGASSERT(dev);
  struct up_dev_s *priv =
    (struct up_dev_s *)dev->priv;
  DEBUGASSERT(priv);
  if (!priv->attached)
    {
      up_enable_irq(priv->irq);
      irq_attach(priv->irq, priv->handler, NULL);
#if defined(CONFIG_SERIAL_RXDMA)
      if (priv->rxdma_used)
        {
          up_enable_irq(priv->rxdma_irq);
          irq_attach(priv->rxdma_irq, priv->rxdma_handler, NULL);
        }
#endif
#if defined(CONFIG_SERIAL_TXDMA)
      if (priv->txdma_used)
        {
          up_enable_irq(priv->txdma_irq);
          irq_attach(priv->txdma_irq, priv->txdma_handler, NULL);
        }
#endif
      priv->attached = true;
    }
  return OK;
}

/****************************************************************************
 * Name: rtl876x_serial_detach
 *
 * Description:
 *   Detach UART interrupts.  This method is called when the serial port is
 *   closed normally just before the shutdown method is called.  The
 *   exception is the serial console which is never shutdown.
 *
 ****************************************************************************/

static void rtl876x_serial_detach(struct uart_dev_s *dev)
{
  struct up_dev_s *priv =
    (struct up_dev_s *)dev->priv;
  if (priv->attached)
    {
      irq_detach(priv->irq);
      up_disable_irq(priv->irq);
#if defined(CONFIG_SERIAL_RXDMA)
      if (priv->rxdma_used)
        {
          irq_detach(priv->rxdma_irq);
          up_disable_irq(priv->rxdma_irq);
        }
#endif
#if defined(CONFIG_SERIAL_TXDMA)
      if (priv->txdma_used)
        {
          irq_detach(priv->txdma_irq);
          up_disable_irq(priv->txdma_irq);
        }
#endif
    }

  priv->attached = false;
}

/****************************************************************************
 * Name: rtl876x_serial_ioctl
 *
 * Description:
 *   All ioctl calls will be routed through this method
 *
 ****************************************************************************/

static int rtl876x_serial_ioctl(struct file *filep, int cmd,
                               unsigned long arg)
{
  /* There are no platform-specific ioctl commands */

  return -ENOTTY;
}

/****************************************************************************
 * Name: rtl876x_serial_receive
 *
 * Description:
 *   Called (usually) from the interrupt level to receive one
 *   character from the USART.  Error bits associated with the
 *   receipt are provided in the return 'status'.
 *
 ****************************************************************************/

#if defined(RTL876x_UART_HAVE_RXNODMA)
static int rtl876x_serial_receive(struct uart_dev_s *dev,
                                 unsigned int *status)
{
  struct up_dev_s *priv =
    (struct up_dev_s *)dev->priv;

  uint8_t data = priv->uart_base->RB_THR;

  /* Get the Rx byte plux error information.  Return those in status */

  *status = (priv->uart_base->LSR & 0x9e) << 16 | data;

  /* Then return the actual received byte */

  return (int)data;
}
#endif

/****************************************************************************
 * Name: rtl876x_serial_rxint
 *
 * Description:
 *   Call to enable or disable RX interrupts
 *
 ****************************************************************************/

#if defined(RTL876x_UART_HAVE_RXNODMA)
static void rtl876x_serial_rxint(struct uart_dev_s *dev, bool enable)
{
  struct up_dev_s *priv =
    (struct up_dev_s *)dev->priv;

  /* UART receive interrupts:
   *
   * BIT          Status                        Meaning
   * ---------------- -------------- ---------------------- -------------
   * BIT0         UART_INT_RD_AVA               Received Data Ready
   *                                            to be Read
   * BIT2         UART_INT_RX_LINE_STS          Framing Error
   * BIT6         UART_INT_RX_BREAK             Break error
   * BIT7         UART_INT_RX_IDLE              Rx line idle
   */

  irqstate_t flags;
  flags = enter_critical_section();
  if (enable)
    {
      /* Receive an interrupt when their is anything in the Rx data
       * register (or an Rx timeout occurs).
       */

      UART_INTConfig(priv->uart_base, UART_INT_RD_AVA,  ENABLE);
      UART_INTConfig(priv->uart_base, UART_INT_RX_IDLE, ENABLE);
      UART_MaskINTConfig(priv->uart_base, UART_INT_MASK_RD_AVA,  DISABLE);
      UART_MaskINTConfig(priv->uart_base, UART_INT_MASK_RX_IDLE, DISABLE);
      priv->ie |= UART_INT_RD_AVA | UART_INT_RX_IDLE;
  #ifdef CONFIG_UART_ERRINTS
      UART_INTConfig(priv->uart_base, UART_INT_RX_BREAK,    ENABLE);
      UART_INTConfig(priv->uart_base, UART_INT_RX_LINE_STS, ENABLE);
      UART_MaskINTConfig(priv->uart_base,
                                       UART_INT_MASK_RX_BREAK, DISABLE);
      UART_MaskINTConfig(priv->uart_base,
                                    UART_INT_MASK_RX_LINE_STS, DISABLE);
      priv->ie |= UART_INT_RX_BREAK | UART_INT_RX_LINE_STS;
  #endif
    }
  else
    {
      UART_INTConfig(priv->uart_base, UART_INT_RD_AVA,  DISABLE);
      UART_INTConfig(priv->uart_base, UART_INT_RX_IDLE, DISABLE);
      UART_MaskINTConfig(priv->uart_base, UART_INT_MASK_RD_AVA,  ENABLE);
      UART_MaskINTConfig(priv->uart_base, UART_INT_MASK_RX_IDLE, ENABLE);
      priv->ie &= ~(UART_INT_RD_AVA | UART_INT_RX_IDLE);
  #ifdef CONFIG_UART_ERRINTS
      UART_INTConfig(priv->uart_base, UART_INT_RX_BREAK,    DISABLE);
      UART_INTConfig(priv->uart_base, UART_INT_RX_LINE_STS, DISABLE);
      UART_MaskINTConfig(priv->uart_base, UART_INT_MASK_RX_BREAK,    ENABLE);
      UART_MaskINTConfig(priv->uart_base, UART_INT_MASK_RX_LINE_STS, ENABLE);
      priv->ie &= ~(UART_INT_RX_BREAK | UART_INT_RX_LINE_STS);
  #endif
    }

  leave_critical_section(flags);
}
#endif

/****************************************************************************
 * Name: rtl876x_serial_rxavailable
 *
 * Description:
 *   Return true if the receive register is not empty
 *
 ****************************************************************************/

static bool rtl876x_serial_rxavailable(struct uart_dev_s *dev)
{
  struct up_dev_s *priv =
    (struct up_dev_s *)dev->priv;

  return (priv->uart_base->LSR & BIT0);
}

/****************************************************************************
 * Name: rtl876x_serial_send
 *
 * Description:
 *   This method will send one byte on the USART
 *
 ****************************************************************************/

static void rtl876x_serial_send(struct uart_dev_s *dev, int ch)
{
  struct up_dev_s *priv =
    (struct up_dev_s *)dev->priv;

  priv->uart_base->RB_THR = ch;
}

/****************************************************************************
 * Name: rtl876x_serial_txint
 *
 * Description:
 *   Call to enable or disable TX interrupts
 *
 ****************************************************************************/
#if defined(RTL876x_UART_HAVE_TXNODMA)
static void rtl876x_serial_txint(struct uart_dev_s *dev, bool enable)
{
  uint16_t nbytes = 0;

  /* USART transmit interrupts:
   *
   * BIT          Status                        Meaning
   * --------------- ------------- ---------------------------- -------------
   * BIT1         UART_INT_TX_FIFO_EMPTY        Transmit Data Register Empty
   * BIT4         UART_INT_TX_DONE              Transmission Complete
   */

  if (!enable)
    {
      /* Disable the TX interrupt */
      return;
    }

  while (dev->xmit.head != dev->xmit.tail)
    {
      while (!rtl876x_serial_txready(dev));

      rtl876x_serial_send(dev, dev->xmit.buffer[dev->xmit.tail]);

      /* Increment the tail index */

      if (++(dev->xmit.tail) >= dev->xmit.size)
        {
          dev->xmit.tail = 0;
        }

      nbytes++;
    }

  /* If any bytes were removed from the buffer, inform any waiters that
   * there is space available.
   */

  if (nbytes)
    {
      uart_datasent(dev);
    }

}
#endif
/****************************************************************************
 * Name: rtl876x_serial_txready
 *
 * Description:
 *   Return true if the transmit data register is ready
 *
 ****************************************************************************/

static bool rtl876x_serial_txready(struct uart_dev_s *dev)
{
  struct up_dev_s *priv =
    (struct up_dev_s *)dev->priv;

  return ((priv->uart_base->FIFO_LEVEL & 0x1f) < 15);
}

/****************************************************************************
 * Name: rtl876x_serial_txempty
 *
 * Description:
 *   Return true if the transmit data register is empty
 *
 ****************************************************************************/

static bool rtl876x_serial_txempty(struct uart_dev_s *dev)
{
  struct up_dev_s *priv =
    (struct up_dev_s *)dev->priv;

  return (priv->uart_base->LSR & BIT6);
}

#if !defined(RTL876x_UART_NO_DMA)
static int rtl876x_serial_dma_setup(struct uart_dev_s *dev)
{
  struct up_dev_s *priv =
  (struct up_dev_s *)dev->priv;

  /* Enable UART clock , config UART pin */

  if (!priv->initialized)
    {
      RCC_PeriphClockCmd(APBPeriph_GDMA, APBPeriph_GDMA_CLOCK, ENABLE);
      GDMA_InitTypeDef GDMA_InitStruct;
      UART_InitTypeDef UART_InitStruct;
      if (priv->uart_base == UART0)
        {
          RCC_PeriphClockCmd(APBPeriph_UART0, \
                                       APBPeriph_UART0_CLOCK, ENABLE);
          Pinmux_Config(priv->tx_gpio, UART0_TX);
          Pinmux_Config(priv->rx_gpio, UART0_RX);
        }
      else if (priv->uart_base == UART1)
        {
          RCC_PeriphClockCmd(APBPeriph_UART1, APBPeriph_UART0_CLOCK, ENABLE);
          Pinmux_Config(priv->tx_gpio, UART1_TX);
          Pinmux_Config(priv->rx_gpio, UART1_RX);
        }
      Pad_Config(priv->tx_gpio, PAD_PINMUX_MODE, PAD_IS_PWRON,
                          PAD_PULL_NONE, PAD_OUT_ENABLE, PAD_OUT_HIGH);
      Pad_Config(priv->rx_gpio, PAD_PINMUX_MODE, PAD_IS_PWRON,
                          PAD_PULL_UP, PAD_OUT_DISABLE, PAD_OUT_LOW);

      rtl876x_serial_switch_config(priv);
      UART_StructInit(&UART_InitStruct);
      UART_InitStruct.UART_OvsrAdj      = baudrate_table[priv->baud].ovsr_adj;
      UART_InitStruct.UART_Div          = baudrate_table[priv->baud].div;
      UART_InitStruct.UART_Ovsr         = baudrate_table[priv->baud].ovsr;
      UART_InitStruct.UART_WordLen      = priv->bits;
      UART_InitStruct.UART_StopBits     = priv->stopbits2;
      UART_InitStruct.UART_Parity       = priv->parity;
      if (priv->baud >= 8)
        {
          UART_InitStruct.UART_IdleTime   = UART_RX_IDLE_128BYTE;
        }
      else if (priv->baud >= 7)
        {
          UART_InitStruct.UART_IdleTime   = UART_RX_IDLE_64BYTE;
        }
      else
        {
          UART_InitStruct.UART_IdleTime   = UART_RX_IDLE_4BYTE;
        }
      UART_InitStruct.UART_RxThdLevel = 1;
      UART_InitStruct.UART_TxThdLevel = 16;
      UART_InitStruct.UART_DmaEn        = UART_DMA_ENABLE;
#if defined(CONFIG_SERIAL_RXDMA)
      if (priv->rxdma_used)
        {
          UART_InitStruct.UART_RxDmaEn      = ENABLE;
          UART_InitStruct.UART_RxWaterLevel = 1;
          switch (priv->rxdma_channel_num)
            {
              case 0:
                priv->rxdma_base = GDMA_Channel0;
                priv->rxdma_irq  = GDMA0_Channel0_VECTORn;
                break;
              case 1:
                priv->rxdma_base = GDMA_Channel1;
                priv->rxdma_irq  = GDMA0_Channel1_VECTORn;
                break;
              case 2:
                priv->rxdma_base = GDMA_Channel2;
                priv->rxdma_irq  = GDMA0_Channel2_VECTORn;
                break;
              case 3:
                priv->rxdma_base = GDMA_Channel3;
                priv->rxdma_irq  = GDMA0_Channel3_VECTORn;
                break;
            }
        }
#endif
#if defined(CONFIG_SERIAL_TXDMA)
      if (priv->txdma_used)
        {
          UART_InitStruct.UART_TxDmaEn      = ENABLE;
          UART_InitStruct.UART_TxWaterLevel = 15;
          switch (priv->txdma_channel_num)
            {
              case 0:
                priv->txdma_base = GDMA_Channel0;
                priv->txdma_irq  = GDMA0_Channel0_VECTORn;
                break;
              case 1:
                priv->txdma_base = GDMA_Channel1;
                priv->txdma_irq  = GDMA0_Channel1_VECTORn;
                break;
              case 2:
                priv->txdma_base = GDMA_Channel2;
                priv->txdma_irq  = GDMA0_Channel2_VECTORn;
                break;
              case 3:
                priv->txdma_base = GDMA_Channel3;
                priv->txdma_irq  = GDMA0_Channel3_VECTORn;
                break;
            }
        }
#endif
      UART_Init(priv->uart_base, &UART_InitStruct);

      /* Initialize GDMA */

#if defined(CONFIG_SERIAL_RXDMA)
      if (priv->rxdma_used)
        {
          GDMA_StructInit(&GDMA_InitStruct);
          GDMA_InitStruct.GDMA_ChannelNum          = priv->rxdma_channel_num;
          GDMA_InitStruct.GDMA_DIR                 = GDMA_DIR_PeripheralToMemory;
          GDMA_InitStruct.GDMA_SourceInc           = DMA_SourceInc_Fix;
          GDMA_InitStruct.GDMA_DestinationInc      = DMA_DestinationInc_Inc;
          GDMA_InitStruct.GDMA_SourceDataSize      = GDMA_DataSize_Byte;
          GDMA_InitStruct.GDMA_DestinationDataSize = GDMA_DataSize_Byte;
          GDMA_InitStruct.GDMA_SourceMsize         = GDMA_Msize_1;
          GDMA_InitStruct.GDMA_DestinationMsize    = GDMA_Msize_1;
          GDMA_InitStruct.GDMA_SourceAddr          = \
                                    (uint32_t)(&(priv->uart_base->RB_THR));
          GDMA_InitStruct.GDMA_DestinationAddr     = (uint32_t)dev->dmarx.buffer;
          GDMA_InitStruct.GDMA_BufferSize          = dev->dmarx.length;
          GDMA_InitStruct.GDMA_SourceHandshake     = priv->rxdma_handshake;

          GDMA_Init(priv->rxdma_base, &GDMA_InitStruct);
        }
#endif
#if defined(CONFIG_SERIAL_TXDMA)
      if (priv->txdma_used)
        {
          GDMA_StructInit(&GDMA_InitStruct);
          GDMA_InitStruct.GDMA_ChannelNum          = priv->txdma_channel_num;
          GDMA_InitStruct.GDMA_DIR                 = GDMA_DIR_MemoryToPeripheral;
          GDMA_InitStruct.GDMA_SourceInc           = DMA_SourceInc_Inc;
          GDMA_InitStruct.GDMA_DestinationInc      = DMA_DestinationInc_Fix;
          GDMA_InitStruct.GDMA_SourceDataSize      = GDMA_DataSize_Byte;
          GDMA_InitStruct.GDMA_DestinationDataSize = GDMA_DataSize_Byte;
          GDMA_InitStruct.GDMA_SourceMsize         = GDMA_Msize_1;
          GDMA_InitStruct.GDMA_DestinationMsize    = GDMA_Msize_1;
          GDMA_InitStruct.GDMA_SourceAddr          = (uint32_t)dev->dmatx.buffer;
          GDMA_InitStruct.GDMA_DestinationAddr     = \
                                          (uint32_t)(&(priv->uart_base->RB_THR));
          GDMA_InitStruct.GDMA_BufferSize          = dev->dmatx.length;
          GDMA_InitStruct.GDMA_SourceHandshake     = priv->txdma_handshake;

          GDMA_Init(priv->txdma_base, &GDMA_InitStruct);
        }
#endif
      /* Set up the cached interrupt enables value */

      priv->ie = 0;

      /* Mark device as initialized. */

      priv->initialized = true;
    }

  return OK;
}
#endif

#if defined(RTL876x_UART_HAVE_RXDMA)
static void rtl876x_serial_dma_receive(struct uart_dev_s *dev)
{
  struct up_dev_s *priv =
    (struct up_dev_s *)dev->priv;
  GDMA_Cmd(priv->rxdma_channel_num, DISABLE);
  rtl876x_rxdma_set_block(dev);
  GDMA_SuspendCmd(priv->rxdma_base, DISABLE);
  GDMA_Cmd(priv->rxdma_channel_num, ENABLE);
}

static void rtl876x_rxdma_set_block(struct uart_dev_s *dev)
{
  struct up_dev_s *priv =
    (struct up_dev_s *)dev->priv;

  struct uart_dmaxfer_s *xfer = &dev->dmarx;

  irqstate_t flags;
  flags = enter_critical_section();

  /* single block */

  if (xfer->length > 0)
    {
      priv->rxdma_base->DAR = (uint32_t)xfer->buffer;
      priv->rxdma_base->CTL_HIGH = (uint32_t)xfer->length;
    }
  else if (xfer->nlength > 0)
    {
      priv->rxdma_base->DAR = (uint32_t)xfer->nbuffer;
      priv->rxdma_base->CTL_HIGH = (uint32_t)xfer->nlength;
    }

  leave_critical_section(flags);
}

static void rtl876x_serial_dma_rxint(struct uart_dev_s *dev, bool enable)
{
  struct up_dev_s *priv =
    (struct up_dev_s *)dev->priv;

  irqstate_t flags;
  flags = enter_critical_section();
  if (enable)
    {
      /* Receive an interrupt when their is anything in the Rx data
       * register (or an Rx timeout occurs).
       */

      UART_INTConfig(priv->uart_base, UART_INT_RX_IDLE, ENABLE);
      UART_MaskINTConfig(priv->uart_base, UART_INT_MASK_RX_IDLE, DISABLE);
      priv->ie |= UART_INT_RX_IDLE;
      GDMA_INTConfig(priv->rxdma_channel_num, GDMA_INT_Transfer, ENABLE);

      uart_recvchars_dma(dev);
    }
  else
    {
      UART_INTConfig(priv->uart_base, UART_INT_RX_IDLE, DISABLE);
      UART_MaskINTConfig(priv->uart_base, UART_INT_MASK_RX_IDLE, ENABLE);
      priv->ie &= ~(UART_INT_RX_IDLE);
      GDMA_INTConfig(priv->rxdma_channel_num, GDMA_INT_Transfer, DISABLE);
    }

  leave_critical_section(flags);

}

static void rtl876x_serial_dma_rxfree(struct uart_dev_s *dev)
{
}

#ifdef CONFIG_RTL876x_UART0_RXDMA
static int uart0_rxdma_handler(int irq, void *context, void *arg)
{
  rtl876x_uart_rxdma_handler(&g_uart0port);
  return OK;
}
#endif
#ifdef CONFIG_RTL876x_UART1_RXDMA
static int uart1_rxdma_handler(int irq, void *context, void *arg)
{
  rtl876x_uart_rxdma_handler(&g_uart1port);
  return OK;
}
#endif

static void rtl876x_uart_rxdma_handler(struct uart_dev_s *dev)
{
  struct up_dev_s *priv =
    (struct up_dev_s *)dev->priv;
  GDMA_SuspendCmd(priv->rxdma_base, ENABLE);
  GDMA_ClearINTPendingBit(priv->rxdma_channel_num, GDMA_INT_Transfer);

  struct uart_dmaxfer_s *xfer = &dev->dmarx;
  xfer->nbytes = GDMA_GetTransferLen(priv->rxdma_base);

  GDMA_SuspendCmd(priv->rxdma_base, DISABLE);
  uart_recvchars_done(dev);
}

static void rtl876x_uart_int_rxdmaprocess(struct uart_dev_s *dev)
{
  struct up_dev_s *priv =
    (struct up_dev_s *)dev->priv;
  GDMA_SuspendCmd(priv->rxdma_base, ENABLE);
  if (priv->uart_base->LSR & BIT0)
    {
      GDMA_SuspendCmd(priv->rxdma_base, DISABLE);
      return;
    }
  struct uart_dmaxfer_s *xfer = &dev->dmarx;
  xfer->nbytes = GDMA_GetTransferLen(priv->rxdma_base);
  GDMA_SuspendCmd(priv->rxdma_base, DISABLE);
  GDMA_Cmd(priv->rxdma_channel_num, DISABLE);
  uart_recvchars_done(dev);
}

#endif /* RTL876x_UART_HAVE_RXDMA */


/****************************************************************************
 * Public Functions
 ****************************************************************************/

#ifdef RTL876x_UART_HAVE_TXDMA
static void rtl876x_serial_dma_txint(struct uart_dev_s *dev, bool enable)
{
  struct up_dev_s *priv =
    (struct up_dev_s *)dev->priv;
  irqstate_t flags;
  flags = enter_critical_section();

  if (enable)
    {
      /* Set to receive an interrupt when the TX data register is empty */

        GDMA_INTConfig(priv->txdma_channel_num, GDMA_INT_Transfer, ENABLE);
        if (!priv->txdma_running)
          {
            GDMA_SuspendCmd(priv->txdma_base,ENABLE);
            uart_xmitchars_dma(dev);
          }
    }
  else
    {
      /* Disable the TX interrupt */

      GDMA_INTConfig(priv->txdma_channel_num, GDMA_INT_Transfer, DISABLE);
    }

  leave_critical_section(flags);

}

static void rtl876x_serial_dma_send(struct uart_dev_s *dev)
{
  struct up_dev_s *priv =
    (struct up_dev_s *)dev->priv;
  GDMA_Cmd(priv->txdma_channel_num, DISABLE);
  rtl876x_txdma_set_block(dev);
  GDMA_SuspendCmd(priv->txdma_base, DISABLE);
  priv->txdma_running = true;
  GDMA_Cmd(priv->txdma_channel_num, ENABLE);
}

static void rtl876x_txdma_set_block(struct uart_dev_s *dev)
{
  struct up_dev_s *priv =
    (struct up_dev_s *)dev->priv;

  struct uart_dmaxfer_s *xfer = &dev->dmatx;

  irqstate_t flags;
  flags = enter_critical_section();

  /* single block */

  if (xfer->length > 0)
    {
      priv->txdma_base->SAR = (uint32_t)xfer->buffer;
      priv->txdma_base->CTL_HIGH = (uint32_t)xfer->length;
    }
  else if (xfer->nlength > 0)
    {
      priv->txdma_base->SAR = (uint32_t)xfer->nbuffer;
      priv->txdma_base->CTL_HIGH = (uint32_t)xfer->nlength;
    }

  leave_critical_section(flags);
}

static void rtl876x_serial_dma_txavail(struct uart_dev_s *dev)
{

}

#if defined(CONFIG_SERIAL_TXDMA) && defined(CONFIG_RTL876x_UART0_TXDMA)
static int uart0_txdma_handler(int irq, void *context, void *arg)
{
  rtl876x_uart_txdma_handler(&g_uart0port);
  return OK;
}
#endif

#if defined(CONFIG_SERIAL_TXDMA) && defined(CONFIG_RTL876x_UART1_TXDMA)
static int uart1_txdma_handler(int irq, void *context, void *arg)
{
  rtl876x_uart_txdma_handler(&g_uart1port);
  return OK;

}
#endif

static void rtl876x_uart_txdma_handler(struct uart_dev_s *dev)
{
  struct up_dev_s *priv =
    (struct up_dev_s *)dev->priv;
  priv->txdma_running = false;

  GDMA_INTConfig(priv->txdma_channel_num, GDMA_INT_Transfer, DISABLE);
  GDMA_ClearINTPendingBit(priv->txdma_channel_num, GDMA_INT_Transfer);
  struct uart_dmaxfer_s *xfer = &dev->dmatx;
  xfer->nbytes = GDMA_GetTransferLen(priv->txdma_base);
  uart_xmitchars_done(dev);
}
#endif

/****************************************************************************
 * Name: arm_earlyserialinit
 *
 * Description:
 *   Performs the low level UART initialization early in debug so that the
 *   serial console will be available during bootup.  This must be called
 *   before arm_serialinit.
 *
 ****************************************************************************/

#ifdef USE_EARLYSERIALINIT
void arm_earlyserialinit(void)
{
#ifdef HAVE_UART

  /* Configure whichever one is the console */

#if CONSOLE_UART >= 0
  g_uart_devs[CONSOLE_UART]->ops->setup(g_uart_devs[CONSOLE_UART]);
#endif
#endif /* HAVE UART */
}
#endif /* USE_EARLYSERIALINIT */

#ifdef CONFIG_PM
static bool console_dlps_check_cb(uint32_t * p_value)
{
  if (!rtl876x_serial_txempty(g_uart_devs[CONSOLE_UART]))
    {
      return false;
    }

  *p_value = UINT32_MAX - 1;
  return true;
}
#endif

/****************************************************************************
 * Name: arm_serialinit
 *
 * Description:
 *   Register serial console and serial ports.  This assumes
 *   that arm_earlyserialinit was called previously.
 *
 ****************************************************************************/

void arm_serialinit(void)
{
#ifdef HAVE_UART
  char devname[16];
  unsigned i;
  unsigned minor = 0;
#if CONSOLE_UART >= 0

  uart_register("/dev/console", g_uart_devs[CONSOLE_UART]);

#ifdef CONFIG_PM
  dlps_check_cb_reg((DLPSEnterCheckFunc)console_dlps_check_cb);
#endif

  /* Register the console UART to ttyS0 and exclude
   * it from initializing it further down
   */

  uart_register("/dev/ttyS0", g_uart_devs[CONSOLE_UART]);
  minor = 1;
#endif /* CONSOLE_UART >= 0 */

  /* Register all remaining UARTs */

  strlcpy(devname, "/dev/ttySx", sizeof(devname));
  for (i = 0; i < RTL876x_NUART; i++)
    {

      /* Don't create a device for non-configured ports. */

      if (g_uart_devs[i] == 0)
        {
          continue;
        }

      /* Don't create a device for the console - we did that above */

      if (g_uart_devs[i]->isconsole)
        {
          continue;
        }

      /* Register USARTs as devices in increasing order */

      devname[9] = '0' + minor++;
      uart_register(devname, g_uart_devs[i]);
    }

#endif /* HAVE UART */
}

/****************************************************************************
 * Name: up_putc
 *
 * Description:
 *   Provide priority, low-level access to support OS debug writes
 *
 ****************************************************************************/

void arm_lowputc(char ch)
{
      /* Wait until the TX data register is ready */

      while (!rtl876x_serial_txready(g_uart_devs[CONSOLE_UART]));

      /* Then send the character */

      rtl876x_serial_send(g_uart_devs[CONSOLE_UART], ch);
}

int up_putc(int ch)
{
  irqstate_t flags;
  flags = enter_critical_section();
#if CONSOLE_UART >= 0

  /* Check for LF */

  if (ch == '\n')
    {
      /* Add CR */

      arm_lowputc('\r');
    }

  arm_lowputc(ch);

#endif
  leave_critical_section(flags);
  return ch;
}


#ifdef CONFIG_PM
#define DATA_RAM_FUNCTION  __attribute__((section(".app.data_ram.text")))
#if defined(CONFIG_RTL876x_UART0) || defined(CONFIG_RTL876x_UART1) 
DATA_RAM_FUNCTION
static void uart_dlps_reg_store(uint32_t *p_buf, UART_TypeDef *UARTx)
{
  UARTx->LCR |= (1 << 7);
  memcpy((uint32_t *)&p_buf[0], (uint32_t *)&UARTx->DLL, 8);
  UARTx->LCR &= (~(1 << 7));

  p_buf[2] = UARTx->DLH_INTCR;
  memcpy((uint32_t *)&p_buf[4], (uint32_t *)&UARTx->LCR, 8);
  memcpy((uint32_t *)&p_buf[6], (uint32_t *)&UARTx->SPR, 8);
  p_buf[8] = UARTx->RX_IDLE_TOCR;
  p_buf[9] = UARTx->RX_IDLE_INTCR;
  p_buf[10] = UARTx->MISCR;
  p_buf[11] = UARTx->INTMASK;
}

DATA_RAM_FUNCTION
static void uart_dlps_reg_restore(uint32_t *p_buf, UART_TypeDef *UARTx)
{
  UARTx->LCR |= (1 << 7);
  memcpy((uint32_t *)&UARTx->DLL, (uint32_t *)&p_buf[0], 8);
  UARTx->LCR &= (~(1 << 7));

  UARTx->INTID_FCR = (((p_buf[7] & BIT24) >> 21) | ((p_buf[7] & 0x7C000000) >> 18) |
                      (1));
  memcpy((uint32_t *)&UARTx->LCR, (uint32_t *)&p_buf[4], 8);
  memcpy((uint32_t *)&UARTx->SPR, (uint32_t *)&p_buf[6], 8);
  UARTx->DLH_INTCR = p_buf[2];
  UARTx->RX_IDLE_TOCR = p_buf[8];
  UARTx->RX_IDLE_INTCR = p_buf[9];
  UARTx->MISCR = p_buf[10];
  UARTx->INTMASK = p_buf[11];
}

#if defined(RTL876x_UART0_DMA_USED) || defined(RTL876x_UART1_DMA_USED)
DATA_RAM_FUNCTION
static void dma_dlps_reg_clear(GDMA_TypeDef *GDMAx)
{
  GDMAx->CLEAR_TFR = 0xff;
  GDMAx->CLEAR_BLOCK = 0xff;
  GDMAx->CLEAR_DST_TRAN = 0xff;
  GDMAx->CLEAR_SRC_TRAN = 0xff;
  GDMAx->CLEAR_ERR = 0xff;
}

static void dma_dlps_reg_store(uint32_t *p_buf, GDMA_TypeDef *GDMAx)
{
  p_buf[0] = GDMAx->DmaCfgReg;
  p_buf[1] = GDMAx->ChEnReg;
  p_buf[2] = GDMAx->MASK_TFR;
  p_buf[3] = GDMAx->MASK_BLOCK;
  p_buf[4] = GDMAx->MASK_SRC_TRAN;
  p_buf[5] = GDMAx->MASK_DST_TRAN;
  p_buf[6] = GDMAx->MASK_ERR;
}

DATA_RAM_FUNCTION
static void dma_dlps_reg_restore(uint32_t *p_buf, GDMA_TypeDef *GDMAx)
{
  GDMAx->DmaCfgReg = p_buf[0];
  dma_dlps_reg_clear(GDMAx);

  GDMAx->MASK_TFR = (p_buf[2] | ((p_buf[2] & 0xff) << 8));
  GDMAx->MASK_BLOCK = (p_buf[3] | ((p_buf[3] & 0xff) << 8));
  GDMAx->MASK_SRC_TRAN = (p_buf[4] | ((p_buf[4] & 0xff) << 8));
  GDMAx->MASK_DST_TRAN = (p_buf[5] | ((p_buf[5] & 0xff) << 8));
  GDMAx->MASK_ERR = (p_buf[6] | ((p_buf[6] & 0xff) << 8));
}

DATA_RAM_FUNCTION
static void dma_channel_dlps_reg_store(uint32_t *p_buf, GDMA_ChannelTypeDef *GDMA_Channelx)
{
  p_buf[0] = GDMA_Channelx->SAR;
  p_buf[1] = GDMA_Channelx->DAR;
  p_buf[2] = GDMA_Channelx->CTL_LOW;
  p_buf[3] = GDMA_Channelx->CTL_HIGH;
  p_buf[4] = GDMA_Channelx->CFG_LOW;
  p_buf[5] = GDMA_Channelx->CFG_HIGH;
}

DATA_RAM_FUNCTION
static void dma_channel_dlps_reg_restore(uint32_t *p_buf, GDMA_ChannelTypeDef *GDMA_Channelx)
{
  GDMA_Channelx->SAR      = p_buf[0];
  GDMA_Channelx->DAR      = p_buf[1];
  GDMA_Channelx->CTL_LOW  = p_buf[2];
  GDMA_Channelx->CTL_HIGH = p_buf[3];
  GDMA_Channelx->CFG_LOW  = p_buf[4];
  GDMA_Channelx->CFG_HIGH = p_buf[5];
}

#endif
#endif

#ifdef CONFIG_RTL876x_UART0
DATA_RAM_FUNCTION
static void uart0_dlps_enter(void)
{
  RCC_PeriphClockCmd(APBPeriph_UART0, APBPeriph_UART0_CLOCK, ENABLE);
  uart_dlps_reg_store((uint32_t *)&uart0_storereg[0], UART0);

#if defined(RTL876x_UART0_DMA_USED)
  RCC_PeriphClockCmd(APBPeriph_GDMA, APBPeriph_GDMA_CLOCK, ENABLE);
  dma_dlps_reg_store((uint32_t *)&gdma_storereg[0], GDMA_BASE);

#if defined(CONFIG_SERIAL_TXDMA) && defined(CONFIG_RTL876x_UART0_TXDMA)
  dma_channel_dlps_reg_store((uint32_t *)&uart0_txdma_storereg[0], g_uart0priv.txdma_base);
#endif
#if defined(CONFIG_SERIAL_RXDMA) && defined(CONFIG_RTL876x_UART0_RXDMA)
  dma_channel_dlps_reg_store((uint32_t *)&uart0_rxdma_storereg[0], g_uart0priv.rxdma_base);
  uart0_rxdma_storereg[3] = 32;
#endif
#endif
    return;
}

DATA_RAM_FUNCTION
static void uart0_dlps_exit(void)
{
  RCC_PeriphClockCmd(APBPeriph_UART0, APBPeriph_UART0_CLOCK, ENABLE);
  uart_dlps_reg_restore((uint32_t *)&uart0_storereg[0], UART0);

#if defined(RTL876x_UART0_DMA_USED)
  RCC_PeriphClockCmd(APBPeriph_GDMA, APBPeriph_GDMA_CLOCK, ENABLE);
  dma_dlps_reg_restore((uint32_t *)&gdma_storereg[0], GDMA_BASE);

#if defined(CONFIG_SERIAL_TXDMA) && defined(CONFIG_RTL876x_UART0_TXDMA)
  dma_channel_dlps_reg_restore((uint32_t *)&uart0_txdma_storereg[0], g_uart0priv.txdma_base);
#endif
#if defined(CONFIG_SERIAL_RXDMA) && defined(CONFIG_RTL876x_UART0_RXDMA)
  dma_channel_dlps_reg_restore((uint32_t *)&uart0_rxdma_storereg[0], g_uart0priv.rxdma_base);
#endif
  dma_dlps_reg_clear(GDMA_BASE);
  GDMA_Cmd(g_uart0priv.rxdma_channel_num, ENABLE);
#endif
  return;
}

#if (defined CONFIG_RTL876x_UART0_CONSOLE)
static void uart0_weakup_handler(void)
{
  if (System_WakeUpInterruptValue(CONFIG_RTL876x_UART0_RX_PIN) == SET)
    {
      System_WakeUpPinDisable(CONFIG_RTL876x_UART0_RX_PIN);
      pm_activity(PM_IDLE_DOMAIN,
                  CONFIG_SERIAL_PM_ACTIVITY_PRIORITY);
    }
}
#endif

static struct dlps_driver_ops rtl_pm_uart0_driver_ops
	__attribute__((section(".dlps.dataon.data")))
	__attribute__((__used__)) = {
    NULL,
    uart0_dlps_enter,
    uart0_dlps_exit,
#if (defined CONFIG_RTL876x_UART0_CONSOLE)
    uart0_weakup_handler,
#endif
};
#endif

#ifdef CONFIG_RTL876x_UART1
DATA_RAM_FUNCTION
static void uart1_dlps_enter(void)
{
  RCC_PeriphClockCmd(APBPeriph_UART1, APBPeriph_UART1_CLOCK, ENABLE);
  uart_dlps_reg_store((uint32_t *)&uart1_storereg[0], UART1);

#if defined(RTL876x_UART1_DMA_USED)
  RCC_PeriphClockCmd(APBPeriph_GDMA, APBPeriph_GDMA_CLOCK, ENABLE);
  dma_dlps_reg_store((uint32_t *)&gdma_storereg[0], GDMA_BASE);

#if defined(CONFIG_SERIAL_TXDMA) && defined(CONFIG_RTL876x_UART1_TXDMA)
  dma_channel_dlps_reg_store((uint32_t *)&uart1_txdma_storereg[0], g_uart1priv.txdma_base);
#endif
#if defined(CONFIG_SERIAL_RXDMA) && defined(CONFIG_RTL876x_UART1_RXDMA)
  dma_channel_dlps_reg_store((uint32_t *)&uart1_rxdma_storereg[0], g_uart1priv.rxdma_base);
#endif
#endif
    return;
}

DATA_RAM_FUNCTION
static void uart1_dlps_exit(void)
{
  RCC_PeriphClockCmd(APBPeriph_UART1, APBPeriph_UART1_CLOCK, ENABLE);
  uart_dlps_reg_restore((uint32_t *)&uart1_storereg[0], UART1);

#if defined(RTL876x_UART1_DMA_USED)
  RCC_PeriphClockCmd(APBPeriph_GDMA, APBPeriph_GDMA_CLOCK, ENABLE);
  dma_dlps_reg_restore((uint32_t *)&gdma_storereg[0], GDMA_BASE);

#if defined(CONFIG_SERIAL_TXDMA) && defined(CONFIG_RTL876x_UART1_TXDMA)
  dma_channel_dlps_reg_restore((uint32_t *)&uart1_txdma_storereg[0], g_uart1priv.txdma_base);
#endif
#if defined(CONFIG_SERIAL_RXDMA) && defined(CONFIG_RTL876x_UART1_RXDMA)
  dma_channel_dlps_reg_restore((uint32_t *)&uart1_rxdma_storereg[0], g_uart1priv.rxdma_base);
#endif
  dma_dlps_reg_clear(GDMA_BASE);
#endif

  return;
}

#if (defined CONFIG_RTL876x_UART1_CONSOLE)
static void uart1_weakup_handler(void)
{
  if (System_WakeUpInterruptValue(CONFIG_RTL876x_UART1_RX_PIN) == SET)
    {
      System_WakeUpPinDisable(CONFIG_RTL876x_UART1_RX_PIN);
      pm_activity(PM_IDLE_DOMAIN,
                  CONFIG_SERIAL_PM_ACTIVITY_PRIORITY);
    }
}
#endif

static struct dlps_driver_ops rtl_pm_uart1_driver_ops
	__attribute__((section(".dlps.dataon.data")))
	__attribute__((__used__)) = {
    NULL,
    uart1_dlps_enter,
    uart1_dlps_exit,
#if (defined CONFIG_RTL876x_UART1_CONSOLE)
    uart1_weakup_handler,
#endif
};

#endif
#endif

#endif /* CONFIG_RTL876x_UART */

