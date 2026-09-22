# Contributing

Issues and pull requests are welcome. Japanese or English is fine.

## Reporting

- **Vulnerabilities**: report them privately, not in a public Issue ([security policy](SECURITY.md)).
- **Hardware results**: the PID (see the README), the firmware commit, the connection (USB, Bluetooth, 2.4 GHz), the host OS and its keyboard layout, what you did and what happened. Results for the untested n version (`0x071a`) are especially useful.
- **Bugs**: the steps, the expected and the actual characters, and whether US-JIS substitution was on. If you changed `config/keymap-options.h` or the keymap, say so.
- Do not attach firmware binaries (`.uf2`, `.hex`, `.elf`); give the commit and the SHA-256 from `build/firmware/build-info.json` instead ([licenses reference](docs/references/licenses.md)).

## Pull requests

Keep one topic per pull request, and do not mix unrelated formatting changes into it. Run the checks that match what you changed and give the results in the description:

| You changed | Run |
| --- | --- |
| Anything | `bash scripts/validate-repository.sh` (required files, pins, Markdown links, the C and S IDs of the spec) |
| `src/`, `include/`, `dts/`, the substitution table, the S tests or `tests/simulation/` | `bash scripts/run-simulation.sh` ([simulation](docs/simulation.md)) |
| Anything the firmware build reads (`config/`, `CMakeLists.txt`, `Kconfig`, `zephyr/module.yml`, sources, `scripts/build-firmware.sh`) | `bash scripts/build-firmware.sh`, and give the artifact SHA-256 from `build-info.json` |

If you flashed a keyboard, give the hardware details listed under Reporting. Say explicitly what you did not check on hardware.

When you change the behavior of the substituter, update the [substitution spec](docs/design/usjis-substitution.md) and the tests in the same pull request. The spec's S tests are numbered S01–S20, and `scripts/validate-repository.sh` checks that numbering; update the check when you add a test.

## Rules

- **No GPL code.** Keychron ZMK contains files under GPL-2.0-or-later (for example in `app/src/launcher/`) and files that largely match QMK code without a notice ([licenses reference](docs/references/licenses.md)). Do not copy code, tables or comments from them, from QMK, or from Keyboard Quantizer into this repository. Describe the behavior you observed in your own words and implement it from the ZMK API.
- Changing the pinned Keychron ZMK commit needs a reason and a description of what was re-tested.
- Do not commit anything from `workspace/` or `build/`.

## Documentation

The English `.md` files are canonical; Japanese translations sit next to them as `.ja.md`. Change the English file first and update the translation in the same pull request if you can. A new document needs only the English file.
