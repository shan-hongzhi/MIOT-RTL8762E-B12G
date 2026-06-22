/**
*********************************************************************************************************
*               Copyright(c) 2015, Realtek Semiconductor Corporation. All rights reserved.
**********************************************************************************************************
* @file     rtl876x_wdg.c
* @brief    This file provides all the Watch Dog firmware functions.
* @details
* @author   Lory_xu
* @date     2016-06-12
* @version  v0.1
*********************************************************************************************************
*/

#include "rtl876x_wdg.h"
#include "patch_iodriver.h"
#include "otp.h"
#include "trace.h"
#include "os_sched.h"
#include "flash_driver.h"
#include "app_section.h"
#include "secure_boot.h"

/* General Purpose FW register */
#define BTAON_FAST_RESET_REASON     0x15
#define RESET_RAM_PATTERN           0x726574

typedef struct _CHECK_RESET_RAM_RECORD
{
    uint32_t check_reset_ram_pattern : 24;
    uint32_t check_reset_ram_type : 8;
} T_CHECK_RESET_RAM_RECORD;

RAM_DATAON_UNINIT_SECTION T_CHECK_RESET_RAM_RECORD check_reset_ram;
APP_CB_WDG_RESET_TYPE __rtl_app_cb_wdg_reset = 0;
T_SW_RESET_REASON reset_reason;

void __RTL_WDG_ClockEnable(void)
{
    HAL_WRITE32(PERIPH_REG_BASE, 0x230, HAL_READ32(PERIPH_REG_BASE, 0x230) | BIT6);
    HAL_WRITE32(PERIPH_REG_BASE, 0x360, HAL_READ32(PERIPH_REG_BASE, 0x360) | BIT8);
    HAL_WRITE32(PERIPH_REG_BASE, 0x210, HAL_READ32(PERIPH_REG_BASE, 0x210) | BIT16);
}

/**
divfactor: 16Bit: 32.768k/(1+divfactor)
cnt_limit: 2^(cnt_limit+1) - 1 ; max 11~15 = 0xFFF
            0: 0x001
            1: 0x003
            2: 0x007
            3: 0x00F
            4: 0x01F
            5: 0x03F
            6: 0x07F
            7: 0x0FF
            8: 0x1FF
            9: 0x3FF
            10: 0x7FF
            11~15: 0xFFF
Wdg_mode: 0: interrupt cpu
          1: reset all except AON
          2: reset core domain
          3: reset all
*/
void __RTL_WDG_Config(
    uint16_t div_factor,
    uint8_t  cnt_limit,
    T_WDG_MODE  wdg_mode
)
{
    T_WATCH_DOG_TIMER_REG wdg_ctrl_value;
    if (patch_WDG_Config)
    {
        if (patch_WDG_Config(div_factor, cnt_limit, wdg_mode))
        {
            return;
        }
    }
    if (div_factor == 0)
    {
        //DBG_DIRECT("WDT Divfactor Can't Be Zero\n");
        div_factor = 1;
    }

    wdg_ctrl_value.d32 = WDG->WDG_CTL;

    wdg_ctrl_value.b.div_factor = div_factor;
    wdg_ctrl_value.b.cnt_limit = cnt_limit;
    wdg_ctrl_value.b.wdg_mode = wdg_mode;

    WDG->WDG_CTL = wdg_ctrl_value.d32;
}


void __RTL_WDG_Enable(void)
{
    T_WATCH_DOG_TIMER_REG wdg_ctrl_value;

    wdg_ctrl_value.d32 = WDG->WDG_CTL;

    wdg_ctrl_value.b.en_byte = 0xA5;
    wdg_ctrl_value.b.clear = 1;
    wdg_ctrl_value.b.timeout = 1; // W1C;

    WDG->WDG_CTL = wdg_ctrl_value.d32;
}


void __RTL_WDG_Disable(void)
{
    T_WATCH_DOG_TIMER_REG wdg_ctrl_value;

    wdg_ctrl_value.d32 = WDG->WDG_CTL;

    wdg_ctrl_value.b.en_byte = 0;
    wdg_ctrl_value.b.clear = 1;
    wdg_ctrl_value.b.timeout = 1; // W1C;

    WDG->WDG_CTL = wdg_ctrl_value.d32;
}


void __RTL_WDG_Restart(void)
{
    T_WATCH_DOG_TIMER_REG wdg_ctrl_value;

    wdg_ctrl_value.d32 = WDG->WDG_CTL;
    wdg_ctrl_value.b.clear = 1;
    WDG->WDG_CTL = wdg_ctrl_value.d32;
}

void WDG_SystemReset_Dump(T_WDG_MODE wdg_mode, T_SW_RESET_REASON reset_reason, uint32_t *wdg_args)
{
    if (patch_WDG_SystemReset)
    {
        if (patch_WDG_SystemReset(wdg_mode, reset_reason, wdg_args))
        {
            return;
        }
    }

    if (__rtl_otp.platform_cfg.run_in_app == 1)
    {
        if (__rtl_app_cb_wdg_reset)
        {
            __rtl_app_cb_wdg_reset(wdg_mode, reset_reason);
        }
    }

    check_reset_ram.check_reset_ram_type = reset_reason;

    if (RESET_ALL_EXCEPT_AON == wdg_mode || RESET_CORE_DOMAIN == wdg_mode)
    {
        __rtl_btaon_fast_write_safe_8b(BTAON_FAST_RESET_REASON, reset_reason);
    }

    __disable_irq();

    uint32_t *sp = wdg_args;
    uint32_t lr = *wdg_args;

    if (__rtl_otp.platform_cfg.logDisable == 0 && __rtl_otp.platform_cfg.dump_info_before_reset)
    {
        extern void __RTL_LOGUARTDriverInit(void);
        extern void __RTL_DumpRawMemory(uint32_t *startAddr, uint32_t size);

        __RTL_LOGUARTDriverInit();
        DBG_DIRECT("Before __RTL_WDG_SystemReset, wdg_mode=%d, Reset reason: 0x%x, sp = 0x%x, sb = %d\n",
                   wdg_mode, reset_reason, sp, secure_boot_failed_line);
        if (RESET_REASON_BOOT_EFUSE_INVALID == reset_reason)
        {
            DBG_DIRECT("boot error code %d, debug info %d",
                       boot_only_ram.secure_boot_error_info.efuse_or_key_error_code,
                       boot_only_ram.secure_boot_error_info.debug_info);
        }

        uint32_t start_addr = (uint32_t)sp & ~(31);
        uint32_t size = 512;
        if (start_addr > BUFFER_RAM_START_ADDR) //sp locate in buffer ram
        {
            if (start_addr > BUFFER_RAM_START_ADDR + BUFFER_RAM_TOTAL_SIZE - 512)
            {
                size = BUFFER_RAM_START_ADDR + BUFFER_RAM_TOTAL_SIZE - start_addr;
            }
        }
        else if (start_addr > DATA_RAM_START_ADDR) //sp locate in data ram
        {
            if (start_addr > DATA_RAM_START_ADDR + DATA_RAM_TOTAL_SIZE - 512)
            {
                size = DATA_RAM_START_ADDR + DATA_RAM_TOTAL_SIZE - start_addr;
            }
        }
        else
        {
            size = 0;
        }

        __RTL_DumpRawMemory((uint32_t *)start_addr, size);
    }

    if (__rtl_otp.platform_cfg.write_info_to_flash_before_reset && __rtl_flash_get_flash_exist())
    {
        int i;
        int max_item = 2 << (__rtl_otp.platform_cfg.reboot_record_item_limit_power_2 - 1);
        for (i = 0; i < max_item; ++i)
        {
            if ((flash_auto_read(__rtl_otp.platform_cfg.reboot_record_address + (i << 3)) == 0xFFFFFFFF) &&
                (flash_auto_read(__rtl_otp.platform_cfg.reboot_record_address + (i << 3) + 4) == 0xFFFFFFFF))
            {
                __rtl_flash_auto_write(__rtl_otp.platform_cfg.reboot_record_address + (i << 3), __rtl_os_sys_time_get());
                __rtl_flash_auto_write(__rtl_otp.platform_cfg.reboot_record_address + (i << 3) + 4, lr);
                break;
            }
        }
    }

    __RTL_WDG_ClockEnable();
    __RTL_WDG_Config(1, 0, wdg_mode);
    __RTL_WDG_Enable();

    while (1); /* wait until reset */
}


__asm void __RTL_WDG_SystemReset(T_WDG_MODE wdg_mode, T_SW_RESET_REASON reset_reason)
{
    extern WDG_SystemReset_Dump
    PRESERVE8

    push     {lr}
    mov      r2, sp
    bl       WDG_SystemReset_Dump
    pop      {pc}

    ALIGN
}

void reset_reason_parse_in_boot(void)
{
    if (check_reset_ram.check_reset_ram_pattern != RESET_RAM_PATTERN)
    {
        reset_reason = RESET_REASON_HW;
    }
    else
    {
        reset_reason = (T_SW_RESET_REASON)__rtl_btaon_fast_read_safe_8b(BTAON_FAST_RESET_REASON);

        if (reset_reason == RESET_REASON_HW)
        {
            reset_reason = (T_SW_RESET_REASON)check_reset_ram.check_reset_ram_type;
        }
    }
}

void reset_reason_reset(void)
{
    check_reset_ram.check_reset_ram_pattern = RESET_RAM_PATTERN;
    check_reset_ram.check_reset_ram_type = RESET_REASON_WDG_TIMEOUT;

    __rtl_btaon_fast_write_8b(BTAON_FAST_RESET_REASON, RESET_REASON_WDG_TIMEOUT);
}

T_SW_RESET_REASON  __rtl_reset_reason_get(void)
{
    return reset_reason;
}


