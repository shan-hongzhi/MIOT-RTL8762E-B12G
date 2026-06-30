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

#ifndef CEIL_DIV
#define CEIL_DIV(A, B)      (((A) + (B) - 1) / (B))
#endif

static uint8_t *find_item(uint8_t *p_base, uint16_t item_type)
{
    /* Check POTP header */
    otp_head_t head = {0};
    MI_OTP_READ(&head, sizeof(head), (uint32_t)p_base);
    if (memcmp(head.name, "POTP", 4) != 0) {
        printk("no mesh otp found.\n");
        return NULL;
    } else if (head.version > 1) {
        printk("this version is not supported.\n");
        return NULL;
    }

    /* Find TYPE item */
    otp_item_t item;
    uint8_t* item_addr = p_base + sizeof(otp_head_t);
    MI_OTP_READ(&item, 4, (uint32_t)item_addr);
    while(item.type != item_type && (uint32_t)item_addr < 4096) {
        item_addr += offsetof(otp_item_t, value) + CEIL_DIV(item.len, 4) * 4;
        MI_OTP_READ(&item, 4, (uint32_t)item_addr);
    }

    if ((uint32_t)item_addr > 4096)
        return NULL;
    else
        return item_addr;
}

int mi_mesh_otp_read(uint16_t item_type, uint8_t *p_out, uint16_t len)
{
    uint8_t *p = find_item(0, item_type);

    if (p == NULL)
        return -1;

    otp_item_t item;
    MI_OTP_READ(&item, sizeof(otp_item_t), (uint32_t)p);

    if (p_out != NULL && item.len > len)
        return -2;

    if (p_out != NULL) {
        p += offsetof(otp_item_t, value);
        MI_OTP_READ(p_out, item.len, (uint32_t)p);
    }

    return item.len;

}
