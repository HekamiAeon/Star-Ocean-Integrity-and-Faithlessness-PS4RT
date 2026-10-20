#!/usr/bin/env bash
set -euo pipefail
cd -- "$(dirname -- "$0")"
command -v cmake >/dev/null
command -v ninja >/dev/null
pkg-config --exists vulkan sdl3 libavcodec libavutil libswscale
cmake -S gpu -B out/gpu -G Ninja -DCMAKE_BUILD_TYPE=Release -DPS4RT_PGO=off -DPS4RT_LTO="${PS4RT_LTO:-OFF}"
ninja -C out/gpu -j "${PS4RT_BUILD_JOBS:-4}" ps4rtgpu
read -r -a includes <<< "$(pkg-config --cflags vulkan sdl3 libavcodec libavutil libswscale)"
read -r -a libraries <<< "$(pkg-config --libs vulkan sdl3 libavcodec libavutil libswscale)"
compiler=${CC:-cc}
mkdir -p out/atrac9
for source in third_party/LibAtrac9/C/src/*.c; do
    "$compiler" -std=c99 -O2 -w -c "$source" -o "out/atrac9/$(basename "${source%.c}").o"
done
ar rcs out/libatrac9.a out/atrac9/*.o
"$compiler" -std=c11 -O2 -Wall -Wextra -Werror -mgeneral-regs-only -c src/isa_emulate.c -o out/isa_emulate.o
"$compiler" -std=c11 -O2 -Wall -Wextra -Werror -pthread -no-pie -DPS4RT_RUNTIME "${includes[@]}" -I. -Isrc \
    src/loader.c src/runtime*.c src/cpu_check.c out/isa_emulate.o src/isa_rewrite.c out/libatrac9.a \
    -lm -Lout/gpu -lps4rtgpu -Wl,-rpath,'$ORIGIN/gpu' -rdynamic "${libraries[@]}" -o out/ps4rt
if [[ ${1:-} == --test ]]; then
    ninja -C out/gpu -j "${PS4RT_BUILD_JOBS:-4}" display-sample-gpu-test transition-snapshot-gpu-test \
        cpu-image-writeback-test color-export-test image-subresource-test gpu-workarounds-test \
        camera-layout-test scene-camera-history-test menu-input-test menu-sounds-test graphics-settings-test
    "$compiler" -std=c11 -O2 -Wall -Wextra -Werror -pthread "${includes[@]}" -I. -Isrc \
        tests/test_content.c src/runtime_content.c -o out/content-test
    ctest --test-dir out/gpu --output-on-failure
    python3 -m unittest discover -s tests -p 'test_*.py' -v
fi
