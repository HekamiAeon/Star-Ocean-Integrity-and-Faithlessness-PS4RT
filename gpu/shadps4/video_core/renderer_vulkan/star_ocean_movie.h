// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <optional>
#include <span>

// Wide screens: the game's video player draws a recorded movie as one quad over the whole
// screen. On a screen wider than the video it keeps the video's shape, centred, with black
// side areas. At 16:9 nothing changes.
namespace Vulkan::StarOceanMovie {

// One texture the draw samples: size and GCN data format.
struct Plane {
    uint32_t width;
    uint32_t height;
    uint32_t data_format;
};

// The video player's frame: an 8-bit luma plane and two 8-bit chroma planes at half its size
// (4:2:0), sampled in that order. Returns the luma (picture) size.
inline std::optional<std::array<uint32_t, 2>> PlanarVideo(std::span<const Plane> planes) {
    constexpr uint32_t format8 = 1;
    if (planes.size() != 3) return {};
    const Plane& luma = planes[0];
    if (luma.data_format != format8 || luma.width < 2 || luma.height < 2) return {};
    for (size_t i = 1; i < planes.size(); ++i) {
        const Plane& chroma = planes[i];
        if (chroma.data_format != format8 || chroma.width != (luma.width + 1) / 2 ||
            chroma.height != (luma.height + 1) / 2) return {};
    }
    return std::array{luma.width, luma.height};
}

// Four clip-space corners (-1/+1, any order) covering the whole viewport.
inline bool FullscreenQuad(std::span<const std::array<float, 2>> points) {
    if (points.size() != 4) return false;
    unsigned corners = 0;
    for (const auto& p : points) {
        if (!std::isfinite(p[0]) || !std::isfinite(p[1]) ||
            std::abs(std::abs(p[0]) - 1) > 0.0001f || std::abs(std::abs(p[1]) - 1) > 0.0001f)
            return false;
        corners |= 1u << ((p[0] > 0 ? 1 : 0) + (p[1] > 0 ? 2 : 0));
    }
    return corners == 15;
}

struct Layout {
    float x;     // video left edge, target pixels
    float width; // video width, target pixels
    // Black side areas: [left_x, left_x + left_w) and [right_x, right_x + right_w).
    int32_t left_x, left_w, right_x, right_w;
    int32_t top, height;
};

// A full-screen recorded movie keeps its own aspect: scaled to the viewport height and centred,
// with black side areas, when the viewport covers the whole target and is wider than the
// video. Viewport is x, y, width, height in target pixels (height may be negative).
inline std::optional<Layout> Fit(std::array<float, 4> viewport, uint32_t target_width,
                                 std::array<uint32_t, 2> video) {
    for (float v : viewport)
        if (!std::isfinite(v)) return {};
    const float height = std::abs(viewport[3]);
    if (!target_width || !video[0] || !video[1] || height < 1 ||
        std::abs(viewport[0]) > 0.5f || std::abs(viewport[2] - float(target_width)) > 0.5f)
        return {};
    const float width = std::round(height * float(video[0]) / float(video[1]));
    if (width + 1 >= viewport[2]) return {}; // 16:9 (or narrower): unchanged
    const float x = std::round((viewport[2] - width) / 2);
    const int32_t top = int32_t(std::lround(std::min(viewport[1], viewport[1] + viewport[3])));
    const int32_t right_x = int32_t(x + width);
    return Layout{x, width, 0, int32_t(x), right_x, int32_t(target_width) - right_x,
                  std::max(top, 0), int32_t(std::lround(height))};
}

} // namespace Vulkan::StarOceanMovie
