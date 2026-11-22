// SPDX-License-Identifier: GPL-2.0-or-later
// Which draws may be skipped while their pipeline compiles (vk_repeated_targets.h): only
// draws into targets the game draws frame after frame. Synthetic addresses only.
#include <cassert>
#include <cstdio>
#include <vector>

#include "video_core/renderer_vulkan/vk_repeated_targets.h"

using Vulkan::RepeatedTargets;

static bool Draw(RepeatedTargets& targets, u64 frame, std::vector<VAddr> addresses) {
    return targets.Note(frame, addresses);
}

int main() {
    constexpr VAddr Scene = 0x1000000, Depth = 0x2000000, Cube = 0x3000000, Shadow = 0x4000000;

    // A scene target drawn every frame becomes skippable after MinFrames frames in a row,
    // and stays so for every draw of the frame.
    {
        RepeatedTargets targets;
        for (u64 frame = 1; frame < RepeatedTargets::MinFrames; ++frame) {
            assert(!Draw(targets, frame, {Scene, Depth}));
        }
        assert(Draw(targets, RepeatedTargets::MinFrames, {Scene, Depth}));
        assert(Draw(targets, RepeatedTargets::MinFrames, {Scene, Depth}));
        assert(Draw(targets, RepeatedTargets::MinFrames, {Scene}));
    }
    // A texture rendered once (Star Ocean's environment cube, several draws in one frame) is
    // never skippable, also when a later frame renders it again.
    {
        RepeatedTargets targets;
        for (int face = 0; face < 6; ++face) {
            assert(!Draw(targets, 100, {Cube + face * 0x8000}));
        }
        assert(!Draw(targets, 400, {Cube}));
    }
    // A draw with a new target next to repeated ones waits; so does a draw without targets.
    {
        RepeatedTargets targets;
        for (u64 frame = 1; frame <= 8; ++frame) {
            Draw(targets, frame, {Scene, Depth});
        }
        assert(!Draw(targets, 8, {Scene, Cube}));
        assert(!Draw(targets, 8, {}));
        assert(Draw(targets, 9, {Scene, Depth}));
    }
    // A pass drawn every other frame counts as repeated; a gap of more frames restarts it.
    {
        RepeatedTargets targets;
        bool repeated = false;
        for (u64 frame = 2; frame <= 2 * RepeatedTargets::MinFrames; frame += 2) {
            repeated = Draw(targets, frame, {Shadow});
        }
        assert(repeated);
        assert(!Draw(targets, 2 * RepeatedTargets::MinFrames + 3, {Shadow}));
    }
    // Old entries are dropped without changing the answer for live targets.
    {
        RepeatedTargets targets;
        for (u64 frame = 1; frame <= 2000; ++frame) {
            Draw(targets, frame, {Scene + frame * 0x100000}); // a fresh target every frame
            assert(Draw(targets, frame, {Scene}) == (frame >= RepeatedTargets::MinFrames));
        }
    }
    std::puts("PASS: repeated targets");
    return 0;
}
