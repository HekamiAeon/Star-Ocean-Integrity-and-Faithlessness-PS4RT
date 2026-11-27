// SPDX-License-Identifier: GPL-2.0-or-later
// Low graphics quality and the game's ambient occlusion draws (StarOceanQuality::Step): which
// draws are left out, which becomes the "open" result, and that everything else is drawn.
#include <cassert>
#include <cstdio>
#include <initializer_list>

#include "video_core/renderer_vulkan/star_ocean_quality.h"

using Vulkan::StarOceanQuality::Occlusion;
using Vulkan::StarOceanQuality::Step;

int main() {
    // The four draws of a frame at 1024x576 (traced): gather and two blurs at 512x288, the
    // upsample at 1024x576; each four vertices into a single-channel target, no depth target.
    assert(Step(true, true, false, 4, 512, 1024) == Occlusion::Skip);
    assert(Step(true, true, false, 4, 1024, 1024) == Occlusion::Open);
    // The same at every internal resolution of the menu.
    for (const unsigned scene : {1024u, 1280u, 1920u, 2560u, 3440u, 3840u, 5120u}) {
        assert(Step(true, true, false, 4, scene / 2, scene) == Occlusion::Skip);
        assert(Step(true, true, false, 4, scene, scene) == Occlusion::Open);
    }
    // Max and High draw them as the game issued them.
    assert(Step(false, true, false, 4, 512, 1024) == Occlusion::Draw);
    assert(Step(false, true, false, 4, 1024, 1024) == Occlusion::Draw);

    // Not the effect: another target format (the scene, the bloom chain, the display buffer).
    assert(Step(true, false, false, 4, 1024, 1024) == Occlusion::Draw);
    assert(Step(true, false, false, 4, 512, 1024) == Occlusion::Draw);
    // A depth target is bound (scene geometry, shadow maps).
    assert(Step(true, true, true, 4, 1024, 1024) == Occlusion::Draw);
    assert(Step(true, true, true, 4, 512, 1024) == Occlusion::Draw);
    // More than one rectangle (text, 2D pieces, geometry).
    assert(Step(true, true, false, 6, 1024, 1024) == Occlusion::Draw);
    assert(Step(true, true, false, 336, 512, 1024) == Occlusion::Draw);
    assert(Step(true, true, false, 0, 512, 1024) == Occlusion::Draw);
    // A single-channel target of another size (a mask or a small table) is left alone.
    assert(Step(true, true, false, 4, 8, 1024) == Occlusion::Draw);
    assert(Step(true, true, false, 4, 256, 1024) == Occlusion::Draw);
    assert(Step(true, true, false, 4, 2048, 1024) == Occlusion::Draw);
    assert(Step(true, true, false, 4, 1024, 1920) == Occlusion::Draw); // 540p's size in a 1080p scene
    // Without a known scene size nothing is touched.
    assert(Step(true, true, false, 4, 0, 0) == Occlusion::Draw);
    assert(Step(true, true, false, 4, 512, 0) == Occlusion::Draw);

    static_assert(Step(true, true, false, 4, 960, 1920) == Occlusion::Skip);
    static_assert(Step(true, true, false, 4, 1920, 1920) == Occlusion::Open);
    std::puts("PASS: low quality ambient occlusion draws");
    return 0;
}
