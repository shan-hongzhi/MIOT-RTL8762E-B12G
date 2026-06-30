#include <zephyr/types.h>
#include <string.h>

#include "mible_crypto.h"
#include "third_party/mbedtls/ccm.h"

typedef struct {
    uint32_t iv;
    uint8_t reserve[4];
    uint32_t counter;
} session_nonce_t;

static uint32_t session_dev_cnt;
static uint32_t session_app_cnt;
static session_ctx_t *p_session;

static int update_cnt(uint32_t *p_cnt, uint16_t cnt_low)
{
    uint16_t last_cnt_low = *p_cnt;

    if ((last_cnt_low > 0x7FFF) && (cnt_low < 0x8000)) {
        *p_cnt += 0x10000UL;
    }

    *(uint16_t *)p_cnt = cnt_low;

    return 0;
}

int mi_session_init(session_ctx_t *p_ctx)
{
    if (p_ctx == NULL) {
        return 1;
    }

    p_session = p_ctx;

    session_app_cnt = 0;
    session_dev_cnt = 0;

    return 0;
}

void mi_session_uninit(void)
{
    p_session = NULL;
}

int mi_session_encrypt(const uint8_t *input, uint16_t ilen, uint8_t *output)
{
    if (!p_session) {
        return 1;
    }

    session_nonce_t nonce = {
        .iv = p_session->dev_iv,
    };

    uint16_t cnt_low = (uint16_t)session_dev_cnt;

    update_cnt(&session_dev_cnt, ++cnt_low);

    nonce.counter = session_dev_cnt;

    output[0] = (uint8_t)session_dev_cnt;
    output[1] = (uint8_t)(session_dev_cnt >> 8);

    return aes_ccm_encrypt_and_tag(p_session->dev_key, (void *)&nonce, sizeof(nonce), NULL, 0,
                       2 + input, ilen - 2, 2 + output, output + ilen, 4);
}

int mi_session_decrypt(const uint8_t *input, uint16_t ilen, uint8_t *output)
{
    uint32_t curr_cnt = session_app_cnt;

    if (!p_session) {
        return 1;
    }

    session_nonce_t nonce = {
        .iv = p_session->app_iv,
    };

    uint16_t cnt_low = (uint16_t)input[1] << 8 | input[0];

    update_cnt(&curr_cnt, cnt_low);

    if (curr_cnt <= session_app_cnt && curr_cnt != 0) {
        return -1;
    } else {
        session_app_cnt = curr_cnt;
    }

    nonce.counter = session_app_cnt;

    return aes_ccm_auth_decrypt(p_session->app_key, (void *)&nonce, sizeof(nonce), NULL, 0,
                    2 + input, ilen - 2 - 4, 2 + output, 2 + output + ilen - 2 - 4,
                    4);
}
