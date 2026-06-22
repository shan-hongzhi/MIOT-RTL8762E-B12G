/**
*********************************************************************************************************
*               Copyright(c) 2018, Realtek Semiconductor Corporation. All rights reserved.
**********************************************************************************************************
* @file     main.c
* @brief    IIC EEPROM demo .
            This file provides demo code of i2c master.
            AT24C02 is slave.
* @details
* @author   echo
* @date     2021-05-27
* @version  v0.1
*********************************************************************************************************
*/

/* Includes ------------------------------------------------------------------*/
#include <stdlib.h>
#include <string.h>
#include "rtl876x_nvic.h"
#include "rtl876x_pinmux.h"
#include "rtl876x_rcc.h"
#include "rtl876x_i2c.h"
#include "trace.h"

/* Defines ------------------------------------------------------------------*/
#define I2C0_SCL_PIN               P4_0
#define I2C0_SDA_PIN               P4_1

/* Globals ------------------------------------------------------------------*/
#define TransferLength             10
uint8_t I2C_Rev_Data[255];

/**
  * @brief  Initialization of pinmux settings and pad settings.
  * @param  No parameter.
  * @return void
*/
void board_i2c_master_init(void)
{
    Pad_Config(I2C0_SCL_PIN, PAD_PINMUX_MODE, PAD_IS_PWRON, PAD_PULL_UP, PAD_OUT_ENABLE,
               PAD_OUT_HIGH);
    Pad_Config(I2C0_SDA_PIN, PAD_PINMUX_MODE, PAD_IS_PWRON, PAD_PULL_UP, PAD_OUT_ENABLE,
               PAD_OUT_HIGH);

    Pinmux_Config(I2C0_SCL_PIN, I2C0_CLK);
    Pinmux_Config(I2C0_SDA_PIN, I2C0_DAT);
}

/**
  * @brief  Initialize I2C Master peripheral.
  * @param  No parameter.
  * @return void
  */
void driver_i2c_master_init(void)
{
    /* Initialize I2C peripheral */
    RCC_PeriphClockCmd(APBPeriph_I2C0, APBPeriph_I2C0_CLOCK, ENABLE);

    I2C_InitTypeDef  I2C_InitStruct;
    I2C_StructInit(&I2C_InitStruct);

    I2C_InitStruct.I2C_ClockSpeed       = 100000;
    I2C_InitStruct.I2C_DeviveMode       = I2C_DeviveMode_Master;
    I2C_InitStruct.I2C_AddressMode      = I2C_AddressMode_7BIT;
    /* The address of AT24C02 is 0X50 */
    I2C_InitStruct.I2C_SlaveAddress     = 0x50;
    I2C_InitStruct.I2C_Ack              = I2C_Ack_Enable;

    I2C_Init(I2C0, &I2C_InitStruct);
    I2C_Cmd(I2C0, ENABLE);
}


void i2c_mem_write(I2C_TypeDef *I2Cx,  uint8_t mem_addr, uint16_t mem_addr_size,
                   const uint8_t *data, uint16_t data_size)
{
    uint16_t write_data_len =  mem_addr_size + data_size;
    uint8_t pWriteBuf[write_data_len];

    if (mem_addr_size == 1)
    {
        pWriteBuf[0] = mem_addr;
        uint32_t i = 1;
        while (data_size--)
        {
            pWriteBuf[i] = *data++;
            i++;
        }
    }
    else if (mem_addr_size == 2)
    {
        pWriteBuf[0] = mem_addr << 8;
        pWriteBuf[1] = (uint8_t)mem_addr & 0xFF;
        uint32_t i = 2;
        while (data_size--)
        {
            pWriteBuf[i] = *data++;
            i++;
        }
    }
    I2C_MasterWrite(I2Cx, pWriteBuf, write_data_len);
}


/**
  * @brief  Demo code of i2c.
  * @param  No parameter.
  * @return void
*/
void i2c_demo(void)
{
    /* Configure pad and pinmux firstly! */
    board_i2c_master_init();

    /* Initialize i2c peripheral */
    driver_i2c_master_init();

    /* I2C master write */
    uint8_t write_data[TransferLength] ;

    for (uint8_t i = 0; i < TransferLength; i++)
    {
        write_data[i] = i;
    }

    uint8_t mem_address = 0x20;

    i2c_mem_write(I2C0,  mem_address, 1, write_data, 10);

    for (uint32_t i = 0; i < 100000; i++);

    uint8_t read_data[TransferLength] = {0};

    I2C_RepeatRead(I2C0, (uint8_t *)&mem_address, 1, read_data, 10);

//    /* Only for Debug */
//    for (uint32_t i = 0; i < 50 ; i++)
//    {
//        DBG_DIRECT("Master Read data = %d\n", read_data[i]);
//    }
}

/**
  * @brief    Entry of app code
  * @return   int (To avoid compile warning)
  */
int main(void)
{
    extern uint32_t random_seed_value;
    srand(random_seed_value);
    __enable_irq();
    i2c_demo();

    while (1)
    {
        __nop();
        __nop();
        __nop();
        __nop();
        __nop();
        __nop();
    }
}

/******************* (C) COPYRIGHT 2018 Realtek Semiconductor Corporation *****END OF FILE****/
