/**
*****************************************************************************************
*     Copyright(c) 2015, Realtek Semiconductor Corporation. All rights reserved.
*****************************************************************************************
  * @file    flash_driver.h
  * @author
  * @version V0.0.1
  * @date    2016-10-19
  * @brief   This file contains all the functions prototypes for the spi flash
  *          library.
  ******************************************************************************
  * @attention
  *
  * This module is a confidential and proprietary property of RealTek and
  * possession or use of this module requires written permission of RealTek.
  *
  * Copyright(c) 2016, Realtek Semiconductor Corporation. All rights reserved.
  ******************************************************************************
  */

/*****************************************************************************************
* Introduction
*****************************************************************************************
* - This module implements the spic & flash functionality.
* - Since different spi flash would define different op code for the same commands,
* - so this driver will mitigate this situation and provide the same APIs.
******************************************************************************************/



#ifndef _FLASH_DRIVER_H
#define _FLASH_DRIVER_H
#include <stdint.h>
#include <stdbool.h>
#include "rtl876x.h"
#include "flash_device.h"

#define _PACKED_                __attribute__((packed))

/** @defgroup FLASH_DRIVER    Flash Driver
    * @brief Flash driver
    * @{
    */

/*****************************************************************************************
* Introduction
*****************************************************************************************
* - This module enables spi flash controller functionality for Bee2
* - The spi flash controller spec locates @
* - https://wiki.realtek.com/display/BTFWKNOW/BumbleBee3+Project
* - The design is based on Rel2.0 spec, and may subject to change without notification
* The SPIC is used to communicate with spi flash, and it supports
* - user mode and auto mode
*     User mode is typical software flow to implement all serial transfer, while auto mode will
*     leverage HW mechanism to access SPI flash as easy as access memory.
* - 1/2/4 channel data bit to TX/RX data
* - flexible command register to support different flash vendor.
******************************************************************************************/

/*============================================================================*
  *                                   Macros
  *============================================================================*/
/** @defgroup FLASH_DRIVER_Exported_Macros Flash Driver Exported Macros
    * @brief
    * @{
    */
#define NO_RECOMMAND_API

#define FLASH_SECTOR_SIZE       (4  * 1024)
#define FLASH_BLOCK_SIZE        (16 * FLASH_SECTOR_SIZE)
#define FLASH_DR_ADDR           (0x40080060)
#define FLASH_DR_ADDR0          (0x40080060)
#define FLASH_DR_ADDR1          (0x40090060)
#define FLASH_DR_ADDR2          (0x400A0060)

#define FLASH_SIZE_MAX          (8 * 1024 * 1024)   /*!< depends on flash auto mode limit, size can't > 8MB. */
#define FLASH_ERASE_RETRY_MAX   10

/* macro about flash calibration */
#define FLASH_CAL_DELAY_CNT_UNIT        32
#define FLASH_CAL_DELAY_LINE_MAX        99
#define CAL_ADDR 0x802030//__rtl_get_header_addr_by_img_id(OTA) + offsetof(T_OTA_HEADER_FORMAT, magic_pattern)

#define FLASH_CAL_DEBUG_LOG 0
#if (FLASH_CAL_DEBUG_LOG == 1)
#define FLASH_CAL_LOG(...) DBG_DIRECT(__VA_ARGS__)
#else
#define FLASH_CAL_LOG(...)
#endif

#define IS_SAME_FLASH_PAGE(addr0, addr1)    (((addr0) & 0xFFFFFF00) == ((addr1) & 0xFFFFFF00))

/**magic value for flash calibration, use magic pattern in patch ext header*/
//#define MAGIC_ADDR_OFFS  (FMC_PATCH_ADDR - FMC_MAIN + sizeof(T_IMAGE_HEADER))
//won't flash patch into fpga. TODO: need find some better place for calibration.
#ifndef _IS_ASIC_
#define MAGIC_DATA_PATTERN FLASH_TABLE_MAGIC_PATTERN
#else
#define MAGIC_DATA_PATTERN FLASH_TABLE_MAGIC_PATTERN
#endif


#define FLASH_VENDOR_MXIC           0XC2
#define FLASH_VENDOR_GD             0XC8
#define FLASH_VENDOR_MICRON         0X20
#define FLASH_VENDOR_WINBOND        0XEF
#define FLASH_VENDOR_ATMEL          0X1F
#define FLASH_VENDOR_SST            0XBF
#define FLASH_VENDOR_SPANSION       0X01
#define FLASH_VENDOR_PARAGON        0xE0
#define FLASH_VENDOR_FUDAN          0xA1
#define FLASH_VENDOR_ESMT           0x1C
#define FLASH_VENDOR_XTX            0x0B
#define FLASH_VENDOR_ZBIT           0x5E
#define FLASH_VENDOR_PUYA           0x85


/** End of FLASH_DRIVER_Exported_Macros
    * @}
    */

/*============================================================================*
  *                                   Types
  *============================================================================*/
/** @defgroup FLASH_DRIVER_Exported_Types Flash Driver Exported Types
  * @brief
  * @{
  */

/********************  Calibration Return Value  *******************/
typedef enum
{
    FLASH_CAL_SUCCESS               =  1,
    FLASH_CAL_NOT_SUPPORT           =  0,
    FLASH_CAL_DLY_COUNT_NOT_FOUND   = -1,
    FLASH_CONFIG_ERROR_TIMEOUT      = -2

} T_FLASH_CAL_ERROR_CODE;


/********************  Access mode of DR register in user mode  *******************/
typedef enum
{
    DATA_BYTE         = 0,
    DATA_HALF         = 1,
    DATA_WORD         = 2
} T_SPIC_BYTE_NUM;

/********************  Flash Interface Type  *******************/
typedef enum
{
    FLASH_MASTER_INTERNAL   = 0,    //!< Master Role with Internal Interface
    FLASH_MASTER_EXTERNAL   = 1,    //!< Master Role with External Interface
    FLASH_SLAVE_EXTERNAL    = 2     //!< Slave Role with External Interface
} T_FLASH_INTERFACE_TYPE;

typedef enum
{
    FLASH_LOCK_USER_MODE_SLEEP = 0,
    FLASH_LOCK_USER_MODE_READ,
    FLASH_LOCK_USER_MODE_WRITE,
    FLASH_LOCK_USER_MODE_ERASE,
    FLASH_LOCK_AUTO_MODE_READ,
    FLASH_LOCK_AUTO_MODE_WRITE,
    FLASH_LOCK_AUTO_MODE_ERASE,
    FLASH_LOCK_USER_READ_WRITE_STATUS_REG,   //add for unlock bp api

    FLASH_LOCK_NONE = 0xFF
} T_LOCK_TYPE;

typedef enum
{
    FLASH_BP_LV_0       = 0x00000000,
    FLASH_BP_LV_1       = 0x00010000,
    FLASH_BP_LV_2       = 0x00020000,
    FLASH_BP_LV_3       = 0x00040000,
    FLASH_BP_LV_4       = 0x00080000,
    FLASH_BP_LV_5       = 0x00100000,
    FLASH_BP_LV_6       = 0x00200000,
    FLASH_BP_LV_7       = 0x00400000,
    FLASH_BP_LV_8       = 0x00800000,
    FLASH_BP_LV_9       = 0x01000000,
} T_FLASH_BP_LV;

typedef enum
{
    FLASH_DP_MXIC = 0,
    FLASH_DP_CMD,
    FLASH_DP_OTHERS,
} T_FLASH_DP_TYPE;

typedef enum
{
    CHANN_SINGLE = 0,
    CHANN_DUAL = 1,
    CHANN_QUAD = 2
} T_CHANN_TYPE;

typedef struct
{
    /* cal info */
    uint8_t     sel_dly_cnt;
    uint8_t     sel_dly_cnt_first;
    uint8_t     sel_dly_line;
    uint8_t     sel_auto_dly_cnt;

} FLASH_CAL_INFO;

typedef enum
{
    FLASH_CAL_SLOW_MODE,
    FLASH_CAL_FAST_MODE
} FLASH_CAL_MODE;

typedef struct
{
    uint8_t              channel_num; /*!< Log DMA Channel Number          */
    GDMA_ChannelTypeDef *channel;     /*!< Log DMA Channel (Register Base) */
    IRQn_Type            irq_no;      /*!< Log DMA Channel IRQ Number      */
    VECTORn_Type         vector_no;   /*!< Log DMA Channel Vector Number   */
    IRQ_Fun              isr;         /*!< Log DMA Channel ISR Handler     */
} T_FLASH_DMA_CFG;

typedef union _PACKED_ _FLASH_ACCESS_INFO
{
    uint32_t d32[2];
    struct
    {
        uint32_t cmd: 8;
        uint32_t addr;
        uint32_t addr_len: 3;
        uint32_t cmd_ch: 2;
        uint32_t addr_ch: 2;
        uint32_t data_ch: 2;
        uint32_t dummy_len: 15;
    };
} FLASH_ACCESS_INFO;

typedef struct
{
    uint32_t      active_pattern;
    __IO uint32_t ctrlr0;           /**< SPIC control register0,        Address offset: 0x000 */
    __IO uint32_t ser;              /**< SPIC slave enable register,    Address offset: 0x010 */
    __IO uint32_t baudr;            /**< SPIC baudrate select register, Address offset: 0x014 */
    __IO uint32_t imr;              /**< SPIC interrupt mask register,      Address offset: 0x02C */
    __IO uint32_t rd_fast_single;   /**< Fast read data command of SPI Flash, offset: 0x0E0 */
    __IO uint32_t rd_dual_o;        /**< Dual output read command of SPI Flash, offset: 0x0E4 */
    __IO uint32_t rd_dual_io;       /**< Dual I/O read command of SPI Flash, offset: 0x0E8 */
    __IO uint32_t rd_quad_o;        /**< Quad output read command of SPI Flash, offset: 0x0EC */
    __IO uint32_t rd_quad_io;       /**< Quad I/O read command of SPI Flash, offset: 0x0F0 */
    __IO uint32_t wr_single;        /**< Page program command of SPI Flash,  offset: 0x0F4 */
    __IO uint32_t wr_dual_i;        /**< Dual data input program command of SPI Flash, offset: 0x0F8 */
    __IO uint32_t wr_dual_ii;       /**< Dual address&data program command of SPI Flash,offset: 0x0FC */
    __IO uint32_t wr_quad_i;        /**< Quad data input program command of SPI Flash,offset: 0x100 */
    __IO uint32_t wr_quad_ii;       /**< Quad address&data program command of SPI Flash,offset: 0x104 */
    __IO uint32_t wr_enable;        /**< Write enabe command of SPI Flash,  Address offset: 0x108 */
    __IO uint32_t rd_status;        /**< Read status command of SPI Flash,   offset: 0x10C */
    __IO uint32_t ctrlr2;           /**< SPIC control register2,            Address offset: 0x110 */
    __IO uint32_t fbaudr;           /**< SPIC fast baudrate select,         Address offset: 0x114 */
    __IO uint32_t user_length;      /**< SPIC address length register,      Address offset: 0x118 */
    __IO uint32_t auto_length;      /**< SPIC auto address length register, Address offset: 0x11C */
    __IO uint32_t valid_cmd;        /**< SPIC valid command register,       Address offset: 0x120 */
    __IO uint32_t dum_byte;         /**< SPIC dummy byte value register,       Address offset: 0x12C */
    __IO uint32_t tpr0;             /**< SPIC timing parameters register,       Address offset: 0x138 */
} SPIC_BKP_TypeDef;

typedef void (*FlashCB)(void);

/** End of FLASH_DRIVER_Exported_Types
  * @}
  */

/** @defgroup FLASH_DRIVER_Registers_Definitions Flash Driver Registers Definitions
  * @brief
  * @{
  */
/*****************************************************************************************
* Registers Definitions --------------------------------------------------------*
********************  Bits definition for SPIC_CTRLR0 register  *******************/
#define BIT_USER_MODE           (0x00000001UL << 31)
#define BIT_UAR                 (0x00000001 << 30)
#define BIT_CK_MTIMES(x)        (((x) & 0x0000001F) << 23)
#define BIT_FAST_RD(x)          (((x) & 0x00000001) << 22)
#define BIT_CMD_CH(x)           (((x) & 0x00000003) << 20)
#define BIT_DATA_CH(x)          (((x) & 0x00000003) << 18)
#define BIT_ADDR_CH(x)          (((x) & 0x00000003) << 16)
#define BIT_DDR_EN(x)           (((x) & 0x00000007) << 13)
#define BIT_GCLK_DIS            (0x00000001 << 10)
#define BIT_TMOD(x)             (((x) & 0x00000003) << 8)
#define BIT_SCPOL               (0x00000001 << 7)
#define BIT_SCPH                (0x00000001 << 6)
#define BIT_SIPOL_EN            (0x00000001 << 5)
#define BIT_SIPOL               ((x) & 0x0000001F)

/********************  Bits definition for SPIC_RX_NDF register  *******************/
#define BIT_NDF(x)              ((x) & 0xffffff)

/********************  Bits definition for SPIC_SSIENR register  *******************/
#define BIT_ATCK_CMD            (0x00000001 << 1)
#define BIT_SPIC_EN             (0x00000001)

/********************  Bits definition for SPIC_BAUDR register  *******************/
#define BIT_SCKDV(x)                ((x) & 0x0fff)

/********************  Bits definition for SPIC_SR register  *******************/
#define BIT_TXE                 (0x00000001 << 5)
#define BIT_RFF                 (0x00000001 << 4)
#define BIT_RFNE                (0x00000001 << 3)
#define BIT_TFE                 (0x00000001 << 2)
#define BIT_TFNF                (0x00000001 << 1)
#define BIT_BUSY                (0x00000001)

/********************  Bits definition for SPIC_IMR register  *******************/
#define BIT_NWEIM               (0x00000001 << 16)
#define BIT_STFIM               (0x00000001 << 15)
#define BIT_STOIM               (0x00000001 << 14)
#define BIT_STUIM               (0x00000001 << 13)
#define BIT_DRSIM               (0x00000001 << 12)
#define BIT_ACSIM               (0x00000001 << 11)
#define BIT_RXSIM               (0x00000001 << 10)
#define BIT_TXSIM               (0x00000001 << 9)
#define BIT_ACEIM               (0x00000001 << 8)
#define BIT_BYEIM               (0x00000001 << 7)
#define BIT_WBEIM               (0x00000001 << 6)
#define BIT_FSEIM               (0x00000001 << 5)
#define BIT_RXFIM               (0x00000001 << 4)
#define BIT_RXOIM               (0x00000001 << 3)
#define BIT_RXUIM               (0x00000001 << 2)
#define BIT_TXOIM               (0x00000001 << 1)
#define BIT_TXEIM               (0x00000001)

/********************  Bits definition for SPIC_ISR register  *******************/
#define BIT_NWEIS               (0x00000001 << 16)
#define BIT_STFIS               (0x00000001 << 15)
#define BIT_STOIS               (0x00000001 << 14)
#define BIT_STUIS               (0x00000001 << 13)
#define BIT_DRSIS               (0x00000001 << 12)
#define BIT_ACSIS               (0x00000001 << 11)
#define BIT_RXSIS               (0x00000001 << 10)
#define BIT_TXSIS               (0x00000001 << 9)
#define BIT_ACEIS               (0x00000001 << 8)
#define BIT_BYEIS               (0x00000001 << 7)
#define BIT_WBEIS               (0x00000001 << 6)
#define BIT_FSEIS               (0x00000001 << 5)
#define BIT_RXFIS               (0x00000001 << 4)
#define BIT_RXOIS               (0x00000001 << 3)
#define BIT_RXUIS               (0x00000001 << 2)
#define BIT_TXOIS               (0x00000001 << 1)
#define BIT_TXEIS               (0x00000001)

/********************  Bits definition for SPIC_RISR register  *******************/
#define BIT_NWEIR               (0x00000001 << 16)
#define BIT_STFIR               (0x00000001 << 15)
#define BIT_STOIR               (0x00000001 << 14)
#define BIT_STUIR               (0x00000001 << 13)
#define BIT_DREIR               (0x00000001 << 12)
#define BIT_ACSIR               (0x00000001 << 11)
#define BIT_TFSIR               (0x00000001 << 10)
#define BIT_USEIR               (0x00000001 << 9)
#define BIT_ACEIR               (0x00000001 << 8)
#define BIT_BYEIR               (0x00000001 << 7)
#define BIT_WBEIR               (0x00000001 << 6)
#define BIT_FSEIR               (0x00000001 << 5)
#define BIT_RXFIR               (0x00000001 << 4)
#define BIT_RXOIR               (0x00000001 << 3)
#define BIT_RXUIR               (0x00000001 << 2)
#define BIT_TXOIR               (0x00000001 << 1)
#define BIT_TXEIR               (0x00000001)

/********************  Bits definition for SPIC_CTRLR2 register  *******************/
#define BIT_FULL_WR             (0x00000001 << 13)
#define BIT_DM_ACT              (0x00000001 << 12)
#define BIT_RX_FIFO_ENTRY(x)    (((x) & 0x0000000f) << 8)
#define BIT_TX_FIFO_ENTRY(x)    (((x) & 0x0000000f) << 4)
/* Indicate the WPn input pin of SPI Flash is connected to:
   0(default): WP=spi_sout[2], 1:WP=spi_sout[3]. */
#define BIT_WPN_DNUM            (0x00000001 << 2)
/*write protect function.*/
#define BIT_WPN_SET             (0x00000001 << 1)
/* SO pin of SPI Flash.0: SO connects to spi_sout[0]. 1(default): SO connects to spi_sout[1].*/
#define BIT_SO_DNUM             (0x00000001)

/********************  Bits definition for SPIC_USER_LENGTH register  *******************/
#define BIT_USER_ADDR_LENGTH(x)        (((x) & 0x0000000f) << 16)
#define BIT_USER_CMD_LENGTH(x)         (((x) & 0x00000003) << 12)
#define BIT_USER_RD_DUMMY_LENGTH(x)    ((x) & 0x00000fff)

/********************  Bits definition for SPIC_AUTO_LENGTH register  *******************/
#define BIT_RDSR_DUMMY_LENGTH(x) (((x) & 0x000000ff) << 20)
#define BIT_AUTO_ADDR_LENGTH(x)  (((x) & 0x0000000f) << 16)
#define BIT_IN_PHYSICAL_CYC(x)   (((x) & 0x0000000f) << 12)
#define BIT_RD_DUMMY_LENGTH(x)   (((x) & 0x00000fff))

/********************  Bits definition for SPIC_VALID_CMD register  *******************/
#define BIT_WR_TRANS_EN         (0x00000001 << 15)
#define BIT_RD_TRANS_EN         (0x00000001 << 14)
#define BIT_DUM_CH              (0x00000001 << 13)
#define BIT_CTRLR0_CH           (0x00000001 << 12)
#define BIT_PRM_EN              (0x00000001 << 11)
#define BIT_RM_WEN              (0x00000001 << 10)
#define BIT_RM_RDSR             (0x00000001 << 9)
#define BIT_WR_QUAD_II          (0x00000001 << 8)
#define BIT_WR_QUAD_I           (0x00000001 << 7)
#define BIT_WR_DUAL_II          (0x00000001 << 6)
#define BIT_WR_DUAL_I           (0x00000001 << 5)
#define BIT_RD_QUAD_IO          (0x00000001 << 4)
#define BIT_RD_QUAD_O           (0x00000001 << 3)
#define BIT_RD_DUAL_IO          (0x00000001 << 2)
#define BIT_RD_DUAL_I           (0x00000001 << 1)
#define BIT_FRD_SINGEL          (0x00000001)

/** End of FLASH_DRIVER_Registers_Definitions
  * @}
  */

/*************************************************************************************************
*                                        functions
*************************************************************************************************/
/** @defgroup FLASH_DRIVER_Exported_Functions Flash Driver Exported Functions
    * @brief
    * @{
    */
/*****************************  Flash erase callback  ****************************/
extern FlashCB flash_erase_enter;
extern FlashCB flash_erase_exit;

extern uint8_t SystemInitFailInfo[3];


__STATIC_INLINE void reg_flash_erase_enter(FlashCB fun)
{
    flash_erase_enter = fun;
}

__STATIC_INLINE void reg_flash_erase_exit(FlashCB fun)
{
    flash_erase_exit = fun;
}

/*****************************  Flash write protect callback  ****************************/
extern FlashCB flash_wp_enable;
extern FlashCB  flash_wp_disable;

__STATIC_INLINE void reg_flash_wp_enable(FlashCB fun)
{
    flash_wp_enable = fun;
}

__STATIC_INLINE void reg_flash_wp_disable(FlashCB fun)
{
    flash_wp_disable = fun;
}

/*********************************  Flash lock callback  ********************************/
extern FlashCB flash_disable_intr;
extern FlashCB flash_enable_intr;

__STATIC_INLINE void reg_flash_disable_intr(FlashCB fun)
{
    flash_disable_intr = fun;
}

__STATIC_INLINE void reg_flash_enable_intr(FlashCB fun)
{
    flash_enable_intr = fun;
}

/*********************************  Flash sleep callback  ********************************/
extern FlashCB flash_sleep_enter;
extern FlashCB flash_sleep_exit;

__STATIC_INLINE void reg_flash_sleep_enter(FlashCB fun)
{
    flash_sleep_enter = fun;
}

__STATIC_INLINE void reg_flash_sleep_exit(FlashCB fun)
{
    flash_sleep_exit = fun;
}



__STATIC_INLINE void flash_copy_from_otp(uint8_t *dest, uint8_t *src, uint16_t len)
{
    for (int i = 0; i < len; i++)
    {
        dest[i] = src[i];
    }
}

/**
 * @brief set spic baudrate register.
 *
 * @param baudrate  to set SPI flash baudrate in user mode.
 * @return
*/
__STATIC_INLINE void spic_set_baudrate(uint16_t baudrate)
{
    if (baudrate > 0 && baudrate < 0xf000)
    {
        SPIC->baudr = BIT_SCKDV(baudrate);
    }
}

/**
 * @brief set spic enable or disable.
 *
 * @param enable          DISABLE to disable cache, ENABLE to enable cache.
 * @return
 * @note: spic should be disabled before programming any register in user mode
*/
__STATIC_INLINE void spic_enable(uint32_t enable)
{
    SPIC->ssienr = enable;
}

/**
 * @brief set addr channel and data channel in ctrl0 register.
 *
 * @return
*/
__STATIC_INLINE void spic_set_multi_ch(uint32_t data_ch, uint32_t addr_ch)
{
    SPIC->ctrlr0 &= ~(BIT_CMD_CH(3) | BIT_ADDR_CH(3) | BIT_DATA_CH(3));

    SPIC->ctrlr0 |= (BIT_ADDR_CH(addr_ch) | BIT_DATA_CH(data_ch));
}

/**
 * @brief clr addr channel and data channel in ctrl0 register.
 *
 * @return
*/
__STATIC_INLINE void spic_clr_multi_ch(void)
{
    SPIC->ctrlr0 &= ~(BIT_CMD_CH(3) | BIT_ADDR_CH(3) | BIT_DATA_CH(3));
}

/**
 * @brief set RX mode in ctrl0 register in user mode.
 *
 * @return
*/
__STATIC_INLINE void spic_set_rx_mode(void)
{
    SPIC->ctrlr0 |= BIT_TMOD(3);
}

/**
 * @brief set spic ctrl1 register.
 *
 * @param ndf   indicate number of data frames that spic will not stop receiving unitl date frame
 *              equals to NDF. This is used for user mode
 * @return
*/
__STATIC_INLINE void spic_set_rx_ndf(uint32_t ndf)
{
    SPIC->rx_ndf = BIT_NDF(ndf);
}

/**
 * @brief set TX mode in ctrlr0 register in user mode.
 *
 * @return
*/
__STATIC_INLINE void spic_set_tx_mode(void)
{
    SPIC->ctrlr0 &= ~(BIT_TMOD(3));
}


/**
 * @brief set spic dr register to implement user mode programming
 *
 * @param byte_num    byte number for every tx.
 * @param data        data to be write into fifo
 * @return
 * @note: dr0 is used to do programming, and endian issue is considered as well.
*/
__STATIC_INLINE void spic_set_dr(T_SPIC_BYTE_NUM byte_num, uint32_t data)
{
    uint32_t wr_data;
    if (byte_num == DATA_HALF)
    {
        wr_data = rtk_cpu_to_le16(data);
        SPIC->dr[0].half = wr_data;
    }
    else if (byte_num == DATA_WORD)
    {
        wr_data = rtk_cpu_to_le32(data);
        SPIC->dr[0].word = wr_data;
    }
    else
    {
        SPIC->dr[0].byte = data;
    }
    return;
}

/**
 * @brief get spic dr register to implement user mode reading
 *
 * @param byte_num    byte number for every tx.
 * @return  data poping from FIFO
 * @note: dr0 is used to do reading, and endian issue is considered as well.
*/
__STATIC_INLINE uint32_t spic_get_dr(T_SPIC_BYTE_NUM byte_num)
{
    uint32_t rd_data = 0;
    if (byte_num == DATA_HALF)
    {
        rd_data = rtk_le16_to_cpu(SPIC->dr[0].half);
    }
    else if (byte_num == DATA_WORD)
    {
        rd_data = rtk_le32_to_cpu(SPIC->dr[0].word);
    }
    else
    {
        rd_data = SPIC->dr[0].byte;
    }
    return rd_data;
}

/**
 * @brief update specific field on SPIC register
 * @param addr    address of SPIC register
 * @param mask    mask of field on SPIC register
 * @param data    data to be written in
 * @return
*/
__STATIC_INLINE void spic_update(__IO uint32_t *addr, uint32_t mask, uint32_t data)
{
    __IO uint32_t temp = *addr & ~mask;
    *addr = temp | (data & mask);
}

/**
 * @brief set dummy_cycle in autolen register[11:0].
 *
 * @return
*/

__STATIC_INLINE void spic_set_rd_dummy_length(uint8_t dummy_len)
{
    uint32_t data = BIT_RD_DUMMY_LENGTH(dummy_len);
    spic_update(&SPIC->user_length, 0xfff, data);
    spic_update(&SPIC->auto_length, 0xfff, data);
}

/**
 * @brief set auto_dum_len in auto_len register[25:18].
 * @note  if RDSR_DUMMY_LENGTH is bigger than 0, auto/user mode addr channel has to be the same
 * as 0x10C[27:26] ST_CMD_CH.
 * @return
*/
__STATIC_INLINE void spic_set_rdsr_dummy_length(uint8_t dummy_len)
{
    uint32_t data = BIT_RDSR_DUMMY_LENGTH(dummy_len);
    spic_update(&SPIC->auto_length, 0xff, data);
}

/**
 * @brief set auto_dum_len in auto_len register[25:18].
 *
 * @return
*/
__STATIC_INLINE void spic_set_auto_dum_len(uint8_t dummy_len)
{
    uint32_t data = BIT_RD_DUMMY_LENGTH(dummy_len);
    spic_update(&SPIC->auto_length, 0xfff, data);
}

/**
 * @brief set spic slave enable register.
 *
 * @param ser_num  to select target SPI flash in user mode.
 * @return
*/
__STATIC_INLINE void spic_set_ser(uint32_t ser_num)
{
    SPIC->ser = 1 << ser_num;
}

/**
* @brief  This function is used to send Rx command to flash to get status register or flash id.
* @param  cmd: command that need to be sent.
* @param  read_len: the number of bytes that will be read by SPIC after sending cmd.
* @param  read_buf: pointer to a byte array which is used to save datas received.
* @return true if success
* @note This function is only used for rx status/flash id ,not used for read flash data.
*/
extern bool flash_cmd_rx(FLASH_ACCESS_INFO *info, uint32_t read_len, uint8_t *read_buf);

/**
 * @brief  This function is used to send Flash Instruction with Data Phase.
 * @param  info: The SPIC setting need to be sent.
 * @param  data_len: The length of Data phase.
 * @param  data_buf: pointer to byte array which are to be sent in Data phase.
 * @return true if success
 * @note
 *     - This function works User mode.
 *     - This function can only be used to tx cmd(WREN,WRSR,DP,RDP,ERASE etc.),not used to tx data.
 */
extern bool flash_cmd_tx(FLASH_ACCESS_INFO *info, uint32_t data_len, uint8_t *data_buf);

/**
 * @brief wait till flash is not busy.
 * @return true if success
*/
extern bool flash_wait_busy(void);

/**
 * @brief enable flash writing via sending WREN cmd to WLE bit in flash status register.
 * @return true if success
 * @note  before sending specific commands, this command is required to send to enable writig.
*/
extern bool flash_write_enable(void);
/**
 * @brief do calibration for current read mode to get rx dummy length.
 * @param none
 * @return none
*/
extern bool flash_calibration(void);
/**
 * @brief set/leave flash to/from high power mode
 * @param new_status  status to be set, ENABLE or DISABLE.
 * @retval 1 if success
 * @retval 0 if not support
 * @retval -1 if fail
 * @note some vendor has to be set either status/status2
*/
extern T_FLASH_RET flash_set_hpm(uint32_t new_status);

/**
 * @brief set baudrate
 * @param
 * @return none
*/
extern void flash_set_baudrate(uint16_t sck_dv);

/**
 * @brief init flash
 * @param none
 * @return true if success
 */
extern bool flash_init(void);

/**
 * @brief erase flash with specific type and address
 * @param type  erase type
 * @param addr  address to erase when erase block or sector
 * @return true if success
 * @note use this function only when no concern to be preempted, otherwise, use the locked one
*/
extern bool __rtl_flash_erase(T_ERASE_TYPE type, uint32_t addr);
/**
 * @brief send a dummy commnad to toggle #CS
 * @return none
*/
extern bool flash_cmd_toggle_cs(void);

/**
 * @brief enable deep sleep mode or leave deep sleep mode
 * @param enable    enable or leave deepsleep mode
 * @return  none
 * @note    not all flash supports this command
*/
extern bool flash_deepsleep(uint32_t enable);

/**
 * @brief flash control for enter low power mode
 * @param   none
 * @return  none
*/
extern void flash_enter_low_power_mode(void);

/**
 * @brief flash control for exit low power mode
 * @param   none
 * @return  none
*/
extern void flash_exit_low_power_mode(void);

/**
 * @brief provide lock function while access flash
 * @param flash_lock_mode flash access mode
 * @retval true succeed to get flash access semaphore
 * @retval false fail to get flash access semaphore
 * @note prevent other flash access and some interrupts; if needed, turn off RF too
*/
extern bool flash_lock(T_LOCK_TYPE flash_lock_mode);

/**
 * @brief provide unlock function while access flash
 * @param flash_lock_mode user mode or auto mode
 * @return none
 * @note prevent other flash access and some interrupts; if needed, turn off RF too
*/
extern void flash_unlock(T_LOCK_TYPE flash_lock_mode);

/**
 * @brief program data to flash via user mode
 * @param start_addr    start address where is going to be written in flash
 * @param data_len      data length to be written
 * @param data          data buffer to be written
 * @return true if success
 * @note start address should be 4 byte align
*/
extern NO_RECOMMAND_API bool __rtl_flash_write(uint32_t start_addr, uint32_t data_len, uint8_t *data);

/**
 * @brief read flash data via auto mode
 * @param addr    address to read
 * @return value in this address
*/
__STATIC_INLINE uint32_t flash_auto_read(uint32_t addr)
{
    return *(uint32_t *)addr;
}

/**
 * @brief set specific bit to specific memory address
 * @param addr specific address, should not be flash address!
 * @param bit bit position to write
 * @return none
*/
static inline void data_set_bit(uint32_t *addr, uint32_t bit)
{
    (*addr) &= ~bit;
}

/**
 * @brief check if specific memory bit has been set.
 * @param flag  value to be checked
 * @param bit bit position to check
 * @retval true if specified bit position of the flag has been set
 * @retval false otherwise
*/
static inline bool data_get_bit(uint32_t flag, uint32_t bit)
{
    return (flag & bit) ? false : true;
}

/**
 * @brief set specific flash bit to specific address
 * @param addr specific address, should not be flash address!
 * @param bit bit position to write
 * @return none
*/
extern void __rtl_flash_set_bit(uint32_t *addr, uint32_t bit);

/**
 * @brief check if specific flash bit has been set.
 * @param flag  value to be checked
 * @param bit bit position to check
 * @retval true if specified bit position of the flag has been set
 * @retval false otherwise
*/
bool __rtl_flash_get_bit(uint32_t flag, uint32_t bit);


/**
 * @brief read flash data via user mode
 * @param start_addr    start_addr address where is going to be read in flash
 * @param data_len      data length to be read
 * @param data          data buffer to be read into
 * @return true if success
 * @note SPIC only supports SPIC_FIFO_SIZE FIFO, so max SPIC_FIFO_SIZE-4 word allowed to be written.
 *       auto mode performance is better than this function, and auto mode read is suggested.
 *       recommand to use auto read other than this api
*/
extern NO_RECOMMAND_API bool flash_read(uint32_t start_addr, uint32_t data_len, uint8_t *data);

extern bool(*flash_cal_read)(uint32_t start_addr, uint32_t data_len, uint8_t *data);

/**
 * @brief read flash split (continuous )read data via user mode
 * @param start_addr    start_addr address where is going to be read in flash
 * @param data_len      data length to be read
 * @param data          data buffer to be read into
 * @return              true if success
 * @note In order to enhance throughput, only word align (4 bytes) is allowed to be written.
 *       auto mode performance is better than this function, and auto mode read is suggested.
 *       recommand to use auto read other than this api
*/
extern bool flash_split_read(uint32_t start_addr, uint32_t data_len, uint8_t *data);

/**
 * @brief init DMA for split read by DMA
 * @param dma_type      select the direction
 * @param src_addr      source address to read
 * @param dst_addr      destenation address to write
 * @param data_len      data length to be read
 * @return none
*/
extern void flash_dma_init(T_FLASH_DMA_TYPE dma_type, uint32_t src_addr, uint32_t dst_addr,
                           uint32_t data_len);

/**
 * @brief init DMA for auto read by DMA
 * @param dma_type      select the direction
 * @param flash_cb      callback function after complete
 * @param src_addr      source address to read
 * @param dst_addr      destenation address to write
 * @param data_len      data length to be read
 * @return none
*/
extern void flash_auto_dma_read(T_FLASH_DMA_TYPE dma_type, FlashCB flash_cb,
                                uint32_t src_addr, uint32_t dst_addr, uint32_t data_len);

/**
 * @start to auto sequential transition read by DMA
 * @param dma_type      select the direction
 * @param flash_cb      callback function after complete
 * @param src_addr      source address to read
 * @param dst_addr      destenation address to write
 * @param data_len      data length to be read
 * @return
*/
extern void flash_auto_seq_trans_dma_read(T_FLASH_DMA_TYPE dma_type, FlashCB flash_cb,
                                          uint32_t src_addr, uint32_t dst_addr, uint32_t data_len);

/**
 * @setup user split read by DMA + HW HSK
 * @param start_addr    start address to read
 * @param data_len      data length to be read
 * @return
 * @note also need to call flash_dma_init() before kick!!
*/
extern void flash_split_read_by_dma(FlashCB flash_cb, uint32_t start_addr, uint32_t dst_addr,
                                    uint32_t data_len);

/**
 * @brief start split read by DMA
 * @param start_addr    start address to read
 * @param data_len      data length to be read
 * @return none
 * @note also need to call @ref flash_dma_init() before kick!!
*/
extern void flash_split_read_by_dma_setup(uint32_t start_addr, uint32_t data_len);

/**
 * @brief start split read by DMA
 * @return none
 * @note need to call @ref flash_split_read_by_dma_setup()
 *  and @ref flash_dma_init() before kick!!
*/
extern void flash_split_read_by_dma_kick(void);

/**
 * @brief program bunch of data to flash via auto mode
 * @param start_addr    start_addr address where is going to be flash in flash
 * @param data          data to be program into
 * @param len           len of data buffer
 * @return none
 * @note  start address should be 4 byte align
*/
extern void flash_auto_write_buffer(uint32_t start_addr, uint32_t *data, uint32_t len);

/**
 * @brief program data to flash via auto mode
 * @param start_addr    start_addr address where is going to be flash in flash
 * @param data          data to be program into
 * @return none
 * @note
 *      1. auto write is sync with auto read. so if your use auto read, this api is preferred.
 *      2. if need sync with other, use api with lock postfix
 *      3. recommandate to use this other than __rtl_flash_write
*/
extern void __rtl_flash_auto_write(uint32_t start_addr, uint32_t data);

/**
 * @brief get rdid of external flash
 * @param none
 * @return rdid of flash
 * @note this is the first cmd, so some spic flash setting would do in this function either.
 *       the command may fail if it's prior to this function.
*/
extern uint32_t flash_get_rdid(void);


/* flash calibration operation */
extern T_FLASH_CAL_ERROR_CODE(*flash_calibration_operate)(T_FLASH_MODE bit_mode,
                                                          FLASH_CAL_MODE cal_mode);

/**
 * @brief restore spic register after resume from dlps.
 * @param none
 * @return none
 * @note spic register is non-retentionable, need to restore it back after resume
*/
extern void flash_restore_spic_register(void);

/**
 * @brief config spic for auto mode based on flash supported cmd.
 *
 * @param bit_mode bit mode to config
 * @return true if success
*/
extern bool flash_config(T_FLASH_MODE bit_mode);

/**
 * @brief configure seq_trans_en
 * @param enable         enable / disable seq_trans
 * @return none
*/
extern void (*flash_seq_trans_enable)(uint8_t enable);

/**
* @brief    get flash size info
* @return   flash size
*/
extern uint32_t flash_get_flash_size(void);

/**
 * @brief load flash query info from available source
 * @param none
 * @return true if get valid flash query info
*/
extern bool flash_load_query_info(void);

/**
* @brief    get block protect level
* @param    *bp_lv  a set of BPx ~ BP0
* @retval   1 if success
* @retval   0 if not support
* @retval  -1 if fail
*/
extern T_FLASH_RET flash_get_block_protect(uint8_t *bp_lv);

/**
* @brief    set block protect by map
* @param    bp_lv a set of BPx ~ BP0
* @retval  1 if success
* @retval  0 if not support
* @retval -1 if fail
*/
extern T_FLASH_RET flash_set_block_protect(uint8_t bp_lv);

extern bool erase_flash_unlock(T_ERASE_TYPE type, uint32_t addr);

extern bool write_flash_unlock(uint32_t start_addr, uint32_t data_len, uint8_t *data);

/**
* @brief    get Top/Bottom bit
* @param    *from_bottom  true if from bottom; false if from top (depends on return true)
* @retval   1 if success
* @retval   0 if not support
* @retval  -1 if  fail
*/
extern T_FLASH_RET flash_get_top_bottom(bool *from_bottom);

/**
* @brief    set Top/Bottom bit
* @param    from_bottom  1 bit for TB in Status Reg
* @retval   1 if success
* @retval   0 if not support
* @retval  -1 if  fail
*/
extern T_FLASH_RET flash_set_top_bottom(bool from_bottom);

/**
* @brief    only unlock prefer section by addres
* @param    unlock_addr address section to be unlocked
* @param    *old_bp_lv before unlock
* @retval   1 if success
* @retval   0 if not support
* @retval  -1 if  fail
*/
extern T_FLASH_RET flash_sw_protect_unlock_by_addr(uint32_t unlock_addr, uint8_t *old_bp_lv);

/**
* @brief    get block all level
* @return   BP all level value
* @return   0xFF if  fail
*/
extern uint8_t flash_get_block_all_lv(void);

/**
 * @brief get flash basic_cfg
 * @param **basic_cfg
 * @return address of basic_cfg
*/
extern void flash_get_basic_cfg(void **basic_cfg);

/**
 * @brief get flash adv_cfg
 * @param **adv_cfg
 * @return address of adv_cfg
*/
extern void flash_get_adv_cfg(void **adv_cfg);

/**
 * @brief get flash query_info
 * @param **query_info
 * @return address of query_info
*/
extern void flash_get_query_info(void **query_info);

/**
 * @brief get current bit mode information
 * @return bit_mode value
*/
extern uint8_t flash_get_curr_bit_mode(void);


/**
 * @brief  software reset flash
 * @param  none
 * @return true if success
*/
extern bool flash_sw_reset(void);

/**
 * @brief  specific configuration
 * @param  none
 * @return true if success
*/
extern bool flash_specific_cfg(void);

/**
 * @brief SPI Flash interrupt handler
 * @return none
 * @note currently only support RXSIS because can't get SPIC->isr
 *     value in handler.
*/
extern void SPIC0_Handler(void);

/** @} */ /* End of group FLASH_DRIVER_Exported_Functions */


/** @} */ /* End of group FLASH_DRIVER */

#endif //_FLASH_DRIVER_H

