/**
*********************************************************************************************************
*               Copyright(c) 2020, Realtek Semiconductor Corporation. All rights reserved.
*********************************************************************************************************
* @file      loop_queue.h
* @brief
* @details
* @author    barry_bian
* @date      2020-04-09
* @version   v1.0
* *********************************************************************************************************
*/

#ifndef __LOOP_QUEUE_H
#define __LOOP_QUEUE_H

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include "trace.h"
#include "string.h"
#include "os_mem.h"

/* Defines ------------------------------------------------------------------*/

#ifdef PRINT_LOG
#define LOOP_QUEUE_DBG_BUFFER(module, level, fmt, param_num,...) \
    DBG_BUFFER_##level(TYPE_BUMBLEBEE3, SUBTYPE_FORMAT, module, fmt, param_num, ##__VA_ARGS__)

#else
#define LOOP_QUEUE_DBG_BUFFER(MODULE, LEVEL, pFormat, para_num,...) ((void)0)
#endif

typedef void *T_LOOP_QUEUE_BUF;

/**
 * @brief Loop queue data struct
 */
typedef struct
{
    volatile bool               is_over_flow;       /* loop queue item size */
    volatile uint16_t           item_size;          /* buf data type */
    volatile uint16_t           buf_max_item_num;   /* Buffer size of loop queue */
    volatile uint16_t           read_index;         /* index of read queue */
    volatile uint16_t           write_index;        /* index of write queue */
    volatile T_LOOP_QUEUE_BUF   p_buf;              /* Buffer for loop queue */
} T_LOOP_QUEUE_DEF;


T_LOOP_QUEUE_DEF *loop_queue_init(uint16_t buf_max_size, uint16_t item_size,
                                  RAM_TYPE ram_type);
void loop_queue_deinit(T_LOOP_QUEUE_DEF **p_loop_queue);
bool loop_queue_is_full(T_LOOP_QUEUE_DEF *p_queue_struct, uint16_t write_size);
bool loop_queue_is_empty(T_LOOP_QUEUE_DEF *p_queue_struct);
uint16_t loop_queue_get_vailid_data_size(T_LOOP_QUEUE_DEF *p_queue_struct);
uint16_t loop_queue_get_free_data_size(T_LOOP_QUEUE_DEF *p_loop_queue);
bool loop_queue_write_buf(T_LOOP_QUEUE_DEF *p_queue_struct, T_LOOP_QUEUE_BUF p_buf, uint16_t size,
                          bool is_overflow_data_abandoned);
bool loop_queue_read_buf(T_LOOP_QUEUE_DEF *p_queue_struct, T_LOOP_QUEUE_BUF p_buf, uint16_t size);
bool loop_queue_copy_buf(T_LOOP_QUEUE_DEF *p_queue_struct, T_LOOP_QUEUE_BUF p_buf, uint16_t size);
bool loop_queue_set_empty(T_LOOP_QUEUE_DEF *p_loop_queue);
#ifdef __cplusplus
}
#endif

#endif /*__UART_TRANS_H*/

/******************* (C) COPYRIGHT 2017 Realtek Semiconductor Corporation *****END OF FILE****/

