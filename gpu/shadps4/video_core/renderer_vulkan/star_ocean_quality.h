// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <cstdint>

namespace Vulkan::StarOceanQuality {

/// ps4rt: what the Low graphics quality does with a draw of the game's ambient occlusion.
///
/// Star Ocean (CUSA03219) builds its ambient occlusion every frame in four draws, each a
/// rectangle of four vertices over the whole target, and they are its only draws whose first
/// colour target is single channel (R8) with no depth target: a gather and two blur passes at
/// half the scene's size, then an upsample into a scene-sized image that the scene's materials
/// read as a factor (1 is open, 0 is occluded). On a Radeon integrated GPU at 1024x576 the four
/// cost 2.0 to 2.7 ms of a 13 to 17 ms frame. Nothing outside the four reads the half-size images.
///
/// The game's own switch for the effect cannot be used: its materials are then built without the
/// occlusion read, a variant the game does not ship for most of them, and those objects are not
/// drawn at all. So the materials stay as they are and the result they read is made "open":
/// the half-size draws are left out and the scene-sized one becomes a clear to 1.
enum class Occlusion {
    Draw, ///< not part of the effect, or not Low: the draw as the game issued it
    Skip, ///< a gather or blur pass: no draw
    Open, ///< the result: a clear to 1 in place of the draw
};

constexpr Occlusion Step(bool low, bool single_channel_target, bool depth_target, uint32_t vertices,
                         uint32_t target_width, uint32_t scene_width) {
    if (!low || !single_channel_target || depth_target || vertices != 4 || scene_width == 0) {
        return Occlusion::Draw;
    }
    if (target_width == scene_width) return Occlusion::Open;
    return target_width * 2 == scene_width ? Occlusion::Skip : Occlusion::Draw;
}

} // namespace Vulkan::StarOceanQuality
