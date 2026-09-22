#!/usr/bin/env python3
"""Run the real native executable and retain logs, config and source provenance.

This script runs inside the container started by scripts/run-simulation.sh:
the repository is mounted read-only at /repo and results are written to /out.
"""
import hashlib
import json
import os
from pathlib import Path
import re
import shutil
import subprocess
import time

ROOT = Path("/repo")
OUT = Path("/out")
# Bounds for a hang, not a slow build. Measured on an otherwise idle Apple
# Silicon host under linux/amd64 emulation all variants finish in a few
# minutes, but a loaded host can push a single configure past two minutes,
# so leave headroom. Actual durations are recorded in results.json.
CONFIGURE_TIMEOUT = 300
BUILD_TIMEOUT = 600
RUN_TIMEOUT = 60
ZMK_SOURCE = "/repo/workspace/simulation/zmk"
LOCK = json.loads((Path("/repo/tests/simulation/dependencies.json")).read_text())
APP = ROOT / "tests/simulation"

# Defects of the pinned upstream implementation that main.c characterizes.
# Update this list deliberately together with main.c and docs/simulation.md.
HKRO_DEFECTS = [
    "implicit-shift-lost-on-physical-shift-release",
    "physical-shift-reported-released-on-morph-release",
    "implicit-shift-overwritten-by-unrelated-key",
    "physical-shift-after-key-applies-to-unsubstituted-usage",
    "mask-cleared-while-other-morph-held",
    "shared-output-released-while-equal-held",
    "same-mod-morph-instance-second-press-rejected",
]
EXPECTED_DEFECTS = {
    "hkro": HKRO_DEFECTS,
    "adaptive": HKRO_DEFECTS + ["international1-dropped-after-six-keys"],
}


def logged(command, path, timeout, quiet_failure=False):
    with path.open("w") as log:
        try:
            # On timeout only the direct child is killed; any ninja/gcc
            # grandchildren die with the container, which exits on the raised
            # error. (Running the child in its own session was tried and made
            # configure hang intermittently under emulation.)
            result = subprocess.run(command, stdout=log, stderr=subprocess.STDOUT, timeout=timeout)
        except subprocess.TimeoutExpired:
            log.flush()
            print(path.read_text()[-6000:], flush=True)
            raise RuntimeError("%s timed out after %ds (see %s)" % (command[0], timeout, path.name))
    if result.returncode and not quiet_failure:
        print(path.read_text()[-6000:], flush=True)
    return result.returncode


# Negative controls: each deliberately breaks one binding in the overlay and must
# fail at the named assertion. A usage swap and a modifier drop are both covered
# so the harness is shown to detect wrong usage AND wrong Shift handling.
# The regexes are coupled to the stringified assertion text in main.c.
NEGATIVE_CONTROLS = {
    "negative-usage": {
        "adaptive": False,
        "replace": ("LS(JIS_YEN)", "LS(JIS_RO)"),
        "expect": r"FAIL C16-shift0-release0 .*keys\[i\] == usage",
        "result": "EXPECTED_FAILURE_C16_WRONG_USAGE",
    },
    "negative-modifier": {
        "adaptive": False,
        "replace": ("LS(JIS_CARET)", "JIS_CARET"),
        "expect": r"FAIL C01-shift0-release0 .*modifiers == mods",
        "result": "EXPECTED_FAILURE_C01_MISSING_SHIFT",
    },
    "negative-mask-left-only": {
        # Right Shift no longer triggers the morph: the normal binding fires with
        # the physical Right Shift, so the shift1 variant sees the wrong report.
        # Left-Shift-only variants cannot tell this apart (0x02 either way).
        "adaptive": False,
        "replace": ("mods = <(MOD_LSFT | MOD_RSFT)>;", "mods = <MOD_LSFT>;"),
        "expect": r"FAIL C01-shift1-release0 .*modifiers == mods",
        "result": "EXPECTED_FAILURE_C01_RIGHT_SHIFT_NOT_MASKED",
    },
    "negative-suppress": {
        # Shift suppression broken: C02 (`@`) keeps the Shift it must drop, so
        # the JIS host would see Shift+@ (`` ` ``) instead of `@`.
        "adaptive": False,
        "replace": ("N2, JIS_AT)", "N2, LS(JIS_AT))"),
        "expect": r"FAIL C02-shift0-release0 .*modifiers == mods",
        "result": "EXPECTED_FAILURE_C02_SHIFT_NOT_SUPPRESSED",
    },
    "negative-usage-adaptive": {
        "adaptive": True,
        "replace": ("LS(JIS_YEN)", "LS(JIS_RO)"),
        "expect": r"FAIL C16-shift0-release0 .*keys\[i\] == usage",
        "result": "EXPECTED_FAILURE_C16_WRONG_USAGE",
    },
}

# CONFIG_ symbols referenced by the compiled dependency sources/headers that are
# intentionally absent from the harness .config (the unit that reads them is
# replaced or the value has no effect on the code under test). Anything else
# missing fails the run so a new upstream `#if CONFIG_*` cannot silently
# evaluate to 0 here.
KCONFIG_ALLOWED_MISSING = {
    "CONFIG_ZMK_USB",  # endpoints.h; endpoints.c is replaced by the recorder
    "CONFIG_ZMK_BLE",  # endpoints.h; endpoints.c is replaced by the recorder
    "CONFIG_BT_MAX_PAIRED",  # Zephyr BT symbol used only in a ble.h macro; ble.c is not compiled
}
# Pinned files read only at CMake time. ninja -t deps cannot list these (edtlib
# reads every binding under app/dts/bindings), so they are enumerated by hand,
# including the bindings' own `include:` targets.
CMAKE_TIME_DEPENDENCIES = [
    "app/dts/bindings/behaviors/zmk,behavior-mod-morph.yaml",
    "app/dts/bindings/behaviors/zmk,behavior-key-press.yaml",
    "app/dts/bindings/behaviors/zero_param.yaml",
    "app/dts/bindings/behaviors/one_param.yaml",
    "app/include/linker/zmk-events.ld",
]


def pinned_dependency_files(build, usjis=False):
    """Every pinned ZMK file this build compiled, included or read at CMake time,
    as paths relative to the ZMK source tree."""
    deps = subprocess.run(["ninja", "-C", str(build), "-t", "deps"], check=True,
                          capture_output=True, text=True).stdout
    # The DTS preprocessor depfile lists headers app.overlay reads (keys.h,
    # modifiers.h) independently of whether main.c also includes them.
    dts_depfile = build / "zephyr/zephyr.dts.d"
    if not dts_depfile.is_file():
        raise RuntimeError("DTS depfile missing: " + str(dts_depfile)
                           + " (DTS-only header dependencies would go unchecked)")
    deps += dts_depfile.read_text().replace("\\\n", " ").replace(":", " ").replace(" ", "\n")
    files = set()
    for line in deps.splitlines():
        candidate = line.strip()
        if candidate.startswith(ZMK_SOURCE + "/") and Path(candidate).is_file():
            files.add(candidate[len(ZMK_SOURCE) + 1:])
    for source in (ROOT / "tests/simulation/CMakeLists.txt").read_text().splitlines():
        match = re.search(r"\$\{ZMK_SOURCE_DIR\}/(app/[^\s)]+\.(?:c|h|ld|yaml))", source)
        if match:
            files.add(match.group(1))
    files.update(CMAKE_TIME_DEPENDENCIES)
    if usjis:
        files -= MOD_MORPH_ONLY
    else:
        files -= USJIS_ONLY
    return sorted(files)


# Pinned files that only one kind of variant compiles: the Mod-Morph
# characterization variants use the Mod-Morph behavior, the substituter variant
# uses the endpoint-changed event instead. Each kind may leave the other's
# entries unused in the lock.
MOD_MORPH_ONLY = {
    "app/src/behaviors/behavior_mod_morph.c",
    "app/dts/bindings/behaviors/zmk,behavior-mod-morph.yaml",
    "app/include/zmk/events/position_state_changed.h",
    "app/include/zmk/matrix.h",
}
USJIS_ONLY = {
    "app/src/events/endpoint_changed.c",
    "app/include/zmk/events/endpoint_changed.h",
    "app/include/zmk/keymap.h",
}


def check_lock_coverage(files, name, allowed_unused=frozenset()):
    """The lock in dependencies.json must name (and match) every pinned file the
    build actually depended on, so a changed header or binding cannot slip past
    verify_pinned_sources()."""
    problems = []
    for relative in files:
        digest = hashlib.sha256(Path(ZMK_SOURCE, relative).read_bytes()).hexdigest()
        locked = LOCK["zmk_file_sha256"].get(relative)
        if locked is None:
            problems.append(relative + " (not in lock; sha256 " + digest + ")")
        elif locked != digest:
            problems.append(relative + " (lock mismatch)")
    if problems:
        raise RuntimeError(name + ": pinned dependency files not covered by dependencies.json: "
                           + "; ".join(problems))
    stale = sorted(set(LOCK["zmk_file_sha256"]) - set(files) - set(allowed_unused))
    if stale:
        raise RuntimeError(name + ": lock entries no longer used by the build: " + ", ".join(stale))
    return len(files)


def check_kconfig_coverage(build, dependency_files, name):
    """Every CONFIG_ token in the pinned ZMK files that were actually compiled or
    included must be decided by the harness .config (or be explicitly allowed)."""
    files = [ZMK_SOURCE + "/" + relative for relative in dependency_files
             if relative.endswith((".c", ".h"))]
    referenced = set()
    for path in files:
        referenced.update(re.findall(r"\bCONFIG_[A-Z0-9_]+\b", Path(path).read_text(errors="replace")))
    config = (build / "zephyr/.config").read_text()
    decided = set(re.findall(r"^(CONFIG_[A-Z0-9_]+)=", config, re.MULTILINE))
    decided.update(re.findall(r"^# (CONFIG_[A-Z0-9_]+) is not set", config, re.MULTILINE))
    missing = sorted(referenced - decided - KCONFIG_ALLOWED_MISSING)
    if missing:
        raise RuntimeError(name + ": CONFIG_ symbols referenced by pinned sources but not decided "
                           "by the harness .config: " + ", ".join(missing))
    return {"pinned_files_scanned": len(files), "config_symbols_referenced": len(referenced),
            "allowed_missing": sorted(KCONFIG_ALLOWED_MISSING & referenced)}


def verify_pinned_sources(provenance):
    """The source tree about to be built must be the bytes prepare-simulation.py
    exported (every file in provenance.json) and the locked files must match
    dependencies.json, whatever happened on this host in between."""
    mismatched = sorted(
        path for path, digest in LOCK["zmk_file_sha256"].items()
        if hashlib.sha256(Path(ZMK_SOURCE, path).read_bytes()).hexdigest() != digest)
    if mismatched:
        raise RuntimeError("Pinned ZMK files differ from dependencies.json: " + ", ".join(mismatched))
    tree = provenance["zmk_file_sha256"]
    on_disk = {str(p.relative_to(ZMK_SOURCE)) for p in Path(ZMK_SOURCE).rglob("*")
               if p.is_file() and p.name != ".DS_Store"}
    changed = sorted(path for path, digest in tree.items()
                     if not Path(ZMK_SOURCE, path).is_file()
                     or hashlib.sha256(Path(ZMK_SOURCE, path).read_bytes()).hexdigest() != digest)
    extra = sorted(on_disk - set(tree))
    if changed or extra:
        raise RuntimeError("Exported ZMK tree differs from provenance.json; changed=%s extra=%s"
                           % (changed[:10], extra[:10]))


def run_variant(name, adaptive, negative=None, usjis=False):
    build = OUT / name
    # Always build from scratch so the recorded executable hash and build log
    # describe a clean build of the pinned sources, not an incremental one.
    shutil.rmtree(build, ignore_errors=True)
    command = ["cmake", "-S", str(APP), "-B", str(build), "-G", "Ninja",
               "-DBOARD=native_posix_64",
               "-DZMK_SOURCE_DIR=/repo/workspace/simulation/zmk",
               "-DCONFIG_ADAPATIVE_NKRO=" + ("y" if adaptive else "n")]
    if usjis:
        # The substituter variant links this repository's module and its own
        # test program; the Mod-Morph characterization overlay is not used.
        command += ["-DUSJIS_TESTS=y", "-DZEPHYR_MODULES=/repo",
                    "-DDTC_OVERLAY_FILE=" + str(APP / "usjis.overlay")]
    if negative:
        original = (APP / "app.overlay").read_text()
        old, new = negative["replace"]
        if original.count(old) != 1:
            raise RuntimeError("Negative control pattern %r must occur exactly once in app.overlay" % old)
        overlay = OUT / (name + ".overlay")
        overlay.write_text(original.replace(old, new))
        command.append("-DDTC_OVERLAY_FILE=" + str(overlay))
    print("Building " + name, flush=True)
    durations = {}
    started = time.monotonic()
    if logged(command, OUT / (name + "-configure.log"), CONFIGURE_TIMEOUT):
        raise RuntimeError(name + " configure failed")
    durations["configure_s"] = round(time.monotonic() - started, 1)
    started = time.monotonic()
    if logged(["cmake", "--build", str(build), "-j", "4"], OUT / (name + "-build.log"), BUILD_TIMEOUT):
        raise RuntimeError(name + " build failed")
    durations["build_s"] = round(time.monotonic() - started, 1)
    dependency_files = pinned_dependency_files(build, usjis)
    locked_files = check_lock_coverage(dependency_files, name,
                                       MOD_MORPH_ONLY if usjis else USJIS_ONLY)
    kconfig = check_kconfig_coverage(build, dependency_files, name)
    executable = build / "zephyr/zephyr.exe"
    log_path = OUT / (name + "-reports.log")
    started = time.monotonic()
    status = logged([str(executable)], log_path, RUN_TIMEOUT, quiet_failure=bool(negative))
    durations["run_s"] = round(time.monotonic() - started, 1)
    log = log_path.read_text()
    result = {"exit_code": status, "executable_sha256": hashlib.sha256(executable.read_bytes()).hexdigest(),
              "log_sha256": hashlib.sha256(log_path.read_bytes()).hexdigest(),
              "locked_dependency_files": locked_files, "kconfig_coverage": kconfig,
              "durations": durations}
    if negative:
        # A compiler failure or unrelated crash does NOT prove the test detects the bug.
        if status != 1 or not re.search(negative["expect"], log):
            raise RuntimeError(name + " did not fail at the expected assertion: " + negative["expect"])
        result["result"] = negative["result"]
        print("PASS " + name + ": " + negative["result"], flush=True)
    elif usjis:
        summary = re.search(r"SUMMARY usjis table=(\d+) scenarios=(\d+) assertions=(\d+) "
                            r"reports=(\d+) consumer_reports=(\d+)", log)
        if status or "USJIS_SIMULATION_PASS" not in log or summary is None:
            raise RuntimeError(name + " execution failed")
        counts = dict(zip(["table", "scenarios", "assertions", "reports", "consumer_reports"],
                          map(int, summary.groups())))
        # 14 shifted rows x 3 Shift variants x 2 release orders + 6 unshifted rows.
        if counts["table"] != 90:
            raise RuntimeError("Incomplete test run: expected 90 table sequences, got %d" % counts["table"])
        result.update(counts)
        result["result"] = "USJIS_TESTS_PASS"
        print(name + ": " + summary.group(0), flush=True)
    else:
        summary = re.search(r"SUMMARY substitutions=(\d+) shift_first_defect_scenarios=(\d+) "
                            r"release_masked_shift_scenarios=(\d+) known_defects=(\d+) "
                            r"assertions=(\d+) reports=(\d+)", log)
        if status or "SIMULATION_PASS" not in log or summary is None:
            raise RuntimeError(name + " execution failed")
        counts = dict(zip(["substitutions", "shift_first_defect_scenarios",
                           "release_masked_shift_scenarios", "known_defects",
                           "assertions", "reports"], map(int, summary.groups())))
        if counts["substitutions"] != 90:
            raise RuntimeError("Incomplete test run: expected 90 substitutions, got %d" % counts["substitutions"])
        # 14 shifted rows x 3 Shift variants, Shift released first; 3 of those rows
        # (C02, C03, C17) suppress Shift and so have no modifier to lose.
        if counts["shift_first_defect_scenarios"] != 33:
            raise RuntimeError("Expected 33 Shift-first scenarios to exhibit the implicit-Shift loss, got %d"
                               % counts["shift_first_defect_scenarios"])
        # 14 shifted rows x 3 Shift variants, morph released first: the release
        # report always shows the still-held physical Shift as released.
        if counts["release_masked_shift_scenarios"] != 42:
            raise RuntimeError("Expected 42 morph-first scenarios to report Shift released, got %d"
                               % counts["release_masked_shift_scenarios"])
        defects = re.findall(r"^KNOWN_DEFECT (.+)$", log, re.MULTILINE)
        expected = set(EXPECTED_DEFECTS["adaptive" if adaptive else "hkro"])
        # Characterization is pinned to the upstream commit. Report exactly what
        # changed so a fixed or newly observed defect is named, not just counted.
        missing, unexpected = expected - set(defects), set(defects) - expected
        if missing or unexpected or counts["known_defects"] != len(defects):
            raise RuntimeError("Known-defect set changed; missing=%s unexpected=%s"
                               % (sorted(missing), sorted(unexpected)))
        result.update(counts)
        result["defects"] = defects
        result["result"] = "CHARACTERIZATION_PASS_NOT_FIRMWARE_ACCEPTANCE"
        print(name + ": " + summary.group(0), flush=True)
        for defect in result["defects"]:
            print("  KNOWN_DEFECT " + defect, flush=True)
    return result


def main():
    result_path = OUT / "results.json"
    provenance = json.loads((ROOT / "workspace/simulation/provenance.json").read_text())
    try:
        verify_pinned_sources(provenance)
    except Exception as error:
        # Never leave a previous run's results.json behind a failed verification.
        result_path.write_text(json.dumps({"status": "FAILED", "error": str(error)}, indent=2) + "\n")
        raise
    # Remove artefacts of previous runs so the results directory only ever
    # describes this execution. Tool caches (HOME lives here too) are kept.
    for stale in OUT.iterdir():
        if stale.name.startswith("."):
            continue
        shutil.rmtree(stale) if stale.is_dir() else stale.unlink()
    (OUT / "source-provenance.json").write_text(json.dumps(provenance, indent=2) + "\n")
    result = {"dependencies": {k: v for k, v in provenance.items() if k != "zmk_file_sha256"},
              "simulation_image": os.environ.get("SIMULATION_IMAGE"),
              "simulation_image_id": os.environ.get("SIMULATION_IMAGE_ID"),
              "tests": {}}
    result["test_file_sha256"] = {
        str(p.relative_to(ROOT)): hashlib.sha256(p.read_bytes()).hexdigest()
        for p in sorted(APP.rglob("*")) if p.is_file() and "__pycache__" not in p.parts
    }
    result["status"] = "RUNNING"
    result_path.write_text(json.dumps(result, indent=2) + "\n")
    try:
        # SIMULATION_VARIANTS (comma separated) limits a local run to some
        # variants; CI runs all of them.
        selected = os.environ.get("SIMULATION_VARIANTS")
        selected = set(selected.split(",")) if selected else None
        def wanted(name):
            return selected is None or name in selected
        if wanted("hkro"):
            result["tests"]["hkro"] = run_variant("hkro", False)
        if wanted("adaptive"):
            result["tests"]["adaptive"] = run_variant("adaptive", True)
        if wanted("usjis"):
            result["tests"]["usjis"] = run_variant("usjis", False, usjis=True)
        if wanted("usjis-adaptive"):
            # The module on the adaptive NKRO hid.c of the "n" version (0x071a).
            result["tests"]["usjis-adaptive"] = run_variant("usjis-adaptive", True, usjis=True)
        for name, control in NEGATIVE_CONTROLS.items():
            if wanted(name):
                result["tests"][name] = run_variant(name, control["adaptive"], control)
        result["status"] = "CHARACTERIZATION_PASS_NOT_FIRMWARE_ACCEPTANCE"
    except Exception as error:
        result["status"] = "FAILED"
        result["error"] = str(error)
        raise
    finally:
        result_path.write_text(json.dumps(result, indent=2) + "\n")
    print("Simulation complete. Reports and results: build/simulation-results/", flush=True)


if __name__ == "__main__":
    main()
