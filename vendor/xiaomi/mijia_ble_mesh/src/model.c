/* model.c */

/*
 * Copyright (c) 2023 Xiaomi.inc
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/types.h>
#include <stddef.h>
#include <string.h>
#include <errno.h>
#include <sys/byteorder.h>
#include <kernel.h>
#include <sys/crc.h>
#include <settings/settings.h>
#include <net/buf.h>
#include <bluetooth/bluetooth.h>
#include <bluetooth/hci.h>
#include <bluetooth/conn.h>
#include <bluetooth/uuid.h>
#include <bluetooth/gatt.h>
#include <bluetooth/mesh.h>

#include "mible.h"
#include "mible_mesh.h"

#if defined(CONFIG_PM)
#define MI_MESH_TEMPLATE_TWO_KEY_SWITCH
#else
#define MI_MESH_TEMPLATE_LIGHTCTL
#endif

extern const struct bt_mesh_model_op mible_mesh_cfg_srv_op[];
extern const struct bt_mesh_model_cb mible_mesh_cfg_srv_cb;

extern const struct bt_mesh_model_op mible_mesh_gen_onoff_srv_op[];
extern const struct bt_mesh_model_op mible_mesh_light_lightness_srv_op[];
extern const struct bt_mesh_model_op mible_mesh_light_ctl_tmp_srv_op[];

extern const struct bt_mesh_model_op mible_mesh_vendor_miot_op[];
extern const struct bt_mesh_model_op mible_mesh_vendor_mjia_op[];

static struct bt_mesh_model root_models[] = {
    BT_MESH_MODEL_CB(BT_MESH_MODEL_ID_CFG_SRV, mible_mesh_cfg_srv_op, NULL, NULL,
             &mible_mesh_cfg_srv_cb),
#if defined(MI_MESH_TEMPLATE_LIGHTNESS) || defined(MI_MESH_TEMPLATE_LIGHTCTL) ||                   \
    defined(MI_MESH_TEMPLATE_ONE_KEY_SWITCH) || defined(MI_MESH_TEMPLATE_TWO_KEY_SWITCH) ||    \
    defined(MI_MESH_TEMPLATE_THREE_KEY_SWITCH) || defined(MI_MESH_TEMPLATE_FAN)
    BT_MESH_MODEL(BT_MESH_MODEL_ID_GEN_ONOFF_SRV, mible_mesh_gen_onoff_srv_op, NULL, NULL),
#endif
#if defined(MI_MESH_TEMPLATE_LIGHTNESS) || defined(MI_MESH_TEMPLATE_LIGHTCTL)
    BT_MESH_MODEL(BT_MESH_MODEL_ID_LIGHT_LIGHTNESS_SRV, mible_mesh_light_lightness_srv_op, NULL,
              NULL),
#endif
};

static struct bt_mesh_model vendor_models[] = {
    BT_MESH_MODEL_VND(MIBLE_MESH_COMPANY_ID_XIAOMI, MIBLE_MESH_MIOT_SPEC_SERVER_MODEL,
              mible_mesh_vendor_miot_op, NULL, NULL),
    BT_MESH_MODEL_VND(MIBLE_MESH_COMPANY_ID_XIAOMI, MIBLE_MESH_MIJIA_SERVER_MODEL,
              mible_mesh_vendor_mjia_op, NULL, NULL)};

static struct bt_mesh_model secondary_0_models[] = {
#if defined(MI_MESH_TEMPLATE_TWO_KEY_SWITCH) || defined(MI_MESH_TEMPLATE_THREE_KEY_SWITCH)
    BT_MESH_MODEL(BT_MESH_MODEL_ID_GEN_ONOFF_SRV, mible_mesh_gen_onoff_srv_op, NULL, NULL),
#endif
#if defined(MI_MESH_TEMPLATE_LIGHTCTL)
    BT_MESH_MODEL(BT_MESH_MODEL_ID_LIGHT_CTL_TEMP_SRV, mible_mesh_light_ctl_tmp_srv_op, NULL,
              NULL),
#endif
};

#if defined(MI_MESH_TEMPLATE_THREE_KEY_SWITCH)
static struct bt_mesh_model secondary_1_models[] = {
    BT_MESH_MODEL(BT_MESH_MODEL_ID_GEN_ONOFF_SRV, mible_mesh_gen_onoff_srv_op, NULL, NULL),
};
#endif

struct bt_mesh_elem elements[] = {
    BT_MESH_ELEM(0, root_models, vendor_models),
#if defined(MI_MESH_TEMPLATE_TWO_KEY_SWITCH) || defined(MI_MESH_TEMPLATE_LIGHTCTL) ||              \
    defined(MI_MESH_TEMPLATE_THREE_KEY_SWITCH)
    BT_MESH_ELEM(0, secondary_0_models, BT_MESH_MODEL_NONE),
#endif
#if defined(MI_MESH_TEMPLATE_THREE_KEY_SWITCH)
    BT_MESH_ELEM(0, secondary_1_models, BT_MESH_MODEL_NONE),
#endif
};

const struct bt_mesh_comp mible_mesh_comp = {
    .cid = BT_COMP_ID_LF,
    .elem = elements,
    .elem_count = ARRAY_SIZE(elements),
};

const struct mible_mesh_model_spec mible_mesh_model_lists[] = {
    {0, 0, 0, 1, &vendor_models[0]},
    {0, 0, 0, 1, &vendor_models[1]},
#if defined(MI_MESH_TEMPLATE_LIGHTNESS) || defined(MI_MESH_TEMPLATE_LIGHTCTL) ||                   \
    defined(MI_MESH_TEMPLATE_ONE_KEY_SWITCH) || defined(MI_MESH_TEMPLATE_TWO_KEY_SWITCH) ||    \
    defined(MI_MESH_TEMPLATE_THREE_KEY_SWITCH) || defined(MI_MESH_TEMPLATE_FAN)
    {2, 1, 0, 0, &root_models[1]},
#if defined(MI_MESH_TEMPLATE_TWO_KEY_SWITCH) || defined(MI_MESH_TEMPLATE_THREE_KEY_SWITCH)
    {3, 1, 1, 0, &secondary_0_models[0]},
#if defined(MI_MESH_TEMPLATE_THREE_KEY_SWITCH)
    {4, 1, 2, 0, &secondary_1_models[0]},
#endif
#elif defined(MI_MESH_TEMPLATE_LIGHTNESS) || defined(MI_MESH_TEMPLATE_LIGHTCTL)
    {2, 2, 0, 0, &root_models[2]},
#if defined(MI_MESH_TEMPLATE_LIGHTCTL)
    {2, 3, 1, 0, &secondary_0_models[ARRAY_SIZE(secondary_0_models) - 1]},
#endif
#endif
#endif
    {0, 0, 0, 0, NULL},
};
