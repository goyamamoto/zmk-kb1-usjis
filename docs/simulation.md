# Input Processing Simulation

## What it is

A Zephyr `native_posix_64` program that builds and runs the actual input-processing code of the pinned Keychron ZMK on a PC. It needs no keyboard: nothing is connected or flashed.

It is not an emulator of the whole B1 Pro. It injects presses and releases into behaviors and runs them through the keycode event, the event manager, `hid_listener` and the HID state. The send function is replaced with one that records each report, and every report is checked. The pinned C files that are compiled are unmodified, and nothing re-implements the input path in another language; only the send side (`endpoints.c`) is replaced. The other listeners of the firmware build (combo, hold-tap, sticky key, caps word, key repeat, `ble.c`, the Launcher's `mousekey.c`) are not linked; [tests/simulation/README.md](../tests/simulation/README.md) has the exact boundary.

## Running it

Requirements: Git, Python 3 and Docker with Linux containers (linux/amd64; on Apple Silicon it runs under emulation).

```sh
bash scripts/run-simulation.sh                           # all variants
SIMULATION_VARIANTS=usjis bash scripts/run-simulation.sh # only the module's tests
```

The first run fetches the pinned sources into `workspace/simulation/` and builds the container image from `tests/simulation/Dockerfile`. Later runs reuse both; the preparation step still tries one `git ls-remote` to warn if the Zephyr branch has moved (offline, it waits up to 60 seconds and continues). The build and run container has no network, mounts the sources read-only and gets no devices. Results go to `build/simulation-results/` (`results.json` and one log per variant).

Pins: Keychron ZMK `c284513085c005edf5f9a52b28c8090cc6ed01d2`, Zephyr `1ae0eb5ce8adafcec993e6fb8f4eeb6f818a7772`, the base image by digest, apt packages by version from a dated `snapshot.debian.org`, and Python packages by version and sha256. `tests/simulation/dependencies.json` also holds the sha256 of every pinned file that is compiled, included or read at configure time, and the runner fails if a file is not in that lock. `docker build` is not byte-reproducible, so the image ID differs between machines; compare runs by `executable_sha256` and `log_sha256` per variant.

Files: [test structure and boundary](../tests/simulation/README.md), [dependency lock](../tests/simulation/dependencies.json), [module tests](../tests/simulation/src/usjis_main.c), [Mod-Morph characterization](../tests/simulation/src/main.c) and its [bindings](../tests/simulation/app.overlay).

## Variants

| Variant | What it builds | Purpose |
| --- | --- | --- |
| `usjis` | This module with the HKRO `hid.c` of `keychron_b1_us` (PID `0x0711`) | Checks the substituter: C01–C20 and the S tests of the [substitution spec](design/usjis-substitution.md) (all but S09; S10 only for the default at boot), plus shared output usages, repeated presses, reverse-order release and the layer mask |
| `usjis-adaptive` | The same with the adaptive NKRO `hid.c` of `keychron_b1_usn` (PID `0x071a`) | The same checks on the n version's HID code |
| `hkro`, `adaptive` | The substitution table written as stock Mod-Morph bindings, no module | Characterizes the stock Mod-Morph approach and reproduces its defects (below) |
| Five negative controls | Deliberately wrong Mod-Morph tables | Show that the checks detect a wrong usage or a wrong Shift |

## Results

All variants on the current tree (`results.json` status `CHARACTERIZATION_PASS_NOT_FIRMWARE_ACCEPTANCE`, which says that a passing simulation is not a hardware acceptance):

| Variant | Result | Table sequences | Scenarios | Assertions | Reports checked |
| --- | --- | --- | --- | --- | --- |
| `usjis` | `USJIS_TESTS_PASS` | 90 | 120 | 4923 | 588 |
| `usjis-adaptive` | `USJIS_TESTS_PASS` | 90 | 120 | 4923 | 588 |
| `hkro` | Characterization passed, 7 known defects reproduced | 90 | — | 3796 | 447 |
| `adaptive` | Characterization passed, 8 known defects reproduced | 90 | — | 5666 | 463 |
| Negative controls (5) | Each failed at its intended check (exit code 1) | — | — | — | — |

The 90 table sequences are the 14 rows with Shift × Left, Right and both Shifts × two release orders, plus the 6 rows without Shift. The module variants check every report as `{modifiers, [usages]}` in order, and the state after each scenario. In the characterization variants, the same 90 sequences show the stock defects in intermediate states: 33 lose the added Shift when the physical Shift is released first, and 42 report Shift as released when the key is released first.

`SIMULATION_PASS` of the characterization variants means that the expected results and the known defects of the stock approach were observed, not that those defects are fixed; the module variants are the ones that must meet the spec.

## Checking for keycode confusion

C08 (`_`), C15 (`\`) and C16 (vertical bar) must each produce a different combination of usage and modifier. The characterization variants check these three reports, and a negative control that sets C16 to `0x87`, the same as C08, must fail.

| Item | Input | Output | Observed report (hex) |
| --- | --- | --- | --- |
| C08 | Shift + US MINUS | Shift + International1 (`0x87`) | `01 02 00 87 00 00 00 00 00` |
| C15 | US BACKSLASH | International1 (`0x87`) | `01 00 00 87 00 00 00 00 00` |
| C16 | Shift + US BACKSLASH | Shift + International3 (`0x89`) | `01 02 00 89 00 00 00 00 00` |

The values are from the `hkro` variant. The first byte is the report ID, followed by the modifiers (`02` = Left Shift), a reserved byte and six usage slots. These are states at the endpoint API, not captured USB frames.

The five negative controls each fail with exit code 1 at the intended check: C16 set to `0x87` (HKRO and adaptive), C01 without its added Shift, the Mod-Morph trigger on Left Shift only (detected only by the Right Shift condition, because a leaked Left Shift and a correct added Shift look the same), and C02 without its Shift mask. A build failure or an unrelated crash does not count as a success of a negative control.

The usage distinctions follow [Microsoft's keyboard input documentation](https://learn.microsoft.com/en-us/windows/win32/inputdev/about-keyboard-input) and [the kbd project's notes on Japanese keys](https://kbd-project.org/docs/scancodes/scancodes-8.html). The simulation does not include the Windows keyboard layout or the IME; the characters themselves are checked on a Windows host.

## Why a Mod-Morph keymap is not enough

Writing the table as Mod-Morph bindings in a keymap, without a module, gives the right first character but breaks in the following ways. All but the last come from upstream ZMK's Mod-Morph and HID code, so moving to official ZMK would not remove them. The module keeps its own press records and reports every event itself while a substituted key is held ([US-JIS architecture](design/usjis-architecture.md)), which avoids the first, second, fourth, fifth and sixth. The third remains by design, because a key's substitution is decided at press (spec section 6). The seventh does not apply (the module uses no Mod-Morph), and the eighth applies to the module as well.

| Defect (`KNOWN_DEFECT` in the log) | Origin | Operation and result |
| --- | --- | --- |
| implicit-shift-lost-on-physical-shift-release | ZMK HID (implicit modifiers assigned on press, cleared as a whole) | After Shift + MINUS is substituted, releasing the physical Shift first drops the added Shift while `0x87` is held: `_` turns into `\` |
| physical-shift-reported-released-on-morph-release | ZMK Mod-Morph (mask removed after the release report) | Releasing MINUS first sends modifiers `00` although the physical Shift is held; Shift reappears at the next key |
| physical-shift-after-key-applies-to-unsubstituted-usage | ZMK Mod-Morph (binding chosen at press) | Shift pressed after US `2` applies to the unsubstituted usage: `"` instead of `@` on a JIS host |
| implicit-shift-overwritten-by-unrelated-key | ZMK HID | Holding EQUAL, then Shift + MINUS, then pressing A keeps the MINUS usage but Shift does not return after A is released |
| mask-cleared-while-other-morph-held | ZMK Mod-Morph (one shared mask) | Overlapping Shift + 2 and Shift + 6 and releasing one clears the mask; Shift then applies to the other |
| shared-output-released-while-equal-held | ZMK HID listener (pre-release of the same usage) | MINUS and EQUAL share an output usage; releasing one releases it while the other is held |
| same-mod-morph-instance-second-press-rejected | ZMK Mod-Morph (`-ENOTSUP` by design) | The same Mod-Morph instance pressed from a second position is rejected |
| international1-dropped-after-six-keys | Keychron adaptive NKRO (`NKRO_MAX_USAGE`) | With A to F held, International1 (`0x87`) is not added to the HID state, and no error is reported. This applies to the module too ([README](../README.md#the-n-version-0x071a-untested)) |

In the adaptive variant, the report count is the number of `zmk_endpoints_send_report` calls. The real n-version firmware sends fewer, because `endpoints.c` skips reports in which only implicit or masked modifiers changed; for the first defect this means a delayed Shift release on hardware instead of a different character.

## Scope

Not covered: the key matrix and kscan, the full keymap, combos, hold-tap, the Launcher, the radios, flash and settings storage, DFU and the bootloader, and Windows' interpretation of the reports.
