// SPDX-License-Identifier: GPL-2.0-or-later
// Wide-screen rules: screen fills, full-canvas pictures, menu backgrounds, the loading screen,
// prompts, the pause menu's panel, recorded movies and layers held in front of the camera.
// Owned fixtures shaped like the traced draws (splash backdrop and background, splash picture,
// menu background and its layers, ending pictures, loading screen cover, save prompt, pause menu
// panel, credits, intro movie, a skill's tint plane, the ending's warp); no game data.
#include <cassert>
#include <cmath>
#include <cstdio>
#include <span>
#include <vector>
#include "video_core/renderer_vulkan/star_ocean_fill.h"
#include "video_core/renderer_vulkan/star_ocean_layers.h"
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
    // The same fill on a 16:9 screen of another size: 1920x1080 pixels of a 3840x2160 target
    // are its top-left quarter (a black box over every fade at 4K, the rest left uncovered).
    out = Widen(Space::Pixel, backdrop, 3840, 2160);
    assert(out && Close((*out)[0], {0, 0}) && Close((*out)[1], {0, 2160}) &&
           Close((*out)[2], {3840, 2160}) && Close((*out)[3], {3840, 0}));
    out = Widen(Space::Pixel, backdrop, 2560, 1440);
    assert(out && Close((*out)[1], {0, 1440}) && Close((*out)[2], {2560, 1440}));
    out = Widen(Space::Pixel, backdrop, 1280, 720); // larger than the screen: now its size
    assert(out && Close((*out)[2], {1280, 720}) && Close((*out)[3], {1280, 0}));
    assert(!Widen(Space::Pixel, partial, 3840, 2160));
    // Which viewports the rules look at. Wider than 16:9: every rule, as before.
    assert(ViewportRules(5120, 1440, 5120, 1440) == Rules::All);
    assert(ViewportRules(2560, 1080, 2560, 1080) == Rules::All);
    assert(ViewportRules(2560, 720, 5120, 1440) == Rules::All); // wide, not the frame
    // The game's 16:9 frame at another size than the canvas: the pixel-space fill alone.
    assert(ViewportRules(3840, 2160, 3840, 2160) == Rules::Fill);
    assert(ViewportRules(2560, 1440, 2560, 1440) == Rules::Fill);
    assert(ViewportRules(1280, 720, 1280, 720) == Rules::Fill);
    assert(ViewportRules(1024, 576, 1024, 576) == Rules::Fill);
    // The game's own size, and targets that are not the frame (a canvas-size picture or a
    // half-size pass at 4K, a half-size pass at 1080p, a shadow map): nothing.
    assert(ViewportRules(1920, 1080, 1920, 1080) == Rules::None);
    assert(ViewportRules(1920, 1080, 3840, 2160) == Rules::None);
    assert(ViewportRules(1920, 1080, 5120, 1440) == Rules::None);
    assert(ViewportRules(960, 540, 1920, 1080) == Rules::None);
    assert(ViewportRules(1280, 720, 2560, 1440) == Rules::None);
    assert(ViewportRules(4096, 4096, 3840, 2160) == Rules::None);
    assert(ViewportRules(0, 2160, 3840, 2160) == Rules::None &&
           ViewportRules(3840, 0, 3840, 2160) == Rules::None &&
           ViewportRules(NAN, 2160, 3840, 2160) == Rules::None);
    // Every internal resolution the menu offers, drawn into its own frame: a rule for each
    // but the game's own 1920x1080.
    constexpr int offered[][2] = {{1920, 1080}, {1280, 720}, {1024, 576}, {2560, 1440},
                                  {3840, 2160}, {2560, 1080}, {3440, 1440}, {3840, 1440},
                                  {3840, 1080}, {5120, 1440}};
    for (const auto& size : offered) {
        const float w = float(size[0]), h = float(size[1]);
        const Rules rules = ViewportRules(w, h, w, h);
        const bool native = size[0] == 1920 && size[1] == 1080;
        const bool wide = size[0] * 9 > size[1] * 16;
        assert(rules == (native ? Rules::None : wide ? Rules::All : Rules::Fill));
        const auto fill = Widen(Space::Pixel, backdrop, w, h);
        assert(native ? !fill : fill && Close((*fill)[2], {w, h}));
    }

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
// the picture's texture, then the portrait from another texture. Splash logos are opaque; the
// ending's pictures are see-through without layers.
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
    // The table of looks is bounded: further backgrounds are stretched while see-through and
    // followed by layers, but not remembered once opaque.
    Backgrounds full;
    for (unsigned i = 0; i < 16; ++i)
        assert(full.Picture(PictureLook{64 + i, 64, {0, 0, 1, 1}}, 1, true) && full.Layer(1, false));
    const PictureLook seventeenth{1000, 64, {0, 0, 1, 1}};
    assert(full.Picture(seventeenth, 1, true) && full.Layer(1, false));
    assert(!full.Picture(seventeenth, 1, false));
    assert(full.Picture(PictureLook{64, 64, {0, 0, 1, 1}}, 1, false));

    // The ending's character pictures, in the traced draw order: each a whole 1920x1080
    // texture fading in and out see-through, followed by the subtitle box from another texture.
    // Only the first frame of the first picture is stretched (still nearly transparent).
    const PictureLook ending{1920, 1080, {0, 0, 1, 1}};
    const unsigned long long picture1 = 0x10c6c00100, picture2 = 0x10cbd90100;
    const unsigned long long subtitles = 0x10a3000000;
    Backgrounds e;
    assert(e.Picture(ending, picture1, true));
    assert(!e.Layer(other, true));      // parked quad: decides nothing
    assert(!e.Layer(subtitles, false)); // another texture first: a picture, not a background
    assert(!e.Picture(ending, picture1, true) && !e.Layer(picture1, false));
    assert(!e.Picture(ending, picture1, false)); // opaque
    assert(!e.Picture(ending, picture1, true));  // fading out
    assert(!e.Picture(ending, picture2, true));  // the next character
    // The menu is still learned as a background afterwards, and the ending stays a picture.
    assert(e.Picture(menu, menu_texture, true) && e.Layer(menu_texture, false));
    assert(e.Picture(menu, menu_texture, false) && !e.Picture(ending, picture2, true));
    // A see-through picture drawn last (the next draw is in another target) is a picture, and
    // so is one followed by another full-canvas picture.
    Backgrounds last;
    assert(last.Picture(ending, picture1, true));
    last.EndLayers();
    assert(!last.Picture(ending, picture1, true));
    Backgrounds twice;
    assert(twice.Picture(ending, picture1, true) && !twice.Picture(logo, logo_texture, false));
    assert(!twice.Picture(ending, picture1, true));
    // A parked quad from the picture's own texture is no layer: the look stays undecided until a
    // visible draw.
    Backgrounds parked_first;
    assert(parked_first.Picture(menu, menu_texture, true) && parked_first.Layer(menu_texture, true));
    assert(parked_first.Layer(menu_texture, false) && parked_first.Picture(menu, menu_texture, false));
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

    // Traced credits frame: 30 name strips in the right column (753x36 pixels from x 1407,
    // text with an empty tail, run past the edge but mostly shown) are no tabs and leave the
    // table free; the panel's bar (from x 1436, hung by Backdrop) then holds the Skip and Back
    // prompts against the edge in every frame, whatever name scrolls level with them.
    EdgeTabs credits;
    credits.Submit();
    credits.Submit();
    for (int i = 0; i < 30; ++i) {
        const float y = 0.47f + 36 * i;
        assert(!credits.Draw(Quad(1407, y, 2160, y + 36), Quad(0, 0, 753, 36)));
        assert(!credits.Draw(Quad(1015.5f, y, 1768.5f, y + 36), Quad(0, 0, 753, 36)));
    }
    credits.Hang({1436, 948, 2091, 993});
    assert(credits.Draw(Quad(1488, 952, 1560, 988), Quad(0, 0, 72, 36)));    // label
    assert(credits.Draw(Quad(1569, 935, 1669, 985), Quad(0, 0, 100, 50)));   // button
    assert(credits.Draw(Quad(1694, 957, 1721, 984), Quad(0, 0, 27, 27)));    // icon
    assert(credits.Draw(Quad(1730, 935, 1830, 985), Quad(0, 0, 100, 50)));   // button
    // A strip hanging just as far past the edge as it shows is not a tab either.
    assert(!credits.Draw(Quad(1560, 100, 2280, 130), Quad(0, 0, 720, 30)));
}

void PauseMenuTests() {
    // Traced pause menu (one draw from its 2048x128 UI texture): one texel blown up over the
    // whole canvas with 10 pixels of overdraw, then the OPTIONS bar along the bottom: its start
    // at its own size, a 30-texel middle stretched to 191 pixels, a 3-pixel end past the edge.
    std::vector<Point> panel, panel_texels;
    const auto add = [&](std::vector<Point> quad, std::vector<Point> texels) {
        panel.insert(panel.end(), quad.begin(), quad.end());
        panel_texels.insert(panel_texels.end(), texels.begin(), texels.end());
    };
    add(Quad(-10, -10, 1929.9f, 1090), Quad(1225, 46, 1226, 47));
    add(Quad(1597, 948, 1897, 993), Quad(1061, 0, 1361, 45));
    add(Quad(1897, 948, 2088, 993), Quad(1361, 0, 1391, 45));
    add(Quad(2088, 948, 2091, 993), Quad(1391, 0, 1394, 45));
    // Not the loading screen's cover: the bar lies on the panel, it is not a hole in it.
    assert(CoverEdge(panel, panel_texels) == Edge::None);
    const auto layout = Backdrop(panel, panel_texels);
    assert(layout && layout->pieces.size() == 4);
    assert(layout->pieces[0] == Piece::Widen);
    for (size_t i = 1; i < 4; ++i) assert(layout->pieces[i] == Piece::Right);
    assert(layout->tab && Close(layout->tab->x0, 1597) && Close(layout->tab->x1, 2091) &&
           Close(layout->tab->y0, 948) && Close(layout->tab->y1, 993));
    // At 5120x1440 (twice as wide as 16:9) the panel covers the whole screen (the canvas is
    // seen from -960 to 2880) and the bar moves by the 960-unit side area; 3440x1440 alike.
    assert(Close(BackdropX(Piece::Widen, -10, 2), -980) && Close(BackdropX(Piece::Widen, 1929.9f, 2), 2899.8f));
    assert(Close(BackdropX(Piece::Right, 1597, 2), 2557) && Close(BackdropX(Piece::Keep, 1597, 2), 1597));
    const float ultrawide = (3440.f / 1440) / (16.f / 9);
    assert(BackdropX(Piece::Widen, -10, ultrawide) < 960 - 1080 * 3440.f / 1440 / 2);
    assert(Close(BackdropX(Piece::Right, 1920, ultrawide), 960 + 1080 * 3440.f / 1440 / 2));
    // The OPTIONS button and label drawn afterwards are inside the bar: against the edge too.
    EdgeTabs tabs;
    tabs.Hang(*layout->tab);
    assert(tabs.Draw(Quad(1649, 952, 1721, 988), Quad(1395, 0, 1438, 21)));
    assert(tabs.Draw(Quad(1730, 935, 1830, 985), Quad(0, 0, 100, 50)));
    assert(!tabs.Draw(Quad(0, 16, 1060, 109), Quad(0, 0, 1060, 93))); // the title banner
    tabs.Submit();
    assert(tabs.Draw(Quad(1649, 952, 1721, 988), Quad(1395, 0, 1438, 21))); // last frame's bar
    tabs.Submit();
    assert(!tabs.Draw(Quad(1649, 952, 1721, 988), Quad(1395, 0, 1438, 21)));

    // The panel alone is widened; with art elsewhere (the title banner) the art stays.
    const auto alone = Backdrop(std::span(panel).first(4), std::span(panel_texels).first(4));
    assert(alone && alone->pieces.size() == 1 && alone->pieces[0] == Piece::Widen && !alone->tab);
    auto with_title = std::vector<Point>(panel.begin(), panel.begin() + 4);
    auto with_title_texels = std::vector<Point>(panel_texels.begin(), panel_texels.begin() + 4);
    const auto title = Quad(0, 16, 1060, 109), title_texels = Quad(0, 0, 1060, 93);
    with_title.insert(with_title.end(), title.begin(), title.end());
    with_title_texels.insert(with_title_texels.end(), title_texels.begin(), title_texels.end());
    const auto kept = Backdrop(with_title, with_title_texels);
    assert(kept && kept->pieces[0] == Piece::Widen && kept->pieces[1] == Piece::Keep && !kept->tab);
    // A parked quad is left alone and does not stop the bar.
    auto parked = panel;
    auto parked_texels = panel_texels;
    const auto nowhere = Quad(-100, -100, -100, -100);
    parked.insert(parked.end(), nowhere.begin(), nowhere.end());
    parked_texels.insert(parked_texels.end(), nowhere.begin(), nowhere.end());
    const auto with_parked = Backdrop(parked, parked_texels);
    assert(with_parked && with_parked->pieces[4] == Piece::Keep && with_parked->tab);

    // Not a backdrop: art covering the canvas at its own size (the menu background's layers),
    // a solid piece short of an edge, a single quad exactly on the canvas (a picture), the
    // loading screen's cover (no piece covers the whole canvas), unreadable or mismatched lists.
    assert(!Backdrop(Quad(-488, -274, 2408, 1354), Quad(0, 0, 2896, 1628)));
    assert(!Backdrop(Quad(0, 0, 1900, 1080), Quad(1225, 46, 1226, 47)));
    assert(!Backdrop(Quad(0, 0, 1920, 1080), Quad(1225, 46, 1226, 47)));
    const std::vector<Point> loading = {{1191, 750}, {1191, 1081}, {1922, 1081}, {1922, 750},
                                        {0, 750},    {0, 1081},    {1191, 1081}, {1191, 750},
                                        {0, 0},      {0, 750},     {1922, 750},  {1922, 0}};
    std::vector<Point> loading_texels = Quad(0, 385, 731, 716);
    for (int i = 0; i < 2; ++i) {
        const auto patch = Quad(873, 385, 882, 394);
        loading_texels.insert(loading_texels.end(), patch.begin(), patch.end());
    }
    assert(CoverEdge(loading, loading_texels) == Edge::Right);
    assert(!Backdrop(loading, loading_texels));
    auto bad = panel;
    bad[2][0] = NAN;
    assert(!Backdrop(bad, panel_texels));
    assert(!Backdrop(panel, std::span(panel_texels).first(8)));
    assert(!Backdrop(std::span(panel).first(6), std::span(panel_texels).first(6)));
}

} // namespace

// Layers held in front of the camera, with the traced world-view-projection rows at 5120x1440
// (twice as wide as 16:9).
void LayerTests() {
    namespace Layers = Vulkan::StarOceanLayers;
    using Layers::Position;
    // A battle skill's purple tint: a flat 4x4 grid at +-5, 16 in front of the camera. On the
    // wide picture it ends at +-0.547 (the box seen from x 1158 to 3961); on 16:9 it reached
    // +-1.094.
    const Layers::Rows tint{1.75f, 0, -1.49012e-08f, 0,           0, 3.55556f, 0, -0.00195312f,
                            2.91038e-11f, 0, -0.000202041f, 10.0988f, 0, 0, 1, 16};
    std::vector<Position> grid;
    for (int row = 0; row <= 4; ++row)
        for (int column = 0; column <= 4; ++column)
            grid.push_back({-5 + 2.5f * column, -5 + 2.5f * row, 0});
    assert(Layers::HeldOnViewAxis(tint));
    assert(Layers::ScreenLayer(tint, grid, 2));
    assert(!Layers::ScreenLayer(tint, grid, 1));      // 16:9: nothing to do
    assert(!Layers::ScreenLayer(tint, grid, 1.0004f)); // within rounding of 16:9
    // Every wide size in the menu (21:9, 24:9, 32:9): the projection's x scale is the 16:9 one
    // (3.5 here) over the stretch, and the plane covers the 16:9 frame at each.
    const std::array<std::array<float, 2>, 5> wide_sizes{
        {{2560, 1080}, {3440, 1440}, {3840, 1440}, {3840, 1080}, {5120, 1440}}};
    for (const auto& [width, height] : wide_sizes) {
        const float stretch = (width / height) / (16.f / 9);
        auto rows = tint;
        rows[0] = 3.5f / stretch;
        assert(Layers::ScreenLayer(rows, grid, stretch));
    }
    // A layer already reaching past the wide picture's sides is widened too, so it frames the
    // wide picture as it framed 16:9.
    auto wider = grid;
    for (auto& p : wider) p[0] *= 2;
    assert(Layers::ScreenLayer(tint, wider, 2));
    // Too small for the 16:9 frame (a smaller plane, or one held further away): art, not a layer.
    std::vector<Position> small = grid;
    for (auto& p : small) p = {p[0] * 0.8f, p[1] * 0.8f, 0};
    assert(!Layers::ScreenLayer(tint, small, 2));
    std::vector<Position> far = grid;
    for (auto& p : far) p[2] = 4;
    assert(!Layers::ScreenLayer(tint, far, 2));
    // A vertex behind the camera, an unreadable one, or too few vertices: left alone.
    auto behind = grid;
    behind[3][2] = -20;
    assert(!Layers::ScreenLayer(tint, behind, 2));
    auto bad = grid;
    bad[7][0] = NAN;
    assert(!Layers::ScreenLayer(tint, bad, 2));
    assert(!Layers::ScreenLayer(tint, std::span(grid).first(2), 2));

    // The ending's warp: its dark rim is a tunnel from a ring of radius 33 at depth 0.105 to one
    // of 27 at 4.43 (traced vertices (6.64, 32.9, 0.105) and (0.316, 27, 4.43)), 13 in front of
    // the camera: on the wide picture it ends at +-0.85, on 16:9 at +-1.7.
    const Layers::Rows rim{0.335703f, 0, -4.65661e-10f, 0, -4.65661e-10f, 0.893572f, 0, -0.000183105f,
                           0, 0, 1e-4f, 10, 0, -7.45058e-09f, 0.497537f, 13};
    std::vector<Position> tunnel;
    for (int i = 0; i < 24; ++i) {
        const float angle = float(i) * 6.2831853f / 24;
        tunnel.push_back({33 * std::cos(angle), 33 * std::sin(angle), 0.105f});
        tunnel.push_back({27 * std::cos(angle), 27 * std::sin(angle), 4.43f});
    }
    assert(Layers::ScreenLayer(rim, tunnel, 2));
    // At 21:9 and 24:9 the rim still reaches past the picture's sides (1.27 and 1.13 times the
    // half-width), but its rounded edge runs through the screen's corners and its dark band over
    // the sides: it is widened at every wide size.
    for (const auto& [width, height] : wide_sizes) {
        const float stretch = (width / height) / (16.f / 9);
        auto rows = rim;
        rows[0] = 0.335703f * 2 / stretch;
        assert(Layers::ScreenLayer(rows, tunnel, stretch));
    }
    // Its bright inner tunnel (traced), far down the view axis: stays as it is.
    const Layers::Rows inner{0.5f, -1.16415e-10f, -2.98023e-08f, 6.10352e-05f, 0, 1.33199f, -4.76837e-07f, -0.000183105f,
                             0, 0, 1e-4f, 10, 0, -1.49012e-08f, 22, 17.9961f};
    const std::vector<Position> inner_vertices{{-4.81f, 12.9f, 13.1f}, {-7.35f, 19, 8.74f}, {-2.31f, 13.7f, 13.1f},
                                               {-7.12f, 11.7f, 13.1f}};
    assert(Layers::HeldOnViewAxis(inner) && !Layers::ScreenLayer(inner, inner_vertices, 2));

    // Not held on the view axis: a camera-facing billboard beside it (traced), the 2D canvas
    // transform, a scene model (traced).
    const Layers::Rows billboard{0.694051f, 3.72529e-09f, 2.98023e-08f, -587.627f, 2.98023e-08f, 2.46774f, 5.96046e-08f, 345.824f,
                                 0, 0, 1e-4f, 10, 5.96046e-08f, -1.49012e-08f, 0.885199f, 1895.91f};
    assert(!Layers::HeldOnViewAxis(billboard));
    const Layers::Rows canvas{0.000520833f, 0, 0, -0.5f, 0, -0.00185185f, 0, 1, 0, 0, 9.10921e-05f, 0.900618f, 0, 0, 0, 1};
    assert(!Layers::HeldOnViewAxis(canvas));
    const Layers::Rows model{-2.207e-08f, -0.1851f, 0.1451f, -17650, 0.8289f, 0.06878f, 0.08774f, -27450,
                             1.333e-06f, -6.115e-06f, -7.8e-06f, 10.86f, -0.03999f, 0.1834f, 0.234f, -25730};
    assert(!Layers::HeldOnViewAxis(model));
    auto behind_origin = tint;
    behind_origin[15] = -16;
    assert(!Layers::HeldOnViewAxis(behind_origin));
    // Flat full-screen passes: w without object z (no perspective). The wide screen never
    // narrowed them; a quad on the whole picture stays as it is.
    const Layers::Rows identity{1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1};
    const std::vector<Position> full{{-1, -1, 0}, {1, -1, 0}, {1, 1, 0}, {-1, 1, 0}};
    assert(!Layers::HeldOnViewAxis(identity) && !Layers::ScreenLayer(identity, full, 2));
    auto flat = tint;
    flat[14] = 0;
    assert(!Layers::HeldOnViewAxis(flat) && !Layers::ScreenLayer(flat, grid, 2));

    // The tint as it fades (traced): right after the whole grid, a part of it held by the same
    // matrix. Alone the part (here the upper half) covers only some of the picture and is no
    // layer; as a piece of the grid widened this frame or the last, it is widened with it.
    std::vector<Position> upper;
    for (const auto& p : grid)
        if (p[1] >= 0) upper.push_back(p);
    assert(!Layers::ScreenLayer(tint, upper, 2) && Layers::InFront(tint, upper));
    Layers::HeldLayers held;
    assert(!held.PieceOf(tint));
    held.Widened(tint);
    assert(held.PieceOf(tint));
    auto rounded = tint;
    rounded[15] = 16.00001f; // constants computed again for the piece
    rounded[0] = 1.7500001f;
    assert(held.PieceOf(rounded));
    auto moved = tint;
    moved[15] = 17; // the same object further away: another matrix
    assert(!held.PieceOf(moved) && !held.PieceOf(rim));
    held.Submit();
    assert(held.PieceOf(tint)); // the grid was drawn last frame
    held.Submit();
    assert(!held.PieceOf(tint)); // and is gone
    assert(!Layers::InFront(tint, behind) && !Layers::InFront(tint, {}));
    // The table is bounded: further layers are widened but not remembered.
    Layers::HeldLayers many;
    for (int i = 0; i < 9; ++i) {
        auto rows = tint;
        rows[15] = 16.f + float(i);
        many.Widened(rows);
    }
    auto ninth = tint;
    ninth[15] = 24;
    assert(many.PieceOf(tint) && !many.PieceOf(ninth));

    // 16-bit floats, as the effect meshes store their positions.
    assert(Layers::FloatToHalf(5) == 0x4500 && Layers::FloatToHalf(-5) == 0xc500);
    assert(Layers::FloatToHalf(1) == 0x3c00 && Layers::FloatToHalf(10) == 0x4900);
    assert(Layers::FloatToHalf(65504) == 0x7bff && Layers::FloatToHalf(65520) == 0x7c00);
    assert(Layers::FloatToHalf(0) == 0 && Layers::FloatToHalf(-0.f) == 0x8000);
    assert(Layers::FloatToHalf(std::ldexp(1.f, -24)) == 0x0001);
    assert(Layers::FloatToHalf(std::ldexp(1.f, -26)) == 0);
    assert(Layers::FloatToHalf(1 + std::ldexp(1.f, -11)) == 0x3c00); // tie: to even
    assert(Layers::FloatToHalf(1 + 3 * std::ldexp(1.f, -11)) == 0x3c02);
    assert(Layers::HalfToFloat(0x4500) == 5 && Layers::HalfToFloat(0xc500) == -5);
    assert(Layers::HalfToFloat(0x0001) == std::ldexp(1.f, -24));
    assert(std::isinf(Layers::HalfToFloat(0x7c00)) && std::isnan(Layers::HalfToFloat(0x7e00)));
    for (unsigned half = 0; half < 0x10000; ++half) {
        if (((half >> 10) & 0x1f) == 0x1f) continue;
        assert(Layers::FloatToHalf(Layers::HalfToFloat(std::uint16_t(half))) == half);
    }
    assert(Layers::FloatToHalf(2 * Layers::HalfToFloat(Layers::FloatToHalf(-4.96f))) ==
           Layers::FloatToHalf(-9.92f));
}

int main() {
    FillTests();
    BackgroundTests();
    LoadingTests();
    PromptTests();
    PauseMenuTests();
    MovieTests();
    LayerTests();
    std::puts("PASS: wide-screen fills, pictures, backgrounds, the loading screen, prompts, the pause menu, movies and camera layers");
    return 0;
}
