// SPDX-License-Identifier: GPL-2.0-or-later
#include "ps4rt_menu_sounds.h"
#include <cassert>
#include <cmath>
#include <cstdio>

int main() {
    using namespace Ps4RtMenuSounds;
    const int expected[] = {1680, 5040, 4560, 6480}; // 48kHz * duration
    for (unsigned i = 0; i < unsigned(Cue::Count); ++i) {
        const auto wave = MakeCue(Cue(i));
        assert(int(wave.size()) == expected[i]);
        assert(wave.front() == 0.0f);
        assert(std::fabs(wave.back()) < 0.001f);
        bool audible = false;
        for (float sample : wave) {
            assert(std::isfinite(sample));
            assert(std::fabs(sample) <= 0.5f);
            if (std::fabs(sample) > 0.01f) audible = true;
        }
        assert(audible);
    }
    // No device is required to create or test any UI tone.
    assert(MakeCue(Cue::Count).empty());
    std::puts("Menu sounds: generated tones bounded and valid PASS");
}
