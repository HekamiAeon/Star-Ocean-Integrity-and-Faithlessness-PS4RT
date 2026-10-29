Star Ocean: Integrity and Faithlessness - PS4RT Linux x86-64

FIRST LAUNCH
Supply your own extracted plaintext PS4 game folder (CUSA03219).
Open a terminal in the extracted player release folder and run:
  bash Play-Game.sh --game "/absolute/path/to/CUSA03219"

SUBSEQUENT LAUNCHES
After the first successful launch, the validated game location is remembered:
  bash Play-Game.sh
An explicit --game always overrides and updates the remembered location.
An invalid or missing game folder causes an error; no picker is used.

OPTIONAL PRIVATE DATA DIRECTORY
Default saves, settings, and prepared game files remain in out/CUSA03219/.
To use a different private folder for those files:
  bash Play-Game.sh --game "/absolute/path/to/CUSA03219" --data-dir "/path/to/private-data"
Use the same --data-dir on later launches. No existing saves are migrated.

GRAPHICS OPTIONS
You can also provide --upscaler off|fsr3|fsr4 and
--output-resolution 1080|1440|4k. Without flags, use saved menu settings.
Restart Game preserves the selected game, private data folder, and overrides.

TROUBLESHOOTING
If the game moves, supply --game again. Select the extracted CUSA03219
directory, not its parent, a PKG, or eboot.bin. The legacy
Play-Star-Ocean.sh launcher remains as a compatibility alias.
Native Linux x86-64, glibc 2.35+, Python 3 and a working Vulkan driver
are required. No PS4 game files or firmware are included.

Instructions: https://github.com/HekamiAeon/Star-Ocean-Integrity-and-Faithlessness-PS4RT

