# 入力処理シミュレーション

> この文書は[英語版](simulation.md)の日本語訳です。内容が食い違う場合は英語版を正本とします。

## 概要

固定したKeychron ZMKの実際の入力処理コードをPC上でビルドして実行する、Zephyr `native_posix_64`のプログラムです。キーボードは不要で、何も接続せず、何も書き込みません。

B1 Pro全体のエミュレータではありません。behaviorへ押下と解放を注入し、keycode event、event manager、`hid_listener`、HID stateを通して実行します。送信関数は各reportを記録する関数に置き換え、すべてのreportを検査します。buildする固定commitのCファイルは変更しておらず、入力経路を別の言語で再実装したものでもありません。置き換えるのは送信側（`endpoints.c`）だけです。firmwareのbuildにある他のlistener（combo、hold-tap、sticky key、caps word、key repeat、`ble.c`、Launcherの`mousekey.c`）はリンクしていません。正確な境界は[tests/simulation/README.md](../tests/simulation/README.md)にあります。

## 実行方法

必要環境はGit、Python 3、Linux containerを使えるDocker（linux/amd64。Apple Siliconではemulationで動きます）です。

```sh
bash scripts/run-simulation.sh                           # all variants
SIMULATION_VARIANTS=usjis bash scripts/run-simulation.sh # only the module's tests
```

初回は固定したソースを`workspace/simulation/`へ取得し、`tests/simulation/Dockerfile`からcontainer imageをbuildします。以降は両方を再利用しますが、準備の段階ではZephyrのbranchが動いていれば警告するため、`git ls-remote`を1回試みます（オフラインでは最大60秒待って続行します）。buildと実行のcontainerはネットワークなし、ソースをread-onlyでmountし、デバイスを渡しません。結果は`build/simulation-results/`へ出力します（`results.json`とvariantごとのlog）。

固定しているもの: Keychron ZMKは`c284513085c005edf5f9a52b28c8090cc6ed01d2`、Zephyrは`1ae0eb5ce8adafcec993e6fb8f4eeb6f818a7772`、base imageはdigest、aptパッケージは日付固定の`snapshot.debian.org`からバージョン、Pythonパッケージはバージョンとsha256です。`tests/simulation/dependencies.json`は、コンパイル、include、configure時の読込の対象となる固定ファイルすべてのsha256も持ち、このlockにないファイルがあるとrunnerは失敗します。`docker build`はbyte単位で再現しないため、image IDはマシンごとに異なります。実行結果の比較にはvariantごとの`executable_sha256`と`log_sha256`を使います。

ファイル: [テスト構成と境界](../tests/simulation/README.md)、[依存先lock](../tests/simulation/dependencies.json)、[moduleのテスト](../tests/simulation/src/usjis_main.c)、[Mod-Morphのcharacterization](../tests/simulation/src/main.c)とその[binding](../tests/simulation/app.overlay)。

## variant

| variant | buildする内容 | 目的 |
| --- | --- | --- |
| `usjis` | このmoduleと`keychron_b1_us`（PID `0x0711`）のHKRO `hid.c` | 置換器を検査する。[置換仕様](design/usjis-substitution.ja.md)のC01〜C20とSテスト（S09を除く。S10は起動時の既定値だけ）、加えて共有する出力usage、繰り返しの押下、逆順の解放、層mask |
| `usjis-adaptive` | 同じものを`keychron_b1_usn`（PID `0x071a`）のadaptive NKRO `hid.c`で | n版のHIDコードで同じ検査を行う |
| `hkro`、`adaptive` | 置換表を標準のMod-Morph bindingとして記述したもの。moduleなし | 標準のMod-Morph方式をcharacterizationし、その不具合（後述）を再現する |
| 負例5種 | 意図的に誤らせたMod-Morphの表 | 検査が誤ったusageや誤ったShiftを検出することを示す |

## 結果

現在のツリーでの全variantの結果です（`results.json`のstatusは`CHARACTERIZATION_PASS_NOT_FIRMWARE_ACCEPTANCE`で、シミュレーションの合格が実機での受け入れではないことを示します）。

| variant | 結果 | 表の系列 | シナリオ | assertion | 検査したreport |
| --- | --- | --- | --- | --- | --- |
| `usjis` | `USJIS_TESTS_PASS` | 90 | 120 | 4923 | 588 |
| `usjis-adaptive` | `USJIS_TESTS_PASS` | 90 | 120 | 4923 | 588 |
| `hkro` | characterization合格、既知の不具合7件を再現 | 90 | — | 3796 | 447 |
| `adaptive` | characterization合格、既知の不具合8件を再現 | 90 | — | 5666 | 463 |
| 負例（5） | それぞれ意図した検査で失敗（exit code 1） | — | — | — | — |

90の表の系列は、Shiftありの14行 × 左Shift・右Shift・両方のShift × 2つの解放順と、Shiftなしの6行です。moduleのvariantは、すべてのreportを`{modifiers, [usages]}`として順に検査し、各シナリオの後の状態も検査します。characterizationのvariantでは、同じ90系列が途中の状態に標準方式の不具合を示します。物理Shiftを先に離すと33系列で付加Shiftが失われ、キーを先に離すと42系列でShiftが離されたと報告されます。

characterizationのvariantの`SIMULATION_PASS`は、標準方式で想定した結果と既知の不具合を観測したという意味で、それらの不具合を修正したことを意味しません。仕様を満たす必要があるのはmoduleのvariantです。

## キーコード混同の検査

C08（`_`）、C15（`\`）、C16（vertical bar）は、それぞれ異なるusageとmodifierの組を出す必要があります。characterizationのvariantはこの3つのreportを検査し、C16をC08と同じ`0x87`にした負例は失敗しなければなりません。

| 項目 | 入力 | 出力 | 観測したreport（hex） |
| --- | --- | --- | --- |
| C08 | Shift + US MINUS | Shift + International1 (`0x87`) | `01 02 00 87 00 00 00 00 00` |
| C15 | US BACKSLASH | International1 (`0x87`) | `01 00 00 87 00 00 00 00 00` |
| C16 | Shift + US BACKSLASH | Shift + International3 (`0x89`) | `01 02 00 89 00 00 00 00 00` |

値は`hkro` variantのものです。先頭のbyteはreport IDで、その後にmodifier（`02` = Left Shift）、reserved byte、6個のusage slotが続きます。これらはendpoint API時点のstateで、採取したUSBフレームではありません。

5種の負例は、それぞれ意図した検査でexit code 1で失敗します。C16を`0x87`にしたもの（HKROとadaptive）、C01の付加Shiftを外したもの、Mod-Morphの発火条件を左Shiftだけにしたもの（漏れた左Shiftと正しい付加Shiftは同じに見えるため、右Shiftの条件でだけ検出されます）、C02のShift maskを外したものです。ビルド失敗や無関係のクラッシュは負例の成功として扱いません。

usageの区別は[Microsoftのキーボード入力の資料](https://learn.microsoft.com/ja-jp/windows/win32/inputdev/about-keyboard-input)と[kbd projectの日本語キーに関する資料](https://kbd-project.org/docs/scancodes/scancodes-8.html)に従います。シミュレーションにはWindowsのkeyboard layoutやIMEは含まれず、文字そのものはWindowsホストで確認します。

## Mod-Morphのkeymapでは足りない理由

moduleを使わずに置換表をkeymapのMod-Morph bindingとして書くと、最初の文字は正しく出ますが、次のように壊れます。最後の1件以外は本家ZMKのMod-MorphとHIDのコードに由来するため、公式ZMKへ移っても解消しません。moduleは独自の押下記録を持ち、置換キーを押している間はすべてのeventのreportを自分で送るため（[US-JISアーキテクチャ](design/usjis-architecture.ja.md)）、1、2、4、5、6番目を回避します。3番目は設計上残ります。キーの置換は押下時に決まるためです（仕様第6節）。7番目は該当せず（moduleはMod-Morphを使いません）、8番目はmoduleにも当てはまります。

| 不具合（logの`KNOWN_DEFECT`） | 由来 | 操作と結果 |
| --- | --- | --- |
| implicit-shift-lost-on-physical-shift-release | ZMKのHID（implicit modifierを押下時に代入し、まとめて消去） | Shift + MINUSを置換した後、物理Shiftを先に離すと、`0x87`の保持中に付加Shiftが消える。`_`が`\`に変わる |
| physical-shift-reported-released-on-morph-release | ZMKのMod-Morph（maskの解除は解放report後） | MINUSを先に離すと、物理Shiftを押したままなのにmodifier `00`が送られる。次のキーでShiftが再び現れる |
| physical-shift-after-key-applies-to-unsubstituted-usage | ZMKのMod-Morph（押下時にbindingを選択） | US `2`の後に押したShiftが非置換のusageに掛かる。JISホストでは`@`ではなく`"`になる |
| implicit-shift-overwritten-by-unrelated-key | ZMKのHID | EQUAL、次にShift + MINUSを保持してAを押すと、MINUSのusageは残るが、Aを離した後にShiftが戻らない |
| mask-cleared-while-other-morph-held | ZMKのMod-Morph（共有の単一mask） | Shift + 2とShift + 6を重ねて片方を離すとmaskが解除され、もう一方にShiftが掛かる |
| shared-output-released-while-equal-held | ZMKのHID listener（同じusageのpre-release） | MINUSとEQUALは出力usageを共有し、一方を離すと、他方を保持中でもそのusageが解放される |
| same-mod-morph-instance-second-press-rejected | ZMKのMod-Morph（設計上の`-ENOTSUP`） | 同じMod-Morph instanceを2つ目のpositionから押すと拒否される |
| international1-dropped-after-six-keys | Keychronのadaptive NKRO（`NKRO_MAX_USAGE`） | A〜Fを保持していると、International1（`0x87`）がHID stateに追加されず、errorも報告されない。これはmoduleにも当てはまる（[README](../README.ja.md#n版0x071a未検証)） |

adaptiveのvariantでは、report数は`zmk_endpoints_send_report`の呼出し回数です。実際のn版firmwareが送る数はこれより少なくなります。`endpoints.c`は、implicitやmaskedのmodifierだけが変わったreportを送らないためです。1番目の不具合では、実機の症状は別の文字ではなく、Shiftの解放の遅延になります。

## 範囲

対象外: key matrixとkscan、keymap全体、combo、hold-tap、Launcher、radio、flashとsettingsの保存、DFUとbootloader、Windowsによるreportの解釈。
