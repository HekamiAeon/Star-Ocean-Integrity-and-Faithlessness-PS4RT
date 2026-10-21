# Linux x86-64 binary release

Extract `star-ocean-5-ps4rt-linux-x86_64-<commit>.tar.gz` anywhere writable.
Launch with `bash Play-Star-Ocean.sh --game '/path/to/extracted/CUSA03219'`,
optionally `--upscaler fsr4 --output-resolution 4k`.

The runtime contains PS4RT's source under `source/`, both host ELF files,
12 bundled application libraries, FSR4 assets, overlay media and Python
launcher. It contains **no PS4 game files**. Supply your own extracted title.

Host requirements: x86-64 Linux, glibc **2.35+**, Python 3, usable Vulkan
loader/ICD/driver and host display/input libraries. Fedora gameplay has been
validated; independent SteamOS compatibility remains unverified. The game
loads its packaged application libraries through `$ORIGIN` RUNPATH.

See `docs/RUNNING_LINUX.md` for controls and `source/packaging/linux/`
for the maintainer build stages and licensing review.
