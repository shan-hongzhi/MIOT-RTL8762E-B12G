/**
*********************************************************************************************************
*               Copyright(c) 2022, Realtek Semiconductor Corporation. All rights reserved.
**********************************************************************************************************
* @file     main.c
* @brief    This is the entry of user code which the main function resides in.
* @details
* @author   mandy
* @date     2022-01-24
* @version  v1.0
*********************************************************************************************************
*/

/*============================================================================*
 *                              Header Files
 *============================================================================*/
#include <stdlib.h>
#include <os_sched.h>
#include <string.h>
#include <trace.h>
#include <gap.h>
#include <gap_adv.h>
#include <profile_server.h>
#include <gap_msg.h>
#include <app_task.h>
#include "board.h"
#include "rtl876x.h"
#include "rtl876x_pinmux.h"
#include "rtl876x_io_dlps.h"
#include "rtl876x_uart.h"
#include "rtl876x_rcc.h"
#include "rtl876x_nvic.h"
#include <dlps.h>
#include "peripheral_dfu.h"
#include "peripheral_dfu_application.h"
#include <uart_driver.h>
#include "peripheral_handle.h"
#include <app_section.h>

/*============================================================================*
 *                              Functions Declaration
 *============================================================================*/
static void global_data_init(void);
static void app_pinmux_config(void);
static void app_pad_config(void);
static void board_init(void);
#if DLPS_EN
static void app_enter_dlps_config(void) DATA_RAM_FUNCTION;
static void app_exit_dlps_config(void) DATA_RAM_FUNCTION;
static bool app_dlps_check_cb(void) DATA_RAM_FUNCTION;
#endif
static void pwr_mgr_init(void);
static void task_init(void);

void System_Handler(void) DATA_RAM_FUNCTION;

/*============================================================================*
 *                              Local Functions
 *============================================================================*/
/******************************************************************
 * @brief  global_data_init() contains the initialization of global data.
 * @param  none
 * @return none
 * @retval void
 */
void global_data_init(void)
{
    uart_init_data();
}

/******************************************************************
 * @brief  app_pinmux_config() contains the initialization of app pinmux config.
 * @param  none
 * @return none
 * @retval void
 */
void app_pinmux_config(void)
{
    uart_pinmux_config();
}

/******************************************************************
 * @brief  app_pad_config() contains the initialization of app pad config.
 * @param  none
 * @return none
 * @retval void
 */
void app_pad_config(void)
{
    uart_init_pad_config();
}

/******************************************************************
 * @brief  board_init() contains the initialization of pinmux settings and pad settings.
 *
 *               All the pinmux settings and pad settings shall be initiated in this function.
 *               But if legacy driver is used, the initialization of pinmux setting and pad setting
 *               should be peformed with the IO initializing.
 * @param  none
 * @return none
 * @retval void
 */
void board_init(void)
{
    app_pinmux_config();
    app_pad_config();
}

#if DLPS_EN
/******************************************************************
 * @brief  this function will be called before enter DLPS
 *
 *  set PAD and wakeup pin config for enterring DLPS
 * @param  none
 * @return none
 * @retval void
 */
void app_enter_dlps_config(void)
{
//  DBG_DIRECT("ENTER");
    uart_enter_dlps_config();
}

/******************************************************************
 * @brief this function will be called after exit DLPS
 *
 *  set PAD and wakeup pin config for enterring DLPS
 *
 * @param  none
 * @return none
 * @retval void
 */
void app_exit_dlps_config(void)
{
//  DBG_DIRECT("EXIT");
    uart_exit_dlps_config();
}

/******************************************************************
 * @brief app_dlps_check_cb() contains the setting about app dlps callback.
 * @param  none
 * @retval ture   able to enter DLPS
 * @retval false  unable to enter DLPS
 */
bool app_dlps_check_cb(void)
{
    return peripheral_handle_check_dlps();
}
#endif

/******************************************************************
 * @brief  System_Handler() contains the handler for System_On interrupt.
 * @param  none
 * @return none
 * @retval void
 */
void System_Handler(void)
{
    APP_PRINT_INFO0("System_Handler");

    NVIC_DisableIRQ(System_IRQn);

    if (System_WakeUpInterruptValue(UART_RX) == SET)
    {
        APP_PRINT_INFO0("UART_DFU Trigger");
        peripheral_handle_disallow_to_enter_dlps();
        peripheral_handle_no_act_timer_restart();
        Pad_ClearWakeupINTPendingBit(UART_RX);
        uart_disable_wakeup_config();
        NVIC_ClearPendingIRQ(System_IRQn);
        return;
    }

    if (SET == System_DebounceWakeupStatus())
    {
        APP_PRINT_INFO0("[System_Handler] pad signal wake up");
    }

    NVIC_ClearPendingIRQ(System_IRQn);
}

/******************************************************************
 * @brief  pwr_mgr_init() contains the setting about power mode.
 * @param  none
 * @return none
 * @retval void
 */
void pwr_mgr_init(void)
{
#if DLPS_EN
    if (false == dlps_check_cb_reg(app_dlps_check_cb))
    {
        DBG_DIRECT("Error: dlps_check_cb_reg(DLPS_RcuCheck) failed!\n");
    }
    DLPS_IORegUserDlpsEnterCb(app_enter_dlps_config);
    DLPS_IORegUserDlpsExitCb(app_exit_dlps_config);
    DLPS_IORegister();
    lps_mode_set(PLATFORM_DLPS_PFM);
#endif
}


/******************************************************************
 * @brief  task_init() contains the initialization of all the tasks.
 *
 *           There are four tasks are initiated.
 *           Lowerstack task and upperstack task are used by bluetooth stack.
 *           Application task is task which user application code resides in.
 *           Emergency task is reserved.
 * @param  none
 * @return none
 * @retval void
*/
void task_init(void)
{
    app_task_init();
}

/*============================================================================*
 *                              Global Functions
 *============================================================================*/
/******************************************************************
 * @brief  driver_init() contains the initialization of peripherals.
 *
 *               Both new architecture driver and legacy driver initialization method can be used.
 * @param  none
 * @return none
 * @retval void
 */
void driver_init(void)
{
    uart_init_driver();
}

/******************************************************************
 * @brief  app_nvic_config() contains the initialization of app NVIC config.
 *
 * @param  none
 * @return none
 * @retval void
 */
void app_nvic_config(void)
{
    uart_nvic_config();
}

/******************************************************************
 * @brief  main() is a start of main codes.
 * @param  none
 * @retval 0
 */
int main(void)
{
    extern uint32_t random_seed_value;
    srand(random_seed_value);
    global_data_init();
    board_init();
    pwr_mgr_init();
#if FEATURE_SUPPORT_NO_ACTION_EXIT_DFU
    peripheral_handle_timer_init();
#endif
    task_init();
    os_sched_start();

    return 0;
}

/******************* (C) COPYRIGHT 2022 Realtek Semiconductor Corporation *****END OF FILE****/
