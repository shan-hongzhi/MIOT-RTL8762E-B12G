/* main.c - Application main entry point */

/*
 * Copyright (c) 2022 Xiaomi Corporation
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <drivers/flash.h>
#include <errno.h>
#include <stddef.h>
#include <storage/flash_map.h>
#include <string.h>
#include <sys/byteorder.h>
#include <sys/reboot.h>
#include <zephyr.h>
#include <zephyr/types.h>

#include "mible.h"
#include "patch_header_check.h"

#if defined(MI_LOG_ENABLED) && (MI_LOG_ENABLED)
#include <sys/printk.h>
#else
#undef printk
#define printk(fmt, ...)
#endif

static const struct device *otp_dev;
static const struct device *ota_dev;

#define PATCH_SIZE     (40 * 1024)
#define SEC_BOOT_SIZE  (4 * 1024)

int mible_otp_init(void)
{
    const struct flash_area *otp;
    int err;

    err = flash_area_open(1, &otp);
    if (err) {
        return err;
    }

    otp_dev = device_get_binding(otp->fa_dev_name);
    if (!otp_dev) {
        return -ENODEV;
    }

    return 0;
}

__attribute__((weak)) int mible_otp_read(void * p_data, uint32_t length, uint32_t address)
{
    if (!otp_dev) {
        return -ENODEV;
    }

    return flash_read(otp_dev, address, p_data, length);
}

#define FLASH_PARTITION_OTA_ID 2

mible_status_t mible_nvm_init(void)
{
    const struct flash_area *ota;
    int err;

    err = flash_area_open(FLASH_PARTITION_OTA_ID, &ota);
    if (err) {
        return err;
    }

    ota_dev = device_get_binding(ota->fa_dev_name);
    if (!ota_dev) {
        return -ENODEV;
    }

    return 0;
}

mible_status_t mible_nvm_read(void *p_data, uint32_t length, uint32_t address)
{
    if (!ota_dev) {
        return -ENODEV;
    }

    return flash_read(ota_dev, address, p_data, length);
}

mible_status_t mible_upgrade_firmware(void)
{
    uint32_t base_addr = PATCH_SIZE + offsetof(T_IMG_CTRL_HEADER_FORMAT, ctrl_flag.flag_value);
    T_IMG_CTRL_HEADER_FORMAT header;

    flash_read(ota_dev, 0, &header, sizeof(T_IMG_CTRL_HEADER_FORMAT));

    if (header.image_id != RomPatch) {
        return MI_ERR_INTERNAL;
    }

    if (header.ctrl_flag.flag_value.image_type & 0x01) {
        flash_read(ota_dev, base_addr, &header.ctrl_flag.flag_value, sizeof(uint16_t));
        header.ctrl_flag.flag_value.not_ready = 0;
        // Does need erase???
        flash_write(ota_dev, base_addr, &header.ctrl_flag.flag_value, sizeof(uint16_t));

        base_addr += SEC_BOOT_SIZE;
    }

    if (header.ctrl_flag.flag_value.image_type & 0x02) {
        flash_read(ota_dev, base_addr, &header.ctrl_flag.flag_value, sizeof(uint16_t));
        header.ctrl_flag.flag_value.not_ready = 0;
        // Does need erase???
        flash_write(ota_dev, base_addr, &header.ctrl_flag.flag_value, sizeof(uint16_t));
    }

    base_addr = offsetof(T_IMG_CTRL_HEADER_FORMAT, ctrl_flag.flag_value);
    flash_read(ota_dev, base_addr, &header.ctrl_flag.flag_value, sizeof(uint16_t));
    header.ctrl_flag.flag_value.not_ready = 0;
    // Does need erase???
    flash_write(ota_dev, base_addr, &header.ctrl_flag.flag_value, sizeof(uint16_t));

    printk("upgrade verify ok, restarting....\n");

    sys_reboot(0);

    return MI_SUCCESS;
}

mible_status_t mible_nvm_write(void *p_data, uint32_t length, uint32_t address)
{
    struct flash_sector ota_sector;
    uint32_t sec_cnt;
    int ret = 0;

    if (!ota_dev) {
        return -ENODEV;
    }

    flash_area_get_sectors(FLASH_PARTITION_OTA_ID, &sec_cnt, &ota_sector);

    if ((address % ota_sector.fs_size) == 0) {
        ret = flash_erase(ota_dev, address, ota_sector.fs_size);
        if (ret) {
            printk("flash erase sector %d ret=%d", ota_sector.fs_size, ret);
            return ret;
        }
    }

    return flash_write(ota_dev, address, p_data, length);
}

uint16_t mible_firmware_crc16(void)
{
    extern T_IMG_CTRL_HEADER_FORMAT __app_flash_start__;
    T_IMG_CTRL_HEADER_FORMAT *rom_h = (T_IMG_CTRL_HEADER_FORMAT *)&__app_flash_start__;

    return rom_h->crc16;
}
