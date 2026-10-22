# Running Star Ocean: Integrity and Faithlessness on Linux

This is an experimental, game-specific native x86-64 Linux compatibility runtime for the PlayStation 4 version of **Star Ocean: Integrity and Faithlessness (CUSA03219)**. PS4RT executes prepared game code with PS4 service compatibility shims and a shadPS4-derived Vulkan renderer. It is **not a general-purpose PS4 emulator**.

You must supply your own **extracted, plaintext** CUSA03219 game folder. Neither this repository nor the launcher includes the game, PS4 firmware, package decryption tools, or downloads.

## Download and run v1.1 (no compiling)

**Playing the game? Download the Linux runtime archive from the [latest release](https://github.com/HekamiAeon/Star-Ocean-Integrity-and-Faithlessness-PS4RT/releases/latest).** Choose `star-ocean-5-ps4rt-linux-x86_64-412d2b1.tar.gz`, not the source archive.

1. Have your own extracted, plaintext `CUSA03219` directory. It must contain `sce_sys/param.sfo`, `eboot.bin`, `sce_module/`, and the game's data. Do not point to a `.pkg`, `eboot.bin` alone, or the parent of `CUSA03219`.
2. Extract the downloaded `.tar.gz` with your file manager, or run the extraction commands below.
3. Open a terminal **inside the extracted Linux release folder**, then run `bash Play-Star-Ocean.sh --game "/path/to/CUSA03219"`. Replace the example path with your real one and keep quotes if it contains spaces.

```bash
tar -xzf star-ocean-5-ps4rt-linux-x86_64-412d2b1.tar.gz
cd star-ocean-5-ps4rt-linux-x86_64-412d2b1
bash Play-Star-Ocean.sh --game "/path/to/CUSA03219"
```

**No library installation step is necessary for FFmpeg, SDL3, fmt, Zydis, or the other bundled application libraries.** No `LD_LIBRARY_PATH` setting or source compilation is required. You still need x86-64 Linux, glibc 2.35+, Python 3, and a working host Vulkan graphics/display stack.

### Graphics: choose settings inside the game

**Hold L2 and click R3 (press the right stick inward)** on a controller, or press **Insert** on the keyboard to open the PS4RT graphics menu. Set **1080p, 1440p or 4K** output, **Off / FSR 3.1 / FSR 4 v07** upscaling (as supported), sharpening, anisotropic filtering (AF), and Sync/Async shader compilation. D-pad moves, Cross confirms, Circle goes back.

**FSR and resolution command-line arguments are optional.** You can set everything in the graphics menu. If you want launch-time overrides, this works too:

```bash
bash Play-Star-Ocean.sh --game "/path/to/CUSA03219" --upscaler fsr4 --output-resolution 4k
```

**IMPORTANT: AF (anisotropic filtering) changes require a game restart to take effect.** Resolution, upscaling, sharpening, and Sync/Async shader compilation apply live. The menu saves changed settings when it closes.

The game is not included in this download. Fedora has been gameplay-tested; SteamOS and other distributions still need independent verification. See [Linux binary release](LINUX_BINARY_RELEASE.md) for the package layout.

## Building from source (developers only)

Install the source dependencies listed in the [README](../README.md), including a C/C++ toolchain, Python 3, CMake, Ninja, pkg-config, Vulkan, SDL3, FFmpeg and the other CMake dependencies. Fedora Linux x86-64 is the tested environment.

From the repository root, build the runtime and run its automated checks:

~~~bash
PS4RT_LTO=OFF bash build.sh --test
~~~

A faster subsequent build, without rerunning all tests:

~~~bash
PS4RT_LTO=OFF bash build.sh
~~~

The runtime binary is generated at `out/ps4rt`. Passing the test suite checks the build, **not** every part of the game.

## Launching a source build (developers only)

Point the launcher at your own extracted CUSA03219 game directory:

~~~bash
bash run.sh --game /path/to/your/CUSA03219
~~~

For the tested 4K / FSR 4 configuration:

~~~bash
bash run.sh \
  --game /path/to/your/CUSA03219 \
  --upscaler fsr4 \
  --output-resolution 4k
~~~

`--upscaler` accepts `off`, `fsr3`, or `fsr4`; `--output-resolution` accepts `1080`, `1440`, or `4k`. These are optional launch-time overrides. Without them, select the graphics options inside the PS4RT overlay. The launcher **requires** `--game`: this release has no folder-selection dialog or remembered game path.

The game folder must include `sce_sys/param.sfo` with the appropriate title ID, `eboot.bin`, `sce_module/` and its required game data. The input must already be extracted and usable; the launcher does not decrypt encrypted game packages.

The launcher prepares and links game code in its private output area, then starts the runtime. You can prepare files without launching the game:

~~~bash
bash run.sh --game /path/to/your/CUSA03219 --prepare-only
~~~

### Local files, saves, and shader caches

A source checkout stores its generated working files under `out/CUSA03219/`:

| Location | Purpose |
| --- | --- |
| `out/CUSA03219/ps4rt.ini` | Saved graphics preferences, including Sync/Async |
| `out/CUSA03219/user/savedata/` | In-game saves |
| `out/CUSA03219/user/savedata-backups/` | Save backups, when generated |
| `out/CUSA03219/user/gpu/cache/CUSA03219/` | Generated game shaders and Vulkan pipeline caches, when present |
| `out/CUSA03219/boot-linked.bin` | Prepared game-linked executable data |

**Never publish or commit `out/`.** Its prepared code contains game material, and the saves and caches are private user data. Game inputs are read from the supplied extracted folder rather than distributed with the runtime.

## Controls

SDL-compatible controllers, including DualSense and XInput pads, and keyboard input are supported. Keyboard defaults apply while the game has focus:

| PS4 control | Keyboard |
| --- | --- |
| Cross / confirm | Space |
| Circle / back | Left Shift |
| Square / Triangle | E / Q |
| Options | Enter |
| D-pad up / down / left / right | I / K / J / L |
| Left stick | W / S / A / D |
| Right stick | Arrow keys |
| L1 / R1 | 1 / 3 |
| L2 / R2 | R / F |
| L3 / R3 | Z / C |
| Left / right touchpad click | Tab / Backspace |

The two touchpad-click actions are mapped independently; their behavior depends on the game.

### Remapping keyboard or gamepad buttons

The default mappings and parsing logic are in [`src/runtime_pad.c`](../src/runtime_pad.c). For custom button mappings, you **do not** need to edit that file or rebuild. Set `PS4RT_KEY_MAP` on the launch command:

~~~bash
PS4RT_KEY_MAP="cross=Q,triangle=Space" \
  bash run.sh --game /path/to/your/CUSA03219
~~~

Entries are comma-separated `ps4_button=SDL key name` pairs. Supported button names include `cross`, `circle`, `square`, `triangle`, `l1`, `r1`, `l2`, `r2`, `l3`, `r3`, `options`, `up`, `down`, `left`, `right`, `touchpad_left` (or `touchpad`), and `touchpad_right`.

`PS4RT_PAD_MAP` uses SDL gamepad button names with the same syntax:

~~~bash
PS4RT_PAD_MAP="cross=b,circle=a" \
  bash run.sh --game /path/to/your/CUSA03219
~~~

Both mapping variables take effect at launch, **not** in the PS4RT settings file. They do not remap analog stick axes, physical triggers, or the Insert / L2+R3 overlay shortcut. There is no in-game key-binding editor.

### Graphics overlay and exiting

Press **Insert**, or hold **L2** and click **R3**, to open the custom Star Ocean PS4RT graphics menu. Use the D-pad to navigate, Cross to select, and Circle to go back. The overlay offers:

- Output resolution: **1080p / 1440p / 4K**.
- Display mode, **Off / Original**, **FSR 3.1**, or **FSR 4 v07** when supported.
- Sharpening and anisotropic filtering (**Game default / 2× / 4× / 8× / 16×**).
- **Asynchronous compilation** ON or OFF. **OFF (Sync)** is the fresh-install default: it may stutter when shaders first compile but has more consistent visuals. **ON (Async)** reduces shader-loading stutter but may produce brief visual flickers in some areas.

Resolution, upscaler selection, sharpening, and shader compilation mode apply live; **IMPORTANT: changing anisotropic filtering (AF) requires restarting the game**. The overlay saves changed settings when you close it. The game renders its original 1080p scene before display reconstruction, so a 4K output is not native 4K game rendering. There is no bundled 60 FPS patch or frame-generation mode.

To quit, choose **Exit Game**, then confirm **Exit to desktop**. Cancel or Circle/Escape returns to the game. **Save progress inside Star Ocean before exiting**; saving PS4RT graphics settings is not a game save.

FSR 3.1 is part of the runtime. The FSR 4 v07 shader/model files and license notice are in `fsr4_shaders/`; they are required renderer dependencies, **not** game shader cache files. FSR 4 needs compatible Vulkan GPU features. If it is unavailable, use FSR 3.1. See the [Linux distribution guide](DISTRIBUTING_LINUX.md).

## Compatibility and limitations

Gameplay has been verified through **Chapter 4**, including the introduction, cinematics, audio, menus, controller input, exploration, battles, and saves. Fedora testing used an AMD Ryzen 7 9700X with an NVIDIA GeForce RTX 3080 Ti and AMD Radeon AI PRO R9700 (RX 9070 XT-class) across development testing. 4K output with FSR 4 v07 was also tested.

**A complete playthrough and broad compatibility across Linux distributions and hardware are not verified.** Some rendering effects may have minor issues in the asynchronous shader mode. PSN functionality is not provided. Other games and PS4 title IDs are outside this game-specific release.
