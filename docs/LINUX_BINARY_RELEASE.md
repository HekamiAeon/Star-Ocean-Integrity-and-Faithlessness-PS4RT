# Run the Linux x86-64 release (v1.1)

**For players: download, extract, point to your game folder, and play. No compiling. No manual FFmpeg/SDL3 installation.**

## 1. Download the right file

Open the [latest GitHub release](https://github.com/HekamiAeon/Star-Ocean-Integrity-and-Faithlessness-PS4RT/releases/latest). Download:

- **`star-ocean-5-ps4rt-linux-x86_64-412d2b1.tar.gz`**: the game runtime for Linux. **Choose this one.**
- `star-ocean-5-ps4rt-source-412d2b1.tar.gz`: source code for developers, **not needed just to play**.

## 2. Have your own extracted game

You must have an already extracted, **plaintext** PS4 game directory named `CUSA03219`. It should contain at least:

```text
CUSA03219/
  eboot.bin
  sce_sys/param.sfo
  sce_module/
  (other game data)
```

Do not point the launcher at a PKG, the `eboot.bin` file, or a directory *above* `CUSA03219`. The download does **not** provide the game, firmware, or a PKG extractor.

## 3. Extract and launch

Extract the Linux archive using your file manager, or from a terminal in the directory where you downloaded it:

```bash
tar -xzf star-ocean-5-ps4rt-linux-x86_64-412d2b1.tar.gz
cd star-ocean-5-ps4rt-linux-x86_64-412d2b1
bash Play-Star-Ocean.sh --game "/path/to/CUSA03219"
```

**Replace `/path/to/CUSA03219` with your actual extracted game folder.** Quotation marks are important if your path contains spaces. Keep the extracted Linux release directory intact and writable; game preparation, local settings and saves are generated inside it.

## 4. Choose your graphics settings in the game

**Hold L2 and click R3 (press the right stick inward)** on a controller. On a keyboard, press **Insert**. This opens the PS4RT graphics menu.

Use the **D-pad** to navigate, **Cross** to select, and **Circle** to go back. You can choose:

- Output resolution: **1080p / 1440p / 4K**
- Upscaling: **Off / FSR 3.1 / FSR 4 v07**, depending on hardware support
- Sharpening and anisotropic filtering (**AF**)
- Shader compilation: **Sync / Async**

**FSR and resolution commands are optional.** You can select them using this menu instead. For people who prefer launch-time overrides, this is also supported:

```bash
bash Play-Star-Ocean.sh --game "/path/to/CUSA03219" --upscaler fsr4 --output-resolution 4k
```

**IMPORTANT: Anisotropic filtering (AF) changes require restarting the game.** Other graphics settings such as resolution, upscaling, sharpening, and Sync/Async shader compilation apply live.

## Requirements and troubleshooting

- x86-64 Linux with **glibc 2.35 or newer**; Fedora has been gameplay-tested, while SteamOS and other distros are unverified.
- Python 3.
- A working Vulkan loader/graphics driver and host display/input libraries.
- Your own extracted, plaintext `CUSA03219` game.

The release bundles **12 application libraries** including FFmpeg 8 and SDL3, so **do not install those libraries manually for the prebuilt release**. No `LD_LIBRARY_PATH` command is needed. The bundled runtime finds its libraries using relative `$ORIGIN` paths.

Do not use `build.sh` unless you specifically want to compile from source. Developer build instructions are in [`packaging/linux/README.md`](../packaging/linux/README.md). Full controls and known limitations are in [Running on Linux](RUNNING_LINUX.md).

The game is experimental; a complete playthrough and compatibility across all distros and GPUs are not verified.
