/*
 * app_main.h
 *
 *  Created on: 2020��12��12��
 *      Author: mi
 */

#ifndef __MAIN_H__
#define __MAIN_H__

#include <stdbool.h>
#include <stdint.h>
#include <string.h>


typedef struct {
    bool        light_on;
    uint16_t    lightness;
    uint16_t    lighttemp;
    uint16_t    lightness_last;
    uint16_t    lighttemp_last;
    int32_t     gmt_offset;
    int32_t     weather;
    uint32_t    utc_time;
}light_demo_t;

extern light_demo_t xiaomi_light;

typedef struct {
    bool        left_on;
    uint8_t     left_mode;
    uint8_t     fault;
    bool        anti_flk;
    bool        right_on;
    uint8_t     right_mode;
    float       power_consumption;
    uint16_t    power_electric;
    bool        accumulation;
    int32_t     gmt_offset;
    int32_t     weather;
    uint32_t    utc_time;
}switch_demo_t;

extern switch_demo_t two_key_switch;

#endif
