# Star Ocean: Integrity and Faithlessness — Linux distribution guide

This document explains how to build and distribute the **Star Ocean: Integrity and Faithlessness PS4RT** Linux source release for **CUSA03219**. It is an experimental, game-specific native x86-64 compatibility runtime, adapted from earlier PS4RT projects and the shadPS4 rendering foundation. Original copyright, GPL, and third-party license notices must remain intact.

**This repository contains the runtime and its licensed dependencies, not the game, its firmware, or decryption tools.** Each player supplies an independently obtained, extracted, plaintext copy of the game.

## For players: download, extract, run

**Do not compile from source to play v1.2.** Download the Linux runtime archive from the [v1.2 release](https://github.com/HekamiAeon/Star-Ocean-Integrity-and-Faithlessness-PS4RT/releases/tag/v1.2). Choose `star-ocean-5-ps4rt-linux-x86_64-7010cc2.tar.gz`, not the source archive.

Extract the download and open a terminal in the extracted folder:

```bash
tar -xzf star-ocean-5-ps4rt-linux-x86_64-7010cc2.tar.gz
cd star-ocean-5-ps4rt-linux-x86_64-7010cc2
bash Play-Game.sh --game "/path/to/CUSA03219"
```

Subsequent launches can use `bash Play-Game.sh` without `--game`; the launcher remembers the validated game location. Use `--game` again if the folder moves. `Play-Star-Ocean.sh` remains a compatibility alias.

Replace `/path/to/CUSA03219` with the path to **your extracted plaintext game folder**. It must contain `sce_sys/param.sfo`, `eboot.bin`, `sce_module/`, and game data. Do not supply the PKG, an executable file, or the parent folder.

**You do not need FFmpeg/SDL3 installation commands or `LD_LIBRARY_PATH`.** The application libraries are bundled. The player still needs glibc 2.35+, Python 3, a compatible Vulkan graphics/display stack, and x86-64 Linux.

**CPU architecture:** The release supports native **x86-64/AMD64** execution. **ARM64 is unsupported**, including ARM Linux systems using FEX, Box64, or similar translation layers. A translator may partially launch the executable, but this does not establish reliable PS4RT gameplay. The system's Linux distribution alone does not determine CPU compatibility.

**FSR and resolution launch arguments are optional.** Hold **L2** and **click R3 (the right stick)**, or press **Insert**, to choose resolution and upscaling in the graphics settings menu. **AF changes require a restart**; the other graphics options apply live. See [Running on Linux](RUNNING_LINUX.md) for details.

### Building from source is for developers

For a conventional host-native source build, install a compiler, CMake, Ninja, pkg-config, Python 3, Vulkan development packages and SDL3, FFmpeg, fmt, Boost, xxhash, robin-map, VMA, Zydis, glslang and X11 development dependencies, then from the repository root:

```bash
PS4RT_LTO=OFF bash build.sh --test
bash run.sh --game "/path/to/CUSA03219"
```

This host-native build links against the **developer's** installed libraries and does not itself produce the standardized release bundle. For a distribution-targeted build, use the automated Ubuntu 22.04/glibc 2.35 pipeline described in [`packaging/linux/README.md`](../packaging/linux/README.md). That container workflow builds the bundled release, and is **not** required by players.

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

These files are **local and private**. Do not distribute any `out/` contents, PS4 system modules, firmware, screenshots or captures from user directories by accident. The game's original input folder is read rather than copied into the repository. The launcher also remembers the selected game directory under `out/CUSA03219/`. The optional `--data-dir` selects a different private working-data location; reuse that option on subsequent launches. Existing saves are not moved automatically.

To restart, choose **Restart Game** in the overlay and confirm. To exit, choose **Exit Game** and confirm **Exit to desktop**. Save in-game first; changing graphics settings does not save gameplay progress.

## Maintainer instructions

### Source and Linux runtime packages

From the repository root, after committing source changes, run `bash packaging/linux/build-images.sh` once to create the build image if needed. Then run `bash packaging/linux/build-runtime.sh`; it prints the resulting bundle path. Package that bundle with:

~~~bash
BUNDLE="/absolute/path/printed/by/build-runtime.sh"
python3 tools/package_star_ocean.py
python3 tools/package_star_ocean.py --runtime --bundle-from "$BUNDLE"
(cd out/community && sha256sum -c *.sha256)
~~~

The source and Linux x86-64 archives keep the original filenames and commit suffix. Runtime packaging uses an allowlist of two tested ELF files and twelve audited shared libraries from the tested build. The separately published source archive contains the corresponding PS4RT source. The player archive excludes the `docs/` tree, game files, linked game code, saves and GPU caches; the required credits and licenses remain available offline.

See [`packaging/linux/README.md`](../packaging/linux/README.md) for build stages, `$ORIGIN` path validation and glibc requirements, and [upstream credits and notices](UPSTREAM_CREDITS.md) for provenance and license information. Independently verify any applicable third-party binary redistribution obligations. Independent Steam Deck testing is still pending.

### License and provenance

PS4RT is **adapted open-source software**, not a clean-room implementation. Retain the credits and original copyright notices for the God of War III and Bloodborne native-port lineage, the shadPS4 renderer, and all vendored components, as recorded in [Upstream credits and notices](UPSTREAM_CREDITS.md).

The runtime source is distributed under **GPL-2.0-or-later**; third-party code and data keep their individual terms. Relevant dependencies include FidelityFX/FSR-Vulkan, ImGui, LibAtrac9, Sirit, half, xbyak, miniz, magic_enum, DejaVu fonts, and others bundled in the source.

FFmpeg licensing obligations depend on the **specific build and linking configuration**. When distributing host binaries or bundled system libraries, inspect the actual FFmpeg build flags, applicable license version, and any corresponding-source obligations. Do not blindly copy system library directories into a tarball. See [FFmpeg legal guidance](https://ffmpeg.org/legal.html) and the [GNU GPL license information](https://www.gnu.org/licenses/).

Review the rights and attribution for the illustrated overlay, bundled menu audio and gameplay screenshot separately before public redistribution. A game-content-free runtime source export does not itself grant permission to redistribute Square Enix or Sony game media, or claim affiliation.

Before announcing a public binary release, perform a final clean-tree audit, test the exact distributed files, verify checksums and dependencies, and publish matching source and license notices. Publication is a separate release decision.
