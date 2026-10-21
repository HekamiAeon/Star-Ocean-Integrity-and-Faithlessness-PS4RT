# Linux x86-64 release build

This preserves the verified Ubuntu 22.04/glibc 2.35 toolchain stages used
for the game's Linux release candidate. These are developer build images,
not a container requirement for players.

Run `bash packaging/linux/build-images.sh` only if rebuilding the toolchain.
Stages: Ubuntu 22.04 + FFmpeg 8.0.3, SDL3/fmt/Zydis/Vulkan dependencies,
GCC 14 + Boost 1.85, Zycore fix, and finally **PNG-enabled FFmpeg**. The
last image is `localhost/ps4rt-builder:linux` and must be the source for any
new FFmpeg bundle. Earlier stages lack PNG decoder support and caused
missing save thumbnails.

The image recipes use versioned source dependencies, but some base images
and conda package builds remain unpinned. They are not a fully hermetic or
bitwise reproducible compiler pipeline. The previously playtested binaries
and library bundle remain the v1.1 source of truth unless a rebuilt runtime
is separately ABI-audited and gameplay-tested.

## Create both archives after committing reviewed changes

```bash
BUNDLE="/absolute/path/printed/by/build-runtime.sh"
python3 tools/package_star_ocean.py --preview
python3 tools/package_star_ocean.py --runtime --bundle-from "$BUNDLE" --preview
python3 tools/package_star_ocean.py
python3 tools/package_star_ocean.py --runtime --bundle-from "$BUNDLE"
(cd out/community && sha256sum -c *.sha256)
```

The committed source archive and Linux runtime archive keep the original
release naming pattern:

- `star-ocean-5-ps4rt-linux-x86_64-<commit>.tar.gz` (+ `.sha256`)
- `star-ocean-5-ps4rt-source-<commit>.tar.gz` (+ `.sha256`)

Runtime packaging copies **only two verified ELF binaries** and **12
allowlisted shared libraries** from the game-tested folder, along with the
committed source/assets. It does not copy user saves, generated game files,
shaders caches, build logs or extracted game data. No global `LD_LIBRARY_PATH`
necessary. Host still needs glibc >=2.35, Vulkan graphics stack and Python3.

Before publishing: complete `packaging/linux/THIRD_PARTY_REVIEW.md`; test
a freshly extracted final archive and on SteamOS.
