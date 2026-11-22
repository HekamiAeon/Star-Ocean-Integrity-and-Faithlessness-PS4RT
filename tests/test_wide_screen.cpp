// SPDX-License-Identifier: GPL-2.0-or-later
// Wide-screen 2D rules: screen fills, full-canvas pictures, menu backgrounds, the loading
// screen, prompts and recorded movies. Owned fixtures shaped like the traced draws (splash
// backdrop and background, splash picture, menu background and its layers, loading screen
// cover, save prompt, intro movie); no game data.
#include <cassert>
#include <cmath>
#include <cstdio>
#include <span>
#include <vector>
#include "video_core/renderer_vulkan/star_ocean_fill.h"
#include "video_core/renderer_vulkan/star_ocean_movie.h"

namespace {

using namespace Vulkan::StarOceanFill;
namespace Movie = Vulkan::StarOceanMovie;

bool Close(Point a, Point b) { return std::abs(a[0] - b[0]) < 0.01f && std::abs(a[1] - b[1]) < 0.01f; }

std::array<float, 8> PixelConstants(float w, float h) { return {2 / w, -2 / h, 1, 1, -1, 1, 0, 0}; }
std::array<float, 8> CanvasConstants(float w, float h) {
    const float visible = 1080 * w / h;
    return {2 / visible, 0, 0, -1920 / visible, 0, -2.f / 1080, 0, 1};
}

bool Close(float a, float b) { return std::abs(a - b) < 0.01f; }

void FillTests() {
    // Classification at 5120x1440 (values as traced) and other sizes.
    const std::array<float, 8> traced_pixel{0.000390625f, -0.00138889f, 1, 1, -1, 1, 0, 0};
    const std::array<float, 8> traced_canvas{0.000520833f, 0, 0, -0.5f, 0, -0.00185185f, 0, 1};
    assert(Classify(traced_pixel, 5120, 1440) == Space::Pixel);
    assert(Classify(traced_canvas, 5120, 1440) == Space::Canvas);
    assert(Classify(PixelConstants(3440, 1440), 3440, 1440) == Space::Pixel);
    assert(Classify(CanvasConstants(3440, 1440), 3440, 1440) == Space::Canvas);
    assert(Classify(CanvasConstants(2560, 1080), 2560, 1080) == Space::Canvas);
    // A transform for another size, a 3D-like matrix, or too few values: none.
    assert(Classify(PixelConstants(1920, 1080), 5120, 1440) == Space::None);
    const std::array<float, 8> other{1.2f, 0, 0.1f, 0, 0, 2.1f, 0, 0};
    assert(Classify(other, 5120, 1440) == Space::None);
    assert(Classify(std::span<const float>(traced_pixel.data(), 4), 5120, 1440) == Space::None);

    // Pixel-space backdrop: canvas-size quad scaled to the screen.
    const Point backdrop[4] = {{0, 0}, {0, 1080}, {1920, 1080}, {1920, 0}};
    auto out = Widen(Space::Pixel, backdrop, 5120, 1440);
    assert(out && Close((*out)[0], {0, 0}) && Close((*out)[1], {0, 1440}) &&
           Close((*out)[2], {5120, 1440}) && Close((*out)[3], {5120, 0}));
    out = Widen(Space::Pixel, backdrop, 7680, 2160);
    assert(out && Close((*out)[2], {7680, 2160}));
    assert(!Widen(Space::Pixel, backdrop, 1920, 1080)); // stock size: unchanged
    const Point partial[4] = {{0, 0}, {0, 540}, {960, 540}, {960, 0}};
    assert(!Widen(Space::Pixel, partial, 5120, 1440)); // only exact full-canvas quads

    // Canvas fill: widened about the canvas centre to the visible width.
    out = Widen(Space::Canvas, backdrop, 5120, 1440); // 32:9: visible canvas -960..2880
    assert(out && Close((*out)[0], {-960, 0}) && Close((*out)[2], {2880, 1080}));
    out = Widen(Space::Canvas, backdrop, 3440, 1440); // 21:9
    assert(out && Close((*out)[0], {-330, 0}) && Close((*out)[2], {2250, 1080}));
    const Point overdraw[4] = {{-1, -1}, {-1, 1081}, {1921, 1081}, {1921, -1}};
    out = Widen(Space::Canvas, overdraw, 5120, 1440);
    assert(out && Close((*out)[0], {-962, -1}) && Close((*out)[2], {2882, 1081}));
    const Point strip[4] = {{0, 900}, {0, 1080}, {1920, 1080}, {1920, 900}}; // full-width bar
    out = Widen(Space::Canvas, strip, 5120, 1440);
    assert(out && Close((*out)[0], {-960, 900}) && Close((*out)[2], {2880, 1080}));
    assert(!Widen(Space::Canvas, backdrop, 1920, 1080));
    assert(!Widen(Space::Canvas, backdrop, 2560, 1440)); // 16:9
    assert(!Widen(Space::Canvas, partial, 5120, 1440));  // panel inside the canvas
    const Point far[4] = {{-200, 0}, {-200, 1080}, {2120, 1080}, {2120, 0}};
    assert(!Widen(Space::Canvas, far, 5120, 1440));
    const Point skew[4] = {{0, 0}, {10, 1080}, {1920, 1080}, {1920, 0}};
    assert(!Widen(Space::Canvas, skew, 5120, 1440));
    assert(!Widen(Space::None, backdrop, 5120, 1440));
    // Full-canvas pictures (traced splash picture: texel-centre insets).
    const Point picture[4] = {{0.0278931f, 0.015686f}, {0.0278931f, 1079.98f}, {1919.97f, 1079.98f}, {1919.97f, 0.015686f}};
    assert(CanvasPicture(Space::Canvas, picture));
    assert(!CanvasPicture(Space::Pixel, picture));
    assert(!CanvasPicture(Space::Canvas, partial));
    assert(!CanvasPicture(Space::Canvas, overdraw)); // overdrawn fill, not a picture
    const Point logo[4] = {{760, 400}, {760, 680}, {1160, 680}, {1160, 400}};
    assert(!CanvasPicture(Space::Canvas, logo));
    assert(SideWidth(5120, 1440) == 1280 && SideWidth(3440, 1440) == 440 && SideWidth(2560, 1080) == 320);
    assert(SideWidth(1920, 1080) == 0 && SideWidth(2560, 1440) == 0);
    // The transition picture: the canvas is drawn across the whole viewport width, centred.
    const auto near = [](float a, float b) { return std::abs(a - b) < 0.01f; };
    const auto s32x9 = StretchedCanvas(5120, 5120.0f / 1440, 16384);
    assert(s32x9 && near((*s32x9)[0], -2560) && near((*s32x9)[1], 10240));
    const auto s21x9 = StretchedCanvas(3440, 3440.0f / 1440, 16384);
    assert(s21x9 && near((*s21x9)[1], 3440 * (3440.0f / 1440) / (16.0f / 9)) &&
           near((*s21x9)[0] * 2 + (*s21x9)[1], 3440));
    assert(!StretchedCanvas(1920, 16.0f / 9, 16384) && !StretchedCanvas(2560, 2560.0f / 1440, 16384));
    assert(!StretchedCanvas(5120, 5120.0f / 1440, 8192)); // wider than the device allows
    assert(!StretchedCanvas(0, 3.5f, 16384) && !StretchedCanvas(5120, NAN, 16384));
    // Linear filtering for the scaled 2D canvas only: 1440 and 720 lines yes, 1080 lines (the
    // original size) and other transforms no.
    assert(SmoothCanvas(Space::Canvas, 1440) && SmoothCanvas(Space::Canvas, 720) &&
           SmoothCanvas(Space::Canvas, 2160) && SmoothCanvas(Space::Canvas, 576));
    assert(!SmoothCanvas(Space::Canvas, 1080) && !SmoothCanvas(Space::Canvas, 1080.2f));
    assert(!SmoothCanvas(Space::Pixel, 1440) && !SmoothCanvas(Space::None, 1440));
    assert(!SmoothCanvas(Space::Canvas, 0) && !SmoothCanvas(Space::Canvas, NAN));
}

// Backgrounds, in the traced draw order of the main menu: the full-canvas picture fading in
// (alpha 0x19, then up to 0xff), the game's parked quads from other textures, the layers from
// the picture's texture, then the portrait from another texture. Splash logos are opaque.
void BackgroundTests() {
    const Point parked[4] = {{-100, -100}, {-100, -100}, {-100, -100}, {-100, -100}};
    const Point panel[4] = {{762, -44}, {762, 980}, {1530, 980}, {1530, -44}};
    const Point line[4] = {{0, 10}, {0, 10}, {1920, 10}, {1920, 10}};
    assert(Degenerate(parked) && Degenerate(line) && !Degenerate(panel));
    const Point bad[4] = {{NAN, 0}, {NAN, 0}, {NAN, 0}, {NAN, 0}};
    assert(!Degenerate(bad)); // unreadable: counts as visible

    const PictureLook menu{2048, 2048, {0, 0, 0.9375f, 0.5273f}};
    const PictureLook logo{4096, 4096, {0, 0, 0.4688f, 0.2637f}};
    const PictureLook logo2{4096, 4096, {0.469f, 0, 0.9377f, 0.2637f}};
    const unsigned long long menu_texture = 0x10b59d0100, logo_texture = 0x10e7010100;
    const unsigned long long other = 0x10b0c1b900, portrait = 0x1115f00100;
    Backgrounds b;
    // Splash logos: opaque (faded through black by colour), never stretched, no layers.
    assert(!b.Picture(logo, logo_texture, false) && !b.Picture(logo2, logo_texture, false));
    assert(!b.Layer(logo_texture, false));
    // Menu opens: the first frame is see-through, so the background and its layers stretch.
    assert(b.Picture(menu, menu_texture, true));
    assert(!b.Layer(other, true));       // parked quad from another texture: skipped
    assert(b.Layer(menu_texture, true));  // parked quad from the background's texture: harmless
    assert(b.Layer(menu_texture, false)); // sparkles
    assert(b.Layer(menu_texture, false)); // glow and scan-line layers
    assert(!b.Layer(portrait, false));    // visible draw from another texture ends the layers
    assert(!b.Layer(menu_texture, false)); // the menu's later icons from the same texture
    // Fully opaque frames keep the stretch; texture moved (menu reopened) as well.
    assert(b.Picture(menu, menu_texture, false) && b.Layer(menu_texture, false));
    b.EndLayers();
    assert(!b.Layer(menu_texture, false));
    assert(b.Picture(menu, 0x10c0000000, false) && b.Layer(0x10c0000000, false));
    // Same texture size, another region: a different picture, still opaque: bars.
    const PictureLook other_region{2048, 2048, {0, 0.5f, 0.9375f, 1}};
    assert(!b.Picture(other_region, menu_texture, false) && !b.Layer(menu_texture, false));
    // Logos stay opaque pictures after the menu was learned.
    assert(!b.Picture(logo, logo_texture, false));
    // Looks compare with a small tolerance and by texture size.
    assert((PictureLook{2048, 2048, {0, 0, 0.93751f, 0.5273f}} == menu));
    assert(!(PictureLook{2048, 1024, {0, 0, 0.9375f, 0.5273f}} == menu));
    // The table of looks is bounded: further see-through pictures are not remembered.
    Backgrounds full;
    for (unsigned i = 0; i < 16; ++i)
        assert(full.Picture(PictureLook{64 + i, 64, {0, 0, 1, 1}}, 1, true));
    assert(!full.Picture(PictureLook{1000, 64, {0, 0, 1, 1}}, 1, true));
    assert(full.Picture(PictureLook{64, 64, {0, 0, 1, 1}}, 1, false));
}

void MovieTests() {
    using namespace Movie;
    // Planar video detection.
    const Plane video[3] = {{1920, 1080, 1}, {960, 540, 1}, {960, 540, 1}};
    const auto size = PlanarVideo(video);
    assert(size && (*size)[0] == 1920 && (*size)[1] == 1080);
    const Plane odd[3] = {{1279, 719, 1}, {640, 360, 1}, {640, 360, 1}};
    assert(PlanarVideo(odd));
    const Plane rgba[3] = {{1920, 1080, 10}, {960, 540, 1}, {960, 540, 1}};
    assert(!PlanarVideo(rgba));
    const Plane full_chroma[3] = {{1920, 1080, 1}, {1920, 1080, 1}, {1920, 1080, 1}};
    assert(!PlanarVideo(full_chroma));
    const Plane two[2] = {{1920, 1080, 1}, {960, 540, 1}};
    assert(!PlanarVideo(two));

    // Full-screen quad: the traced fan order, another order, and non-full quads.
    const std::array<float, 2> fan[4] = {{-1, 1}, {-1, -1}, {1, -1}, {1, 1}};
    assert(FullscreenQuad(fan));
    const std::array<float, 2> strip[4] = {{-1, -1}, {1, -1}, {-1, 1}, {1, 1}};
    assert(FullscreenQuad(strip));
    const std::array<float, 2> inset[4] = {{-0.5f, 0.5f}, {-0.5f, -0.5f}, {0.5f, -0.5f}, {0.5f, 0.5f}};
    assert(!FullscreenQuad(inset));
    const std::array<float, 2> repeated[4] = {{-1, 1}, {-1, 1}, {1, -1}, {1, 1}};
    assert(!FullscreenQuad(repeated));

    const std::array<uint32_t, 2> hd{1920, 1080};
    // 32:9 5120x1440 (traced): negative-height viewport at y = 1440.
    auto fit = Fit({0, 1440, 5120, -1440}, 5120, hd);
    assert(fit && Close(fit->x, 1280) && Close(fit->width, 2560));
    assert(fit->left_x == 0 && fit->left_w == 1280 && fit->right_x == 3840 && fit->right_w == 1280);
    assert(fit->top == 0 && fit->height == 1440);
    // 21:9 sizes.
    fit = Fit({0, 1440, 3440, -1440}, 3440, hd);
    assert(fit && Close(fit->x, 440) && Close(fit->width, 2560) && fit->right_w == 440);
    fit = Fit({0, 1080, 2560, -1080}, 2560, hd);
    assert(fit && Close(fit->x, 320) && Close(fit->width, 1920) && fit->left_w == 320);
    // 32:9 at 1080 lines.
    fit = Fit({0, 1080, 3840, -1080}, 3840, hd);
    assert(fit && Close(fit->x, 960) && Close(fit->width, 1920));
    // 16:9 targets are unchanged.
    assert(!Fit({0, 1080, 1920, -1080}, 1920, hd));
    assert(!Fit({0, 1440, 2560, -1440}, 2560, hd));
    assert(!Fit({0, 2160, 3840, -2160}, 3840, hd));
    // A viewport that does not cover the whole target is left alone.
    assert(!Fit({1280, 1440, 2560, -1440}, 5120, hd));
    assert(!Fit({0, 1440, 2560, -1440}, 5120, hd));
    // Positive-height viewport.
    fit = Fit({0, 0, 5120, 1440}, 5120, hd);
    assert(fit && fit->top == 0 && fit->height == 1440);
    // Bad input.
    assert(!Fit({NAN, 0, 5120, 1440}, 5120, hd));
    assert(!Fit({0, 0, 5120, 1440}, 5120, {0, 1080}));
}

std::vector<Point> Texels(std::span<const Point> uv, float width, float height) {
    std::vector<Point> out;
    for (const auto& t : uv) out.push_back({t[0] * width, t[1] * height});
    return out;
}

void LoadingTests() {
    // Enlargement: text drawn texel for texel, a zoom-in at 3x (the largest traced on text),
    // parked quads, one texel row, triangles.
    const Point strip[4] = {{1200, 196}, {1200, 244}, {1710, 244}, {1710, 196}};
    const Point strip_uv[4] = {{0, 0}, {0, 0.75f}, {0.9961f, 0.75f}, {0.9961f, 0}};
    assert(std::abs(Enlargement(strip, Texels(strip_uv, 512, 64), 4) - 1) < 0.01f);
    const Point zoomed[4] = {{0, 0}, {0, 144}, {1530, 144}, {1530, 0}};
    assert(std::abs(Enlargement(zoomed, Texels(strip_uv, 512, 64), 4) - 3) < 0.01f);
    assert(Enlargement(zoomed, Texels(strip_uv, 512, 64), 4) <= smooth_enlargement);
    const Point parked[4] = {{-100, -100}, {-100, -100}, {-100, -100}, {-100, -100}};
    assert(Enlargement(parked, Texels(strip_uv, 512, 64), 4) == 0);
    const Point row_uv[4] = {{0, 0.5f}, {0, 0.5f}, {1, 0.5f}, {1, 0.5f}};
    assert(std::isinf(Enlargement(strip, Texels(row_uv, 512, 64), 4)));
    const Point triangle[3] = {{0, 0}, {0, 100}, {100, 100}};
    const Point triangle_uv[3] = {{0, 0}, {0, 0.5f}, {0.5f, 0.5f}};
    assert(std::abs(Enlargement(triangle, Texels(triangle_uv, 100, 100), 3) - 2) < 0.01f);
    assert(Enlargement(strip, Texels(strip_uv, 512, 64), 2) == 0);
    const Point bad[4] = {{NAN, 0}, {0, 1}, {1, 1}, {1, 0}};
    assert(std::isinf(Enlargement(bad, Texels(strip_uv, 512, 64), 4)));

    // The traced loading-screen cover (1024x2048 texture): the window at the bottom right, two
    // pieces of a 9x9-texel patch around it.
    const std::vector<Point> cover = {{1191, 750}, {1191, 1081}, {1922, 1081}, {1922, 750},
                                      {0, 750},    {0, 1081},    {1191, 1081}, {1191, 750},
                                      {0, 0},      {0, 750},     {1922, 750},  {1922, 0}};
    const Point patch[4] = {{0.8525f, 0.1880f}, {0.8525f, 0.1924f}, {0.8613f, 0.1924f}, {0.8613f, 0.1880f}};
    std::vector<Point> cover_uv = {{0, 0.1880f}, {0, 0.3496f}, {0.7139f, 0.3496f}, {0.7139f, 0.1880f}};
    cover_uv.insert(cover_uv.end(), patch, patch + 4);
    cover_uv.insert(cover_uv.end(), patch, patch + 4);
    const auto cover_texels = Texels(cover_uv, 1024, 2048);
    const float enlargement = Enlargement(cover, cover_texels, 4);
    assert(enlargement > 200 && enlargement < 220 && enlargement > smooth_enlargement);
    assert(Enlargement(std::span(cover).first(4), std::span(cover_texels).first(4), 4) < 1.01f);
    assert(CoverEdge(cover, cover_texels) == Edge::Right);
    // Mirrored: the window at the bottom left.
    std::vector<Point> mirrored;
    for (const auto& p : cover) mirrored.push_back({1920 - p[0], p[1]});
    assert(CoverEdge(mirrored, cover_texels) == Edge::Left);
    // A parked quad after it changes nothing.
    auto with_parked = cover;
    auto with_parked_texels = cover_texels;
    with_parked.insert(with_parked.end(), parked, parked + 4);
    with_parked_texels.insert(with_parked_texels.end(), cover_texels.begin(), cover_texels.begin() + 4);
    assert(CoverEdge(with_parked, with_parked_texels) == Edge::Right);
    // Not a cover: the top piece missing, one quad, mismatched lists, no one-colour piece (art
    // in three pieces), a window across the whole width, unreadable positions.
    assert(CoverEdge(std::span(cover).first(8), std::span(cover_texels).first(8)) == Edge::None);
    assert(CoverEdge(std::span(cover).first(4), std::span(cover_texels).first(4)) == Edge::None);
    assert(CoverEdge(cover, std::span(cover_texels).first(8)) == Edge::None);
    std::vector<Point> art_texels;
    for (const auto& p : cover) art_texels.push_back(p);
    assert(CoverEdge(cover, art_texels) == Edge::None);
    auto across = cover;
    across[0][0] = across[1][0] = 0;
    assert(CoverEdge(across, cover_texels) == Edge::None);
    auto unreadable = cover;
    unreadable[9][0] = NAN;
    assert(CoverEdge(unreadable, cover_texels) == Edge::None);

    // The cover's texture is drawn against that edge until the game has finished three frames
    // without it; other textures, or another texture size at the same address, are not.
    const unsigned long long loading = 0x10b7500100, hud = 0x10babe0100;
    EdgeArt art;
    assert(art.Draw(loading, 1024, 2048) == Edge::None);
    art.Cover(loading, 1024, 2048, Edge::None);
    assert(art.Draw(loading, 1024, 2048) == Edge::None);
    art.Cover(loading, 1024, 2048, Edge::Right);
    assert(art.Draw(loading, 1024, 2048) == Edge::Right);
    assert(art.Draw(hud, 512, 64) == Edge::None && art.Draw(loading, 512, 64) == Edge::None);
    for (int frame = 0; frame < 100; ++frame) { // the cover is drawn every frame
        art.Submit();
        assert(art.Draw(loading, 1024, 2048) == Edge::Right);
        art.Cover(loading, 1024, 2048, Edge::Right);
    }
    art.Submit();
    art.Submit();
    assert(art.Draw(loading, 1024, 2048) == Edge::Right);
    art.Submit();
    assert(art.Draw(loading, 1024, 2048) == Edge::None);

    // Viewport x at 5120x1440 (1280-pixel side areas) and 3440x1440 (440).
    assert(Close(EdgeViewportX(Edge::Right, 1280), 1280) && Close(EdgeViewportX(Edge::Left, 440), -440));
    assert(Close(EdgeViewportX(Edge::None, 1280), 0));
}

std::vector<Point> Quad(float x0, float y0, float x1, float y1) {
    return {{x0, y0}, {x0, y1}, {x1, y1}, {x1, y0}};
}

void PromptTests() {
    // Traced save prompt at rest: the bar (768x45 texels at their own size), the text strip
    // (555x50) and the button icon (60x60 texels drawn 27x27), in that order.
    const auto bar = Quad(1682, 810, 2450, 855), bar_texels = Quad(0, 0, 768, 45);
    const auto text = Quad(1727, 797, 2282, 847), text_texels = Quad(0, 0, 555, 50);
    const auto icon = Quad(1733, 819, 1760, 846), icon_texels = Quad(742, 0, 802, 60);
    EdgeTabs tabs;
    assert(!tabs.Draw(icon, icon_texels)); // nothing hung off the edge yet this frame
    tabs.Submit();
    tabs.Submit();
    assert(tabs.Draw(bar, bar_texels));
    assert(tabs.Draw(text, text_texels));
    assert(tabs.Draw(icon, icon_texels));
    // Next frame: the icon drawn before its bar is held by the last frame's bar.
    tabs.Submit();
    assert(tabs.Draw(icon, icon_texels));
    tabs.Submit();
    assert(!tabs.Draw(icon, icon_texels)); // the prompt is gone
    // Sliding in from the edge (traced first frame): every piece past the edge.
    assert(tabs.Draw(Quad(1904, 810, 2672, 855), bar_texels));
    assert(tabs.Draw(Quad(1908, 819, 1935, 846), icon_texels));
    tabs.Submit();
    tabs.Submit();

    // Not tabs: the menu's description line (a 2048-texel strip from x 92), the menu
    // background's layers across both edges, the minimap's glow (15 texels blown up to 526
    // pixels), art inside the canvas, art that ends at the edge (a little overdraw).
    assert(!tabs.Draw(Quad(92, 987, 2140, 1035), Quad(0, 0, 2048, 48)));
    assert(!tabs.Draw(Quad(-488, -274, 2408, 1354), Quad(0, 0, 2896, 1628)));
    assert(!tabs.Draw(Quad(1487, -93, 2013, 433), Quad(451, 372, 466, 387)));
    assert(!tabs.Draw(Quad(1584, 4, 1916, 336), Quad(0, 0, 332, 332)));
    assert(!tabs.Draw(Quad(1200, 196, 1922, 244), Quad(0, 0, 722, 48)));
    // A draw mixing a tab with other art, parked quads only, unreadable positions.
    auto mixed = bar;
    auto mixed_texels = bar_texels;
    const auto minimap = Quad(1584, 4, 1916, 336), minimap_texels = Quad(0, 0, 332, 332);
    mixed.insert(mixed.end(), minimap.begin(), minimap.end());
    mixed_texels.insert(mixed_texels.end(), minimap_texels.begin(), minimap_texels.end());
    assert(!tabs.Draw(mixed, mixed_texels));
    assert(!tabs.Draw(Quad(-100, -100, -100, -100), bar_texels));
    auto bad = bar;
    bad[1][0] = NAN;
    assert(!tabs.Draw(bad, bar_texels));
    // Art outside every tab, or only partly inside one, stays in the 16:9 area; parked quads
    // in a draw inside a tab change nothing.
    assert(tabs.Draw(bar, bar_texels));
    assert(!tabs.Draw(Quad(1600, 819, 1700, 846), icon_texels));
    assert(!tabs.Draw(Quad(1733, 700, 1760, 760), icon_texels));
    auto icon_parked = icon;
    auto icon_parked_texels = icon_texels;
    const auto parked = Quad(-100, -100, -100, -100);
    icon_parked.insert(icon_parked.end(), parked.begin(), parked.end());
    icon_parked_texels.insert(icon_parked_texels.end(), icon_texels.begin(), icon_texels.end());
    assert(tabs.Draw(icon_parked, icon_parked_texels));
    // Many tabs in one frame: the table is bounded, later ones still go to the edge.
    for (int i = 0; i < 20; ++i) assert(tabs.Draw(Quad(1700, 100 + i * 40, 2400, 130 + i * 40), Quad(0, 0, 700, 30)));
}

} // namespace

int main() {
    FillTests();
    BackgroundTests();
    LoadingTests();
    PromptTests();
    MovieTests();
    std::puts("PASS: wide-screen fills, pictures, backgrounds, the loading screen, prompts and movies");
    return 0;
}
