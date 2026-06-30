/*
 * miio_user_api.h
 *
 *  Created on: 2023/06/14
 *      Author: mi
 */
#include <sys/reboot.h>
#include "mible.h"
#include "mible_user_api.h"

#define RECORD_ID_OFFSET_USER        0x100

/**
 *@brief    reboot device.
 *@return   0: success, negetive value: failure
 */
static inline int miio_system_reboot(void)
{
    sys_reboot(0);
    return MI_SUCCESS;
}

/**
 *@brief    set mibeacon advertising timeout, after timeout advertising will stop.
 *@param    [in] millsecond : adv timeout in ms, 0 is stop adv, 0xffffffff is always on.
 *@return   0: success, negetive value: failure
 */
static inline int miio_system_set_adv_timeout(uint32_t timeout)
{
    return mibeacon_set_adv_timeout(timeout);
}


/* FLASH related function*/

/*
 * @brief   Create a record in flash
 * @param   [in] record_id: identify a record in flash
 *          [in] len: record length
 * @return  MI_SUCCESS              Create successfully.
 *          MI_ERR_INVALID_LENGTH   Size was 0, or higher than the maximum
 *allowed size.
 *          MI_ERR_NO_MEM,          Not enough flash memory to be assigned
 *
 * */
static inline int miio_record_create(uint16_t record_id, uint16_t len)
{
    return MI_SUCCESS;
}

/*
 * @brief   Delete a record in flash
 * @param   [in] record_id: identify a record in flash
 * @return  MI_SUCCESS              Delete successfully.
 *          MI_ERR_INVALID_PARAM   Invalid record id supplied.
 * */
static inline int miio_record_delete(uint16_t record_id)
{
    return mible_record_delete(RECORD_ID_OFFSET_USER + record_id);
}

/*
 * @brief   Restore data to flash
 * @param   [in] record_id: identify an area in flash
 *          [out] p_data: pointer to data
 *          [in] len: data buffer length
 * @return  MI_SUCCESS              The command was accepted.
 *          MI_ERR_INVALID_LENGTH   Size was 0, or higher than the maximum
 *allowed size.
 *          MI_ERR_INVALID_PARAM   Invalid record id supplied.
 *          MI_ERR_INVALID_ADDR     Invalid pointer supplied.
 * */
static inline int miio_record_read(uint16_t record_id, uint8_t* p_data, uint16_t len)
{
    return mible_record_read(RECORD_ID_OFFSET_USER + record_id, p_data, len);
}

/*
 * @brief   Store data to flash
 * @param   [in] record_id: identify an area in flash
 *          [in] p_data: pointer to data
 *          [in] len: data length
 * @return  MI_SUCCESS              The command was accepted.
 *          MI_ERR_INVALID_LENGTH   Size was 0, or higher than the maximum
 * allowed size.
 *          MI_ERR_INVALID_PARAM   p_data is not aligned to a 4 byte boundary.
 * @note    Should use asynchronous mode to implement this function.
 *          The data to be written to flash has to be kept in memory until the
 * operation has terminated, i.e., an event is received.
 *          When record writing complete , call mible_arch_event_callback function and pass MIBLE_ARCH_EVT_RECORD_WRITE_CMP event and result.
 * */
static inline int miio_record_write(uint16_t record_id, const uint8_t *p_data, uint16_t len)
{
    return mible_record_write(RECORD_ID_OFFSET_USER + record_id, p_data, len);
}

/*TIMER related function*/

/*
 * @brief   Create a timer.
 * @param   [out] pp_timer: a pointer to timer handle address which can uniquely
 *  identify the timer.
 *          [in] timeout_handler: a function will be called when the timer expires.
 *          [in] mode: repeated or single shot.
 * @return  MI_SUCCESS             If the timer was successfully created.
 *          MI_ERR_INVALID_PARAM   Invalid pointer supplied.
 *          MI_ERR_INVALID_STATE   timer module has not been initialized or the
 * timer is running.
 *          MI_ERR_NO_MEM          timer pool is full.
 *
 * */
static inline int miio_timer_create(void** pp_timer, mible_timer_handler timeout_handler, mible_timer_mode mode)
{
    return mible_user_timer_create(pp_timer, timeout_handler, mode);
}

/*
 * @brief   Delete a timer.
 * @param   [in] timer_handle: unique index of the timer.
 * @return  MI_SUCCESS             If the timer was successfully deleted.
 *          MI_ERR_INVALID_PARAM   Invalid timer id supplied..
 * */
static inline int miio_timer_delete(void* timer_handle)
{
    return mible_user_timer_delete(timer_handle);
}

/*
 * @brief   Start a timer.
 * @param   [in] timer_handle: unique index of the timer.
 *          [in] time_ms: number of milliseconds to time-out event
 * (minimum 10 ms).
 *          [in] p_context: parameters that can be passed to
 * timeout_handler
 *
 * @return  MI_SUCCESS             If the timer was successfully started.
 *          MI_ERR_INVALID_PARAM   Invalid timer id supplied.
 *          MI_ERR_INVALID_STATE   If the application timer module has not been
 * initialized or the timer has not been created.
 *          MI_ERR_NO_MEM          If the timer operations queue was full.
 * @note    If the timer has already started, it will start counting again.
 * */
static inline int miio_timer_start(void* timer_handle, uint32_t time_ms, void* p_context)
{
    return mible_user_timer_start(timer_handle, time_ms, p_context);
}

/*
 * @brief   Stop a timer.
 * @param   [in] timer_handle: unique index of the timer.
 * @return  MI_SUCCESS             If the timer was successfully stopped.
 *          MI_ERR_INVALID_PARAM   Invalid timer id supplied.
 *
 * */
static inline int miio_timer_stop(void* timer_handle)
{
    return mible_user_timer_stop(timer_handle);
}

/**
 *@brief    send properties_changed (multi props).
 *@param    [in] nums: properties numbers.
 *@param    [in] newProps: properties siid/piid/value.
 *@return   0: success, negetive value: failure
 */
static inline int miio_gatt_properties_changed(uint8_t nums, mible_spec_property_t *newProps)
{
    return mible_gatt_spec_send_properties_changed(nums, newProps);
}

/**
 *@brief    send event_occurred.
 *@param    [in] siid: service id.
 *@param    [in] piid: property id.
 *@param    [in] newArgs: event args.
 *@return   0: success, negetive value: failure
 */
static inline int miio_gatt_event_occurred(uint16_t siid, uint16_t eiid, mible_spec_arguments_t *newArgs)
{
    return mible_gatt_spec_send_event_occurred(siid, eiid, newArgs);
}

#if defined(CONFIG_MIBLE_PSK) && (CONFIG_MIBLE_PSK)


#else
#include "mible_mesh.h"

/**
 *@brief    restore device (MANU).
 *@return   0: success, negetive value: failure
 */
static inline int miio_system_restore(void)
{
    return mible_mesh_restore(1);
}

/**
 *@brief    set node rx scan window.
 *@param    [in] level : 0: OFF 1% window for receive iv, 1: LOW 15% window,
 *          2: NORMAL 20% window, 3: HIGH 30% window, 4: FULL 100% window
 *@return   0: success, negetive value: failure
 */
static inline int miio_mesh_set_scan_level(uint8_t level)
{
    return mible_mesh_device_scan_set(level);
}


/**
 *@brief    period send properties_changed.
 *@param    [in] siid: service id.
 *@param    [in] piid: property id.
 *@param    [in] period: time in s, 0: add to 3min circle pub,
 *          others: if period > 5min, send properties_changed to gateway every period
 *@return   0: success, negetive value: failure
 */
static inline int miio_mesh_properties_period_publish_init(uint16_t siid, uint16_t piid, uint32_t period)
{
    return mible_mesh_pub_add(siid, piid, period);
}

/**
 *@brief    send mesh properties_changed.
 *@param    [in] siid: service id.
 *@param    [in] piid: property id.
 *@param    [in] newValue: property value.
 *@return   0: success, negetive value: failure
 */
static inline int miio_mesh_properties_changed(uint16_t siid, uint16_t piid, mible_spec_property_value_t *newValue)
{
    return mible_mesh_send_property_changed(siid, piid, newValue);
}

/**
 *@brief    send mesh event occurred.
 *@param    [in] siid: service id.
 *@param    [in] piid: property id.
 *@param    [in] newArgs: event args.
 *@return   0: success, negetive value: failure
 */
static inline int miio_mesh_event_occurred(uint16_t siid, uint16_t eiid, mible_spec_arguments_t *newArgs)
{
    return mible_mesh_send_event_occurred(siid, eiid, newArgs);
}

/**
 *@brief    require service info: GMT offset, Weather or UTC time.
 *          callback event: on_property_set callback, data: int32.
 *@param    [in] type: 1: SERVER_GMT_OFFSET, 2: SERVER_WEATHER, 3: SERVER_UTC_TIME.
 *@return   0: success, negetive value: failure
 */
static inline int miio_mesh_request_property(uint8_t type)
{
    return mible_mesh_send_property_request(128, type);
}
#endif  //#if defined(CONFIG_MIBLE_PSK) && (CONFIG_MIBLE_PSK)

