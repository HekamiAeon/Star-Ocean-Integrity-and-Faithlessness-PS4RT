// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
#include <cstdint>
#include <limits>
#include <span>

namespace Vulkan::StarOceanLayers {

// Effects can hold a mesh in front of the camera to tint or warp the whole picture. A battle
// skill's purple tint is a flat 4x4 grid (positions +-5) scaled 35 x 20 and held 16 in front of
// the camera, on 16:9 9% past the frame left and right and 11% above and below; the ending's warp
// is an oval tunnel held 13 to 17 in front whose dark rim reaches 1.7 times the half-width. With
// the wide (Hor+) projection both cover only the middle of the picture: a box, and an oval with
// the plain scene beside it. Such a screen layer is widened about the view axis by the canvas
// stretch, so it covers the wide picture as it covered the 16:9 one.
//
// A screen layer is a draw whose world-view-projection (the first four rows of its first
// vertex-shader constants: clip x, y, depth and w, each applied to (position, 1)) is held in
// front of the 3D camera: object x feeds only clip x, object y only clip y, object z depth and,
// through the camera's perspective, w (the projection the wide screen widened), and the
// object's origin lies on the view axis in front of the camera. Every vertex is in front of the
// camera, and together they cover the authored 16:9 frame on every side. Such a layer is
// widened at every wide size, also when it already reaches past the wide picture's sides: the
// warp's rim reaches 1.13 times the half-width at 24:9, but there its rounded edge runs through
// the screen's corners and its dark band over the picture's sides, which stay bright on 16:9.
// Billboards (off the view axis), camera-held art smaller than the frame (the warp's inner
// tunnel) and flat full-screen passes (w without object z: never narrowed) stay as they are.

using Position = std::array<float, 3>;
using Rows = std::array<float, 16>;

// The matrix part of the test: held in front of the 3D camera, on the view axis.
inline bool HeldOnViewAxis(const Rows& m) {
    for (const float value : m)
        if (!std::isfinite(value)) return false;
    const float* x = m.data();
    const float* y = x + 4;
    const float* w = x + 12;
    const float sx = std::abs(x[0]), sy = std::abs(y[1]);
    const float sw = std::max(std::abs(w[2]), std::abs(w[3]));
    constexpr float other_axis = 1e-4f;
    if (!(sx > 0) || !(sy > 0) || !(w[3] > 0) || !(std::abs(w[2]) > other_axis * w[3]))
        return false;
    if (std::abs(x[1]) > other_axis * sx || std::abs(x[2]) > other_axis * sx ||
        std::abs(y[0]) > other_axis * sy || std::abs(y[2]) > other_axis * sy ||
        std::abs(w[0]) > other_axis * sw || std::abs(w[1]) > other_axis * sw)
        return false;
    constexpr float off_axis = 1e-3f;
    return std::abs(x[3]) <= off_axis * w[3] && std::abs(y[3]) <= off_axis * w[3];
}

// The whole test, for a picture `stretch` times as wide as 16:9 and the positions of every
// vertex the draw uses.
inline bool ScreenLayer(const Rows& m, std::span<const Position> positions, float stretch) {
    if (!(stretch > 1.0005f) || !std::isfinite(stretch) || positions.size() < 3 ||
        !HeldOnViewAxis(m))
        return false;
    const float* x = m.data();
    const float* y = x + 4;
    const float* w = x + 12;
    constexpr float infinity = std::numeric_limits<float>::infinity();
    float x0 = infinity, x1 = -infinity, y0 = infinity, y1 = -infinity;
    for (const auto& p : positions) {
        const float cw = w[0] * p[0] + w[1] * p[1] + w[2] * p[2] + w[3];
        const float cx = x[0] * p[0] + x[1] * p[1] + x[2] * p[2] + x[3];
        const float cy = y[0] * p[0] + y[1] * p[1] + y[2] * p[2] + y[3];
        if (!(cw > 1e-6f * w[3]) || !std::isfinite(cx) || !std::isfinite(cy)) return false;
        x0 = std::min(x0, cx / cw);
        x1 = std::max(x1, cx / cw);
        y0 = std::min(y0, cy / cw);
        y1 = std::max(y1, cy / cw);
    }
    constexpr float edge = 0.999f;
    return x0 * stretch <= -edge && x1 * stretch >= edge && y0 <= -edge && y1 >= edge;
}

// Every vertex in front of the camera (positive w).
inline bool InFront(const Rows& m, std::span<const Position> positions) {
    const float* w = m.data() + 12;
    for (const auto& p : positions) {
        const float cw = w[0] * p[0] + w[1] * p[1] + w[2] * p[2] + w[3];
        if (!(cw > 1e-6f * w[3])) return false;
    }
    return !positions.empty();
}

// The battle skill's tint is drawn as the whole grid and, right after it, a part of the grid
// held by the same matrix (same object, scale and distance); the part alone covers only some of
// the picture. A draw held with the matrix of a layer widened this frame or the last is a piece
// of that layer and is widened with it, whatever part of the picture it covers.
class HeldLayers {
public:
    // A layer was widened (ScreenLayer) with this matrix.
    void Widened(const Rows& rows) {
        if (count < current.size()) current[count++] = rows;
    }
    bool PieceOf(const Rows& rows) const {
        for (size_t i = 0; i < count; ++i)
            if (Same(current[i], rows)) return true;
        for (size_t i = 0; i < previous_count; ++i)
            if (Same(previous[i], rows)) return true;
        return false;
    }
    // The game finished a frame (submitted its work).
    void Submit() {
        previous = current;
        previous_count = count;
        count = 0;
    }

private:
    // Equal up to rounding (each piece's constants are computed on their own).
    static bool Same(const Rows& a, const Rows& b) {
        for (size_t i = 0; i < a.size(); ++i)
            if (!(std::abs(a[i] - b[i]) <= 1e-5f * std::max(std::abs(a[i]), std::abs(b[i])) + 1e-7f))
                return false;
        return true;
    }
    std::array<Rows, 8> current{}, previous{};
    size_t count = 0, previous_count = 0;
};

// 16-bit floats: the effect meshes keep their positions in them.
inline float HalfToFloat(std::uint16_t half) {
    const std::uint32_t sign = std::uint32_t(half & 0x8000u) << 16;
    const std::uint32_t exponent = (half >> 10) & 0x1fu, mantissa = half & 0x3ffu;
    if (exponent == 0) {
        const float value = std::ldexp(float(mantissa), -24);
        return sign ? -value : value;
    }
    if (exponent == 0x1f)
        return std::bit_cast<float>(sign | 0x7f800000u | (mantissa << 13));
    return std::bit_cast<float>(sign | ((exponent + 112) << 23) | (mantissa << 13));
}

// Rounded to nearest, ties to even.
inline std::uint16_t FloatToHalf(float value) {
    std::uint32_t bits = std::bit_cast<std::uint32_t>(value);
    const auto sign = std::uint16_t((bits >> 16) & 0x8000u);
    bits &= 0x7fffffffu;
    if (bits >= 0x7f800000u) return sign | (bits > 0x7f800000u ? 0x7e00u : 0x7c00u);
    if (bits >= 0x477ff000u) return sign | 0x7c00u; // 65520 and more round to infinity
    if (bits < 0x38800000u) {                       // a subnormal half, or zero
        if (bits < 0x33000000u) return sign;
        const std::uint32_t shift = 126 - (bits >> 23);
        const std::uint32_t mantissa = (bits & 0x7fffffu) | 0x800000u;
        std::uint32_t half = mantissa >> shift;
        const std::uint32_t rest = mantissa & ((1u << shift) - 1), halfway = 1u << (shift - 1);
        if (rest > halfway || (rest == halfway && (half & 1u))) ++half;
        return std::uint16_t(sign | half);
    }
    const std::uint32_t rounded = bits + 0xfffu + ((bits >> 13) & 1u);
    return std::uint16_t(sign | ((rounded - 0x38000000u) >> 13));
}

} // namespace Vulkan::StarOceanLayers
