/****************************************************************************
 * vendor/realtek/chips/rtl8762e/rtl876x_boot.c
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
#include <inttypes.h>
#include <assert.h>
#include <debug.h>
#include <sched.h>
#include <nuttx/timers/oneshot.h>
#include <nuttx/mm/mm.h>
#include <nuttx/crypto/crypto.h>
#include <nuttx/arch.h>
#include <nuttx/i2c/i2c_master.h>
#include <nuttx/analog/adc.h>
#include <sys/boardctl.h>
#include <nvic.h>
#include "time.h"
#include "clock/clock.h"
#include "arm_internal.h"
#include "mem_config.h"
#include "mem_types.h"
#include "patch_os.h"
#include "system_rtl876x.h"
#include "irq.h"
#include "rtl876x_wdg.h"
#include "rtl_flash.h"
#include "otp.h"
#include "dlps.h"
#include "app_section.h"
#include "rtl876x_driver_sample.h"
#include "rtl876x_wdg.h"

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

/****************************************************************************
 * Public Data
 ****************************************************************************/

/****************************************************************************
 * Private Functions
 ****************************************************************************/
DATA_RAM_FUNCTION
static void system_restore_change_msp(void)
{
  extern void __rtl_power_manager_cpu_restore(void);

  __set_MSP(((uint32_t)_ebss + CONFIG_IDLETHREAD_STACKSIZE));
  __ISB();
  __rtl_power_manager_cpu_restore();
}

/****************************************************************************
 * Public Functions
 ****************************************************************************/

/****************************************************************************
 * Name: board_early_initialize
 *
 * Description:
 *   Reset board.  Support for this function is required by board-level
 *   logic if CONFIG_BOARDCTL_RESET is selected.
 *
 * Input Parameters:
 *   status - Status information provided with the reset event.  This
 *            meaning of this status information is board-specific.  If not
 *            used by a board, the value zero may be provided in calls to
 *            board_reset().
 *
 * Returned Value:
 *   If this function returns, then it was not possible to power-off the
 *   board due to some constraints.  The return value int this case is a
 *   board-specific reason for the failure to shutdown.
 *
 ****************************************************************************/

void board_early_initialize(void)
{
  volatile int ret;

  __rtl_ota_header_addr = 0;
#ifndef CONFIG_RTL876x_BT_DBG
  OTP->logDisable = 1;
#else
  OTP->logDisable = 0;
#endif
  __RTL_SystemInit();
  putreg32((uint32_t)_vectors, ARMV6M_SYSCON_VECTAB);

  __rtl_hal_setup_hardware();
  __rtl_hal_setup_cpu();
  extern bool rtl_table_update(void);
  rtl_table_update();

#ifdef CONFIG_WATCHDOG
  extern int rtl_iwdginitialize(void);
  ret = rtl_iwdginitialize();
  if (ret == 0)
    {
      ferr("wdg init okay\n");
    }
  else
    {
      ferr("wdg is null\n");
    }
#endif

#if defined(CONFIG_RTL876x_I2C0_MASTER)
  extern struct i2c_master_s *rtl_i2cbus_uninitialize(int port);
  extern struct i2c_master_s *rtl_i2cbus_initialize(int port);

  struct i2c_master_s *i2c = rtl_i2cbus_initialize(0);
  if (i2c == NULL)
    {
      ferr("i2c0 init failed\n");
    }

  ret = i2c_register(i2c, 0);
  if (ret < 0)
    {
      ferr("ERROR: Failed to register i2c0: %d\n", ret);
      (void)rtl_i2cbus_uninitialize(0);
    }
#endif

#ifdef CONFIG_RTL876x_FLASH
#ifdef CONFIG_MTD_CONFIG_FAIL_SAFE
  ret = rtl876x_partition_init();
  if (ret == 0)
    {
      ferr("mtd init okay\n");
    }
  else
    {
      ferr("mtd is null\n");
    }
#endif
#endif

#ifdef CONFIG_FS_PROCFS
  /* Mount the procfs file system */

  ret = nx_mount(NULL, "/proc", "procfs", 0, NULL);
  if (ret < 0)
    {
      syslog(LOG_ERR, "ERROR: Failed to mount procfs at /proc: %d\n",
             ret);
    }
#endif

#ifdef CONFIG_FS_TMPFS
  /* Mount the tmpfs file system */

  ret = nx_mount(NULL, "/tmp", "tmpfs", 0, NULL);
  if (ret < 0)
    {
      syslog(LOG_ERR, "ERROR: Failed to mount tmpfs at /tmp: %d\n",
             ret);
    }
#endif

#ifdef CONFIG_RTL876x_AES
  extern void __rtl_hw_aes_create_mutex(void);
  __rtl_hw_aes_create_mutex();
#endif

  __rtl_power_manager_init();

#ifdef CONFIG_PM
  __rtl_platform_pm_init();
#endif

#ifdef CONFIG_BT
  __rtl_init_osc_sdm_timer();
  __rtl_low_stack_init();
  NVIC_SetPriority(2, 0);
#endif

#if CONFIG_MM_REGIONS > 1
  extern void rtl_addregion(void);
  rtl_addregion();
#endif

  extern void (*__rtl_system_restore)(void);
  __rtl_system_restore = system_restore_change_msp;

#ifdef CONFIG_RTL876x_DRIVER_SAMPLE
  __rtl_driver_sample();
#endif
}

/****************************************************************************
 * Name: board_reset
 *
 * Description:
 *   Reset board.  Support for this function is required by board-level
 *   logic if CONFIG_BOARDCTL_RESET is selected.
 *
 * Input Parameters:
 *   status - Status information provided with the reset event.  This
 *            meaning of this status information is board-specific.  If not
 *            used by a board, the value zero may be provided in calls to
 *            board_reset().
 *
 * Returned Value:
 *   If this function returns, then it was not possible to power-off the
 *   board due to some constraints.  The return value int this case is a
 *   board-specific reason for the failure to shutdown.
 *
 ****************************************************************************/

int board_reset(int status)
{
//   up_systemreset();
  __RTL_WDG_SystemReset(RESET_ALL_EXCEPT_AON, status);
  return 0;
}

/****************************************************************************
 * Name: board_app_initialize
 *
 * Description:
 *   Perform architecture specific initialization
 *
 * Input Parameters:
 *   arg - The boardctl() argument is passed to the board_app_initialize()
 *         implementation without modification.  The argument has no
 *         meaning to NuttX; the meaning of the argument is a contract
 *         between the board-specific initialization logic and the
 *         matching application logic.  The value could be such things as a
 *         mode enumeration value, a set of DIP switch switch settings, a
 *         pointer to configuration data read from a file or serial FLASH,
 *         or whatever you would like to do with it.  Every implementation
 *         should accept zero/NULL as a default configuration.
 *
 * Returned Value:
 *   Zero (OK) is returned on success; a negated errno value is returned on
 *   any failure to indicate the nature of the failure.
 *
 ****************************************************************************/

int board_app_initialize(uintptr_t arg)
{
  return OK;
}

#ifdef CONFIG_BOARDCTL_RESET_CAUSE
int board_reset_cause(struct boardioc_reset_cause_s *cause)
{
  uint32_t wdg_reset_lr = (__rtl_btaon_fast_read_safe(0x1c6) << 16) | \
                                (__rtl_btaon_fast_read_safe(0x1c4));
  cause->cause = BOARDIOC_RESETCAUSE_NONE;
  if (wdg_reset_lr == 0)
    {
      cause->cause = BOARDIOC_RESETCAUSE_SYS_CHIPPOR;
    }
  else
    {
      cause->cause = BOARDIOC_RESETCAUSE_SYS_RWDT;
    }

  return 0K;
}
#endif
