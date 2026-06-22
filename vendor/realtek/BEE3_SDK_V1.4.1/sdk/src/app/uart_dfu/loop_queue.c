/**
*********************************************************************************************************
*               Copyright(c) 2020, Realtek Semiconductor Corporation. All rights reserved.
**********************************************************************************************************
* @file      loop_queue.c
* @brief     This file provides loop queue for user.
* @details
* @author    barry_bian
* @date      2020-05-12
* @version   v1.1
*********************************************************************************************************
*/

/*============================================================================*
 *                              Header Files
 *============================================================================*/
#include "loop_queue.h"
#include "app_section.h"

/*============================================================================*
 *                              Functions Declaration
 *============================================================================*/
bool loop_queue_is_full(T_LOOP_QUEUE_DEF *p_queue_struct, uint16_t write_size) DATA_RAM_FUNCTION;
bool loop_queue_is_empty(T_LOOP_QUEUE_DEF *p_queue_struct) DATA_RAM_FUNCTION;
uint16_t loop_queue_get_vailid_data_size(T_LOOP_QUEUE_DEF *p_queue_struct) DATA_RAM_FUNCTION;
uint16_t loop_queue_get_free_data_size(T_LOOP_QUEUE_DEF *p_loop_queue) DATA_RAM_FUNCTION;
bool loop_queue_write_buf(T_LOOP_QUEUE_DEF *p_queue_struct, T_LOOP_QUEUE_BUF p_buf, uint16_t size,
                          bool is_overflow_data_abandoned) DATA_RAM_FUNCTION;
bool loop_queue_read_buf(T_LOOP_QUEUE_DEF *p_queue_struct, T_LOOP_QUEUE_BUF p_buf,
                         uint16_t size) DATA_RAM_FUNCTION;
bool loop_queue_copy_buf(T_LOOP_QUEUE_DEF *p_queue_struct, T_LOOP_QUEUE_BUF p_buf,
                         uint16_t size) DATA_RAM_FUNCTION;

/*============================================================================*
 *                              Global Functions
 *============================================================================*/
/******************************************************************
 * @brief   Initializes loop queue to their default reset values.
 * @param   buf_max_size - loop queue buffer max size.
 * @param   item_size - queue item size.
 * @param   ram_type - RAM_TYPE_DATA_ON or RAM_TYPE_BUFFER_ON.
 * @retval  void*
 */
T_LOOP_QUEUE_DEF *loop_queue_init(uint16_t item_cnt, uint16_t item_size,
                                  RAM_TYPE ram_type)
{
    if ((item_cnt == 0) && (item_size == 0))
    {
        LOOP_QUEUE_DBG_BUFFER(MODULE_APP, LEVEL_ERROR, "[loop queue] buf_max_size is invalid!", 0);
        return NULL;
    }
    if (ram_type != RAM_TYPE_DATA_ON && ram_type != RAM_TYPE_BUFFER_ON)
    {
        LOOP_QUEUE_DBG_BUFFER(MODULE_APP, LEVEL_ERROR, "[loop queue] ram type is invalid", 0);
        return NULL;
    }
    T_LOOP_QUEUE_DEF *p_loop_queue = NULL;
    p_loop_queue = os_mem_alloc(ram_type, sizeof(T_LOOP_QUEUE_DEF));
    if (p_loop_queue != NULL)
    {
        uint32_t buf_max_size = item_cnt * item_size;
        memset(p_loop_queue, 0, sizeof(T_LOOP_QUEUE_DEF));
        p_loop_queue->item_size = item_size;
        p_loop_queue->buf_max_item_num = item_cnt;
        p_loop_queue->is_over_flow = false;
        p_loop_queue->p_buf = NULL;
        p_loop_queue->p_buf = os_mem_alloc(ram_type, buf_max_size);
        if (p_loop_queue->p_buf != NULL)
        {
            memset((uint8_t *)p_loop_queue->p_buf, 0, buf_max_size);
        }
        else
        {
            os_mem_free(p_loop_queue);
            p_loop_queue = NULL;
        }
    }
    return p_loop_queue;
}

/******************************************************************
 * @brief   Deinitializes loop queue
 * @param   p_loop_queue - point to loop queue which needs deinitialization.
 * @return  none
 * @retval  void
 */
void loop_queue_deinit(T_LOOP_QUEUE_DEF **p_loop_queue)
{
    if (*p_loop_queue != NULL)
    {
        if ((*p_loop_queue)->p_buf != NULL)
        {
            os_mem_free((*p_loop_queue)->p_buf);
            (*p_loop_queue)->p_buf = NULL;
        }
        os_mem_free((*p_loop_queue));
        *p_loop_queue = NULL;
    }
}

/******************************************************************
 * @brief   Get valid data length of loop queue.
 * @param   p_loop_queue - point to loop queue dta struct.
 * @return  valid data size of loop queue.
 * @retval  uint16_t
 */
uint16_t loop_queue_get_vailid_data_size(T_LOOP_QUEUE_DEF *p_loop_queue)
{
    if (p_loop_queue == NULL)
    {
        LOOP_QUEUE_DBG_BUFFER(MODULE_APP, LEVEL_ERROR, "[loop queue] pointer is invalid!", 0);
        return 0;
    }

    if (p_loop_queue->write_index == p_loop_queue->read_index)
    {
        if (p_loop_queue->is_over_flow == true)
        {
            return (p_loop_queue->item_size * p_loop_queue->buf_max_item_num);
        }
        else
        {
            return 0;
        }
    }
    else
    {
        return (p_loop_queue->item_size * ((p_loop_queue->buf_max_item_num + p_loop_queue->write_index -
                                            p_loop_queue->read_index) % p_loop_queue->buf_max_item_num));
    }
}

/******************************************************************
 * @brief   Get free data length of loop queue.
 * @param   p_queue_struct - point to loop queue dta struct.
 * @return  free data size of loop queue.
 * @retval  uint16_t
 */
uint16_t loop_queue_get_free_data_size(T_LOOP_QUEUE_DEF *p_loop_queue)
{
    if (p_loop_queue == NULL)
    {
        LOOP_QUEUE_DBG_BUFFER(MODULE_APP, LEVEL_ERROR, "[loop queue] pointer is invalid!", 0);
        return 0;
    }

    if (p_loop_queue->write_index == p_loop_queue->read_index)
    {
        if (p_loop_queue->is_over_flow == true)
        {
            return 0;
        }
        else
        {
            return (p_loop_queue->item_size * p_loop_queue->buf_max_item_num);
        }
    }
    else
    {
        return (p_loop_queue->item_size * ((p_loop_queue->buf_max_item_num - p_loop_queue->write_index +
                                            p_loop_queue->read_index) % p_loop_queue->buf_max_item_num));
    }
}

/******************************************************************
 * @brief   check loop queue if will full or not.
 * @param   p_loop_queue - point to loop queue dta struct.
 * @return  loop queue is full or not
 * @retval  TRUE - full
 * @retval  FALSE - not full
 */
bool loop_queue_is_full(T_LOOP_QUEUE_DEF *p_loop_queue, uint16_t write_size)
{
    if (p_loop_queue == NULL)
    {
        LOOP_QUEUE_DBG_BUFFER(MODULE_APP, LEVEL_ERROR, "[loop queue] pointer is invalid!", 0);
        return true;
    }
    if (write_size % p_loop_queue->item_size != 0)
    {
        LOOP_QUEUE_DBG_BUFFER(MODULE_APP, LEVEL_ERROR, "[loop queue] buf_max_size is invalid!", 0);
        return true;
    }

    if (p_loop_queue->is_over_flow == true)
    {
        LOOP_QUEUE_DBG_BUFFER(MODULE_APP, LEVEL_ERROR, "[loop queue] Loop Queue is already full!", 0);
        return true;
    }
    else if (write_size >= loop_queue_get_free_data_size(p_loop_queue))
    {
        LOOP_QUEUE_DBG_BUFFER(MODULE_APP, LEVEL_ERROR, "[loop queue] Loop Queue will over flow!", 0);
        return true;
    }

    return false;
}

/******************************************************************
 * @brief   check loop queue if empty or not.
 * @param   p_loop_queue - point to loop queue dta struct.
 * @return  loop queue is empty or not
 * @retval  TRUE - empty
 * @retval  FALSE - empty full
 */
bool loop_queue_is_empty(T_LOOP_QUEUE_DEF *p_loop_queue)
{
    if (p_loop_queue == NULL)
    {
        LOOP_QUEUE_DBG_BUFFER(MODULE_APP, LEVEL_ERROR, "[loop queue] pointer is invalid!", 0);
        return false;
    }

    if (p_loop_queue->is_over_flow == false && p_loop_queue->write_index == p_loop_queue->read_index)
    {
        return true;
    }

    return false;
}

/******************************************************************
 * @brief   Write source buffer data to loop queue.
 * @param   p_loop_queue - point to loop queue data struct.
 * @param   p_write_buf - point to write buffer.
 * @param   size - size of data to be written.
 * @return  result
 * @retval  true or false
 */
bool loop_queue_write_buf(T_LOOP_QUEUE_DEF *p_loop_queue, T_LOOP_QUEUE_BUF p_write_buf,
                          uint16_t size, bool is_overflow_data_abandoned)
{
    uint16_t queue_buf_cur_idx = 0;
    uint16_t write_buf_cur_idx = 0;
    uint16_t remain_len = 0;
    uint16_t write_len = 0;
    uint16_t data_size = 0;

    /* Check parameters */
    if (p_write_buf == NULL || p_loop_queue == NULL || p_loop_queue->p_buf == NULL)
    {
        LOOP_QUEUE_DBG_BUFFER(MODULE_APP, LEVEL_ERROR, "[loop queue] pointer is invalid!", 0);
        return false;
    }

    if (size <= 0 || size % p_loop_queue->item_size != 0)
    {
        LOOP_QUEUE_DBG_BUFFER(MODULE_APP, LEVEL_ERROR, "[loop queue] write data size is invalid!", 0);
        return false;
    }

    if (!loop_queue_is_full(p_loop_queue, size))
    {
        remain_len = size / p_loop_queue->item_size;
    }
    else
    {
        if (is_overflow_data_abandoned == true)
        {
            remain_len = loop_queue_get_free_data_size(p_loop_queue) / p_loop_queue->item_size;
        }
        else
        {
            remain_len = size / p_loop_queue->item_size;
        }
        p_loop_queue->is_over_flow = true;
    }

    while (remain_len)
    {
        if (p_loop_queue->write_index + remain_len <= p_loop_queue->buf_max_item_num)
        {
            write_len = remain_len;
        }
        else
        {
            write_len = p_loop_queue->buf_max_item_num - p_loop_queue->write_index;
        }

        queue_buf_cur_idx = p_loop_queue->item_size * p_loop_queue->write_index;
        data_size = p_loop_queue->item_size * write_len;
        memcpy((uint8_t *)p_loop_queue->p_buf + queue_buf_cur_idx,
               (uint8_t *)p_write_buf + write_buf_cur_idx, data_size);
        write_buf_cur_idx += data_size;
        p_loop_queue->write_index = (p_loop_queue->write_index + write_len) %
                                    p_loop_queue->buf_max_item_num;
        remain_len -= write_len;
    }
    return true;
}

/******************************************************************
 * @brief   Read data from loop queue.
 * @param   p_queue_struct - point to loop queue data struct.
 * @param   p_read_buf - point to read buffer
 * @param   size - size of data to be read.
 * @return  result
 * @retval  true or false
 */
bool loop_queue_read_buf(T_LOOP_QUEUE_DEF *p_loop_queue, T_LOOP_QUEUE_BUF p_read_buf,
                         uint16_t size)
{
    uint16_t queue_buf_cur_idx = 0;
    uint16_t read_buf_cur_idx = 0;
    uint16_t remain_len = 0;
    uint16_t read_len = 0;
    uint16_t data_size = 0;

    /* Check parameters */
    if (p_read_buf == NULL || p_loop_queue == NULL || p_loop_queue->p_buf == NULL)
    {
        LOOP_QUEUE_DBG_BUFFER(MODULE_APP, LEVEL_ERROR, "[loop queue] pointer is invalid!", 0);
        return false;
    }

    if (size <= 0 || size % p_loop_queue->item_size != 0)
    {
        LOOP_QUEUE_DBG_BUFFER(MODULE_APP, LEVEL_ERROR, "[loop queue] read data size is invalid!", 0);
        return false;
    }

    if (loop_queue_get_vailid_data_size(p_loop_queue) < size)
    {
        LOOP_QUEUE_DBG_BUFFER(MODULE_APP, LEVEL_ERROR, "[loop queue] there is not enough data to read!", 0);
        return false;
    }

    remain_len = size / p_loop_queue->item_size;

    while (remain_len)
    {
        if (p_loop_queue->read_index + remain_len <= p_loop_queue->buf_max_item_num)
        {
            read_len = remain_len;
        }
        else
        {
            read_len = p_loop_queue->buf_max_item_num - p_loop_queue->read_index;
        }

        queue_buf_cur_idx = p_loop_queue->item_size * p_loop_queue->read_index;
        data_size = p_loop_queue->item_size * read_len;
        memcpy((uint8_t *)p_read_buf + read_buf_cur_idx, (uint8_t *)p_loop_queue->p_buf + queue_buf_cur_idx,
               data_size);
        read_buf_cur_idx += data_size;
        p_loop_queue->read_index = (p_loop_queue->read_index + read_len) % p_loop_queue->buf_max_item_num;
        remain_len -= read_len;
    }

    p_loop_queue->is_over_flow = false;

    return true;
}

/******************************************************************
 * @brief   copy buffer data from loop queue.
 * @param   p_loop_queue - point to loop queue data struct.
 * @param   p_copy_buf - point to copy buffer
 * @param   size - size of data to be copy.
 * @return  result
 * @retval  true or false
 */
bool loop_queue_copy_buf(T_LOOP_QUEUE_DEF *p_loop_queue, T_LOOP_QUEUE_BUF p_copy_buf,
                         uint16_t size)
{
    uint16_t queue_buf_cur_idx = 0;
    uint16_t copy_buf_cur_idx = 0;
    uint16_t remain_len = 0;
    uint16_t copy_len = 0;
    uint16_t data_size = 0;
    uint16_t copy_index = 0;

    /* Check parameters */
    if (p_copy_buf == NULL || p_loop_queue == NULL || p_loop_queue->p_buf == NULL)
    {
        LOOP_QUEUE_DBG_BUFFER(MODULE_APP, LEVEL_ERROR, "[loop queue] pointer is invalid!", 0);
        return false;
    }

    if (size <= 0 || size % p_loop_queue->item_size != 0)
    {
        LOOP_QUEUE_DBG_BUFFER(MODULE_APP, LEVEL_ERROR, "[loop queue] read data size is invalid!", 0);
        return false;
    }

    if (loop_queue_get_vailid_data_size(p_loop_queue) < size)
    {
        LOOP_QUEUE_DBG_BUFFER(MODULE_APP, LEVEL_ERROR, "[loop queue] there is not enough data to copy!", 0);
        return false;
    }

    remain_len = size / p_loop_queue->item_size;
    copy_index = p_loop_queue->read_index;

    while (remain_len)
    {
        if (copy_index + remain_len <= p_loop_queue->buf_max_item_num)
        {
            copy_len = remain_len;
        }
        else
        {
            copy_len = p_loop_queue->buf_max_item_num - copy_index;
        }

        queue_buf_cur_idx = p_loop_queue->item_size * copy_index;
        data_size = p_loop_queue->item_size * copy_len;
        memcpy((uint8_t *)p_copy_buf + copy_buf_cur_idx, (uint8_t *)p_loop_queue->p_buf + queue_buf_cur_idx,
               data_size);
        copy_buf_cur_idx += data_size;
        copy_index = (copy_index + copy_len) % p_loop_queue->buf_max_item_num;
        remain_len -= copy_len;
    }
    return true;
}

/******************************************************************
 * @brief   empty the loop queue.
 * @param   p_loop_queue - point to loop queue data struct.
 * @return  result
 * @retval  true or false
 */
bool loop_queue_set_empty(T_LOOP_QUEUE_DEF *p_loop_queue)
{
    if (p_loop_queue == NULL)
    {
        LOOP_QUEUE_DBG_BUFFER(MODULE_APP, LEVEL_ERROR, "[loop queue] pointer is invalid!", 0);
        return false;
    }

    p_loop_queue->is_over_flow = false;
    p_loop_queue->read_index = p_loop_queue->write_index;

    return true;
}
/******************* (C) COPYRIGHT 2020 Realtek Semiconductor Corporation *****END OF FILE****/

