# Contributing

> この文書は[英語版](CONTRIBUTING.md)の日本語訳です。内容が食い違う場合は英語版を正本とします。

IssueとPull Requestを歓迎します。日本語でも英語でもかまいません。

## 報告

- **脆弱性**: 公開のIssueではなく、非公開で報告してください（[セキュリティポリシー](SECURITY.md)）。
- **実機での結果**: PID（READMEを参照）、firmwareのcommit、接続方式（USB、Bluetooth、2.4 GHz）、ホストOSとそのキーボード配列、何をして何が起きたか。未検証のn版（`0x071a`）の結果は特に役立ちます。
- **不具合**: 手順、期待した文字と実際の文字、US-JIS置換がオンだったか。`config/keymap-options.h`やkeymapを変更した場合は、そのことも書いてください。
- firmwareのバイナリ（`.uf2`、`.hex`、`.elf`）は添付しないでください。代わりにcommitと、`build/firmware/build-info.json`にあるSHA-256を書いてください（[ライセンス参照](docs/references/licenses.ja.md)）。

## Pull Request

1つのPull Requestには1つの話題だけを含め、関係のない整形の変更を混ぜないでください。変更した内容に応じた検証を実行し、結果を説明欄に書いてください。

| 変更したもの | 実行するもの |
| --- | --- |
| すべて | `bash scripts/validate-repository.sh`（必須ファイル、固定値、Markdownのリンク、仕様のCとSのID） |
| `src/`、`include/`、`dts/`、置換表、S test、`tests/simulation/` | `bash scripts/run-simulation.sh`（[シミュレーション](docs/simulation.ja.md)） |
| firmware buildが読むもの（`config/`、`CMakeLists.txt`、`Kconfig`、`zephyr/module.yml`、ソース、`scripts/build-firmware.sh`） | `bash scripts/build-firmware.sh`。`build-info.json`にある生成物のSHA-256を書く |

キーボードへ書き込んだ場合は、「報告」に挙げた実機の情報を書いてください。実機で確認していないことは、確認していないと明記してください。

置換器の動作を変えるときは、同じPull Requestで[置換仕様](docs/design/usjis-substitution.ja.md)とテストを更新してください。仕様のS testはS01〜S20と番号が付いており、`scripts/validate-repository.sh`がその番号を検査します。テストを追加したときは、その検査も更新してください。

## ルール

- **GPLのコードは入れません。** Keychron ZMKには、GPL-2.0-or-laterのファイル（例: `app/src/launcher/`）と、表記がないがその多くの部分がQMKのコードと一致するファイルがあります（[ライセンス参照](docs/references/licenses.ja.md)）。それら、QMK、Keyboard Quantizerのコード、表、コメントをこのリポジトリへコピーしないでください。観察した挙動を自分の言葉で説明し、ZMKのAPIから実装してください。
- 固定しているKeychron ZMKのcommitを変更するには、理由と、何を再テストしたかの説明が必要です。
- `workspace/`や`build/`の中のものはcommitしないでください。

## 文書

英語の`.md`を正本とし、日本語訳は同じ場所に`.ja.md`で置きます。英語版を先に変更し、できれば同じPull Requestで日本語訳も更新してください。新しい文書は英語版だけでかまいません。
