#ifndef MIBLE_MCU_H_
#define MIBLE_MCU_H_
#include <stdint.h>
#include "mible.h"
#include <nuttx/config.h>

#if defined(CONFIG_MIBLE_USE_MCU_OTA)
typedef enum {
    MIBLE_MCU_GET_VERSION,
    MIBLE_MCU_READ_DFUINFO,
    MIBLE_MCU_WRITE_DFUINFO,
    MIBLE_MCU_WRITE_FIRMWARE,
    MIBLE_MCU_VERIFY_FIRMWARE,
    MIBLE_MCU_UPGRADE_FIRMWARE,
    MIBLE_MCU_TRANSFER,
} mible_mcu_cmd_t;

typedef struct {
    uint32_t address;
    uint32_t length;
    void *p_data;
} mible_mcu_nvminfo_t;

mible_status_t mible_mcu_send_get_info(uint32_t *delay);
mible_status_t mible_mcu_read_get_info(uint8_t *buf);
void mible_mcu_read_dfuinfo(uint8_t *m_dfu_info, uint8_t len);
mible_status_t mible_mcu_write_dfuinfo(uint8_t *m_dfu_info, uint8_t len);
mible_status_t mible_mcu_upgrade_firmware(void);
mible_status_t mible_mcu_nvm_write(void *p_data, uint32_t length, uint32_t address);
mible_status_t mible_mcu_verify_firmware(void);
mible_status_t mible_mcu_cmd_send(mible_mcu_cmd_t cmd, void *arg);
mible_status_t mible_mcu_cmd_wait(mible_mcu_cmd_t cmd, void *arg);

int mible_mcu_init(void);
void mible_mcu_deinit(void);
mible_status_t mible_mcu_send_buffer(const uint8_t *pSend_Buf, uint16_t vCount);
void mible_mcu_send_byte(uint8_t byte);
int mible_mcu_flushinput(void);
mible_status_t mible_mcu_recv_bytes(void *bytes, uint16_t req_count, uint32_t timeout);
int mible_mcu_uart_recv_data(uint8_t *buf, size_t count,uint32_t timeout);
int mible_mcu_uart_send_data(uint8_t *buf, size_t count);
#endif
#endif /* MIBLE_MCU_H_ */
