// SPDX-License-Identifier: GPL-2.0-or-later
// The upscaler jitter of a light pass, packed into the scene-size push constant
// (Shader::PushData::PackJitter) and read back the way the FragCoord emission does.
#include <cassert>
#include <cmath>
#include <cstdio>

#include "shader_recompiler/resource.h"

using Shader::PushData;

static float Unpack(u32 packed, u32 comp) {
    const u32 steps = (packed >> (comp * PushData::JitterBits)) & ((1u << PushData::JitterBits) - 1);
    return float(steps) / float(PushData::JitterSteps) - 0.5f;
}

int main() {
    // Marked as jitter, never as a reduced size (heights stay below 1 << 15).
    const u32 zero = PushData::PackJitter(0.0f, 0.0f);
    assert(zero & PushData::JitterFlag);
    assert(Unpack(zero, 0) == 0.0f && Unpack(zero, 1) == 0.0f);
    assert(!((7680u | 4320u << 16) & PushData::JitterFlag));

    // Halton(2, 3) phases of a 72-phase sequence, the corners, and both axes kept apart.
    for (u32 i = 1; i <= 72; ++i) {
        float x = 0.0f, y = 0.0f;
        for (float f = 0.5f, n = float(i); n > 0; f *= 0.5f, n = std::floor(n / 2))
            x += f * std::fmod(n, 2.0f);
        for (float f = 1.0f / 3.0f, n = float(i); n > 0; f /= 3.0f, n = std::floor(n / 3))
            y += f * std::fmod(n, 3.0f);
        const u32 packed = PushData::PackJitter(x - 0.5f, y - 0.5f);
        assert(std::fabs(Unpack(packed, 0) - (x - 0.5f)) <= 0.5f / PushData::JitterSteps);
        assert(std::fabs(Unpack(packed, 1) - (y - 0.5f)) <= 0.5f / PushData::JitterSteps);
    }
    const u32 corners = PushData::PackJitter(-0.5f, 0.5f);
    assert(Unpack(corners, 0) == -0.5f && Unpack(corners, 1) == 0.5f);
    const u32 clamped = PushData::PackJitter(-2.0f, 3.0f);
    assert(Unpack(clamped, 0) == -0.5f && Unpack(clamped, 1) == 0.5f);
    std::puts("screen pass jitter: ok");
    return 0;
}
