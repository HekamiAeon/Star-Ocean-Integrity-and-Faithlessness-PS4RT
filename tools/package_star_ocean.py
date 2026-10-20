#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""Build Star Ocean 5 PS4RT v1.0 source/runtime tarballs from reviewed Git files.

Only the host ELF executables are read from the local build output. Never copy
any other contents of out/, saves, PS4 executables, or shader caches.
"""
import argparse
import hashlib
import io
import json
from pathlib import Path, PurePosixPath
import subprocess
import tarfile

ROOT = Path(__file__).resolve().parent.parent
BINARIES = {"out/ps4rt": ROOT / "out/ps4rt",
            "out/gpu/libps4rtgpu.so": ROOT / "out/gpu/libps4rtgpu.so"}
PRIVATE_COMPONENTS = {
    ".git", ".agents", ".codex", ".ghidra", "__pycache__", "original",
    "references", "Reference", "analysis", "savedata", "savedata-backups",
    "saves", "user", "cache", "caches", "captures", "screenshots",
    "frame-dump", "CMakeFiles",
}
PRIVATE_NAMES = {"AGENTS.md", "CLAUDE.md", "LAB_README.md",
                 "PORTING_LOG.md", "AUTOMATED_PORTING.md"}
PRIVATE_SUFFIXES = {".pkg", ".prx", ".sprx", ".self", ".elf", ".sfo",
                    ".cpk", ".usm", ".g1t", ".g1m", ".at9", ".pfs",
                    ".iso", ".o", ".obj", ".a", ".so", ".dll",
                    ".exe", ".log", ".cache"}
PROHIBITED_PREFIXES = ("out/", "build/", "dist/", "templates/")


def git(*args):
    return subprocess.check_output(["git", "-C", str(ROOT), *args])


def checksum(data):
    return hashlib.sha256(data).hexdigest()


def committed_source():
    # Source files must match the exact HEAD revision, not uncommitted work.
    if subprocess.run(["git", "-C", str(ROOT), "diff", "--quiet", "HEAD"],
                      check=False).returncode:
        raise SystemExit("Commit tracked changes before packaging.")
    if subprocess.run(["git", "-C", str(ROOT), "diff", "--cached", "--quiet"],
                      check=False).returncode:
        raise SystemExit("Commit staged changes before packaging.")
    revision = git("rev-parse", "HEAD").decode().strip()
    modes = {}
    for entry in git("ls-tree", "-r", "-z", "HEAD").split(b"\0"):
        if not entry:
            continue
        meta, rawpath = entry.split(b"\t", 1)
        mode, kind, _ = meta.decode().split()
        path = rawpath.decode()
        parts = PurePosixPath(path).parts
        if kind != "blob" or mode not in ("100644", "100755"):
            raise SystemExit("Refusing symlink or submodule: " + path)
        if (not parts or ".." in parts or
            any(p in PRIVATE_COMPONENTS for p in parts) or
            path.startswith(PROHIBITED_PREFIXES) or
            parts[-1] in PRIVATE_NAMES or
            parts[-1].startswith(("PORTING_LOG", "NEXT_SESSION", "RESUME")) or
            PurePosixPath(path).suffix.lower() in PRIVATE_SUFFIXES):
            raise SystemExit("Forbidden tracked source file: " + path)
        modes[path] = int(mode, 8)
    source = {}
    with tarfile.open(fileobj=io.BytesIO(git("archive", "--format=tar", "HEAD")),
                      mode="r:") as archive:
        for entry in archive:
            if entry.isdir():
                continue
            if not entry.isfile():
                raise SystemExit("Non-regular committed file: " + entry.name)
            source[entry.name] = archive.extractfile(entry).read()
    if not source or set(source) != set(modes):
        raise SystemExit("Git tree and archive file lists differ.")
    return revision, source, modes


def verify_release_files(source):
    required = {
        "README.md", "LICENSE", "run.sh", "scripts/ps4rt_launch.py",
        "scripts/prepare.py", "scripts/link_modules.py", "scripts/link_libc.py",
        "scripts/content_profile.py", "gpu/assets/star_ocean_overlay.png",
        "gpu/assets/sounds/navigate.wav", "gpu/assets/sounds/confirm.wav",
        "gpu/assets/sounds/cancel.wav", "docs/RUNNING_LINUX.md",
        "docs/DISTRIBUTING_LINUX.md", "docs/UPSTREAM_CREDITS.md",
        "docs/images/readme/sample.png",
        "packaging/fsr4-v07-assets.json",
    }
    for name in sorted(required - set(source)):
        raise SystemExit("Missing required release file: " + name)
    lock = json.loads(source["packaging/fsr4-v07-assets.json"])
    if len(lock["files"]) != 189:
        raise SystemExit("Unexpected FSR4 asset list.")
    for name, metadata in lock["files"].items():
        data = source.get(name)
        if data is None or len(data) != metadata["size"] or checksum(data) != metadata["sha256"]:
            raise SystemExit("FSR4 checksum/size mismatch: " + name)


def host_elf(path):
    if path.is_symlink() or not path.is_file():
        raise SystemExit("Missing host binary; run build.sh: " + str(path))
    data = path.read_bytes()
    if (len(data) < 64 or data[:4] != b"\x7fELF" or
        data[4:6] != bytes([2, 1]) or
        int.from_bytes(data[18:20], "little") != 62):
        raise SystemExit("Not a 64-bit x86-64 ELF host binary: " + str(path))
    return data


def runtime_entries(source, modes):
    # GPL corresponding source is retained under source/. Only the required
    # host binaries, shaders, media and Python launcher are at the package root.
    package = {"source/" + name: (data, modes[name]) for name, data in source.items()}
    root_files = {
        "run.sh", "README.md", "LICENSE", "packaging/fsr4-v07-assets.json",
        "docs/RUNNING_LINUX.md", "docs/DISTRIBUTING_LINUX.md",
        "docs/UPSTREAM_CREDITS.md",
    }
    for name, data in source.items():
        if (name in root_files or name.startswith((
            "scripts/", "fsr4_shaders/", "gpu/assets/",
            "gpu/third_party/fsr-vulkan/generated/"
        ))):
            package[name] = (data, modes[name])
    launcher = (
        '#!/usr/bin/env bash\nset -euo pipefail\n'
        'cd -- "$(dirname -- "$0")"\n'
        'exec bash run.sh "$@"\n'
    ).encode()
    package["Play-Star-Ocean.sh"] = (launcher, 0o755)
    for name, path in BINARIES.items():
        package[name] = (host_elf(path), 0o755)
    return package


def export(target, dirname, revision, kind, items):
    if target.exists() or target.is_symlink():
        raise SystemExit("Refusing to overwrite existing file: " + str(target))
    manifest = {
        "revision": revision,
        "kind": kind,
        "files": {name: checksum(data) for name, (data, _) in sorted(items.items())},
        "notice": "No original PS4 game files, saves or generated GPU caches included.",
    }
    payload = dict(items)
    payload["MANIFEST.json"] = (
        (json.dumps(manifest, indent=2, sort_keys=True) + "\n").encode(), 0o644
    )
    staging = target.with_name(target.name + ".part")
    if staging.exists() or staging.is_symlink():
        raise SystemExit("Refusing existing staging output: " + str(staging))
    with tarfile.open(staging, "w:gz") as tar:
        for name, (data, mode) in sorted(payload.items()):
            member = PurePosixPath(name)
            if member.is_absolute() or ".." in member.parts:
                raise SystemExit("Unsafe archive path: " + name)
            info = tarfile.TarInfo(dirname + "/" + name)
            info.uid = info.gid = 0
            info.uname = info.gname = ""
            info.mode = mode
            info.size = len(data)
            tar.addfile(info, io.BytesIO(data))
    staging.rename(target)
    sha = target.with_name(target.name + ".sha256")
    sha.write_text(checksum(target.read_bytes()) + "  " + target.name + "\n")
    print(f"Created {target} ({target.stat().st_size:,} bytes)")
    print(f"Created {sha}")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--runtime", action="store_true",
                        help="Package locally compiled Linux runtime, with source")
    parser.add_argument("--preview", action="store_true",
                        help="Print manifest file list without writing archives")
    parser.add_argument("--output", type=Path, default=ROOT / "out/community")
    args = parser.parse_args()
    revision, source, modes = committed_source()
    verify_release_files(source)
    kind = "linux-x86_64" if args.runtime else "source"
    name = f"star-ocean-5-ps4rt-{kind}-{revision[:7]}"
    items = (runtime_entries(source, modes) if args.runtime else
             {path: (content, modes[path]) for path, content in source.items()})
    if args.preview:
        print(json.dumps({"revision": revision, "kind": kind,
                          "paths": sorted(items)}, indent=2))
        return
    args.output.mkdir(parents=True, exist_ok=True)
    export(args.output / (name + ".tar.gz"), name, revision, kind, items)


if __name__ == "__main__":
    main()
