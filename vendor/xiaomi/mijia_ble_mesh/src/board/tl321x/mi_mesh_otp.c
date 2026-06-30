#include <zephyr/types.h>

#include <stdio.h>
#include <string.h>
#include <stddef.h>
#include "mi_mesh_otp.h"

#if defined(MI_LOG_ENABLED) && (MI_LOG_ENABLED)
#include <sys/printk.h>
#else
#undef printk
#define printk(fmt, ...)
#endif

#if defined(CONFIG_MIBLE_POTP)
#include "mi_potp_0xec4d3ebdd2c1.c"
extern const unsigned char mi_potp[];

int mible_otp_read(void *dst, size_t len, const uint32_t src_addr)
{
    (void)memcpy(dst, mi_potp + src_addr, len);
    return 0;
}
#else
extern int mible_otp_read(void * p_data, uint32_t length, uint32_t address);
#endif

#define MI_OTP_READ         mible_otp_read

/* old addr*/
// #define ROOT_CERT_ADDR      0x0400
// #define DEV_CERT_ADDR       0x0049
/* new addr*/
#define ROOT_CERT_ADDR      0x0049
#define MANU_CERT_ADDR      0x0200
#define DEV_CERT_ADDR       0x0400
#define DEV_INFO_ADDR       0x0000
#define DEV_DCOC_OFFSET     0x0000
#define DEV_RFPARAM_OFFSET  0x0002
#define DEV_PSK_OFFSET      0x0003
#define DEV_PRIKEY_OFFSET   0x0025
#define DEV_PSK_LEN         34
#define DEV_PRIKEY_LEN      36

int mi_mesh_otp_read(uint16_t item_type, uint8_t *p_out, uint16_t len)
{
    int err;
    uint16_t otp_len;

    if(p_out == NULL)
        return -1;

    // find item bt type
    switch(item_type){
    case OTP_DEV_CERT:
        MI_OTP_READ(&otp_len, sizeof(otp_len), DEV_CERT_ADDR);
        if (otp_len > len)
            return -2;
        MI_OTP_READ(p_out, otp_len, DEV_CERT_ADDR+sizeof(otp_len)); 
        break;
    case OTP_MANU_CERT:
        MI_OTP_READ(&otp_len, sizeof(otp_len), MANU_CERT_ADDR);
        if (otp_len > len)
            return -2;
        MI_OTP_READ(p_out, otp_len, MANU_CERT_ADDR+sizeof(otp_len));
        break;
    case OTP_ROOT_CERT:
        MI_OTP_READ(&otp_len, sizeof(otp_len), ROOT_CERT_ADDR);
        if (otp_len > len)
            return -2;
        MI_OTP_READ(p_out, otp_len, ROOT_CERT_ADDR+sizeof(otp_len));
        break;
    case OTP_DEV_CERT_PRI:
        otp_len = DEV_PRIKEY_LEN;
        if (otp_len > len)
            return -2;
        MI_OTP_READ(p_out, otp_len, DEV_INFO_ADDR+DEV_PRIKEY_OFFSET);
        break;
    case OTP_DEV_PSK:
        otp_len = DEV_PSK_LEN;
        if (otp_len > len)
            return -2;
        MI_OTP_READ(p_out, otp_len, DEV_INFO_ADDR+DEV_PSK_OFFSET);
        break;
    }

    return otp_len;
}
