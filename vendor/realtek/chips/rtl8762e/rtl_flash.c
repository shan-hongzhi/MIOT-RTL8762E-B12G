/****************************************************************************
 * arch/arm/src/rtl8762e/rtl_flash.c
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
#include <sys/param.h>
#include "rtl_flash.h"

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

#ifdef CONFIG_RTL876x_FLASH
#define FMC_MAIN_ADDR             0x800000
#define BLOCK_SIZE                CONFIG_RTL876X_FLASH_BLKSIZE
#define ERASE_BLOCK_SIZE          0x1000  /* Nor flash erase block size: 4kB */
#define flash_ioctl_get_size_main 0x03
#define MTD_ERASED_STATE          0xff

#define RTL876x_START_ADDR        0x0000
#define RTL876x_START_SIZE        (4 * 1024)

#define RTL876x_OEM_CONFIG_PATH   "/dev/oem_config"
#define RTL876x_OEM_CONFIG_ADDR   (RTL876x_START_ADDR + RTL876x_START_SIZE)
#define RTL876x_OEM_CONFIG_SIZE   (4 * 1024)

#define RTL876x_OTA_HEADER_PATH   "/dev/ota_header"
#define RTL876x_OTA_HEADER_ADDR   (RTL876x_OEM_CONFIG_ADDR + RTL876x_OEM_CONFIG_SIZE)
#define RTL876x_OTA_HEADER_SIZE   (4 * 1024)

#define RTL876x_SECURE_BOOT_PATH  "/dev/secure_boot"
#define RTL876x_SECURE_BOOT_ADDR  (RTL876x_OTA_HEADER_ADDR + RTL876x_OTA_HEADER_SIZE)
#define RTL876x_SECURE_BOOT_SIZE  (4 * 1024)

#define RTL876x_PATCH_PATH        "/dev/patch"
#define RTL876x_PATCH_ADDR        (RTL876x_SECURE_BOOT_ADDR + RTL876x_SECURE_BOOT_SIZE)
#define RTL876x_PATCH_SIZE        (40 * 1024)

#define RTL876x_APPLICATION_PATH  "/dev/application"
#define RTL876x_APPLICATION_ADDR  (RTL876x_PATCH_ADDR + RTL876x_PATCH_SIZE)
#define RTL876x_APPLICATION_SIZE  (228 * 1024)

#define RTL876x_APP_DATA_PATH     "/dev/app_data"
#define RTL876x_APP_DATA_ADDR     (RTL876x_APPLICATION_ADDR + RTL876x_APPLICATION_SIZE)
#define RTL876x_APP_DATA_SIZE     (12 * 1024)

#define RTL876x_OTA_TEMP_PATH     "/dev/ota_temp"
#define RTL876x_OTA_TEMP_ADDR     (RTL876x_APP_DATA_ADDR + RTL876x_APP_DATA_SIZE)
#define RTL876x_OTA_TEMP_SIZE     (200 * 1024)

#define RTL876x_NVS_PATH          "/dev/mtdnvs_flash"
#define RTL876x_NVS_ADDR          (RTL876x_OTA_TEMP_ADDR + RTL876x_OTA_TEMP_SIZE)
#define RTL876x_NVS_SIZE          (12 * 1024)

#define RTL876x_MIJIA_CERT_PATH   "/dev/mijia_cert"
#define RTL876x_MIJIA_CERT_ADDR   (RTL876x_NVS_ADDR + RTL876x_NVS_SIZE)
#define RTL876x_MIJIA_CERT_SIZE   (4 * 1024)

/****************************************************************************
 * Private Function prototypes
 ****************************************************************************/

static int rtl_flash_erase(struct mtd_dev_s *dev, off_t startblock,
                           size_t nblocks);
static ssize_t rtl_flash_bread(struct mtd_dev_s *dev, off_t startblock,
                               size_t nblocks,  uint8_t *buffer);
static ssize_t rtl_flash_bwrite(struct mtd_dev_s *dev, off_t startblock,
                                size_t nblocks,  const uint8_t *buffer);
static ssize_t rtl_flash_read(struct mtd_dev_s *dev, off_t offset,
                              size_t nbytes, uint8_t *buffer);
#ifdef CONFIG_MTD_BYTE_WRITE
static ssize_t rtl_flash_write(struct mtd_dev_s *dev, off_t offset,
                               size_t nbytes, const uint8_t *buffer);
#endif
static int rtl_flash_ioctl(struct mtd_dev_s *dev,
                                       int cmd, unsigned long arg);

/****************************************************************************
 * Private Data
 ****************************************************************************/

static struct mtd_dev_s g_mtd_ops =
{
  .erase             = rtl_flash_erase,
  .bread             = rtl_flash_bread,
  .bwrite            = rtl_flash_bwrite,
  .read              = rtl_flash_read,
#ifdef CONFIG_MTD_BYTE_WRITE
  .write             = rtl_flash_write,
#endif
  .ioctl             = rtl_flash_ioctl,
  .isbad             = NULL,
  .markbad           = NULL,
};

static int g_flash_size = 0;

#ifdef CONFIG_MTD_CONFIG_FAIL_SAFE
struct rtl876x_partition_s
{
  char *path;
  off_t firstblock;
  off_t nblocks;
};

/* Flash mtd partition table */

static const struct rtl876x_partition_s rtl876x_part_table[] =
{
  {
    .path       = RTL876x_OEM_CONFIG_PATH,
    .firstblock = RTL876x_OEM_CONFIG_ADDR / BLOCK_SIZE,
    .nblocks    = RTL876x_OEM_CONFIG_SIZE / BLOCK_SIZE,
  },
  {
    .path       = RTL876x_OTA_HEADER_PATH,
    .firstblock = RTL876x_OTA_HEADER_ADDR / BLOCK_SIZE,
    .nblocks    = RTL876x_OTA_HEADER_SIZE / BLOCK_SIZE,
  },
  {
    .path       = RTL876x_SECURE_BOOT_PATH,
    .firstblock = RTL876x_SECURE_BOOT_ADDR / BLOCK_SIZE,
    .nblocks    = RTL876x_SECURE_BOOT_SIZE / BLOCK_SIZE,
  },
  {
    .path       = RTL876x_PATCH_PATH,
    .firstblock = RTL876x_PATCH_ADDR / BLOCK_SIZE,
    .nblocks    = RTL876x_PATCH_SIZE / BLOCK_SIZE,
  },
  {
    .path       = RTL876x_APPLICATION_PATH,
    .firstblock = RTL876x_APPLICATION_ADDR / BLOCK_SIZE,
    .nblocks    = RTL876x_APPLICATION_SIZE / BLOCK_SIZE,
  },
  {
    .path       = RTL876x_APP_DATA_PATH,
    .firstblock = RTL876x_APP_DATA_ADDR / BLOCK_SIZE,
    .nblocks    = RTL876x_APP_DATA_SIZE / BLOCK_SIZE,
  },
  {
    .path       = RTL876x_OTA_TEMP_PATH,
    .firstblock = RTL876x_OTA_TEMP_ADDR / BLOCK_SIZE,
    .nblocks    = RTL876x_OTA_TEMP_SIZE / BLOCK_SIZE,
  },
  {
    .path       = RTL876x_NVS_PATH,
    .firstblock = RTL876x_NVS_ADDR / BLOCK_SIZE,
    .nblocks    = RTL876x_NVS_SIZE / BLOCK_SIZE,
  },
  {
    .path       = RTL876x_MIJIA_CERT_PATH,
    .firstblock = RTL876x_MIJIA_CERT_ADDR / BLOCK_SIZE,
    .nblocks    = RTL876x_MIJIA_CERT_SIZE / BLOCK_SIZE,
  },
};

#endif
/****************************************************************************
 * Name: rtl_flash_erase
 *
 * Erase the specified erase blocks (units are erase block : 4kB).
 * Semantic Clarification:  Here, we are not referring to the erase block
 *  according to the FLASH data sheet.  Rather, we are referring to
 * the *smallest* erasable part of the FLASH which may have a name like
 * a page or sector or subsector.
 *
 ****************************************************************************/

static int rtl_flash_erase(struct mtd_dev_s *dev, off_t startblock,
                           size_t nblocks)
{
  int ret = true;
  for (int block_num = 0; block_num < nblocks; block_num++)
    {
      ret = __rtl_flash_erase_locked(FLASH_ERASE_SECTOR,
                       FMC_MAIN_ADDR + (startblock + block_num) * 0x1000);
      if (!ret)
        {
          break;
        }
    }
  return ret;
}

/****************************************************************************
 * Name: rtl_flash_bread
 *
 * Description: Read from the specified read blocks
 ****************************************************************************/

static ssize_t rtl_flash_bread(struct mtd_dev_s *dev, off_t startblock,
                               size_t nblocks,  uint8_t *buffer)
{
  bool read_result =
        __rtl_flash_read_locked(FMC_MAIN_ADDR + startblock * BLOCK_SIZE,
        nblocks * BLOCK_SIZE, buffer);
  if (!read_result)
    {
      return 0;
    }
  else
    {
      return nblocks;
    }
}

/****************************************************************************
 * Name: rtl_flash_bwrite
 *
 * Description: write from the specified write blocks
 ****************************************************************************/

static ssize_t rtl_flash_bwrite(struct mtd_dev_s *dev, off_t startblock,
                                size_t nblocks,  const uint8_t *buffer)
{
  bool write_result =
          __rtl_flash_write_locked(FMC_MAIN_ADDR + startblock * BLOCK_SIZE,
          nblocks * BLOCK_SIZE, buffer);
  if (!write_result)
    {
      return 0;
    }
  else
    {
      return nblocks;
    }
}

/****************************************************************************
 * Name: rtl_flash_read
 *
 * Some devices may support byte oriented reads (optional).  Most MTD
 * devices are inherently block oriented so byte-oriented writing is not
 * supported. It is recommended that low-level drivers not support read()
 * if it requires buffering.
 ****************************************************************************/

static ssize_t rtl_flash_read(struct mtd_dev_s *dev, off_t offset,
                              size_t nbytes, uint8_t *buffer)
{
  bool read_result =
        __rtl_flash_read_locked(FMC_MAIN_ADDR + offset, nbytes, buffer);
  if (!read_result)
    {
      return 0;
    }
  else
    {
      return offset;
    }
}

/****************************************************************************
 * Name: rtl_flash_write
 *
 * Some devices may support byte oriented reads (optional).  Most MTD
 * devices are inherently block oriented so byte-oriented writing is not
 * supported. It is recommended that low-level drivers not support read()
 * if it requires buffering.
 ****************************************************************************/

#ifdef CONFIG_MTD_BYTE_WRITE
static ssize_t rtl_flash_write(struct mtd_dev_s *dev, off_t offset,
                               size_t nbytes, const uint8_t *buffer)
{
  bool write_result;
  size_t len = nbytes;

  if ((uint32_t)buffer < 0x800000) {
    write_result = __rtl_flash_write_locked(FMC_MAIN_ADDR + offset,
                                            nbytes, buffer);
    return write_result ? len : -EACCES;
  }

  do {
      uint8_t flash_write_buffer[32];
      size_t write_len = MIN(sizeof(flash_write_buffer), nbytes);

      /* Copy the data to the buffer and write, the temporary buffer is used
        * to make sure the argument buffer is not in flash. Telink flash is
        * not allowed read during flash writing.
        */
      memcpy(flash_write_buffer, buffer, write_len);
      write_result = __rtl_flash_write_locked(FMC_MAIN_ADDR + offset,
                                              write_len, flash_write_buffer);
      if (!write_result)
        {
          return -EACCES;
        }

      offset += write_len;
      buffer += write_len;
      nbytes -= write_len;

  } while (nbytes > 0);

  return len;
}
#endif

/****************************************************************************
 * Name: rtl_flash_ioctl
 *
 * Support other, less frequently used commands:
 *  - MTDIOC_GEOMETRY:  Get MTD geometry
 *  - MTDIOC_XIPBASE:   Convert block to physical address for
 *    eXecute-In-Place
 *  - MTDIOC_BULKERASE: Erase the entire device
 * (see include/nuttx/fs/ioctl.h)
 *
 ****************************************************************************/

static int rtl_flash_ioctl(struct mtd_dev_s *dev, int cmd, unsigned long arg)
{
  int ret = OK;
  switch (cmd)
    {
      case MTDIOC_GEOMETRY:
        {
          struct mtd_geometry_s *geo = (struct mtd_geometry_s *)arg;
          if (geo != NULL)
            {
              memset(geo, 0, sizeof(*geo));
              geo->blocksize    = BLOCK_SIZE;
              geo->erasesize    = ERASE_BLOCK_SIZE;
              geo->neraseblocks = g_flash_size / ERASE_BLOCK_SIZE;
            }

          break;
      }

      case MTDIOC_BULKERASE:
        {
          int whole_block_num = (g_flash_size - ERASE_BLOCK_SIZE)
                                 / ERASE_BLOCK_SIZE;
          for (int block_num = 0; block_num < whole_block_num; block_num++)
            {
              ret =__rtl_flash_erase_locked(FLASH_ERASE_SECTOR,
                   FMC_MAIN_ADDR + ERASE_BLOCK_SIZE +
                   block_num * ERASE_BLOCK_SIZE);
              if (!ret)
                {
                  break;
                }
            }

          break;
        }

      case MTDIOC_ERASESTATE:
        {
          uint8_t *result = (uint8_t *)arg;
          *result = MTD_ERASED_STATE;
          ret = OK;
        }
      default:
        ret = 0;
    }

  return ret;
}

/****************************************************************************
 * Public Functions
 ****************************************************************************/

/****************************************************************************
 * Name: rtl_flash_mtd_initialize
 *
 * Description: Bind a block mode driver that uses the built-in rtl8762e
 * flash programming commands for read/write access to unused flash.
 ****************************************************************************/

struct mtd_dev_s *rtl_flash_mtd_initialize(void)
{
  g_flash_size = __rtl_flash_ioctl(flash_ioctl_get_size_main, 0, 0);
  return &g_mtd_ops;
}

#ifdef CONFIG_MTD_CONFIG_FAIL_SAFE
int rtl876x_partition_init(void)
{
  volatile int ret;
  int num;
  num = sizeof(rtl876x_part_table) / sizeof(struct rtl876x_partition_s);

  struct mtd_dev_s * mtd_dev = rtl_flash_mtd_initialize();

  if (mtd_dev != NULL)
    {
      struct mtd_dev_s *mtdpartition;
      extern int register_mtddriver(const char *path, struct mtd_dev_s *mtd,
                       mode_t mode, void *priv);

      for (uint32_t i = 0; i < num; ++i)
        {
          mtdpartition = mtd_partition(mtd_dev, \
                                       rtl876x_part_table[i].firstblock, \
                                       rtl876x_part_table[i].nblocks);
          if (mtdpartition != NULL)
            {
              ret = register_mtddriver(rtl876x_part_table[i].path, \
                                       mtdpartition, 0666, NULL);
              if (ret != 0)
                {
                  break;
                }
            }

          else
            {
              break;
            }
        }
    }

  else
    {
      ret = -EIO;
    }

  return ret;
}
#endif
#endif /* CONFIG_RTL876x_FLASH */