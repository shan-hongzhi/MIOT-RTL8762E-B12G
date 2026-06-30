#ifndef __MIBLE_STD_AUTH_H__
#define __MIBLE_STD_AUTH_H__
#include <stdint.h>
#include <stdbool.h>
#include "mible.h"

#define SCHD_IDLE                      0

#define REG_TYPE                       0x10UL
#define REG_START                      (REG_TYPE)
#define REG_SUCCESS                    (REG_TYPE+1)
#define REG_FAILED                     (REG_TYPE+2)
#define REG_VERIFY_SUCC                (REG_TYPE+3)
#define REG_VERIFY_FAIL                (REG_TYPE+4)
#define REG_START_WO_PKI               (REG_TYPE+5)

#define LOG_TYPE                       0x20UL
#define LOG_START                      (LOG_TYPE)
#define LOG_SUCCESS                    (LOG_TYPE+1)
#define LOG_INVALID_LTMK               (LOG_TYPE+2)
#define LOG_FAILED                     (LOG_TYPE+3)
#define LOG_START_W_RANDOM             (LOG_TYPE+4)

#define SYS_TYPE                       0xA0UL
#define SYS_KEY_RESTORE                (SYS_TYPE)
#define SYS_KEY_DELETE                 (SYS_TYPE+1)
#define SYS_DEV_INFO_GET               (SYS_TYPE+2)
//#define SYS_MSC_SELF_TEST              (SYS_TYPE+3)
#define SYS_LINK_PARAM_CONFIG          (SYS_TYPE+4)
#define SYS_RXFER_TEST                 (SYS_TYPE+5)
#define SYS_MCU_INFO_READ              (SYS_TYPE+6)

#define ERR_TYPE                       0xE0UL
#define ERR_NOT_REGISTERED             (ERR_TYPE)
#define ERR_REGISTERED                 (ERR_TYPE+1)
#define ERR_REPEAT_LOGIN               (ERR_TYPE+2)
#define ERR_INVALID_OOB                (ERR_TYPE+3)

extern atomic_t mible_status;

uint8_t get_mi_reg_stat(void);
uint8_t get_mi_authorization(void);
void psm_delete(void);
void psm_restore(void);

#endif  /* __MIBLE_STD_AUTH_H__ */