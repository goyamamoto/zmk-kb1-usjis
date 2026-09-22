# US-JIS 置換仕様

> この文書は[英語版](usjis-substitution.md)の日本語訳です。内容が食い違う場合は英語版を正本とします。

## 1. 目的

Keychron B1 ProのUS物理配列から発生するkeyboard usageを、日本語キーボード配列に固定されたホストがUSキーキャップの印字どおりに解釈する入力へ置換します。

この文書は「何を置換するか」を定めます。moduleでの実現方法は[US-JISアーキテクチャ](usjis-architecture.ja.md)を参照してください。置換表は、Keyboard Quantizer（[sekigon-gonnoc/vial-qmk](https://github.com/sekigon-gonnoc/vial-qmk)、branch `keyboard-quantizer-b`）のUSキーをJIS OSで使うためのkey overrideについて、外部から観測できる動作から導きました。そのコード、表、コメントは一切使用していません。

## 2. 前提と用語

- **置換（US-JIS置換）**: 入力chordが第4節のC01〜C20に一致したとき、JISホストへ報告するusageとShiftを置き換えること。JISの変換キー（`INT4`）・無変換キー（`INT5`）や、IMEのかな漢字変換とは別のものである。
- **置換したキー**: 押した時点の入力を置換したキー。短く「置換キー」とも書く。同じ物理キーでも、押した時点のShiftやmodeによって置換するかどうかが変わる（例えば`2`はShiftなしなら非置換、Shift付きならC02として置換）。modifierキーは置換しない。
- **入力chord**: US物理配列のkey usageと、その押下時に有効なmodifierの組。Shiftは、物理Shift（explicit modifier）と、そのキーのbindingが付けたShift（`&kp LS(x)`等のimplicit modifier）のどちらでもよい。他のキーのbindingが付けたShiftは含めない。一致の判定にはShiftだけを使い、Ctrl、Alt、GUIは判定に使わず出力に保持する（ショートカットの扱いは第11節）。例えば`&kp PLUS`（`LS(EQUAL)`）はC10に一致する。Mod-Morph等の基点のbehaviorがmaskしたShiftは考慮しない（下記の制限を参照）。
- **置換対象キー**: C01〜C20の入力chordに現れるキー（`` ` ``、`2`、`6`〜`0`、`-`、`=`、`[`、`]`、`\`、`;`、`'`）。押した時点の条件によって置換したキーにも非置換にもなる。
- **出力chord**: JISホストへ報告するkey usageとmodifierの組。
- **判定に使うShift**: 左Shiftまたは右Shift。文字判定では両者を同じShift条件として扱う。
- **JIS記号キー**: JIS配列上の`^`、`@`、`[`、`]`、`;`、`:`、backslash/underscore等の位置。実装時はHID Usage Tablesと対象ホストでusage IDを確定する。
- **identity**: 入力eventをusage、modifier、押下・解放とも変更しないこと。reportが基点と同一になる。
- **非置換**: usageを置換しないこと。置換したキーが押下中でなければidentityと同じ。押下中は、reportのmodifierが第6節の競合方針に従う。
- **押下記録（active entry）**: keyboard pageの非modifierキーの押下ごとに、置換結果または非置換を解放まで保持する記録（第6節、第7節）。置換したキーの記録を置換entry、置換しないキーの記録を非置換entryと呼ぶ。
- **実効状態**: 現在適用されているmode。pending中の要求状態（第7節）とは区別する。

### 実装の制限

`src/usjis.c`の置換器は、この仕様を次の制限つきで実装しています。

- 置換は`CONFIG_ZMK_USJIS_LAYER_MASK`の層が有効な間だけ行います（既定は層2。B1 ProのOS切替スイッチのWin側が`&mo 2`で保持する層）。Mac側ではモードが`ENABLED`でもすべての入力がidentityです。
- 押下記録は入力usageで識別します。同じusageを2つの位置に割り当てることは対象外で、押したままのusageをもう一度押すとその記録を置き換えます（S09は対象外）。
- 押したままのMod-Morphキーの基点のmaskは取得しません。置換対象キーのMod-Morphは対象外です。Fn層のEscのMod-Morphは置換と関わりません。
- 置換キーを押している間、consumerとsystem usageのimplicit modifier（例: `&kp LS(C_VOL_UP)`）はkeyboard reportへ反映しません。
- endpoint切替によるclearは、`zmk_endpoint_changed`を受けたときに、押下記録の出力usageがまだ押されているかで検出します。

対象ホストは、日本語106/109キーボード配列のWindowsです。macOSは置換を必要とせず、OS切替スイッチのMac側ではすべての入力がidentityです。

## 3. 置換モード

状態は次の2値です。

| 状態 | 動作 |
| --- | --- |
| `DISABLED` | 全eventをidentityとして扱う |
| `ENABLED` | keyboard usageのうち置換表に一致する入力だけを置換する。それ以外は非置換 |

操作は`ON`、`OFF`、`TOGGLE`の3種を提供します。要求後の状態が第7節の基準（pendingがあればpendingの要求状態、なければ実効状態）と同じになる`ON`または`OFF`は、成功するno-opです。pendingの要求も実効状態も変えず、保存もしません。したがって、`ENABLED`で`OFF`がpending中の`ON`はno-opではなく、pendingの要求を置き換え、状態は`ENABLED`のまま変わりません（第7節の手順4）。初回起動、設定欠損、設定version不一致、値不正時の既定値は`DISABLED`とします。

## 4. 正準置換表

以下の出力chordはJISホスト上で目的文字を作る論理表現です。出力にShiftが不要な行では入力Shiftを抑制し、必要な行では左右どちらの入力Shiftでも出力側にShiftを1つ付加します。**付加する出力Shiftは常にLeft Shift（modifier bit `0x02`）とし、入力が右Shiftでも右Shiftを出力しません。** これは実装非依存の仕様であり、文字の生成に左右の区別は不要で、シミュレーションと置換器が同じ期待値を共有するために固定します。

| ID | 入力chord | 目的文字 | JISホスト向け出力chord |
| --- | --- | --- | --- |
| C01 | `Shift + GRAVE` | `~` | `Shift + JIS_CARET` |
| C02 | `Shift + 2` | `@` | `JIS_AT` |
| C03 | `Shift + 6` | `^` | `JIS_CARET` |
| C04 | `Shift + 7` | `&` | `Shift + 6` |
| C05 | `Shift + 8` | `*` | `Shift + JIS_COLON` |
| C06 | `Shift + 9` | `(` | `Shift + 8` |
| C07 | `Shift + 0` | `)` | `Shift + 9` |
| C08 | `Shift + MINUS` | `_` | `Shift + JIS_RO` |
| C09 | `EQUAL` | `=` | `Shift + MINUS` |
| C10 | `Shift + EQUAL` | `+` | `Shift + SEMICOLON` |
| C11 | `LEFT_BRACKET` | `[` | `JIS_LEFT_BRACKET` |
| C12 | `Shift + LEFT_BRACKET` | `{` | `Shift + JIS_LEFT_BRACKET` |
| C13 | `RIGHT_BRACKET` | `]` | `JIS_RIGHT_BRACKET` |
| C14 | `Shift + RIGHT_BRACKET` | `}` | `Shift + JIS_RIGHT_BRACKET` |
| C15 | `BACKSLASH` | `\` | `JIS_RO` |
| C16 | `Shift + BACKSLASH` | vertical bar (`U+007C`) | `Shift + JIS_YEN` |
| C17 | `Shift + SEMICOLON` | `:` | `JIS_COLON` |
| C18 | `QUOTE` | `'` | `Shift + 7` |
| C19 | `Shift + QUOTE` | `"` | `Shift + 2` |
| C20 | `GRAVE` | `` ` `` | `Shift + JIS_AT` |

`JIS_RO`はKeyboard page `0x07`のInternational1（usage `0x87`）、`JIS_YEN`はInternational3（usage `0x89`）です。C08、C15、C16が別のusageまたはmodifierの組を出すことは[シミュレーション](../simulation.ja.md)で検査しています。C15は`U+005C`を期待します。フォントによっては円記号の字形で表示されるため、画面の見た目ではなく文字コードと用途（パス、正規表現など）で判定します。Windowsでの文字コードと字形は、実機で個別には確認していません。

シミュレーションで使用するその他のJIS位置は次のとおりです。ZMKのUS向けキー名がJISホストでも同じ文字を表すという意味ではありません。

| JIS位置 | Keyboard usage | ZMK側の識別子 |
| --- | --- | --- |
| `JIS_CARET` | `0x2E` | `EQUAL` |
| `JIS_AT` | `0x2F` | `LEFT_BRACKET` |
| `JIS_LEFT_BRACKET` | `0x30` | `RIGHT_BRACKET` |
| `JIS_RIGHT_BRACKET` | `0x32` | `NON_US_HASH` |
| `JIS_COLON` | `0x34` | `SINGLE_QUOTE` |
| `JIS_RO` | `0x87` | `INTERNATIONAL_1` |
| `JIS_YEN` | `0x89` | `INTERNATIONAL_3` |

resolver（`src/usjis_resolver.c`）とシミュレーションがこれらの値を使います。

## 5. 置換しない入力

`ENABLED`でも、次は置換しません（非置換）。

- 英字、数字のShiftなし入力。
- C01からC20の条件に一致しない記号入力。
- Ctrl、Alt、GUI単独および通常のmodifier event。
- consumer usageとsystem control usage。
- Bluetooth、2.4 GHz、電源、DFU、Launcher用の内部behavior。
- US-JIS モードの操作そのもの。

Ctrl、Alt、GUIは一致の判定に使わず出力に保持するため、これらと置換対象キーのchordはShiftだけで照合します（第2節）。例えば`Ctrl + =`は`Ctrl + Shift + MINUS`になります。Altを含むgrave accentについては第8節を参照してください。

これらの入力はusageを変更しません。置換したキーを押している間に重ねて押した場合、reportのmodifierは第6節の競合方針に従います。置換したキーが押下中でなければidentityです。

## 6. 押下と解放の規則

1. 置換判定は物理キーの押下時に1回だけ行う。
2. 判定結果には出力usage、追加modifier、抑制modifierを含める。置換した場合、抑制modifierには入力の左右Shiftを常に含め、Shiftが必要な行は追加modifierでLeft Shiftを付け直す（第4節）。bindingが付けたShiftも出力には残さない。Ctrl、Alt、GUIは保持する。
3. 判定結果を押下記録として保存する。
4. 同じ物理入力の解放時は現在のmodeやShiftを再評価しない。
5. 押下記録に保存した出力usageを解放する。modifierは競合規則で決める。
6. 解放後に押下記録を削除する。

これにより、解放時のShiftの状態は、解放するusageに影響しません。`RShift down -> 2 down -> RShift up -> 2 up`では、`2`のkey upで押下時に決めた`0x2F`（C02）を解放します（S03）。`2 down -> RShift down -> 2 up -> RShift up`では`0x1F`（非置換の`2`）を解放します。別usageの解放や押しっぱなしは発生しません。

置換した入力の文字は押下時に確定し、押下中に物理Shiftを押しても離しても変わりません（repeatも同じ文字）。置換しない入力は基点と同じで、押下後に押したShiftがそのままreportに効きます。そのため`2`を押してからShiftを押すと、reportは`Shift + 2`になり、JISホストでrepeatが続けば`@`ではなく`"`になります。modifierの押下でrepeatが止まるか続くかはホストに依存します。これは既知の制約です（[シミュレーション](../simulation.ja.md)の`physical-shift-after-key-applies-to-unsubstituted-usage`）。

### 長押し

置換後usageは物理キーを押している間、HID report上で押下状態を維持します。firmwareが文字列やtap eventを連打してrepeatを生成してはいけません。repeatの開始時間と間隔はホスト設定に委ねます。

### 重複と同時押し

- 異なる置換対象キーを2個まで重ねた押下・逆順解放で、usageまたはmodifierを残留させない。
- 同じusageを複数の物理位置へ割り当てることは対象外とする（第2節）。usageごとの数では、どのキーが離されたかを判定できない。S09は、入力元を識別する実装に求める結果を示す。
- HID modifierはreport全体へ作用するため、同時に保持したキーの間でmodifierの要求が競合する。競合は次の方針で解決し、どの場合もstuck keyとstuck modifierを許さない。

### modifierの競合方針

Shiftを抑制する置換キー（C02等）と付加する置換キー（C09等）、または置換キーと置換しないキーを重ねて押すと、全キーの要求を1つのreportでは同時に表現できません。keyboard pageの非modifierキーは、modeや置換の有無にかかわらず常に押下記録に残します（第7節のpending判定にも使う）。`ENABLED`中、置換したキーが1個以上押下中の間は、その記録を使って次の方針で解決します。以下、各項を競合規則1〜7と呼びます。

1. 押下の順序はkeycode eventの順とする（hold-tap等では物理的な押下順と異なり得る）。
2. 置換しないキーの要求は、基点がそのkey downで報告するmodifierのうちkeyに付随する分とする。つまりbindingが付けたmodifier（`&kp LS(x)`等のimplicit modifier）と、Mod-Morph等の基点のbehaviorが抑制していたmodifierである（実装は基点のmaskを取得しない。第2節）。置換器は新たに追加も抑制もしない。
3. reportのmodifierは、物理modifierと、押下中のキーのうち**最後に押されたキー**の要求から決める。
4. 新しいキーのkey downは、そのキーの要求を反映したmodifierと同じreportで報告する。出力usageが既に押下中なら、先にそのusageの解放reportを、それまでのmodifierのまま送ってから押下する（基点のpre-releaseと同じ）。解放時は、押下中の他のキーがその出力usageを報告していない場合に限り、出力usageを解放する。
5. 最後に押されたキーを解放したら、残りのキーのうち最後に押されたキーの要求へmodifierを戻し、key upと同じ1件のreportで報告する。押下中のキーがなければ物理modifierだけに戻す。modifierの変更を次のkey eventまで遅らせない。戻すのはShiftの要求だけとし、Ctrl、Alt、GUIの要求は一度外れたら、そのキーを離すまで戻さない（単独押下と見なされるのを避けるため）。
6. 物理modifierが変わったときも同じ規則で計算し直す。置換で付加したShiftを物理Shiftとして扱わない。競合規則5で外れたCtrl、Alt、GUIの要求は、この再計算でも戻さない。物理modifierキーのeventでは、基点と同じく、結果が変わらなくてもkeyboard reportを1件送る。consumer/system usageのeventでは、keyboard reportのmodifierが変わらなければkeyboard reportを送らない（基点と同じ）。
7. 置換したキーが全て離れたら、競合規則5より優先して、modifierを基点の処理に戻す（基点は解放時にimplicit modifierを消去する。押下中の基点behaviorのmaskは戻さない）。このときもkey upと戻したmodifierを1件のreportで送る。例えば`&kp LS(A)`を保持したまま置換キーを押して離すと、`A`のShiftは戻らない。置換したキーより後に押した`&kp LS(A)`でも同じで、C09 → `LS(A)` → C09の解放では、最後に押した`A`のShiftも外れる。これは基点の`hid_listener`と同じ制約である。置換したキーが押下中でない間の重ね押しは、基点と同じ結果になる。

根拠は次のとおりです。文字はkey downの時点のmodifierで決まります。ホストのkey repeatは、通常は最後に押したキーにだけ行われます（ここでは実測していない）。最後に押したキーを優先すれば、各キーの最初の文字と、repeatするキーの文字を正しくできます。競合規則5と競合規則7により、`&uc LG(E)`等の保持中に他のキーをtapしても、GUI等の再押下は起きません。

代替案として、最初に押した置換キーを優先する案と、付加・抑制をmodifier bitごとに合算する案を検討しました。前者は後から押したキーの最初の文字を誤り、後者は付加と抑制が同時に要求されたときにどちらの文字も保証できないため採用しません。

`DISABLED`中はこの方針を適用せず、基点のmodifier処理のまま（identity）とします。mode切替は非modifierキーが全て離れるまで保留するため（第7節）、押下中のキーに両方の処理が混ざることはありません。

この方針での期待結果は次のとおりです。

| 操作 | 期待する文字 | 途中のmodifier | 試験 |
| --- | --- | --- | --- |
| `Shift`を押したまま`2`（C02）を保持し、`A`を押す | `@`の後に`A` | `A`の押下中は抑制を外す。`A`を離すと抑制へ戻る | S11 |
| `EQUAL`（C09）を保持し、`A`を押して離す | `=`の後に`a` | `A`の押下中は付加Shiftを外す。`A`を離すと付加Shiftへ戻る | S12 |
| `EQUAL`（C09）を保持したまま、`Shift`を押して`2`（C02）を押す | `=`の後に`@` | 後から押したC02の抑制を優先する。C02を離すと付加Shiftへ戻る | S13 |
| `Shift + MINUS`（C08）を保持し、先に`Shift`を離す | `_`のまま | 付加Shiftを維持する。repeatも`_` | S14 |

先に押したキーは、後のキーを押している間、reportのmodifierが自分の要求と異なります。ホストがそのキーのrepeatを再開する場合や、key upの時点のmodifierを使うアプリケーションでは、結果が異なり得ます。これは既知の制約です。

adaptive NKRO構成（`keychron_b1_usn`、PID `0x071a`）の基点は、implicitまたはmaskedのmodifierだけが変わるreportをUSBとBluetoothへ送信しません（explicit modifierの変化は送信する）。この構成では、key upを伴うreportは送信されますが、clear直後の再計算のようにmodifierだけが変わるreportは送信されません。`src/usjis.c`の置換器には影響しません。置換器が送るreportはすべてusageの押下か解放、またはexplicit modifierのeventを伴い、それらが基点の送信フラグ（`KB_RPT`）を立てるためです。この構成で落ちうるのはclear直後の再計算のreportだけで、modifierしか変わらないときは何も送られず、次のキーeventが正しいmodifierを運びます。`usjis-adaptive`のシミュレーションvariantがこの構成でSテストを実行します（[US-JISアーキテクチャ](usjis-architecture.ja.md)第10節）。

## 7. モード切替中の規則

keyboard pageの非modifierキーが1個以上押下中なら、置換したかどうかにかかわらず、mode変更は即時適用せずpendingにします。`DISABLED`中に押したキーと`ENABLED`中に押したキーを混在させないためです。modifierキー（Shift、Ctrl、Alt、GUI）とconsumer/system usageは、どちらのmodeでもusageを置換しないため、押下中でもpendingの条件に含めません。

1. 要求後の状態を計算する。基準はpendingがあればpendingの要求状態、なければ実効状態とする。`ON`は`ENABLED`、`OFF`は`DISABLED`、`TOGGLE`は基準の反転とする。
2. 非modifierキーが押下中なら、要求状態をpendingへ保存する。押下中でなければ直ちに適用する。
3. pending中に別のmode操作が来たら、手順1のとおりpendingの要求状態を基準に計算し直して置き換える。したがって`TOGGLE`の連打は実効状態ではなく最新の要求を反転する。`TOGGLE`を2回行うと互いに打ち消す。それ以前にpendingの要求があれば、要求はその要求に戻る。なければ要求は実効状態と同じになり、手順4により何も変わらない（S16）。
4. 最後の非modifierキーを解放した直後にpending状態を適用する。適用する状態が実効状態と同じなら何もしない（保存もしない）。
5. 実際に適用した状態だけを永続化する。
6. 各キーの解放は、そのキーの押下時に記録した結果（置換または非置換）に従う。解放時のmodeで判定し直さない。

mode keyの押下と解放はホストへ送信しません。

### endpoint切替と切断

固定基点でHID reportがclearされるのは、現在のtransportが`NONE`以外で、別のtransportへ切り替わるときだけです（`endpoints.c`の`update_current_endpoint`）。このときreportのusageとmodifierを0にし、切替前のendpointへ空のreportを送ります。explicit、implicit、maskedのmodifierの内部状態は残ります。`NONE`への切替、`NONE`からの切替、Bluetooth profileの切替ではclearしません。`OUT_BLE`と`OUT_24G`の操作にはrebootする経路があります。スライドスイッチをBluetoothまたは2.4 GHzからケーブルの位置へ動かしたとき、USBが給電されていてケーブルの位置と判定されれば（`get_mode_status()==0x03`）USBへ切り替わってclearが起き、給電されていなければ`NONE`になりclearは起きません（`behavior_outputs.c`）。USBへの切替ではホストでUSBの再列挙が起きます。切替前のendpointへの空reportは、Bluetoothや2.4 GHzの切断要求の後に送られるため、届くとは限りません。

押下中だった物理キーは、clearの後も解放eventを出します。このため押下記録は次のように扱います。

- clearが起きたら、その時点の押下記録を全てclear済みとする。clear済みの記録はpendingの判定には数えるが、modifierの計算（最後に押されたキーの選定）と、競合規則7の「置換したキーが押下中」には数えない（第6節）。
- clearの直後に、置換器が設定したimplicit modifierを0に、maskを押下中の基点behaviorの値に戻し、modifierを計算し直す。
- 置換したキーのclear済み記録の解放では、記録を削除し、出力usageの解放reportを新しいendpointへ送らない。modifierは計算し直し、変わった場合は報告する。
- 置換しないキーの記録の解放は、clear済みでない置換したキーが押下中でなければ、clear済みでも基点へそのまま渡す（`DISABLED`中は常に基点の処理）。基点は解放時にimplicit modifierを消去するため、渡さないとmodifierが残る。clear済みでない置換したキーが押下中なら、置換器が解放を自分で処理し、第6節で決まるmodifierを保つ（[US-JISアーキテクチャ](usjis-architecture.ja.md)第7節）。
- clear後に押したキーは通常どおり記録し、置換する。
- clearを伴わない切替では、押下記録とHID stateをそのまま扱う（基点と同じ）。
- rebootでは押下記録とpendingを失う。pendingは保存しないため、起動後は保存済みのmode（第9節）になる。
- 置換器は、`zmk_endpoint_changed`を受けたときに、押下記録の出力usageがまだ押されているかでclearを検出する（[US-JISアーキテクチャ](usjis-architecture.ja.md)第8節）。eventだけでは足りない。clearを伴わない`NONE`からの切替でも発行され、clearを誤って仮定すると置換した出力usageが押されたまま残るためである。

## 8. Grave accentとOS mode

Windows日本語配列での仕様はC01とC20です。次は実機で確認していません。

- `Alt + GRAVE`が全角/半角切替として必要か。
- KeychronのWindows/macOS modeがusageまたはmodifierを事前に書き換えるか。
- macOS日本語入力でC01/C20が同じ出力chordを要求するか。
- 左Alt/左GUI swap後にどの論理modifierを判定すべきか。

grave accentにOS別の特例はありません。

## 9. 設定仕様

modeは、Zephyr Settingsのkey `usjis/mode`に2バイトのrecordとして保存します。

| field | 型 | 値 |
| --- | --- | --- |
| `version` | `uint8_t` | 1 |
| `enabled` | `uint8_t` | 0（`DISABLED`）または1（`ENABLED`） |

- 設定の読込前は、modeは`DISABLED`である。
- recordの欠損、不正長、不正値、未知versionでは`DISABLED`になり、その理由をlogへ残す。
- 保存するのは実際に適用したmode変更だけで、変更の2秒後（`CONFIG_ZMK_USJIS_SETTINGS_SAVE_DELAY_MS`）に保存する。このため操作を繰り返してもflashへの書込みは1回になる。pendingの要求は保存しない。
- 書込みの失敗はlogへ残す。このとき、次に保存が成功するまで実効状態と保存済みのmodeが異なる。
- キーボードのfactory reset（Fn+Shift+Escを10秒長押し）はこのrecordを消さない。

## 10. 自動テスト仕様

### 表駆動テスト

C01からC20の各行について次を検証します。

- Shiftを含む行は、左Shiftと右Shiftのどちらでも一致する。Shiftを含まない行は、Shiftなしで一致する。
- 押下結果が正しいusage、追加modifier、抑制modifierを持つ。
- 解放が押下時の結果を使用する。
- 表にないShift条件は非置換になる。

### 状態遷移テスト

Shiftを使うSは、付加するLeft Shift（`0x02`）と区別できるよう、物理Shiftに右Shift（`0x20`）を使います。reportは`{modifier, [usage]}`で表します。

| ID | 操作列 | 必須結果 |
| --- | --- | --- |
| S01 | mode無効でC01〜C20の入力chordを押下・解放 | 全eventがidentity |
| S02 | mode有効で`RShift down -> C02 down -> C02 up -> RShift up` | `{20,[]}`、`{00,[2F]}`、`{20,[]}`、`{00,[]}`の4件。C02のkey upのreportで右Shiftを戻す |
| S03 | mode有効で`RShift down -> C02 down -> RShift up -> C02 up` | `{20,[]}`、`{00,[2F]}`、`{00,[2F]}`、`{00,[]}`の4件。右Shiftを先に離しても、C02のkey upで`0x2F`を解放する |
| S04 | mode有効で`C09 down -> mode OFF -> C09 up` | OFFはpending、C09を正しく解放後に適用 |
| S05 | mode有効で`RShift down -> C02 down -> C08 down -> C08 up -> C02 up -> RShift up` | modifierは`20`、`00`、`02`、`00`、`20`、`00`の順（6件）。`0x2F`と`0x87`を1回ずつ解放し、残留なし |
| S06 | mode有効で置換したキーを長押し | key downを保持し、tap列を生成しない |
| S07 | mode有効と無効のそれぞれで、consumer/system usageを押下・解放 | eventとreportが不変 |
| S08 | mode有効と無効のそれぞれで、mode keyを押下・解放 | ホストreportにmode keyが現れない |
| S09 | mode有効で、`2`を割り当てた2つのキーA、Bについて`A down -> RShift down -> B down -> A up -> B up -> RShift up`と、Bを先に離す`A down -> RShift down -> B down -> B up -> A up -> RShift up` | Aは非置換の`2`（`0x1F`）、BはC02（`JIS_AT`、`0x2F`）。どちらの順でも、A upでは`0x1F`だけ、B upでは`0x2F`だけを解放する。最後に残留なし |
| S10 | 設定欠損・破損・未知versionで起動 | `DISABLED` |
| S11 | mode有効で`RShift down -> C02 down -> A down -> A up -> C02 up -> RShift up` | `@`の後に`A`。modifierは`20`、`00`、`20`（Aのkey down）、`00`（Aのkey up）、`20`、`00`の順。残留なし |
| S12 | mode有効で`C09 down -> A down -> A up -> C09 up` | `=`の後に`a`。modifierは`02`、`00`（Aのkey down）、`02`（Aのkey up）、`00`の順。残留なし |
| S13 | mode有効で`C09 down -> RShift down -> C02 down -> C02 up -> RShift up -> C09 up` | `=`の後に`@`。modifierは`02`、`02`、`00`（C02のkey down）、`02`（C02のkey up）、`02`、`00`の順。残留なし |
| S14 | mode有効で`RShift down -> C08 down -> RShift up -> C08 up` | `{20,[]}`、`{02,[87]}`、`{02,[87]}`、`{00,[]}`の4件。右Shiftの入力でも出力は左Shiftで、C08の保持中は`Shift + JIS_RO`を維持する |
| S15 | mode無効で`A down -> mode ON -> RShift down -> 2 down -> 2 up -> A up -> 2 down -> 2 up -> RShift up` | 1回目の`Shift + 2`は置換しない（`DISABLED`のためidentity）。`A`の解放でONを適用し、2回目は`@`（C02）になる |
| S16 | mode有効で`C09 down -> TOGGLE -> TOGGLE -> C09 up` | 解放後も`ENABLED`（自動テストは内部APIで、実機は解放後にC09をtapして`=`が出ることで確認）。状態が変わらないため設定を書き込まない |
| S17 | mode有効で無線（Bluetoothまたは2.4 GHz）に接続中、USBを接続し、`C09 down -> スライドスイッチを無線の位置からケーブルの位置へ -> RShift down -> A down -> A up -> RShift up -> C09 up` | 切替前の接続先へは、C09の押下report（`Shift + 0x2D`）の後、基点と同じく空reportが1件送られる（到達は保証しない）。切替後のreportにC09の出力と付加Shiftが現れず、`RShift + A`は`{20,[04]}`（大文字の`A`）になる。clearの検知（第7節）が必要である。USBの再列挙が終わってから操作する。解放後に押下記録が残らない |
| S18 | mode有効で`&kp LS(A) down -> C09 down -> C09 up -> A up` | C09のkey upのreportはShiftなしで、以降は基点と同じ状態になる。`A`のShiftは戻らない（競合規則7） |
| S19 | mode有効で`A down -> C09 down -> A up -> consumerキー down -> consumerキー up -> C09 up` | C09のkey downのreportから、C09のkey upの直前のreportまで、どのkeyboard reportでもC09の付加Shiftが消えない |
| S20 | mode有効で`C09 down -> &kp LC(X) down -> A down -> A up -> Alt down -> Alt up -> X up -> C09 up` | `A`のkey up以降、`Alt`の押下・解放の後も、`LC(X)`のCtrlは戻らない（競合規則5、競合規則6）。残留なし |

### テストの実行場所

[シミュレーション](../simulation.ja.md)の`usjis`と`usjis-adaptive`のvariantは、このmoduleを固定基点の`hid.c`、`hid_listener.c`、event managerとリンクし、すべてのreportを検査します。

| 試験 | シミュレーション |
| --- | --- |
| C01〜C20 | 全行。Shiftを含む行は左Shift、右Shift、両方のShiftで、解放順は両方 |
| S01〜S08、S11〜S16、S18〜S20 | すべて |
| S09（同じusageを2回） | 実行しない。同じusageを2か所へ割り当てることは対象外（第2節） |
| S10（起動時の設定） | 設定がない状態での起動時の既定値だけ（シミュレーションには設定のbackendがない） |
| S17（endpointのclear） | テストプログラムが`endpoints.c`と同じ方法でclearを再現する（固定された`endpoints.c`はbuildしない） |

実機で途中のreportを判定するには、ホスト側でUSBをキャプチャします（WindowsではUSBPcap等）。2.4 GHzではdongleのUSB reportを採取します。

## 11. 制限と未確認事項

| 項目 | 状態 |
| --- | --- |
| 同じusageの複数位置への割当 | 対象外。押したままのusageをもう一度押すと、その記録を置き換える（S09） |
| 置換対象キーのMod-Morph、sticky key、macro | 押したままのMod-Morphキーの基点のmaskは取得せず、判定でShiftと合成しない。例えば`Shift + &gresc`は基点ではShiftをmaskして`` ` ``を出すが、判定ではC01になる。既定のkeymapにはそのようなbindingはない |
| 文字列macro | `&macro`とLauncherの文字列macro（`send_string`）のkeycode eventは、キーの押下と同じく置換器を通る。`keychron_b1_us`は米国配列用の表で文字列を変換する。JISホストでの結果は確認していない |
| grave accentとOS mode | 第8節の項目は確認していない |
| ショートカット | Ctrl、Alt、GUIは保持し、判定はShiftだけで行うため、`Ctrl + =`は`Ctrl + Shift + MINUS`になる。アプリケーションがこのchordをどう解釈するかは確認していない |
| 入力条件 | かな入力、全角記号の設定、リモートデスクトップ（文字転送とスキャンコード転送）は確認していない |
| 切断と復帰 | Bluetoothまたは2.4 GHzの切断、再接続、sleepからの復帰をまたぐ押下記録は、実機で試験していない |
| Launcher | `&usjis`を置いた位置はLauncherでは空欄に見え、そこにキーを割り当てるとbehaviorが置き換わる。Launcherの保存済みkeymapは、firmwareの既定と異なる位置を起動時に上書きし（`launcher.c`の`via_ee_read_keymap`）、firmwareを書き直しても消えない。既定を変えたkeymapを書き込んだ後は、Launcherの「reset keymap」を使う |
