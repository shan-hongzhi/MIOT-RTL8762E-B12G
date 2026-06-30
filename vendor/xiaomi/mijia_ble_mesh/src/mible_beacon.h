#ifndef MIBLE_BEACON_H_
#define MIBLE_BEACON_H_

#include <zephyr/types.h>
#include <stdbool.h>
#include <stdint.h>
#include <sys/atomic.h>
#include "mible.h"


/**
 * @note mibeacon object advertising configuration
 *
 * The mibeacon object is an adv message contains the status or event. BLE gateway
 * can receive the beacon message (by active scanning) and upload it to server for
 * triggering customized home automation scene.
 *
 * OBJ_QUEUE_SIZE      : max num of objects can be concurrency advertising
 *                       ( actually, it will be sent one by one )
 * OBJ_ADV_INTERVAL_MS : the object adv interval
 * OBJ_ADV_TIMEOUT_MS  : the time one object will be continuously sent.
 */

#ifndef OBJ_QUEUE_SIZE
#define OBJ_QUEUE_SIZE         8
#endif

#ifndef OBJ_ADV_INTERVAL_MS
#define OBJ_ADV_INTERVAL_MS    30
#endif

#ifndef OBJ_ADV_TIMEOUT_MS
#define OBJ_ADV_TIMEOUT_MS     1000
#endif


/**
 * @note Device side has RESET button.
 */
#ifndef HAVE_RESET_BUTTON
#define HAVE_RESET_BUTTON      1
#endif

/**
 * @note Device side has bind confirm button.
 */
#ifndef HAVE_CONFIRM_BUTTON
#define HAVE_CONFIRM_BUTTON    0
#endif

typedef uint8_t mible_addr_t[6];

typedef struct {
    uint8_t         reserved0           :1;
    uint8_t         reserved1           :1;
    uint8_t         compacted           :1;
    uint8_t         is_encrypt          :1;
    uint8_t         mac_include         :1;
    uint8_t         cap_include         :1;
    uint8_t         obj_include         :1;
    uint8_t         mesh_include        :1;

    uint8_t         registered          :1;
    uint8_t         solicite            :1;
    uint8_t         auth_mode           :2;
    uint8_t         version             :4;
} mibeacon_frame_ctrl_t;

typedef struct {
    void            *node;
    uint16_t        type;
    uint8_t         len;
    uint8_t         need_encrypt;
    uint8_t         compacted;
    uint8_t         val[12];
    atomic_t        flags;
 } mibeacon_obj_t;

struct mibeacon_obj {
    uint16_t        *type;
    uint8_t         len;
    uint8_t         *need_encrypt;
    uint8_t         *val;
 };

typedef struct {
    uint8_t         connectable  :1;
    uint8_t         centralable  :1;
    uint8_t         encryptable  :1;
    uint8_t         bondAbility  :2;
    uint8_t         IO_capability:1;
    uint8_t         reserved     :2;
} mibeacon_capability_t;

typedef struct {
    uint8_t in_digits            :1;
    uint8_t RFU0                 :1;
    uint8_t RFU1                 :1;
    uint8_t RFU2                 :1;
    uint8_t out_digits           :1;
    uint8_t RFU3                 :1;
    uint8_t RFU4                 :1;
    uint8_t out_image            :1;

    uint8_t reserved             :8;
} mibeacon_cap_sub_io_t;

typedef struct {
    uint8_t         pb_adv       :1;
    uint8_t         pb_gatt      :1;
    uint8_t         state        :2;
    uint8_t         version      :4;

    uint8_t         reserved     :8;
} mibeacon_mesh_t;

typedef struct {
    mibeacon_frame_ctrl_t   frame_ctrl;
    uint32_t                pid;
    mible_addr_t           *p_mac;
    mibeacon_capability_t  *p_capability;
    mibeacon_cap_sub_io_t  *p_cap_sub_IO;
    uint8_t                *p_wifi_mac;
    mibeacon_obj_t         **p_obj;
    uint8_t                 obj_num;
    mibeacon_mesh_t        *p_mesh;
} mibeacon_config_t;

typedef union {
    struct{
        uint16_t piid : 9;
        uint16_t flag : 1;
        uint16_t siid : 4;
        uint16_t head : 2;
    } prop;
    struct{
        uint16_t eiid : 9;
        uint16_t flag : 1;
        uint16_t siid : 4;
        uint16_t head : 2;
    } event;
    struct{
        uint16_t id : 4;
        uint16_t reserved :12;
    } packed;
    uint16_t all;
} spec_id_v2_t;

typedef struct{
    uint16_t object_id;
    uint8_t  len;
    uint8_t  val[12];
    uint8_t  remain;
    uint32_t seqnum;
}mible_arch_mibeacon_t;

void advertising_init(uint8_t solicite_bind);
int advertising_start(uint16_t adv_interval_ms);

int mibeacon_property_changed(uint8_t siid, uint16_t piid, mible_spec_property_value_t *newValue, uint8_t isUrgent);
int mibeacon_event_occurred(uint8_t siid, uint16_t eiid, mible_spec_arguments_t *newArgs, uint8_t isUrgent);

int mibeacon_init(uint8_t key[16], bool newone);
int mible_adv_start(void);
int mible_adv_suspend(void);
int mible_adv_resume(void);

#endif //MIBLE_BEACON_H_
