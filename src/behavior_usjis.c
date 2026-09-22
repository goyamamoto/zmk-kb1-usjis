/* SPDX-License-Identifier: MIT
 *
 * &usjis USJIS_OFF | USJIS_ON | USJIS_TOG: US-JIS substitution mode operations.
 * Acts on the press; the release does nothing. Nothing reaches the host.
 */
#define DT_DRV_COMPAT zmk_behavior_usjis

#include <zephyr/device.h>
#include <zephyr/logging/log.h>
#include <drivers/behavior.h>
#include <zmk/behavior.h>
#include <zmk/usjis.h>

LOG_MODULE_DECLARE(usjis, CONFIG_ZMK_USJIS_LOG_LEVEL);

static int on_pressed(struct zmk_behavior_binding *binding, struct zmk_behavior_binding_event event) {
    switch (binding->param1) {
    case ZMK_USJIS_OP_OFF:
    case ZMK_USJIS_OP_ON:
    case ZMK_USJIS_OP_TOGGLE:
        zmk_usjis_request((enum zmk_usjis_op)binding->param1);
        return ZMK_BEHAVIOR_OPAQUE;
    default:
        LOG_ERR("unknown usjis operation %u", binding->param1);
        return -ENOTSUP;
    }
}

static int on_released(struct zmk_behavior_binding *binding, struct zmk_behavior_binding_event event) {
    return ZMK_BEHAVIOR_OPAQUE;
}

static int behavior_usjis_init(const struct device *dev) { return 0; }

static const struct behavior_driver_api behavior_usjis_driver_api = {
    .binding_pressed = on_pressed,
    .binding_released = on_released,
};

#define USJIS_INST(n)                                                                              \
    DEVICE_DT_INST_DEFINE(n, behavior_usjis_init, NULL, NULL, NULL, APPLICATION,                   \
                          CONFIG_KERNEL_INIT_PRIORITY_DEFAULT, &behavior_usjis_driver_api);

DT_INST_FOREACH_STATUS_OKAY(USJIS_INST)
