# B1 Pro US Fn Combo Key Reference (Pinned Baseline)

This is the result of statically reading `app/boards/shields/keychron/b1/us/keychron_b1_us.keymap`, `app/src/combo.c` and `app/src/launcher/` at the pinned Keychron ZMK commit `c284513085c005edf5f9a52b28c8090cc6ed01d2`. It covers only the special operations that are implemented as combos on the Fn layers. Nothing here has been checked on hardware.

## 1. Assignments

| Position | Key | Combo | Condition | Action |
| --- | --- | --- | --- | --- |
| 49, 56 | J, Z | `combo_jz` | While Fn is held, pressed together within 1 second, held for 3 seconds | `OUT_RECOVER` (`zmk_factory_recover`, restores the factory state) |
| 57, 51 | X, L | `combo_xl` | Same as above | `OUT_FN` (swaps the F1–F12 row; rewrites the dynamic keymap and saves it after 1.5 seconds) |
| 60 | B | `combo_b` | While Fn is held, single key | `OUT_BAT` while pressed (battery level LED) |
| 68 | Win / Option | `combo_win_l` | While Fn is held, held for 3 seconds | `OUT_FN_WIN` (toggles Win lock) |

- The combos use `layers = <1 3>`, so they are active on both the Mac Fn layer (1) and the Win Fn layer (3).
- J, Z, X, L and Win alone are `&trans` on the Fn layers. For J, Z, X and L, when a combo does not complete, the character of the normal layer is output. Win is a single-key combo that always completes. K is not part of any combo.
- `combo_jz`, `combo_xl` and `combo_win_l` lead to tap-preferred hold-taps with `tapping-term-ms = <3000>`, whose tap side is `&none`.

## 2. Response

- **Deferral:** `combo.c` does not send the press of a candidate key to the host until the partner key arrives, the key is released, or `timeout-ms` (1000) passes. Holding down J, Z, X or L while Fn is held outputs the key up to 1 second late. A single tap is sent on release. Because combos are position-based, keys remapped at these positions in the Launcher are deferred in the same way. This firmware removes `combo_jz` and `combo_xl`, so the deferral is gone (section 4).
- **B and Win are single-key combos, so they fire immediately** and have no deferral. `combo.c` fires a combo as soon as it is the only candidate and fully pressed.
- **A tap of Fn+Win / Fn+Option outputs nothing.** This is because the tap side of the hold-tap is `&none`. Only a 3-second hold toggles Win lock.
- **Pressing other keys during the 3-second hold affects the hold-tap decision.** Because the hold-tap is tap-preferred, the behavior when another key is pressed during the hold needs to be checked on hardware.

## 3. Relation to Launcher

- The combos are fixed in the devicetree, and Launcher (a VIA-compatible dynamic keymap) has no combo API. Even if other keycodes are assigned to J, Z, X, L and Win, the combos keep firing at the physical positions.
- Conversely, the firmware accepts battery level and Win lock as single-key custom keycodes (`UC_BATINFO` → `&out OUT_BAT`, `QK_MAGIC_TOGGLE_GUI` → `lp_fn_win`). However, the Launcher definition for `0x0711` (`875824913.json`) has "Win Lock" but not "Battery Level", so whether battery level can be selected from the Launcher list is unverified. The F-row swap goes through a macro equivalent to `UC_KEYCHRON`, and factory reset has no single-key representation.

## 4. Changes in this firmware

- `combo_jz` and `combo_xl` are removed, so J, Z, X and L are sent without delay while Fn is held.
- Factory reset (`OUT_RECOVER`) moves to Fn+Shift+Esc held for 10 seconds; Fn+Esc alone stays Esc.
- F-row swap (`OUT_FN`) moves to Fn+Caps Lock held for 3 seconds.
- `combo_b` (battery level) and `combo_win_l` (Win lock) are unchanged.

See `config/keychron_b1_us.keymap` and the [README](../../README.md#what-the-firmware-does).
