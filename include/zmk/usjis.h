/* SPDX-License-Identifier: MIT */
#pragma once

#include <stdbool.h>
#include <stdint.h>

/* US-JIS substitution: public API of the module (docs/design/usjis-*.md). */

enum zmk_usjis_op { ZMK_USJIS_OP_OFF = 0, ZMK_USJIS_OP_ON = 1, ZMK_USJIS_OP_TOGGLE = 2 };

/* Effective mode (spec section 3). Substitution also requires an active layer
 * in CONFIG_ZMK_USJIS_LAYER_MASK (the Win position of the OS switch on the B1 Pro). */
bool zmk_usjis_is_enabled(void);
/* True while a mode request waits for all non-modifier keys to be released. */
bool zmk_usjis_is_pending(void);
/* A mode operation (spec section 7). Returns the requested mode. */
bool zmk_usjis_request(enum zmk_usjis_op op);
/* Number of press records currently held (substituted or not). */
unsigned zmk_usjis_active_entries(void);
/* False if the startup listener-order check failed; the module then never substitutes. */
bool zmk_usjis_listener_order_ok(void);
/* Number of mode values written to settings since boot (0 without settings). */
unsigned zmk_usjis_settings_writes(void);
