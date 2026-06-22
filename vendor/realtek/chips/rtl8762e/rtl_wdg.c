/****************************************************************************
 * arch/arm/src/rtl8762e/rtl_wdg.c
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
#include <nuttx/arch.h>

#include <inttypes.h>
#include <assert.h>
#include <errno.h>
#include <debug.h>

#include <nuttx/irq.h>
#include <nuttx/clock.h>
#include <nuttx/timers/watchdog.h>
#include <arch/board/board.h>

#include "arm_internal.h"
#include "rtl876x.h"
#include "rtl876x_wdg.h"
#include <string.h>

#ifdef CONFIG_WATCHDOG
#define WDG_MAXTIMEOUT_MS   8386560

extern uint32_t __rtl_platform_rtc_get_counter(void);

/****************************************************************************
 * Private Function prototypes
 ****************************************************************************/
static int      rtl_start(struct watchdog_lowerhalf_s *lower);
static int      rtl_stop(struct watchdog_lowerhalf_s *lower);
static int      rtl_keepalive(struct watchdog_lowerhalf_s *lower);
static int      rtl_getstatus(struct watchdog_lowerhalf_s *lower,
                  struct watchdog_status_s *status);
static int      rtl_settimeout(struct watchdog_lowerhalf_s *lower,
                  uint32_t timeout);
static xcpt_t   rtl_capture(struct watchdog_lowerhalf_s *lower,
                            xcpt_t handler);
static int      rtl_wdg_handler(int irq, void *context, void *arg);
/****************************************************************************
 * Private Data
 ****************************************************************************/
static const struct watchdog_ops_s g_wdgops =
{
  .start      = rtl_start,
  .stop       = rtl_stop,
  .keepalive  = rtl_keepalive,
  .getstatus  = rtl_getstatus,
  .settimeout = rtl_settimeout,
  .capture    = rtl_capture,
  .ioctl      = NULL,
};

/****************************************************************************
 * Private Types
 ****************************************************************************/

/* This structure provides the private representation of the "lower-half"
 * driver state structure.  This structure must be cast-compatible with the
 * well-known watchdog_lowerhalf_s structure.
 */

struct rtl_lowerhalf_s
{
  const struct watchdog_ops_s  *ops; /* Lower half operations */
  uint32_t timeout;                  /* The (actual) selected timeout, unit: 31.25us */
  uint32_t lastreset;                /* The last reset time, unit: 31.25us  */
  uint32_t nexttimeout;              /* The next timeout time, unit: 31.25us  */
  bool     started;                  /* true: The watchdog timer has been started */
  void     *upper;                   /* Pointer to watchdog_upperhalf_s */
  xcpt_t   handler;                  /* User interrupt handler */
  T_WDG_MODE mode;
};

/* "Lower half" driver state */

struct rtl_lowerhalf_s g_wdgdev;

/****************************************************************************
 * Private Functions
 ****************************************************************************/
/****************************************************************************
 * Name: rtl_wdg_handler
 *
 *   WDG early warning interrupt
 ****************************************************************************/

static int rtl_wdg_handler(int irq, void *context, void *arg)
{
  struct rtl_lowerhalf_s *priv = arg;
  if (priv->handler)
    {
      priv->handler(irq, context, arg);
    }

  return OK;
}

/****************************************************************************
 * Name: rtl_start
 *
 * Start the watchdog timer, resetting the time to the current timeout.
 ****************************************************************************/
static int rtl_start(struct watchdog_lowerhalf_s *lower)
{
  struct rtl_lowerhalf_s *priv = (struct rtl_lowerhalf_s *)lower;

  DEBUGASSERT(priv);

  /* Have we already been started? */

  if (!priv->started)
    {
      irqstate_t flags;
      flags = enter_critical_section();
      priv->lastreset = __rtl_platform_rtc_get_counter();
      priv->nexttimeout = __rtl_platform_rtc_get_counter() + priv->timeout;

      __RTL_WDG_Enable();
      priv->started   = true;
      __RTL_WDG_Restart();
      leave_critical_section(flags);
    }

  return 0;
}

/****************************************************************************
 * Name: rtl_stop
 *
 * Stop the watchdog timer
 ****************************************************************************/
static int rtl_stop(struct watchdog_lowerhalf_s *lower)
{
  struct rtl_lowerhalf_s *priv = (struct rtl_lowerhalf_s *)lower;

  DEBUGASSERT(priv);

  if(priv->started == true)
    {
      irqstate_t flags;
      flags = enter_critical_section();
      priv->started = false;

      __RTL_WDG_Disable();
      leave_critical_section(flags);
    }

  return 0;
}

/****************************************************************************
 * Name: rtl_keepalive
 *
 * Reset the watchdog timer to the current timeout value, prevent any
 * imminent watchdog timeouts.  This is sometimes referred as "pinging" the
 * watchdog timer or "petting the dog"
 ****************************************************************************/
static int rtl_keepalive(struct watchdog_lowerhalf_s *lower)
{
  struct rtl_lowerhalf_s *priv = (struct rtl_lowerhalf_s *)lower;

  DEBUGASSERT(priv);

  irqstate_t flags;
  flags = enter_critical_section();
  priv->lastreset = __rtl_platform_rtc_get_counter();
  priv->nexttimeout = __rtl_platform_rtc_get_counter() + priv->timeout;

  __RTL_WDG_Restart();
  leave_critical_section(flags);
  return 0;
}

/****************************************************************************
 * Name: rtl_getstatus
 *
 * Get the current watchdog timer status
 ****************************************************************************/
static int rtl_getstatus(struct watchdog_lowerhalf_s *lower,
                  struct watchdog_status_s *status)
{
  struct rtl_lowerhalf_s *priv = (struct rtl_lowerhalf_s *)lower;

  DEBUGASSERT(priv);

  /* Return the status bit */

  status->flags = 0;
  if (priv->started)
    {
      status->flags |= WDFLAGS_ACTIVE;
    }

  /* Return the actual timeout in milliseconds */

  status->timeout = priv->timeout / 32;

  /* Return the approximate time until the watchdog timer expiration */

  status->timeleft = (priv->nexttimeout - __rtl_platform_rtc_get_counter()) / 32;
  return 0;
}

/****************************************************************************
 * Name: rtl_settimeout
 *
 * Set a new timeout value (and reset the watchdog timer)
 ****************************************************************************/
static int rtl_settimeout(struct watchdog_lowerhalf_s *lower,
                  uint32_t timeout)
{
  struct rtl_lowerhalf_s *priv = (struct rtl_lowerhalf_s *)lower;
  uint32_t div_factor;
  uint32_t div_tick;
  uint8_t i;
  uint32_t cnt_limit;

  DEBUGASSERT(priv);

  if (timeout < 1 || timeout > WDG_MAXTIMEOUT_MS)
    {
      return -1;
    }

  div_factor = 32 * timeout / 4095;

  if(div_factor < 1)
    {
      div_factor = 1;
    }

  div_tick = 125 * (div_factor + 1) / 4;

  i = 0;
  for(i=1; i<13; i++)
    {
      if( timeout * 1000 < div_tick * ((1 << i)- 1) )
        {
          break;
        }
    }

  cnt_limit = i - 1;

  irqstate_t flags;
  flags = enter_critical_section();
  priv->lastreset = __rtl_platform_rtc_get_counter();
  priv->timeout = timeout * 32;
  priv->nexttimeout = __rtl_platform_rtc_get_counter() + priv->timeout;
  __RTL_WDG_Disable();
  __RTL_WDG_Config(div_factor, cnt_limit, RESET_ALL);

  leave_critical_section(flags);

  return 0;
}

/****************************************************************************
 * Name: rtl_capture
 *
 * Don't reset on watchdog timer timeout; instead, call this user provider
 * timeout handler.
 ****************************************************************************/
static xcpt_t rtl_capture(struct watchdog_lowerhalf_s *lower,
                            xcpt_t handler)
{
  struct rtl_lowerhalf_s *priv = (struct rtl_lowerhalf_s *)lower;
  irqstate_t flags;
  xcpt_t oldhandler;

  DEBUGASSERT(priv);

  /* Get the old handler return value */

  flags      = enter_critical_section();
  oldhandler = priv->handler;

  /* Save the new handler */

  priv->handler = handler;

  /* Are we attaching or detaching the handler? */

  if (handler)
    {
      priv->mode = INTERRUPT_CPU;
      if (priv->started)
        {
          rtl_stop(lower);
          up_enable_irq(WDG_VECTORn);
          rtl_settimeout(lower, priv->timeout / 32);
          rtl_start(lower);
        }
      else
        {
          up_enable_irq(WDG_VECTORn);
          rtl_settimeout(lower, priv->timeout / 32);
        }
    }
  else
    {
      priv->mode = RESET_ALL;
      if (priv->started)
        {
          rtl_stop(lower);
          up_disable_irq(WDG_VECTORn);
          rtl_settimeout(lower, priv->timeout / 32);
          rtl_start(lower);
        }
      else
        {
          up_disable_irq(WDG_VECTORn);
          rtl_settimeout(lower, priv->timeout / 32);
        }
    }

  leave_critical_section(flags);
  return oldhandler;
}

/****************************************************************************
 * Public Functions
 ****************************************************************************/

/****************************************************************************
 * Name: rtl_iwdginitialize
 *
 * Description:
 *   Initialize the IWDG watchdog timer.  The watchdog timer is initialized
 *   and registers as 'devpath'.  The initial state of the watchdog timer is
 *   disabled.
 *
 * Input Parameters:
 *   devpath - The full path to the watchdog.  This should be of the form
 *     /dev/watchdog0
 *   lsifreq - The calibrated LSI clock frequency
 *
 * Returned Value:
 *   None
 *
 ****************************************************************************/

int rtl_iwdginitialize(void)
{
  struct rtl_lowerhalf_s *priv = &g_wdgdev;
  DEBUGASSERT(priv);

  /* NOTE we assume that clocking to the IWDG has already been provided by
  * the RCC initialization logic.
  */

  /* Initialize the driver state structure. */

  priv->ops     = &g_wdgops;
  priv->mode    = RESET_ALL;
  __RTL_WDG_ClockEnable();
#if defined(CONFIG_RTL876x_WDG_INTERVAL_MS)
  rtl_settimeout((struct watchdog_lowerhalf_s *)priv,
                                  CONFIG_RTL876x_WDG_INTERVAL_MS);
#else
  rtl_settimeout((struct watchdog_lowerhalf_s *)priv,
                                  1000);
#endif
  __RTL_WDG_Disable();
  priv->started   = false;
  irq_attach(WDG_VECTORn, rtl_wdg_handler, priv);
  priv->upper = watchdog_register(CONFIG_WATCHDOG_DEVPATH, \
                      (struct watchdog_lowerhalf_s *)priv);
  if (priv->upper != NULL)
    {
      return OK;
    }
  else
    {
      return -EEXIST;
    }
}

#endif /* CONFIG_WATCHDOG */

