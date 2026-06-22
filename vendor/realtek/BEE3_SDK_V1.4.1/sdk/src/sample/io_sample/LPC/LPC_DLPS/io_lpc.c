/**
*****************************************************************************************
*     Copyright(c) 2017, Realtek Semiconductor Corporation. All rights reserved.
*****************************************************************************************
   * @file      lpc.c
   * @brief
   * @author    yuan
   * @date      2018-05-04
   * @version   v0.1
   **************************************************************************************
   * @attention
   * <h2><center>&copy; COPYRIGHT 2017 Realtek Semiconductor Corporation</center></h2>
   **************************************************************************************
  */

/*============================================================================*
 *                              Header Files
 *============================================================================*/
#include "io_lpc.h"

#include "board.h"
#include "app_task.h"

bool IO_LPC_DLPS_Enter_Allowed = false;

/**
  * @brief  Initialize uart global data.
  * @param  No parameter.
  * @return void
  */
void global_data_lpc_init(void)
{
    IO_LPC_DLPS_Enter_Allowed = true;
}

/**
  * @brief  Handle lpc data function.
  * @param  No parameter.
  * @return void
  */
void io_lpc_handle_msg(T_IO_MSG *io_lpc_msg)
{
    uint16_t type = io_lpc_msg->type;
    if (IO_MSG_TYPE_BAT_LPC == type)
    {
        APP_PRINT_INFO0("io_handle_lpc_msg: VoltageDection done ");

        extern volatile void (*platform_delay_ms)(uint32_t t);
        platform_delay_ms(100);

        LPC_INTConfig(LPC_INT_LPCOMP_VOL, ENABLE);
        IO_LPC_DLPS_Enter_Allowed = true;
    }
}

/**
  * @brief  Handle lpc msg function.
  * @param  No parameter.
  * @return void
  */
void io_handle_lpc_msg(T_IO_MSG *io_lpc_msg)
{
    io_lpc_handle_msg(io_lpc_msg);
}

/**
  * @brief  IO enter dlps check function.
  * @param  No parameter.
  * @return void
  */
bool io_lpc_dlps_check(void)
{
    return IO_LPC_DLPS_Enter_Allowed;
}

/**
  * @brief  IO enter dlps call back function.
  * @param  No parameter.
  * @return void
  */
void io_lpc_dlps_enter(void)
{
    /* Notes: DBG_DIRECT is only used in debug demo, do not use in app project.*/
    DBG_DIRECT("DLPS ENTER");
}

/**
  * @brief  IO exit dlps call back function.
  * @param  No parameter.
  * @return void
  */
void io_lpc_dlps_exit(void)
{
    /* Notes: DBG_DIRECT is only used in debug demo, do not use in app project.*/
    DBG_DIRECT("DLPS EXIT");
}

/**
  * @brief  Initialization of pinmux settings and pad settings.
  * @param   No parameter.
  * @return  void
  */
void board_lpc_init(void)
{
    Pad_Config(LPC_CAPTURE_PIN, PAD_SW_MODE, PAD_IS_PWRON, PAD_PULL_NONE, PAD_OUT_DISABLE,
               PAD_OUT_HIGH);
    Pinmux_Config(LPC_CAPTURE_PIN, IDLE_MODE);
}

/**
  * @brief  Initialize RTC peripheral.
  * @param   No parameter.
  * @return  void
  */
void driver_lpc_init(void)
{
    LPC_DeInit();
    LPC_InitTypeDef LPC_InitStruct;
    LPC_StructInit(&LPC_InitStruct);

    LPC_InitStruct.LPC_Channel   = LPC_CAPTURE_CHANNEL;
    LPC_InitStruct.LPC_Edge      = LPC_VOLTAGE_DETECT_EDGE;
    LPC_InitStruct.LPC_Threshold = LPC_VOLTAGE_DETECT_THRESHOLD;
    LPC_Init(&LPC_InitStruct);
    LPC_Cmd(ENABLE);

    /* Config LPC voltage detection interrupt */
    LPC_INTConfig(LPC_INT_LPCOMP_VOL, ENABLE);

    /* Enable LPC DLPS Wakeup Config */
    LPC_WKCmd(ENABLE);
    RTC_SystemWakeupConfig(ENABLE);
}

/**
  * @brief  Initialize NVIC peripheral.
  * @param   No parameter.
  * @return  void
  */
void nvic_lpc_init(void)
{
    /* Config LPC interrupt */
    NVIC_InitTypeDef NVIC_InitStruct;
    NVIC_InitStruct.NVIC_IRQChannel = LPCOMP_IRQn;
    NVIC_InitStruct.NVIC_IRQChannelPriority = 3;
    NVIC_InitStruct.NVIC_IRQChannelCmd = ENABLE;
    NVIC_Init(&NVIC_InitStruct);
    LPC_INTCmd(ENABLE);
}

/**
  * @brief  LPC battery detection interrupt handle function.
  * @param  None.
  * @return None.
  */
void LPCOMP_Handler(void)
{
    LPC_INTConfig(LPC_INT_LPCOMP_VOL, DISABLE);

    T_IO_MSG int_lpc_msg;
    int_lpc_msg.type = IO_MSG_TYPE_BAT_LPC;

    if (false == app_send_msg_to_apptask(&int_lpc_msg))
    {
        APP_PRINT_ERROR0("[io_lpc] LPCOMP_Handler: Send int_lpc_msg failed!");
        return;
    }
}
