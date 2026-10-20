# Star Ocean: Integrity and Faithlessness — Linux distribution guide

This document explains how to build and distribute the **Star Ocean: Integrity and Faithlessness PS4RT** Linux source release for **CUSA03219**. It is an experimental, game-specific native x86-64 compatibility runtime, adapted from earlier PS4RT projects and the shadPS4 rendering foundation. Original copyright, GPL, and third-party license notices must remain intact.

**This repository contains the runtime and its licensed dependencies, not the game, its firmware, or decryption tools.** Each player supplies an independently obtained, extracted, plaintext copy of the game.

## Player instructions

**This v1.0 repository is a source checkout, not a downloadable prebuilt runtime archive.** Clone or download the source and follow the [Running on Linux guide](RUNNING_LINUX.md) to install prerequisites, build, and start the game.

From the source root:

~~~bash
PS4RT_LTO=OFF bash build.sh --test

bash run.sh \
  --game /path/to/your/CUSA03219 \
  --upscaler fsr4 \
  --output-resolution 4k
~~~

`--upscaler fsr4` and `--output-resolution 4k` are optional; the launcher also supports `fsr3` or `off` and 1080p or 1440p outputs. A GUI game-folder chooser is **not** implemented, and `--game` is required. The source release has no `Play-Valkyria.sh`-style packaged launcher or `--data-dir` argument.

The supplied directory must include `sce_sys/param.sfo`, a plaintext `eboot.bin`, compatible modules under `sce_module/`, and the required game data. The launcher does not decrypt PKGs, fetch game content, or access PSN.

### Host requirements

**Linux x86-64, tested on Fedora.** Building uses a C/C++ toolchain, Python 3, CMake, Ninja, pkg-config, a compatible Vulkan loader/driver and development libraries for SDL3 and FFmpeg, plus the libraries required by the project's [README](../README.md) and `gpu/CMakeLists.txt` (including fmt, xxhash, Boost, robin-map, Vulkan Memory Allocator, Zydis, glslang, and X11).

The build also uses vendored third-party sources where included. For troubleshooting missing key pkg-config modules:

~~~bash
pkg-config --modversion vulkan sdl3 libavcodec libavutil libswscale
~~~

Other distributions may work with compatible libraries, but have not been verified. The program is compiled and linked against the local host's libraries; **a build made on Fedora is not guaranteed to run unchanged on an older distribution**. Do not assume a universal glibc minimum from a prior Valkyria build. A relocatable binary archive or AppImage would need separate build-baseline and dependency testing.

### Graphics, controls, and gameplay status

Open the illustrated in-game menu with **Insert**, or press **L2 + R3 click**. Navigate with D-pad, Cross, and Circle (A/B for typical Xbox-style controllers).

The compact menu offers 1080p, 1440p, or 4K output; Off / Original, FSR 3.1, or FSR 4 v07; display mode, sharpening, and anisotropic filtering (Game default / 2× / 4× / 8× / 16×). AF requires a restart; resolution, upscaling, sharpening, and shader compilation mode apply live.

**Shader compilation is a player choice:** **Sync (default)** may stutter during first-use compilation but reduces transient rendering corruption; **Async** reduces these stutters but can flicker briefly in some areas. The setting is saved to `out/CUSA03219/ps4rt.ini` when the menu closes. The game retains its original frame-rate behavior. Display reconstruction to 4K does not mean the original 1080p scene is natively rasterized at 4K.

The FSR 3.1 implementation and the bundled, checksum-recorded **FSR 4 v07** resources are part of the source distribution. The `fsr4_shaders/` assets, their license notice, and `packaging/fsr4-v07-assets.json` **must remain with a release that supports FSR 4**. FSR 4 requires suitable Vulkan device capabilities and falls back to FSR 3.1 if unsupported. This release is not FSR 4.1.1.

Gameplay testing reached **Chapter 4** on Fedora with an AMD Ryzen 7 9700X, NVIDIA GeForce RTX 3080 Ti, and AMD Radeon AI PRO R9700 (RX 9070 XT-class). The full game and additional Linux hosts have not been exhaustively tested.

### User data and privacy

Source builds generate output under `out/CUSA03219/`:

| Path | Contains |
| --- | --- |
| `out/CUSA03219/boot-linked.bin`, `boot.bin`, `eboot.elf` and similar files | Prepared or linked, game-derived binary data |
| `out/CUSA03219/ps4rt.ini` | Saved graphics settings |
| `out/CUSA03219/user/savedata/` and `savedata-backups/` | Player saves and backups |
| `out/CUSA03219/user/gpu/cache/CUSA03219/` | Generated game shader and pipeline caches |

These files are **local and private**. Do not distribute any `out/` contents, PS4 system modules, firmware, screenshots or captures from user directories by accident. The game's original input folder is read rather than copied into the repository. `--data-dir` is not supported by this launcher.

To exit, choose **Exit Game** in the overlay and confirm **Exit to desktop**. Save in-game first; changing graphics settings does not save gameplay progress.

## Maintainer instructions

### Source releases

Create a reviewed, clean source checkpoint and export the tracked content rather than zipping an arbitrary development directory. This project does **not** include the Valkyria-only `tools/package_valkyria.py` exporter, so its commands and runtime archive manifest do not apply here.

From a clean checkout at the release revision:

~~~bash
git status --short
PS4RT_LTO=OFF bash build.sh --test
mkdir -p out/community
git archive --format=tar.gz -o out/community/star-ocean-5-ps4rt-v1.0-source.tar.gz HEAD
sha256sum out/community/star-ocean-5-ps4rt-v1.0-source.tar.gz \
  > out/community/star-ocean-5-ps4rt-v1.0-source.tar.gz.sha256
~~~

`git status --short` should be reviewed for unintended or untracked changes first; `git archive HEAD` includes **tracked files** from that commit, not untracked local files. Review the resulting tarball's file list before publishing.

~~~bash
tar -tzf out/community/star-ocean-5-ps4rt-v1.0-source.tar.gz | less
~~~

Keep `README.md`, `LICENSE`, `docs/UPSTREAM_CREDITS.md`, source files, build scripts, bundled dependency code, license notices, and required FSR3/FSR4 assets. The official port may also contain its intended README gameplay image and `gpu/assets/` overlay art and sound effects; identify and review these assets separately from the runtime's generated game data.

**Never include** game extractions or encrypted/decrypted PKGs, proprietary linked game binaries, PS4 firmware/modules, personal saves, shader/pipeline caches under `out/`, debugging logs, private analysis work, reference repositories, or historical AGENTS/porting-log files. The project's ignore rules are a safeguard, **not** a substitute for checking the archive.

A local archive does not automatically create a GitHub Release or upload anything.

### Optional prebuilt runtime release

There is **no verified one-command binary packager** for this Star Ocean repository. If a portable release is made later, build its host binaries from the exact source revision to be released, test extraction and execution on the claimed host distributions, and audit runtime library dependencies.

The current runtime is built as `out/ps4rt` and `out/gpu/libps4rtgpu.so` (plus local dependencies). A valid package would need a launcher adjusted to the packaged layout, the Python game-preparation/linking helpers, the required FSR resources, and a distribution-specific approach to external system libraries and compatible drivers. Do not publish an archive merely because the local checkout launches correctly.

### License and provenance

PS4RT is **adapted open-source software**, not a clean-room implementation. Retain the credits and original copyright notices for the God of War III and Bloodborne native-port lineage, the shadPS4 renderer, and all vendored components, as recorded in [Upstream credits and notices](UPSTREAM_CREDITS.md).

The runtime source is distributed under **GPL-2.0-or-later**; third-party code and data keep their individual terms. Relevant dependencies include FidelityFX/FSR-Vulkan, ImGui, LibAtrac9, Sirit, half, xbyak, miniz, magic_enum, DejaVu fonts, and others bundled in the source.

FFmpeg licensing obligations depend on the **specific build and linking configuration**. When distributing host binaries or bundled system libraries, inspect the actual FFmpeg build flags, applicable license version, and any corresponding-source obligations. Do not blindly copy system library directories into a tarball. See [FFmpeg legal guidance](https://ffmpeg.org/legal.html) and the [GNU GPL license information](https://www.gnu.org/licenses/).

Review the rights and attribution for the illustrated overlay, bundled menu audio and gameplay screenshot separately before public redistribution. A game-content-free runtime source export does not itself grant permission to redistribute Square Enix or Sony game media, or claim affiliation.

Before announcing a public binary release, perform a final clean-tree audit, test the exact distributed files, verify checksums and dependencies, and publish matching source and license notices. Publication is a separate release decision.
