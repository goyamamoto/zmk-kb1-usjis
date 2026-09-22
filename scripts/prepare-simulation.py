#!/usr/bin/env python3
"""Fetch pinned sources; export ZMK without altering any reference checkout."""
import hashlib
import io
import json
from pathlib import Path
import subprocess
import tarfile

ROOT = Path(__file__).resolve().parents[1]
LOCK = json.loads((ROOT / "tests/simulation/dependencies.json").read_text())
WORK = ROOT / "workspace/simulation"


def git(path, *args):
    return subprocess.check_output(["git", "-C", str(path), *args])


def pinned_repo(path, url, commit):
    if not path.exists():
        path.mkdir(parents=True)
        subprocess.run(["git", "init", str(path)], check=True, stdout=subprocess.DEVNULL)
        subprocess.run(["git", "-C", str(path), "remote", "add", "origin", url], check=True)
    available = subprocess.run(
        ["git", "-C", str(path), "cat-file", "-e", commit + "^{commit}"],
        stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL,
    ).returncode == 0
    if not available:
        subprocess.run(["git", "-C", str(path), "fetch", "--depth=1", "origin", commit], check=True)
    return path


def main():
    if "revision: " + LOCK["zmk_commit"] not in (ROOT / "config/west.yml").read_text():
        raise RuntimeError("Simulation ZMK commit differs from project manifest")
    WORK.mkdir(parents=True, exist_ok=True)
    # Always use a dedicated clone; the research checkout is never touched.
    reference = pinned_repo(WORK / "zmk-git", LOCK["zmk_repository"], LOCK["zmk_commit"])
    # Disable EOL conversion so the archive bytes and hashes do not depend on
    # the host's core.autocrlf setting.
    archive = git(reference, "-c", "core.autocrlf=false", "-c", "core.eol=lf",
                  "archive", LOCK["zmk_commit"])
    exported = WORK / "zmk"
    hashes = {}
    expected_paths = set()
    with tarfile.open(fileobj=io.BytesIO(archive)) as tree:
        for member in tree.getmembers():
            relative = Path(member.name)
            if relative.is_absolute() or ".." in relative.parts:
                raise RuntimeError("Unsafe source archive path")
            if member.isdir():
                continue
            expected_paths.add(relative)
            # Export only regular files: reject unexpected links rather than follow them.
            if not member.isfile():
                raise RuntimeError("Unexpected archive entry: " + member.name)
            data = tree.extractfile(member).read()
            destination = exported / relative
            if destination.exists():
                if destination.is_symlink() or not destination.is_file() or destination.read_bytes() != data:
                    raise RuntimeError(
                        "Modified or stale simulation dependency: "
                        + str(destination)
                        + "; remove workspace/simulation/zmk and retry"
                    )
            else:
                destination.parent.mkdir(parents=True, exist_ok=True)
                destination.write_bytes(data)
                destination.chmod(member.mode)
            hashes[member.name] = hashlib.sha256(data).hexdigest()

    # The files the harness compiles or depends on must match the lock, not
    # just be whatever the local git object store returns for the commit.
    mismatched = sorted(path for path, digest in LOCK["zmk_file_sha256"].items()
                        if hashes.get(path) != digest)
    if mismatched:
        raise RuntimeError("Pinned ZMK files differ from tests/simulation/dependencies.json: "
                           + ", ".join(mismatched))

    # Files added under the export (for example a shadowing header in
    # app/include) are not in the archive and would otherwise go unnoticed.
    if exported.exists():
        extra = sorted(
            str(path.relative_to(exported))
            for path in exported.rglob("*")
            if not path.is_dir() and path.name != ".DS_Store"
            and path.relative_to(exported) not in expected_paths
        )
        if extra:
            raise RuntimeError(
                "Unexpected files in simulation dependency: " + ", ".join(extra[:10])
                + "; remove workspace/simulation/zmk and retry"
            )

    zephyr = pinned_repo(WORK / "zephyr", LOCK["zephyr_repository"], LOCK["zephyr_commit"])
    head = subprocess.run(["git", "-C", str(zephyr), "rev-parse", "--verify", "-q", "HEAD"],
                          capture_output=True, text=True).stdout.strip() or None
    if head is None:
        subprocess.run(["git", "-C", str(zephyr), "checkout", "--detach", LOCK["zephyr_commit"]], check=True)
    elif head != LOCK["zephyr_commit"]:
        raise RuntimeError("Unexpected Zephyr HEAD; use a fresh workspace/simulation/zephyr")
    if git(zephyr, "status", "--porcelain", "--untracked-files=all").strip():
        raise RuntimeError("Zephyr has modified or untracked files")

    # The firmware manifest imports Zephyr by branch name. Warn (do not fail) if
    # that branch no longer points at the pinned commit, so drift between the
    # harness and a fresh `west update` is visible.
    ref = "refs/heads/" + LOCK["zephyr_ref"]
    try:
        remote = subprocess.run(["git", "ls-remote", LOCK["zephyr_repository"], ref],
                                capture_output=True, text=True, timeout=60)
        head = remote.stdout.split()[0] if remote.returncode == 0 and remote.stdout.split() else None
    except (subprocess.TimeoutExpired, OSError):
        head = None
    if head is None:
        print("WARNING: could not resolve %s on %s; drift check skipped" % (ref, LOCK["zephyr_repository"]))
    elif head != LOCK["zephyr_commit"]:
        print("WARNING: %s now resolves to %s but the simulation pins %s (resolved on %s)"
              % (LOCK["zephyr_ref"], head, LOCK["zephyr_commit"], LOCK["zephyr_ref_resolved_on"]))

    provenance = dict(LOCK, zmk_archive_sha256=hashlib.sha256(archive).hexdigest(),
                      zmk_file_sha256=hashes)
    (WORK / "provenance.json").write_text(json.dumps(provenance, indent=2) + "\n")
    print("Pinned simulation sources verified (ZMK and Zephyr; no hardware patch).")


if __name__ == "__main__":
    main()
