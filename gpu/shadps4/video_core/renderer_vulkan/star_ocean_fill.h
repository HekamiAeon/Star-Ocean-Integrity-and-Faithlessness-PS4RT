// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <optional>
#include <span>
#include <utility>
#include <vector>

// Wide screens: the game's 2D layer stays a 1920x1080 canvas, shown in a centred 16:9 area.
// Plain colour quads that cover that whole canvas are screen fills (black backdrops behind
// splash pictures, fades, dimmed backgrounds); on a wide screen they must cover the whole
// screen, not the 16:9 area. Pictures keep their 16:9 shape, except the game's transition
// picture and backgrounds the game draws see-through (StretchedCanvas, Backgrounds); the
// loading screen's corner art goes into the screen's corner (CoverEdge, EdgeArt), the
// prompts hung off the right edge to the screen's right edge (EdgeTabs), and the pause
// menu's panel across the whole screen (Backdrop).
namespace Vulkan::StarOceanFill {

constexpr float canvas_width = 1920;
constexpr float canvas_height = 1080;

enum class Space {
    None,
    Pixel,  // engine 2D pixel transform of the target: x * 2/W - 1, y * -2/H + 1
    Canvas, // UI camera: the canvas centred at the target's height (16:9 area)
};

using Point = std::array<float, 2>;

inline bool Near(float a, float b, float tolerance) {
    return std::isfinite(a) && std::abs(a - b) <= tolerance;
}

// Which 2D transform the draw's first vertex-shader constants hold, for a W x H target.
// Pixel: (2/W, -2/H, 1, 1, -1, 1, ...). Canvas: the row-major UI matrix
// (2/Wc, 0, 0, -1920/Wc, 0, -2/1080, 0, 1) with Wc = 1080 * W / H.
inline Space Classify(std::span<const float> c, float width, float height) {
    if (c.size() < 8 || !(width > 0) || !(height > 0)) return Space::None;
    const auto rel = [](float a, float b) { return Near(a, b, std::abs(b) * 0.0005f); };
    if (rel(c[0], 2 / width) && rel(c[1], -2 / height) && Near(c[2], 1, 1e-4f) &&
        Near(c[3], 1, 1e-4f) && Near(c[4], -1, 1e-4f) && Near(c[5], 1, 1e-4f))
        return Space::Pixel;
    const float visible = canvas_height * width / height;
    if (rel(c[0], 2 / visible) && Near(c[1], 0, 1e-7f) && Near(c[2], 0, 1e-7f) &&
        rel(c[3], -canvas_width / visible) && Near(c[4], 0, 1e-7f) &&
        rel(c[5], -2 / canvas_height) && Near(c[6], 0, 1e-7f) && Near(c[7], 1, 1e-4f))
        return Space::Canvas;
    return Space::None;
}

// New corner positions for a full-canvas fill quad on a W x H target, or nothing when the quad
// is not one or the target is 16:9 (nothing to widen).
inline std::optional<std::array<Point, 4>> Widen(Space space, std::span<const Point> points,
                                                 float width, float height) {
    if (points.size() != 4 || !(width > 0) || !(height > 0)) return {};
    float x0 = points[0][0], x1 = x0, y0 = points[0][1], y1 = y0;
    for (const auto& p : points) {
        if (!std::isfinite(p[0]) || !std::isfinite(p[1])) return {};
        x0 = std::min(x0, p[0]); x1 = std::max(x1, p[0]);
        y0 = std::min(y0, p[1]); y1 = std::max(y1, p[1]);
    }
    // An axis-aligned rectangle: every corner on its edges.
    for (const auto& p : points)
        if ((p[0] != x0 && p[0] != x1) || (p[1] != y0 && p[1] != y1)) return {};
    std::array<Point, 4> out{};
    if (space == Space::Pixel) {
        // Exactly the canvas, in pixels of a larger target: scaled to the target.
        constexpr float tol = 0.01f;
        if (!Near(x0, 0, tol) || !Near(y0, 0, tol) || !Near(x1, canvas_width, tol) ||
            !Near(y1, canvas_height, tol))
            return {};
        if (Near(width, canvas_width, 0.5f) && Near(height, canvas_height, 0.5f)) return {};
        for (size_t i = 0; i < 4; ++i)
            out[i] = {points[i][0] * width / canvas_width, points[i][1] * height / canvas_height};
        return out;
    }
    if (space == Space::Canvas) {
        // Spans the canvas width (a little overdraw allowed): widened about the centre.
        if (x0 > 0 || x0 < -8 || x1 < canvas_width || x1 > canvas_width + 8) return {};
        const float stretch = (width / height) / (canvas_width / canvas_height);
        if (stretch < 1.0005f) return {};
        const float centre = canvas_width / 2;
        for (size_t i = 0; i < 4; ++i)
            out[i] = {centre + (points[i][0] - centre) * stretch, points[i][1]};
        return out;
    }
    return {};
}

// A picture (textured quad) covering the whole canvas is 16:9-only art: on a wide screen it
// keeps its shape in the centred 16:9 area with black side areas, as on 16:9 nothing behind
// it shows (see Backgrounds for the exception). Texel-centre insets (up to half a pixel) are
// allowed.
inline bool CanvasPicture(Space space, std::span<const Point> points) {
    if (space != Space::Canvas || points.size() != 4) return false;
    unsigned corners = 0;
    for (const auto& p : points) {
        const bool left = Near(p[0], 0, 0.5f), right = Near(p[0], canvas_width, 0.5f);
        const bool top = Near(p[1], 0, 0.5f), bottom = Near(p[1], canvas_height, 0.5f);
        if ((!left && !right) || (!top && !bottom)) return false;
        corners |= 1u << ((right ? 1 : 0) + (bottom ? 2 : 0));
    }
    return corners == 15;
}

// The viewport (x, width) that draws the whole canvas across a W-pixel-wide viewport of the
// given aspect instead of in its centred 16:9 area. For the game's screen-transition picture:
// the game copies the whole screen into a 1920x1080 picture and fades it out over the canvas,
// so on a wide screen the copy holds the wide view squeezed to 16:9. Nothing on a 16:9 or
// narrower viewport, or when the device's widest viewport is too small.
inline std::optional<std::array<float, 2>> StretchedCanvas(float viewport_width, float aspect,
                                                           float max_width) {
    const float stretch = aspect / (canvas_width / canvas_height);
    if (!(viewport_width > 0) || !std::isfinite(stretch) || stretch < 1.0005f) return {};
    const float width = viewport_width * stretch;
    if (width > max_width) return {};
    return std::array<float, 2>{(viewport_width - width) / 2, width};
}

// A quad whose corners span no area draws nothing (the game parks unused UI quads on one
// point off the canvas).
inline bool Degenerate(std::span<const Point> points) {
    if (points.empty()) return true;
    float x0 = points[0][0], x1 = x0, y0 = points[0][1], y1 = y0;
    for (const auto& p : points) {
        if (!std::isfinite(p[0]) || !std::isfinite(p[1])) return false;
        x0 = std::min(x0, p[0]); x1 = std::max(x1, p[0]);
        y0 = std::min(y0, p[1]); y1 = std::max(y1, p[1]);
    }
    return x1 <= x0 || y1 <= y0;
}

// What a full-canvas picture shows: its texture's size and the texture region it covers.
struct PictureLook {
    unsigned width{}, height{};    ///< texture size, texels
    std::array<float, 4> region{}; ///< texture coordinates: left, top, right, bottom
    bool operator==(const PictureLook& other) const {
        if (width != other.width || height != other.height) return false;
        for (size_t i = 0; i < region.size(); ++i)
            if (!Near(region[i], other.region[i], 1e-4f)) return false;
        return true;
    }
};

// The main menu's background is a full-canvas picture that fades in see-through (vertex alpha
// below 1) over the field, followed at once by layers from the same texture (the menu's
// sparkles, glow and scan lines). Black side areas would cover the field at once and switch
// the screen's shape, so it is a background: stretched across the whole width
// (StretchedCanvas) with its layers, up to the first visible draw from another texture, and
// still once it is opaque, so the menu never switches to black sides.
// The ending's character pictures also fade in and out see-through, but no layer follows them
// (the next visible draws are the subtitles and the HUD): they are 16:9 art like the splash
// logos (only ever opaque, faded through black) and keep the 16:9 shape. A see-through picture
// of a new look is stretched for its first frame, the background until shown otherwise, and
// its look is remembered as a background once a layer follows it, or as a picture when another
// draw comes first.
class Backgrounds {
public:
    // A full-canvas picture from `texture`: true when it is drawn across the whole width.
    bool Picture(const PictureLook& look, unsigned long long texture, bool see_through) {
        Settle(false);
        bool background = Has(looks, look);
        if (!background && see_through && !Has(pictures, look)) {
            pending = look;
            undecided = background = true;
        }
        layers = background ? texture : 0;
        return background;
    }
    // A later draw into the wide target, from `texture` (0: none): true when it is a layer of
    // the background just drawn. A visible draw from another texture ends the layers.
    bool Layer(unsigned long long texture, bool hidden) {
        if (layers && texture == layers) {
            if (!hidden) Settle(true);
            return true;
        }
        if (!hidden) EndLayers();
        return false;
    }
    void EndLayers() {
        Settle(false);
        layers = 0;
    }

private:
    using Looks = std::array<PictureLook, 16>;
    static bool Has(const std::pair<Looks, size_t>& table, const PictureLook& look) {
        return std::find(table.first.begin(), table.first.begin() + table.second, look) !=
               table.first.begin() + table.second;
    }
    // Remembers the undecided picture's look as a background (a layer followed it) or as a
    // picture; a full table remembers nothing more, and such pictures are decided each frame.
    void Settle(bool layer) {
        if (!undecided) return;
        undecided = false;
        auto& table = layer ? looks : pictures;
        if (table.second < table.first.size()) table.first[table.second++] = pending;
    }
    std::pair<Looks, size_t> looks{};    ///< backgrounds
    std::pair<Looks, size_t> pictures{}; ///< see-through pictures without layers
    PictureLook pending{};               ///< the see-through picture awaiting its layers
    bool undecided = false;
    unsigned long long layers = 0; ///< texture of the background whose layers may follow
};

// The game draws its 2D canvas (text, icons, panels) with nearest filtering: at 1920x1080 each
// texel is one pixel. At another internal resolution the canvas is scaled (1.33x at 1440
// lines), where nearest filtering repeats some texel rows and columns and not others, so text
// looks blocky. Canvas draws there are filtered linearly. `viewport_height` in guest pixels.
inline bool SmoothCanvas(Space space, float viewport_height) {
    return space == Space::Canvas && viewport_height > 0 && std::isfinite(viewport_height) &&
           std::abs(viewport_height - canvas_height) > 0.5f;
}

// How many canvas pixels the most enlarged piece of a 2D draw gives each texel, along the edges
// of its primitives (`corners` vertices each: 4 for quads, 3 for triangles; `texels` are
// texture coordinates times the texture size). Pieces with no area on the canvas (parked quads)
// are skipped; 0 when nothing can be measured, infinity for a piece showing one texel row.
inline float Enlargement(std::span<const Point> positions, std::span<const Point> texels,
                         size_t corners) {
    if (corners < 3 || positions.size() != texels.size()) return 0;
    float most = 0;
    for (size_t first = 0; first + corners <= positions.size(); first += corners) {
        for (size_t i = 0; i < corners; ++i) {
            const auto& p0 = positions[first + i];
            const auto& p1 = positions[first + (i + 1) % corners];
            const auto& t0 = texels[first + i];
            const auto& t1 = texels[first + (i + 1) % corners];
            const float pixels = std::hypot(p1[0] - p0[0], p1[1] - p0[1]);
            const float span = std::hypot(t1[0] - t0[0], t1[1] - t0[1]);
            if (!std::isfinite(pixels) || !std::isfinite(span))
                return std::numeric_limits<float>::infinity();
            if (pixels < 0.5f) continue;
            most = std::max(most, span > 0 ? pixels / span : std::numeric_limits<float>::infinity());
        }
    }
    return most;
}

// Linear filtering only suits art drawn at about its own size. The game also fills areas with a
// few texels of one colour blown up hundreds of times (the loading screen's black cover is
// three pieces of one 9x9-texel patch); linear filtering blends their edges with the texels
// around the patch, which shows the pieces as faint rectangles. Draws with such a piece keep
// the game's nearest filter.
constexpr float smooth_enlargement = 4;

// The loading screen (black, with a glow and "Now Loading" in the bottom-right corner) is drawn
// from one texture: the glow, then one draw of three quads that covers the whole canvas in
// black (two pieces of a 9x9-texel patch, enlarged 213 times) around a window of art drawn at
// its own size in the bottom-right corner, then the text. The glow runs off the canvas's right
// edge (up to x = 2539) and was cut by the screen edge on a 16:9 screen; in the centred 16:9
// area it would run out into the black side area. So a draw like that cover gives the canvas
// edge its window touches, and everything drawn from its texture is drawn with the canvas
// against that edge of the screen: the corner art sits in the screen's corner and runs off the
// screen edge as on 16:9 (EdgeArt). The window is a hole in the cover: no solid piece lies
// under it. The pause menu's panel is a solid piece over the whole canvas with a bar drawn on
// top of it, at the right edge too; that is a Backdrop, not a cover.
enum class Edge { None, Left, Right };

struct Rect {
    float x0{}, y0{}, x1{}, y1{};
};

// The rectangle a quad's corners span; nothing when a corner is not finite.
inline std::optional<Rect> Bounds(std::span<const Point> piece) {
    if (piece.empty()) return {};
    Rect area{piece[0][0], piece[0][1], piece[0][0], piece[0][1]};
    for (const auto& p : piece) {
        if (!std::isfinite(p[0]) || !std::isfinite(p[1])) return {};
        area = {std::min(area.x0, p[0]), std::min(area.y0, p[1]), std::max(area.x1, p[0]),
                std::max(area.y1, p[1])};
    }
    return area;
}

inline float Overlap(const Rect& a, const Rect& b) {
    return std::max(0.0f, std::min(a.x1, b.x1) - std::max(a.x0, b.x0)) *
           std::max(0.0f, std::min(a.y1, b.y1) - std::max(a.y0, b.y0));
}

inline Edge CoverEdge(std::span<const Point> positions, std::span<const Point> texels) {
    constexpr size_t corners = 4;
    if (positions.size() != texels.size() || positions.size() < 2 * corners) return Edge::None;
    constexpr Rect canvas{0, 0, canvas_width, canvas_height};
    float area = 0;
    std::vector<Rect> solids, windows;
    Rect all{canvas_width, canvas_height, 0, 0};
    for (size_t first = 0; first + corners <= positions.size(); first += corners) {
        const auto piece = positions.subspan(first, corners);
        if (Degenerate(piece)) continue;
        const auto bounds = Bounds(piece);
        if (!bounds) return Edge::None;
        all = {std::min(all.x0, bounds->x0), std::min(all.y0, bounds->y0),
               std::max(all.x1, bounds->x1), std::max(all.y1, bounds->y1)};
        area += Overlap(*bounds, canvas);
        if (Enlargement(piece, texels.subspan(first, corners), corners) > smooth_enlargement)
            solids.push_back(*bounds);
        else
            windows.push_back(*bounds);
    }
    if (solids.empty() || windows.empty() || all.x0 > 0.5f || all.y0 > 0.5f ||
        all.x1 < canvas_width - 0.5f || all.y1 < canvas_height - 0.5f ||
        area < canvas_width * canvas_height * 0.99f)
        return Edge::None;
    float window_x0 = canvas_width, window_x1 = 0;
    for (const auto& window : windows) {
        for (const auto& solid : solids)
            if (Overlap(window, solid) > 0.01f * Overlap(window, window)) return Edge::None;
        window_x0 = std::min(window_x0, window.x0);
        window_x1 = std::max(window_x1, window.x1);
    }
    const bool left = window_x0 <= 0.5f, right = window_x1 >= canvas_width - 0.5f;
    return left == right ? Edge::None : right ? Edge::Right : Edge::Left;
}

// The texture of the last such cover and its edge. The loading screen draws its cover alone
// for a second or more (while the screen is black) before the glow fades in, so the glow is
// placed from its first visible frame. Forgotten once the game has finished `forget` frames
// without the cover, so a texture later loaded at the same address is drawn normally.
class EdgeArt {
public:
    static constexpr unsigned forget = 3;

    void Cover(unsigned long long texture, unsigned width, unsigned height, Edge edge) {
        if (edge == Edge::None || !texture) return;
        art = {texture, width, height, edge};
        idle = 0;
    }
    // The edge a draw from `texture` (width x height texels) is drawn against.
    Edge Draw(unsigned long long texture, unsigned width, unsigned height) const {
        return art.texture && texture == art.texture && width == art.width &&
                       height == art.height
                   ? art.edge
                   : Edge::None;
    }
    // The game finished a frame (submitted its work).
    void Submit() {
        if (art.texture && ++idle >= forget) art = {};
    }

private:
    struct Art {
        unsigned long long texture{};
        unsigned width{}, height{};
        Edge edge{};
    } art;
    unsigned idle = 0;
};

// The save and talk prompts are tabs hung off the screen's right edge: a 768-pixel bar resting at
// x 1682..2450 (it slides in from the edge), of which a 16:9 screen shows the first 238 pixels,
// with the button icon and the text drawn over it. In the centred 16:9 area the whole bar would
// show, floating past the area's edge. Art drawn at its own size (smooth_enlargement) that
// starts in the right half of the canvas and runs past its right edge is such a tab; it is drawn
// against the screen's right edge, and so are the draws the game then puts inside it. Other art
// that runs past the edge does not start in the right half (the menu's description line is a
// 2048-pixel text strip from x 92, the menu background's layers cross both edges) or is blown
// up (the minimap's glow).
class EdgeTabs {
public:
    // A canvas draw's quads: true when the draw goes against the screen's right edge.
    bool Draw(std::span<const Point> positions, std::span<const Point> texels) {
        constexpr size_t corners = 4;
        if (positions.size() != texels.size()) return false;
        bool visible = false, tabs = true, inside = true;
        for (size_t first = 0; first + corners <= positions.size(); first += corners) {
            const auto piece = positions.subspan(first, corners);
            if (Degenerate(piece)) continue;
            const auto area = Bounds(piece);
            if (!area) return false;
            visible = true;
            tabs = tabs && area->x0 >= canvas_width / 2 && area->x1 > canvas_width + 8 &&
                   Enlargement(piece, texels.subspan(first, corners), corners) <=
                       smooth_enlargement;
            inside = inside && Inside(*area);
        }
        if (!visible) return false;
        if (tabs) {
            for (size_t first = 0; first + corners <= positions.size(); first += corners) {
                const auto piece = positions.subspan(first, corners);
                if (!Degenerate(piece)) Hang(*Bounds(piece));
            }
            return true;
        }
        return inside;
    }
    // A tab drawn within another draw (Backdrop): the draws inside it go to the edge too.
    void Hang(const Rect& tab) {
        if (count < current.size()) current[count++] = tab;
    }
    // The game finished a frame (submitted its work). Tabs of this frame and the last one hold
    // the draws inside them.
    void Submit() {
        previous = current;
        previous_count = count;
        count = 0;
    }

private:
    // Within a tab's width, its middle line within the tab's height.
    bool Inside(const Rect& area) const {
        const float middle = (area.y0 + area.y1) / 2;
        const auto within = [&](const Rect& tab) {
            return area.x0 >= tab.x0 - 0.5f && area.x1 <= tab.x1 + 0.5f && middle >= tab.y0 &&
                   middle <= tab.y1;
        };
        for (size_t i = 0; i < count; ++i)
            if (within(current[i])) return true;
        for (size_t i = 0; i < previous_count; ++i)
            if (within(previous[i])) return true;
        return false;
    }
    std::array<Rect, 8> current{}, previous{};
    size_t count = 0, previous_count = 0;
};

// The pause menu draws its dark panel and the bar along its bottom as one draw from its UI
// texture: a few texels of one colour blown up over the whole canvas (with a little overdraw),
// then the bar's three pieces on top, running off the right edge like the prompts (EdgeTabs).
// In the centred 16:9 area the panel would end at the area's edges and leave the game beside
// it undimmed. So a solid piece (enlarged more than smooth_enlargement) covering the whole
// canvas is a backdrop and is widened across the whole screen, as on 16:9; when the draw's
// other pieces together start in the right half and run past the right edge, they are a bar
// hung off that edge and go against the screen's right edge (with the draws inside them,
// EdgeTabs::Hang). Other pieces stay where they are. A single quad exactly on the canvas is a
// picture (CanvasPicture) and is left to those rules.
enum class Piece : unsigned char { Keep, Widen, Right };

struct BackdropLayout {
    std::vector<Piece> pieces; ///< one per quad, in draw order
    std::optional<Rect> tab;   ///< the pieces that go against the right edge
};

inline std::optional<BackdropLayout> Backdrop(std::span<const Point> positions,
                                              std::span<const Point> texels) {
    constexpr size_t corners = 4;
    if (positions.size() != texels.size() || positions.size() < corners ||
        positions.size() % corners)
        return {};
    if (positions.size() == corners && CanvasPicture(Space::Canvas, positions)) return {};
    BackdropLayout out;
    out.pieces.assign(positions.size() / corners, Piece::Keep);
    bool backdrop = false, others = false;
    Rect rest{canvas_width, canvas_height, 0, 0};
    for (size_t first = 0; first < positions.size(); first += corners) {
        const auto piece = positions.subspan(first, corners);
        if (Degenerate(piece)) continue;
        const auto area = Bounds(piece);
        if (!area) return {};
        if (area->x0 <= 0.5f && area->y0 <= 0.5f && area->x1 >= canvas_width - 0.5f &&
            area->y1 >= canvas_height - 0.5f &&
            Enlargement(piece, texels.subspan(first, corners), corners) > smooth_enlargement) {
            out.pieces[first / corners] = Piece::Widen;
            backdrop = true;
        } else {
            others = true;
            rest = {std::min(rest.x0, area->x0), std::min(rest.y0, area->y0),
                    std::max(rest.x1, area->x1), std::max(rest.y1, area->y1)};
        }
    }
    if (!backdrop) return {};
    if (others && rest.x0 >= canvas_width / 2 && rest.x1 > canvas_width + 8) {
        out.tab = rest;
        for (size_t i = 0; i < out.pieces.size(); ++i) {
            const auto piece = positions.subspan(i * corners, corners);
            if (out.pieces[i] == Piece::Keep && !Degenerate(piece)) out.pieces[i] = Piece::Right;
        }
    }
    return out;
}

// A backdrop piece's canvas x on a target `stretch` times as wide as 16:9: widened about the
// centre, or moved by the side area's width (in canvas units) against the right edge.
inline float BackdropX(Piece piece, float x, float stretch) {
    const float centre = canvas_width / 2;
    return piece == Piece::Widen   ? centre + (x - centre) * stretch
           : piece == Piece::Right ? x + centre * (stretch - 1)
                                   : x;
}

// The viewport x (the game's is 0) that draws the canvas against `edge` of a target with
// `side`-pixel side areas, instead of in its centred 16:9 area.
inline float EdgeViewportX(Edge edge, float side) {
    return edge == Edge::Right ? side : edge == Edge::Left ? -side : 0;
}

// Width of each black side area beside the centred 16:9 area (0 on a 16:9 or narrower target).
inline unsigned SideWidth(unsigned width, unsigned height) {
    const double area = double(height) * canvas_width / canvas_height;
    return area + 1 < width ? unsigned((width - area) / 2 + 0.5) : 0;
}

} // namespace Vulkan::StarOceanFill
