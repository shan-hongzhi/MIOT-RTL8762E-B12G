/**
*********************************************************************************************************
*               Copyright(c) 2015, Realtek Semiconductor Corporation. All rights reserved.
**********************************************************************************************************
* @file     main.c
* @brief    This is the entry of user code which the main function resides in.
* @details
* @author
* @date
* @version
*********************************************************************************************************
*/
#include <stdlib.h>
#include "gap.h"
#include "gap_adv.h"
#include "gap_msg.h"
#include "gap_bond_le.h"
#include "gap_conn_le.h"
#include "profile_server.h"
#include "bas.h"
#include "dis.h"
#include "dfu_service.h"
#include "ota_service.h"
#include "rtl876x_pinmux.h"
#include "rtl876x_io_dlps.h"
#include "rtl876x_rcc.h"
#include "rtl876x_nvic.h"
#include "rtl876x_gpio.h"
#include "dlps.h"
#include "os_sched.h"
#include "trace.h"
#include "patch_header_check.h"
#include "flash_device.h"
#include "app_task.h"
#include "ota_application.h"
#include "board.h"

/*============================================================================*
 *                              Macros
 *============================================================================*/
/* What is the advertising interval when device is discoverable (units of 625us, 160=100ms) */
#define DEFAULT_ADVERTISING_INTERVAL_MIN            320 /* 200ms */
#define DEFAULT_ADVERTISING_INTERVAL_MAX            320 /* 200ms */

/*============================================================================*
 *                              External Variables
 *============================================================================*/
uint8_t g_ota_mode;
bool allowed_ota_enter_dlps = true;

T_SERVER_ID g_bas_service_id;
T_SERVER_ID g_dis_service_id;
T_SERVER_ID g_ota_service_id;

/*============================================================================*
 *                              Local Variables
 *============================================================================*/
// GAP - SCAN RSP data (max size = 31 bytes)
static uint8_t SCAN_RSP_DATA[] =
{
    0x03,
    GAP_ADTYPE_APPEARANCE,
    LO_WORD(GAP_GATT_APPEARANCE_UNKNOWN),
    HI_WORD(GAP_GATT_APPEARANCE_UNKNOWN),
};

// GAP - Advertisement data (max size = 31 bytes, though this is
static uint8_t ADV_DATA[] =
{
    0x02,
    GAP_ADTYPE_FLAGS,
    GAP_ADTYPE_FLAGS_LIMITED | GAP_ADTYPE_FLAGS_BREDR_NOT_SUPPORTED,

    0x03,
    GAP_ADTYPE_APPEARANCE,
    LO_WORD(GAP_GATT_APPEARANCE_UNKNOWN),
    HI_WORD(GAP_GATT_APPEARANCE_UNKNOWN),

    0x08,
    GAP_ADTYPE_LOCAL_NAME_COMPLETE,
    'B', 'L', 'E', '_', 'O', 'T', 'A'
};

/*============================================================================*
 *                              Functions
 *============================================================================*/
/******************************************************************
 * @fn          Initial gap parameters
 * @brief      Initialize peripheral and gap bond manager related parameters
 *
 * @return     void
 */
void app_le_gap_init(void)
{
    //device name and device appearance
    uint8_t  device_name[GAP_DEVICE_NAME_LEN] = "BLE_OTA";
    uint16_t appearance = GAP_GATT_APPEARANCE_KEYBOARD;

    //advertising parameters
    uint8_t  adv_evt_type = GAP_ADTYPE_ADV_IND;
    uint8_t  adv_direct_type = GAP_REMOTE_ADDR_LE_PUBLIC;
    uint8_t  adv_direct_addr[GAP_BD_ADDR_LEN] = {0};
    uint8_t  adv_chann_map = GAP_ADVCHAN_ALL;
    uint8_t  adv_filter_policy = GAP_ADV_FILTER_ANY;
    uint16_t adv_int_min = DEFAULT_ADVERTISING_INTERVAL_MIN;
    uint16_t adv_int_max = DEFAULT_ADVERTISING_INTERVAL_MIN;

    //GAP Bond Manager parameters
    uint8_t  auth_pair_mode = GAP_PAIRING_MODE_PAIRABLE;
    uint16_t auth_flags = GAP_AUTHEN_BIT_BONDING_FLAG | GAP_AUTHEN_BIT_MITM_FLAG;
    uint8_t  auth_io_cap = GAP_IO_CAP_NO_INPUT_NO_OUTPUT;
    uint8_t  auth_oob = false;
    uint8_t  auth_use_fix_passkey = false;
    uint32_t auth_fix_passkey = 0;
    uint8_t  auth_sec_req_enalbe = false;
    uint16_t auth_sec_req_flags = GAP_AUTHEN_BIT_NONE;

    //Register gap callback
    le_register_app_cb(app_gap_callback);

    //Set device name and device appearance
    le_set_gap_param(GAP_PARAM_DEVICE_NAME, GAP_DEVICE_NAME_LEN, device_name);
    le_set_gap_param(GAP_PARAM_APPEARANCE, sizeof(appearance), &appearance);

    //Set advertising parameters
    le_adv_set_param(GAP_PARAM_ADV_EVENT_TYPE, sizeof(adv_evt_type), &adv_evt_type);
    le_adv_set_param(GAP_PARAM_ADV_DIRECT_ADDR_TYPE, sizeof(adv_direct_type), &adv_direct_type);
    le_adv_set_param(GAP_PARAM_ADV_DIRECT_ADDR, sizeof(adv_direct_addr), adv_direct_addr);
    le_adv_set_param(GAP_PARAM_ADV_CHANNEL_MAP, sizeof(adv_chann_map), &adv_chann_map);
    le_adv_set_param(GAP_PARAM_ADV_FILTER_POLICY, sizeof(adv_filter_policy), &adv_filter_policy);
    le_adv_set_param(GAP_PARAM_ADV_INTERVAL_MIN, sizeof(adv_int_min), &adv_int_min);
    le_adv_set_param(GAP_PARAM_ADV_INTERVAL_MAX, sizeof(adv_int_max), &adv_int_max);
    le_adv_set_param(GAP_PARAM_ADV_DATA, sizeof(ADV_DATA), ADV_DATA);
    le_adv_set_param(GAP_PARAM_SCAN_RSP_DATA, sizeof(SCAN_RSP_DATA), SCAN_RSP_DATA);

    // Setup the GAP Bond Manager
    gap_set_param(GAP_PARAM_BOND_PAIRING_MODE, sizeof(auth_pair_mode), &auth_pair_mode);
    gap_set_param(GAP_PARAM_BOND_AUTHEN_REQUIREMENTS_FLAGS, sizeof(auth_flags), &auth_flags);
    gap_set_param(GAP_PARAM_BOND_IO_CAPABILITIES, sizeof(auth_io_cap), &auth_io_cap);
    gap_set_param(GAP_PARAM_BOND_OOB_ENABLED, sizeof(auth_oob), &auth_oob);
    le_bond_set_param(GAP_PARAM_BOND_FIXED_PASSKEY, sizeof(auth_fix_passkey), &auth_fix_passkey);
    le_bond_set_param(GAP_PARAM_BOND_FIXED_PASSKEY_ENABLE, sizeof(auth_use_fix_passkey),
                      &auth_use_fix_passkey);
    le_bond_set_param(GAP_PARAM_BOND_SEC_REQ_ENABLE, sizeof(auth_sec_req_enalbe), &auth_sec_req_enalbe);
    le_bond_set_param(GAP_PARAM_BOND_SEC_REQ_REQUIREMENT, sizeof(auth_sec_req_flags),
                      &auth_sec_req_flags);

}

/******************************************************************
 * @fn          Initial profile
 * @brief      Add simple profile service and register callbacks
 *
 * @return     void
 */
void app_le_profile_init(void)
{
    server_init(3);
    g_bas_service_id = bas_add_service(app_profile_callback);
    g_dis_service_id = dis_add_service(app_profile_callback);
    g_ota_service_id = ota_add_service(app_profile_callback);
    server_register_app_cb(app_profile_callback);
}
/**
* @brief    Board_Init() contains the initialization of pinmux settings and pad settings.
*
*               All the pinmux settings and pad settings shall be initiated in this function.
*               But if legacy driver is used, the initialization of pinmux setting and pad setting
*               should be peformed with the IO initializing.
*
* @return  void
*/
void board_init(void)
{
    Pinmux_Config(TP0, DWGPIO);
    Pad_Config(TP0, PAD_PINMUX_MODE, PAD_IS_PWRON, PAD_PULL_DOWN, PAD_OUT_DISABLE, PAD_OUT_LOW);
}

/**
* @brief    Driver_Init() contains the initialization of peripherals.
*
*               Both new architecture driver and legacy driver initialization method can be used.
*
* @return  void
*/
void driver_init(void)
{
    RCC_PeriphClockCmd(APBPeriph_GPIO, APBPeriph_GPIO_CLOCK, ENABLE);
    GPIO_InitTypeDef gpio_struct;
    GPIO_StructInit(&gpio_struct);
    gpio_struct.GPIO_Pin = GPIO_GetPin(TP0);
    gpio_struct.GPIO_Mode = GPIO_Mode_IN;
    GPIO_Init(&gpio_struct);
    g_ota_mode = !GPIO_ReadInputDataBit(GPIO_GetPin(TP0));
}

#if DLPS_EN
/**
 * @brief this function will be called before enter DLPS
 *
 *  set PAD and wakeup pin config for enterring DLPS
 *
 * @param none
 * @return none
 * @retval void
*/
void ota_enter_dlps_set(void)
{

}

/**
 * @brief this function will be called after exit DLPS
 *
 *  set PAD and wakeup pin config for enterring DLPS
 *
 * @param none
 * @return none
 * @retval void
*/
void ota_exit_dlps_init(void)
{

}

bool dlps_ota_check(void)
{
    return allowed_ota_enter_dlps;
}
#endif

/**
* @brief    PwrMgr_Init() contains the setting about power mode.
*
* @return  void
*/
void pwr_mgr_init(void)
{
#if DLPS_EN
    if (false == dlps_check_cb_reg(dlps_ota_check))
    {
        DBG_DIRECT("Error: dlps_check_cb_reg(dlps_ota_check) failed!\n");
    }
    DLPS_IORegUserDlpsEnterCb(ota_enter_dlps_set);
    DLPS_IORegUserDlpsExitCb(ota_exit_dlps_init);
    DLPS_IORegister();
    lps_mode_set(LPM_DLPS_MODE);
#endif
}


/**
* @brief  Task_Init() contains the initialization of all the tasks.
*
*           There are four tasks are initiated.
*           Lowerstack task and upperstack task are used by bluetooth stack.
*           Application task is task which user application code resides in.
*           Emergency task is reserved.
*
* @return  void
*/
void task_init(void)
{
    app_task_init();
}

/**
* @brief  print all images version to check ota whether success.
* @return  void
*/
void print_all_images_version(void)
{
    T_IMG_ID image_id;
    T_IMAGE_VERSION image_version;

    image_id = OTA;
    if (get_active_bank_image_version(image_id, &image_version))
    {
        DBG_DIRECT("image:0x%x,version =0x%x, sub_version:%d.%d.%d.%d", image_id,
                   image_version.ver_info.version,
                   image_version.ver_info.header_sub_version._version_major,
                   image_version.ver_info.header_sub_version._version_minor,
                   image_version.ver_info.header_sub_version._version_revision,
                   image_version.ver_info.header_sub_version._version_reserve);
    }
    else
    {
        DBG_DIRECT("image:0x%x get_active_bank_image_version fail!!!", image_id);
    }

    for (image_id = SecureBoot; image_id < IMAGE_MAX; image_id++)
    {
        if (get_active_bank_image_version(image_id, &image_version))
        {
            DBG_DIRECT("image:0x%x,version =0x%x, sub_version:%d.%d.%d.%d", image_id,
                       image_version.ver_info.version,
                       image_version.ver_info.img_sub_version._version_major,
                       image_version.ver_info.img_sub_version._version_minor,
                       image_version.ver_info.img_sub_version._version_revision,
                       image_version.ver_info.img_sub_version._version_reserve);
        }
        else
        {
            DBG_DIRECT("image:0x%x get_active_bank_image_version fail!!!", image_id);
        }
    }

    //Read back user data version
    T_IMG_CTRL_HEADER_FORMAT user_ctrl_header;
    flash_read_locked(flash_get_bank_addr(FLASH_BKP_DATA1), sizeof(T_IMG_CTRL_HEADER_FORMAT),
                      (uint8_t *)&user_ctrl_header);
    if (user_ctrl_header.image_id == IMAGE_USER_DATA &&
        !user_ctrl_header.ctrl_flag.flag_value.not_ready)
    {
        T_IMAGE_VERSION usr_data_version;
        flash_read_locked(flash_get_bank_addr(FLASH_BKP_DATA1) + 0x60, sizeof(T_IMAGE_VERSION),
                          (uint8_t *)&usr_data_version);

        DBG_DIRECT("image:0x%x,version =0x%x, sub_version:%d.%d.%d.%d", user_ctrl_header.image_id,
                   usr_data_version.ver_info.version,
                   usr_data_version.ver_info.img_sub_version._version_major,
                   usr_data_version.ver_info.img_sub_version._version_minor,
                   usr_data_version.ver_info.img_sub_version._version_revision,
                   usr_data_version.ver_info.img_sub_version._version_reserve);
    }
    else
    {
        DBG_DIRECT("user data is not exist! image:0x%x, not_ready=%d", user_ctrl_header.image_id,
                   user_ctrl_header.ctrl_flag.flag_value.not_ready);
    }

}

int main(void)
{
    DBG_DIRECT("Normal OTA Demo main");
    print_all_images_version();

    board_init();
    driver_init();
    le_gap_init(1);
    gap_lib_init();
    app_le_gap_init();
    app_le_profile_init();
    pwr_mgr_init();
    task_init();

    os_sched_start();

    return 0;
}
