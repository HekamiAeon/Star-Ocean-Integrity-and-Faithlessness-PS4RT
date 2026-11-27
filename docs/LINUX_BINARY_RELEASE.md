# Run the Star Ocean: Integrity and Faithlessness PS4RT Linux x86-64 release (v1.2)

**For players: download the portable Linux runtime, extract it, and point the launcher at your own game. No compilation or manual FFmpeg/SDL3 installation is required.**

## 1. Download the right file

Open the [Star Ocean: Integrity and Faithlessness PS4RT v1.2 GitHub release](https://github.com/HekamiAeon/Star-Ocean-Integrity-and-Faithlessness-PS4RT/releases/tag/v1.2)
(or [the latest release](https://github.com/HekamiAeon/Star-Ocean-Integrity-and-Faithlessness-PS4RT/releases/latest)). Download:

- **[star-ocean-5-ps4rt-linux-x86_64-7010cc2.tar.gz](https://github.com/HekamiAeon/Star-Ocean-Integrity-and-Faithlessness-PS4RT/releases/download/v1.2/star-ocean-5-ps4rt-linux-x86_64-7010cc2.tar.gz)**: prebuilt Linux runtime. **Choose this to play.**
- [star-ocean-5-ps4rt-linux-x86_64-7010cc2.tar.gz.sha256](https://github.com/HekamiAeon/Star-Ocean-Integrity-and-Faithlessness-PS4RT/releases/download/v1.2/star-ocean-5-ps4rt-linux-x86_64-7010cc2.tar.gz.sha256): SHA256 verification file for the runtime.
- [star-ocean-5-ps4rt-source-7010cc2.tar.gz](https://github.com/HekamiAeon/Star-Ocean-Integrity-and-Faithlessness-PS4RT/releases/download/v1.2/star-ocean-5-ps4rt-source-7010cc2.tar.gz): matching source code for developers, **not required just to play**.
- [star-ocean-5-ps4rt-source-7010cc2.tar.gz.sha256](https://github.com/HekamiAeon/Star-Ocean-Integrity-and-Faithlessness-PS4RT/releases/download/v1.2/star-ocean-5-ps4rt-source-7010cc2.tar.gz.sha256): SHA256 verification file for the source archive.

These are the four uploaded release assets. GitHub's automatic “Source code”
ZIP/tarball links are different from the matching source package above.

To verify your runtime download, save the Linux `.tar.gz` and its `.sha256`
in the same directory and run:

```bash
sha256sum -c star-ocean-5-ps4rt-linux-x86_64-7010cc2.tar.gz.sha256
```

It should report `OK`.

## 2. Supply your own extracted PS4 game

The game is **Star Ocean: Integrity and Faithlessness, title ID CUSA03219**. Supply an extracted, *plaintext* game directory with a structure like:

```text
CUSA03219/
  eboot.bin
  sce_sys/param.sfo
  sce_module/
  (other game data)
```

Point the launcher to `CUSA03219` itself, **not** a PKG, `eboot.bin`, or its parent directory.

Neither the runtime nor the source archive includes PS4 game data, updates, DLC, firmware, decryption keys, or saved games.

## 3. Extract and launch

Extract the Linux archive using your file manager, or in the directory
containing the downloaded runtime:

```bash
tar -xzf star-ocean-5-ps4rt-linux-x86_64-7010cc2.tar.gz
cd star-ocean-5-ps4rt-linux-x86_64-7010cc2
bash Play-Game.sh --game "/path/to/CUSA03219"
```

Replace the quoted path with the location of **your own extracted game folder**. Keep the entire extracted PS4RT directory together and writable: prepared game files, settings, saves and shader caches live under that directory (`--data-dir` selects another folder); your extracted game folder is read, not modified.

**Later launches:** After successful first-time preparation, run `bash Play-Game.sh` without `--game`. The player remembers the validated CUSA03219 directory. Supply `--game` again if it moves. With no configured game path, the launcher prints an example command and directs you to `PLAYER_README.txt` in the extracted player folder.

Optional command-line overrides are available:

```bash
bash Play-Game.sh --game "/path/to/CUSA03219" --upscaler fsr4 --output-resolution 4k
```

## Requirements and limitations

- Native **x86-64/AMD64 Linux**, with **glibc 2.35+** and **Python 3**. ARM64 and CPU translators such as FEX or Box64 are not supported or validated, they might or might not work depending on the device.
- Working Vulkan 1.3-capable graphics driver/loader and compatible desktop display/input environment. FSR 4 depends on additional compatible device features and falls back to other supported options when unavailable.
- Fedora Linux with AMD/RADV/NVIDIA graphics has been gameplay-tested, and Steam Deck is confirmed working.
- The portable bundles its audited application libraries, including SDL3 and FFmpeg. **Do not install separate versions or set `LD_LIBRARY_PATH` merely to run the portable.**
