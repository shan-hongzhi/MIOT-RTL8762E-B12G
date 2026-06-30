/* main.c - Application main entry point */

/*
 * Copyright (c) 2015-2016 Intel Corporation
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/types.h>
#include <string.h>
#include <malloc.h>
#include <errno.h>
#include <kernel.h>
#include <sys/byteorder.h>
#include <sys/ioctl.h>
#include <sys/crc.h>
#include <fcntl.h>

#include <settings/settings.h>
#include <net/buf.h>
#include <bluetooth/bluetooth.h>
#include <bluetooth/hci.h>
#include <bluetooth/conn.h>
#include <bluetooth/uuid.h>
#include <bluetooth/gatt.h>
#include <bluetooth/mesh.h>
#include <nuttx/timers/watchdog.h>
#include <../../subsys/bluetooth/host/hci_core.h>
#include <../../subsys/bluetooth/mesh/access.h>

#include "main.h"
#include "mible.h"
#include "mible_mesh.h"
#include "mible_spec.h"
#include "mible_user_api.h"
#include "miio_user_api.h"
#include "mesh_vnd_lpn.h"
#include "mesh_vnd_df.h"
#include "mible_trace.h"

#include "rtl876x.h"
#include "rtl876x_gpio.h"
#include "rtl876x_tim.h"
#include "rtl876x_pinmux.h"
#include "rtl876x_rcc.h"
#include "rtl876x_nvic.h"
#include "rtl876x_bitfields.h"
#include "dlps.h"
#include "rtl876x_wdg.h"

#if defined(MI_LOG_ENABLED) && (MI_LOG_ENABLED)
#include <sys/printk.h>
#else
#undef printk
#define printk(fmt, ...)
#endif

#if CONFIG_BT_PER_ADV_SYNC
#include "platform_utils.h"

#define PER_ADV_REQ_MAX_TIMEOUT 12*60 //(min)
static bool is_pa_sync = false;
static uint32_t last_timeout = 0;

per_adv_sync_gw_info_t gw_info_record = {0};
mible_status_t main_lpn_gw_info_restore(void);
static void main_lpn_per_adv_sync_timer_handler(void * arg);
void main_lpn_per_adv_pub_req_init(void);
void main_lpn_padv_adv_sync_request(bool enable);
uint32_t main_lpn_rand(uint32_t down, uint32_t up);
void main_lpn_per_adv_property_change(uint8_t siid,uint8_t piid,uint8_t is_sync);
void main_lpn_record_reset(void);
#endif

#define PRODUCT_ID                      19274
#define MODEL_NAME                      "xiaomi.switch.mesh2"

#define DEVELOPER_VERSION               203

#define TEST_GATT_SPEC                  0

#define SWITCH_POWER_ON_COUNT           4
#define SWITCH_POWER_ON_TIME            8000 //!< millisecond
#define RECORD_ID_POWER_ON_COUNT        0
#define RECORD_ID_LIGHT_ON_STATE        1
#if CONFIG_BT_PER_ADV_SYNC
#define RECORD_ID_SYNC_GW_INFO          2
#define RECORD_ID_LPN_MODE_STATE        3
#endif

static void *pwn_timer;
static void *app_handle;
static void *reset_timer;
static void *conn_timer;
#if CONFIG_BT_PER_ADV_SYNC
static void *sync_timer = NULL;
#endif

static bool is_connected = false;
static bool is_logined = false;
static bool is_provisioned = false;

static uint8_t gatt_spec_buffer[1024];
static struct mible_gatt_spec_tx gatt_spec_pool[5];
const volatile struct mible_config mi_config;

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

#define BT_ADV_SCAN_UNIT(_ms) ((_ms) * 8 / 5)

/*
 * Set Advertisement data. Based on the Eddystone specification:
 * https://github.com/google/eddystone/blob/master/protocol-specification.md
 * https://github.com/google/eddystone/tree/master/eddystone-url
 */
static const struct bt_data ad[] = {
    BT_DATA_BYTES(BT_DATA_FLAGS, BT_LE_AD_NO_BREDR),
    BT_DATA_BYTES(BT_DATA_UUID16_ALL, 0xaa, 0xfe),
    BT_DATA_BYTES(BT_DATA_SVC_DATA16,
              0xaa, 0xfe, /* Eddystone UUID */
              0x10, /* Eddystone-URL frame type */
              0x00, /* Calibrated Tx power at 0m */
              0x00, /* URL Scheme Prefix http://www. */
              'z', 'e', 'p', 'h', 'y', 'r',
              'p', 'r', 'o', 'j', 'e', 'c', 't',
              0x08) /* .org */
};

int mible_gap_user_noconn_adv_start(uint32_t adv_interval, uint8_t count, const struct bt_data *ad,
                    size_t ad_len)
{
    static struct bt_le_ext_adv *g_user_adv = NULL;

    struct bt_le_adv_param param = {
        .interval_min = BT_ADV_SCAN_UNIT(adv_interval),
        .interval_max = BT_ADV_SCAN_UNIT(adv_interval),
        .options = BT_LE_ADV_OPT_USE_IDENTITY,
    };
    struct bt_le_ext_adv_start_param start = {
        .num_events = count,
    };
    int err;
#if defined(CONFIG_BT_MESH_STATISTIC)
    mible_trace_update_user_adv_cnt(count);
#endif
    if (!g_user_adv) {
        err = bt_le_ext_adv_create(&param, NULL, &g_user_adv);
        if (err) {
            printk("adv create failed (err %d)\n", err);
            return -EINVAL;
        }
    }

    if (atomic_test_bit(g_user_adv->flags, BT_ADV_ENABLED)) {
        return -EBUSY;
    }

    err = bt_le_ext_adv_update_param(g_user_adv, &param);
    if (err) {
        printk("Unable set adv params\n");
        return err;
    }

    err = bt_le_ext_adv_set_data(g_user_adv, ad, ad_len, NULL, 0);
    if (err) {
        printk("Unable set adv data\n");
        return err;
    }

    err = bt_le_ext_adv_start(g_user_adv, &start);
    if (err) {
        printk("Unable start adv\n");
        return err;
    }

    return 0;
}

// xiaomi mesh develop board (B12G) pin map
#define LED1_R              P2_4
#define LED2_G              P2_5
#define LED3_B              P2_6
#define LED4_C              P0_2
#define LED5_W              P0_1

#define BUTTON_0            P4_2
#define GPIO_PIN_BUTTON_0   GPIO_GetPin(BUTTON_0)
#define GPIO_BUTTON0_IRQn    GPIO30_IRQn
#define GPIO_BUTTON0_Handler GPIO30_Handler
#define BUTTON_1            P4_1
#define GPIO_PIN_BUTTON_1   GPIO_GetPin(BUTTON_1)
#define GPIO_BUTTON1_IRQn    GPIO29_IRQn
#define GPIO_BUTTON1_Handler GPIO29_Handler

#define BUTTON_2            P4_0
#define GPIO_PIN_BUTTON_2   GPIO_GetPin(BUTTON_2)
#define GPIO_BUTTON2_IRQn    GPIO13_IRQn
#define GPIO_BUTTON2_Handler GPIO13_Handler

switch_demo_t two_key_switch = {
    .left_on = false,
    .left_mode = 0,
    .fault = 0,
    .anti_flk = 0,
    .right_on = false,
    .right_mode = 0,
    .anti_flk = false,
    .power_consumption = 5.0,
    .power_electric = 200,
    .accumulation = true,
    .gmt_offset = 0,
    .weather = 0,
    .utc_time = 0,
};

int key1_onoff_set(bool on)
{
    printk("key1 %s!\n", on? "on" : "off");

    two_key_switch.left_on = on;
    Pad_Config(LED1_R, PAD_SW_MODE, PAD_IS_PWRON, PAD_PULL_NONE, PAD_OUT_ENABLE, on ? PAD_OUT_HIGH : PAD_OUT_LOW);

    return 0;
}

int key2_onoff_set(bool on)
{
    two_key_switch.right_on = on;
    Pad_Config(LED2_G, PAD_SW_MODE, PAD_IS_PWRON, PAD_PULL_NONE, PAD_OUT_ENABLE, on ? PAD_OUT_HIGH : PAD_OUT_LOW);

    return 0;
}

void user_scan_check(void)
{
    if(is_provisioned){
        if(two_key_switch.anti_flk || is_connected){
            // if flk mode or connected, set scan low mode(15%)
            miio_mesh_set_scan_level(MESH_SCAN_LOW);
        }else{
            // set scan normal mode(20%)
            miio_mesh_set_scan_level(MESH_SCAN_NORMAL);
        }
    }else{
        // if need scan when unprov, set scan low mode(15%)
        miio_mesh_set_scan_level(MESH_SCAN_NORMAL);
    }
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
    int ret;
    if(is_connected){
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
    if(is_connected){
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
#endif

static void button_process_handler(void *args)
{
    printk("button pressed!\n");

    mible_gap_user_noconn_adv_start(20, 8, ad, ARRAY_SIZE(ad));

    two_key_switch.left_on = two_key_switch.left_on? false:true;
    key1_onoff_set(two_key_switch.left_on);

    if(is_provisioned){
        // first upload button event for automation
        miio_mesh_event_occurred(3, 1, NULL);

        mible_spec_property_value_t value;
        mible_spec_property_value_new_boolean(&value, two_key_switch.left_on);
        miio_mesh_properties_changed(2, 1, &value);
#if TEST_GATT_SPEC
        gatt_prop();
        gatt_event();
#endif
    }
}

#if defined(CONFIG_PM)
#define DATA_RAM_FUNCTION  __attribute__((section(".app.data_ram.text")))
static uint32_t gpio_storereg[10];
bool IO_DLPS_Enter_Allowed = true;

DATA_RAM_FUNCTION
static bool gpio_check_dlps(uint32_t *timeout)
{
    return IO_DLPS_Enter_Allowed;
}

DATA_RAM_FUNCTION
static void gpio_enter_dlps(void)
{
  RCC_PeriphClockCmd(APBPeriph_GPIO, APBPeriph_GPIO_CLOCK, ENABLE);

  memcpy((uint32_t *)&gpio_storereg[0], (uint32_t *)&GPIO->DATAOUT, 12);
  memcpy((uint32_t *)&gpio_storereg[3], (uint32_t *)&GPIO->INTEN, 16);

  gpio_storereg[7] = GPIO->DEBOUNCE;
  gpio_storereg[8] = *(__IO uint32_t *)(0x40000344UL);
  gpio_storereg[9] = GPIO->INTBOTHEDGE;

  /* for evb weakup */
  // config this pin (pad) to software mode: waiting ext interrupt to wakeup.
  Pad_Config(BUTTON_0, PAD_SW_MODE, PAD_IS_PWRON, PAD_PULL_UP, PAD_OUT_DISABLE, PAD_OUT_LOW);

  extern void System_WakeUpDebounceTime(uint8_t time);
  System_WakeUpDebounceTime(10);
  // use this pin to wakeup the chip from Deep Low Power Mode.
  if (GPIO_ReadInputDataBit(GPIO_PIN_BUTTON_0)){
    System_WakeUpPinEnable(BUTTON_0, PAD_WAKEUP_POL_LOW, PAD_WK_DEBOUNCE_DISABLE, 0);
  }else{
    System_WakeUpPinEnable(BUTTON_0, PAD_WAKEUP_POL_HIGH, PAD_WK_DEBOUNCE_DISABLE, 0);
  }
}

DATA_RAM_FUNCTION
static void gpio_exit_dlps(void)
{
  RCC_PeriphClockCmd(APBPeriph_GPIO, APBPeriph_GPIO_CLOCK, ENABLE);

  memcpy((uint32_t *)&GPIO->DATAOUT, (uint32_t *)&gpio_storereg[1], 8);

  memcpy((uint32_t *)&GPIO->INTMASK, (uint32_t *)&gpio_storereg[4], 12);

  GPIO->DEBOUNCE      = gpio_storereg[7];
  GPIO->DATAOUT       = gpio_storereg[0];
  GPIO->INTCLR        = ~(gpio_storereg[1]);
  GPIO->INTEN         = gpio_storereg[3];
  *(__IO uint32_t *)(0x40000344UL) = gpio_storereg[8];
  GPIO->INTBOTHEDGE = gpio_storereg[9];

  /* for evb weakup */
  // config this pin (pad) to muxplexing mode.
  Pad_Config(BUTTON_0, PAD_PINMUX_MODE, PAD_IS_PWRON, PAD_PULL_UP, PAD_OUT_DISABLE, PAD_OUT_HIGH);
}

DATA_RAM_FUNCTION
static void gpio_wakeup_dlps(void)
{
  NVIC_DisableIRQ(System_IRQn);

  if (System_WakeUpInterruptValue(BUTTON_0) == SET)
  {
    Pad_ClearWakeupINTPendingBit(BUTTON_0);
    System_WakeUpPinDisable(BUTTON_0);
    IO_DLPS_Enter_Allowed = false;
  }

  // MUST clear debounce bit here.
  __rtl_btaon_fast_write_safe_8b(0x2b, (__rtl_btaon_fast_read_safe_8b(0x2b) | BIT7));
  NVIC_ClearPendingIRQ(System_IRQn);
}

static struct dlps_driver_ops rtl_pm_driver_ops[]
    __attribute__((section(".dlps.dataon.data")))
    __attribute__((__used__)) = {
    [0] = {
        gpio_check_dlps,
        gpio_enter_dlps,
        gpio_exit_dlps,
        gpio_wakeup_dlps,
    },
};
#endif

static int GPIO_INPUT_HANDLER(int irq, void *context, void *arg)
{
    if (GPIO_GetINTStatus(GPIO_PIN_BUTTON_0)) {
        GPIO_INTConfig(GPIO_PIN_BUTTON_0, DISABLE);
        GPIO_MaskINTConfig(GPIO_PIN_BUTTON_0, ENABLE);

        printk("GPIO_INPUT_HANDLER %d\n", GPIO_ReadInputDataBit(GPIO_PIN_BUTTON_0));
        /* Config WakeUp pin */
        if (GPIO_ReadInputDataBit(GPIO_PIN_BUTTON_0)){
            printk("button release\n");
            static struct mible_task_item button_irq_tasklet;
            mible_single_task_post(&button_irq_tasklet, button_process_handler, NULL);

            GPIO->INTPOLARITY = GPIO->INTPOLARITY & (~GPIO_PIN_BUTTON_0);
        }else{
            printk("button press\n");
            GPIO->INTPOLARITY = (GPIO->INTPOLARITY & (~GPIO_PIN_BUTTON_0)) | GPIO_PIN_BUTTON_0;
        }
#if defined(CONFIG_PM)
        IO_DLPS_Enter_Allowed = true;
#endif
        GPIO_ClearINTPendingBit(GPIO_PIN_BUTTON_0);
        GPIO_MaskINTConfig(GPIO_PIN_BUTTON_0, DISABLE);
        GPIO_INTConfig(GPIO_PIN_BUTTON_0, ENABLE);
    }else{
        printk("GPIO_INPUT_HANDLER not BUTTON0\n");
    }

    return 0;
}

static int board_init(void)
{
    // Pad_Config(BUTTON_0, PAD_SW_MODE, PAD_IS_PWRON, PAD_PULL_UP, PAD_OUT_DISABLE, PAD_OUT_HIGH);
    // Pad_Config(BUTTON_1, PAD_SW_MODE, PAD_IS_PWRON, PAD_PULL_UP, PAD_OUT_DISABLE, PAD_OUT_HIGH);
    // Pad_Config(BUTTON_2, PAD_SW_MODE, PAD_IS_PWRON, PAD_PULL_UP, PAD_OUT_DISABLE, PAD_OUT_HIGH);

    Pad_Config(LED1_R, PAD_SW_MODE, PAD_IS_PWRON, PAD_PULL_NONE, PAD_OUT_ENABLE, PAD_OUT_LOW);
    Pad_Config(LED2_G, PAD_SW_MODE, PAD_IS_PWRON, PAD_PULL_NONE, PAD_OUT_ENABLE, PAD_OUT_LOW);
    Pad_Config(LED3_B, PAD_SW_MODE, PAD_IS_PWRON, PAD_PULL_NONE, PAD_OUT_ENABLE, PAD_OUT_LOW);
    Pad_Config(LED4_C, PAD_SW_MODE, PAD_IS_PWRON, PAD_PULL_NONE, PAD_OUT_ENABLE, PAD_OUT_LOW);
    Pad_Config(LED5_W, PAD_SW_MODE, PAD_IS_PWRON, PAD_PULL_NONE, PAD_OUT_ENABLE, PAD_OUT_LOW);

    /* INIT BUTTON IRQ*/
    RCC_PeriphClockCmd(APBPeriph_GPIO, APBPeriph_GPIO_CLOCK, ENABLE);

    Pad_Config(BUTTON_0, PAD_PINMUX_MODE, PAD_IS_PWRON, PAD_PULL_UP, PAD_OUT_DISABLE, PAD_OUT_HIGH);
    Pinmux_Config(BUTTON_0, DWGPIO);

    /* Config WakeUp pin */
    System_WakeUpPinEnable(BUTTON_0, PAD_WAKEUP_POL_LOW, PAD_WK_DEBOUNCE_DISABLE, 0);

    GPIO_InitTypeDef GPIO_struct_init;
    GPIO_StructInit(&GPIO_struct_init);

    GPIO_struct_init.GPIO_Pin = GPIO_PIN_BUTTON_0;
    GPIO_struct_init.GPIO_Mode = GPIO_Mode_IN;
    GPIO_struct_init.GPIO_DebounceTime = 10;
    GPIO_struct_init.GPIO_ITTrigger = GPIO_INT_Trigger_EDGE;
    GPIO_struct_init.GPIO_ITCmd = ENABLE;
    GPIO_struct_init.GPIO_ITDebounce = GPIO_INT_DEBOUNCE_ENABLE;
    GPIO_struct_init.GPIO_ITPolarity = GPIO_INT_POLARITY_ACTIVE_LOW;
    GPIO_Init(&GPIO_struct_init);

    NVIC_InitTypeDef NVIC_InitStruct;
    NVIC_InitStruct.NVIC_IRQChannel = GPIO_BUTTON0_IRQn;
    NVIC_InitStruct.NVIC_IRQChannelPriority = 3;
    NVIC_InitStruct.NVIC_IRQChannelCmd = ENABLE;
    NVIC_Init(&NVIC_InitStruct);

    GPIO_MaskINTConfig(GPIO_PIN_BUTTON_0, DISABLE);
    GPIO_INTConfig(GPIO_PIN_BUTTON_0, ENABLE);

    /* register nuttx IRQ*/
    extern void up_enable_irq(int irq);
    up_enable_irq(GPIO_Group2_VECTORn);
    irq_attach(GPIO_Group2_VECTORn, GPIO_INPUT_HANDLER, NULL);

    return 0;
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
        buf->len = MIN(strlen("1234567890abcdefghijklmn"), 62);
        memcpy(buf->buff, "1234567890abcdefghijklmn", buf->len);
        break;
    default:
        buf->code = MI_ERR_NOT_FOUND;
        return;
    }

    buf->code = MI_SUCCESS;

    return;
}

static void mible_conn_timeout_handler(void * arg)
{
    mible_status_t status;

    miio_timer_delete(conn_timer);
    conn_timer = NULL;

    if (is_logined == true) {
        return;
    }

    printk("mible conn timeout");

    status = mible_gap_disconnect(0);
    if (status) {
        printk("Unable disconnect %d", status);
    }
}

static void user_event_cb(mible_user_event_type_t event, void *user_data)
{
    static const uint8_t *const strs[] = {
        [MIBLE_USER_PROV_CB_TYPE_UNPROV] = "Unprovision",
        [MIBLE_USER_PROV_CB_TYPE_PROVED] = "Provisioned",
        [MIBLE_USER_PROV_CB_TYPE_START] = "Provision start",
        [MIBLE_USER_PROV_CB_TYPE_COMPLETE] = "Provision completed",
        [MIBLE_USER_PROV_CB_TYPE_FAIL] = "Provision failed",
        [MIBLE_USER_PROV_CB_TYPE_RESET] = "Provision Reseted",
        [MIBLE_USER_PROV_CB_TYPE_RECOVERY] = "Provision Recoveried",
        [MIBLE_USER_LOGIN_CB_TYPE_START] = "Login start",
        [MIBLE_USER_LOGIN_CB_TYPE_COMPLETE] = "Login completed",
        [MIBLE_USER_LOGIN_CB_TYPE_FAIL] = "Login failed",
        [MIBLE_USER_GAP_CB_CONNECTED] = "Connected",
        [MIBLE_USER_GAP_CB_DISCONNECTED] = "Disconnected",
        [MIBLE_USER_SCENE_CB_ADD] = "Scene added",
        [MIBLE_USER_SCENE_CB_DELETE] = "Scene deleted",
        [MIBLE_USER_SCENE_CB_RECALL] = "Scene recalled",
    };

    if (event >= ARRAY_SIZE(strs)) {
        return;
    }

    printk("%s\n", strs[event]);
    switch (event)
    {
    case MIBLE_USER_PROV_CB_TYPE_UNPROV:
        printk("MIBLE_USER_PROV_CB_TYPE_UNPROV\n");
        is_provisioned = false;
        user_scan_check();
        //TODO: stop adv after 30min or start scan for remote controller
        //mible_mesh_device_scan_set(MESH_SCAN_LOW);
        //miio_system_set_adv_timeout(1800000);
#if CONFIG_BT_PER_ADV_SYNC
        main_lpn_record_reset();
#endif
        break;
    case MIBLE_USER_PROV_CB_TYPE_FAIL:
        is_provisioned = false;
        break;
    case MIBLE_USER_PROV_CB_TYPE_START:
        break;
    case MIBLE_USER_PROV_CB_TYPE_RECOVERY:
    case MIBLE_USER_PROV_CB_TYPE_COMPLETE:
        printk("MIBLE_USER_PROV_CB_TYPE_COMPLETE\n");
        is_provisioned = true;
        user_scan_check();
        //TODO: ADD Period Power Properties
        miio_mesh_properties_period_publish_init(6, 1, 3600);
        miio_mesh_properties_period_publish_init(6, 2, 600);
        //keep adv always on
        miio_system_set_adv_timeout(0xFFFFFFFF);
#if CONFIG_BT_PER_ADV_SYNC
        main_lpn_per_adv_pub_req_init();
        miio_mesh_properties_period_publish_init(3, 3, 0);
        miio_mesh_properties_period_publish_init(128, 128, 0);
#endif
        break;
    case MIBLE_USER_PROV_CB_TYPE_PROVED:
        if (reset_timer) {
            break;
        }
        printk("MIBLE_USER_PROV_CB_TYPE_PROVED\n");
        is_provisioned = true;
        user_scan_check();
        //TODO: ADD Period Power Properties
        miio_mesh_properties_period_publish_init(6, 1, 3600);
        miio_mesh_properties_period_publish_init(6, 2, 600);
        //keep adv always on
        miio_system_set_adv_timeout(0xFFFFFFFF);
#if CONFIG_BT_PER_ADV_SYNC
        main_lpn_per_adv_pub_req_init();
        miio_mesh_properties_period_publish_init(3, 3, 0);
        miio_mesh_properties_period_publish_init(128, 128, 0);
#endif
        break;
    case MIBLE_USER_PROV_CB_TYPE_RESET:
        break;
    case MIBLE_USER_GAP_CB_CONNECTED:
    {
        int err;

        err = miio_timer_create(&conn_timer,
                        mible_conn_timeout_handler,
                        MIBLE_TIMER_SINGLE_SHOT);
        if (err) {
            printk("Unable create conn timeout timer");
        } else {
            miio_timer_start(conn_timer, 20000, NULL);
        }

        is_connected = true;
        is_logined = false;
#if CONFIG_BT_PER_ADV_SYNC
        if(is_pa_sync){
            printk("GAP connected,padv sync need terminate\n");
            mesh_vnd_per_adv_sync_stop();
        }
#else
        user_scan_check();
#endif
    }break;
    case MIBLE_USER_GAP_CB_DISCONNECTED:
    {
        if (conn_timer) {
            miio_timer_delete(conn_timer);
            conn_timer = NULL;
        }

        is_connected = false;
        is_logined = false;
#if CONFIG_BT_PER_ADV_SYNC
        if(two_key_switch.right_mode != 0){
            uint32_t random = main_lpn_rand(1,20);
            printk("GAP disconnected,padv request after random:%ld(s)\n",random);
            uint32_t start_timeout = random * 1000;
            if(sync_timer !=NULL){
                miio_timer_start(sync_timer, start_timeout, NULL);
            }
        }
#else
        user_scan_check();
#endif
    }break;
    case MIBLE_USER_LOGIN_CB_TYPE_COMPLETE:
    {
        is_logined = true;
    }break;
    case MIBLE_USER_SCENE_CB_ADD:
    {
        //Confirmed subscribe 0xFEFE
        extern struct bt_mesh_elem elements[];
        struct bt_mesh_model *mod = &elements[0].vnd_models[0];

        uint16_t *entry = bt_mesh_model_find_group(&mod, MIBLE_MESH_DEVICE_GROUP);
        if(entry == NULL){
            entry = bt_mesh_model_find_group(&mod, BT_MESH_ADDR_UNASSIGNED);
            if(entry){
                *entry = MIBLE_MESH_DEVICE_GROUP;
                if (IS_ENABLED(CONFIG_BT_SETTINGS)) {
                    bt_mesh_model_sub_store(mod);
                }
                printk("success sub dev group addr %04x!!\n", MIBLE_MESH_DEVICE_GROUP);
            }
        }
    }break;
    default:
        break;
    }
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

static void P_1_1_Manufacturer_doGet(mible_spec_bearer_t bearer, mible_spec_property_operation_t *o)
{
}

static void P_1_2_Model_doGet(mible_spec_bearer_t bearer, mible_spec_property_operation_t *o)
{
}

static void P_1_3_SerialNumber_doGet(mible_spec_bearer_t bearer, mible_spec_property_operation_t *o)
{
}

static void P_1_4_FirmwareRevision_doGet(mible_spec_bearer_t bearer,
                     mible_spec_property_operation_t *o)
{
}

static void P_2_1_On_doGet(mible_spec_bearer_t bearer, mible_spec_property_operation_t *o)
{
    mible_spec_property_value_new_boolean(&o->value, two_key_switch.left_on);
}

static void P_2_1_On_doSet(mible_spec_bearer_t bearer, mible_spec_property_operation_t *o)
{
    if (o->value.format != MIBLE_SPEC_PROPERTY_FORMAT_BOOL)
    {
        o->code = MIBLE_SPEC_OPERATION_ERROR_VALUE;
        return;
    }

    key1_onoff_set(o->value.data.boolean);
#if CONFIG_BT_PER_ADV_SYNC
    if(!is_pa_sync){
        user_scan_check();
    }
#endif
    o->code = MIBLE_SPEC_OPERATION_OK;
}

static void P_2_2_Mode_doGet(mible_spec_bearer_t bearer, mible_spec_property_operation_t *o)
{
    mible_spec_property_value_new_uchar(&o->value, two_key_switch.left_mode);
}

static void P_2_2_Mode_doSet(mible_spec_bearer_t bearer, mible_spec_property_operation_t *o)
{
    if (o->value.format != MIBLE_SPEC_PROPERTY_FORMAT_UCHAR && 
        o->value.format != MIBLE_SPEC_PROPERTY_FORMAT_NUMBER)
    {
        o->code = MIBLE_SPEC_OPERATION_ERROR_VALUE;
        return;
    }

    two_key_switch.left_mode = o->value.data.ucharValue;
    o->code = MIBLE_SPEC_OPERATION_OK;
}

static void P_2_3_Fault_doGet(mible_spec_bearer_t bearer, mible_spec_property_operation_t *o)
{
    mible_spec_property_value_new_uchar(&o->value, two_key_switch.fault);
}

static void P_2_4_Anti_Flicker_doGet(mible_spec_bearer_t bearer, mible_spec_property_operation_t *o)
{
    mible_spec_property_value_new_boolean(&o->value, two_key_switch.anti_flk);
}

static void P_2_4_Anti_Flicker_doSet(mible_spec_bearer_t bearer, mible_spec_property_operation_t *o)
{
    if (o->value.format != MIBLE_SPEC_PROPERTY_FORMAT_BOOL)
    {
        o->code = MIBLE_SPEC_OPERATION_ERROR_VALUE;
        return;
    }

    two_key_switch.anti_flk = o->value.data.boolean;
    user_scan_check();

    o->code = MIBLE_SPEC_OPERATION_OK;
}

static void P_2_1_On_doAction(mible_spec_bearer_t bearer, mible_spec_action_operation_t *o)
{
    mible_spec_property_value_t value;

    if(two_key_switch.left_on){
        two_key_switch.left_on = false;
    }else{
        two_key_switch.left_on = true;
    }
    key1_onoff_set(two_key_switch.left_on);

    mible_spec_property_value_new_boolean(&value, two_key_switch.left_on);
    miio_mesh_properties_changed(2, 1, &value);

#if CONFIG_BT_PER_ADV_SYNC
    if(!is_pa_sync){
        user_scan_check();
    }
#endif

    o->vals.size = 0;
    o->code = MIBLE_SPEC_OPERATION_OK;
}

static void P_3_1_On_doGet(mible_spec_bearer_t bearer, mible_spec_property_operation_t *o)
{
    mible_spec_property_value_new_boolean(&o->value, two_key_switch.right_on);
}

static void P_3_1_On_doSet(mible_spec_bearer_t bearer, mible_spec_property_operation_t *o)
{
    if (o->value.format != MIBLE_SPEC_PROPERTY_FORMAT_BOOL)
    {
        o->code = MIBLE_SPEC_OPERATION_ERROR_VALUE;
        return;
    }

    key2_onoff_set(o->value.data.boolean);

#if CONFIG_BT_PER_ADV_SYNC
        if(!is_pa_sync){
            user_scan_check();
        }
#endif

    o->code = MIBLE_SPEC_OPERATION_OK;
}

static void P_3_2_Mode_doGet(mible_spec_bearer_t bearer, mible_spec_property_operation_t *o)
{
    mible_spec_property_value_new_uchar(&o->value, two_key_switch.right_mode);
}

static void P_3_2_Mode_doSet(mible_spec_bearer_t bearer, mible_spec_property_operation_t *o)
{
    if (o->value.format != MIBLE_SPEC_PROPERTY_FORMAT_UCHAR && 
        o->value.format != MIBLE_SPEC_PROPERTY_FORMAT_NUMBER)
    {
        o->code = MIBLE_SPEC_OPERATION_ERROR_VALUE;
        return;
    }
#if CONFIG_BT_PER_ADV_SYNC
    two_key_switch.right_mode = o->value.data.ucharValue;
    main_lpn_padv_adv_sync_request((bool)two_key_switch.right_mode);
#else
    two_key_switch.right_mode = o->value.data.ucharValue;
#endif
    o->code = MIBLE_SPEC_OPERATION_OK;
}

static void P_3_3_Status_doGet(mible_spec_bearer_t bearer, mible_spec_property_operation_t *o)
{
    //siid.piid just for test
#if CONFIG_BT_PER_ADV_SYNC
    mible_spec_property_value_new_uchar(&o->value, (is_pa_sync==0)?1:2);
#else
    mible_spec_property_value_new_uchar(&o->value, 1);
#endif
}

static void P_3_1_On_doAction(mible_spec_bearer_t bearer, mible_spec_action_operation_t *o)
{
    mible_spec_property_value_t value;

    if(two_key_switch.right_on){
        two_key_switch.right_on = false;
    }else{
        two_key_switch.right_on = true;
    }
    key2_onoff_set(two_key_switch.right_on);

    mible_spec_property_value_new_boolean(&value, two_key_switch.right_on);
    miio_mesh_properties_changed(3, 1, &value);

#if CONFIG_BT_PER_ADV_SYNC
        if(!is_pa_sync){
            user_scan_check();
        }
#endif

    o->vals.size = 0;
    o->code = MIBLE_SPEC_OPERATION_OK;
}

static void P_6_1_Power_Consumption_doGet(mible_spec_bearer_t bearer, mible_spec_property_operation_t *o)
{
    mible_spec_property_value_new_float(&o->value, two_key_switch.power_consumption);
    two_key_switch.power_consumption += 1.0;
}

static void P_6_2_Electric_Power_doGet(mible_spec_bearer_t bearer, mible_spec_property_operation_t *o)
{
    mible_spec_property_value_new_ushort(&o->value, two_key_switch.power_electric);
    two_key_switch.power_electric += 100;
}

static void P_6_3_Accumulation_doGet(mible_spec_bearer_t bearer, mible_spec_property_operation_t *o)
{
    mible_spec_property_value_new_boolean(&o->value, two_key_switch.accumulation);
}

static void P_7_1_Indicator_Light_doGet(mible_spec_bearer_t bearer, mible_spec_property_operation_t *o)
{
    mible_spec_property_value_new_boolean(&o->value, false);
}

static void P_7_1_Indicator_Light_doSet(mible_spec_bearer_t bearer, mible_spec_property_operation_t *o)
{

}

static void P_128_1_GMT_Offset_doSet(mible_spec_bearer_t bearer, mible_spec_property_operation_t *o)
{
    two_key_switch.gmt_offset = o->value.data.longValue;
    o->code = MIBLE_SPEC_OPERATION_OK;
}

static void P_128_2_Weather_doSet(mible_spec_bearer_t bearer, mible_spec_property_operation_t *o)
{
    two_key_switch.weather = o->value.data.longValue;
    o->code = MIBLE_SPEC_OPERATION_OK;
}

static void P_128_3_UTC_Time_doSet(mible_spec_bearer_t bearer, mible_spec_property_operation_t *o)
{
    two_key_switch.utc_time = o->value.data.ulongValue;
    o->code = MIBLE_SPEC_OPERATION_OK;
}

#if CONFIG_BT_PER_ADV_SYNC
static void P_128_128_Sync_Addr_doGet(mible_spec_bearer_t bearer, mible_spec_property_operation_t *o)
{
    if(is_pa_sync){
        per_adv_sync_gw_info_t gw_info;
        mesh_vnd_per_adv_get_gw_info(&gw_info);

        mible_spec_property_value_new_ushort(&o->value, gw_info.addr);
    }else{
        mible_spec_property_value_new_ushort(&o->value, 0);
    }
    
}
#endif

MIBLE_SPEC_DEFINE(spec_handlers)
    MIBLE_SPEC_SIID_START(1)
        MIBLE_SPEC_HANDLER(1, P_1_1_Manufacturer_doGet, NULL, NULL),
        MIBLE_SPEC_HANDLER(2, P_1_2_Model_doGet, NULL, NULL),
        MIBLE_SPEC_HANDLER(3, P_1_3_SerialNumber_doGet, NULL, NULL),
        MIBLE_SPEC_HANDLER(4, P_1_4_FirmwareRevision_doGet, NULL, NULL),
    MIBLE_SPEC_SIID_END

    MIBLE_SPEC_SIID_START(2)
        MIBLE_SPEC_HANDLER(1, P_2_1_On_doGet, P_2_1_On_doSet, P_2_1_On_doAction),
        MIBLE_SPEC_HANDLER(2, P_2_2_Mode_doGet, P_2_2_Mode_doSet, NULL),
        MIBLE_SPEC_HANDLER(3, P_2_3_Fault_doGet, NULL, NULL),
        MIBLE_SPEC_HANDLER(4, P_2_4_Anti_Flicker_doGet, P_2_4_Anti_Flicker_doSet, NULL),
    MIBLE_SPEC_SIID_END

    MIBLE_SPEC_SIID_START(3)
        MIBLE_SPEC_HANDLER(1, P_3_1_On_doGet, P_3_1_On_doSet, P_3_1_On_doAction),
        MIBLE_SPEC_HANDLER(2, P_3_2_Mode_doGet, P_3_2_Mode_doSet, NULL),
        MIBLE_SPEC_HANDLER(3, P_3_3_Status_doGet, NULL, NULL),
    MIBLE_SPEC_SIID_END

    MIBLE_SPEC_SIID_START(6)
        MIBLE_SPEC_HANDLER(1, P_6_1_Power_Consumption_doGet, NULL, NULL),
        MIBLE_SPEC_HANDLER(2, P_6_2_Electric_Power_doGet, NULL, NULL),
        MIBLE_SPEC_HANDLER(3, P_6_3_Accumulation_doGet, NULL, NULL),
    MIBLE_SPEC_SIID_END

    MIBLE_SPEC_SIID_START(7)
        MIBLE_SPEC_HANDLER(1, P_7_1_Indicator_Light_doGet, P_7_1_Indicator_Light_doSet, NULL),
    MIBLE_SPEC_SIID_END

    MIBLE_SPEC_SIID_START(128)
        MIBLE_SPEC_HANDLER(1, NULL, P_128_1_GMT_Offset_doSet, NULL),
        MIBLE_SPEC_HANDLER(2, NULL, P_128_2_Weather_doSet, NULL),
        MIBLE_SPEC_HANDLER(3, NULL, P_128_3_UTC_Time_doSet, NULL),
#if CONFIG_BT_PER_ADV_SYNC
        MIBLE_SPEC_HANDLER(128, P_128_128_Sync_Addr_doGet, NULL, NULL),
#endif
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
    [MIBLE_SPEC_BEARER_MESH] = "[MESH]",
};

static void do_spec_get(mible_spec_bearer_t bearer, mible_spec_property_operation_t *o)
{
    const struct mible_spec_piid_or_aiid_list *i;

    if (bearer >= ARRAY_SIZE(spec_bearers)) {
        return;
    }

    printk("%s %s siid %d piid %d\n", __FUNCTION__, spec_bearers[bearer], o->siid, o->piid);

    i = find_spec_handler(o->siid, o->piid);
    if (!i || !(i->get)) {
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

    printk("%s %s siid %d piid %d arg[size %d]\n", __FUNCTION__, spec_bearers[bearer], o->siid, o->aiid,
           o->vals.size);

    i = find_spec_handler(o->siid, o->aiid);
    if (!i || !(i->invoke)) {
        return;
    }

    i->invoke(bearer, o);
}

#if CONFIG_BT_PER_ADV_SYNC
static void sync_event_handler(per_adv_sync_dev_state_t status, uint16_t node, void * data)
{
    switch (status) {
    case PER_ADV_SYNC_GW_NODE_FIND_NONE:
        printk("GW node find none\n");
        is_pa_sync = false;
        //user_scan_check should be called,donot remove this function
        user_scan_check();
        break;
    case PER_ADV_SYNC_LOST:
        printk("Exit per adv lost, padv from %04X\n\n", node);
        is_pa_sync = false;
        user_scan_check();
        uint32_t random = main_lpn_rand(1,20);
        uint32_t retry_timeout = random * 1000;
        if(sync_timer !=NULL){
            miio_timer_start(sync_timer, retry_timeout, NULL);
        }

        //test siid/piid
        main_lpn_per_adv_property_change(3,3,(is_pa_sync==0)?1:2);
        break;

    case PER_ADV_SYNC_SUCCESS:
        printk("Enter per adv recv mode, padv from %04X\n\n", node);
        main_lpn_gw_info_restore();
        is_pa_sync = true;
        last_timeout = 0;
        if(sync_timer !=NULL){
            miio_timer_stop(sync_timer);
        }

        //test siid/piid
         main_lpn_per_adv_property_change(3,3,(is_pa_sync==0)?1:2);
        break;

    case PER_ADV_SYNC_STOP:
        printk("Per adv stop, padv from %04X\n\n", node);
        last_timeout = 0;
        is_pa_sync = false;
        //user_scan_check should be called,donot remove this function
        user_scan_check();
        if(sync_timer !=NULL){
            miio_timer_stop(sync_timer);
        }

        //test siid/piid
         main_lpn_per_adv_property_change(3,3,(is_pa_sync==0)?1:2);
        break;
    default:
        break;
    }
}
#endif

#define APP_CFG_INFO __attribute__((section(".cfg"))) __attribute__((used))
const volatile struct mible_config APP_CFG_INFO mi_config = {
    .magic = 0xdeadbeef,
    .cfg =
    {
        .pid = PRODUCT_ID,
        .dev_version = DEVELOPER_VERSION,
        .model_name = MODEL_NAME,

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
        #if defined(CONFIG_BT_MESH_PSK)
            .psk_init = mible_psk_auth_enable,
        #endif
        #if defined(CONFIG_BT_MESH_PATH)
            .path_init = mible_path_ota_enable,
        #endif
        #if defined(CONFIG_BT_PER_ADV_SYNC)
            .lpn_init = mible_pa_lpn_enable,
        #endif
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
    #if defined(CONFIG_BT_PER_ADV_SYNC)
        .pa_lpn.sync = sync_event_handler,
    #endif
    },
};

uint32_t scan_count, ext_adv_cnt, lost_cnt, per_adv_cnt;

static void scan_recv(const struct bt_le_scan_recv_info *info,
            struct net_buf_simple *buf)
{
    scan_count++;
    if(info->adv_type == BT_GAP_ADV_TYPE_EXT_ADV){
        ext_adv_cnt++;
    }
}

static struct bt_le_scan_cb ext_scan_cb = {
    .recv = scan_recv,
};

static void pwr_timer_handler(void * p_context)
{
    int err;
    uint32_t pwr_cnt;

    if (!p_context) {
        miio_timer_delete(pwn_timer);
        pwn_timer = NULL;

        err = miio_record_delete(RECORD_ID_POWER_ON_COUNT);
        if (err) {
            printk("Unable to delete recoid id:0x%04x",
                      RECORD_ID_POWER_ON_COUNT);
        }

        return;
    }

    pwr_cnt = (uint32_t)p_context;

    miio_timer_start(pwn_timer, SWITCH_POWER_ON_TIME, NULL);

    err = miio_record_write(RECORD_ID_POWER_ON_COUNT,
                (uint8_t *)&pwr_cnt, sizeof(pwr_cnt));
    if (err) {
        printk("Unable to store recoid id:0x%04x",
                  RECORD_ID_POWER_ON_COUNT);
    }
}

static bool switch_flash_restore(void)
{
    uint32_t pwr_cnt = 0;
    int ret;

    ret = miio_record_read(RECORD_ID_POWER_ON_COUNT,
                   (uint8_t *)&pwr_cnt, sizeof(pwr_cnt));
    if (ret != MI_SUCCESS) {
        goto store;
    }

    printk("Power on count %ld\n", pwr_cnt);

    if (pwr_cnt >= SWITCH_POWER_ON_COUNT) {
        return true;
    }

store:
    pwr_cnt++;

    ret = miio_timer_create(&pwn_timer,
                pwr_timer_handler,
                MIBLE_TIMER_SINGLE_SHOT);
    if (ret) {
        printk("Unable to create timer for power reset");
        return false;
    }

    miio_timer_start(pwn_timer, 1000, (void *)pwr_cnt);

    return false;
}

static void node_restore_timer_handler(void * arg)
{
    int err;

    err = miio_record_delete(RECORD_ID_POWER_ON_COUNT);
    if (err) {
        printk("Unable to delete recoid id:0x%04x",
                  RECORD_ID_POWER_ON_COUNT);
    }

    //use param 0 to close recovery
    mible_mesh_restore(0);
}

static void app_periodic_handler(void * arg)
{
    struct mallinfo info;
    int err = 0;

    info = mallinfo();
    printk("Heap left:%d Scan cnt:%ld, ext cnt:%ld, per cnt %ld\n",
        info.fordblks, scan_count, ext_adv_cnt, per_adv_cnt);

    scan_count = 0;
    ext_adv_cnt = 0;
    per_adv_cnt = 0;

    err = ioctl(fd_wd, WDIOC_KEEPALIVE, 0);
    if(err < 0){
        printk("wd feed fail %d\n", err);
    }
}

/* Used for factory test */
typedef struct {
    uint8_t major;
    uint8_t minor;
    uint8_t revision;
    uint8_t padding;
    uint16_t developer_version;

    uint16_t  p;
    const uint8_t   *m;

    uint32_t magic;
} info_t;

#define APP_VERSION_INFO __attribute__((section(".info"))) __attribute__((used))

const info_t mi_info APP_VERSION_INFO = {
    .major = MIBLE_LIB_MAJOR,
    .minor = MIBLE_LIB_MINOR,
    .revision = MIBLE_LIB_REVISION,
    .developer_version = DEVELOPER_VERSION,

    .p = PRODUCT_ID,
    .m = MODEL_NAME,

    .magic = 0x12345678,
};

#if CONFIG_BT_PER_ADV_SYNC
void main_lpn_record_reset(void)
{
    mible_status_t ret = MI_SUCCESS;
    uint8_t enable = 0;
    ret = miio_record_write(RECORD_ID_LPN_MODE_STATE,&enable, sizeof(uint8_t));
    if (ret !=MI_SUCCESS) {
        printk("Unable to reset lpn_mode\n");
    }

    per_adv_sync_gw_info_t gw_info = {0};
    ret = miio_record_write(RECORD_ID_SYNC_GW_INFO,(uint8_t *)&gw_info, sizeof(gw_info));
    if (ret !=MI_SUCCESS) {
        printk("Unable to reset gw info\n");
    }
}

void main_lpn_per_adv_property_change(uint8_t siid,uint8_t piid,uint8_t is_sync)
{
    mible_spec_property_value_t value;
    mible_spec_property_value_new_uchar(&value, is_sync);
    mible_mesh_send_property_changed(siid, piid, &value);
}

void main_lpn_padv_adv_sync_request(bool enable)
{
    mible_status_t ret = MI_SUCCESS;

    ret = miio_record_write(RECORD_ID_LPN_MODE_STATE,(uint8_t *)&enable, sizeof(bool));
    if (ret !=MI_SUCCESS) {
        printk("Unable to store lpn_mode\n");
    }

    //printk("is_pa_sync:%d,enable:%d\n",is_pa_sync,enable);

    if(enable){
        uint32_t random = main_lpn_rand(1,20);
        printk("Per adv request after random:%ld(s)\n",random);
        uint32_t start_timeout = random * 1000;
        if(sync_timer !=NULL){
            miio_timer_start(sync_timer, start_timeout, NULL);
        }
    }else{
       mesh_vnd_per_adv_sync_stop();
    }
}

mible_status_t main_lpn_gw_info_restore(void)
{
    mible_status_t ret = MI_ERR_INVALID_PARAM;
    per_adv_sync_gw_info_t gw_info;

    //get getway info
    mesh_vnd_per_adv_get_gw_info(&gw_info);

    if (gw_info.addr == 0 || gw_info.addr > 0x7FFF){
        printk("Store sync_ctx param error\n");
        return MI_ERR_INVALID_PARAM;
    }

    if((gw_info_record.addr == gw_info.addr)&&
                (gw_info_record.net_idx == gw_info.net_idx)){
        printk("GW info is same,restore ignore\n");
        return MI_SUCCESS;
    }

    ret = miio_record_write(RECORD_ID_SYNC_GW_INFO,
                    (uint8_t *)&gw_info, sizeof(gw_info));
    if (ret !=MI_SUCCESS) {
        printk("Unable to store gw info\n");
    }

    return ret;
}

mible_status_t main_lpn_gw_info_read(void)
{
    int ret;

    ret = miio_record_read(RECORD_ID_SYNC_GW_INFO,
                   (uint8_t *)&gw_info_record, sizeof(gw_info_record));

    if (ret !=MI_SUCCESS) {
        printk("Unable to read gw info\n");
        return MI_ERR_NOT_FOUND;
    }

    if (gw_info_record.addr == 0 || gw_info_record.addr > 0x7FFF){
        printk("Read sync_ctx param error\n");
        return MI_ERR_INVALID_PARAM;
    }

    mesh_vnd_per_adv_set_gw_info(gw_info_record.addr,gw_info_record.net_idx,gw_info_record.app_idx);
    return MI_SUCCESS;
}

static void main_lpn_per_adv_sync_timer_handler(void * arg)
{
    uint32_t timeout_min = 0;
    uint32_t random_min = 0;
    if(!is_pa_sync){
        int err = mesh_vnd_per_adv_sync_request();
        printk("Start sync req err:%d\n",err);

        if(last_timeout < PER_ADV_REQ_MAX_TIMEOUT){
            random_min = main_lpn_rand(1,10);
            timeout_min =2 * last_timeout + random_min;

            if(timeout_min > PER_ADV_REQ_MAX_TIMEOUT){
                timeout_min = PER_ADV_REQ_MAX_TIMEOUT;
            }

            last_timeout = timeout_min;
        }else{
            timeout_min = PER_ADV_REQ_MAX_TIMEOUT;
        }

        printk("Next sync req:%ld(s),random:%ld(m)\n",timeout_min*60,random_min);
        miio_timer_start(sync_timer, timeout_min*60*1000, NULL);
    }else{
        last_timeout = 0;
    }
}

void main_lpn_per_adv_pub_req_init(void)
{
    printk("Start lpn init\n");

    if(mi_config.cfg.init.lpn_init == NULL){
        return;
    }

    int err;
    err = mi_config.cfg.init.lpn_init();
    if(err < 0){
        printk("LPN init failed (err %d)\n", err);
    }

    err = miio_timer_create(&sync_timer,
                    main_lpn_per_adv_sync_timer_handler,
                    MIBLE_TIMER_SINGLE_SHOT);
    if (err < 0) {
        printk("Unable to create sync timer\n");
    }

    mible_status_t ret = miio_record_read(RECORD_ID_LPN_MODE_STATE,
                    (uint8_t *)&two_key_switch.right_mode, sizeof(uint8_t));
    if (ret !=MI_SUCCESS) {
        printk("Unable to read lpn_mode\n");
        return;
    }

    if (two_key_switch.right_mode == 0) {
        printk("LPN mode is off\n");
        return;
    }

    ret  = main_lpn_gw_info_read();

    if((ret == MI_SUCCESS)&&(err == 0)){
        uint32_t random = main_lpn_rand(1,20);
        uint32_t first_boot_time = random * 1000;
        miio_timer_start(sync_timer, first_boot_time, NULL);
    }
}

uint32_t main_lpn_rand(uint32_t down, uint32_t up)
{
    uint32_t val, diff = up - down;
    int err;

    err = bt_rand(&val, sizeof(val));//__rtl_platform_random()
    if (err) {
        return down;
    }

    return down + (val % diff);
}
#endif

T_SW_RESET_REASON reset_reason_8762e(void)
{
    T_SW_RESET_REASON sw_reset_type = __rtl_reset_reason_get();
    uint32_t wdg_reset_lr = (__rtl_btaon_fast_read_safe(0x1c6) << 16) | (__rtl_btaon_fast_read_safe(0x1c4));

    if (wdg_reset_lr & 0xffff0000) {
        if (sw_reset_type == RESET_REASON_HW) {
            sw_reset_type = RESET_REASON_WDG_TIMEOUT;
        }
    } else {
        sw_reset_type = RESET_REASON_HW;
    }

    return sw_reset_type;
}

int main(int argc, FAR char *argv[])
{
    int err;

    printk("Model Name %s PID %ld Develop Version %d\n",
            mi_config.cfg.model_name, mi_config.cfg.pid, mi_config.cfg.dev_version);

    err = wdg_init();
    if (err) {
        printk("watchdog init failed (err %d)\n", err);
        return err;
    }

    err = board_init();
    if (err) {
        printk("board init failed (err %d)\n", err);
        return err;
    }

    /* let's wait for a while avoid power reset */
    // Note: delay init bt for Capacitor charging when Relay on
    // Boot time ~ 0.37s * 4.5mA, delay time ~ 2s * 0.02mA
    if (IS_ENABLED(CONFIG_PM)) {
        k_sleep(K_SECONDS(2));
    }

    extern int zblue_main(void);
    err = zblue_main();
    if (err) {
        printk("zblue init failed (err %d)\n", err);
        return err;
    }

    err = bt_enable(NULL);
    if (err) {
        printk("Bluetooth init failed (err %d)\n", err);
        return err;
    }

    err = mible_service_init();
    if (err) {
        printk("mi service init failed (err %d)\n", err);
        return err;
    }

    err = mible_mesh_start();
    if (err) {
        printk("mesh start failed (err %d)\n", err);
        return err;
    }
    bt_le_scan_cb_register(&ext_scan_cb);

    if (switch_flash_restore()) {
        printk("miio_system_restore\n");
        /* Call restore later, since mijia uninit yet.  */
        err = miio_timer_create(&reset_timer,
                    node_restore_timer_handler,
                    MIBLE_TIMER_SINGLE_SHOT);
        if (err) {
            printk("Unable to create restore timer");
            return err;
        }

        /* Indicate system restore in progress. */
        miio_timer_start(reset_timer, 1000, NULL);
    }

    err = miio_timer_create(&app_handle, app_periodic_handler,
                 MIBLE_TIMER_REPEATED);
    if (err) {
        printk("Unable to create app timer");
        return err;
    }
    miio_timer_start(app_handle, 5000, NULL);

#if defined(CONFIG_BT_MESH_STATISTIC)
    mible_trace_update_reboot_reason(reset_reason_8762e());
    mible_trace_init();
#endif

    while (1) {
        mible_events_process();
    }

    return 0;
}
