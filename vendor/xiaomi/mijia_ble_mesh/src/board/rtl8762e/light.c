#include <sys/ioctl.h>
#include <nuttx/timers/pwm.h>

#include "light.h"
#include "miio_user_api.h"

#include "rtl876x_rcc.h"
#include "rtl876x_pinmux.h"
#include "rtl876x_tim.h"

#if defined(MI_LOG_ENABLED) && (MI_LOG_ENABLED)
#include <sys/printk.h>
#else
#undef printk
#define printk(fmt, ...)
#endif

#define TIME_INFINITE               0xFFFFFFFF
/* timer interval, minimum value is 10ms */
#define LIGHT_MONITOR_INTERVAL      20
#define MAX_ACTION_NUM              5

static void *light_ctl_timer = NULL;

typedef struct
{
    uint16_t current_lightness;
    uint16_t target_lightness;
    uint8_t steps;
    int32_t step_delta;
} light_lightness_action_t;

typedef struct
{
    uint16_t lightness_begin;
    uint16_t lightness_end;
    bool in_begin_phase;
    uint32_t begin_count;
    uint32_t begin_cur_count;
    uint32_t end_count;
    uint32_t end_cur_count;
    uint32_t times;
} light_blink_action_t;

typedef struct
{
    uint16_t lightness_begin;
    uint16_t lightness_end;
    uint16_t current_lightness;
    bool in_forward_phase;
    uint8_t current_forward_steps;
    uint8_t forward_steps;
    int32_t forward_step_delta;
    uint8_t current_reverse_steps;
    uint8_t reverse_steps;
    int32_t reverse_step_delta;
    uint32_t times;
    bool half_breath_end;
} light_breath_action_t;

typedef enum
{
    LIGHT_ACTION_LIGHTNESS_LINEAR,
    LIGHT_ACTION_BLINK,
    LIGHT_ACTION_BREATH,
    LIGHT_ACTION_UNKNOWN,
} light_action_type_t;

typedef union
{
    light_lightness_action_t lightness;
    light_blink_action_t blink;
    light_breath_action_t breath;
} light_action_value_t;

typedef struct
{
    bool busy;
    bool need_change;
    light_t *light;
    light_change_done_cb change_done;
    light_action_type_t type;
    light_action_value_t value;
} light_action_t;

static light_action_t light_actions[MAX_ACTION_NUM];

void light_pin_config(const light_t *light)
{
    if (PIN_INVALID == light->pin_num)
    {
        return ;
    }
    /* pad & pinmux */
    Pad_Config(light->pin_num, PAD_PINMUX_MODE, PAD_IS_PWRON, PAD_PULL_NONE, PAD_OUT_ENABLE,
               PAD_OUT_LOW);
    Pinmux_Config(light->pin_num, light->pin_func);
    /* TIM */
    TIM_TimeBaseInitTypeDef TIM_InitStruct;
    TIM_StructInit(&TIM_InitStruct);
    TIM_InitStruct.TIM_PWM_En = PWM_ENABLE;
    /*<! PWM output freqency = 40M/(TIM_PWM_High_Count + TIM_PWM_Low_Count) */
    /*<! PWM duty cycle = TIM_PWM_High_Count/(TIM_PWM_High_Count + TIM_PWM_Low_Count) */
    uint32_t high_count;
    if (0xffff == light->lightness)
    {
        high_count = LED_PWM_COUNT;
    }
    else
    {
        high_count = (LED_PWM_COUNT / 65535.0) * light->lightness;
    }

    if (light->pin_high_on)
    {
        TIM_InitStruct.TIM_PWM_High_Count = high_count;
        TIM_InitStruct.TIM_PWM_Low_Count = LED_PWM_COUNT - high_count;
    }
    else
    {
        TIM_InitStruct.TIM_PWM_High_Count = LED_PWM_COUNT - high_count;
        TIM_InitStruct.TIM_PWM_Low_Count = high_count;
    }

    TIM_InitStruct.TIM_Mode = TIM_Mode_UserDefine;
    TIM_InitStruct.TIM_SOURCE_DIV = TIM_CLOCK_DIVIDER_1;
    TIM_TimeBaseInit(light->tim_id, &TIM_InitStruct);
    /* Enable PWM output */
    TIM_Cmd(light->tim_id, ENABLE);
}

void light_lighten(light_t *light, uint16_t lightness)
{
    if (PIN_INVALID == light->pin_num)
    {
        return ;
    }

    uint32_t high_count;
    if (0xffff == lightness)
    {
        high_count = LED_PWM_COUNT;
    }
    else
    {
        high_count = (LED_PWM_COUNT / 65535.0) * lightness;
    }

    if (light->pin_high_on)
    {
        TIM_PWMChangeFreqAndDuty(light->tim_id, high_count, LED_PWM_COUNT - high_count);
    }
    else
    {
        TIM_PWMChangeFreqAndDuty(light->tim_id, LED_PWM_COUNT - high_count, high_count);
    }

    light->lightness = lightness;
}

static light_action_t *request_action(light_t *light)
{
    /* find same first */
    for (uint8_t i = 0; i < MAX_ACTION_NUM; ++i)
    {
        if ((light == light_actions[i].light) && (light_actions[i].busy))
        {
            return &light_actions[i];
        }
    }

    /* no same, find empty */
    for (uint8_t i = 0; i < MAX_ACTION_NUM; ++i)
    {
        if (!light_actions[i].busy)
        {
            light_actions[i].busy = true;
            return &light_actions[i];
        }
    }

    return NULL;
}

static void release_action(light_action_t *action)
{
    action->busy = false;
    printk("release_action pin_num %d, cd %p\n", action->light->pin_num, action->change_done);
    if (NULL != action->change_done)
    {
        action->change_done(action->light);
    }
}

static bool is_all_light_idle(void)
{
    for (uint8_t channel = 0; channel < MAX_ACTION_NUM; ++channel)
    {
        if (light_actions[channel].busy && light_actions[channel].need_change)
        {
            return false;
        }
    }
    return true;
}

static void light_controller_timer_start(void)
{
    if (NULL != light_ctl_timer)
    {
        int ret = miio_timer_start(light_ctl_timer, LIGHT_MONITOR_INTERVAL, NULL);
        if(ret){
            printk("light_controller_timer_start fail\n");
        }
    }
}

static void light_ctl_timeout_handle(void *pargs)
{
    for (uint8_t channel = 0; channel < MAX_ACTION_NUM; ++channel) 
    {
        if (light_actions[channel].busy && light_actions[channel].need_change)
        {
            switch (light_actions[channel].type)
            {
            case LIGHT_ACTION_LIGHTNESS_LINEAR:
                if (light_actions[channel].value.lightness.steps > 1)
                {
                    light_lighten(light_actions[channel].light,
                                  light_actions[channel].value.lightness.current_lightness);
                    light_actions[channel].value.lightness.current_lightness +=
                        light_actions[channel].value.lightness.step_delta;
                    light_actions[channel].value.lightness.steps --;
                }
                else
                {
                    light_lighten(light_actions[channel].light,
                                  light_actions[channel].value.lightness.target_lightness);
                    if (light_actions[channel].value.lightness.target_lightness > 0)
                    {
                        light_actions[channel].light->lightness_last =
                            light_actions[channel].value.lightness.target_lightness;
                    }

                    light_actions[channel].need_change = false;
                    release_action(&light_actions[channel]);
                }
                break;
            case LIGHT_ACTION_BLINK:
                if (light_actions[channel].value.blink.times > 0)
                {
                    if (light_actions[channel].value.blink.in_begin_phase)
                    {
                        if (light_actions[channel].value.blink.begin_cur_count >=
                            light_actions[channel].value.blink.begin_count)
                        {
                            light_actions[channel].value.blink.in_begin_phase = false;
                            light_actions[channel].value.blink.begin_cur_count = 0;
                        }
                        else
                        {
                            if (0 == light_actions[channel].value.blink.begin_cur_count)
                            {
                                light_lighten(light_actions[channel].light,
                                              light_actions[channel].value.blink.lightness_begin);
                            }
                            light_actions[channel].value.blink.begin_cur_count ++;
                        }
                    }
                    else
                    {
                        if (light_actions[channel].value.blink.end_cur_count >=
                            light_actions[channel].value.blink.end_count)
                        {
                            light_actions[channel].value.blink.in_begin_phase = true;
                            light_actions[channel].value.blink.end_cur_count = 0;

                            if (TIME_INFINITE != light_actions[channel].value.blink.times)
                            {
                                light_actions[channel].value.blink.times --;
                            }
                        }
                        else
                        {
                            if (0 == light_actions[channel].value.blink.end_cur_count)
                            {
                                light_lighten(light_actions[channel].light, light_actions[channel].value.blink.lightness_end);
                            }
                            light_actions[channel].value.blink.end_cur_count ++;
                        }
                    }
                }
                else
                {
                    light_actions[channel].need_change = false;
                    release_action(&light_actions[channel]);
                }
                break;
            case LIGHT_ACTION_BREATH:
                if (light_actions[channel].value.breath.times > 0)
                {
                    if (light_actions[channel].value.breath.in_forward_phase)
                    {
                        if (light_actions[channel].value.breath.current_forward_steps >=
                            light_actions[channel].value.breath.forward_steps)
                        {
                            light_lighten(light_actions[channel].light,
                                          light_actions[channel].value.breath.lightness_end);
                            light_actions[channel].value.breath.current_lightness =
                                light_actions[channel].value.breath.lightness_end;
                            light_actions[channel].value.breath.current_reverse_steps = 0;
                            light_actions[channel].value.breath.in_forward_phase = false;
                            if (1 == light_actions[channel].value.breath.times)
                            {
                                if (light_actions[channel].value.breath.half_breath_end)
                                {
                                    light_actions[channel].value.breath.times = 0;
                                    light_actions[channel].need_change = false;
                                    release_action(&light_actions[channel]);
                                }
                            }
                        }
                        else
                        {
                            light_lighten(light_actions[channel].light,
                                          light_actions[channel].value.breath.current_lightness);
                            light_actions[channel].value.breath.current_lightness +=
                                light_actions[channel].value.breath.forward_step_delta;
                            light_actions[channel].value.breath.current_forward_steps ++;
                        }
                    }
                    else
                    {
                        if (light_actions[channel].value.breath.current_reverse_steps >=
                            light_actions[channel].value.breath.reverse_steps)
                        {
                            light_lighten(light_actions[channel].light,
                                          light_actions[channel].value.breath.lightness_begin);
                            light_actions[channel].value.breath.current_lightness =
                                light_actions[channel].value.breath.lightness_begin;
                            light_actions[channel].value.breath.current_forward_steps = 0;
                            light_actions[channel].value.breath.in_forward_phase = true;

                            if (TIME_INFINITE != light_actions[channel].value.breath.times)
                            {
                                light_actions[channel].value.breath.times --;
                            }
                        }
                        else
                        {
                            light_lighten(light_actions[channel].light,
                                          light_actions[channel].value.breath.current_lightness);
                            light_actions[channel].value.breath.current_lightness +=
                                light_actions[channel].value.breath.reverse_step_delta;
                            light_actions[channel].value.breath.current_reverse_steps ++;
                        }
                    }
                }
                else
                {
                    light_actions[channel].need_change = false;
                    release_action(&light_actions[channel]);
                }
                break;
            default:
                break;
            }
        }
    }

    /* Check light controler status */
    if (is_all_light_idle())
    {
        miio_timer_stop(light_ctl_timer);
    }
}

void light_set_lightness_linear(light_t *light, uint16_t lightness, uint32_t time,
                                light_change_done_cb pcb)
{
    light_action_t *paction = request_action(light);

    if (NULL != paction)
    {
        paction->light = light;
        paction->type = LIGHT_ACTION_LIGHTNESS_LINEAR;
        paction->change_done = pcb;
        paction->value.lightness.current_lightness = light->lightness;
        paction->value.lightness.target_lightness = lightness;
        paction->value.lightness.steps = time / LIGHT_MONITOR_INTERVAL;
        if (0 == paction->value.lightness.steps)
        {
            paction->value.lightness.steps = 1;
        }
        paction->value.lightness.step_delta = (int32_t)(
                                                  paction->value.lightness.target_lightness -
                                                  paction->value.lightness.current_lightness) / paction->value.lightness.steps;
        paction->need_change = true;

        printk("set_lightness_linear step %d, delta %ld, curr %d, tar %d\n", paction->value.lightness.steps, 
            paction->value.lightness.step_delta, light->lightness, lightness);
        light_controller_timer_start();
    }
}

void light_breath(light_t *light, uint16_t lightness_begin, uint16_t lightness_end,
                  uint32_t interval, uint8_t forward_duty, uint32_t times, bool half_breath_end,
                  light_change_done_cb pcb)
{
    if (forward_duty > 100)
    {
        forward_duty = 100;
    }

    light_action_t *paction = request_action(light);
    if (NULL != paction)
    {
        uint32_t total_steps = interval / LIGHT_MONITOR_INTERVAL;
        if (0 == total_steps)
        {
            total_steps = 1;
        }
        paction->light = light;
        paction->type = LIGHT_ACTION_BREATH;
        paction->change_done = pcb;
        paction->value.breath.lightness_begin = lightness_begin;
        paction->value.breath.lightness_end = lightness_end;
        paction->value.breath.current_lightness = lightness_begin;
        paction->value.breath.in_forward_phase = true;
        paction->value.breath.current_forward_steps = 0;
        paction->value.breath.forward_steps = total_steps * forward_duty / 100;
        paction->value.breath.current_reverse_steps = 0;
        paction->value.breath.reverse_steps = total_steps -
                                              paction->value.breath.forward_steps;
        if(paction->value.breath.forward_steps == 0)
            paction->value.breath.forward_steps = 1;
        if(paction->value.breath.reverse_steps == 0)
            paction->value.breath.reverse_steps = 1;

        paction->value.breath.forward_step_delta = (int32_t)(lightness_end - lightness_begin) /
                                                   paction->value.breath.forward_steps;
        paction->value.breath.reverse_step_delta = (int32_t)(lightness_begin - lightness_end) /
                                                   paction->value.breath.reverse_steps;
        paction->value.breath.times = times;
        paction->value.breath.half_breath_end = half_breath_end;
        paction->need_change = true;

        light_controller_timer_start();
    }
}

bool light_controller_init(void)
{
    if (NULL != light_ctl_timer){
        printk("light_ctl_timer already create\n");
        return true;
    }else{
        int ret = miio_timer_create(&light_ctl_timer,
                light_ctl_timeout_handle,
                MIBLE_TIMER_REPEATED);
        if (ret) {
            printk("light_ctl_timer create fail\n");
            return false;
        }

        ret = miio_timer_start(light_ctl_timer, LIGHT_MONITOR_INTERVAL, NULL);
        if (ret) {
            printk("light_ctl_timer start fail\n");
            return false;
        }
    }
    printk("light_ctl_timer start succ\n");

    return true;
}

void light_driver_init(void)
{
    /* turn on timer clock */
    RCC_PeriphClockCmd(APBPeriph_TIMER, APBPeriph_TIMER_CLOCK, ENABLE);
    *((volatile uint32_t *)0x40000360UL) &= ~(1 << 10);
}
