# Star Ocean 5: Integrity and Faithlessness — PS4RT for PC Linux Port

An experimental, game-specific Linux compatibility runtime for **Star Ocean: Integrity and Faithlessness**
(PlayStation 4, **CUSA03219**), tested on **Fedora Linux, x86-64**.

PS4RT prepares and runs the game's original x86-64 code on a Linux host, supplies
compatibility implementations of the PS4 services it needs, and uses a Vulkan
graphics renderer derived from shadPS4. This is **not a general-purpose PS4
emulator**, a native recreation of the game, or a replacement for owning the
game.

<p align="center">
  <img src="docs/images/readme/sample.png" alt="Star Ocean: Integrity and Faithlessness running on Linux with PS4RT" width="900">
</p>

**No game files, firmware, or decryption tools are supplied.** You must provide
your own extracted, plaintext CUSA03219 game folder. The launcher does not
download games, decrypt encrypted packages, or connect to PSN. This project is
not affiliated with Square Enix, Sony, or AMD.

## Download and play (no compiling required)

**Want to play the game? Use the prebuilt Linux release. You do not need to compile PS4RT or install FFmpeg, SDL3, or other bundled application libraries.**

1. **Download:** Open the [v1.2 GitHub release](https://github.com/HekamiAeon/Star-Ocean-Integrity-and-Faithlessness-PS4RT/releases/tag/v1.2) and download the **`star-ocean-5-ps4rt-linux-x86_64-7010cc2.tar.gz`** file. The separately named **`star-ocean-5-ps4rt-source-7010cc2.tar.gz`** archive is for developers. v1.2 supersedes v1.1; earlier versions remain available for rollback.
2. **Have your own extracted PS4 game:** The launcher needs an already extracted, plaintext **`CUSA03219` folder** containing `sce_sys/param.sfo`, `eboot.bin`, `sce_module/`, and the game's data. **Point to the CUSA03219 folder itself**, not the PKG, not its parent directory, and not the `eboot.bin` file.
3. **Extract the Linux download** (using your file manager's Extract option, or the commands below).
4. **Launch the game** using the command below, replacing the example game path with the real location of your `CUSA03219` folder:

```bash
tar -xzf star-ocean-5-ps4rt-linux-x86_64-7010cc2.tar.gz
cd star-ocean-5-ps4rt-linux-x86_64-7010cc2
bash Play-Game.sh --game "/path/to/CUSA03219"
```

On later launches, the validated game folder is remembered, so run `bash Play-Game.sh` without an argument. Explicit `--game` always takes priority and updates the remembered location. `Play-Star-Ocean.sh` remains a compatibility alias for existing shortcuts.

**That's it.** There is no `LD_LIBRARY_PATH` setup, and no need to compile the game.

Leave the extracted release folder together. The first launch creates local working data under `out/CUSA03219/` by default; your game extraction is read rather than overwritten. Quote paths containing spaces. If no valid game folder is configured, the launcher prints an example `--game` command and refers you to `PLAYER_README.txt`. No graphical folder picker or Tkinter installation is needed. The optional `--data-dir` selects a different private data directory, which must be supplied again on later launches.

**Graphics are controlled in-game.** Hold **L2** and **click the right stick (R3)** to open the graphics settings. On a keyboard, press **Insert**. Choose the resolution (1080p, 1440p, 4K), upscaling (Off, FSR 3.1, or FSR 4 v07 where supported), sharpening, AF, and Sync/Async shader compilation. Use the D-pad, Cross to confirm, and Circle to go back.

**You do NOT need to specify FSR or resolution at launch.** Those command-line options are entirely optional. If you prefer, you can start directly with FSR 4 and 4K output:

```bash
bash Play-Game.sh --game "/path/to/CUSA03219" --upscaler fsr4 --output-resolution 4k
```

**IMPORTANT: Changing anisotropic filtering (AF) requires restarting the game.** The other graphics settings above apply without a restart.

**Player requirements:** x86-64 Linux with glibc **2.35 or newer**, Python 3, a working Vulkan loader/driver, and compatible host display/input libraries. Required app libraries such as FFmpeg 8 and SDL3 are already in the download. No `LD_LIBRARY_PATH` command is needed. Fedora gameplay has been tested. Steam Deck operation has been reported, but broader independent SteamOS and other-distribution validation remains pending.

**Architecture support:** The prebuilt runtime targets **native x86-64 (AMD64) CPUs only**. ARM64 devices, including Snapdragon-based handhelds running Linux through FEX or Box64, are **not supported or validated**. Such translation layers may start the game, but correct execution and stability are not guaranteed. An ARM-compatible Linux distribution does not remove the CPU translation requirement.

For a more detailed walkthrough, see [Running on Linux](docs/RUNNING_LINUX.md). The download does **not** include PS4 game files or firmware.

## Current status

**Experimental and playable through tested early-story sections; not a verified
full playthrough.** Tested on Fedora with an **AMD Ryzen 7 9700X** CPU and
**NVIDIA GeForce RTX 3080 Ti** and **AMD Radeon RX 9070 XT-class**
graphics (the AMD hardware used was a **Radeon AI PRO R9700**). Verified
gameplay includes the introduction, story cinematics, menus, audio,
controller input, town exploration, battles, and save/load progression
through **Chapter 4**.
4K output with FSR 4 v07 has also been tested. Other hardware
configurations, Linux distributions, and the full game have not
been independently verified.

- SDL-compatible gamepads, including DualSense and XInput, plus keyboard controls.
- Output at **1080p, 1440p, or 4K**, with **FSR 3.1**, optional **FSR 4 v07**,
  and sharpening controls.
- **Anti-aliasing (AA)** is provided through the selected FSR reconstruction
  mode. Sharpening is a separate image adjustment.
- **Anisotropic filtering (AF)** is separate from AA: choose Game default,
  2×, 4×, 8×, or 16×. AF changes require restarting the game.
- Custom illustrated PS4RT in-game graphics overlay with menu sound effects.
  Open it with **Insert**, or press **L2 + R3 click**. Navigate with the D-pad,
  confirm with Cross, and go back with Circle.
- **Synchronous / asynchronous shader compilation** is selectable live from
  the in-game overlay, with the choice saved when the overlay closes:
  - **Sync (default):** occasional stutters while new shaders load, with
    more reliable rendering.
  - **Async:** smoother gameplay during shader loading, with occasional brief
    visual flickers in some areas.
- **Restart Game** in the graphics menu asks for confirmation before restarting. Save in-game progress first; restarting does not create a game save.
- **Exit Game** in the graphics menu requests confirmation before returning
  to the Linux desktop. Save your in-game progress first; this action does
  not trigger an automatic game save.
- Higher output resolutions use reconstruction/upscaling rather than a claim
  of native 4K game rendering.
- Original game frame-rate behavior is retained; no bundled 60 FPS patch or
  frame-generation gameplay mode is claimed.

FSR 3.1 is available in the runtime. The FSR 4 **v07** shader/model assets
and their license notice are included in `fsr4_shaders/`; hashes and provenance
are recorded in `packaging/fsr4-v07-assets.json`. FSR 4 requires compatible
Vulkan GPU features and otherwise falls back to FSR 3.1. These are **upscaler
dependencies**, not cached shaders compiled from the game.

## Build from source (developers only)

**Downloaded the prebuilt Linux release? Skip this section.** The commands below are for contributors compiling PS4RT; players should use the `Play-Game.sh` instructions above.

From a source checkout, install a C/C++ toolchain, CMake, Ninja, pkg-config, Python 3, and development packages for Vulkan, SDL3, FFmpeg, fmt, Boost, xxhash, robin-map, Vulkan Memory Allocator, Zydis, glslang, and X11. Included third-party source components are built as needed.

```bash
PS4RT_LTO=OFF bash build.sh --test
bash run.sh --game "/path/to/CUSA03219"
```

The old source-build commands remain valid on a host that has the required development packages. The `--test` build runs automated tests, not a full gameplay test. Source checkouts prepare private, game-derived files under `out/` without modifying the original game folder. Never commit or distribute `out/`, which can contain game-derived binaries, saves, and GPU caches.

For creating the glibc 2.35 release binaries with the standardized Ubuntu 22.04 container instead of a host-native source build, see [Linux release build instructions](packaging/linux/README.md) and the [distribution guide](docs/DISTRIBUTING_LINUX.md).

### Keyboard and controller remapping

Default keyboard controls include **WASD** (left stick), **arrow keys**
(right stick), **Space** (Cross), **Left Shift** (Circle), **E** (Square),
**Q** (Triangle), and **I/J/K/L** (D-pad).

You can remap PS4 buttons at launch through `PS4RT_KEY_MAP`:

```bash
PS4RT_KEY_MAP="cross=Q,triangle=Space" \
  bash run.sh --game /path/to/your/CUSA03219
```

`PS4RT_PAD_MAP` similarly remaps gamepad buttons. These are launch-time
environment variables, **not keys in `ps4rt.ini`**; there is currently
no in-game key-binding editor. Default mappings and keyboard stick axes
are defined in [`src/runtime_pad.c`](src/runtime_pad.c). These button maps
do not remap the analog stick axes or the PS4RT menu shortcut.

### Saves and private files

The source checkout stores generated game code, settings, saves, and GPU
caches under `out/CUSA03219/`. Notable paths include:

- `out/CUSA03219/ps4rt.ini`: saved in-game graphics settings.
- `out/CUSA03219/game-folder.txt`: remembered validated game directory.
- `out/CUSA03219/user/savedata/`: game save data.
- `out/CUSA03219/user/savedata-backups/`: save backups, when present.
- `out/CUSA03219/user/gpu/cache/CUSA03219/`: generated GPU shaders
  and pipeline caches, when present.

Pass `--data-dir "/your/private/path"` to keep working data elsewhere. Use the same option on subsequent launches; it does not move existing saves automatically.

**Never publish the `out/` directory.** Prepared game executables and
linked data can contain proprietary game material, while savedata and caches
belong to individual players. None of these belong in source control or
release archives.

## Credits, origins, and license

This Star Ocean-specific project is **adapted open-source compatibility
software**, not a clean-room implementation. Its runtime builds on work from:

- [FelipePrado0/gow3_pc](https://github.com/FelipePrado0/gow3_pc), the
  God of War III compatibility-runtime project from which the porting
  runtime was adapted.
- [deadinside28/bloodborne_pc](https://github.com/deadinside28/bloodborne_pc)
  and [Supermedo/bloodborne_pc](https://github.com/Supermedo/bloodborne_pc),
  earlier Bloodborne PC runtime and launcher lineage.
- [shadPS4](https://github.com/shadps4-emu/shadPS4) and related Vulkan
  renderer and shader-recompiler contributors.

Development also used [REA: Reverse Engineer Anything](https://github.com/morluto/rea)
and [Ghidra](https://github.com/NationalSecurityAgency/ghidra) as
reverse-engineering **analysis tools**, not embedded runtime code.

See [Upstream credits and notices](docs/UPSTREAM_CREDITS.md) for additional
contributors, source provenance, and third-party licenses. Original authors'
copyright and license notices remain in the source tree. The project's
runtime source is distributed under **GPL-2.0-or-later**; vendored components
retain their respective terms. A separate project name and fresh Git history
do not change those obligations.
