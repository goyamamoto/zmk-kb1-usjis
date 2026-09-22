# firmwareのライセンスと配布方針

> この文書は[英語版](licenses.md)の日本語訳です。内容が食い違う場合は英語版を正本とします。

buildしたfirmwareに含まれるコードのライセンスについて、固定したKeychron ZMK（`c284513085c005edf5f9a52b28c8090cc6ed01d2`）と下記の出典から読み取った事実と、そこから決めた方針です。技術的な記録であり、法的助言ではありません。

## 配布方針

- **このリポジトリが配布するのはソースだけです。** このプロジェクトのmoduleとkeymap（MIT）、west manifest、script、test、文書を含みます。Keychron ZMK、QMK、Keyboard Quantizerのコードは含まず、バイナリも含みません。
- **firmwareのバイナリ（`.uf2`、`.hex`、`.elf`）は公開しません。** releaseにも、GitHub Actionsのartifactにも、IssueやPull Requestの添付にも置きません。Pull Requestにはローカルbuildの SHA-256 だけを記録します。
- **利用者は各自で** `bash scripts/build-firmware.sh` **でbuildします。** scriptは固定したKeychron ZMKとZephyrのソースをローカルのworkspaceへ取得します。自分のキーボードのためにbuildして使うことは、何も配布しません。

理由: buildしたfirmwareは、複数の条件のコード（下記）をlinkした1つの結合物です。これを配布するには、含まれるGPL-2.0-or-laterのコードと両立する条件で結合物全体を配布できる必要がありますが、2.4 GHzのbinary library（Nordic 5-Clause、ソースなし）と、ライセンス表記のないKeychronのファイルは、それを満たしません。Keychron自身は同じ組み合わせを配布していますが、それはKeychronの判断であり、他者に権利を与えるものではありません。2.4 GHz libraryを外してbuildしても、後者の問題は残ります。

## buildしたfirmwareに含まれるもの

| 部分 | ライセンス | 出典 |
| --- | --- | --- |
| このプロジェクトのmodule（`src/`、`include/`、`dts/`）とkeymap | MIT | このリポジトリ |
| ZMK | MIT | 固定したKeychron ZMK、upstream ZMK |
| Zephyrとそのmodule | Apache-2.0（moduleは各自のライセンス） | 固定したZephyr `1ae0eb5ce8adafcec993e6fb8f4eeb6f818a7772` |
| 表記のあるKeychron Launcherコード | GPL-2.0-or-later | 固定したKeychron ZMKの`app/src/launcher/` |
| 表記のないKeychron Launcherコード（`launcher.c`、`launcher.h`、`map_keycode.c`） | 大部分がQMK（GPL-2.0-or-later）、KeychronのQMK fork（GPL-2.0-or-later）、Zephyr（Apache-2.0）、ZMK（MIT）と一致。詳細は下記 | 固定したKeychron ZMK |
| Keychron独自のコード（kscan、ble、leds、dfu、24Gの接続部など） | ライセンス表記なし | 固定したKeychron ZMK |
| 2.4 GHz link library `lib_nrf_esb_24G.a` | LicenseRef-Nordic-5-Clause、バイナリのみ | 固定したKeychron ZMKの`app/src/24G/` |

## Launcherコードと2.4 GHz libraryについての事実

- 固定基点のLauncherの`app/src/launcher/dynamic_keymap.c`、`send_string.c`、`mousekey.c`はGPL-2.0-or-laterの表記を持ち、`CONFIG_ZMK_LAUNCHER`（既定で有効）のときにbuildへ入る。
- GPL-2.0-or-later表記のheaderは`mousekey.h`、`keycodes.h`、`send_string.h`、`send_string_keycodes.h`、`sendstring_japanese.h`、`keymap_japanese.h`、`dynamic_keymap.h`、`dynamic_macro.h`。このうち`mousekey.h`、`keycodes.h`、`send_string.h`（とそこからincludeされる`send_string_keycodes.h`）は、この設定に関係なく`hog.c`、`endpoints.c`、`behavior_macro.c`からincludeされる。`dynamic_macro.h`はどこからもincludeされていない。
- Launcherの設定に関係なく、本体が次のようにLauncherのコードを使うため、Launcherを外すのは容易ではない。`endpoints.c`は`mousekey_send()`を、`hog.c`は`mousekey_get_report()`を、`behavior_macro.c`は`send_string_with_delay()`と`dynamic_keymap_macro_send()`を呼ぶ。`keymap.c`は`launcher.h`（GPL-2.0-or-later表記の`keycodes.h`をinclude）をincludeし、`via_ee_device`と`dynamic_keymap_set_keycode()`を使う。`dfu/tdfu.c`は`launcher.c`の`zmk_usb_hid_via_send()`を呼ぶ。
- `launcher.c`、`launcher.h`、`map_keycode.c`にはライセンス表記がない。その多くの部分が、他のプロジェクトのコードと空白を除いて一致する。
  - QMK（GPL-2.0-or-later）: `launcher.c`は`quantum/via.c`の大半（`raw_hid_receive`、`via_init`、`via_qmk_*`のhandler等のVIAのcommand処理と、そのコメント）を含む。`launcher.h`は`quantum/via.h`の大半（`id_bootloader_jump`等のVIAのcommand定義を含む）と`quantum/dynamic_keymap.c`の一部を含む。`map_keycode.c`は`tmk_core/protocol/report.h`の`KEYCODE2SYSTEM`と`KEYCODE2CONSUMER`と、`quantum/keymap_common.c`の`action_for_keycode()`のcase labelとコメントを含む。短い行の一部はQMKの別の場所にもある（例: `raw_hid_send`の冒頭は`tmk_core/protocol/chibios/usb_main.c`）。
  - QMK `b31426252ed379937132ed7d2baf84a677123b5b`で、連続する空白を1つにまとめ両端を除いた後の20文字以上の行（`#include`で始まる行を除く）を重複なく数えると、`launcher.c`は`quantum/via.c`のそうした行339行のうち276行を、`launcher.h`は`quantum/via.h`の147行のうち135行を含む。
  - QMKのコードは`b31426252e`（2023-11-01）以降のもの。Launcherの`keycodes.h`が定義し`map_keycode.c`が使う`BASIC_KEYCODE_RANGE`は、QMKがこのcommitで追加した。また`via.c`と`via.h`の一致した部分を変える`2b00b846dc`（2025-03-21）より前のもの。KeychronのQMK fork（branch `wls_2025q1`、commit `c8b2b237b4ef09765e48cbb5c1fdb0afd164289f`）にも同じ`via.c`と`via.h`があるため、どのリポジトリから写したかは特定できない。
  - KeychronのQMK fork（GPL-2.0-or-later）: `launcher.c`のfactory testの処理（`factory_test_send`、`factory_test_rx`、`dip_switch_update_user`とcommand定義）は、`keyboards/keychron/common/factory_test.c`（`https://github.com/Keychron/qmk_firmware`、branch `wireless_playground`、commit `666862cb8123b64a6b96718d739c6203ad99031f`。「Copyright 2021~2025 @ Keychron」）を縮めた写しである。
  - Zephyr（Apache-2.0）: `launcher.c`の設定読込の補助（`direct_immediate_value`、`direct_loader_immediate_value`、`load_immediate_value`）は、固定したZephyrの`samples/subsys/settings/src/main.c`（「Copyright (c) 2019 Nordic Semiconductor ASA」）と一致する。
  - ZMK（MIT）: `launcher.h`の`ZMK_HID_MAIN_VAL_*`の定義とHID descriptorの項目は固定基点のZMKの`app/include/zmk/hid.h`と、`launcher.c`のUSB HIDの一部の行は`app/src/usb_hid.c`と一致する（「Copyright (c) 2020 The ZMK Contributors」）。
  - QMKの表記は、`via.c`、`via.h`、`dynamic_keymap.c`がJason Williams (Wilba)、`report.h`と`keymap_common.c`がJun Wako。3ファイルのすべての行の由来をたどったわけではない。
- 2.4 GHzには`app/src/24G/lib_nrf_esb_24G.a`（binary library）があり、Bシリーズ共通の`app/boards/arm/keychron/keychron_defconfig`（`CONFIG_ZMK_NRF_24G_ECB=y`）でlinkされる。`app/src/24G/CMakeLists.txt`のSPDXは`LicenseRef-Nordic-5-Clause`。
- 現段階で適法・違反を断定しない。結合物の配布と個別コードのライセンスの区別は[GNUのFAQ](https://www.gnu.org/licenses/gpl-faq.html.en)を参照する。
