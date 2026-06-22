/**
*****************************************************************************************
*     Copyright(c) 2017, Realtek Semiconductor Corporation. All rights reserved.
*****************************************************************************************
  * @file    hw_aes.h
  * @brief   HW aes apis abstracted for encryption related.
  * @details AES encryption APIs which delivers HW implemented reliable and safe AES solution.
  * @author  Lory Xu
  * @date    2017.5.17
  * @version v1.0
  * *************************************************************************************
  */

#ifndef HW_AES_H
#define HW_AES_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>

/** @defgroup  HW_AES    Hardware AES
    * @brief API Sets for hardware AES engine
    * @{
    */


/*============================================================================*
  *                                   Types
  *============================================================================*/
/** @defgroup HW_AES_Exported_Types HW AES Exported Types
  * @{
  */

/** @brief encrytion zone for image encryption feature. */
typedef enum
{
    RTL_ZONE0,
    RTL_ZONE1,
    RTL_ZONE_NOT_USED
} T_PROTECT_ZONE;

/** @brief AES mode definition for HW AES. */
typedef enum
{
    RTL_AES_MODE_NONE,
    RTL_AES_MODE_CBC,
    RTL_AES_MODE_ECB,
    RTL_AES_MODE_CFB,
    RTL_AES_MODE_OFB,
    RTL_AES_MODE_CTR
} T_HW_AES_MODE;

/** End of HW_AES_Exported_Types
  * @}
  */

/*============================================================================*
  *                                Functions
  *============================================================================*/
/** @defgroup HW_AES_Exported_Functions HW AES Exported Functions
    * @brief
    * @{
    */

extern bool __rtl_hw_aes_decrypt_16byte(uint8_t *input, uint8_t *output);

/**
    * @brief  128 bit AES encryption via DMA on speicified plain data and keys
    * @param  p_in              specifed plain data to be encypted
    * @param  p_out            output buffer to store encrypted data
    * @param  data_word_len     input buffer length
    * @param  key               key buffer
    * @param  iv                initialization vector for AES CBC mode
    * @param  mode              aes mode specified by @ref T_HW_AES_MODE
    * @return encryption results
    * @retval true      successful
    * @retval false     fail
    * @note   HWAES_DMA_RX_CH_NUM and HWAES_DMA_TX_CH_NUM DMA channels are occupied by this function
    */
bool __rtl_hw_aes_encrypt128_use_dma(uint32_t *p_in, uint32_t *p_out, uint16_t data_word_len,
                               uint32_t *key, uint32_t *iv, T_HW_AES_MODE mode);
/**
    * @brief  128 bit AES decryption via DMA on speicified data and keys
    * @param  p_in              specifed encrypted data to be decypted
    * @param  p_out             output buffer to store plain data
    * @param  data_word_len     input buffer length
    * @param  key               key buffer
    * @param  iv                initialization vector for AES CBC mode
    * @param  mode              aes mode specified by @ref T_HW_AES_MODE
    * @return encryption results
    * @retval true      successful
    * @retval false     fail
    * @note   HWAES_DMA_RX_CH_NUM and HWAES_DMA_TX_CH_NUM DMA channels are occupied by this function
    */
bool __rtl_hw_aes_decrypt128_use_dma(uint32_t *p_in, uint32_t *p_out, uint16_t data_word_len,
                               uint32_t *key, uint32_t *iv, T_HW_AES_MODE mode);
/**
    * @brief  256 bit AES encryption via DMA on speicified plain data and keys
    * @param  p_in              specifed plain data to be encypted
    * @param  p_out             output buffer to store encrypted data
    * @param  data_word_len     input buffer length
    * @param  key               key buffer
    * @param  iv                initialization vector for AES CBC mode
    * @param  mode              aes mode specified by @ref T_HW_AES_MODE
    * @return encryption results
    * @retval true      successful
    * @retval false     fail
    * @note   HWAES_DMA_RX_CH_NUM and HWAES_DMA_TX_CH_NUM DMA channels are occupied by this function
    */
bool hw_aes_encrypt256_use_dma(uint32_t *p_in, uint32_t *p_out, uint16_t data_word_len,
                               uint32_t *key, uint32_t *iv, T_HW_AES_MODE mode);
/**
    * @brief  256 bit AES decryption via DMA on speicified data and keys
    * @param  p_in              specifed encrypted data to be decypted
    * @param  p_out             output buffer to store plain data
    * @param  data_word_len     input buffer length
    * @param  key               key buffer
    * @param  iv                initialization vector for AES CBC mode
    * @param  mode              aes mode specified by @ref T_HW_AES_MODE
    * @return encryption results
    * @retval true      successful
    * @retval false     fail
    * @note   HWAES_DMA_RX_CH_NUM and HWAES_DMA_TX_CH_NUM DMA channels are occupied by this function
    */
bool hw_aes_decrypt256_use_dma(uint32_t *p_in, uint32_t *p_out, uint16_t data_word_len,
                               uint32_t *key, uint32_t *iv, T_HW_AES_MODE mode);
bool __rtl_hw_aes_encrypt128(uint32_t *p_in, uint32_t *p_out, uint16_t data_word_len,
                      uint32_t *p_key,
                      uint32_t *p_iv, T_HW_AES_MODE mode);



bool __rtl_hw_aes_decrypt128(uint32_t *p_in, uint32_t *p_out, uint16_t data_word_len,
                      uint32_t *p_key,
                      uint32_t *p_iv, T_HW_AES_MODE mode);



bool __rtl_hw_aes_encrypt256(uint32_t *p_in, uint32_t *p_out, uint16_t data_word_len,
                      uint32_t *p_key,
                      uint32_t *p_iv, T_HW_AES_MODE mode);



bool __rtl_hw_aes_decrypt256(uint32_t *p_in, uint32_t *p_out, uint16_t data_word_len,
                      uint32_t *p_key,
                      uint32_t *p_iv, T_HW_AES_MODE mode);
/** @} */ /* End of group HW_AES_Exported_Functions */


/** @} */ /* End of group HW_AES */
#ifdef __cplusplus
}
#endif

#endif
