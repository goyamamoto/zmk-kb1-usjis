#!/usr/bin/env bash
# Reproducible firmware build for the Keychron B1 Pro US.
#
# Runs inside the pinned zmkfirmware/zmk-build-arm image (Zephyr SDK 0.15.2,
# west, cmake, ninja). The west workspace lives in workspace/firmware/ (not
# tracked by Git), this repository is mounted into it as `config`, and the
# outputs land in build/firmware/.
#
#   bash scripts/build-firmware.sh            # prepare (network) + build (offline)
#   bash scripts/build-firmware.sh prepare    # fetch pinned sources, apply the Zephyr patch
#   bash scripts/build-firmware.sh build      # build from the prepared workspace, no network
#   SHIELD=keychron_b1_usn bash scripts/build-firmware.sh build   # "n" version, untested
#
# The pins: config/west.yml (ZMK and Zephyr commits; Zephyr's own manifest
# pins its modules), the Zephyr patch shipped in the pinned ZMK commit, and
# the image digest below. build/firmware/build-info.json records what was
# actually used.
set -euo pipefail

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$repo_root"
case "$repo_root" in
  *,*) echo "Repository path must not contain a comma (docker --mount syntax)" >&2; exit 1 ;;
esac

# zmkfirmware/zmk-build-arm:3.2, linux/amd64 manifest, resolved on 2026-09-22.
image_platform="linux/amd64"
image_ref="zmkfirmware/zmk-build-arm:3.2@sha256:ca704b1fe68535b9e426aa6c8a800dcec5a4c00576aaf593fbabea0146897a73"

board="keychron"
# SHIELD=keychron_b1_usn selects the "n" version (PID 0x071a) build, which is
# not verified on hardware. The artifact name follows the shield.
shield="${SHIELD:-keychron_b1_us}"
case "$shield" in
  keychron_b1_us)  artifact="keychron-b1-us-usjis" ;;
  keychron_b1_usn) artifact="keychron-b1-usn-usjis" ;;
  *) echo "unsupported SHIELD=$shield (keychron_b1_us or keychron_b1_usn)" >&2; exit 2 ;;
esac

mode="${1:-all}"
case "$mode" in
  all|prepare|build|in-container-prepare|in-container-build) ;;
  *) echo "usage: $0 [all|prepare|build]" >&2; exit 2 ;;
esac

workspace="$repo_root/workspace/firmware"
out_dir="$repo_root/build/firmware"

if [[ "$mode" != in-container-* ]]; then
  mkdir -p "$workspace/config" "$out_dir"

  # Version string embedded in the firmware (Zephyr BUILD_VERSION, printed in
  # the boot banner and stored in version.h) and recorded in build-info.json.
  repo_commit="$(git rev-parse HEAD)"
  # Keychron ZMK embeds __DATE__ and __TIME__ (tdfu.c, launcher.c). GCC takes
  # them from SOURCE_DATE_EPOCH when it is set, so the build is reproducible
  # and the Launcher reports the commit time as the build time.
  # BUILD_VERSION and SOURCE_DATE_EPOCH from the environment override the
  # values derived from git, to compare builds of different commits byte for byte.
  source_date_epoch="${SOURCE_DATE_EPOCH:-$(git log -1 --format=%ct HEAD)}"
  if [[ -n "$(git status --porcelain --untracked-files=no)" ]]; then
    repo_dirty=true
    build_version="${BUILD_VERSION:-usjis-$(git rev-parse --short HEAD)-dirty}"
  else
    repo_dirty=false
    build_version="${BUILD_VERSION:-usjis-$(git rev-parse --short HEAD)}"
  fi

  run_in_container() {
    local network="$1" step="$2"
    # The repository is mounted read-only as the workspace's manifest repository
    # (config); the workspace and the output directory are the only writable
    # mounts. Run as the invoking user so nothing on the host becomes root-owned.
    docker run --rm --platform "$image_platform" --network "$network" \
      --user "$(id -u):$(id -g)" \
      --mount "type=bind,source=$workspace,target=/workspace" \
      --mount "type=bind,source=$repo_root,target=/workspace/config,readonly" \
      --mount "type=bind,source=$out_dir,target=/out" \
      --workdir /workspace \
      --env HOME=/workspace/.home \
      --env BUILD_VERSION="$build_version" \
      --env SOURCE_DATE_EPOCH="$source_date_epoch" \
      --env REPO_COMMIT="$repo_commit" \
      --env REPO_DIRTY="$repo_dirty" \
      --env IMAGE_REF="$image_ref" \
      --env BOARD="$board" --env SHIELD="$shield" --env ARTIFACT="$artifact" \
      "$image_ref" bash /workspace/config/scripts/build-firmware.sh "in-container-$step"
  }

  case "$mode" in
    all)     run_in_container bridge prepare; run_in_container none build ;;
    prepare) run_in_container bridge prepare ;;
    build)   run_in_container none build ;;
  esac
  exit 0
fi

# ---------------------------------------------------------------- container
step="${mode#in-container-}"
mkdir -p "$HOME"
# The mounts are owned by the host user, which git treats as dubious ownership
# when the container's user id differs.
git config --global --add safe.directory '*'
cd /workspace

patch_file="/workspace/zmk/0001-esb-nrf-fix.patch"

zephyr_patched() {
  # The patch is applied as a commit on top of the pinned Zephyr commit.
  [[ -d zephyr ]] \
    && git -C zephyr apply --check --reverse "$patch_file" >/dev/null 2>&1
}

if [[ "$step" == prepare ]]; then
  if [[ ! -d .west ]]; then
    west init -l config --mf config/west.yml
  fi
  west update --narrow --fetch-opt=--depth=1
  if ! zephyr_patched; then
    git -C zephyr apply --check "$patch_file"
    git -C zephyr -c user.name=zmk-usjis -c user.email=build@invalid \
      am --committer-date-is-author-date "$patch_file"
  fi
  west list -f '{name} {path} {sha}' > /workspace/.west-list
  exit 0
fi

# ---- build (offline)
[[ -d .west ]] || { echo "workspace not prepared; run: bash scripts/build-firmware.sh prepare" >&2; exit 1; }
zephyr_patched || { echo "Zephyr patch is not applied; run prepare again" >&2; exit 1; }
# The checked-out sources must be the pinned ones.
west list -f '{name} {path} {sha}' > /workspace/.west-list
expected_zmk="$(sed -nE 's/^ *revision: ([0-9a-f]{40})$/\1/p' config/config/west.yml | sed -n 1p)"
expected_zephyr="$(sed -nE 's/^ *revision: ([0-9a-f]{40})$/\1/p' config/config/west.yml | sed -n 2p)"
grep -q "^zmk zmk $expected_zmk$" /workspace/.west-list || { echo "zmk checkout is not at the pinned commit" >&2; exit 1; }
[[ "$(git -C zephyr rev-parse HEAD~1)" == "$expected_zephyr" ]] || { echo "zephyr checkout is not the pinned commit plus the patch" >&2; exit 1; }

build_dir="/workspace/build/$shield"
rm -rf "$build_dir"
# Register the Zephyr CMake package in the container user's home (which is
# inside the workspace) so that find_package(Zephyr) resolves.
export ZEPHYR_BASE=/workspace/zephyr
west zephyr-export >/dev/null
west build -s zmk/app -d "$build_dir" -b "$BOARD" -- \
  -DSHIELD="$SHIELD" \
  -DZMK_CONFIG=/workspace/config/config \
  -DBUILD_VERSION="$BUILD_VERSION" \
  2>&1 | tee /out/build.log

# ---- collect outputs and the record of what was built
rm -f /out/"$ARTIFACT".uf2 /out/"$ARTIFACT".hex /out/"$ARTIFACT".elf /out/"$ARTIFACT".map
cp "$build_dir/zephyr/zmk.uf2" /out/"$ARTIFACT".uf2
cp "$build_dir/zephyr/zmk.hex" /out/"$ARTIFACT".hex
cp "$build_dir/zephyr/zmk.elf" /out/"$ARTIFACT".elf
cp "$build_dir/zephyr/zmk.map" /out/"$ARTIFACT".map
cp "$build_dir/zephyr/.config" /out/zephyr.config
cp "$build_dir/zephyr/zephyr.dts" /out/zephyr.dts
cp "$build_dir/zephyr_modules.txt" /out/zephyr_modules.txt
python3 - > /out/python-packages.txt <<'PY'
import importlib.metadata as m
print("\n".join(sorted({f"{d.metadata['Name']}=={d.version}" for d in m.distributions()}, key=str.lower)))
PY

python3 - "$build_dir" <<'PY'
import hashlib, json, os, re, subprocess, sys
build_dir = sys.argv[1]
def sha(p): return hashlib.sha256(open(p, "rb").read()).hexdigest()
log = open("/out/build.log", encoding="utf-8", errors="replace").read()
def find(pattern):
    m = re.search(pattern, log, re.M); return m.group(1).strip() if m else None
projects = [l.split() for l in open("/workspace/.west-list").read().splitlines() if l.strip()]
art = os.environ["ARTIFACT"]
info = {
    "repository_commit": os.environ["REPO_COMMIT"],
    "repository_dirty": os.environ["REPO_DIRTY"] == "true",
    "build_version": os.environ["BUILD_VERSION"],
    "source_date_epoch": int(os.environ["SOURCE_DATE_EPOCH"]),
    "board": os.environ["BOARD"],
    "shield": os.environ["SHIELD"],
    "zmk_config": "/workspace/config/config",
    "keymap_file": find(r"^-- Using keymap file: (.*)$"),
    "config_kconfig_files": re.findall(r"^-- ZMK Config Kconfig: (.*)$", log, re.M),
    "west_projects": [{"name": n, "path": p, "sha": s} for n, p, s in projects],
    "zephyr_head_with_patch": subprocess.check_output(["git", "-C", "/workspace/zephyr", "rev-parse", "HEAD"]).decode().strip(),
    "zephyr_patch": {"file": "zmk/0001-esb-nrf-fix.patch", "sha256": sha("/workspace/zmk/0001-esb-nrf-fix.patch")},
    "container_image": os.environ["IMAGE_REF"],
    "zephyr_sdk_version": open("/opt/zephyr-sdk-0.15.2/sdk_version").read().strip() if os.path.exists("/opt/zephyr-sdk-0.15.2/sdk_version") else None,
    "west_version": subprocess.check_output(["west", "--version"]).decode().split()[-1],
    "cmake_version": subprocess.check_output(["cmake", "--version"]).decode().split()[2],
    "compiler": subprocess.check_output([re.search(r"^CMAKE_C_COMPILER:\w+=(.*)$", open(build_dir + "/CMakeCache.txt").read(), re.M).group(1), "--version"]).decode().splitlines()[0],
    "zephyr_modules": [l.split('":"')[0].strip('"') for l in open("/out/zephyr_modules.txt").read().splitlines() if l.strip()],
    "python_packages": open("/out/python-packages.txt").read().split(),
    "artifacts": {f: sha("/out/" + f) for f in (art + ".uf2", art + ".hex", art + ".elf")},
}
json.dump(info, open("/out/build-info.json", "w"), indent=2)
print(json.dumps({k: info[k] for k in ("build_version", "keymap_file", "config_kconfig_files", "zephyr_sdk_version", "artifacts")}, indent=2))
PY
