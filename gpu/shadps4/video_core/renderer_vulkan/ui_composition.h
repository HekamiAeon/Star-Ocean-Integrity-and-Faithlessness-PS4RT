// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include <array>
#include <cmath>
#include <cstdint>
#include "video_core/renderer_vulkan/scene_resolution.h"

namespace Vulkan::UiComposition {

enum class Background { None, Copy, Temporal };

// UI composition must not require scene depth/camera or an active FSR context.
constexpr Background Choose(bool scaled, bool ui_draw, bool scene_ready, bool fsr_active) {
    if (!scaled || !ui_draw) {
        return Background::None;
    }
    return scene_ready && fsr_active ? Background::Temporal : Background::Copy;
}

// A viewport covering the game's frame (the HUD's display buffer).
inline bool NativeViewport(float width, float height) {
    return std::abs(std::abs(width) - float(SceneResolution::game_frame.width)) < 0.5f &&
           std::abs(std::abs(height) - float(SceneResolution::game_frame.height)) < 0.5f;
}
// A viewport covering the 3D scene (the same size as the frame in this port).
inline bool SceneViewport(float width, float height) {
    return std::abs(std::abs(width) - float(SceneResolution::game_scene.width)) < 0.5f &&
           std::abs(std::abs(height) - float(SceneResolution::game_scene.height)) < 0.5f;
}

// Scaleform draws, including the first stencil/movie pass. A native-size viewport
// alone also matches every fullscreen post pass when guest targets stay at 1080p.
constexpr bool MovieShader(uint64_t hash) {
    return hash == 0x34e8a281 || hash == 0x81d336ce || hash == 0x09957251 ||
           hash == 0x24042a9b || hash == 0xa400228b;
}

inline std::array<float, 2> Scale(uint32_t guest_width, uint32_t guest_height,
                                uint32_t output_width, uint32_t output_height,
                                bool native_coordinates) {
    return {float(output_width) / float(native_coordinates ? SceneResolution::game_frame.width : guest_width),
            float(output_height) / float(native_coordinates ? SceneResolution::game_frame.height : guest_height)};
}

} // namespace Vulkan::UiComposition
