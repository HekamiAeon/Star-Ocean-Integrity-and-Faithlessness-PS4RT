# Run the Linux x86-64 release (v1.2)

**For players: download, extract, point to your game folder, and play. No compiling. No manual FFmpeg/SDL3 installation.**

## 1. Download the right file

Open the [v1.2 GitHub release](https://github.com/HekamiAeon/Star-Ocean-Integrity-and-Faithlessness-PS4RT/releases/tag/v1.2). Download:

- **`star-ocean-5-ps4rt-linux-x86_64-7010cc2.tar.gz`**: the game runtime for Linux. **Choose this one.**
- `star-ocean-5-ps4rt-source-7010cc2.tar.gz`: source code for developers, **not needed just to play**.

The earlier v1.0 and v1.1 archives are preserved for historical reference and rollback; v1.2 is the recommended download.

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
tar -xzf star-ocean-5-ps4rt-linux-x86_64-7010cc2.tar.gz
cd star-ocean-5-ps4rt-linux-x86_64-7010cc2
bash Play-Game.sh --game "/path/to/CUSA03219"
```

**Replace `/path/to/CUSA03219` with your actual extracted game folder.** Quotation marks are important if your path contains spaces. Keep the extracted Linux release directory intact and writable.

Star Ocean stores its prepared data, settings, and saves under `out/CUSA03219/` by default. After a successful first launch, run `bash Play-Game.sh` without `--game` to reuse the remembered game directory. Providing `--game` again updates that directory. The old `Play-Star-Ocean.sh` launcher remains a compatibility alias. If no game directory is configured, the launcher prints an example command and refers to `PLAYER_README.txt`. There is no Tkinter picker. You can choose a different private location with `--data-dir`, which must also be used on later launches.

## 4. Choose your graphics settings in the game

**Hold L2 and click R3 (press the right stick inward)** on a controller. On a keyboard, press **Insert**. This opens the PS4RT graphics menu.

Use the **D-pad** to navigate, **Cross** to select, and **Circle** to go back. You can choose:

- Output resolution: **1080p / 1440p / 4K**
- Upscaling: **Off / FSR 3.1 / FSR 4 v07**, depending on hardware support
- Sharpening and anisotropic filtering (**AF**)
- Shader compilation: **Sync / Async**

**FSR and resolution commands are optional.** You can select them using this menu instead. For people who prefer launch-time overrides, this is also supported:

```bash
bash Play-Game.sh --game "/path/to/CUSA03219" --upscaler fsr4 --output-resolution 4k
```

The overlay also offers **Restart Game** with a confirmation prompt. Save your progress first, since restarting does not create a game save.

**IMPORTANT: Anisotropic filtering (AF) changes require restarting the game.** Other graphics settings such as resolution, upscaling, sharpening, and Sync/Async shader compilation apply live.

## Requirements and troubleshooting

- x86-64 Linux with **glibc 2.35 or newer**; Fedora has been gameplay-tested, while SteamOS and other distros are unverified.
- Python 3.
- A working Vulkan loader/graphics driver and host display/input libraries.
- Your own extracted, plaintext `CUSA03219` game.

The release bundles **12 application libraries** including FFmpeg 8 and SDL3, so **do not install those libraries manually for the prebuilt release**. No `LD_LIBRARY_PATH` command is needed. The bundled runtime finds its libraries using relative `$ORIGIN` paths.

Do not use `build.sh` unless you specifically want to compile from source. Developer build instructions are in [`packaging/linux/README.md`](../packaging/linux/README.md). Full controls and known limitations are in [Running on Linux](RUNNING_LINUX.md).

The game is experimental; a complete playthrough and compatibility across all distros and GPUs are not verified.
