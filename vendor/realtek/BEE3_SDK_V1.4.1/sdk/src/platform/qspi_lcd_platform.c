#include "qspi_lcd_platform.h"
#include "rtl876x.h"
#include "rtl876x_rcc.h"
#include "rtl876x_pinmux.h"

void qspi_lcd_init(void)
{
    /* module init */
    RCC_PeriphClockCmd(APBPeriph_FLASH2, APBPeriph_FLASH2_CLOCK, (FunctionalState)ENABLE);

    Pinmux_Config(P4_3, IDLE_MODE);
    Pinmux_Config(P4_0, IDLE_MODE);
    Pinmux_Config(P4_2, IDLE_MODE);
    Pinmux_Config(P4_1, IDLE_MODE);
    Pinmux_Config(P0_7, IDLE_MODE);
    Pinmux_Config(P0_6, IDLE_MODE);

    Pad_Config(P4_3, PAD_PINMUX_MODE, PAD_IS_PWRON, PAD_PULL_UP,
               PAD_OUT_DISABLE, PAD_OUT_LOW);
    Pad_Config(P4_1, PAD_PINMUX_MODE, PAD_IS_PWRON, PAD_PULL_NONE,
               PAD_OUT_DISABLE, PAD_OUT_LOW);
    Pad_Config(P0_7, PAD_PINMUX_MODE, PAD_IS_PWRON, PAD_PULL_NONE,
               PAD_OUT_DISABLE, PAD_OUT_LOW);
    Pad_Config(P4_2, PAD_PINMUX_MODE, PAD_IS_PWRON, PAD_PULL_NONE,
               PAD_OUT_DISABLE, PAD_OUT_LOW);
    Pad_Config(P4_0, PAD_PINMUX_MODE, PAD_IS_PWRON, PAD_PULL_NONE,
               PAD_OUT_DISABLE, PAD_OUT_LOW);
    Pad_Config(P0_6, PAD_PINMUX_MODE, PAD_IS_PWRON, PAD_PULL_NONE,
               PAD_OUT_DISABLE, PAD_OUT_LOW);

    /* spic2 enable controller */
    SYSBLKCTRL->u_210.BITS_210.BIT_SOC_FLASH_EN2 = 1;
    /* spic2 master enable */
    SYSBLKCTRL->u_2A8.BITS_2A8.SPIC2_MASTER_EN = 1;
    /* spic2 write enable default (Jimmy) */

    /* spic1 & spic2 share clock source enable bit */
    HAL_WRITE32(PERIPH_REG_BASE, 0x2D0, HAL_READ32(PERIPH_REG_BASE, 0x2D0) | BIT5);

    /* set cs and baudrate */
    SPIC2->ssienr = 0;
    SPIC2->ser = BIT0;
    SPIC2->baudr = 1;
}


