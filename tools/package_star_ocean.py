#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""Build audited source and Linux x86-64 runtime tarballs from committed Git files.

The runtime requires the tested binary bundle. Never include game content,
saves, linked game executables or shader caches from any build directory.
"""
import argparse
import hashlib
import io
import json
import os
import re
import shutil
from pathlib import Path, PurePosixPath
import subprocess
import tarfile

ROOT = Path(__file__).resolve().parent.parent
BINARIES = {"out/ps4rt": ROOT / "out/ps4rt",
            "out/gpu/libps4rtgpu.so": ROOT / "out/gpu/libps4rtgpu.so"}
LINUX_LIBRARIES = (
    "libavcodec.so.62", "libavformat.so.62", "libavutil.so.60",
    "libfmt.so.11", "libgcc_s.so.1", "libSDL3.so.0",
    "libstdc++.so.6", "libswresample.so.6", "libswscale.so.9",
    "libxxhash.so.0", "libz.so.1", "libZydis.so.5.0",
)
EXPECTED_RUNPATH = {
    "out/ps4rt": "$ORIGIN/../lib:$ORIGIN/gpu",
    "out/gpu/libps4rtgpu.so": "$ORIGIN/../../lib",
}

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
        "packaging/fsr4-v07-assets.json", "docs/LINUX_BINARY_RELEASE.md",
        "packaging/linux/README.md", "packaging/linux/THIRD_PARTY_REVIEW.md",
        "packaging/linux/Containerfile.ffmpeg-png",
        "packaging/linux/Play-Game.sh",
        "packaging/linux/Play-Star-Ocean.sh",
        "packaging/linux/PLAYER_README.txt",
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


def verify_bundle(bundle):
    """Ensure every shipped ELF uses only the audited relative library layout."""
    for program in ("patchelf", "readelf", "ldd"):
        if not shutil.which(program):
            raise SystemExit("Install required audit tool: " + program)
    names = {**EXPECTED_RUNPATH, **{"lib/" + n: "$ORIGIN" for n in LINUX_LIBRARIES}}
    files = {}
    for name, expected_rpath in names.items():
        path = bundle / name
        host_elf(path)
        actual = subprocess.check_output(["patchelf", "--print-rpath", str(path)], text=True).strip()
        if actual != expected_rpath:
            raise SystemExit("Unexpected RUNPATH for " + name + ": " + repr(actual))
        versions = subprocess.check_output(["readelf", "-W", "--version-info", str(path)], text=True)
        maxima = [tuple(map(int, v.split('.'))) for v in re.findall(r"GLIBC_([0-9]+(?:\.[0-9]+)+)", versions)]
        if maxima and max(maxima) > (2, 35):
            raise SystemExit("GLIBC requirement exceeds 2.35: " + name)
        files[name] = path
    env = os.environ.copy()
    env.pop("LD_LIBRARY_PATH", None)
    env.pop("LD_PRELOAD", None)
    for name in EXPECTED_RUNPATH:
        run = subprocess.run(["ldd", str(files[name])], capture_output=True, text=True, env=env)
        output = run.stdout + run.stderr
        if run.returncode or "not found" in output:
            raise SystemExit("Missing dependency in " + name + ":\n" + output)
        for line in output.splitlines():
            m = re.match(r"\s*(\S+)\s+=>\s+(/\S+)\s+\(", line)
            if m and m.group(1) in LINUX_LIBRARIES:
                if Path(m.group(2)).resolve() != (bundle / "lib" / m.group(1)).resolve():
                    raise SystemExit("Dependency escapes the bundle: " + line)
    return files


# Required legal notices remain beside the portable runtime. The matching,
# complete source archive (including docs/) is published separately.
PLAYER_LICENSE_FILES = {
    "gpu/shadps4/LICENSE",
    "gpu/third_party/fonts/LICENSE-DejaVu.txt",
    "gpu/third_party/fsr-vulkan/LICENSE.txt",
    "gpu/third_party/fsr-vulkan/NOTICE.md",
    "gpu/third_party/half/LICENSE.txt",
    "gpu/third_party/imgui/LICENSE.txt",
    "gpu/third_party/magic_enum/LICENSE",
    "gpu/third_party/miniz/LICENSE",
    "gpu/third_party/sirit/LICENSE.txt",
    "gpu/third_party/xbyak/COPYRIGHT",
    "third_party/LibAtrac9/LICENSE",
}


def runtime_entries(source, modes):
    """Player archive: runnable code, assets and legal notices; no docs/source tree."""
    package = {}
    root_files = {
        "run.sh", "LICENSE", "packaging/fsr4-v07-assets.json",
    } | PLAYER_LICENSE_FILES
    for name, data in source.items():
        if (name in root_files or name.startswith((
            "scripts/", "fsr4_shaders/", "gpu/assets/",
            "gpu/third_party/fsr-vulkan/generated/"
        ))):
            package[name] = (data, modes[name])
    # Keep upstream attribution available offline without a docs/ folder.
    if "docs/UPSTREAM_CREDITS.md" in source:
        package["CREDITS.md"] = (source["docs/UPSTREAM_CREDITS.md"], 0o644)
    # One tracked player README and launcher implementation for both the
    # tested build bundle and final release. Keep the historical launcher alias.
    package["PLAYER_README.txt"] = (
        source["packaging/linux/PLAYER_README.txt"], 0o644)
    package["Play-Game.sh"] = (
        source["packaging/linux/Play-Game.sh"], 0o755)
    package["Play-Star-Ocean.sh"] = (
        source["packaging/linux/Play-Star-Ocean.sh"], 0o755)
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
    parser.add_argument("--bundle-from", type=Path,
                        help="Existing tested Linux bundle with out/ and lib/; required for --runtime")
    parser.add_argument("--preview", action="store_true",
                        help="Print manifest file list without writing archives")
    parser.add_argument("--output", type=Path, default=ROOT / "out/community")
    args = parser.parse_args()
    if args.runtime != (args.bundle_from is not None):
        raise SystemExit("Runtime packages require --runtime --bundle-from <audited-bundle>; "
                         "omit both for source packages")
    revision, source, modes = committed_source()
    verify_release_files(source)
    if args.runtime:
        bundle = args.bundle_from.expanduser().resolve()
        # Never attach binaries from another commit to a manifest for HEAD.
        provenance = bundle.parent / "source-revision.txt"
        if not provenance.is_file() or provenance.read_text().strip() != revision:
            raise SystemExit("Bundle revision does not match committed source. "
                             "Rebuild with packaging/linux/build-runtime.sh.")
        # The build must contain exactly the reviewed source's overlay media.
        for name in ("gpu/assets/star_ocean_overlay.png",
                     "gpu/assets/sounds/navigate.wav",
                     "gpu/assets/sounds/confirm.wav",
                     "gpu/assets/sounds/cancel.wav"):
            path = bundle / name
            if not path.is_file() or path.read_bytes() != source[name]:
                raise SystemExit("Missing or mismatched build overlay asset: " + name)
        files = verify_bundle(bundle)
        BINARIES.clear()
        BINARIES.update({name: files[name] for name in EXPECTED_RUNPATH})
    kind = "linux-x86_64" if args.runtime else "source"
    name = f"star-ocean-5-ps4rt-{kind}-{revision[:7]}"
    items = (runtime_entries(source, modes) if args.runtime else
             {path: (content, modes[path]) for path, content in source.items()})
    if args.runtime:
        for library_name in LINUX_LIBRARIES:
            items["lib/" + library_name] = (host_elf(files["lib/" + library_name]), 0o644)
    if args.preview:
        print(json.dumps({"revision": revision, "kind": kind,
                          "paths": sorted(items)}, indent=2))
        return
    args.output.mkdir(parents=True, exist_ok=True)
    export(args.output / (name + ".tar.gz"), name, revision, kind, items)


if __name__ == "__main__":
    main()
