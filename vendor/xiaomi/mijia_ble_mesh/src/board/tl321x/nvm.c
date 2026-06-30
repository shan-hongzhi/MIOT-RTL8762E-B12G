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
#include <nuttx/fs/fs.h>
#include <nuttx/mtd/mtd.h>

#include "mible.h"

#if defined(MI_LOG_ENABLED) && (MI_LOG_ENABLED)
#include <sys/printk.h>
#else
#undef printk
#define printk(fmt, ...)
#endif

#define FLASH_PARTITION_NVS_ID          0
#define FLASH_PARTITION_OTP_ID          1
#define FLASH_PARTITION_BANKA_ID        2
#define FLASH_PARTITION_BANKB_ID        3

#define TELINK_FIRMWARE_POSITION        0x20
#define TELINK_FIRMWARE_VALUE           0x4b
#define TELINK_FIRMWARE_ERASE_VALUE     0xff
#define TELINK_FIRMWARE_INVALID_VALUE   0x00

static bool current_is_bank_b;
static uint8_t ota_partition_id;
static const struct device *otp_dev;
static const struct device *ota_dev;
// extern const unsigned char mi_potp[];

int mible_otp_init(void)
{
    const struct flash_area *otp;
    int err;

    err = flash_area_open(FLASH_PARTITION_OTP_ID, &otp);
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
        printk("mible_otp_read: otp_dev is NULL");
        return -ENODEV;
    }

    if(address % 512 + length > 512){
        printk("mible_otp_read: address + length > 512");
        return -ESPIPE;
    }

    return flash_read(otp_dev, address, p_data, length);
}

static bool is_in_bank_b(struct mtd_dev_s *dev)
{
    uint8_t val;

    (void)dev->read(dev, TELINK_FIRMWARE_POSITION, 1, &val);

    return (val != TELINK_FIRMWARE_VALUE);
}

mible_status_t mible_nvm_init(void)
{
    int ret;
    struct inode *node;
    const struct flash_area *ota;

    ret = find_mtddriver("/dev/banka", &node);
    if (ret) {
        printk("mible_nvm_init: find_mtddriver failed");
        return -ENODEV;
    }

    current_is_bank_b = is_in_bank_b(node->u.i_mtd);

    if(!current_is_bank_b){
        ota_partition_id = FLASH_PARTITION_BANKB_ID;
    }else{
        ota_partition_id = FLASH_PARTITION_BANKA_ID;
    }

    ret = flash_area_open(ota_partition_id, &ota);
    printk("mible_nvm_init BANK %d ret=%d", ota_partition_id, ret);

    if(ret){
        printk("mible_nvm_init: flash_area_open failed");
        return ret;
    }

    ota_dev = device_get_binding(ota->fa_dev_name);
    if (!ota_dev) {
        printk("mible_nvm_init: device_get_binding failed");
        return -ENODEV;
    }

    return 0;
}

mible_status_t mible_nvm_read(void *p_data, uint32_t length, uint32_t address)
{
    int ret;

    if (!ota_dev) {
        printk("mible_nvm_read: ota_dev is NULL");
        return -ENODEV;
    }

    ret = flash_read(ota_dev, address, p_data, length);

    if(ret){
        printk("mible_nvm_read: flash_read failed %d", ret);
        return ret;
    }

    if(address <= TELINK_FIRMWARE_POSITION && address+length > TELINK_FIRMWARE_POSITION){
        ((uint8_t *)p_data)[TELINK_FIRMWARE_POSITION - address] = TELINK_FIRMWARE_VALUE;
        printk("mible_nvm_read replace data[%ld] to TELINK_FIRMWARE_VALUE", 
            TELINK_FIRMWARE_POSITION - address);
    }

    return MI_SUCCESS;
}

mible_status_t mible_upgrade_firmware(void)
{
    int ret;
    struct inode *node;

    ret = find_mtddriver("/dev/banka", &node);
    if (ret) {
        printk("mible_upgrade_firmware: find_mtddriver failed");
        return -ENODEV;
    }

    struct mtd_dev_s *mtd_dev_a = node->u.i_mtd;
    uint8_t val = current_is_bank_b? TELINK_FIRMWARE_VALUE : TELINK_FIRMWARE_INVALID_VALUE;

    mtd_dev_a->write(mtd_dev_a, TELINK_FIRMWARE_POSITION, 1, &val);

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
        printk("mible_nvm_write: ota_dev is NULL");
        return -ENODEV;
    }

    flash_area_get_sectors(ota_partition_id, &sec_cnt, &ota_sector);

    if ((address % ota_sector.fs_size) == 0) {
        ret = flash_erase(ota_dev, address, ota_sector.fs_size);
        if (ret) {
            printk("flash erase sector %d ret=%d", ota_sector.fs_size, ret);
            return ret;
        }
    }

    if(current_is_bank_b && address <= TELINK_FIRMWARE_POSITION &&
        address + length > TELINK_FIRMWARE_POSITION){
        ((uint8_t *)p_data)[TELINK_FIRMWARE_POSITION - address] = TELINK_FIRMWARE_ERASE_VALUE;
        printk("mible_nvm_write replace pdata[%ld] to %02x", TELINK_FIRMWARE_POSITION - address,
            ((uint8_t *)p_data)[TELINK_FIRMWARE_POSITION - address]);
    }

    return flash_write(ota_dev, address, p_data, length);
}


uint16_t mible_firmware_crc16(void)
{
    return 0;
}
