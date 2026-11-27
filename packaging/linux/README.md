# Star Ocean: Integrity and Faithlessness Linux release builder

This reproduces the Ubuntu 22.04 / glibc 2.35 / GCC 14 / FFmpeg 8 developer build environment validated and gameplay-tested with Star Ocean. Players **do not** need Podman or development libraries.

From this source repository:

```bash
# Only if you do not already have localhost/ps4rt-builder:linux:
bash packaging/linux/build-images.sh

# Clean committed-source build, isolated from your game and saves:
bash packaging/linux/build-runtime.sh
```

The second command prints a `BUNDLE=/absolute/path` at the end. Once its executable has passed gameplay testing, package it:

```bash
BUNDLE="/path/printed/by/build-runtime.sh"
python3 tools/package_star_ocean.py --runtime --bundle-from "$BUNDLE" --preview
python3 tools/package_star_ocean.py
python3 tools/package_star_ocean.py --runtime --bundle-from "$BUNDLE"
(cd out/community && sha256sum -c *.sha256)
```

The source and runtime archives keep the existing
`star-ocean-5-ps4rt-linux-x86_64-<commit>.tar.gz` and
`star-ocean-5-ps4rt-source-<commit>.tar.gz` names, each with a
`.sha256` companion. Bundled runtime includes `out/ps4rt`, `out/gpu/libps4rtgpu.so`, and 12 audited shared libraries. Library lookup uses relative `$ORIGIN` paths. Keep host glibc 2.35+, Python 3, and a working Vulkan driver.

`Play-Game.sh` is the canonical player launcher, with `Play-Star-Ocean.sh` retained as a compatibility alias. First launch: `bash Play-Game.sh --game "/absolute/path/to/CUSA03219"`. After a valid first launch, `bash Play-Game.sh` uses the remembered folder. An explicit `--game` always overrides and updates that path. No Tkinter or graphical folder picker is used. Existing `--data-dir`, save files, restart logic and preparation behavior remain unchanged. Linux is the supported target.

The final builder image includes **PNG-enabled FFmpeg**, needed for in-game
thumbnail decoding. Earlier image stages without that decoder must not be
used for release bundles.

The image recipes are not bit-for-bit reproducible because some toolchain packages are not pinned to exact build artifacts. Gameplay testing was on Fedora and Steam Deck. The [v1.2 release](https://github.com/HekamiAeon/Star-Ocean-Integrity-and-Faithlessness-PS4RT/releases/tag/v1.2) is the current recommended download. Do not put game files, prepared executables, user saves, shaders, captures, or firmware into published archives. See [upstream credits and notices](../../docs/UPSTREAM_CREDITS.md) for attribution and licensing information.

**Release workflow:** validate the built bundle, build the exact committed-source archive, test the freshly extracted archive, then publish separately. v1.0 and v1.1 are retained as superseded historical releases; v1.2 is the recommended download.

## Tracked overlay artwork and audio

The reviewed player media files are tracked in the repository at these paths:

```text
gpu/assets/star_ocean_overlay.png
gpu/assets/sounds/navigate.wav
gpu/assets/sounds/confirm.wav
gpu/assets/sounds/cancel.wav
```

`open.wav` is optional. The audio player uses a synthesized fallback
when any WAV is missing. The game opens the menu with the confirm sound.
The portrait is drawn **inside** the translucent menu panel, center-cropped
to preserve its aspect ratio. The compact menu measures 1080x690 at 1080p.

The recorded files must be ordinary RIFF WAV files under 2 MiB each.
The portrait should be PNG, maximum 4096x4096. The build script copies
the tracked `gpu/assets/` files from the committed source snapshot into the isolated
build bundle; the runtime exporter requires that bundle's media to match them.
The public source archive also includes the tracked media, so a clean clone can
reproduce the player's overlay. The executable still works without them.

**Before committing or publishing artwork or audio, verify redistribution rights,
attributions and any required permission.** The packaging step verifies that the bundle's media
match the committed files, not copyright or ownership.

The **player archive excludes the entire `docs/` tree**, with the required
license files kept at their source paths and the credits copied to top-level
`CREDITS.md`. The source archive leaves out the project guides and the README
screenshot; only `docs/UPSTREAM_CREDITS.md` is exported from `docs/`.
