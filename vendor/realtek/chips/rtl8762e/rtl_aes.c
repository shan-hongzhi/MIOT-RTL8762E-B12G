/****************************************************************************
 * arch/arm/src/rtl8762e/rtl_aes.c
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

#include <stdint.h>

#include <nuttx/irq.h>
#include <nuttx/crypto/crypto.h>
#include <nuttx/mutex.h>
#include <assert.h>
#include <debug.h>
#include <semaphore.h>

#include "hw_aes.h"

#ifdef CONFIG_RTL876x_AES
/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

#define AES_BLK_SHIFT                   (4)
#define AES_BLK_SIZE                    (1 << AES_BLK_SHIFT)

/****************************************************************************
 * Private Types
 ****************************************************************************/

/****************************************************************************
 * Private data
 ****************************************************************************/

static mutex_t g_aes_lock = NXMUTEX_INITIALIZER;

/****************************************************************************
 * Public data
 ****************************************************************************/

/****************************************************************************
 * Private Functions
 ****************************************************************************/

static T_HW_AES_MODE pm_state_to_rtl_aes_mode(int mode)
{
  switch (mode)
    {
      case AES_MODE_ECB:
        return RTL_AES_MODE_ECB;

      case AES_MODE_CBC:
        return RTL_AES_MODE_CBC;

      case AES_MODE_CTR:
        return RTL_AES_MODE_CTR;

      case AES_MODE_CFB:
        return RTL_AES_MODE_CFB;

      default:
        break;
    }

    return RTL_AES_MODE_NONE;
}

static inline void swap_buf(const uint8_t *src, uint8_t *dst, uint32_t len)
{
    uint32_t i;

    for (i = 0; i < len; i++)
    {
        dst[len - 1 - i] = src[i];
    }
}

/****************************************************************************
 * Public Functions
 ****************************************************************************/

int aes_cypher(void *out, const void *in, size_t size, const void *iv,
               const void *key, size_t keysize, int mode, int encrypt)
{
  int ret = OK;
  uint32_t aes_key_swap[4] = {0};
  uint32_t iv_swap[4] = {0};
  uint32_t in_swap[4], out_swap[4];

  /* Input check, error condition*/
  if (size != 16 || keysize != 16)
    {
      return -EINVAL;
    }

  ret = nxmutex_lock(&g_aes_lock);
  if (ret < 0)
    {
      return ret;
    }

  /* encrypt or decrypt */
  swap_buf(key, (uint8_t *)aes_key_swap, keysize);

  if(iv)
    {
      swap_buf(iv, (uint8_t *)iv_swap, 16);
    }

  /* Most significant octet of plaintextData corresponds to in[0] */
  swap_buf(in, (uint8_t *)in_swap, 16);

  if (encrypt)
    {

      __rtl_hw_aes_encrypt128(in_swap, out_swap, size/4, aes_key_swap, iv_swap, pm_state_to_rtl_aes_mode(mode));
    }
  else
    {
      __rtl_hw_aes_decrypt128(in_swap, out_swap, size/4, aes_key_swap, iv_swap, pm_state_to_rtl_aes_mode(mode));
    }

  swap_buf((uint8_t *)out_swap, out, 16);

  ret = nxmutex_unlock(&g_aes_lock);
  if (ret < 0)
    {
      return ret;
    }

  return ret;
}
#endif