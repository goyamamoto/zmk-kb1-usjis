#!/usr/bin/env bash
set -euo pipefail
repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$repo_root"
case "$repo_root" in
  *,*) echo "Repository path must not contain a comma (docker --mount syntax)" >&2; exit 1 ;;
esac
python3 scripts/prepare-simulation.py
sim_platform="$(python3 -c 'import json; print(json.load(open("tests/simulation/dependencies.json"))["platform"])')"
sim_base="$(python3 -c 'import json; print(json.load(open("tests/simulation/dependencies.json"))["container_base"])')"
grep -Fq "FROM $sim_base" tests/simulation/Dockerfile || {
  echo "tests/simulation/Dockerfile base image differs from dependencies.json" >&2; exit 1; }

# The simulation image is built from the pinned Dockerfile and tagged by the
# hash of Dockerfile + requirements.txt, so a change always yields a new image.
# Building needs the network once; the simulation run below has none.
dockerfile_sha="$(python3 -c 'import hashlib; h=hashlib.sha256(); [h.update(open(f,"rb").read()) for f in ("tests/simulation/Dockerfile","tests/simulation/requirements.txt")]; print(h.hexdigest()[:16])')"
sim_image="zmk-usjis-simulation:$dockerfile_sha"
if ! docker image inspect "$sim_image" >/dev/null 2>&1; then
  docker build --platform "$sim_platform" -t "$sim_image" -f tests/simulation/Dockerfile tests/simulation
fi
# The image ID (content digest of the built image) is recorded in results.json,
# so two runs can be compared on what toolchain actually ran, not just the tag.
sim_image_id="$(docker image inspect --format '{{.Id}}' "$sim_image")"
mkdir -p build/simulation-results

# No device forwarding, privileged mode, USB/IP or network inside the container.
# Sources are read-only; the only writable host mount contains build/test results.
# Run as the invoking user so results are not root-owned on Linux hosts; point
# HOME and the cache at the writable mount because the image's home is root's.
docker run --rm --platform "$sim_platform" --network none \
    --user "$(id -u):$(id -g)" \
    --mount "type=bind,source=$repo_root,target=/repo,readonly" \
    --mount "type=bind,source=$repo_root/build/simulation-results,target=/out" \
    --workdir /repo \
    --env HOME=/out \
    --env XDG_CACHE_HOME=/out/.cache \
    --env SIMULATION_IMAGE="$sim_image" \
    --env SIMULATION_IMAGE_ID="$sim_image_id" \
    --env SIMULATION_VARIANTS="${SIMULATION_VARIANTS:-}" \
    --env ZEPHYR_BASE=/repo/workspace/simulation/zephyr \
    --env ZEPHYR_TOOLCHAIN_VARIANT=host \
    "$sim_image" python3 /repo/tests/simulation/run.py
