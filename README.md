# ZMK US-JIS for Keychron B1 Pro

Firmware for the Keychron B1 Pro US layout (PID `0x0711`) that types what its keycaps show even on hosts whose keyboard layout is fixed to Japanese, and that fixes a few usability problems of the stock firmware. It is a Zephyr module plus a keymap on top of Keychron's ZMK fork, built reproducibly in a container.

Japanese: [README.ja.md](README.ja.md)

At a glance:

- **US-JIS mode** (Fn+Tab): on a host set to the Japanese keyboard layout, `` ` ~ @ ^ & * ( ) _ = + [ { ] } \ | : ' " `` type as printed on the US keycaps. Off by default, remembered across power cycles, Win mode only.
- **IME keys beside Space**: tap the left one for IME off and the right one for IME on (Win: Muhenkan / Henkan, Mac: Eisu / Kana); held, they stay Alt / Cmd.
- **Caps Lock and Left Ctrl swapped**, no Fn-layer delay, a factory reset that does not fire by accident.
- Same features for the NuPhy Air60 V2 in QMK: [goyamamoto/qmk-firmware](https://github.com/goyamamoto/qmk-firmware).

## Supported hardware: check your PID first

Keychron sells several B1 Pro versions under one name. This firmware is for exactly one of them, and firmware for another version must not be flashed: the versions differ in the key matrix and in the firmware they run, so a mismatch gives wrong or dead keys at best.

| USB PID | Keychron version | Layout | Status here |
| --- | --- | --- | --- |
| `0x0711` | B1 Pro US, ZMK based | ANSI | **Supported**: this firmware, verified on hardware |
| `0x071a` | B1 Pro US "n" version, ZMK based | ANSI | **Untested build available**: `SHIELD=keychron_b1_usn bash scripts/build-firmware.sh build` gives `keychron-b1-usn-usjis.uf2`. Nobody has flashed it yet; see [The n version](#the-n-version-0x071a-untested) |
| `0x0714` | B1 Pro (official list labels it ANSI) | ANSI | Not supported: its firmware is distributed encrypted (`.kfw`), no source is public, it may not be ZMK at all |
| `0x0712`, `0x071b`, `0x0713`, `0x071c` | UK and JIS versions | ISO, JIS | Not supported |

How to find your PID:

- Keychron Launcher: Settings → Device Info.
- macOS: `ioreg -p IOUSB -l -w0 | grep -A25 'Keychron B1 Pro@' | grep -E '"(idVendor|idProduct)"'`. The value is decimal; `1809` is `0x0711`.
- Windows: Device Manager → the keyboard → Details → Hardware Ids, which contain `VID_3434&PID_0711`.
- Linux: `lsusb`, which lists `3434:0711`.

Only if it is `0x0711`, continue with the default build. For `0x071a`, read the section below and decide for yourself. For the other versions, the safe option is to keep the official firmware. The table comes from the matrix and base-layer keycodes of each shield in the pinned Keychron ZMK and from [Keychron's firmware download list](https://www.keychron.com/blogs/archived/how-to-use-the-launcher-web-app-or-manually-flash-firmware-for-your-b-pro-series-keyboard).

### The n version (`0x071a`), untested

The pinned Keychron ZMK has a shield for it (`keychron_b1_usn`): a different key matrix, the same key positions in the keymap, and an "adaptive NKRO" HID mode. `config/keychron_b1_usn.keymap` applies this project's keymap changes to Keychron's n-version keymap, and the module's tests also run on the adaptive NKRO HID code in the simulation (`usjis-adaptive` variant). What has not happened is a build being flashed to an `0x071a` keyboard, so treat it as a best-effort port:

- The recovery path should be the same (reset switch, `NRF52BOOT` drive; Keychron's own n-version firmware is a UF2 for the nRF52840), but that is inferred from the sources, not tried.
- `B1_SWAP_CTRL_CAPS` is ignored for this shield: the n version's firmware has a matrix workaround for Caps+Alt chords that is enabled only when the Caps/Ctrl swap is made in the Launcher, so make the swap there.
- In adaptive NKRO, when seven or more keys are held, the JIS keys `\` and `_` (International1, usage `0x87`) are dropped by the stock HID code. Ordinary typing is not affected.
- Please report the result either way in an Issue: which keys work, and whether the PID and version show correctly.

## What the firmware does

Compared with the stock firmware, the keyboard behaves as follows. Everything else (USB, Bluetooth, 2.4 GHz, the Launcher, DFU, battery, LEDs) is the stock code at the pinned commit.

| Feature | Operation | Notes |
| --- | --- | --- |
| US-JIS substitution | Fn+Tab toggles it. Off on first boot; the setting survives power cycles | Only with the OS switch in the Win position. The symbols `` ` ~ @ ^ & * ( ) _ = + [ { ] } \ \| : ' " `` come out as printed on a Japanese host. macOS does not need it |
| No Fn-layer delay | — | The stock Fn+J+Z and Fn+X+L combos held J, Z, X and L back for up to 1 s while Fn was held. They are gone |
| Factory reset | Fn+Shift+Esc held for 10 s | Clears Bluetooth bonds and Launcher settings. Fn+Esc alone is Esc. Unplug and reconnect USB afterwards |
| F-row swap | Fn+Caps Lock held for 3 s | Same function as the stock Fn+X+L |
| Battery level, Win lock | Fn+B, Fn+Win held for 3 s | Unchanged |
| IME off / IME on | Tap the key left / right of Space. Held, they are the usual modifiers (Win: Alt, Mac: Cmd) | Win: Muhenkan / Henkan, Mac: Eisu / Kana. Optional, see below |
| Ctrl and Caps Lock swapped | — | On the base layers of both OS modes. Fn+Caps Lock still swaps the F row. Optional, see below |

The IME keys and the Ctrl/Caps swap are compile-time options in [config/keymap-options.h](config/keymap-options.h) (`B1_IME_TAP`, `B1_SWAP_CTRL_CAPS`, both 1 by default). Set them to 0 and rebuild if you do not want them.

On Windows, the IME keys need one setting in Microsoft IME: assign Muhenkan to "IME-off" and Henkan to "IME-on" (設定 → 時刻と言語 → 言語と地域 → 日本語 → Microsoft IME → キーとタッチのカスタマイズ). macOS needs no setting.

## Flashing

**No firmware binary is published.** The built firmware links Keychron's code (parts of it GPL-2.0-or-later, parts without a license notice) and a Nordic binary library, and this repository has no right to redistribute that combination; see the [licenses reference](docs/references/licenses.md). Build it yourself with the command in [Building](#building), which needs only Docker, then flash the `.uf2` it produces.

1. Hold the reset switch in the hole on the back of the keyboard while connecting USB. A drive named `NRF52BOOT` appears.
2. Copy `build/firmware/keychron-b1-us-usjis.uf2` to the drive. The drive disappears and the keyboard restarts.
3. The same path recovers the keyboard from any firmware, including the official one, because the bootloader is not touched.

Launcher notes:

- Positions that hold this firmware's own behaviors (Fn+Tab, Fn+Esc, Fn+Caps Lock, the two keys beside Space) show as blank in the Launcher. They work; assigning something to them in the Launcher replaces the behavior.
- The Launcher's saved keymap overrides the firmware defaults at boot wherever they differ, and the F-row swap saves the whole layer. After flashing a firmware whose base-layer defaults changed (for example the Ctrl/Caps swap), run "reset keymap" in the Launcher once. The factory reset does not clear the saved keymap.
- The USB device version (`bcdDevice`) is 1.04 and the Launcher shows v1.0.4, like the stock firmware; the Launcher's build date is the commit time of the build.

## Building

You need Git and Docker; nothing else is installed on your machine. The toolchain runs inside the pinned `zmkfirmware/zmk-build-arm:3.2` image (Zephyr SDK 0.15.2). Tested on macOS with Docker Desktop on Apple Silicon (the image is linux/amd64 and runs under emulation); Linux with Docker should work the same; on Windows use WSL2 with Docker Desktop (untested).

### Step by step

1. Install Docker (Docker Desktop on macOS or Windows, the `docker` package on Linux) and start it. Check with `docker info`.
2. Clone this repository and enter its directory.

3. Optionally edit `config/keymap-options.h` (IME keys, Ctrl/Caps swap) or `config/keychron_b1_us.keymap`.
4. Build:

   ```sh
   bash scripts/build-firmware.sh
   ```

   The first run downloads the image (about 2 GB) and the pinned sources (about 2 GB with the build tree, into `workspace/firmware/`), applies the Zephyr patch and builds. Expect 10 minutes or more on the first run, and 1 to 2 minutes for later builds. It ends with a JSON summary that names the keymap file that was used and the SHA-256 of the artifacts.
5. The firmware is `build/firmware/keychron-b1-us-usjis.uf2`. Flash it as described in [Flashing](#flashing).

To rebuild after changing the keymap or the options, run `bash scripts/build-firmware.sh build`; it needs no network. `bash scripts/build-firmware.sh prepare` alone refreshes the sources.

### If something goes wrong

- `Cannot connect to the Docker daemon`: Docker is not running.
- `Repository path must not contain a comma`: move the clone to a path without a comma.
- `zmk checkout is not at the pinned commit` or `Zephyr patch is not applied`: delete `workspace/firmware/` and run the script again without arguments.
- The first run stops while downloading: it needs network access to GitHub and Docker Hub; run it again.
- The build itself fails: `build/firmware/build.log` has the compiler output. A build of an unmodified checkout has been verified; if yours fails, the change is most likely in the keymap.

### What the script does

`prepare` creates a west workspace in `workspace/firmware/` with this repository as its `config` directory, fetches Keychron ZMK and Zephyr at the commits that [config/west.yml](config/west.yml) pins (Zephyr's own manifest pins its modules), and applies the Zephyr patch that ships in the pinned ZMK commit (`0001-esb-nrf-fix.patch`) as a commit on top of the pinned Zephyr commit.

`build` builds `keychron` / `keychron_b1_us` with `ZMK_CONFIG` set to this repository's `config/`, so `config/keychron_b1_us.keymap` and `config/keychron_b1_us.conf` are used and this repository is detected as the Zephyr module `zmk-feature-usjis`. It runs without network and refuses to run if the checkouts are not at the pinned commits or the patch is not applied.

Outputs in `build/firmware/`:

- `keychron-b1-us-usjis.uf2` (also `.hex`, `.elf`, `.map`)
- `zephyr.config`, `zephyr.dts`, `zephyr_modules.txt`, `build.log`
- `build-info.json`: the repository commit, the commit of every west project, the patch hash, the image digest, the SDK, west, CMake and compiler versions, the Python packages, and the SHA-256 of the artifacts

The firmware carries `usjis-<commit>` as its Zephyr build version, and the build date is the commit time (`SOURCE_DATE_EPOCH`), so two builds of the same commit produce identical files. A build from a dirty working tree is marked `-dirty`.

## How the substitution works

The module's sources join the ZMK `app` library, and `CMakeLists.txt` moves the substituter's listener to just before `hid_listener.c`, after hold-tap, sticky key, caps word and key repeat. The module checks this order at startup and never substitutes if it is wrong. While a substituted key is held, the module reports every keyboard event itself so that the report's modifiers follow the conflict policy of the [substitution spec](docs/design/usjis-substitution.md); otherwise it passes events through unchanged. The [architecture](docs/design/usjis-architecture.md) has the details.

## Verification

```sh
bash scripts/validate-repository.sh   # repository structure, pins, links, IDs
bash scripts/run-simulation.sh        # native simulation (Docker)
```

The simulation builds the real pinned `hid.c`, `hid_listener.c` and event manager for `native_posix_64`. The `usjis` variant links this module and checks every HID report of the substitution table (C01–C20) and the S tests of the spec; the other variants characterize the stock Mod-Morph path and its known defects. See [Verification results and scope](docs/simulation.md). `SIMULATION_VARIANTS=usjis bash scripts/run-simulation.sh` runs only the module's tests.

## Repository Layout

```text
.
├── .github/        GitHub Actions workflows (build, simulation, validation), Dependabot
├── config/         west manifest, keymaps, conf files and keymap-options.h
├── docs/           substitution spec, architecture, simulation, references
├── dts/            devicetree binding of the &usjis behavior
├── include/        public header and dt-bindings of the module
├── scripts/        build, validation and simulation runners
├── src/            the substituter, its resolver table and the mode behavior
├── tests/          native input simulation
├── zephyr/         Zephyr module metadata
├── build.yaml      build targets
├── CMakeLists.txt  module build entry point
├── Kconfig         module configuration (US-JIS options, Keychron shield Kconfig)
├── build/          build and simulation outputs, not tracked by Git
└── workspace/      fetched sources, not tracked by Git
```

Documents: [US-JIS substitution spec](docs/design/usjis-substitution.md), [US-JIS architecture](docs/design/usjis-architecture.md), [input processing simulation](docs/simulation.md), [stock Fn combo keys](docs/references/b1-special-keys.md), [licenses and distribution policy](docs/references/licenses.md). Each has a Japanese translation next to it (`.ja.md`).

This repository does not vendor Keychron ZMK; [config/west.yml](config/west.yml) pins it to a commit and the build fetches it.

## Contributing

Issues and pull requests are welcome; see [CONTRIBUTING.md](CONTRIBUTING.md). Reports from hardware are especially useful: the PID, the connection (USB, Bluetooth, 2.4 GHz), the host OS and keyboard layout, and what happened.

## Upstream and acknowledgements

- Keychron ZMK: `https://github.com/Keychron/zmk.git`, branch `keychron_bpro`, pinned commit `c284513085c005edf5f9a52b28c8090cc6ed01d2`.
- The substitution table was worked out from the observable behavior of the US-key-on-JIS-OS key override of [Keyboard Quantizer](https://github.com/sekigon-gonnoc/vial-qmk) (branch `keyboard-quantizer-b`). No code, table or comment of it is used.

## License

This repository is under the [MIT License](LICENSE). The firmware you build from it also contains code under other terms; see the [licenses reference](docs/references/licenses.md).
