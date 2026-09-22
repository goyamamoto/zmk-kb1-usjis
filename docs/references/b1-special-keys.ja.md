# B1 Pro US Fn combo キー参照（固定基点）

> この文書は[英語版](b1-special-keys.md)の日本語訳です。内容が食い違う場合は英語版を正本とします。

Keychron ZMK固定commit `c284513085c005edf5f9a52b28c8090cc6ed01d2`の`app/boards/shields/keychron/b1/us/keychron_b1_us.keymap`、`app/src/combo.c`、`app/src/launcher/`を静的に読んだ結果です。対象はFnレイヤーでcomboとして実装されている特殊操作に限ります。実機での確認は行っていません。

## 1. 割り当て

| 位置 | キー | combo | 条件 | 動作 |
| --- | --- | --- | --- | --- |
| 49, 56 | J, Z | `combo_jz` | Fn押下中、1秒以内に同時押し、3秒保持 | `OUT_RECOVER`（`zmk_factory_recover`、工場出荷状態へ復元） |
| 57, 51 | X, L | `combo_xl` | 同上 | `OUT_FN`（F1〜F12行の入れ替え。動的keymapを書き換え1.5秒後に保存） |
| 60 | B | `combo_b` | Fn押下中、単キー | 押下中`OUT_BAT`（電池残量LED） |
| 68 | Win / Option | `combo_win_l` | Fn押下中、3秒保持 | `OUT_FN_WIN`（Winロック切替） |

- comboは`layers = <1 3>`で、Mac側Fnレイヤー（1）とWin側Fnレイヤー（3）の両方で有効です。
- J、Z、X、L、Win単体はFnレイヤーで`&trans`です。J、Z、X、Lはcombo不成立時に通常レイヤーの文字が出ます。Winは常に成立する1キーのcomboです。Kはcomboに含まれません。
- `combo_jz`、`combo_xl`、`combo_win_l`の先は`tapping-term-ms = <3000>`のtap-preferred hold-tapで、tap側は`&none`です。

## 2. 反応

- **保留:** `combo.c`は候補キーの押下を、相手キーが来るか、解放されるか、`timeout-ms`（1000）を過ぎるまでホストへ送りません。Fnを押しながらJ、Z、X、Lを押しっぱなしにすると最大1秒遅れて出ます。単発tapは解放時に送られます。comboはキーの位置で決まるため、Launcherでこれらの位置に割り当て直したキーも同じように保留されます。このfirmwareは`combo_jz`と`combo_xl`を削除しているため、保留はなくなっています（第4節）。
- **BとWinは単キーcomboなので即発火**し、保留はありません。`combo.c`は、候補が1つだけになり、そのキーがすべて押された時点でcomboを働かせます。
- **Fn+Win / Fn+Optionのtapは何も出ません。** hold-tapのtap側が`&none`のためです。3秒保持でのみWinロックが切り替わります。
- **3秒保持の間、他のキーを押すとhold-tapの判定に影響します。** tap-preferredなので、保持中に別キーを押した場合の挙動は実機で確認が必要です。

## 3. Launcherとの関係

- comboはdevicetree固定で、Launcher（VIA互換の動的keymap）にcombo APIはありません。J、Z、X、L、Winに別のkeycodeを割り当ててもcomboは物理位置で発火し続けます。
- 逆に、firmwareは電池残量とWinロックを単キーのcustom keycode（`UC_BATINFO` → `&out OUT_BAT`、`QK_MAGIC_TOGGLE_GUI` → `lp_fn_win`）として受け付けます。ただし`0x0711`向けのLauncher定義（`875824913.json`）には「Win Lock」はあるが「Battery Level」はなく、Launcherの一覧から電池残量を選べるかは未確認です。F行入れ替えは`UC_KEYCHRON`相当のマクロ経由で、工場リセットは単キーの表現がありません。

## 4. このfirmwareでの変更

- `combo_jz`と`combo_xl`を削除したため、Fn押下中のJ、Z、X、Lは遅延なく送られます。
- 工場リセット（`OUT_RECOVER`）はFn+Shift+Escの10秒保持へ移しました。Fn+Esc単体はEscのままです。
- F行入れ替え（`OUT_FN`）はFn+Caps Lockの3秒保持へ移しました。
- `combo_b`（電池残量）と`combo_win_l`（Winロック）は変更していません。

`config/keychron_b1_us.keymap`と[README](../../README.ja.md#このfirmwareがすること)を参照してください。
