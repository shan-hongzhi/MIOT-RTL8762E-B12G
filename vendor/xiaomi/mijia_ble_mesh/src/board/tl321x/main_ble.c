/* main.c - Application main entry point */

/*
 * Copyright (c) 2015-2016 Intel Corporation
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/types.h>
#include <stddef.h>
#include <string.h>
#include <errno.h>
#include <sys/byteorder.h>
#include <sys/crc.h>

#include <settings/settings.h>
#include <net/buf.h>
#include <bluetooth/bluetooth.h>
#include <bluetooth/hci.h>
#include <bluetooth/conn.h>
#include <bluetooth/uuid.h>
#include <bluetooth/gatt.h>

#include <malloc.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <nuttx/timers/watchdog.h>

#include "mible.h"
#include "mible_beacon.h"
#include "mible_spec.h"
#include "mible_user_api.h"
#include "miio_user_api.h"
#include "mible_psk_auth.h"

#include <../../subsys/bluetooth/host/hci_core.h>

#include "light.h"
#include "drivers/ep2t5x/pwm.h"
#include "drivers/ep2t5x/gpio.h"
#include "drivers/ep2t5x/ext_driver/ext_pm.h"
#include "lib/include/plic.h"
#include "lib/include/pm/pm.h"

#if defined(MI_LOG_ENABLED) && (MI_LOG_ENABLED)
#include <sys/printk.h>
#else
#undef printk
#define printk(fmt, ...)
#endif

#define PRODUCT_ID                      156//4611//15062
#define MODEL_NAME                      "xiaomi.bledemo.v1"//"miaomiaoce.sensor_ht.t6"//"miot.sensor_ht.t0509"
#define DEVELOPER_VERSION               56
#define OOB_USE_NUMPAD_INPUT            BIT(0)
#define OOB_USE_QR_INPUT                BIT(3)
#define OOB_USE_DISPLAT_OUT             BIT(4)
#define OOB_USE_QR_CODE_OUT             BIT(7)

#define OOB_IO_CAP                      OOB_USE_NUMPAD_INPUT | OOB_USE_QR_CODE_OUT

#define TEST_GATT_SPEC                  1
#define TEST_ADV_BEACON                 0

#undef  HAVE_CONFIRM_BUTTON
#define HAVE_CONFIRM_BUTTON             1

/***********************board_init()****************************/
// xiaomi mesh develop board (B13G) pin map
#define LED1_R              GPIO_PC2
#define LED2_G              GPIO_PC3
#define LED3_B              GPIO_PB5
#define LED4_C              GPIO_PD4
#define LED5_W              GPIO_PD7

#define BUTTON_0            GPIO_PE2
#define BUTTON_1            GPIO_PE3

static void *m_bindconfilrm_timer = NULL;
static void bind_confirm_timeout(void * p_context)
{
    printk("bind confirm bit clear.\n");
    advertising_init(0);
}

static bool is_logined = false;
static bool is_provisioned = false;

/* NOTE: advertise will close after 30min if unreg.
 */
#define MIBEACON_ADV_TIMEOUT            (30*60*1000)
#define MIBEACON_ADV_ON                 (0xFFFFFFFF)
#define MIBEACON_ADV_OFF                (0)

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <termios.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <sys/uio.h>
static void user_event_cb(mible_user_event_type_t event, void *user_data)
{
    static const uint8_t *const strs[] = {
        [MIBLE_USER_PROV_CB_TYPE_COMPLETE] = "reg success",
        [MIBLE_USER_PROV_CB_TYPE_FAIL] = "reg failed",
        [MIBLE_USER_PROV_CB_TYPE_PROVED] = "Found the KEYINFO",
        [MIBLE_USER_PROV_CB_TYPE_UNPROV] = "Not found the KEYINFO",
        [MIBLE_USER_PROV_CB_TYPE_RESET] = "key delete success",
        [MIBLE_USER_GAP_CB_CONNECTED] = "Connected!",
        [MIBLE_USER_GAP_CB_DISCONNECTED] = "Disconnected!",
        [MIBLE_USER_LOGIN_CB_TYPE_START] = "Login start",
        [MIBLE_USER_LOGIN_CB_TYPE_COMPLETE] = "Login completed",
        [MIBLE_USER_LOGIN_CB_TYPE_FAIL] = "Login failed",
        [MIBLE_USER_OOB_GENERATE] = "Oob generate",
        [MIBLE_USER_OOB_INPUT] = "Oob input",
    };

    if (event >= ARRAY_SIZE(strs)) {
        return;
    }

    printk("%s\n", strs[event]);
    switch (event)
    {
    case MIBLE_USER_PROV_CB_TYPE_PROVED:
    case MIBLE_USER_PROV_CB_TYPE_COMPLETE:
        is_provisioned = true;
        advertising_init(0);
        advertising_start(500);
        //keep adv always on
        miio_system_set_adv_timeout(MIBEACON_ADV_ON);
        break;

    case MIBLE_USER_PROV_CB_TYPE_UNPROV:
    case MIBLE_USER_PROV_CB_TYPE_FAIL:
        is_provisioned = false;
        advertising_init(0);
        advertising_start(100);
        miio_system_set_adv_timeout(MIBEACON_ADV_TIMEOUT);
        break;

    case MIBLE_USER_PROV_CB_TYPE_RESET:
        sys_reboot(0);
        break;

    case MIBLE_USER_LOGIN_CB_TYPE_COMPLETE:
        is_logined = true;
        break;

    case MIBLE_USER_GAP_CB_CONNECTED:
        is_logined = false;
        break;

    case MIBLE_USER_GAP_CB_DISCONNECTED:
        advertising_init(0);
        advertising_start(500);
        break;

    case MIBLE_USER_OOB_GENERATE:
        extern int mible_psk_oob_rsp(uint8_t const * const in, uint8_t ilen);
        extern void print_hex_array(const unsigned char *data, int len);
        uint8_t oob[16] = {0};

        bt_rand(oob, 16);
        printk("OOB genertated:\n");
        
        if ((*(uint16_t *)user_data & 0xff) == 0x80) {
            uint8_t oob[16] = {0xa0, 0xa1, 0xa2, 0xa3, 0xa4, 0xa5, 0xa6, 0xa7, 0xa8,
                                0xa9, 0xaa, 0xab, 0xac, 0xad, 0xae, 0xaf};
            print_hex_array(oob, 16);
            mible_psk_oob_rsp(oob, 16);
        } else if ((*(uint16_t *)user_data & 0xff) == 0x10) {
            for (int i = 0; i < 6; i++) {
                oob[i] = '0' + oob[i] % 10;
            }
            mible_psk_oob_rsp(oob, 6);
            print_hex_array(oob, 6);
        }
        break;

    case MIBLE_USER_OOB_INPUT:
        static struct file console_file0;
        int n = 0;

        file_open(&console_file0, "/dev/ttyS0", O_RDWR | O_BINARY);
        if ((*(uint16_t *)user_data & 0xff) == 0x8) {
            while (n < 16) {
                file_read(&console_file0, &oob[n], 1);
                n++;
            }
            mible_psk_oob_rsp(oob, 16);
        } else if ((*(uint16_t *)user_data & 0xff) == 0x1) {
            while (n < 6) {
                file_read(&console_file0, &oob[n], 1);
                n++;
            }
            mible_psk_oob_rsp(oob, 6);
        }

        file_close(&console_file0);
        break;
    default:
        break;
    }
    return;
}


static void user_devinfo_callback(mible_dev_info_type_t type, mible_dev_info_t *buf)
{
    static const uint8_t support_devinfo[] = {
        MIBLE_DEV_INFO_HARDWARE_VERSION,
        MIBLE_DEV_INFO_NEW_SN,
    };

    switch (type) {
    case MIBLE_DEV_INFO_SUPPORT:
        buf->len = sizeof(support_devinfo);
        memcpy(buf->buff, support_devinfo, buf->len);
        break;
    case MIBLE_DEV_INFO_HARDWARE_VERSION:
        buf->len = strlen("MHCB12G");
        memcpy(buf->buff, "MHCB12G", buf->len);
        break;
    case MIBLE_DEV_INFO_NEW_SN:
        // maxlen 62
        buf->len = MIN(strlen("123456789/123456789/123456789/123456789/"
                      "123456789/123456789/12"), 62);
        memcpy(buf->buff,
               "123456789/123456789/123456789/123456789/123456789/"
               "123456789/12",
               buf->len);
        break;
    default:
        buf->code = MI_ERR_NOT_FOUND;
        return;
    }

    buf->code = MI_SUCCESS;

    return;
}

static void dfu_event_cb(mible_dfu_state_t state, const mible_dfu_param_t *param)
{
    static const uint8_t *const strs[] = {
        [MIBLE_DFU_STATE_START] = "Start",
        [MIBLE_DFU_STATE_UPGRADE_STATUS] = "Upgrade Status",
        [MIBLE_DFU_STATE_TRANSFER] = "Transfer",
        [MIBLE_DFU_STATE_VERIFY] = "Verify",
        [MIBLE_DFU_STATE_SWITCH] = "Switch",
        [MIBLE_DFU_STATE_PROCESS] = "Process Report",
        [MIBLE_DFU_STATE_CANCEL] = "Cancel",
    };

    mible_dfu_param_t *p_dfu = (mible_dfu_param_t *)param;

    if (state >= ARRAY_SIZE(strs)) {
        return;
    }else if(state == MIBLE_DFU_STATE_UPGRADE_STATUS){
        p_dfu->upgrade_status.rsp = p_dfu->upgrade_status.req;
        printk("Req silent %d, install %d, Rsp silent %d, install %d\n", 
            p_dfu->upgrade_status._req.silent, p_dfu->upgrade_status._req.install,
            p_dfu->upgrade_status._rsp.silent, p_dfu->upgrade_status._rsp.install);
    }else if(state == MIBLE_DFU_STATE_VERIFY){
        printk("verify result is %d, version %u.%u.%u_%04u\n", p_dfu->verify.value,
            p_dfu->verify.version.major, p_dfu->verify.version.minor, 
            p_dfu->verify.version.revision, p_dfu->verify.version.build);
        //TODO: p_dfu->verify.value return MIBLE_DFU_STATUS_ERR_INVALID_VERSION when verion is old
    }

    printk("DFU %s\n", strs[state]);
}

#if TEST_GATT_SPEC
const char *str1 = "Hi, Susan, We re throwing a surprise party for Xiaomings birthday. \
As scheduled, all the classmates in our class will gather at the Students  Centre at \
8 pm this Friday evening to celebrate Xiaoming s birthday. We would feel much honored \
if you could come and enjoy it, which is to finish at 9, lasting an hour. As the party \
starts, we will light the candles and sing Happy Birthday for him. Then the birthday \
cake will be presented and cut for all of us to enjoy, following which we ll sing songs \
and play some small games. What s more, as a surprise, we ll all prepare some secret \
birthday gifts for him, and it ll be great to see how surprised he will be seeing the \
gifts. As it s a surprise party, he will know nothing about it beforehand, and we do hope\
that you could manage to attend it. Looking forward to your coming. Yours Li Hua";

void gatt_prop()
{
    printk("Enter gatt_prop!\n");
    int ret;
    if(is_logined){
        mible_spec_property_t Props;
        Props.siid = 1;Props.piid = 2; 
        mible_spec_property_value_new_uchar(&Props.value, 80);
        miio_gatt_properties_changed(1, &Props);

        Props.siid = 2;Props.piid = 4; 
        mible_spec_property_value_new_float(&Props.value, -12.3f);
        miio_gatt_properties_changed(1, &Props);

        Props.siid = 3;Props.piid = 6; 
        mible_spec_property_value_new_string(&Props.value, "abcdefg1234567!");
        miio_gatt_properties_changed(1, &Props);


        mible_spec_property_t newProps[5];
        newProps[0].siid = 1;newProps[0].piid = 2; 
        mible_spec_property_value_new_uchar(&newProps[0].value, 80);
        newProps[1].siid = 1;newProps[1].piid = 2; 
        mible_spec_property_value_new_uchar(&newProps[1].value, 100);
        newProps[2].siid = 2;newProps[2].piid = 4; 
        mible_spec_property_value_new_float(&newProps[2].value, -12.3f);
        newProps[3].siid = 2;newProps[3].piid = 4; 
        mible_spec_property_value_new_float(&newProps[3].value, 23.4f);
        newProps[4].siid = 3;newProps[4].piid = 6; 
        mible_spec_property_value_new_string(&newProps[4].value, str1);
        ret = miio_gatt_properties_changed(5, newProps);
        printk("%s[%d]ret=%d strlen(str1)=%d\n", __FUNCTION__, __LINE__, ret, strlen(str1));
    }
}

void gatt_event()
{
    if(is_logined){
        //5-1006 Door Ring
        miio_gatt_event_occurred(5, 1006, NULL);
        //2-1007 Lock abnormal
        mible_spec_arguments_t newArgs;
        mible_spec_argument_t arguments[2];
        newArgs.size = 2;
        newArgs.arguments = arguments;
        arguments[0].piid = 4;
        mible_spec_property_value_new_uchar(&arguments[0].value, 3);
        arguments[1].piid = 6;
        mible_spec_property_value_new_string(&arguments[1].value, "abcdefghijklmnopqrstuvwxyz1234567890!!!");
        miio_gatt_event_occurred(2, 1007, &newArgs);
    }
}

static char str2[64] = "string test ok!!!";

static struct {
    bool        s_bool;
    uint8_t     s_uchar;
    uint16_t    s_ushort;
    uint32_t    s_ulong;
    int8_t      s_char;
    int16_t     s_short;
    int32_t     s_long;
    float       s_float;
    int64_t     s_longlong;
    char        *s_string;
    int         s_strlen;
} data_type_test = {
    .s_float = 0.0,
    .s_string = str2,
    .s_strlen = 18,
};


static void P_3_1_doGet(mible_spec_bearer_t bearer, mible_spec_property_operation_t *o)
{
    mible_spec_property_value_new_boolean(&o->value, data_type_test.s_bool);
}

static void P_3_2_doGet(mible_spec_bearer_t bearer, mible_spec_property_operation_t *o)
{
    mible_spec_property_value_new_uchar(&o->value, data_type_test.s_uchar);
}

static void P_3_3_doGet(mible_spec_bearer_t bearer, mible_spec_property_operation_t *o)
{
    mible_spec_property_value_new_ushort(&o->value, data_type_test.s_ushort);
}

static void P_3_4_doGet(mible_spec_bearer_t bearer, mible_spec_property_operation_t *o)
{
    mible_spec_property_value_new_ulong(&o->value, data_type_test.s_ulong);
}

static void P_3_5_doGet(mible_spec_bearer_t bearer, mible_spec_property_operation_t *o)
{
    mible_spec_property_value_new_char(&o->value, data_type_test.s_char);
}

static void P_3_6_doGet(mible_spec_bearer_t bearer, mible_spec_property_operation_t *o)
{
    mible_spec_property_value_new_short(&o->value, data_type_test.s_short);
}

static void P_3_7_doGet(mible_spec_bearer_t bearer, mible_spec_property_operation_t *o)
{
    mible_spec_property_value_new_long(&o->value, data_type_test.s_long);
}

static void P_3_8_doGet(mible_spec_bearer_t bearer, mible_spec_property_operation_t *o)
{
    // mible_spec_property_value_new_float(&o->value, data_type_test.s_float);
    o->code = MIBLE_SPEC_OPERATION_INVALID;
}

static void P_3_9_doGet(mible_spec_bearer_t bearer, mible_spec_property_operation_t *o)
{
    if(bearer != MIBLE_SPEC_BEARER_GATT){
        o->code = MIBLE_SPEC_OPERATION_INVALID;
        return;
    }
    mible_spec_property_value_new_longlong(&o->value, data_type_test.s_longlong);
}

static void P_3_10_doGet(mible_spec_bearer_t bearer, mible_spec_property_operation_t *o)
{
    if(bearer != MIBLE_SPEC_BEARER_GATT){
        o->code = MIBLE_SPEC_OPERATION_INVALID;
        return;
    }
    mible_spec_property_value_new_nstring(&o->value, data_type_test.s_string, data_type_test.s_strlen);
}

static void P_3_1_doSet(mible_spec_bearer_t bearer, mible_spec_property_operation_t *o)
{
    data_type_test.s_bool = o->value.data.boolean;
    o->code = MIBLE_SPEC_OPERATION_OK;
}

static void P_3_2_doSet(mible_spec_bearer_t bearer, mible_spec_property_operation_t *o)
{
    data_type_test.s_uchar = o->value.data.ucharValue;
    o->code = MIBLE_SPEC_OPERATION_OK;
}

static void P_3_3_doSet(mible_spec_bearer_t bearer, mible_spec_property_operation_t *o)
{
    data_type_test.s_ushort = o->value.data.ushortValue;
    o->code = MIBLE_SPEC_OPERATION_OK;
}

static void P_3_4_doSet(mible_spec_bearer_t bearer, mible_spec_property_operation_t *o)
{
    data_type_test.s_ulong = o->value.data.ulongValue;
    o->code = MIBLE_SPEC_OPERATION_OK;
}

static void P_3_5_doSet(mible_spec_bearer_t bearer, mible_spec_property_operation_t *o)
{
    data_type_test.s_char = o->value.data.charValue;
    o->code = MIBLE_SPEC_OPERATION_OK;
}

static void P_3_6_doSet(mible_spec_bearer_t bearer, mible_spec_property_operation_t *o)
{
    data_type_test.s_short = o->value.data.shortValue;
    o->code = MIBLE_SPEC_OPERATION_OK;
}

static void P_3_7_doSet(mible_spec_bearer_t bearer, mible_spec_property_operation_t *o)
{
    data_type_test.s_long = o->value.data.longValue;
    o->code = MIBLE_SPEC_OPERATION_OK;
}

static void P_3_8_doSet(mible_spec_bearer_t bearer, mible_spec_property_operation_t *o)
{
    data_type_test.s_float = o->value.data.floatValue;
    // o->code = MIBLE_SPEC_OPERATION_OK;
    o->code = MIBLE_SPEC_OPERATION_INVALID;
}

static void P_3_9_doSet(mible_spec_bearer_t bearer, mible_spec_property_operation_t *o)
{
    if(bearer != MIBLE_SPEC_BEARER_GATT){
        o->code = MIBLE_SPEC_OPERATION_INVALID;
        return;
    }
    data_type_test.s_longlong = o->value.data.longlongValue;
    o->code = MIBLE_SPEC_OPERATION_OK;
}

static void P_3_10_doSet(mible_spec_bearer_t bearer, mible_spec_property_operation_t *o)
{
    if(bearer != MIBLE_SPEC_BEARER_GATT){
        o->code = MIBLE_SPEC_OPERATION_INVALID;
        return;
    }
    
    if(o->value.data.string.length >= sizeof(str2)){
        o->code = MIBLE_SPEC_OPERATION_ERROR_VALUE;
        return;
    }
    
    data_type_test.s_strlen = o->value.data.string.length;
    memcpy(data_type_test.s_string, o->value.data.string.str, o->value.data.string.length);
    o->code = MIBLE_SPEC_OPERATION_OK;
}

#endif


MIBLE_SPEC_DEFINE(spec_handlers)
    MIBLE_SPEC_SIID_START(1)
        MIBLE_SPEC_HANDLER(1, P_3_1_doGet, P_3_1_doSet, NULL),
        MIBLE_SPEC_HANDLER(2, P_3_2_doGet, P_3_2_doSet, NULL),
        MIBLE_SPEC_HANDLER(3, P_3_3_doGet, P_3_3_doSet, NULL),
        MIBLE_SPEC_HANDLER(4, P_3_4_doGet, P_3_4_doSet, NULL),
        MIBLE_SPEC_HANDLER(5, P_3_5_doGet, P_3_5_doSet, NULL),
        MIBLE_SPEC_HANDLER(6, P_3_6_doGet, P_3_6_doSet, NULL),
        MIBLE_SPEC_HANDLER(7, P_3_7_doGet, P_3_7_doSet, NULL),
        MIBLE_SPEC_HANDLER(8, P_3_8_doGet, P_3_8_doSet, NULL),
        MIBLE_SPEC_HANDLER(9, P_3_9_doGet, P_3_9_doSet, NULL),
        MIBLE_SPEC_HANDLER(10, P_3_10_doGet, P_3_10_doSet, NULL),
    MIBLE_SPEC_SIID_END
MIBLE_SPEC_END

static const struct mible_spec_piid_or_aiid_list *
    find_spec_handler(uint16_t siid, uint16_t piid_or_aiid)
{
    int i, j;

    for (i = 0; i < ARRAY_SIZE(spec_handlers); i++) {
        if (spec_handlers[i].siid == siid) {
            break;
        }
    }

    if (i == ARRAY_SIZE(spec_handlers)) {
        return NULL;
    }

    for (j = 0; spec_handlers[i].lists[j].piid_or_aiid != 0; j++) {
        if (spec_handlers[i].lists[j].piid_or_aiid == piid_or_aiid) {
            break;
        }
    }

    if (spec_handlers[i].lists[j].piid_or_aiid == 0) {
        return NULL;
    }

    return &spec_handlers[i].lists[j];
}


static const uint8_t *const spec_bearers[] = {
    [MIBLE_SPEC_BEARER_GATT] = "[GATT]",
};

static void do_spec_get(mible_spec_bearer_t bearer, mible_spec_property_operation_t *o)
{
    const struct mible_spec_piid_or_aiid_list *i;

    if (bearer >= ARRAY_SIZE(spec_bearers)) {
        return;
    }

    printk("%s %s siid %d piid %d type %d val %08lx\n", __FUNCTION__, spec_bearers[bearer], o->siid, o->piid, o->value.format, o->value.data.ulongValue);

    i = find_spec_handler(o->siid, o->piid);
    if (!i || !(i->get)) {
        o->code = MIBLE_SPEC_OPERATION_INVALID;
        return;
    }

    i->get(bearer, o);
}

static void do_spec_set(mible_spec_bearer_t bearer, mible_spec_property_operation_t *o)
{
    const struct mible_spec_piid_or_aiid_list *i;

    if (bearer >= ARRAY_SIZE(spec_bearers)) {
        return;
    }

    printk("%s %s siid %d piid %d type %d val %08lx\n", __FUNCTION__, spec_bearers[bearer], 
            o->siid, o->piid, o->value.format, o->value.data.ulongValue);

    i = find_spec_handler(o->siid, o->piid);
    if (!i || !(i->set)) {
        return;
    }

    i->set(bearer, o);
}

static void do_spec_invoke(mible_spec_bearer_t bearer, mible_spec_action_operation_t *o)
{
    const struct mible_spec_piid_or_aiid_list *i;

    if (bearer >= ARRAY_SIZE(spec_bearers)) {
        return;
    }

    printk("%s %s siid %d aiid %d arg[size %d]\n", __FUNCTION__, spec_bearers[bearer], o->siid, o->aiid,
           o->vals.size);

    i = find_spec_handler(o->siid, o->aiid);
    if (!i || !(i->invoke)) {
        return;
    }

    i->invoke(bearer, o);
}


static uint8_t gatt_spec_buffer[1024];
static struct mible_gatt_spec_tx gatt_spec_pool[5];
const struct mible_config mi_config = {
    .cfg =
    {
        .pid                = PRODUCT_ID,
        .dev_version        = DEVELOPER_VERSION,
        .model_name         = MODEL_NAME,
        .io                 = OOB_IO_CAP,
        .have_reset_button  = HAVE_RESET_BUTTON,
        .have_confirm_button = HAVE_CONFIRM_BUTTON,

        .gatt_spec =
        {
            .rx_buf = gatt_spec_buffer,
            .max_rx_buf_len = ARRAY_SIZE(gatt_spec_buffer),
            .max_tx_buf_len = ARRAY_SIZE(gatt_spec_buffer),
            .tx_pool = gatt_spec_pool,
            .max_tx_pool_num = ARRAY_SIZE(gatt_spec_pool),
        },

        .init = 
        {
            .gatt_spec_init = mible_gatt_spec_enable,
        },
    },
    .cb =
    {
        .dev_info.req = user_devinfo_callback,
        .event.event = user_event_cb,
        .dfu_info.event = dfu_event_cb,
        .spec.get = do_spec_get,
        .spec.set = do_spec_set,
        .spec.invoke = do_spec_invoke,
    },
};


static void enqueue_new_objs(void)
{
    static float temp = 0.0f;
    static float hum = 0.0f;
    static int8_t  battery;

    temp = temp < 50.0f ? temp + 1.0f : -50.0f;
    hum = hum < 100.0f ? hum + 1.0f : 0.0f;
    battery = battery < 100 ? battery + 1 : 0;

    mible_spec_property_value_t battery_value;
    mible_spec_property_value_t temp_value;
    mible_spec_property_value_t hum_value;
    mible_spec_property_value_new_char(&battery_value,battery);
    mible_spec_property_value_new_float(&temp_value,temp);
    mible_spec_property_value_new_float(&hum_value,hum);
    mibeacon_property_changed(2, 1003, &battery_value, 0);
    mibeacon_property_changed(3, 1001, &temp_value, 0);
    mibeacon_property_changed(3, 1008, &hum_value, 0);
    mibeacon_event_occurred(2, 1001, NULL, 1);
}

#if TEST_ADV_BEACON
static void *object_test_timer = NULL;
static void object_test_callback(void *p_context)
{
    printk("enter object_test_callback\n");
    static uint8_t times = 0;
    enqueue_new_objs();
    // gatt_event();
    times++;
    printk("times: %d\n", times);
}
#endif

static void button_process_handler(void *args)
{
    //P2_2 is the reset button
    if(*(int*)args == BUTTON_1){
        printk("restore...\n");
        psm_delete();
        mible_record_delete(RECORD_ID_DFU_INFO);
        advertising_init(0);
        advertising_start(100);
    } 
    //P4_2 is the event reporting button
    else if(*(int*)args == BUTTON_0){
        if (get_mi_reg_stat()){
            enqueue_new_objs();
#if TEST_GATT_SPEC
            if(get_mi_authorization()){
                gatt_prop();
                gatt_event();
            }
#endif
        }else{
            printk("start solicited adv\n");
            advertising_init(1);
            miio_timer_start(m_bindconfilrm_timer, 10000, NULL); // start the 10-second timer.
        }
    }
}

int ep2t5x_gpio_irq_handler(int irq, FAR void *context, FAR void *arg)
{
    UNUSED(irq);
    UNUSED(context);
    UNUSED(arg);

    printk("GPIO_INPUT_HANDLER %d\n", gpio_get_level(BUTTON_0));
    /* Config WakeUp pin */
    if (gpio_get_level(BUTTON_0)){
        printk("button release\n");
        static struct mible_task_item button_irq_tasklet;
        mible_single_task_post(&button_irq_tasklet, button_process_handler, NULL);

        gpio_set_irq(GPIO_IRQ7, BUTTON_0, INTR_FALLING_EDGE);
    }else{
        printk("button pressed\n");
        gpio_set_irq(GPIO_IRQ7, BUTTON_0, INTR_RISING_EDGE);
    }

    gpio_clr_irq_status(GPIO_IRQ_IRQ7);
    return 0;
}

static int board_init(void)
{
    gpio_function_en(LED1_R);
    gpio_output_en(LED1_R);
    gpio_set_level(LED1_R, 0);
    gpio_function_en(LED2_G);
    gpio_output_en(LED2_G);
    gpio_set_level(LED2_G, 0);

    gpio_function_en(BUTTON_0);
    gpio_output_dis(BUTTON_0);
    gpio_input_en(BUTTON_0);
    gpio_set_up_down_res(BUTTON_0, GPIO_PIN_PULLUP_10K);
    gpio_set_irq(GPIO_IRQ7, BUTTON_0, INTR_FALLING_EDGE);
    gpio_set_irq_mask(GPIO_IRQ_IRQ7);
     /* external irq need to add RISCV_ARCH_IRQS */
    irq_attach(IRQ_GPIO_IRQ7 + RISCV_ARCH_IRQS, ep2t5x_gpio_irq_handler, NULL);
    plic_interrupt_enable(IRQ_GPIO_IRQ7);

    return 0;
}

/* Config WatchDog*/
static int fd_wd;
struct board_ctl {
    unsigned long req;
    unsigned long val;
};

static const struct board_ctl wdog_ctls[] = {
    { WDIOC_SETTIMEOUT, 20 * 1000 },
    { WDIOC_START, 0 },
};

static const struct {
    int *fd;
    const char *dev_path;
    int oflag;
    const struct board_ctl *ctls;
    uint32_t ctls_count;
} hw_drivers[] = {
    { &fd_wd,    "/dev/watchdog0", O_RDONLY, wdog_ctls, ARRAY_SIZE(wdog_ctls) },
};

static int wdg_init(void)
{
    int err = 0;

    for (int i = 0; i < ARRAY_SIZE(hw_drivers); i++) {
        int fd = open(hw_drivers[i].dev_path, hw_drivers[i].oflag);
        if (fd < 0) {
            printk("%s open failed\n", hw_drivers[i].dev_path);
            return fd;
        }

        *hw_drivers[i].fd = fd;

        for (int j = 0; j < hw_drivers[i].ctls_count; j++) {
            err = ioctl(fd, hw_drivers[i].ctls[j].req, hw_drivers[i].ctls[j].val);
            if (err < 0) {
                printk("%s ioctl req %ld val %ld err:%d\n", hw_drivers[i].dev_path,
                    hw_drivers[i].ctls[j].req, hw_drivers[i].ctls[j].val, err);
                return err;
            }
        }
    }

    return err;
}

int main(int argc, FAR char *argv[])
{
    int err;
    printk("psk_auth start!\n");

    printk("Model Name: %s\nPID: %ld\nDevelop Version: %d\n",
            mi_config.cfg.model_name, mi_config.cfg.pid, mi_config.cfg.dev_version);

    // err = wdg_init();
    // if (err) {
    //     printk("watchdog init failed (err %d)\n", err);
    //     return err;
    // }

    err = board_init();
    if (err) {
        printk("board_init_BUTTON_0 failed (err %d)\n", err);
        return err;
    }

    extern int zblue_main(void);
    err = zblue_main();
    if (err) {
        printk("zblue init failed (err %d)\n", err);
        return err;
    }

    err = bt_enable(NULL); // enable bluetooth
    if (err) {
        printk("Bluetooth initialized failed\n");
        return err;
    }

    if (IS_ENABLED(CONFIG_SETTINGS)) {
        if(settings_subsys_init()) {
            printk("Settings init failed!\n");
            return -EINVAL;
        }

        if (IS_ENABLED(CONFIG_BT_SETTINGS)) {
            settings_load();
        }
    }

    err = mible_service_init();
    if (err) {
        printk("mi service init failed (err %d)\n", err);
        return err;
    }   

    mible_user_timer_create(&m_bindconfilrm_timer, bind_confirm_timeout, MIBLE_TIMER_SINGLE_SHOT);

    psm_restore();

#if TEST_GATT_SPEC
    if(mi_config.cfg.init.gatt_spec_init){
    err = mi_config.cfg.init.gatt_spec_init();
    printk("gatt_spec_init (err %d)\n", err);
}
#endif

#if TEST_ADV_BEACON
    err = miio_timer_create(&object_test_timer, object_test_callback, MIBLE_TIMER_REPEATED);
    if (err) {
        printk("Unable to create object_test_timer");
        return err;
    }
    miio_timer_start(object_test_timer, 5000, NULL);
#endif
    extern void mible_tasks_exec(void);
    while(1) {
        mible_tasks_exec();
    }

    return 0;
}
