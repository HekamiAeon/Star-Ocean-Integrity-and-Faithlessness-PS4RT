# Upstream source credits and notices

PS4RT is an independent project and release effort, but its
compatibility runtime is **adapted open-source software**, not a clean-room implementation.
The current source tree incorporates earlier runtime and renderer work. The original
contributors remain credited here even where their game-specific features were removed.

## Runtime lineage

- [Felipe Prado / gow3_pc](https://github.com/FelipePrado0/gow3_pc): original
  God of War III PC compatibility-runtime implementation from which the runtime was adapted.
- [deadinside28 / bloodborne_pc](https://github.com/deadinside28/bloodborne_pc):
  earlier runtime lineage.
- [Supermedo / bloodborne_pc](https://github.com/Supermedo/bloodborne_pc):
  earlier Windows adaptation and launcher lineage.
- [shadPS4 contributors](https://github.com/shadps4-emu/shadPS4):
  emulator video core, GPU drivers and shader-recompiler foundations.
- [cuesta4 / shadPS4](https://github.com/cuesta4/shadPS4/tree/eltutz):
  related renderer and compatibility contributions.

## Research and reverse-engineering tools

- [REA: Reverse Engineer Anything (morluto/rea)](https://github.com/morluto/rea):
  agent-facing investigation tooling used during development with native binary
  analysis. REA is **not** a runtime-code ancestor or a bundled game dependency.
- [Ghidra](https://github.com/NationalSecurityAgency/ghidra):
  reverse-engineering suite used for binary analysis alongside REA. Its
  software is not incorporated into the distributed runtime.

## Licensing

This project retains source-level copyright, SPDX and license notices.
The project license is [GPL-2.0-or-later](../LICENSE). Adapted files and vendored
dependencies keep their original license notices under their respective source
directories. Source builds may link local system dependencies. The Linux v1.2 binary archive bundles audited FFmpeg and other application libraries with independent redistribution terms.

FSR-Vulkan/AMD FidelityFX, ImGui, LibAtrac9, Sirit, half, xbyak, miniz, magic_enum,
DejaVu fonts, and additional vendored libraries retain their own authors and licenses.
Refer to the included third-party source and license files for the applicable terms.

A distinct PS4RT project name does not change the copyright or origin of
code incorporated from upstream projects.
