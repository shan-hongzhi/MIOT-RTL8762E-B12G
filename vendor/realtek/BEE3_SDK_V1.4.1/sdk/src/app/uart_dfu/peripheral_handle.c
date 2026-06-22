/**
*********************************************************************************************************
*               Copyright(c) 2022, Realtek Semiconductor Corporation. All rights reserved.
**********************************************************************************************************
* @file     peripheral_handle.c
* @brief    This is the entry of user code which the peripheral handle module resides in.
* @details
* @author   mandy
* @date     2022-01-04
* @version  v1.0
*********************************************************************************************************
*/

/*============================================================================*
 *                              Header Files
 *============================================================================*/
#include <string.h>
#include "board.h"
#include "uart_driver.h"
#include "peripheral_handle.h"
#include <trace.h>
#include "os_timer.h"
#include "rtl876x_wdg.h"
#include <app_section.h>
#include "app_task.h"
#include "os_sync.h"
#include "loop_queue.h"
#include "peripheral_dfu.h"

/*============================================================================*
 *                              Local Variables
 *============================================================================*/
#if FEATURE_SUPPORT_NO_ACTION_EXIT_DFU
static TimerHandle_t no_act_exit_dfu_timer;
#endif

/*============================================================================*
 *                              Global Variables
 *============================================================================*/

/*============================================================================*
 *                              Functions Declaration
 *============================================================================*/
#if FEATURE_SUPPORT_NO_ACTION_EXIT_DFU
static void no_act_exit_dfu_timer_callback(TimerHandle_t p_timer) DATA_RAM_FUNCTION;
#endif
void peripheral_handle_allow_to_enter_dlps(void) DATA_RAM_FUNCTION;
void peripheral_handle_disallow_to_enter_dlps(void) DATA_RAM_FUNCTION;
bool peripheral_handle_check_dlps(void) DATA_RAM_FUNCTION;
void peripheral_handle_send_data(uint8_t *buf, uint32_t length) DATA_RAM_FUNCTION;
bool peripheral_handle_decode_packet(T_PERIPHERAL_PACKET_DEF *p_packet,
                                     uint8_t *payload_buf) DATA_RAM_FUNCTION;
/*============================================================================*
 *                              Local Functions
 *============================================================================*/
#if FEATURE_SUPPORT_NO_ACTION_EXIT_DFU
/******************************************************************
 * @brief no action exit dfu timer callback
 *
 * no_act_exit_dfu_timer_callback is used to exit peripheral dfu after timeout
 *
 * @param p_timer - timer handler
 * @return none
 * @retval void
 */
void no_act_exit_dfu_timer_callback(TimerHandle_t p_timer)
{
    APP_PRINT_INFO0("[no_act_exit_dfu_timer_callback]");
    peripheral_dfu_deinit(PERIP_DFU_STOP_FALG);
}
#endif

/*============================================================================*
 *                              Global Functions
 *============================================================================*/
/******************************************************************
 * @brief peripheral handle allow to enter dlps function
 * @param  none
 * @return none
 * @retval void
 */
void peripheral_handle_allow_to_enter_dlps(void)
{
    APP_PRINT_INFO0("[peripheral_handle_allow_to_enter_dlps]");
    uart_global_data.is_allowed_to_enter_dlps = true;
}

/******************************************************************
 * @brief peripheral handle disallow to enter dlps function
 * @param  none
 * @return none
 * @retval void
 */
void peripheral_handle_disallow_to_enter_dlps(void)
{
    APP_PRINT_INFO0("[peripheral_handle_disallow_to_enter_dlps]");
    uart_global_data.is_allowed_to_enter_dlps = false;
}

/******************************************************************
 * @brief peripheral handle check dlps function
 * @param  none
 * @return none
 * @retval void
 */
bool peripheral_handle_check_dlps(void)
{
    return uart_global_data.is_allowed_to_enter_dlps;
}

#if FEATURE_SUPPORT_NO_ACTION_EXIT_DFU
/******************************************************************
 * @brief peripheral handle timer init function
 * @param  none
 * @return none
 * @retval void
 */
void peripheral_handle_timer_init(void)
{
    /* no_act_exit_dfu_timer is used to exit peripheral dfu after timeout if there is on action */
    if (false == os_timer_create(&no_act_exit_dfu_timer, "no_act_exit_dfu_timer",  1, \
                                 NO_ACTION_EXIT_DFU_TIMEOUT, false, no_act_exit_dfu_timer_callback))
    {
        APP_PRINT_INFO0("[peripheral_handle_timer_init] init no_act_exit_dfu_timer failed");
    }
}

/******************************************************************
 * @brief peripheral handle no act timer restart function
 * @param  none
 * @return none
 * @retval void
 */
void peripheral_handle_no_act_timer_restart(void)
{
    APP_PRINT_INFO0("[peripheral_handle_no_act_timer_restart]");
    os_timer_restart(&no_act_exit_dfu_timer, NO_ACTION_EXIT_DFU_TIMEOUT);
}

/******************************************************************
 * @brief peripheral handle no act timer stop function
 * @param  none
 * @return none
 * @retval void
 */
void peripheral_handle_no_act_timer_stop(void)
{
    APP_PRINT_INFO0("[peripheral_handle_no_act_timer_stop]");
    os_timer_stop(&no_act_exit_dfu_timer);
}
#endif

/******************************************************************
 * @brief    peripheral handle send data
 * @param    buf - point to data buf.
 * @param    length - length of data to be sent.
 * @return   none
 * @retval   void
 */
void peripheral_handle_send_data(uint8_t *buf, uint32_t length)
{
    APP_PRINT_INFO1("[peripheral_handle_send_data] length = %d", length);
    uart_send_data(buf, length);
}

/******************************************************************
 * @brief   Initializes peripheral packet data structure.
 * @param   p_packet - point to peripheral packet structure.
 * @return  none
 * @retval  void
 */
void peripheral_handle_init_packet_struct(T_PERIPHERAL_PACKET_DEF *p_packet)
{
    /* Initialize UART packet data structure */
    p_packet->buf_index   = 0;
    p_packet->opcode     = 0;
    p_packet->payload_len = 0;
    p_packet->crc_value = 0;
    p_packet->peripheral_status = PERIPHERAL_STATE_WAIT_HEADER;
}

/******************************************************************
 * @brief   decode packet.
 * @param   p_packet - point to peripheral packet struct.
 * @param   payload_buf - point to payload buf.
 * @return  result
 * @retval  true or false
 */
bool peripheral_handle_decode_packet(T_PERIPHERAL_PACKET_DEF *p_packet, uint8_t *payload_buf)
{
    bool is_paket_complete = false;
    uint8_t *p_loop_buf = (uint8_t *)p_uart_queue->p_buf;

    APP_PRINT_INFO0("[peripheral_handle_decode_packet] decode uart packet");
    APP_PRINT_INFO2("[peripheral_handle_decode_packet] read_index = %d, write_index = %d",
                    p_uart_queue->read_index, p_uart_queue->write_index);

    while (p_uart_queue->read_index != p_uart_queue->write_index)
    {
        switch (p_packet->peripheral_status)
        {
        case PERIPHERAL_STATE_WAIT_HEADER:
            {
                /* Header: one byte */
                if (p_loop_buf[p_uart_queue->read_index] == PACKET_CMD_START_BYTE_VALUE)
                {
                    if (true == loop_queue_read_buf(p_uart_queue, &p_packet->header_buf[p_packet->buf_index], 1))
                    {
                        p_packet->buf_index++;
                    }
                    p_packet->peripheral_status = PERIPHERAL_STATE_WAIT_OPCODE;
                }
                else
                {
                    p_uart_queue->read_index++;
                    APP_PRINT_INFO1("[peripheral_handle_decode_packet] start byte error. 0x%x",
                                    p_loop_buf[p_uart_queue->read_index]);
                }
                break;
            }
        case PERIPHERAL_STATE_WAIT_OPCODE:
            {
                while (!loop_queue_is_empty(p_uart_queue))
                {
                    /* Obtain command field data */
                    if (true == loop_queue_read_buf(p_uart_queue, &p_packet->header_buf[p_packet->buf_index], 1))
                    {
                        p_packet->buf_index++;
                    }

                    /* OPCODE: two bytes */
                    if (p_packet->buf_index == (PACKET_CMD_START_BYTE_LEN + PACKET_CMD_OPCODE_LEN))
                    {
                        p_packet->opcode = (p_packet->header_buf[p_packet->buf_index - 1] << 8) +
                                           p_packet->header_buf[p_packet->buf_index - 2];

                        APP_PRINT_INFO1("[peripheral_handle_decode_packet] opCode = 0x%x", p_packet->opcode);

                        p_packet->peripheral_status = PERIPHERAL_STATE_WAIT_LENGTH;
                        break;
                    }
                }
                break;
            }

        case PERIPHERAL_STATE_WAIT_LENGTH:
            {
                while (!loop_queue_is_empty(p_uart_queue))
                {
                    /* Obtain command field data */
                    if (true == loop_queue_read_buf(p_uart_queue, &p_packet->header_buf[p_packet->buf_index], 1))
                    {
                        p_packet->buf_index++;
                    }

                    /* LENGTH: two bytes */
                    if (p_packet->buf_index == (PACKET_CMD_START_BYTE_LEN + PACKET_CMD_OPCODE_LEN +
                                                PACKET_CMD_LENGTH_LEN))
                    {
                        p_packet->payload_len = (p_packet->header_buf[p_packet->buf_index - 1] << 8) +
                                                p_packet->header_buf[p_packet->buf_index - 2];
                        p_packet->peripheral_status = PERIPHERAL_STATE_WAIT_PARAMS;
                        APP_PRINT_INFO1("[peripheral_handle_decode_packet] param_len = %d", p_packet->payload_len);
                        break;
                    }
                }
                break;
            }

        case PERIPHERAL_STATE_WAIT_PARAMS:
            {
                /* Obtain parameter field data */
                if (loop_queue_get_vailid_data_size(p_uart_queue) < p_packet->payload_len)
                {
                    p_packet->is_payload_len_error = true;
                }
                else
                {
                    p_packet->is_payload_len_error = false;
                }
                if (true == loop_queue_read_buf(p_uart_queue, payload_buf, p_packet->payload_len))
                {
                    p_packet->buf_index += p_packet->payload_len;
                }
                p_packet->peripheral_status = PERIPHERAL_STATE_WAIT_CRC;
                break;
            }

        case PERIPHERAL_STATE_WAIT_CRC:
            {
                uint8_t buf[PACKET_CMD_CRC_DATA_LEN] = {0};
                uint16_t vailid_data_size = loop_queue_get_vailid_data_size(p_uart_queue);
                if (vailid_data_size != 0)
                {
                    if (vailid_data_size >= PACKET_CMD_CRC_DATA_LEN)
                    {
                        if (true == loop_queue_read_buf(p_uart_queue, &buf, PACKET_CMD_CRC_DATA_LEN))
                        {
                            p_packet->buf_index += PACKET_CMD_CRC_DATA_LEN;
                        }
                    }
                    else
                    {
                        loop_queue_set_empty(p_uart_queue);
                    }
                }
                p_packet->crc_value = (buf[0] << 8) + buf[1];

                is_paket_complete = true;
            }
        default:
            {
                break;
            }
        }
    }

    return is_paket_complete;
}

/******************* (C) COPYRIGHT 2022 Realtek Semiconductor Corporation *****END OF FILE****/
