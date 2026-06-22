/****************************************************************************
 * arch/arm/src/rtl8762e/rtl_serial.h
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


#ifndef __ARCH_ARM_SRC_RTL_RTL876X_UART_H
#define __ARCH_ARM_SRC_RTL_RTL876X_UART_H

/****************************************************************************
 * Included Files
 ****************************************************************************/
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

#include "rtl876x_rcc.h"
#include "rtl876x_uart.h"
#include "rtl876x_nvic.h"
#include "rtl876x_pinmux.h"
/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

#ifdef CONFIG_RTL876x_UART


#if defined(CONFIG_SERIAL_TXDMA) && \
   (defined(CONFIG_RTL876x_UART0_TXDMA) || \
    defined(CONFIG_RTL876x_UART1_TXDMA))
#define RTL876x_UART_HAVE_TXDMA
#endif

#if defined(CONFIG_SERIAL_RXDMA) && \
   (defined(CONFIG_RTL876x_UART0_RXDMA) || \
    defined(CONFIG_RTL876x_UART1_RXDMA))
#define RTL876x_UART_HAVE_RXDMA
#endif

#if defined(CONFIG_SERIAL_TXDMA) && defined(CONFIG_SERIAL_RXDMA) && \
    ((defined(CONFIG_RTL876x_UART0_RXDMA) && \
     defined(CONFIG_RTL876x_UART0_TXDMA)) || \
    (defined(CONFIG_RTL876x_UART1_RXDMA) && \
     defined(CONFIG_RTL876x_UART1_TXDMA)))
#define RTL876x_UART_HAVE_TXRXDMA
#endif

#if (defined(CONFIG_SERIAL_RXDMA) && defined(CONFIG_RTL876x_UART0_RXDMA)) \
 || (defined(CONFIG_SERIAL_TXDMA) && defined(CONFIG_RTL876x_UART0_TXDMA)) 
#define RTL876x_UART0_DMA_USED
#endif

#if (defined(CONFIG_SERIAL_RXDMA) && defined(CONFIG_RTL876x_UART1_RXDMA)) \
 || (defined(CONFIG_SERIAL_TXDMA) && defined(CONFIG_RTL876x_UART1_TXDMA)) 
#define RTL876x_UART1_DMA_USED
#endif

#if (!defined(RTL876x_UART0_DMA_USED) && defined(CONFIG_RTL876x_UART0)) \
 || (!defined(RTL876x_UART1_DMA_USED) && defined(CONFIG_RTL876x_UART1))
#define RTL876x_UART_HAVE_NODMA
#endif

#if (defined(CONFIG_RTL876x_UART0) && \
   (!defined(CONFIG_SERIAL_RXDMA) || !defined(CONFIG_RTL876x_UART0_RXDMA))) || \
    (defined(CONFIG_RTL876x_UART1) && \
   (!defined(CONFIG_SERIAL_RXDMA) || !defined(CONFIG_RTL876x_UART1_RXDMA)))
#define RTL876x_UART_HAVE_RXNODMA
#endif

#if (defined(CONFIG_RTL876x_UART0) && \
   (!defined(CONFIG_SERIAL_TXDMA) || !defined(CONFIG_RTL876x_UART0_TXDMA))) || \
    (defined(CONFIG_RTL876x_UART1) && \
   (!defined(CONFIG_SERIAL_TXDMA) || !defined(CONFIG_RTL876x_UART1_TXDMA)))
#define RTL876x_UART_HAVE_TXNODMA
#endif

#if !defined(CONFIG_SERIAL_RXDMA)
#define RTL876x_UART_NO_RXDMA
#endif

#if !defined(CONFIG_SERIAL_TXDMA)
#define RTL876x_UART_NO_TXDMA
#endif

#if      defined(RTL876x_UART_NO_RXDMA) && defined(RTL876x_UART_NO_TXDMA)
#define RTL876x_UART_NO_DMA
#endif

/****************************************************************************
 * Public Function Prototypes
 ****************************************************************************/

#ifdef USE_SERIALDRIVER

#ifdef USE_EARLYSERIALINIT
void arm_earlyserialinit(void);
#endif

void arm_serialinit(void);
int up_putc(int ch);

#else /* USE_SERIALDRIVER */

int up_putc(int ch);

#endif /* USE_SERIALDRIVER */

#endif /* CONFIG_RTL876x_UART */
#endif /* __ARCH_ARM_SRC_RTL_RTL876X_UART_H */
