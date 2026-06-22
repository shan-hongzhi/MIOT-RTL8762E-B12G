/**
*********************************************************************************************************
*               Copyright(c) 2022, Realtek Semiconductor Corporation. All rights reserved.
*********************************************************************************************************
* @file      peripheral_dfu.c
* @brief     peripheral dfu implementation
* @details   peripheral dfu implementation
* @author    mandy
* @date      2022-01-05
* @version   v1.0
* *********************************************************************************************************
*/
#include <trace.h>
#include <string.h>
#include <gap_msg.h>
#include <board.h>
#include <app_msg.h>
#include "os_timer.h"
#include "peripheral_dfu_application.h"
#include "rtl876x_wdg.h"
#include "rtl876x_uart.h"
#include "trace.h"
#include "gap_msg.h"
#include "flash_device.h"
#include "peripheral_dfu.h"
#include "patch_header_check.h"
#include "board.h"
#include "flash_adv_cfg.h"
#include "os_sched.h"
#include "mem_config.h"
#include "otp.h"
#include "rtl876x_hw_aes.h"
#include "patch_header_check.h"
#include "dfu_flash.h"
#include "os_sched.h"
#include "app_section.h"
#include "os_sync.h"
#include "platform_utils.h"
#include "hw_aes.h"
#include "peripheral_handle.h"

/*============================================================================*
 *                              Local Variables
 *============================================================================*/
static uint8_t TempBufferHead[DFU_TEMP_BUFFER_SIZE];
static uint16_t ota_tmp_buf_used_size = 0;
static T_PERIP_DFU_PARA dfuPara;
static uint8_t temp_image_num = 0;
static T_PERIP_DFU_TEMP_IMAGE_INFO temp_image_info[IMAGE_MAX - SecureBoot];
static T_PERIPHERAL_PACKET_DEF uart_packet;
T_PERIP_DFU_ACTIVE_BANK_IMG_INFO ota_active_bank_info;
/*============================================================================*
 *                              Global Variables
 *============================================================================*/

/*============================================================================*
 *                              Functions Declaration
 *============================================================================*/

/*============================================================================*
 *                              Local Functions
 *============================================================================*/
/******************************************************************
 * @brief    peripheral dfu crc16 check
 * @param    buf - point to data buf.
 * @param    length - data length.
 * @param    value - initial value.
 * @return   uint16_t
 * @retval   crc16 result
 */
uint16_t peripheral_dfu_crc16_check(uint8_t *buf, uint16_t len, uint16_t value)
{
    uint16_t b = 0xA001;
    bool reminder = 0;

    for (uint16_t i = 0; i < len; i++)
    {
        value ^= buf[i];
        for (uint8_t j = 0; j < 8; j++)
        {
            reminder = value % 2;
            value >>= 1;
            if (reminder == 1)
            {
                value ^= b;
            }
        }
    }

    return value;
}

/******************************************************************
 * @brief    peripheral dfu send response
 * @param    opcode - point to data buf.
 * @param    len - the length of response payload.
 * @param    payload - point to response payload.
 * @return   none
 * @retval   void
 */
void peripheral_dfu_send_rsp(uint8_t opcode, uint8_t error_code, uint16_t len, uint8_t *payload)
{
    uint8_t rsp_event[PERIP_DFU_MAX_EVENT_PACKET_LEN] = {};

    rsp_event[0] = PERIP_DFU_OPCODE_EVENT;
    rsp_event[1] = opcode;
    rsp_event[2] = PERIP_DFU_OPCODE_EVENT_HEADER;
    rsp_event[3] = error_code;
    rsp_event[4] = len & 0xff;
    rsp_event[5] = len >> 8;
    if (len)
    {
        memcpy(rsp_event + 6, (uint8_t *)payload, len);
    }
    uint16_t crc_value = peripheral_dfu_crc16_check(rsp_event, len + 6, 0xFFFF);
    rsp_event[len + 6] = crc_value & 0xff;
    rsp_event[len + 7] = crc_value >> 8;

    peripheral_handle_send_data(rsp_event, len + 8);
}

/******************************************************************
 * @brief    peripheral dfu handle valid fw
 * @param    none
 * @return   none
 * @retval   void
 */
void peripheral_dfu_handle_valid_fw(void)
{
    bool check_result = false;
    bool is_enable_bank_switch = is_ota_support_bank_switch();
    uint8_t status = PERIP_DFU_PAYLOAD_SUCCESS;

    if (is_enable_bank_switch)
    {
        check_result = dfu_check_checksum(dfuPara.ctrl_header.image_id, 0);
    }
    else
    {
        check_result = dfu_check_checksum(temp_image_info[temp_image_num].image_id,
                                          temp_image_info[temp_image_num].image_offset);
    }
    APP_PRINT_INFO1("peripheral_dfu_handle_valid_fw: check_result=%d (1: Success, 0: Fail)",
                    check_result);

    if (check_result)
    {
        if (!is_enable_bank_switch)
        {
            temp_image_num ++;
            APP_PRINT_INFO1("DFU_OPCODE_VALID_FW: temp_image_num=%d ", temp_image_num);
        }
        status = PERIP_DFU_PAYLOAD_SUCCESS;
    }
    else
    {
        status = PERIP_DFU_PAYLOAD_CRC_CHECK_ERROR;
        peripheral_dfu_deinit(PERIP_DFU_STOP_FALG);
    }

    peripheral_dfu_send_rsp(PERIP_DFU_OPCODE_VALID_IMAGE, PERIP_DFU_FRAME_NO_ERROR,
                            PERIP_DFU_PAYLOAD_LEN_VALID_IMAGE, &status);
}

/******************************************************************
 * @brief    peripheral dfu handle active image
 * @param    none
 * @return   none
 * @retval   void
 */
void peripheral_dfu_handle_active_image(void)
{
    if (!is_ota_support_bank_switch())
    {
        uint32_t base_addr = 0;
        T_IMG_CTRL_HEADER_FORMAT *p_header = NULL;
        if (IMAGE_USER_DATA == dfuPara.ctrl_header.image_id)
        {
            /* because flash_get_bank_addr(FLASH_BKP_DATA1) is not located flash block protect range, needn't unlock bp */
            base_addr = flash_get_bank_addr(FLASH_BKP_DATA1) | FLASH_OFFSET_TO_NO_CACHE;
        }
        else
        {
            /* check OTA temp or running bank to see if received image is OK.*/
            base_addr = get_temp_ota_bank_addr_by_img_id((T_IMG_ID)dfuPara.ctrl_header.image_id);
        }

        for (uint8_t i = 0; i < temp_image_num; i++)
        {
            p_header = (T_IMG_CTRL_HEADER_FORMAT *)(base_addr + temp_image_info[i].image_offset);
            dfu_set_image_ready(p_header);
        }
    }
}

/******************************************************************
 * @brief    peripheral dfu get OTA active bank image info
 * @param    T_PERIP_DFU_ACTIVE_BANK_IMG_INFO
 * @return   none
 * @retval   void
 * image_exist
 * bit0: ota header, bit: fsbl, bit2: patch ..., bit9: appdata6, bit10: upperstack
 * img_indicator
 * Indications for each image version.each indication use 2bit.
 * 00: image is not existed.
 * 01: image is existed in bank0,ota should update image for bank1.
 * 10: image is existed in bank1,ota should update image for bank0.
 * 11: image is standalone. ota should update image for standalone.
 *
 * bit[1:0]: Image 0
 * bit[2N+1:2N]:Image N

 * Image indicator for bee3 is as below:
 * Image 0   OTA Header File
 * Image 1   Secure Boot Loader Image
 * Image 2   ROM Patch Image
 * Image 3   APP Image
 * Image 4   APP Data1 File
 * Image 5   APP Data2 File
 * Image 6   APP Data3 File
 * Image 7   APP Data4 File
 * Image 8   APP Data5 File
 * Image 9   APP Data6 File
 * Image 10  Upperstack image
 */
static void peripheral_dfu_get_ota_active_bank_image_info(T_PERIP_DFU_ACTIVE_BANK_IMG_INFO
                                                          *p_ota_image_info)
{
    uint32_t image_location = IMAGE_NOEXIST;
    bool enable_bank_switch = is_ota_support_bank_switch();
    p_ota_image_info->image_num = 0;

    if (enable_bank_switch)
    {
        uint32_t ota_bank0_addr = flash_get_bank_addr(FLASH_OTA_BANK_0);
        if (ota_bank0_addr == get_active_ota_bank_addr())
        {
            image_location = IMAGE_LOCATION_BANK0;
        }
        else
        {
            image_location = IMAGE_LOCATION_BANK1;
        }
    }
    else
    {
        image_location = IMAGE_FIX_BANK_EXIST;
    }

    for (T_IMG_ID image_id = OTA; image_id < IMAGE_MAX; image_id++)
    {
        if (0 != get_active_bank_image_size_by_img_id(image_id))
        {
            p_ota_image_info->image_num++;
            p_ota_image_info->image_exist |= BIT(image_id - OTA);
            p_ota_image_info->image_indicator |= image_location << ((OTA_HEADER + image_id - OTA) * 2);
        }
    }
}

/******************************************************************
 * @brief    peripheral dfu read device information
 * @param    none
 * @return   none
 * @retval   void
 */
void peripheral_dfu_handle_read_device_info(void)
{
    T_PERIP_DFU_DEVICE_INFO DeviceInfo;

    DeviceInfo.ic_type = DEFINED_IC_TYPE;
    DeviceInfo.max_buffer_size = DFU_TEMP_BUFFER_SIZE;
    DeviceInfo.ota_mode.value = 0;

    /* prepare img_indicator, if modify here need to sync with vaule
        of BLE_SERVICE_CHAR_IMAGE_VERSION_INDEX */
    if (is_ota_support_bank_switch())
    {
        //dual bank must support multiple image update
        DeviceInfo.ota_mode.mode_flag.multi_img = 1;
        uint32_t ota_bank0_addr = flash_get_bank_addr(FLASH_OTA_BANK_0);
        if (ota_bank0_addr == get_active_ota_bank_addr())
        {
            /* meaningful only when multi_img = 1 */
            DeviceInfo.temp_bank_size = flash_get_bank_size(FLASH_OTA_BANK_1) / FMC_SEC_SECTION_LEN;
        }
        else
        {
            /* meaningful only when multi_img = 1 */
            DeviceInfo.temp_bank_size = flash_get_bank_size(FLASH_OTA_BANK_0) / FMC_SEC_SECTION_LEN;
        }
    }
    else
    {
#if (SUPPORT_TEMP_COMBINED_OTA == 1)
        DeviceInfo.ota_mode.mode_flag.multi_img = 1;
#else
        DeviceInfo.ota_mode.mode_flag.multi_img = 0;
#endif
        /* meaningful only when multi_img = 1 */
        DeviceInfo.temp_bank_size = flash_get_bank_size(FLASH_OTA_TMP) / FMC_SEC_SECTION_LEN;
    }
    /*read flash map info to set image indicator*/
    peripheral_dfu_get_ota_active_bank_image_info(&ota_active_bank_info);
    DeviceInfo.img_indicator = ota_active_bank_info.image_indicator;

    DFU_PRINT_INFO3("<==ota_attr_read_cb: copy_img=0x%x, multi_img=0x%x, img_indicator=0x%x",
                    DeviceInfo.ota_mode.mode_flag.copy_img, DeviceInfo.ota_mode.mode_flag.multi_img,
                    DeviceInfo.img_indicator);

    peripheral_dfu_send_rsp(PERIP_DFU_OPCODE_READ_DEVICE_INFO, PERIP_DFU_FRAME_NO_ERROR,
                            sizeof(T_PERIP_DFU_DEVICE_INFO), (uint8_t *)&DeviceInfo);
}

/******************************************************************
 * @brief    peripheral dfu read version information
 * @param    none
 * @return   none
 * @retval   void
 */
void peripheral_dfu_handle_read_version_info(void)
{
    uint8_t index = 0;
    T_IMAGE_VERSION image_ver;
    uint32_t image_version[IMAGE_MAX - OTA] = {0};

    if (0 == ota_active_bank_info.image_num)
    {
        peripheral_dfu_get_ota_active_bank_image_info(&ota_active_bank_info);
    }
    DFU_PRINT_INFO2("image_exist=0x%x(BIT0:OTA, BIT1:patch, ...), image_num=%d",
                    ota_active_bank_info.image_exist, ota_active_bank_info.image_num);

    for (T_IMG_ID image_id = OTA; image_id < IMAGE_MAX; image_id++)
    {
        if (ota_active_bank_info.image_exist & BIT(image_id - OTA))
        {
            get_active_bank_image_version(image_id, &image_ver);
            image_version[index] = image_ver.ver_info.version;
            index ++;
            DFU_PRINT_INFO2("image:0x%x exist!, version=0x%x",
                            image_id, image_ver.ver_info.version);
        }
    }

    peripheral_dfu_send_rsp(PERIP_DFU_OPCODE_READ_VERSION_INFO, PERIP_DFU_FRAME_NO_ERROR,
                            index * sizeof(uint32_t), (uint8_t *)image_version);
}

/******************************************************************
 * @brief    peripheral dfu handle packet request
 * @param    length - data length
 * @return   result
 * @retval   uint8_t
 */
uint8_t peripheral_dfu_handle_packet_req(uint16_t length)
{
    uint8_t status = PERIP_DFU_PAYLOAD_SUCCESS;
    bool is_new_image = false;

    APP_PRINT_INFO4("[peripheral_dfu_handle_packet_req] length=%d, cur_offset =%d, ota_temp_buf_used_size = %d,image_total_length= %d",
                    length,
                    dfuPara.cur_offset,
                    ota_tmp_buf_used_size,
                    dfuPara.image_total_length
                   );

    if (dfuPara.cur_offset + ota_tmp_buf_used_size + length > dfuPara.image_total_length)
    {
        status = PERIP_DFU_PAYLOAD_LENGTH_ERROR;
        APP_PRINT_ERROR0("PERIP_DFU_PAYLOAD_LENGTH_ERROR");
    }
    else
    {
#if HW_AES_KEY
        if (OTP->ota_with_encryption_data)
        {
            dfu_hw_aes_decrypt_image(p_value, p_value, length);
        }
#endif
        ota_tmp_buf_used_size += length;

        if (ota_tmp_buf_used_size == DFU_TEMP_BUFFER_SIZE ||
            dfuPara.cur_offset + ota_tmp_buf_used_size == dfuPara.image_total_length
           )
        {
            is_new_image = (dfuPara.cur_offset == 0 ? true : false);
            APP_PRINT_INFO1("[peripheral_dfu_handle_packet_req] is_new_image = %d", is_new_image);

            uint32_t result = dfu_update(dfuPara.ctrl_header.image_id,
                                         dfuPara.cur_offset + temp_image_info[temp_image_num].image_offset,
                                         ota_tmp_buf_used_size, (uint32_t *)TempBufferHead, is_new_image);
            if (result)
            {
                status = PERIP_DFU_PAYLOAD_UPDATE_ERROR;
                dfuPara.dfu_update_retry_count++;
                APP_PRINT_ERROR1("[peripheral_dfu_handle_packet_req]PERIP_DFU_PAYLOAD_UPDATE_ERROR result = %d",
                                 result);
            }
            else
            {
                dfuPara.cur_offset += ota_tmp_buf_used_size;
                dfuPara.dfu_update_retry_count = 0;
            }
            ota_tmp_buf_used_size = 0;
            if (dfuPara.dfu_update_retry_count >= DFU_UPDATE_RETRY_MAX_NUM)
            {
                peripheral_dfu_deinit(PERIP_DFU_STOP_FALG);
                APP_PRINT_ERROR0("[peripheral_dfu_handle_packet_req]dfu update retry times over setting value");
            }
        }
    }

    return status;
}

/******************************************************************
 * @brief    peripheral dfu handle control point request
 * @param    opcode - operation code value
 * @param    length - the length of payload data
 * @param    p_value - point to payload data
 * @return   void
 * @retval   none
 */
void peripheral_dfu_handle_control_point_req(uint8_t opcode, uint16_t length, uint8_t *p_value)
{
    T_PERIP_DFU_CTRL_POINT dfu_control_point;
    uint8_t notif_data[20] = {0};
    uint8_t status = PERIP_DFU_PAYLOAD_SUCCESS;

    APP_PRINT_INFO2("[peripheral_dfu_handle_control_point_req] opcode=0x%x, length=%d", opcode, length);

    dfu_control_point.opcode = opcode;

    //check the range of the opcode

    switch (dfu_control_point.opcode)
    {
    case PERIP_DFU_OPCODE_START_DFU:
        dfuPara.cur_offset = 0;
        ota_tmp_buf_used_size = 0;

#if HW_AES_KEY
        if (OTP->ota_with_encryption_data)
        {
            APP_PRINT_INFO1("ctrl header before decryped=%b", TRACE_BINARY(16, p_value));
            dfu_hw_aes_decrypt_image(p_value, p_value, 16);
            APP_PRINT_INFO1("ctrl header after decryped=%b", TRACE_BINARY(16, p_value));
        }
#endif
        dfu_control_point.start_dfu.ic_type = (*p_value);
        p_value += 1;
        dfu_control_point.start_dfu.secure_version = (*p_value);
        p_value += 1;
        LE_ARRAY_TO_UINT16(dfu_control_point.start_dfu.ctrl_flag.value, p_value);
        p_value += 2;
        LE_ARRAY_TO_UINT16(dfu_control_point.start_dfu.image_id, p_value);
        p_value += 2;
        LE_ARRAY_TO_UINT16(dfu_control_point.start_dfu.crc16, p_value);
        p_value += 2;

        LE_ARRAY_TO_UINT32(dfu_control_point.start_dfu.payload_len, p_value);

        APP_PRINT_INFO6("PERIP_DFU_OPCODE_START_DFU: ic_type=0x%x, secure_version=0x%x, ctrl_flag.value=0x%x, signature=0x%x,crc16=0x%x*4Bytes, image_length=0x%x",
                        dfu_control_point.start_dfu.ic_type,
                        dfu_control_point.start_dfu.secure_version,
                        dfu_control_point.start_dfu.ctrl_flag.value,
                        dfu_control_point.start_dfu.image_id,
                        dfu_control_point.start_dfu.crc16,
                        dfu_control_point.start_dfu.payload_len
                       );
        dfuPara.ctrl_header.ic_type = dfu_control_point.start_dfu.ic_type;
        dfuPara.ctrl_header.ctrl_flag.value = dfu_control_point.start_dfu.ctrl_flag.value;
        dfuPara.ctrl_header.image_id = dfu_control_point.start_dfu.image_id;
        dfuPara.ctrl_header.crc16 = dfu_control_point.start_dfu.crc16;
        dfuPara.ctrl_header.payload_len = dfu_control_point.start_dfu.payload_len;
        dfuPara.image_total_length = dfuPara.ctrl_header.payload_len + IMG_HEADER_SIZE;

        if (dfuPara.ctrl_header.ic_type == DEFINED_IC_TYPE)
        {
            if (((dfuPara.ctrl_header.image_id >= OTA) && (dfuPara.ctrl_header.image_id < IMAGE_MAX))
                || (dfuPara.ctrl_header.image_id == IMAGE_USER_DATA))
            {
                /*disable bank switch, need record temp image info*/
                if (!is_ota_support_bank_switch())
                {
                    temp_image_info[temp_image_num].image_id = (T_IMG_ID)dfuPara.ctrl_header.image_id;
//                    temp_image_info[temp_image_num].image_size = UP_ALIGN(dfuPara.image_total_length, FMC_SEC_SECTION_LEN);
                    temp_image_info[temp_image_num].image_size = dfuPara.image_total_length;
                    if (temp_image_num == 0)
                    {
                        temp_image_info[temp_image_num].image_offset = 0;
                    }
                    else
                    {
                        temp_image_info[temp_image_num].image_offset = temp_image_info[temp_image_num - 1].image_offset +
                                                                       temp_image_info[temp_image_num - 1].image_size;
                    }
                }
                APP_PRINT_INFO4("PERIP_DFU_OPCODE_START_DFU: image_num=%d, image_id=0x%x, image_size=0x%x, image_offset=0x%x",
                                temp_image_num, temp_image_info[temp_image_num].image_id,
                                temp_image_info[temp_image_num].image_size, temp_image_info[temp_image_num].image_offset);

                //can check battery level here

                status = PERIP_DFU_PAYLOAD_SUCCESS;
            }
            else
            {
                status = PERIP_DFU_PAYLOAD_IMAGE_ID_ERROR;
                peripheral_dfu_deinit(PERIP_DFU_STOP_FALG);
            }
        }
        else
        {
            status = PERIP_DFU_PAYLOAD_IC_TYPE_ERROR;
            peripheral_dfu_deinit(PERIP_DFU_STOP_FALG);
        }

        peripheral_dfu_send_rsp(dfu_control_point.opcode, PERIP_DFU_FRAME_NO_ERROR,
                                PERIP_DFU_PAYLOAD_LEN_START_DFU, &status);
        break;

    case PERIP_DFU_OPCODE_WRITE_IMAGE:
        status = peripheral_dfu_handle_packet_req(length);

        notif_data[0] = (dfuPara.cur_offset + ota_tmp_buf_used_size) & 0xff;
        notif_data[1] = ((dfuPara.cur_offset + ota_tmp_buf_used_size) >> 8) & 0xff;
        notif_data[2] = ((dfuPara.cur_offset + ota_tmp_buf_used_size) >> 16) & 0xff;
        notif_data[3] = (dfuPara.cur_offset + ota_tmp_buf_used_size) >> 24;
        notif_data[4] = status;

        peripheral_dfu_send_rsp(dfu_control_point.opcode, PERIP_DFU_FRAME_NO_ERROR,
                                PERIP_DFU_PAYLOAD_LEN_WRITE_IMAGE, notif_data);
        break;

    case PERIP_DFU_OPCODE_VALID_IMAGE:
        APP_PRINT_INFO1("PERIP_DFU_OPCODE_VALID_IMAGE: signature = 0x%x", dfuPara.ctrl_header.image_id);

        /*if ota large img, need modify wdg timeout period*/
        if (OTP->wdgEnableInRom && dfuPara.image_total_length > 0x100000)
        {
            /*1M and less---4s, 2M and less---8s,..., 8M and less---32s*/
            uint32_t img_align_len = ((dfuPara.image_total_length + (0x100000 - 1)) & (~(0x100000 - 1)));
            uint8_t wdg_period_4s = (img_align_len / 0x100000);
            uint16_t div_factor = (32 * wdg_period_4s - 1);
            APP_PRINT_INFO2("PERIP_DFU_OPCODE_VALID_IMAGE: Change WDG Period to %ds, div_factor=%d",
                            wdg_period_4s << 2, div_factor);
            WDG_Config(div_factor, 15, RESET_ALL); //31 - 4s, 63 - 8s
        }

        peripheral_dfu_handle_valid_fw();
        break;

    case PERIP_DFU_OPCODE_ACTIVE_IMAGE_RESET:
        /*notify bootloader to reset and use new image*/
        APP_PRINT_INFO0("PERIP_DFU_OPCODE_ACTIVE_IMAGE_RESET");

        /*when disable bank switch, need set image ready*/
        peripheral_dfu_handle_active_image();

        status = PERIP_DFU_PAYLOAD_SUCCESS;
        peripheral_dfu_send_rsp(PERIP_DFU_OPCODE_ACTIVE_IMAGE_RESET, PERIP_DFU_FRAME_NO_ERROR,
                                PERIP_DFU_PAYLOAD_LEN_ACTIVE_IMAGE_RESET, &status);

        if (!is_ota_support_bank_switch())
        {
            /*note: must unlock flash bp for ota copy before reset when not support bank switch*/
            unlock_flash_bp_all();
        }

#if (ENABLE_AUTO_BANK_SWITCH == 1)
        if (is_ota_support_bank_switch())
        {
            uint32_t ota_addr;
            ota_addr = get_header_addr_by_img_id(OTA);
            APP_PRINT_INFO1("DFU_OPCODE_ACTIVE_IMAGE_RESET: Bank switch erase ota_addr=0x%x", ota_addr);
            unlock_flash_bp_all();
            flash_erase_locked(FLASH_ERASE_SECTOR, ota_addr);
            lock_flash_bp();
        }
#endif

        dfu_fw_reboot(true);
        break;

    case PERIP_DFU_OPCODE_SYSTEM_RESET:
        APP_PRINT_INFO0("PERIP_DFU_OPCODE_SYSTEM_RESET");

        /*if select not active image by Phone even if image transport successful. Not for single bank user data*/
        if (dfuPara.ctrl_header.image_id >= OTA && dfuPara.ctrl_header.image_id < IMAGE_MAX)
        {
            uint32_t temp_addr = get_temp_ota_bank_addr_by_img_id((T_IMG_ID)dfuPara.ctrl_header.image_id);

            T_IMG_CTRL_HEADER_FORMAT *p_temp_header = (T_IMG_CTRL_HEADER_FORMAT *)temp_addr;
            if (p_temp_header && !p_temp_header->ctrl_flag.flag_value.not_ready)
            {
                flash_erase_locked(FLASH_ERASE_SECTOR, temp_addr);
            }
        }

        status = PERIP_DFU_PAYLOAD_SUCCESS;
        peripheral_dfu_send_rsp(dfu_control_point.opcode, PERIP_DFU_FRAME_NO_ERROR,
                                PERIP_DFU_PAYLOAD_LEN_SYSTEM_RESET, &status);

        dfu_fw_reboot(false);
        break;

    case PERIP_DFU_OPCODE_READ_DEVICE_INFO:
        APP_PRINT_INFO0("PERIP_DFU_OPCODE_READ_DEVICE_INFO");
        peripheral_dfu_deinit(PERIP_DFU_START_FALG);
        peripheral_dfu_handle_read_device_info();
        break;

    case PERIP_DFU_OPCODE_READ_VERSION_INFO:
        APP_PRINT_INFO0("PERIP_DFU_OPCODE_READ_VERSION_INFO");
        peripheral_dfu_handle_read_version_info();
        break;

    default:
        {
            APP_PRINT_INFO1("peripheral_dfu_handle_control_point_req: Unknown Opcode=0x%x",
                            dfu_control_point.opcode);
        }
        break;
    }
}

/******************************************************************
 * @brief    peripheral dfu check packet
 * @param    p_packet - point to peripheral packet
 * @return   result
 * @retval   true or false
 */
bool peripheral_dfu_check_packet(T_PERIPHERAL_PACKET_DEF *p_packet)
{
    uint8_t response_payload = 0;

    //1 check opcode
    uint8_t opcode_high = ((p_packet->opcode) >> 8);
    uint8_t opcode_low = ((p_packet->opcode) & 0xff);

    if ((opcode_low > PERIP_DFU_OPCODE_MIN) && (opcode_low < PERIP_DFU_OPCODE_MAX) &&
        (opcode_high == PACKET_CMD_HIGH_OPCODE_VALUE))
    {
        APP_PRINT_INFO0("[peripheral_dfu_check_packet] valid opCode");
    }
    else
    {
        APP_PRINT_INFO0("[peripheral_dfu_check_packet] invalid opCode");
        peripheral_dfu_send_rsp(opcode_low, PERIP_DFU_FRAME_OPCODE_ERROR, 0, &response_payload);
        return false;
    }

    //2 check payload len
    if (p_packet->is_payload_len_error)
    {
        APP_PRINT_INFO0("[peripheral_dfu_check_packet] payload len error");
        peripheral_dfu_send_rsp(opcode_low, PERIP_DFU_FRAME_LENGTH_ERROR, 0, &response_payload);
        return false;
    }
    //3 check crc
    uint16_t crc_calc_value = 0;
    crc_calc_value = peripheral_dfu_crc16_check(p_packet->header_buf, 5, 0xFFFF);
    crc_calc_value = peripheral_dfu_crc16_check(TempBufferHead + ota_tmp_buf_used_size,
                                                p_packet->payload_len, crc_calc_value);
    crc_calc_value = (crc_calc_value >> 8) | (crc_calc_value & 0xff) << 8;

    APP_PRINT_INFO2("[peripheral_dfu_check_packet] crc_calc_value = 0x%x, crc_value = 0x%x",
                    crc_calc_value, p_packet->crc_value);
    /* CRC check */
    if (crc_calc_value != p_packet->crc_value)
    {
        /* CRC check error */
        APP_PRINT_INFO0("[peripheral_dfu_check_packet] CRC check Error!");
        peripheral_dfu_send_rsp(opcode_low, PERIP_DFU_FRAME_CRC_ERROR, 0, &response_payload);
        return false;
    }

    return true;
}
/*============================================================================*
 *                              Global Functions
 *============================================================================*/
/******************************************************************
 * @brief    peripheral dfu deinit
 * @param    none
 * @return   none
 * @retval   void
 */
void peripheral_dfu_deinit(uint8_t dfu_stop_flag)
{
    ota_tmp_buf_used_size = 0;
    memset(&dfuPara, 0, sizeof(dfuPara));
    temp_image_num = 0;
    memset(temp_image_info, 0, sizeof(temp_image_info));

    if (dfu_stop_flag)
    {
        peripheral_handle_allow_to_enter_dlps();
        peripheral_handle_no_act_timer_stop();
    }
}

/******************************************************************
 * @brief    peripheral dfu handle function
 * @param    none
 * @return   none
 * @retval   void
 */
void peripheral_dfu_handle(void)
{
    APP_PRINT_INFO0("[peripheral_dfu_handle]");

    if (peripheral_handle_decode_packet(&uart_packet, TempBufferHead + ota_tmp_buf_used_size))
    {
        if (peripheral_dfu_check_packet(&uart_packet))
        {
#if FEATURE_SUPPORT_NO_ACTION_EXIT_DFU
            peripheral_handle_no_act_timer_restart();
#endif
            peripheral_dfu_handle_control_point_req(uart_packet.opcode & 0xff, uart_packet.payload_len,
                                                    TempBufferHead);
        }
        else
        {
            peripheral_dfu_deinit(PERIP_DFU_STOP_FALG);
        }
        /* Reset decoding status */
        peripheral_handle_init_packet_struct(&uart_packet);
    }
}

/******************* (C) COPYRIGHT 2022 Realtek Semiconductor Corporation *****END OF FILE****/
