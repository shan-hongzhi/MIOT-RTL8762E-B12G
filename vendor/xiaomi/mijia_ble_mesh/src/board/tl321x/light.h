#ifndef LIGHT_H_
#define LIGHT_H_

#include <stddef.h>
#include <string.h>
#include <stdbool.h>

#define DEFAULT_LIGHTNESS           0

#define LED_PWM_FREQ                10000 //!< 10KHz
#define LED_PWM_COUNT               (40000000/LED_PWM_FREQ) // 4000
/* invalid pin */
#define PIN_INVALID                 0xff

typedef struct
{
    int pin_num;
    uint8_t pwm_id;
    bool pin_high_on;
    uint16_t lightness;
    uint16_t lightness_last;
} light_t;

/**
 * @defgroup Light_Controller_Exported_Types Light Controller Exported Types
 * @brief
 * @{
 */
typedef void (*light_change_done_cb)(light_t *light);

/**
 * @brief config light pin
 * @param[in] light: light handle
 */
void light_pin_config(const light_t *light);

/**
 * @brief set light lightness
 * @param[in] light: light handle
 * @param[in] lightness: light lightness
 * @note update lightness file only
 */
extern void light_lighten(light_t *light, uint16_t lightness);

/**
 * @brief set lightness gradual change
 * @param[in] light: light channel
 * @param[in] lightness: target lightness
 * @param[in] time: gradual change time
 * @param[in] cb: light change done callback function
 */
void light_set_lightness_linear(light_t *light, uint16_t lightness, uint32_t time,
                                light_change_done_cb pcb);

/**
 * @brief breath light
 * @param[in] light: light channel
 * @param[in] lightness_begin: breath start lightness
 * @param[in] lightness_end: breath end lightness
 * @param[in] interval: breath interval, the unit is ms
 * @param[in] forward_duty: start lightness duty in total interval, value range is 0-100
 *            for example, intreval is 1000ms, duty is 60, then start lightness will lighten
 *            600ms, end lightness will lighten 400ms
 * @param[in] times: breath times
 * @param[in] half_breath_end: whether the last breath is half or not, if TURE, lightness will stay
 *            on lightness_end, otherwise lightness till stay on lightness_begin
 * @param[in] cb: light change done callback function
 */
void light_breath(light_t *light, uint16_t lightness_begin, uint16_t lightness_end,
                  uint32_t interval, uint8_t forward_duty, uint32_t times, bool half_breath_end,
                  light_change_done_cb pcb);

/**
 * @brief initialize light controller
 * @retval TRUE: initialize success
 * @retval FALSE: initialize failed
 */
bool light_controller_init(void);

/**
 * @brief initialize light driver common part
 */
void light_driver_init(void);

#endif //LIGHT_H_
