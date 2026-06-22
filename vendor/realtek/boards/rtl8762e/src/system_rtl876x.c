/****************************************************************************
 * vendor/realtek/boards/rtl8762e/src/system_rtl876x.c
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

#include <nuttx/init.h>
#include <nuttx/config.h>
#include "debug.h"
#include "version.h"
#include "rtl876x.h"
#include "patch_header_check.h"
#include "app_section.h"
#include "rom_uuid.h"
#include "platform_utils.h"
#include "rtl876x_wdg.h"
#include "arm_internal.h"
#include "trace.h"
#include "mem_config.h"

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/
#define IMG_HDR_MAGIC_PATTERN       0X5A5A12A5

#define SHARE_CACHE_RAM_0K          0x82F70000
#define SHARE_CACHE_RAM_4K          0x2F2D0002
#define SHARE_CACHE_RAM_8K          0xA2AA0003

#define APP_FAKE_PAYLOAD_LEN        0x100

/** @brief shared cache ram size (adjustable, config SHARE_CACHE_RAM_SIZE: 0/4KB/8KB) */

#ifdef CONFIG_RTL876x_CACHE
#define SHARE_CACHE_RAM_SIZE           (0 * 1024)
#else
#define SHARE_CACHE_RAM_SIZE           (8 * 1024)
#endif
/****************************************************************************
 * Public Data
 ****************************************************************************/

void system_init(void);

static const T_IMG_HEADER_FORMAT img_header APP_FLASH_HEADER =
{
    .ctrl_header =
    {
        .ic_type = DEFINED_IC_TYPE,
        .secure_version = 0,

        .ctrl_flag.flag_value.xip = 1,
        .ctrl_flag.flag_value.enc = 0,
        .ctrl_flag.flag_value.load_when_boot = 0,
        .ctrl_flag.flag_value.enc_key_select = 0,

        .ctrl_flag.flag_value.enc_load = 0,
        .ctrl_flag.flag_value.not_ready = 0,
        .ctrl_flag.flag_value.not_obsolete = 1,
        .ctrl_flag.flag_value.compressed_not_ready = 0,
        .ctrl_flag.flag_value.compressed_not_obsolete = 1,
        .ctrl_flag.flag_value.integrity_check_en_in_boot = 0,

        .image_id = AppPatch,
        .payload_len = APP_FAKE_PAYLOAD_LEN,
    },
    .uuid = DEFINE_rom_uuid,
    .magic_pattern = IMG_HDR_MAGIC_PATTERN,

    .load_base = 0,
    .exe_base = (uint32_t) system_init,
    .load_len = 0,

#if (APP_BANK == 0)
    .image_base = BANK0_APP_ADDR,
#else
    .image_base = BANK1_APP_ADDR,
#endif

    .git_ver =
    {
        .ver_info.sub_version._version_major = VERSION_MAJOR,
        .ver_info.sub_version._version_minor = VERSION_MINOR,
        .ver_info.sub_version._version_revision = VERSION_REVISION,
        .ver_info.sub_version._version_reserve = VERSION_BUILDNUM % 32,
        ._version_commitid = VERSION_GCID,
        ._customer_name =
        {CN_1, CN_2, CN_3, CN_4, CN_5, CN_6, CN_7, CN_8},
    },

    .app_cb_signature = SIGNATURE_APP_CB,
    .app_cb_table_base_address = 0
};

static const T_AUTH_HEADER_FORMAT auth_header APP_FLASH_HEADER_AUTH =
{
    .header_mac =
    {[0 ... 15] = 0xff},
    .payload_mac =
    {[0 ... 15] = 0xff},
};

uint32_t random_seed_value;

/****************************************************************************
 * Private Functions
 ****************************************************************************/

void random_seed_init(void)
{
  random_seed_value = __rtl_platform_random(0xffffffff);
}

__attribute__((optimize(0)))
static void __first_stage_init(void)
{
    extern uint32_t *__ram_first_stage_text_start__;
    extern uint32_t *__ram_first_stage_text_load_ad__;
    extern uint32_t *__ram_first_stage_text_length__;

    for(int i = 0; i < ((unsigned int)&__ram_first_stage_text_length__)/4 ; i++)
    {
      *(&__ram_first_stage_text_start__ + i) = *(&__ram_first_stage_text_load_ad__ + i);
    }
}

__attribute__((optimize("O1")))
__attribute__((section(".app.first_stage_ram.text")))
static void __xip_copy(void *dst, const void *src, uint32_t length)
{
    length >>= 2;

    for(int i = 0; i < length; i++)
    {
      *((uint32_t *)dst + i) = *((const uint32_t *)src + i);
    }
}

__attribute__((optimize("O1")))
__attribute__((section(".app.first_stage_ram.text")))
static void __xip_clear(void *dst, uint32_t length)
{
    length >>= 2;

    for(int i = 0; i < length; i++)
    {
      *((uint32_t *)dst + i) = 0x00;
    }
}

APP_FLASH_TEXT_SECTION
static void __ram_init(void)
{
    __first_stage_init();

    extern uint32_t *__ram_second_stage_text_start__;
    extern uint32_t *__ram_second_stage_text_load_ad__;
    extern uint32_t *__ram_second_stage_text_length__;

    __xip_copy(&__ram_second_stage_text_start__,
            &__ram_second_stage_text_load_ad__,
            (unsigned int)&__ram_second_stage_text_length__);

    extern uint32_t *__ram_dataon_ro_start__;
    extern uint32_t *__ram_ro_load_ad__;
    extern uint32_t *__ram_dataon_ro_length__;

    __xip_copy(&__ram_dataon_ro_start__,
            &__ram_ro_load_ad__,
            (unsigned int)&__ram_dataon_ro_length__);

    extern uint32_t *__ram_dataon_rw_start__;
    extern uint32_t *__ram_rw_load_ad__;
    extern uint32_t *__ram_dataon_rw_length__;

    __xip_copy(&__ram_dataon_rw_start__,
            &__ram_rw_load_ad__,
            (unsigned int)&__ram_dataon_rw_length__);

    extern uint32_t *__ram_dataon_zi_start__;
    extern uint32_t *__ram_dataon_zi_length__;

    __xip_clear(&__ram_dataon_zi_start__,
            (unsigned int)&__ram_dataon_zi_length__);

    extern uint32_t *__buf_dataon_rw_start__;
    extern uint32_t *__buf_rw_load_ad__;
    extern uint32_t *__buf_dataon_rw_length__;

    __xip_copy(&__buf_dataon_rw_start__,
            &__buf_rw_load_ad__,
            (unsigned int)&__buf_dataon_rw_length__);

    extern uint32_t *__buf_dataon_bss_rw_start__;
    extern uint32_t *__buf_dataon_bss_length__;

    __xip_clear(&__buf_dataon_bss_rw_start__,
            (unsigned int)&__buf_dataon_bss_length__);

    extern uint32_t *__patch_rw_start__;
    extern uint32_t *__patch_rw_load_ad__;
    extern uint32_t *__patch_rw_length__;

    __xip_copy(&__patch_rw_start__,
            &__patch_rw_load_ad__,
            (unsigned int)&__patch_rw_length__);
}

#define APP2_IMG_ADDR  0x854000
APP_MAIN_FUNC get_app_image_entry_addr(uint16_t image_id, uint32_t header_addr)
{
/*check ic_type, not_ready, image_id, uuid*/

  if (!__rtl_check_header_valid(header_addr, (T_IMG_ID)image_id))
    {
      return NULL;
    }

  T_IMG_HEADER_FORMAT *header = (T_IMG_HEADER_FORMAT *)header_addr;

  if (image_id <= OTA || image_id >= IMAGE_MAX)
    {
        return NULL;
    }

  if(!header->ctrl_header.ctrl_flag.flag_value.not_obsolete)
    {
        return NULL;
    }

  if(header->ctrl_header.ctrl_flag.flag_value.image_type)
    {
        return NULL;
    }

  if(header->magic_pattern != IMG_HDR_MAGIC_PATTERN)
    {
        return NULL;
    }
  
//   //will increase boot time, check image sha256 value
//   if(!check_image_chksum(header))
//     {
//         return NULL;
//     }

  APP_MAIN_FUNC entry_func = (APP_MAIN_FUNC)(header->exe_base | 1);

  return entry_func;
}

#if defined(CONFIG_SEGGER_RTT)
extern unsigned SEGGER_RTT_Write(unsigned BufferIndex, const void* pBuffer, unsigned NumBytes);

static bool rtl_log_write(void *pPPB, const uint8_t *source, uint16_t size)
{
  (void)SEGGER_RTT_Write(0, source, size);

  return true;
}
#endif

void print_reset_reason(void)
{
    uint32_t wdg_reset_pc = (__rtl_btaon_fast_read_safe(0x1c2) << 17) | (__rtl_btaon_fast_read_safe(0x1c0) << 1);
    uint32_t wdg_reset_lr = (__rtl_btaon_fast_read_safe(0x1c6) << 16) | (__rtl_btaon_fast_read_safe(0x1c4));
    uint32_t wdg_reset_psr = (__rtl_btaon_fast_read_safe(0x1ca) << 16) | (__rtl_btaon_fast_read_safe(0x1c8));

    uint32_t aon_wdg_reset_pc = (__rtl_btaon_fast_read_safe(0x1ce) << 17) | (__rtl_btaon_fast_read_safe(
                                                                           0x1cc) << 1);
    uint32_t aon_wdg_reset_lr = (__rtl_btaon_fast_read_safe(0x1d2) << 16) | (__rtl_btaon_fast_read_safe(0x1d0));
    uint32_t aon_wdg_reset_psr = (__rtl_btaon_fast_read_safe(0x1d6) << 16) | (__rtl_btaon_fast_read_safe(0x1d4));

    if (wdg_reset_lr == 0)
    {
        syslog(0,"[Debug Info]RESET Reason: HW");

        uint32_t wdg_reg_backup = WDG->WDG_CTL;

        T_WDG_MODE reset_mode = (T_WDG_MODE)((wdg_reg_backup >> 29) & 0x3);
        if (INTERRUPT_CPU == reset_mode)
        {
            NVIC_DisableIRQ(WDG_IRQn);
        }

        __RTL_WDG_ClockEnable();
        __RTL_WDG_Config(1, 0, INTERRUPT_CPU);
        __RTL_WDG_Enable();
        __rtl_platform_delay_ms(1);

        __RTL_WDG_Disable();
        __rtl_platform_delay_us(150);

        if (INTERRUPT_CPU == reset_mode)
        {
            NVIC_ClearPendingIRQ(WDG_IRQn);
            NVIC_EnableIRQ(WDG_IRQn);
        }
        WDG->WDG_CTL = wdg_reg_backup;
    }
    else
    {
        T_SW_RESET_REASON sw_reset_type = __rtl_reset_reason_get();

        //fix reset reason error when watchdog timeout and ROM_WATCH_DOG_MODE is RESET_ALL
        if (sw_reset_type == RESET_REASON_HW)
        {
            sw_reset_type = RESET_REASON_WDG_TIMEOUT;
        }

        syslog(0,"[Debug Info]RESET Reason: SW, TYPE 0x%x", sw_reset_type);
        syslog(0,"[Debug Info]WDG: pc %lx, lr %lx, psr %lx", wdg_reset_pc, wdg_reset_lr,
                         wdg_reset_psr);
        syslog(0,"[Debug Info]AON WDG: pc %lx, lr %lx, psr %lx", aon_wdg_reset_pc, aon_wdg_reset_lr,
                         aon_wdg_reset_psr);
    }
}

APP_FLASH_TEXT_SECTION
void system_init(void)
{
  APP_MAIN_FUNC func = NULL;

  if (FLASH_SUCCESS == __rtl_flash_try_high_speed(FLASH_MODE_2BIT))
  {
      APP_PRINT_INFO0("Switch to 2-Bit flash mode");
  }

  func = (APP_MAIN_FUNC)get_app_image_entry_addr(AppPatch, APP2_IMG_ADDR);
  if (func)
    {
      func();
      return;
    }

  __set_MSP(((uint32_t)_ebss + CONFIG_IDLETHREAD_STACKSIZE));

  __rtl_update_ram_layout(APP_GLOBAL_SIZE, HEAP_DATA_ON_SIZE,
                    SHARE_CACHE_RAM_SIZE);

  __ram_init();

#if defined(CONFIG_SEGGER_RTT)
  extern bool (* __rtl_log_write) (void *pPPB, const uint8_t *source, uint16_t size);
  __rtl_log_write = rtl_log_write;
#endif

#ifdef USE_EARLYSERIALINIT
  arm_earlyserialinit();
#endif

  syslog(0,"Firmware for APP \n");
  print_reset_reason();

  nx_start();
}

/****************************************************************************
 * Public Functions
 ****************************************************************************/

int rtl_main(int argc, FAR char *argv[])
{
  DBG_DIRECT("enter task !");

  while (1);
}
