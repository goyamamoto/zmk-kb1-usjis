# ZMK US-JIS for Keychron B1 Pro

> この文書は[英語版](README.md)の日本語訳です。内容が食い違う場合は英語版を正本とします。

Keychron B1 Pro US配列（PID `0x0711`）を、日本語キーボードとして固定されたホストでもキーキャップの印字どおりに打てるようにし、あわせて純正firmwareの使いにくい点を直したfirmwareです。KeychronのZMK forkの上に載せるZephyr moduleとkeymapで、コンテナ内で再現可能にbuildします。

## 対応機種: まずPIDを確認してください

KeychronはB1 Proという1つの名前で複数の版を売っています。このfirmwareはそのうちの1つ専用で、別の版に書き込んではいけません。版によってキーマトリクスと動いているfirmwareが違うため、合わないと良くてもキーが化けたり効かなくなったりします。

| USB PID | Keychronの版 | 配列 | ここでの扱い |
| --- | --- | --- | --- |
| `0x0711` | B1 Pro US、ZMKベース | ANSI | **対応**: このfirmwareの対象。実機で確認済み |
| `0x071a` | B1 Pro US「n」版、ZMKベース | ANSI | **未検証のbuildあり**: `SHIELD=keychron_b1_usn bash scripts/build-firmware.sh build`で`keychron-b1-usn-usjis.uf2`ができる。まだ誰も書き込んでいない。[n版](#n版0x071a未検証)を参照 |
| `0x0714` | B1 Pro（公式の一覧ではANSI） | ANSI | 非対応: firmwareは暗号化された`.kfw`で配布され、ソースは非公開。ZMKかどうかも不明 |
| `0x0712`、`0x071b`、`0x0713`、`0x071c` | UK版とJIS版 | ISO、JIS | 非対応 |

PIDの調べ方:

- Keychron Launcher: Settings → Device Info。
- macOS: `ioreg -p IOUSB -l -w0 | grep -A25 'Keychron B1 Pro@' | grep -E '"(idVendor|idProduct)"'`。値は10進で、`1809`が`0x0711`です。
- Windows: デバイスマネージャー → キーボード → 詳細 → ハードウェアID。`VID_3434&PID_0711`が含まれます。
- Linux: `lsusb`で`3434:0711`と表示されます。

`0x0711`のときだけ既定のbuildへ進んでください。`0x071a`は下の節を読んで自分で判断してください。それ以外の版では、公式firmwareのまま使うのが安全です。この表は、固定したKeychron ZMKにある各shieldのキーマトリクスとベース層のkeycode、および[Keychronのfirmwareダウンロード一覧](https://www.keychron.com/blogs/archived/how-to-use-the-launcher-web-app-or-manually-flash-firmware-for-your-b-pro-series-keyboard)に基づきます。

### n版（0x071a）、未検証

固定したKeychron ZMKにはn版のshield（`keychron_b1_usn`）があります。キーマトリクスは別物ですが、keymap上のキー位置は同じで、HIDは「adaptive NKRO」という方式です。`config/keychron_b1_usn.keymap`はこのプロジェクトのkeymap変更をKeychronのn版keymapに適用したもので、moduleのテストもシミュレーションでadaptive NKROのHIDコード上で通しています（`usjis-adaptive` variant）。まだ行われていないのは、buildを`0x071a`の実機に書き込むことです。そのため、できる範囲での移植として扱ってください。

- 復旧経路は同じはずです（リセットスイッチ、`NRF52BOOT`ドライブ。Keychron自身のn版firmwareもnRF52840向けのUF2）が、ソースからの推定で、試していません。
- `B1_SWAP_CTRL_CAPS`はこのshieldでは無視します。n版のfirmwareにはCaps+Altの同時押しに対するマトリクスの回避策があり、Caps/Ctrlの入れ替えをLauncherで行ったときだけ有効になるためです。入れ替えはLauncherで行ってください。
- adaptive NKROでは、7つ以上のキーを押している間、JISの`\`と`_`（International1、usage `0x87`）が純正のHIDコードで落とされます。通常の入力には影響しません。
- 結果はどちらでもIssueで教えてください。どのキーが動いたか、PIDとversionが正しく表示されるか。

## このfirmwareがすること

純正firmwareと比べて、キーボードは次のように動きます。それ以外（USB、Bluetooth、2.4 GHz、Launcher、DFU、電池、LED）は固定したcommitの純正コードそのままです。

| 機能 | 操作 | 備考 |
| --- | --- | --- |
| US-JIS置換 | Fn+Tabで切替。初回起動時はオフ。設定は電源を入れ直しても残る | OS切替スイッチがWin側のときだけ効く。日本語ホストで `` ` ~ @ ^ & * ( ) _ = + [ { ] } \ \| : ' " `` が印字どおりに出る。macOSでは不要 |
| Fn層の遅れなし | — | 純正のFn+J+ZとFn+X+Lのcomboは、Fnを押している間J、Z、X、Lを最大1秒押し止めていた。これをなくした |
| 工場リセット | Fn+Shift+Escを10秒長押し | Bluetoothのbondと Launcherの設定を消す。Fn+Esc単独はEsc。実行後はUSBを抜き差しする |
| F行入れ替え | Fn+Caps Lockを3秒長押し | 純正のFn+X+Lと同じ機能 |
| 電池残量、Winロック | Fn+B、Fn+Winを3秒長押し | 変更なし |
| IMEオフ / IMEオン | スペースの左 / 右のキーをtap。押したままなら従来のmodifier（Win: Alt、Mac: Cmd） | Win: 無変換 / 変換、Mac: 英数 / かな。任意（下記） |
| CtrlとCaps Lockの入れ替え | — | 両OSモードのベース層。Fn+Caps LockのF行入れ替えはそのまま。任意（下記） |

IMEキーとCtrl/Caps入れ替えは[config/keymap-options.h](config/keymap-options.h)のコンパイル時オプションです（`B1_IME_TAP`、`B1_SWAP_CTRL_CAPS`、既定はどちらも1）。不要なら0にしてbuildし直してください。

WindowsでIMEキーを使うには、Microsoft IMEで無変換を「IME-オフ」、変換を「IME-オン」に割り当てます（設定 → 時刻と言語 → 言語と地域 → 日本語 → Microsoft IME → キーとタッチのカスタマイズ）。macOSは設定不要です。

## 書き込み

**firmwareのバイナリは公開していません。** buildしたfirmwareはKeychronのコード（一部はGPL-2.0-or-later、一部はライセンス表記なし）とNordicのbinary libraryをlinkしており、このリポジトリにはその組み合わせを再配布する権利がありません。[ライセンス参照](docs/references/licenses.ja.md)を参照してください。[Build](#build)のコマンド（Dockerだけで動きます）で各自buildし、できた`.uf2`を書き込んでください。

1. 本体裏の穴にあるリセットスイッチを押しながらUSBを接続します。`NRF52BOOT`という名前のドライブが現れます。
2. `build/firmware/keychron-b1-us-usjis.uf2`をそのドライブにコピーします。ドライブが消えてキーボードが再起動します。
3. bootloaderには触れないので、純正を含むどのfirmwareからでも同じ手順で戻せます。

Launcherに関する注意:

- このfirmware独自の動作を置いた位置（Fn+Tab、Fn+Esc、Fn+Caps Lock、スペース両隣）はLauncherでは空欄に見えます。動作はしています。Launcherでそこに何かを割り当てると、その動作は置き換わります。
- Launcherの保存済みkeymapは、firmware既定と異なる位置を起動時に上書きします。F行入れ替えは層全体を保存します。ベース層の既定が変わったfirmware（例: Ctrl/Caps入れ替え）を書き込んだあとは、Launcherで「キーマップをリセット」を一度実行してください。工場リセットでは保存済みkeymapは消えません。
- USBのdevice version（`bcdDevice`）は1.04で、Launcherは純正と同じくv1.0.4と表示します。Launcherのbuild日時は、buildしたcommitの時刻です。

## Build

必要なのはGitとDockerだけで、ほかにインストールするものはありません。toolchainは固定した`zmkfirmware/zmk-build-arm:3.2` image（Zephyr SDK 0.15.2）の中で動きます。Apple SiliconのmacOSとDocker Desktopで確認済みです（imageはlinux/amd64で、エミュレーションで動きます）。LinuxとDockerでも同じように動くはずです。WindowsはWSL2とDocker Desktopを使ってください（未確認）。

### 手順

1. Dockerをインストールして起動します（macOSとWindowsはDocker Desktop、Linuxは`docker`パッケージ）。`docker info`で確認できます。
2. このリポジトリをcloneして、そのディレクトリに入ります。

3. 必要なら`config/keymap-options.h`（IMEキー、Ctrl/Caps入れ替え）や`config/keychron_b1_us.keymap`を編集します。
4. buildします。

   ```sh
   bash scripts/build-firmware.sh
   ```

   初回はimage（約2 GB）と固定したソース（build treeを含めて約2 GB、`workspace/firmware/`へ）をダウンロードし、Zephyr patchを当ててbuildします。初回は10分以上、2回目以降は1〜2分が目安です。最後に、使われたkeymapファイルと生成物のSHA-256を含むJSONの要約が出ます。
5. firmwareは`build/firmware/keychron-b1-us-usjis.uf2`です。[書き込み](#書き込み)の手順で書き込みます。

keymapやオプションを変えたあとは`bash scripts/build-firmware.sh build`でbuildし直します。ネットワークは不要です。`bash scripts/build-firmware.sh prepare`だけを実行するとソースを取り直します。

### うまくいかないとき

- `Cannot connect to the Docker daemon`: Dockerが起動していません。
- `Repository path must not contain a comma`: cloneをカンマを含まないパスへ移してください。
- `zmk checkout is not at the pinned commit`や`Zephyr patch is not applied`: `workspace/firmware/`を削除して、引数なしでscriptをもう一度実行してください。
- 初回のダウンロード中に止まる: GitHubとDocker Hubへのネットワークアクセスが必要です。もう一度実行してください。
- build自体が失敗する: `build/firmware/build.log`にcompilerの出力があります。変更していないcheckoutのbuildは確認済みなので、失敗するならkeymapの変更が原因の可能性が高いです。

### scriptがすること

`prepare`は、`workspace/firmware/`に、このリポジトリを`config`とするwest workspaceを作り、[config/west.yml](config/west.yml)が固定するcommitのKeychron ZMKとZephyrを取得し（Zephyrのmoduleは、Zephyr自身のmanifestが固定する）、固定したZMK commitに同梱のZephyr patch（`0001-esb-nrf-fix.patch`）を、固定したZephyr commitの上にcommitとして当てます。

`build`は、`ZMK_CONFIG`にこのリポジトリの`config/`を指定して`keychron` / `keychron_b1_us`をbuildします。これにより`config/keychron_b1_us.keymap`と`config/keychron_b1_us.conf`が使われ、このリポジトリはZephyr module `zmk-feature-usjis`として検出されます。ネットワークなしで動き、checkoutが固定したcommitにないか、patchが当たっていなければ実行を拒否します。

`build/firmware/`の出力:

- `keychron-b1-us-usjis.uf2`（`.hex`、`.elf`、`.map`も）
- `zephyr.config`、`zephyr.dts`、`zephyr_modules.txt`、`build.log`
- `build-info.json`: リポジトリのcommit、すべてのwest projectのcommit、patchのhash、imageのdigest、SDK、west、CMake、compilerのversion、Pythonパッケージ、生成物のSHA-256

firmwareはZephyrのbuild versionとして`usjis-<commit>`を持ち、build日時はcommit時刻（`SOURCE_DATE_EPOCH`）になるため、同じcommitのbuildは同一のファイルになります。作業treeに未commitの変更があるbuildには`-dirty`が付きます。

## 置換の仕組み

moduleのソースはZMKの`app`ライブラリに入り、`CMakeLists.txt`が置換器のlistenerを`hid_listener.c`の直前、hold-tap、sticky key、caps word、key repeatの後ろへ移します。moduleは起動時にこの順序を検査し、違っていれば決して置換しません。置換キーを押している間は、reportのmodifierが[置換仕様](docs/design/usjis-substitution.ja.md)の競合方針に従うよう、moduleがすべてのkeyboard eventを自分で報告します。それ以外ではeventをそのまま通します。詳細は[アーキテクチャ](docs/design/usjis-architecture.ja.md)にあります。

## 検証

```sh
bash scripts/validate-repository.sh   # リポジトリ構成、固定値、リンク、ID
bash scripts/run-simulation.sh        # ネイティブシミュレーション（Docker）
```

シミュレーションは、固定した実物の`hid.c`、`hid_listener.c`、event managerを`native_posix_64`向けにbuildします。`usjis` variantはこのmoduleを結合し、置換表（C01〜C20）と仕様のSテストのHID reportをすべて検査します。他のvariantは純正のMod-Morph経路とその既知の不具合を特性記録します。[検証結果と適用範囲](docs/simulation.ja.md)を参照してください。`SIMULATION_VARIANTS=usjis bash scripts/run-simulation.sh`でmoduleのテストだけを実行できます。

## リポジトリ構成

```text
.
├── .github/        GitHub Actionsのworkflow（build、シミュレーション、検証）
├── config/         west manifest、keymap、confファイル、keymap-options.h
├── docs/           置換仕様、アーキテクチャ、シミュレーション、参照文書
├── dts/            &usjis behaviorのdevicetree binding
├── include/        moduleの公開ヘッダとdt-bindings
├── scripts/        build、検証、シミュレーションのrunner
├── src/            置換器、その置換表、モードbehavior
├── tests/          ネイティブ入力シミュレーション
├── zephyr/         Zephyr moduleのメタデータ
├── build.yaml      build対象
├── CMakeLists.txt  module buildの入口
├── Kconfig         moduleの設定（US-JISのオプション、Keychron shieldのKconfig）
├── build/          buildとシミュレーションの出力、Git管理外
└── workspace/      取得したソース、Git管理外
```

文書: [US-JIS置換仕様](docs/design/usjis-substitution.ja.md)、[US-JISアーキテクチャ](docs/design/usjis-architecture.ja.md)、[入力処理シミュレーション](docs/simulation.ja.md)、[純正のFn comboキー](docs/references/b1-special-keys.ja.md)、[ライセンスと配布方針](docs/references/licenses.ja.md)。いずれも同じ場所に日本語訳（`.ja.md`）があります。

このリポジトリはKeychron ZMKをvendorしません。[config/west.yml](config/west.yml)がcommitを固定し、buildがそれを取得します。

## 貢献

IssueとPull Requestを歓迎します。[CONTRIBUTING.ja.md](CONTRIBUTING.ja.md)を参照してください。実機からの報告は特に役立ちます。PID、接続方式（USB、Bluetooth、2.4 GHz）、ホストOSとキーボード配列、そして何が起きたか。

## Upstreamと謝辞

- Keychron ZMK: `https://github.com/Keychron/zmk.git`、branch `keychron_bpro`、固定commit `c284513085c005edf5f9a52b28c8090cc6ed01d2`。
- 置換表は、[Keyboard Quantizer](https://github.com/sekigon-gonnoc/vial-qmk)（branch `keyboard-quantizer-b`）の「JIS OSでUSキー」key overrideの、外から観察できる挙動をもとに作りました。そのコード、表、コメントは一切使っていません。

## ライセンス

このリポジトリは[MIT License](LICENSE)です。ここからbuildしたfirmwareには、別の条件のコードも含まれます。[ライセンス参照](docs/references/licenses.ja.md)を参照してください。
