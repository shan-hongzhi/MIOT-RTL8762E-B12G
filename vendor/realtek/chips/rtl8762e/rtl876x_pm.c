/****************************************************************************
 * vendor/realtek/chips/rtl8762e/rtl876x_pm.c
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
#include <nuttx/power/pm.h>
#include <errno.h>
#include "arm_internal.h"
#include "rtl_serial.h"
#include "dlps.h"
#include "trace.h"
#include "patch.h"
#include "patch_os.h"
#include "rtl876x_nvic.h"
#include "rtl876x_pinmux.h"
#include "rtl876x_rcc.h"
#include "rtl876x_bitfields.h"
#include "rtl876x_wdg.h"
#if defined CONFIG_RTL876x_AON_WDG
#include "rtl876x_aon_wdg.h"
#endif
/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/
#if defined CONFIG_PM
#define US_TO_TICK_THRESHOLD                        (0x3FFFFFFF)
#define US_TO_PF_RTC_TICK(us)                       (((us) > US_TO_TICK_THRESHOLD)?(((us)/125) << 2):(((us) << 2)/125))
#define S_TO_PF_RTC_TICK(us)                        ((us)*125 << 8)

#if defined CONFIG_PM_DEBUG
#define TEST_PM_STATE_PIN0 P0_0
#define TEST_PM_STATE_PIN1 P4_0
#endif

#define DATA_RAM_FUNCTION  __attribute__((section(".app.data_ram.text")))

static int pm_prepare(struct pm_callback_s *cb, int domain,
                         enum pm_state_e pmstate);

static void up_pm_notify(struct pm_callback_s *cb, int domain,
                          enum pm_state_e pmstate);
typedef enum
{
    PM_UNIT_ACTIVE          = 0,
    PM_UNIT_INACTIVE        = 1,
    PM_UNIT_UNKNOWN         = 2,
} PMUnitStatus;

typedef bool (*PMUnitWFICheckFunc)(void);
typedef struct _PowerManagerUnit
{
    void * reserved[5];
    PMUnitWFICheckFunc wfi_check_func;
    uint16_t name;
    uint8_t status;
    void * reserved1;
    bool check_result;
    uint32_t wakeup_time;
    uint32_t suspended;
    void * reserved2;
} PowerManagerUnit;
typedef struct _PowerManagerSystem
{
    PowerManagerUnit *unit_array[2][1];
    uint32_t allow_wakeup_system_level;
    uint32_t suspended;
} PowerManagerSystem;

extern void (*__rtl_power_manager_store_and_enter)(uint32_t,
                                      PowerManagerUnit *);
extern void (*__rtl_power_manager_check)(uint32_t, PowerManagerUnit *);
extern PowerManagerSystem __rtl_power_manager_system;
/****************************************************************************
 * Private Data
 ****************************************************************************/

static struct pm_callback_s g_platform_pm =
{
  .prepare    = pm_prepare,
  .notify     = up_pm_notify,
};

volatile uint32_t Pinmux_StoreReg[10];
volatile uint32_t CPU_StoreReg[3];
volatile uint32_t CPU_StoreReg_IP[8];
volatile uint32_t PeriIntStoreReg = 0;

static irqstate_t irq_flags;
/****************************************************************************
 * Public Data
 ****************************************************************************/

/****************************************************************************
 * Private Functions
 ****************************************************************************/
DATA_RAM_FUNCTION
static int pm_prepare(struct pm_callback_s *cb, int domain,
                         enum pm_state_e pmstate)
{
  PowerManagerUnit *p_cur_unit = __rtl_power_manager_system.unit_array[1][0];
  switch (pmstate)
    {
    case PM_NORMAL:
      break;
    case PM_IDLE:
      if (p_cur_unit->wfi_check_func())
        {
          return OK;
        }
        else
        {
          return -EBUSY;
        }
      break;

    case PM_STANDBY:
    case PM_SLEEP:
      extern uint32_t __rtl_power_manager_get_system_level_wakeup_time(uint32_t val);
      extern bool __rtl_power_manager_get_system_level_check_result(uint32_t val);

      uint32_t pre_sys_lv_wakeup_time = 0xffffffff;
      pre_sys_lv_wakeup_time = __rtl_power_manager_get_system_level_wakeup_time(0);
      if (__rtl_power_manager_get_system_level_check_result(0) != true)
       {
          return -EBUSY;
       }

      __rtl_power_manager_check(pre_sys_lv_wakeup_time, p_cur_unit);
      if ((p_cur_unit->status == PM_UNIT_ACTIVE) &&
                        (p_cur_unit->check_result == true))
        {
          return OK;
        }
      else
        {
          return -EBUSY;
        }
      break;
    default:
      break;
    }

  return OK;
}

static void up_pm_notify(struct pm_callback_s *cb, int domain,
                          enum pm_state_e pmstate)
{
  switch (pmstate)
    {
      case(PM_RESTORE):
        {
#if defined CONFIG_RTL876x_AON_WDG
          aon_wdg_disable();
#endif
        }
        break;

      case(PM_STANDBY):
      case(PM_SLEEP):
        {
#if defined CONFIG_RTL876x_AON_WDG
          aon_wdg_enable();
#endif
        }
        break;

      default:
        {
          /* Should not get here */
        }
        break;
    }
}

DATA_RAM_FUNCTION
PlatformPowerMode pm_state_to_rtl_power_mode(enum pm_state_e state)
{
  switch (state)
    {
      case PM_NORMAL:
        break;

      case PM_IDLE:
        break;

      case PM_STANDBY:
        return PLATFORM_DLPS_PFM;

      case PM_SLEEP:
        return PLATFORM_DLPS_PFM;
        break;

      default:
        break;
    }

    return PLATFORM_ACTIVE;
}

/****************************************************************************
 * Name: up_idlepm
 *
 * Description:
 *   Perform IDLE state power management.
 *
 ****************************************************************************/
DATA_RAM_FUNCTION
static void up_idlepm(void)
{
  enum pm_state_e oldstate = PM_NORMAL;
  enum pm_state_e newstate;
  PlatformPowerMode state;
  int ret;

  /* Decide, which power saving level can be obtained */

  newstate = pm_checkstate(PM_IDLE_DOMAIN);

  /* Check for state changes */

  if (newstate != oldstate)
    {
#ifdef DBG_PM
      DBG_DIRECT("newstate %d oldstate %d", newstate, oldstate);
#endif

#ifdef CONFIG_BT
      extern void (*__rtl_power_manager_handler)(void);
      __rtl_power_manager_handler();
#endif

      irq_flags = enter_critical_section();
      sched_lock();
      /* MCU-specific power management logic */

      state = pm_state_to_rtl_power_mode(newstate);
      lps_mode_set(state);
      PowerManagerUnit *p_cur_unit = __rtl_power_manager_system.unit_array[1][0];

      /* Then force the global state change */

      ret = pm_changestate(PM_IDLE_DOMAIN, newstate);

      if (ret < 0)
        {
          sched_unlock();
          leave_critical_section(irq_flags);
          return;
        }

      switch (newstate)
        {
        case PM_NORMAL:
          break;

        case PM_IDLE:
          __WFI();
          break;

        case PM_STANDBY:
#if defined CONFIG_PM_DEBUG
          Pad_Config(TEST_PM_STATE_PIN0, PAD_SW_MODE, PAD_IS_PWRON,
                      PAD_PULL_NONE, PAD_OUT_ENABLE, PAD_OUT_HIGH);
#endif
          __rtl_power_manager_store_and_enter(1, p_cur_unit);
          break;

        case PM_SLEEP:
#if defined CONFIG_PM_DEBUG
          Pad_Config(TEST_PM_STATE_PIN0, PAD_SW_MODE, PAD_IS_PWRON,
                      PAD_PULL_DOWN, PAD_OUT_ENABLE, PAD_OUT_HIGH);
          Pad_Config(TEST_PM_STATE_PIN1, PAD_SW_MODE, PAD_IS_PWRON,
                              PAD_PULL_NONE, PAD_OUT_DISABLE, PAD_OUT_LOW);
          System_WakeUpPinEnable(TEST_PM_STATE_PIN1,
                          PAD_WAKEUP_POL_HIGH, 0, 0);
#endif
          __rtl_power_manager_store_and_enter(1, p_cur_unit);
          break;

        default:
          break;
        }
      sched_unlock();
      leave_critical_section(irq_flags);

    }

}

/****************************************************************************
 * Function:  up_idle
 *
 * Description:
 *   idle task
 ****************************************************************************/

DATA_RAM_FUNCTION
void up_idle(void)
{
  while (1)
    {
      __RTL_LogUartDMAIdleHook();
#if ((defined CONFIG_SUPPRESS_INTERRUPTS) || \
                                    (defined CONFIG_SUPPRESS_TIMER_INTS))
      nxsched_process_timer();
#else

      /* Perform IDLE mode power management */
      __RTL_WDG_Restart();
      up_idlepm();

#if defined CONFIG_PM_DEBUG
      Pad_Config(TEST_PM_STATE_PIN0, PAD_SW_MODE, PAD_IS_PWRON,
                  PAD_PULL_NONE, PAD_OUT_ENABLE, PAD_OUT_LOW);
      Pad_ControlSelectValue(TEST_PM_STATE_PIN1, PAD_PINMUX_MODE);
      Pinmux_Config(TEST_PM_STATE_PIN1, DWGPIO);

      RCC_PeriphClockCmd(APBPeriph_GPIO, APBPeriph_GPIO_CLOCK, ENABLE);
      GPIO_InitTypeDef GPIO_InitStruct;
      GPIO_StructInit(&GPIO_InitStruct);
      GPIO_InitStruct.GPIO_Pin= GPIO_GetPin(TEST_PM_STATE_PIN1);
      GPIO_InitStruct.GPIO_Mode= GPIO_Mode_IN;
      GPIO_InitStruct.GPIO_ITCmd= DISABLE;

      GPIO_Init(&GPIO_InitStruct);
#endif

    }
#endif
}

/****************************************************************************
 * Public Functions
 ****************************************************************************/

/****************************************************************************
 * Name: arm_pminitialize
 *
 * Description:
 *   This function is called by MCU-specific logic at power-on reset in
 *   order to provide one-time initialization the power management subsystem.
 *   This function must be called *very* early in the initialization sequence
 *   *before* any other device drivers are initialized (since they may
 *   attempt to register with the power management subsystem).
 *
 * Input Parameters:
 *   None.
 *
 * Returned Value:
 *   None.
 *
 ****************************************************************************/

DATA_RAM_FUNCTION
void Pinmux_DLPS_Enter(void)
{
  uint8_t i = 0;

  for (i = 0; i < 10; i++)
    {
      Pinmux_StoreReg[i] = PINMUX->CFG[i];
    }

  return;
}

DATA_RAM_FUNCTION
void pinmux_dlps_exit(void)
{
  uint8_t i;

  for (i = 0; i < 10; i++)
    {
      PINMUX->CFG[i] = Pinmux_StoreReg[i];
    }

  return;
}

DATA_RAM_FUNCTION
void CPU_DLPS_Enter(void)
{
  uint32_t i;

  CPU_StoreReg[0] = NVIC->ISER[0];
  CPU_StoreReg[1] = NVIC->ISPR[0];

  for (i = 0; i < 8; ++i)
    {
      CPU_StoreReg_IP[i] = NVIC->IP[i];
    }

  CPU_StoreReg[2] = SCB->VTOR;
  PeriIntStoreReg = PERIPHINT->EN;

  return;
}

DATA_RAM_FUNCTION
void CPU_DLPS_Exit(void)
{
  uint32_t i;

  if (CPU_StoreReg[0] & CPU_StoreReg[1])
    {
      OS_PRINT_WARN1("miss interrupt: pending register: 0x%x",
                    CPU_StoreReg[1]);
    }

  NVIC->IP[0] |=  CPU_StoreReg_IP[0] &
                  0xff000000;
  for (i = 1; i < 8; ++i)
    {
      NVIC->IP[i] = CPU_StoreReg_IP[i];
    }

  SCB->VTOR = CPU_StoreReg[2];
  PERIPHINT->EN = PeriIntStoreReg;
  NVIC->ISER[0] = CPU_StoreReg[0];

  return;
}

#ifdef CONFIG_RTL876x_UART0_CONSOLE

DATA_RAM_FUNCTION
void data_uart_dlps_exit_cb(void)
{
  Pad_ControlSelectValue(CONFIG_RTL876x_UART0_TX_PIN, PAD_PINMUX_MODE);
  Pad_ControlSelectValue(CONFIG_RTL876x_UART0_RX_PIN, PAD_PINMUX_MODE);
}

DATA_RAM_FUNCTION
void data_uart_dlps_enter_cb(void)
{
  Pad_ControlSelectValue(CONFIG_RTL876x_UART0_TX_PIN, PAD_SW_MODE);
  Pad_ControlSelectValue(CONFIG_RTL876x_UART0_RX_PIN, PAD_SW_MODE);
  System_WakeUpPinEnable(CONFIG_RTL876x_UART0_RX_PIN,
                          PAD_WAKEUP_POL_LOW, 0, 0);
}

#endif

#ifdef CONFIG_RTL876x_UART1_CONSOLE
DATA_RAM_FUNCTION
void data_uart_dlps_exit_cb(void)
{
  Pad_ControlSelectValue(CONFIG_RTL876x_UART1_TX_PIN, PAD_PINMUX_MODE);
  Pad_ControlSelectValue(CONFIG_RTL876x_UART1_RX_PIN, PAD_PINMUX_MODE);
}

DATA_RAM_FUNCTION
void data_uart_dlps_enter_cb(void)
{
  Pad_ControlSelectValue(CONFIG_RTL876x_UART1_TX_PIN, PAD_SW_MODE);
  Pad_ControlSelectValue(CONFIG_RTL876x_UART1_RX_PIN, PAD_SW_MODE);
  System_WakeUpPinEnable(CONFIG_RTL876x_UART1_RX_PIN,
                          PAD_WAKEUP_POL_LOW, 0, 0);
}
#endif

#if defined CONFIG_ONESHOT_DLPS_CHECK
bool oneshot_dlps_check_cb(uint32_t * p_value)
{
  *p_value = US_TO_PF_RTC_TICK(tim_remain_us());
  return true;
}
#endif

#if defined CONFIG_PM_DEBUG
bool gpio_dlps_check_cb(uint32_t * p_value)
{
  return !GPIO_ReadInputDataBit(GPIO_GetPin(TEST_PM_STATE_PIN1));
}
#endif

extern uint32_t *__dlps_dataon_start__;
extern uint32_t *__dlps_dataon_end__;

DATA_RAM_FUNCTION
static bool rtl_ops_dlps_check_cb(uint32_t * p_value)
{
  *p_value = 0x00ffffff;

  for (struct dlps_driver_ops *ops = (void *)&__dlps_dataon_start__;
        ops != (void *)&__dlps_dataon_end__; ops++)
    {
      uint32_t v = 0x00ffffff;
      bool t;

      if (ops->check_cb)
        {
          t = ops->check_cb(&v);
          if (!t)
            {
              return false;
            }

          if (v < *p_value)
            {
              *p_value = v;
            }
        }
    }

    return true;
}

DATA_RAM_FUNCTION
void System_Handler(int irq, void *context, void *arg)
{
  for (struct dlps_driver_ops *ops = (void *)&__dlps_dataon_start__;
        ops != (void *)&__dlps_dataon_end__; ops++)
    {
      if (ops->weakup_cb)
        {
          ops->weakup_cb();
        }
    }

//   Pad_ClearAllWakeupINT();
}

DATA_RAM_FUNCTION
void Log_SWD_DLPS_Enter(void)
{
    if (OTP->SWD_ENABLE)
    {
        Pad_Config(P1_0, PAD_SW_MODE, PAD_IS_PWRON, PAD_PULL_DOWN, PAD_OUT_DISABLE, PAD_OUT_LOW);
        Pad_Config(P1_1, PAD_SW_MODE, PAD_IS_PWRON, PAD_PULL_DOWN, PAD_OUT_DISABLE, PAD_OUT_LOW);
    }

    Pad_Config(OTP->logPin, PAD_SW_MODE, PAD_IS_PWRON, PAD_PULL_UP, PAD_OUT_DISABLE, PAD_OUT_LOW);
}

DATA_RAM_FUNCTION
void Log_SWD_DLPS_Exit(void)
{
    Pad_Config(OTP->logPin, PAD_PINMUX_MODE, PAD_IS_PWRON, PAD_PULL_NONE, PAD_OUT_DISABLE, PAD_OUT_LOW);

    if (OTP->SWD_ENABLE)
    {
        Pad_Config(P1_0, PAD_PINMUX_MODE, PAD_IS_PWRON, PAD_PULL_UP, PAD_OUT_DISABLE, PAD_OUT_LOW);
        Pad_Config(P1_1, PAD_PINMUX_MODE, PAD_IS_PWRON, PAD_PULL_UP, PAD_OUT_DISABLE, PAD_OUT_LOW);
    }
}

DATA_RAM_FUNCTION
void DLPS_IO_EnterDlpsCb(void)
{
  NVIC_DisableIRQ(System_IRQn);
  CPU_DLPS_Enter();

  Pinmux_DLPS_Enter();

#if defined(CONFIG_RTL876x_UART0_CONSOLE) || defined(CONFIG_RTL876x_UART1_CONSOLE)
  data_uart_dlps_enter_cb();
#endif

  for (struct dlps_driver_ops *ops = (void *)&__dlps_dataon_start__;
        ops != (void *)&__dlps_dataon_end__; ops++)
    {
      if (ops->enter_dlps_cb)
        {
          ops->enter_dlps_cb();
        }
    }

  Log_SWD_DLPS_Enter();
}

#if defined CONFIG_RTL876x_AON_WDG
DATA_RAM_FUNCTION
bool aon_watch_dog_check_cb(uint32_t * p_value)
{
  *p_value = S_TO_PF_RTC_TICK(CONFIG_RTL876x_AON_WDG_TIMEOUT_SECONDS);
  return true;
}
#endif

DATA_RAM_FUNCTION
void DLPS_IO_ExitDlpsCb(void)
{
  pinmux_dlps_exit();

  Log_SWD_DLPS_Exit();

  for (struct dlps_driver_ops *ops = (void *)&__dlps_dataon_start__;
        ops != (void *)&__dlps_dataon_end__; ops++)
    {
      if (ops->exit_dlps_cb)
        {
          ops->exit_dlps_cb();
        }
    }

#if defined(CONFIG_RTL876x_UART0_CONSOLE) || defined(CONFIG_RTL876x_UART1_CONSOLE)
  data_uart_dlps_exit_cb();
#endif

  NVIC_InitTypeDef nvic_init_struct;
  nvic_init_struct.NVIC_IRQChannel         = System_IRQn;
  nvic_init_struct.NVIC_IRQChannelCmd      = (FunctionalState)ENABLE;
  nvic_init_struct.NVIC_IRQChannelPriority = 0;
  NVIC_Init(&nvic_init_struct);

  CPU_DLPS_Exit();
}

extern void (*__rtl_platform_pm_pend)(void);

DATA_RAM_FUNCTION
bool rtl_pm_switch_to_idle(void)
{
  __rtl_platform_pm_pend();

  pm_changestate(PM_IDLE_DOMAIN, PM_RESTORE);

  /* Change to default state */
  pm_changestate(PM_IDLE_DOMAIN, PM_NORMAL);

  sched_unlock();
  leave_critical_section(irq_flags);

  up_idle();

  return true;
}

#if defined CONFIG_PM_DEBUG
static void gpio_weakup_handler(void)
{
  System_WakeUpPinDisable(TEST_PM_STATE_PIN1);
}

static struct dlps_driver_ops rtl_pm_driver_ops
	__attribute__((section(".dlps.dataon.data")))
	__attribute__((__used__)) = {
    gpio_dlps_check_cb,
    NULL,
    NULL,
    gpio_weakup_handler,
};
#endif

void arm_pminitialize(void)
{
  pm_initialize();
  pm_register(&g_platform_pm);

  __rtl_btmac_pm_set_power_mode(BTMAC_DEEP_SLEEP);

#if defined CONFIG_ONESHOT_DLPS_CHECK
  dlps_check_cb_reg((DLPSEnterCheckFunc)oneshot_dlps_check_cb);
#endif

 if (__dlps_dataon_start__ < __dlps_dataon_end__)
   {
     dlps_check_cb_reg((DLPSEnterCheckFunc)rtl_ops_dlps_check_cb);
   }

  __rtl_platform_pm_restore_os_tick_count = ({ void lamba(void) { return; } &lamba; });

  irq_attach(System_VECTORn, (xcpt_t)System_Handler, NULL);

  dlps_hw_control_cb_reg(DLPS_IO_EnterDlpsCb, PLATFORM_PM_STORE);
  dlps_hw_control_cb_reg(DLPS_IO_ExitDlpsCb, PLATFORM_PM_PEND);
#ifdef CONFIG_BT
  __rtl_bt_pm_register();
#endif

#if defined CONFIG_PM_DEBUG
  Pad_Config(TEST_PM_STATE_PIN1, PAD_PINMUX_MODE, PAD_IS_PWRON,
                      PAD_PULL_NONE, PAD_OUT_DISABLE, PAD_OUT_LOW);
  Pinmux_Config(TEST_PM_STATE_PIN1, DWGPIO);

  RCC_PeriphClockCmd(APBPeriph_GPIO, APBPeriph_GPIO_CLOCK, ENABLE);
  GPIO_InitTypeDef GPIO_InitStruct;
  GPIO_StructInit(&GPIO_InitStruct);
  GPIO_InitStruct.GPIO_Pin= GPIO_GetPin(TEST_PM_STATE_PIN1);
  GPIO_InitStruct.GPIO_Mode= GPIO_Mode_IN;
  GPIO_InitStruct.GPIO_ITCmd= DISABLE;

  GPIO_Init(&GPIO_InitStruct);
#endif

#if defined CONFIG_RTL876x_AON_WDG
  aon_wdg_init(0, CONFIG_RTL876x_AON_WDG_TIMEOUT_SECONDS);
  dlps_check_cb_reg((DLPSEnterCheckFunc)aon_watch_dog_check_cb);
#endif

}

#endif
