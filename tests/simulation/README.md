# Native simulation

Run from the repository root:

```sh
bash scripts/run-simulation.sh
```

Requires Git, Python 3 and Docker with BuildKit (Docker Desktop or root Docker;
rootless Docker/Podman map `--user` differently and are untested; Linux
containers; linux/amd64 emulation on Apple Silicon). First execution fetches pinned ZMK/Zephyr sources and builds the
container image from `Dockerfile` (base image pinned by digest, apt packages
pinned by version from a dated snapshot.debian.org archive, Python packages pinned by version and sha256 in
`requirements.txt`, tagged by the hash of both files; about 720 MB). The built
image's content ID is recorded in `results.json` as `simulation_image_id`.
Note that `docker build` is not byte-reproducible (layer timestamps), so two
builds of the same Dockerfile get different IDs; the stable cross-machine
identity is the `executable_sha256` and `log_sha256` per variant. Record local
runs of the CI-equivalent steps in the pull request description, with the commit
they ran on. Not yet verified with the current harness: building the image from
the snapshot, the `docker save`/`load` cache path, and running as uid 1001 on Linux (none of them
depends on the harness code). Sources are stored under ignored `workspace/simulation/`.
Subsequent executions need no dependency download when those sources and the
image are present (the preparation step still tries one `git ls-remote` to warn
about Zephyr branch drift; offline it waits up to 60 s and then continues). Dependencies are fixed by `dependencies.json`; the ZMK SHA
must match the project manifest.

The build/run container has no network, hardware/device passthrough or privileged
mode and runs as the invoking user (`--user`), so results are not root-owned on
Linux hosts. Repository/dependency sources are mounted read-only. Only
`build/simulation-results/` is writable from the container; it is emptied at the
start of every run (tool caches under dot-directories are kept) so it only ever
describes one execution. No flash command is
present. Source snapshots are validated before each run, and a source hash manifest
is included with the results. The existing research checkout is never checked out
or patched. Its working-tree edits are not used.

## Boundary

This is a Zephyr 3.2 `native_posix_64` application, not a full B1 emulator.
It compiles these **unchanged files from the pinned Keychron commit**:

- `hid.c`, `hid_listener.c`
- `event_manager.c`, `events/keycode_state_changed.c`
- `behaviors/behavior_key_press.c`, `behaviors/behavior_mod_morph.c`

It also uses the original headers, Devicetree bindings, generated behavior
syscalls, and event linker section. Test input calls the real behavior API with
positions/timestamps. Physical matrix scanning, keymap/layer dispatch, combos,
hold-tap and Launcher are outside this harness. The input is scripted; there is no
interactive keyboard GUI.

The endpoint function records the actual HID state at every call of
`zmk_endpoints_send_report`; it does not model USB/BLE/2.4 GHz timing or packet
conversion. In the adaptive build this is more than the real `b1n` endpoint
sends, because upstream `endpoints.c` gates transmission on `kb_changed()` /
`nkro_changed()` and implicit-modifier updates do not set those flags. For the
adaptive build this changes the symptom of the Shift-first characterization:
the report that drops the implicit Shift is not sent by the real `b1n` endpoint,
so the host keeps seeing Shift down until the next report. HKRO
(`keychron_b1_us`) has no such gate. Win-lock is stubbed
off and `macro_running` is zero. No other input/HID implementation is replaced,
but several that the `keychron_b1_us` firmware links are simply absent: the
firmware's `app/CMakeLists.txt` compiles (in any non-split build, which B1 is) `combo.c`,
`behavior_hold_tap.c`, `behavior_sticky_key.c`, `behavior_caps_word.c` and
`behavior_key_repeat.c`, and `ble.c` and the Launcher's `mousekey.c` also
subscribe to `keycode_state_changed` (`wpm.c` too, when `CONFIG_ZMK_WPM` is on; it is
not in the B1 defconfig). Static reading says they are passive for
the key-press / Mod-Morph inputs used here (they act only on their own
behaviors, on the GEN_BUTTON page, or on reconnect), so the recorded reports
should be unchanged, but that is a static argument and the firmware's listener
order (link order of the `.event_subscription` section) is not reproduced.
Nordic-specific startup, LED, radio, power, DFU and firmware packaging are not
linked. Consequently the Nordic ESB patch is not applied to this native-only
Zephyr checkout. Do not reuse it as a hardware build workspace without the
separate hardware build procedure.

The original full `app` unconditionally links hardware-dependent LED and DFU
units, and its startup expects `kscan0` and battery/radio functions. This isolated
app tests the relevant implementation without changing the hardware firmware to
make native builds work. It does not assert that the original full `west test`
suite runs unchanged.

## Substituter variant (`usjis`)

The `usjis` variant builds `src/usjis_main.c` instead of `src/main.c`, links this
repository as the Zephyr module `zmk-feature-usjis` (`-DZEPHYR_MODULES=/repo`; there
is no west workspace) and compiles the pinned `events/endpoint_changed.c` instead of
`behavior_mod_morph.c`. The module's own CMake places the substituter's listener just
before `hid_listener.c`, exactly as in the firmware build, and the test program checks
the module's startup order check first. Its bindings are `usjis.overlay` (plain `&kp`
and `&usjis`); `app.overlay` is not used. It runs the substitution table C01–C20
(90 sequences: left, right and both Shift, both release orders, plus the unshifted rows),
the binding-Shift variant, Ctrl kept, S01–S08 and S11–S20 of the specification, the
shared output usage `0x2D` in both orders, a repeated press of one usage and a
reverse-order release. Every report is checked as `{modifiers,[usages]}` in order.
S17 reproduces the endpoint clear by calling the HID clear functions and raising
`zmk_endpoint_changed` as `endpoints.c` does. The summary line is
`SUMMARY usjis table=90 scenarios=N assertions=M reports=R consumer_reports=C` and the
pass marker is `USJIS_SIMULATION_PASS`. `SIMULATION_VARIANTS=usjis bash scripts/run-simulation.sh`
runs only this variant locally.
The `usjis-adaptive` variant runs the same program on the adaptive NKRO build of
`hid.c` (the `keychron_b1_usn` configuration, PID `0x071a`).

## Tests and results

- Unchanged key press and normal Mod-Morph branch controls.
- C01–C20: 90 sequences (14 shifted rows × left/right/both × two release
  orders, plus 6 unshifted rows). The numeric HID usage/modifier expectations in
  C are written from the specification table, not derived from the DTS overlay;
  the same author wrote both, so this separates the oracle from the bindings,
  not from the reviewer. The expected modifier for every Shift-adding row is
  Left Shift (`0x02`) regardless of which physical Shift was pressed; the
  specification fixes this (section 4), so a future engine that preserved the
  physical side would fail, by design, every sequence of the 11 rows that take
  physical Shift and add Shift when Right Shift or both Shifts are pressed
  (11 rows × 2 Shift variants × 2 release orders = 44 sequences).
- In the 42 "Shift released first" sequences the intermediate state after the
  physical Shift release is checked as a characterization: the pinned upstream
  drops the implicit Shift while the substituted usage is still held, so in the
  11 rows × 3 Shift variants = 33 sequences that need Shift the host sees the
  unshifted JIS key (for C08 `_` becomes `\`). These sequences count as PASS for release matching and final
  state, and are reported separately as `shift_first_defect_scenarios`.
- In the 42 "morph released first" sequences the release report shows modifier
  `00` although the physical Shift is still held: Mod-Morph clears its Shift
  mask only after the release report and clearing sends no report. Counted as
  `release_masked_shift_scenarios`; see KNOWN_DEFECT
  `physical-shift-reported-released-on-morph-release`.
- No automatic taps while a key is held (one-second hold check). For the units
  compiled here this is expected — none of them owns a timer — so it guards the
  harness itself, not firmware or host auto-repeat behaviour.
- Regression characterizations of the pinned upstream implementation's known
  modifier/usage-ownership defects. These intentionally assert the observed
  defects; their passing **does not mean the defects are fixed**. Two of them
  are mainline ZMK design limits rather than Keychron changes:
  `same-mod-morph-instance-second-press-rejected` (the intended `-ENOTSUP`) and
  `implicit-shift-overwritten-by-unrelated-key` (a consequence of the single
  global implicit-modifier state: every press assigns it and every release
  clears it). The `hid_listener.c` comment acknowledges only the release-time
  symptom (releasing an earlier key drops a later key's Shift), not the
  press-time overwrite this scenario shows. Under the substitution spec (S12)
  dropping that Shift while `A` is held is intended; what remains a defect is
  that it is not restored when `A` is released. In fact all seven HKRO defects come
  from logic the pinned tree shares with mainline ZMK (Mod-Morph and HID); only
  `international1-dropped-after-six-keys` is Keychron-specific (adaptive NKRO).
- Every Mod-Morph press event and release event produces exactly one report.
  This bounds the reports of those two events only; reports caused by other
  events in a sequence (physical Shift release) are covered by the
  characterization above.
- HKRO and adaptive-NKRO HID-state builds. Adaptive transport serialization is
  not covered; high international usage overflow is characterized separately,
  with a positive control (a seventh ordinary usage does enter the NKRO bitmap)
  and the observation that the press API returns success while dropping it.
- Five negative-control builds, each breaking one overlay binding on purpose:
  `negative-usage` maps C16 to International1 and must fail at the C16
  expected-usage assertion; `negative-modifier` drops the Shift from C01 and must
  fail at the C01 modifier assertion; `negative-mask-left-only` restricts the
  Mod-Morph trigger to Left Shift and must fail in the Right-Shift variant
  (`shift1`), which is the only variant that can tell a leaked physical Shift
  from a correct implicit one; `negative-suppress` keeps the Shift that C02
  must drop and must fail at the C02 modifier assertion; `negative-usage-adaptive`
  repeats the usage control in the adaptive build. A build error, timeout, unrelated failure
  or a successful run fails the control.
- Kconfig coverage: the harness declares only the symbols the compiled units
  need (`Kconfig`), it does not source upstream `app/Kconfig`. After each build
  the runner collects every `CONFIG_` token from the pinned ZMK files that were
  compiled or included (from `ninja -t deps`) and fails if one is not decided by
  the effective `.config`, except `CONFIG_ZMK_USB`/`CONFIG_ZMK_BLE`
  (`endpoints.h`; `endpoints.c` is replaced) and `CONFIG_BT_MAX_PAIRED` (a
  `ble.h` macro; `ble.c` is not compiled). The choice alternatives
  (`ZMK_HID_REPORT_TYPE_NKRO`, `ZMK_HID_CONSUMER_REPORT_USAGES_BASIC`) and the
  split symbols are declared "not set" in `Kconfig`, as in the B1 build. Known
  differences from the firmware build: `ZMK_LOG_LEVEL` is 0 here (upstream
  default 1); `CONFIG_ASSERT=y` and `CONFIG_LOG=y` are harness-only (the
  firmware build compiles `__ASSERT` out). The check only asks whether a symbol
  is decided, not whether its value matches the firmware.
- Each variant is built from a fresh build directory, so the recorded executable
  hash and build log describe a clean build of the pinned sources.

Which build corresponds to the project's firmware target:

| Build | Upstream shield (`Kconfig.shield`) | USB PID | Note |
| --- | --- | --- | --- |
| `hkro` | `keychron_b1_us` (project `build.yaml` target) | `0x0711` | does not select `ADAPATIVE_NKRO` |
| `adaptive` | `keychron_b1_usn` | `0x071a` | selects `ADAPATIVE_NKRO`; not the current target |

Files in `build/simulation-results/`:

- `results.json`: counts, known defects, dependency/test/binary/log hashes, the
  number of locked dependency files, Kconfig coverage, per-step durations, and
  the simulation image tag and content ID.
- `hkro-reports.log`, `adaptive-reports.log`: every attempted keyboard report.
- `negative-*-reports.log`: expected failures on the intentionally wrong bindings.
- `*-configure.log`, `*-build.log`: configuration/build evidence.
- `hkro/zephyr/.config`, `adaptive/zephyr/.config`: effective settings.
- `source-provenance.json`: original ZMK file hashes and dependency identities,
  written before the first build so it is present for failed runs too.

`dependencies.json` also locks the sha256 of every pinned ZMK file the harness
compiles, includes or reads at CMake time. Compile/include dependencies are
derived after each build from `ninja -t deps` and the DTS preprocessor depfile;
CMake-time dependencies (the behavior binding yamls, their `include:` targets and
the event linker script) cannot be derived that way and are enumerated in
`run.py`. `run.py` fails if any derived or enumerated file is missing from the
lock or differs from it, and if the lock names a file the build no longer
uses. Before building, `run.py` also checks the whole exported
tree against `provenance.json` and the locked files against the lock; on failure
it overwrites `results.json` with `FAILED` so no previous run's results survive. It pins Zephyr by commit and also records the branch the
firmware manifest imports (`zephyr_ref`) and when it was resolved.
`prepare-simulation.py` warns, without failing, when that branch no longer
points at the pinned commit.

`REPORT` lines contain the scenario, sequential send number, then hexadecimal
bytes: report ID, modifier mask, reserved byte, then the keyboard-state array.
HKRO has six usage slots. The adaptive build's underlying struct has a larger
array; this recorder dumps the entire struct, including unused trailing bytes,
rather than the transport-specific serialized length. `0x02` is left Shift and
`0x20` is right Shift. These are observations at the endpoint API; physical USB
boot/report protocol framing is not tested.

The `SIMULATION_PASS` marker means characterization checks ran successfully.
Production acceptance remains blocked by the explicitly reported known defects.
When the implementation changes, replace those characterizations with tests of
the agreed corrected behavior; do not silently remove them.

This harness does not load the Windows keyboard layout. It verifies distinct HID
input and state, not the resulting Unicode character, IME behavior, shortcut
semantics or host-generated repeat. See [the results and limitations](../../docs/simulation.md).
