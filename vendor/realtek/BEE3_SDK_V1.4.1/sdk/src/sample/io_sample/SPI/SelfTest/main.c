/**
*********************************************************************************************************
*               Copyright(c) 2018, Realtek Semiconductor Corporation. All rights reserved.
**********************************************************************************************************
* @file     main.c
* @brief    uart demo polling tx and rx.
* @details
* @author   echo
* @date     2021-05-25
* @version  v0.1
*********************************************************************************************************
*/

/* Includes ------------------------------------------------------------------*/
#include <stdlib.h>
#include <string.h>
#include "rtl876x_nvic.h"
#include "rtl876x_pinmux.h"
#include "rtl876x_rcc.h"
#include "rtl876x_spi.h"
#include "board.h"
#include "trace.h"

/**
  * @brief  Initialization of pinmux settings and pad settings.
  * @param  No parameter.
  * @return void
*/
void board_spi_master_init(void)
{
    Pad_Config(SPI_MASTER_SCK_PIN, PAD_PINMUX_MODE, PAD_IS_PWRON, PAD_PULL_NONE, PAD_OUT_ENABLE,
               PAD_OUT_HIGH);
    Pad_Config(SPI_MASTER_MOSI_PIN, PAD_PINMUX_MODE, PAD_IS_PWRON, PAD_PULL_NONE, PAD_OUT_ENABLE,
               PAD_OUT_HIGH);
    Pad_Config(SPI_MASTER_MISO_PIN, PAD_PINMUX_MODE, PAD_IS_PWRON, PAD_PULL_NONE, PAD_OUT_ENABLE,
               PAD_OUT_HIGH);
    Pad_Config(SPI_MASTER_CS_PIN, PAD_PINMUX_MODE, PAD_IS_PWRON, PAD_PULL_NONE, PAD_OUT_ENABLE,
               PAD_OUT_HIGH);

    Pinmux_Config(SPI_MASTER_SCK_PIN, SPI1_CLK_MASTER);
    Pinmux_Config(SPI_MASTER_MOSI_PIN, SPI1_MO_MASTER);
    Pinmux_Config(SPI_MASTER_MISO_PIN, SPI1_MI_MASTER);
    Pinmux_Config(SPI_MASTER_CS_PIN, SPI1_SS_N_0_MASTER);
}

/**
  * @brief  Initialization of pinmux settings and pad settings.
  * @param  No parameter.
  * @return void
*/
void board_spi_slave_init(void)
{
    Pad_Config(SPI_SLAVE_SCK_PIN, PAD_PINMUX_MODE, PAD_IS_PWRON, PAD_PULL_NONE, PAD_OUT_ENABLE,
               PAD_OUT_HIGH);
    Pad_Config(SPI_SLAVE_MOSI_PIN, PAD_PINMUX_MODE, PAD_IS_PWRON, PAD_PULL_NONE, PAD_OUT_ENABLE,
               PAD_OUT_HIGH);
    Pad_Config(SPI_SLAVE_MISO_PIN, PAD_PINMUX_MODE, PAD_IS_PWRON, PAD_PULL_NONE, PAD_OUT_ENABLE,
               PAD_OUT_HIGH);
    Pad_Config(SPI_SLAVE_CS_PIN, PAD_PINMUX_MODE, PAD_IS_PWRON, PAD_PULL_NONE, PAD_OUT_ENABLE,
               PAD_OUT_HIGH);

    Pinmux_Config(SPI_SLAVE_SCK_PIN, SPI0_CLK_SLAVE);
    Pinmux_Config(SPI_SLAVE_MOSI_PIN, SPI0_SI_SLAVE);
    Pinmux_Config(SPI_SLAVE_MISO_PIN, SPI0_SO_SLAVE);
    Pinmux_Config(SPI_SLAVE_CS_PIN, SPI0_SS_N_0_SLAVE);
}

/**
  * @brief  Initialize SPI Master peripheral.
  * @param  No parameter.
  * @return void
*/
void driver_spi_master_init(void)
{
    SPI_DeInit(SPI_MASTER);

    RCC_PeriphClockCmd(APBPeriph_SPI1, APBPeriph_SPI1_CLOCK, ENABLE);

    SPI_InitTypeDef  SPI_InitStructure;
    SPI_StructInit(&SPI_InitStructure);

    SPI_InitStructure.SPI_Direction   = SPI_Direction_FullDuplex;
    SPI_InitStructure.SPI_Mode        = SPI_Mode_Master;
    SPI_InitStructure.SPI_DataSize    = SPI_DataSize_8b;
    SPI_InitStructure.SPI_CPOL        = SPI_CPOL_High;
    SPI_InitStructure.SPI_CPHA        = SPI_CPHA_1Edge;
    SPI_InitStructure.SPI_BaudRatePrescaler  = 4;
    SPI_InitStructure.SPI_TxThresholdLevel  = 10;    /* Transmit FIFO Threshold */
    SPI_InitStructure.SPI_RxThresholdLevel  =
        0;     /* cause SPI_INT_RXF interrupt if data length in receive FIFO  >= SPI_RxThresholdLevel + 1*/
    SPI_InitStructure.SPI_FrameFormat = SPI_Frame_Motorola;

    SPI_Init(SPI_MASTER, &SPI_InitStructure);
    SPI_Cmd(SPI_MASTER, ENABLE);
}

/**
  * @brief  Initialize SPI Slave peripheral.
  * @param  No parameter.
  * @return void
*/
void driver_spi_slave_init(void)
{
    SPI_DeInit(SPI_SLAVE);

    RCC_PeriphClockCmd(APBPeriph_SPI0, APBPeriph_SPI0_CLOCK, ENABLE);

    SPI_InitTypeDef  SPI_InitStructure;
    SPI_StructInit(&SPI_InitStructure);

    SPI_InitStructure.SPI_Direction   = SPI_Direction_FullDuplex;
    SPI_InitStructure.SPI_Mode        = SPI_Mode_Slave;
    SPI_InitStructure.SPI_DataSize    = SPI_DataSize_8b;
    SPI_InitStructure.SPI_CPOL        = SPI_CPOL_High;
    SPI_InitStructure.SPI_CPHA        = SPI_CPHA_1Edge;
    SPI_InitStructure.SPI_BaudRatePrescaler  = 4;
    SPI_InitStructure.SPI_TxThresholdLevel  = 2;    /* Transmit FIFO Threshold */
    SPI_InitStructure.SPI_RxThresholdLevel  =
        2;     /* cause SPI_INT_RXF interrupt if data length in receive FIFO  >= SPI_RxThresholdLevel + 1*/
    SPI_InitStructure.SPI_FrameFormat = SPI_Frame_Motorola;

    SPI_Init(SPI_SLAVE, &SPI_InitStructure);
    SPI_Cmd(SPI_SLAVE, ENABLE);

    /* detect receive data */
    SPI_INTConfig(SPI_SLAVE, SPI_INT_RXF, ENABLE);
    /* Config SPI interrupt */
    NVIC_InitTypeDef NVIC_InitStruct;
    NVIC_InitStruct.NVIC_IRQChannel = SPI0_IRQn;
    NVIC_InitStruct.NVIC_IRQChannelPriority = 1;
    NVIC_InitStruct.NVIC_IRQChannelCmd = ENABLE;
    NVIC_Init(&NVIC_InitStruct);
}

void spi_transfer_demo(void)
{
    uint8_t spiWriteBuf[16] = {0x00, 0x01, 0x02, 0x03, 0x04 };

    SPI_SendBuffer(SPI_MASTER, spiWriteBuf, 5);
}

/**
  * @brief  Demo code of uart.
  * @param  No parameter.
  * @return void
*/
void spi_demo(void)
{
    board_spi_master_init();
    board_spi_slave_init();
    driver_spi_master_init();
    driver_spi_slave_init();

    /* SPI Transmission demo */
    spi_transfer_demo();

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

/**
  * @brief    Entry of app code
  * @return   int (To avoid compile warning)
  */
int main(void)
{
    extern uint32_t random_seed_value;
    srand(random_seed_value);
    __enable_irq();
    spi_demo();

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

void SPI0_Handler(void)
{
    DBG_DIRECT("In SPI Handler");
    uint8_t len = 0;
    uint8_t idx = 0;
    uint8_t SPI_ReadINTBuf[16] = {0, 0, 0, 0};

    if (SPI_GetINTStatus(SPI_SLAVE, SPI_INT_RXF) == SET)
    {
        len = SPI_GetRxFIFOLen(SPI_SLAVE);
        for (idx = 0; idx < len; idx++)
        {
            /* must read all data in receive FIFO , otherwise cause SPI_INT_RXF interrupt again */
            SPI_ReadINTBuf[idx] = SPI_ReceiveData(SPI_SLAVE);
            DBG_DIRECT("SPI_ReadINTBuf[idx]=%d\n", SPI_ReadINTBuf[idx]);
        }
    }

}




/******************* (C) COPYRIGHT 2018 Realtek Semiconductor Corporation *****END OF FILE****/
