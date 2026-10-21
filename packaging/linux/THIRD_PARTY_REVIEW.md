# Third-party library redistribution checklist

**Not complete: do not publicly publish RC1 or v1.1 until resolved.**
The source release currently includes PS4RT corresponding source, but the
bundled shared libraries have separate licenses and sometimes independent
corresponding-source obligations. This note is NOT the license texts.

Libraries: FFmpeg 8.0.3 (avcodec, avformat, avutil, swscale, swresample),
SDL3, fmt11, Zydis5, xxhash, zlib, libstdc++ and libgcc_s. For each binary:
record provenance/version, actual license/copyright text, any required source
or source offer, and include the notices in the runtime download. Check
Zycore as part of Zydis and GCC's Runtime Library Exception.

FFmpeg's official LGPL checklist specifically asks distributors to provide
**exact matching FFmpeg source**, configure command, patches (if any), license
notice and source access near the binary download. Check actual FFmpeg flags
(no `--enable-gpl`/`--enable-nonfree`) and retain the PNG/zlib settings.
See https://ffmpeg.org/legal.html .

Also review overlay art, recorded menu audio, screenshot provenance and FSR
assets. Never include the proprietary PS4 title, firmware, saved data or
prepared game executables. This checklist is not legal advice.
