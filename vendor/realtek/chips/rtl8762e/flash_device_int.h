/**
  ******************************************************************************
  * @file    flash_device_int.h
  * @author
  * @version V0.1.1
  * @date    2016-11-9
  * @brief   This file contains all the functions regarding flash device.
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
* - Flash function would be splitted into two modules: flash_driver and flash_device
* - flash_driver will handle flash_spic register to provide flash basic read/write function
* - flash_device would provide flash layout or other functions.
******************************************************************************************/
#ifndef _FLASH_DEVICE_INT_H
#define _FLASH_DEVICE_INT_H
#include "flash_device.h"

/*============================================================================*
  *                                   Configuration
  *============================================================================*/
#define FLASH_HIGH_SPEED_READ           1       // must be opened !! (temp disable, ROM full!)

#if (FLASH_HIGH_SPEED_READ == 1)
#define FLASH_AUTO_SEQ_TRANS            1       // this function must request RDC to support (bee2 already supported)

#define FLASH_USER_SPLIT_DMA_HW_HSK     0       // this function must request RDC to support (bee2 already supported)
#endif

#define FLASH_FULL_TEST                 0




/*============================================================================*
  *                                   Internal
  *============================================================================*/
#define BIT_PMUX_SPIM_EN_OPT BIT25


/*============================================================================*
  *                                   Macros
  *============================================================================*/

#define FLASH_SPEC_CFG_MAX    6

#define FLASH_NORMAL_READ_CLOCK_MAX     33300000

/* Flash Query Info rsvd[1] bits */
#define FLASH_QI_EXT_FLASH_BIT          0x01
#define FLASH_QI_4_BIT_MODE_BIT         0x02
#define FLASH_QI_PKG_ID_VALID_BIT       0x80

#define FLASH_WAIT_MAX                  12000000        // max value for 8MB flash

#if (FLASH_FULL_TEST == 1)
#define BT_TICK_2_SEC(t)    ((float)t*(float)312500/(float)1000000000)
#define TICK_2_SEC(t)       ((float)t*(float)50/(float)1000000000)
#define MBPS_CAL(b,s)       ((float)(b*8)/(float)s/(float)0x100000)

/* Generate measure result base on BT tick */
#define TIME_MEA_RESULT(res)     \
    res.mea_info.time_cost = BT_TICK_2_SEC((res.mea_info.end_tick - res.mea_info.start_tick));\
    res.mea_info.mbps      = MBPS_CAL(res.mea_info.data_size, res.mea_info.time_cost);\
    DBG_DIRECT("[Flash TC][Result][%s][%s] Time cost = %f (s), Bit rate = %f (Mbps)", ((res.test_pass==1)?("Pass"):("Fail")),res.tc_name, res.mea_info.time_cost, res.mea_info.mbps)


/* Generate measure result base on clock tick */
#define TIME_MEA_RESULT2(res)     \
    res.mea_info.time_cost = TICK_2_SEC((res.mea_info.end_tick - res.mea_info.start_tick));\
    res.mea_info.mbps      = MBPS_CAL(res.mea_info.data_size, res.mea_info.time_cost);\
    DBG_DIRECT("[Flash TC][Result][%s][%s] Time cost = %f (s), Bit rate = %f (Mbps)", ((res.test_pass==1)?("Pass"):("Fail")),res.tc_name, res.mea_info.time_cost, res.mea_info.mbps)

#endif

/*** flash ioctrl related. ***/
#define FLASH_IOCTRL_ERASE_MAGIC_PAR1 0x12345678
#define FLASH_IOCTRL_ERASE_MAGIC_PAR2 0x5a5a1234
typedef enum _IOCTL_CODE
{
    flash_ioctl_mass_erase                      = 0x0,

    flash_ioctl_get_page_size,
    flash_ioctl_get_start_addr_main,
    flash_ioctl_get_size_main,
    flash_ioctl_get_flash_otp_offset,
    flash_ioctl_get_curr_bit_mode,
    flash_ioctl_get_rdid,
    flash_ioctl_get_wait_busy_ctr,


    flash_ioctl_set_base                        = 0x1000,
    flash_ioctl_set_otp_cfg,
    flash_ioctl_set_sw_protect,
    flash_ioctl_set_tb_bit,
    flash_ioctl_set_deep_power_down,
    flash_ioctl_set_sw_protect_unlock_by_addr,
    flash_ioctl_set_baudrate_clk_div,
    flash_ioctl_set_log_bitmap,
    flash_ioctl_set_rsvd0,      //seq_trans_enable
    flash_ioctl_set_pa33_delay,


    flash_ioctl_dump_base                       = 0x2000,
    flash_ioctl_dump_cfg,
    flash_ioctl_dump_sw_protect_info,
    flash_ioctl_dump_top_bottom_info,


    flash_ioctl_exec_base                       = 0x3000,
    flash_ioctl_exec_load_cfg,
    flash_ioctl_exec_bp_dp_test,                        // execute BlockProtect & DeepPowerdown test
    flash_ioctl_exec_dp_test,
    flash_ioctl_exec_bit_mode_switch,
    flash_ioctl_exec_flash_sw_reset,                    // 0x3005
    flash_ioctl_exec_ft_test,
    flash_ioctl_exec_ext_flash_ft_test,
    flash_ioctl_exec_high_speed_read_test,
    flash_ioctl_exec_wdg_reset,
    flash_ioctl_exec_flash_clock_switch_test,       // Bee2 no PLL, do not support clock switch
    flash_ioctl_exec_flash_init,                        // 0x300B
    flash_ioctl_exec_running_test,
    flash_ioctl_exec_flash_erase,
    flash_ioctl_exec_if_switch,
    flash_ioctl_exec_eqc_test,

    //Compatible for Bee2 B-cut
    flash_ioctl_app_base                       = 0x5000,

} IOCTL_CODE;

/** ioctrl results **/
typedef enum _IOCTL_RESULT
{
    RESULT_FAILURE = 0,
    RESULT_SUCCESS = 1
} IOCTL_RESULT;
/*** flash ioctrl end. ***/

typedef enum
{
    FLASH_CFG_BASIC = 1,                // can't be configured by vendor config
    FLASH_CFG_ADV,
    FLASH_QUERY_INFO,
} FLASH_CFG_TYPE;



typedef struct
{
    uint32_t signature;
    uint16_t length;
} __attribute__((packed)) T_CONFIG_HEADER;

#define FLASH_TABLE_MAGIC_PATTERN (0x5a5a12a5)

typedef struct
{
    uint32_t  Version;

    // flash info
    uint32_t  Info;
    uint32_t  PageSize;
    uint32_t  Main_Addr;
    //uint32_t  Main_Size;

    // flash layout table
    uint32_t  ota_bank0_addr;
    uint32_t  ota_bank0_size;
    uint32_t  ota_bank1_addr;
    uint32_t  ota_bank1_size;
    uint32_t  ftl_addr;
    uint32_t  ftl_size;

    uint32_t  ota_tmp_addr;
    uint32_t  ota_tmp_size;

    uint32_t  bkp_data1_addr;
    uint32_t  bkp_data1_size;
    uint32_t  bkp_data2_addr;
    uint32_t  bkp_data2_size;

    // only patch_addr is fixed
    // other fields are changable
    // app/ota/ftl/dsp dont need to be sequencial
    // just beware of the size of them have no overlap condition

    uint32_t  HardFault_Record_CFG;
    // default value: 0x7
    // [0]: 1, enable saving HardFault_Record
    // [1]: reserved//1, 1 bit slow mode
    // [2]: 1, dump HardFaultRecord in boot time
    // [3]: 1, Clean up HardFaultRecord after dumping HardFault_Record
    // [4]: 1, Clean up HardFaultRecord before saving HardFault_Record
    uint32_t  HardFault_Record_BegAddr;
    uint32_t  HardFault_Record_EndAddr;
    uint8_t   toggle_cs_byte[3];

} T_FLASH_INFO_TBL;

/* query_info */
typedef struct
{
    // Top/Bottom bit offset of status reg. (some chip may use BPx as TB)
    // set "0xFF" means BP only support lock / unlock all (no level)
    // set "0x7F" means BP support lock / unlock all, half, none
    uint8_t         tb_offset;
    uint8_t         manu_id;
    uint16_t        device_id;      //  memory_type + memory_density
    uint32_t        flash_size;
    uint8_t         bp_all_lv;      // Block Protect All Level (depends on flash_size)
    uint8_t         bp_mask;        // Block Protect (BPx~BP0); BP2~BP0=0x07, BP3~BP0=0x0F
    uint8_t         pkg_id;
    /* use pkg_info bit 0 for IS_EXT_flash flag */
    /* use pkg_info bit 1 for support 4 bit-mode flag */
    /* use pkg_info bit 7 to notify package ID is valid */
    uint8_t         pkg_info;
} T_FLASH_QUERY_TBL;

/* basic command set */
typedef struct
{
    /* force use command set from OTP */
    //uint8_t cfg_from_otp;           /*!< Specifies the Flash configuration source ( 0: QVL; 1: OTP) */

    uint8_t cmd_rd_data;            /*!< Specifies single data read cmd */
    uint8_t cmd_rd_dual_o;          /*!< Specifies dual data read cmd */
    uint8_t cmd_rd_dual_io;         /*!< Specifies dual data/addr read cmd */
    uint8_t cmd_pp;                 /*!<Specifies single page program cmd*/
    uint8_t cmd_wr_en;              /*!< Specifies the Write Enable(WREN) instruction*/
    uint8_t cmd_rd_id;              /*!< Specifies the Read ID instruction*/
    uint8_t cmd_rd_status;          /*!< Specifies the Read Status Register*/
    uint8_t cmd_wr_status;          /*!< Specifies the Write Status Register*/
    uint8_t cmd_chip_e;             /*!< Specifies the Erase Chip instruction which is for erasing whole chip*/
    uint8_t cmd_block_e;            /*!< Specifies the Erase Block instruction which is for erasing 64kB*/
    uint8_t cmd_sector_e;           /*!< Specifies the Erase Sector instruction which is for erasing 4kB*/
    uint8_t cmd_pwdn_release;       /*!< Specifies the Release from Deep Power Down instruction*/
    uint8_t cmd_pwdn;               /*!< Specifies the Deep Power Down instruction*/
    uint8_t cmd_en_reset;           /*!< Specifies the Enable Reset instruction*/
    uint8_t cmd_reset;              /*!< Specifies the Reset instruction*/

    uint8_t manu_id;
    uint16_t device_id;
} T_FLASH_BASIC_CFG;

/* advanced command set */
typedef struct
{
    uint8_t flash_qe_bit;           /*!< Specifies the QE bit in status register which is used to
                                            enable Quad I/O mode . */
    uint8_t cmd_rd_quad_o;          /*!< Specifies quad data read cmd */
    uint8_t cmd_rd_quad_io;         /*!< Specifies quad data/addr read cmd */
    uint8_t cmd_ppx4;               /*!< Specifies quad page program cmd */
    uint8_t cmd_wr_en_vol;          /*!< Specifies the Write Enable for Volatile Status Register instruction*/
    uint8_t cmd_rd_status2;         /*!< Specifies the Read Status Register2*/
    uint8_t cmd_wr_status2;         /*!< Specifies the Write Status Register2*/
    uint8_t cmd_rdcr;               /*!< Specifies read config command, it's used on MXIC for high power mode*/
    uint8_t cmd_hpm;                /*!< Specifies the high performance command.*/

    uint8_t manu_id;
    uint16_t device_id;
} T_FLASH_ADV_CFG;

typedef struct
{
    uint8_t in_use              : 1;
    uint8_t spec_addr_len       : 2;
    uint8_t spec_byte_map       : 4;        // used to identify which spec_byte is needed
    uint8_t rsvd                : 1;
    uint16_t front_delay_10us;              // delay max 655360 us
    uint16_t rear_delay_10us;               // delay max 655360 us
    uint8_t  spec_byte[4];
} __attribute__((packed)) T_FLASH_SPEC_CFG;

/**
  * @brief  FLASH Init structure definition
  */
typedef struct
{
    //uint32_t flash_id;          /*!< Specifies the flash vendor ID.*/
    uint32_t valid_cmd;         /*!< to store valid command set for this flash*/
    uint16_t baud_rate;         /*!< Specifies the spi_sclk divider value.*/
    uint8_t current_read_cmd;   /*!< Specifies the current read cmd which is used to read data from
                                flash in current bitmode. */


    struct
    {
        uint8_t flash_sr2_exists: 1; /*!< Specifies whether flash chip has Status Register2 or not*/

        uint8_t flash_hpm_exists: 1; /*!< Specifies whether high power mode exists*/
        uint8_t flash_hpm_cmd_exists: 1; /*! Specifies whether using hpm command or wrst to set hpm*/
        uint8_t read_addr_ch: 2;  /*! Specifies flash address channel*/
        uint8_t read_data_ch: 2; /*! Specifies flash data channel*/
        uint8_t write_pp4_mode: 1;  /*! specifies use pp or 4pp to write. 1 if 4pp enable.*/
    };

    /* calibration data */
    uint8_t flash_rd_dummy_cycle; /*!< Specifies the read dummy cycle of multi chann bitmode */
    uint8_t flash_rd_st_dummy_cycle;

    T_FLASH_BASIC_CFG   basic_cfg;
    T_FLASH_ADV_CFG     adv_cfg;
    T_FLASH_QUERY_TBL   query_info;
} T_FLASH_DEVICE_INFO;


extern T_FLASH_DEVICE_INFO __rtl_flash_device_info;

/*************************************************************************************************
* external functions
*************************************************************************************************/
/**
 * @brief init flash device to auto mode for further flash program
 * @param
 * @return
*/
extern void flash_device_init(void);

/**
 * @brief resume from dlps, re_init flash and cache
 *
 * @param
 * @return
*/
extern void flash_resume_from_dlps(void);

/**
 * @brief set flash_exists status
 * @param exist     set flash_exists outside
 * @return
 * @notes
*/
extern void flash_set_flash_exist(T_FLASH_EXIST_LV exist);

/**
 * @brief get flash default block protect level depend on different flash id and different flash layout
 * @param none
 * @return default set bp level
*/
extern uint8_t flash_get_default_bp_lv(void);

/**
 * @brief check flash bp level before ota copy, if flash is locked, must unlock bp in NVRAM
 * @param none
 * @return none
*/
extern void check_flash_bp_before_ota_copy(void);

/**
 * @brief unlock total flash bp
 * @param pre_bp_lv get flash bp level before unlock
 * @return
*/
bool flash_unlock_bp_all(uint8_t *pre_bp_lv);

/**
 * @brief try changing to high speed mode after patch is ready
 * @param none
 * @retval 1 if switch to new bit mode success
 * @retval 0 if not support selected speed
 * @retval -1 if switch to new bit mode fail
 * @note only call this function in patch to support different flash vendor.
 */
extern T_FLASH_RET(*flash_try_high_speed_mode)(void);

#endif //_FLASH_DEVICE_ROM_H
