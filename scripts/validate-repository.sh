#!/usr/bin/env bash

set -euo pipefail

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$repo_root"

expected_revision="c284513085c005edf5f9a52b28c8090cc6ed01d2"

required_files=(
  ".gitignore"
  ".github/workflows/simulation.yml"
  ".github/workflows/build.yml"
  "config/keychron_b1_us.keymap"
  "config/keychron_b1_us.conf"
  "config/keychron_b1_usn.keymap"
  "config/keychron_b1_usn.conf"
  "config/keymap-options.h"
  "scripts/build-firmware.sh"
  "README.md"
  "LICENSE"
  "CONTRIBUTING.md"
  "build.yaml"
  "config/west.yml"
  "zephyr/module.yml"
  "CMakeLists.txt"
  "Kconfig"
  "docs/design/usjis-architecture.md"
  "docs/design/usjis-substitution.md"
  "docs/simulation.md"
  "docs/references/b1-special-keys.md"
  "docs/references/licenses.md"
  "scripts/prepare-simulation.py"
  "scripts/run-simulation.sh"
  "tests/simulation/CMakeLists.txt"
  "tests/simulation/Dockerfile"
  "tests/simulation/requirements.txt"
  "tests/simulation/Kconfig"
  "tests/simulation/README.md"
  "tests/simulation/app.overlay"
  "tests/simulation/dependencies.json"
  "tests/simulation/prj.conf"
  "tests/simulation/run.py"
  "tests/simulation/src/main.c"
  "tests/simulation/src/usjis_main.c"
  "tests/simulation/usjis.overlay"
  "src/usjis.c"
  "src/usjis_resolver.c"
  "src/usjis_resolver.h"
  "src/behavior_usjis.c"
  "include/zmk/usjis.h"
  "include/dt-bindings/zmk/usjis.h"
  "dts/bindings/behaviors/zmk,behavior-usjis.yaml"
)

for required_file in "${required_files[@]}"; do
  if [[ ! -f "$required_file" ]]; then
    printf 'Missing required file: %s\n' "$required_file" >&2
    exit 1
  fi
done

git diff --check

grep -Fq "revision: $expected_revision" config/west.yml
python3 - "$expected_revision" <<'PY'
import json
from pathlib import Path
import sys

dependencies = json.loads(Path("tests/simulation/dependencies.json").read_text())
if dependencies.get("zmk_commit") != sys.argv[1]:
  raise SystemExit("Simulation ZMK commit differs from project manifest")
# The firmware build and the simulation must use the same Zephyr commit.
manifest = Path("config/west.yml").read_text()
if "revision: " + dependencies["zephyr_commit"] not in manifest:
  raise SystemExit("Zephyr commit in config/west.yml differs from the simulation pin")
PY
grep -Fq "import: app/west.yml" config/west.yml
grep -Fq "path: config" config/west.yml
grep -Fq "board: keychron" build.yaml
grep -Fq "shield: keychron_b1_us" build.yaml
grep -Fq "name: zmk-feature-usjis" zephyr/module.yml
grep -Fq "/workspace/" .gitignore

# Collect the list first so that a find failure stops the script under set -e.
markdown_files="$(find . -path './workspace' -prune -o -name '*.md' -type f -print)"
broken_links=0
while IFS= read -r markdown_file; do
  [[ -n "$markdown_file" ]] || continue
  while IFS= read -r target; do
    target="${target%%#*}"
    [[ -z "$target" ]] && continue
    link_path="$(dirname "$markdown_file")/$target"
    if [[ ! -e "$link_path" ]]; then
      printf 'Broken Markdown link: %s -> %s\n' "$markdown_file" "$target" >&2
      broken_links=1
    fi
  done < <(grep -oE '\]\([^):]+\.md(#[^)]*)?\)' "$markdown_file" | sed -E 's/^\]\(([^)#]+)(#[^)]*)?\)$/\1/' || true)
done <<< "$markdown_files"

if [[ "$broken_links" -ne 0 ]]; then
  exit 1
fi

# English .md files are canonical; every Japanese translation (.ja.md) must
# sit next to its English counterpart.
# Collect the list first so that a find failure stops the script under set -e.
translations="$(find . -path './workspace' -prune -o -name '*.ja.md' -type f -print)"
missing_english=0
while IFS= read -r translation; do
  [[ -n "$translation" ]] || continue
  english="${translation%.ja.md}.md"
  if [[ ! -f "$english" ]]; then
    printf 'Japanese translation without English counterpart: %s\n' "$translation" >&2
    missing_english=1
  fi
done <<< "$translations"

if [[ "$missing_english" -ne 0 ]]; then
  exit 1
fi

expected_substitution_ids="$(printf 'C%02d\n' {1..20})"
actual_substitution_ids="$(sed -nE 's/^\| (C[0-9]{2}) \|.*/\1/p' docs/design/usjis-substitution.md)"
if [[ "$actual_substitution_ids" != "$expected_substitution_ids" ]]; then
  printf 'Substitution table must contain C01 through C20 exactly once and in order.\n' >&2
  exit 1
fi

expected_state_ids="$(printf 'S%02d\n' {1..20})"
actual_state_ids="$(sed -nE 's/^\| (S[0-9]{2}) \|.*/\1/p' docs/design/usjis-substitution.md)"
if [[ "$actual_state_ids" != "$expected_state_ids" ]]; then
  printf 'State test table must contain S01 through S20 exactly once and in order.\n' >&2
  exit 1
fi

printf 'Repository validation passed.\n'
