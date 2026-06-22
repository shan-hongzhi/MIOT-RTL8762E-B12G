/****************************************************************************
 * arch/arm/src/rtl8762e/rtl_flash.h
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

#ifndef __ARCH_ARM_SRC_RTL_RTL876X_FLASH_H
#define __ARCH_ARM_SRC_RTL_RTL876X_FLASH_H

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include <errno.h>
#include "rtl876x.h"
#include <nuttx/config.h>
#include <nuttx/irq.h>
#include <nuttx/mtd/mtd.h>
#include <string.h>
#include "flash_device.h"
/****************************************************************************
 * Public Function Prototypes
 ****************************************************************************/

/****************************************************************************
 * Name: rtl_timer_initialize
 ****************************************************************************/
#ifdef CONFIG_RTL876x_FLASH

struct mtd_dev_s *rtl_flash_mtd_initialize(void);

#ifdef CONFIG_MTD_CONFIG_FAIL_SAFE
int rtl876x_partition_init(void);
#endif
#endif /* CONFIG_RTL876x_FLASH */
#endif /* __ARCH_ARM_SRC_RTL_RTL876X_FLASH_H */