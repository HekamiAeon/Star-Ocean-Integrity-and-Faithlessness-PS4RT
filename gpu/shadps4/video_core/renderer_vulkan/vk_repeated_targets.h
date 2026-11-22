// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include <algorithm>
#include <array>
#include <span>
#include <unordered_map>

#include "common/types.h"

namespace Vulkan {

/// ps4rt: the render targets a game draws frame after frame. While a draw's shaders or pipeline
/// compile in the background, the draw can be skipped only if all its targets are of this kind:
/// the next frames draw them again, so the skip shows for a frame at most. A target drawn once
/// (a texture the game renders and then keeps, such as Star Ocean's environment cube) is not,
/// and its draw has to wait for the pipeline or the texture stays incomplete.
class RepeatedTargets {
public:
    static constexpr u32 MinFrames = 4; ///< drawn in at least this many frames in a row,
    static constexpr u64 MaxGap = 2;    ///< allowing passes drawn every other frame
    static constexpr size_t MaxTargets = 9; ///< 8 color targets and the depth buffer

    /// Called for every draw with the current frame number and the draw's target addresses.
    /// Returns true when the draw has targets and all of them are repeated.
    bool Note(u64 frame, std::span<const VAddr> targets) {
        if (frame == last_frame && targets.size() == last_count &&
            std::equal(targets.begin(), targets.end(), last.begin())) {
            return last_result;
        }
        bool repeated = !targets.empty();
        for (const VAddr address : targets) {
            auto& entry = history[address];
            if (entry.frames == 0 || entry.last_frame != frame) {
                entry.frames = entry.frames && frame - entry.last_frame <= MaxGap ? entry.frames + 1 : 1;
                entry.last_frame = frame;
            }
            repeated &= entry.frames >= MinFrames;
        }
        last_count = std::min(targets.size(), MaxTargets); // a longer list never matches again
        std::copy_n(targets.begin(), last_count, last.begin());
        last_frame = frame;
        last_result = repeated;
        if (frame >= pruned_frame + PruneFrames) {
            std::erase_if(history, [frame](const auto& item) {
                return item.second.last_frame + PruneFrames < frame;
            });
            pruned_frame = frame;
        }
        return repeated;
    }

private:
    static constexpr u64 PruneFrames = 600;
    struct History {
        u64 last_frame = 0;
        u32 frames = 0;
    };
    std::unordered_map<VAddr, History> history;
    std::array<VAddr, MaxTargets> last{};
    size_t last_count = 0;
    u64 last_frame = ~0ull;
    bool last_result = false;
    u64 pruned_frame = 0;
};

} // namespace Vulkan
