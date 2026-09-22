# Licenses of the Firmware and Distribution Policy

Facts about the licenses of the code that the built firmware contains, read from the pinned Keychron ZMK (`c284513085c005edf5f9a52b28c8090cc6ed01d2`) and the sources named below, and the policy that follows from them. This is an engineering record, not legal advice.

## Distribution policy

- **This repository distributes source only.** It contains this project's module and keymap (MIT), the west manifest, scripts, tests and documents. It contains no code from Keychron ZMK, QMK or the Keyboard Quantizer, and no binary.
- **No firmware binary (`.uf2`, `.hex`, `.elf`) is published**: not in releases, not as GitHub Actions artifacts, not attached to Issues or Pull Requests. Pull Requests record the SHA-256 of local builds instead.
- **Users build the firmware themselves** with `bash scripts/build-firmware.sh`, which fetches the pinned Keychron ZMK and Zephyr sources into a local workspace. Building and using the firmware on one's own keyboard does not distribute anything.

Why: the built firmware is one combined work that links code under several terms (section below). Distributing it would require the whole work to be distributable under terms compatible with the GPL-2.0-or-later code it contains, and the 2.4 GHz binary library (Nordic 5-Clause, no source) and the Keychron files without a license notice do not provide that. Keychron distributes the same combination itself; that is Keychron's own decision and gives no rights to others. Building without the 2.4 GHz library would not remove the second problem.

## What the built firmware contains

| Part | License | Source |
| --- | --- | --- |
| This project's module (`src/`, `include/`, `dts/`) and keymap | MIT | This repository |
| ZMK | MIT | Pinned Keychron ZMK, upstream ZMK |
| Zephyr and its modules | Apache-2.0 (and the modules' own licenses) | Pinned Zephyr `1ae0eb5ce8adafcec993e6fb8f4eeb6f818a7772` |
| Keychron Launcher code with notices | GPL-2.0-or-later | `app/src/launcher/` of the pinned Keychron ZMK |
| Keychron Launcher code without notices (`launcher.c`, `launcher.h`, `map_keycode.c`) | Largely matches QMK (GPL-2.0-or-later), Keychron's QMK fork (GPL-2.0-or-later), Zephyr (Apache-2.0) and ZMK (MIT); details below | Pinned Keychron ZMK |
| Keychron's own code (kscan, ble, leds, dfu, 24G glue and others) | No license notice | Pinned Keychron ZMK |
| 2.4 GHz link library `lib_nrf_esb_24G.a` | LicenseRef-Nordic-5-Clause, binary only | `app/src/24G/` of the pinned Keychron ZMK |

## Facts about the Launcher code and the 2.4 GHz library

- In the pinned baseline, the Launcher files `app/src/launcher/dynamic_keymap.c`, `send_string.c` and `mousekey.c` carry GPL-2.0-or-later notices and are built when `CONFIG_ZMK_LAUNCHER` is set (enabled by default).
- The headers with GPL-2.0-or-later notices are `mousekey.h`, `keycodes.h`, `send_string.h`, `send_string_keycodes.h`, `sendstring_japanese.h`, `keymap_japanese.h`, `dynamic_keymap.h` and `dynamic_macro.h`. Of these, `mousekey.h`, `keycodes.h` and `send_string.h` (and `send_string_keycodes.h`, which it includes) are included from `hog.c`, `endpoints.c` and `behavior_macro.c` regardless of this setting. `dynamic_macro.h` is not included anywhere.
- Regardless of the Launcher setting, the main code uses Launcher code as follows, so removing the Launcher is not easy. `endpoints.c` calls `mousekey_send()`, `hog.c` calls `mousekey_get_report()`, and `behavior_macro.c` calls `send_string_with_delay()` and `dynamic_keymap_macro_send()`. `keymap.c` includes `launcher.h` (which includes `keycodes.h`, with a GPL-2.0-or-later notice) and uses `via_ee_device` and `dynamic_keymap_set_keycode()`. `dfu/tdfu.c` calls `zmk_usb_hid_via_send()` in `launcher.c`.
- `launcher.c`, `launcher.h` and `map_keycode.c` have no license notice. Large parts of them match, apart from whitespace, code from other projects:
  - QMK (GPL-2.0-or-later): `launcher.c` contains most of `quantum/via.c` (the VIA command handling, such as `raw_hid_receive`, `via_init` and the `via_qmk_*` handlers, with their comments). `launcher.h` contains most of `quantum/via.h` (including VIA command definitions such as `id_bootloader_jump`) and part of `quantum/dynamic_keymap.c`. `map_keycode.c` contains the `KEYCODE2SYSTEM` and `KEYCODE2CONSUMER` helpers of `tmk_core/protocol/report.h`, and the case labels and comments of `action_for_keycode()` in `quantum/keymap_common.c`. A few short lines also appear elsewhere in QMK, for example the start of `raw_hid_send` in `tmk_core/protocol/chibios/usb_main.c`.
  - Measured at QMK `b31426252ed379937132ed7d2baf84a677123b5b`, counting distinct lines of 20 or more characters after collapsing runs of whitespace to one space and trimming both ends, and excluding lines that start with `#include`: `launcher.c` contains 276 of the 339 such lines of `quantum/via.c`, and `launcher.h` contains 135 of the 147 such lines of `quantum/via.h`.
  - The QMK code is from `b31426252e` (2023-11-01) or later: the Launcher's `keycodes.h` defines, and `map_keycode.c` uses, `BASIC_KEYCODE_RANGE`, which QMK added in that commit. It is from before `2b00b846dc` (2025-03-21), which changes the matched text of `via.c` and `via.h`. Keychron's QMK fork has the same `via.c` and `via.h` (branch `wls_2025q1`, commit `c8b2b237b4ef09765e48cbb5c1fdb0afd164289f`), so which repository was copied cannot be determined.
  - Keychron's QMK fork (GPL-2.0-or-later): the factory test handling of `launcher.c` (`factory_test_send`, `factory_test_rx`, `dip_switch_update_user` and the command definitions) is a reduced copy of `keyboards/keychron/common/factory_test.c` (`https://github.com/Keychron/qmk_firmware`, branch `wireless_playground`, commit `666862cb8123b64a6b96718d739c6203ad99031f`; "Copyright 2021~2025 @ Keychron").
  - Zephyr (Apache-2.0): the settings-loading helpers of `launcher.c` (`direct_immediate_value`, `direct_loader_immediate_value`, `load_immediate_value`) match `samples/subsys/settings/src/main.c` of the pinned Zephyr ("Copyright (c) 2019 Nordic Semiconductor ASA").
  - ZMK (MIT): the `ZMK_HID_MAIN_VAL_*` definitions and HID descriptor items of `launcher.h` match `app/include/zmk/hid.h` of the pinned ZMK, and some USB HID lines of `launcher.c` match its `app/src/usb_hid.c` ("Copyright (c) 2020 The ZMK Contributors").
  - The QMK notices are by Jason Williams (Wilba) for `via.c`, `via.h` and `dynamic_keymap.c`, and by Jun Wako for `report.h` and `keymap_common.c`. Not every line of the three files has been traced.
- For 2.4 GHz there is `app/src/24G/lib_nrf_esb_24G.a` (a binary library), linked through `app/boards/arm/keychron/keychron_defconfig` (`CONFIG_ZMK_NRF_24G_ECB=y`), which is shared by the B series. The SPDX identifier in `app/src/24G/CMakeLists.txt` is `LicenseRef-Nordic-5-Clause`.
- At this stage, do not conclude compliance or violation. For the distinction between distributing a combined work and the licenses of individual code, see the [GNU FAQ](https://www.gnu.org/licenses/gpl-faq.html.en).
