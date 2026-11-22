// SPDX-License-Identifier: GPL-2.0-or-later
// Settings persistence and safe provider transitions, without game files or Vulkan.
#include <array>
#include <cassert>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <initializer_list>
#include <unistd.h>
#include "ps4rt_graphics.h"
#include "ps4rt_settings.h"

int main() {
    using namespace Ps4RtSettings;
    char path[] = "/tmp/ps4rt-scene-settings-XXXXXX";
    const int fd = mkstemp(path);
    assert(fd >= 0);
    close(fd);
    setenv("PS4RT_CONFIG", path, 1);
    for (const char* key : {"PS4RT_UPSCALER", "PS4RT_OUTPUT_RES", "PS4RT_RCAS",
                            "PS4RT_OBJECT_MOTION", "PS4RT_JITTER",
                            "PS4RT_ASYNC_SHADERS", "PS4RT_SO_RESOLUTION"}) unsetenv(key);
    auto& s = Get();
    Load();
    assert(s.native_scene && s.CompactGraphicsMenu());
    assert(s.upscaler == UpscalerOff && s.output_res == OutputDefault);
    assert(!s.object_motion && !s.jitter && !s.rcas && s.temporal_problem);
    // Fresh configs ship with the visually stable synchronous path.
    assert(!s.async_shaders);
    // The overlay changes this atomic live. Make sure closing the panel and
    // relaunching respects either saved choice, without launcher overrides.
    s.async_shaders = true;
    Save();
    s.async_shaders = false;
    Load();
    assert(s.async_shaders);
    s.async_shaders = false;
    Save();
    s.async_shaders = true;
    Load();
    assert(!s.async_shaders);
    for (int provider : {UpscalerOff, UpscalerFsr3, UpscalerFsr4}) {
        for (int output : {1, 2, 3}) {
            s.sharpen = true;
            s.sharpness = 0.42f;
            s.anisotropy = 16;
            s.output_res = output;
            SetSceneUpscaler(provider, false);
            Save();
            Load();
            assert(s.upscaler == provider && s.startup_upscaler == provider);
            assert(s.output_res == output && s.startup_output_res == output);
            assert(s.sharpness > 0.419f && s.sharpness < 0.421f);
            assert(s.anisotropy == 16 && s.startup_anisotropy == 16);
            assert(!s.object_motion && !s.fsr1 && !s.spatial_aa);
            assert(s.jitter == (provider != UpscalerOff));
            assert(s.rcas == (provider == UpscalerOff));
        }
    }
    setenv("PS4RT_UPSCALER", "off", 1);
    setenv("PS4RT_OUTPUT_RES", "1920x1080", 1);
    Load();
    assert(s.upscaler == UpscalerOff && s.output_res == 1 && !s.jitter);
    unsetenv("PS4RT_UPSCALER");
    unsetenv("PS4RT_OUTPUT_RES");
    Load();
    assert(!s.object_motion && s.temporal_problem);
    // Game language (ps4rt_launch.py protocol): the env carries the active console language;
    // ps4rt.ini only ever carries a pending change, never read back by Load().
    const auto ini_language = [&] {
        FILE* file = std::fopen(path, "r");
        assert(file);
        char line[256];
        int value = -99;
        while (std::fgets(line, sizeof(line), file)) std::sscanf(line, "language=%d", &value);
        std::fclose(file);
        return value;
    };
    unsetenv("PS4RT_LANGUAGE_CHOICES");
    setenv("PS4RT_LANGUAGE", "1", 1);
    Load();
    assert(s.language == -1 && s.startup_language == -1); // no choices offered: no row
    Save();
    assert(ini_language() == -1);
    setenv("PS4RT_LANGUAGE_CHOICES", "1=English|2=French", 1);
    Load();
    assert(s.language == 1 && s.startup_language == 1);
    Save();
    assert(ini_language() == -1); // unchanged: no pending choice
    s.language = 2;               // the menu picks French
    Save();
    assert(ini_language() == 2);
    Load(); // a stale pending value never becomes the current language
    assert(s.language == 1 && s.startup_language == 1);
    s.language = 2;
    s.language = 1; // picked, then picked back: nothing pending
    Save();
    assert(ini_language() == -1);
    for (const char* bad : {"", "x", "3x", "-1", "31"}) {
        setenv("PS4RT_LANGUAGE", bad, 1);
        Load();
        assert(s.language == -1 && s.startup_language == -1);
    }
    unsetenv("PS4RT_LANGUAGE");
    unsetenv("PS4RT_LANGUAGE_CHOICES");

    // Internal resolution: the Code Fairy port's list, keys, menu order and aspect groups.
    namespace G = Ps4RtGraphics;
    assert(G::Resolutions.size() == 10 && G::Resolutions[0] == "native");
    assert(G::ParseResolution("3440x1440") == 6 && G::ParseResolution("720p") == 1 && G::ParseResolution("540p") == 2);
    assert(G::ParseResolution("") == 0 && G::ParseResolution("4K") == 0 && G::ParseResolution("1280x720") == 0);
    assert(G::ResolutionWidths[0] == 1920 && G::ResolutionHeights[0] == 1080);
    assert(G::ResolutionWidths[2] == 1024 && G::ResolutionHeights[2] == 576); // "540p"
    {
        // Every resolution is in the menu once, under the heading of its aspect, with its size
        // as the label; within a group the sizes grow.
        std::array<bool, 10> listed{};
        int next = 0;
        for (const auto& group : G::ResolutionMenuGroups) {
            assert(group.first == next && group.count > 0);
            int aw = 0, ah = 0;
            assert(std::sscanf(group.label, "%d:%d", &aw, &ah) == 2);
            for (int i = group.first; i < group.first + group.count; ++i) {
                const int r = G::ResolutionMenuOrder[i];
                assert(r >= 0 && r < 10 && !listed[r]);
                listed[r] = true;
                const int w = G::ResolutionWidths[r], h = G::ResolutionHeights[r];
                assert(std::abs(double(w) / h - double(aw) / ah) < 0.06);
                char label[32];
                std::snprintf(label, sizeof(label), "%d x %d", w, h);
                assert(!std::strncmp(G::ResolutionMenuLabels[i], label, std::strlen(label)));
                if (i > group.first)
                    assert(w * h > G::ResolutionWidths[G::ResolutionMenuOrder[i - 1]] *
                                       G::ResolutionHeights[G::ResolutionMenuOrder[i - 1]]);
                assert(G::ResolutionOfSize(w, h, -1) == r);
                assert(r == 0 ? !G::InternalSize(w, h) : G::InternalSize(w, h));
            }
            next += group.count;
        }
        assert(next == 10);
    }
    assert(G::ResolutionOfSize(1600, 900, 4) == 4 && G::ResolutionOfSize(0, 0, -1) == -1);
    assert(G::InternalSize(1600, 900) && G::InternalSize(2880, 1200));
    assert(!G::InternalSize(1920, 1080) && !G::InternalSize(1440, 1080) && !G::InternalSize(1920, 1200));
    assert(!G::InternalSize(640, 350) && !G::InternalSize(0, 0) && !G::InternalSize(-3440, -1440));
    assert(!G::InternalSize(20000, 1080));
    assert(G::WideImage(2560, 1080) && G::WideImage(3440, 1440) && G::WideImage(5120, 1440));
    assert(!G::WideImage(1920, 1080) && !G::WideImage(2560, 1440) && !G::WideImage(3840, 2160));
    assert(!G::WideImage(1440, 1080) && !G::WideImage(0, 0) && !G::WideImage(-3440, -1440));
    // A 1920x1080 launch: the menu's choice is only the selection for the next start.
    Load();
    assert(s.internal_width == 0 && s.internal_height == 0 && s.render_resolution == 0 && !GraphicsNeedRestart());
    SetSceneUpscaler(UpscalerFsr3, false);
    s.render_resolution = 6;
    assert(GraphicsNeedRestart());
    Save();
    // The launcher passes the saved choice as a size. The launch runs at that size with the
    // upscaler held off, and the saved upscaler choice survives a save made during it. That
    // holds for an ultrawide size and for a 16:9 one, smaller or larger than 1080p.
    const struct { const char* size; int width, height, choice; } launches[] = {
        {"3440x1440", 3440, 1440, 6}, {"1280x720", 1280, 720, 1}, {"1024x576", 1024, 576, 2},
        {"3840x2160", 3840, 2160, 4}};
    for (const auto& launch : launches) {
        s.render_resolution = launch.choice;
        Save();
        setenv("PS4RT_SO_RESOLUTION", launch.size, 1);
        Load();
        assert(s.internal_width == launch.width && s.internal_height == launch.height);
        assert(s.render_resolution == launch.choice && s.startup_render_resolution == launch.choice);
        assert(!GraphicsNeedRestart());
        assert(s.upscaler == UpscalerOff && s.kept_upscaler == UpscalerFsr3 && !s.jitter);
        s.render_resolution = 0;
        assert(GraphicsNeedRestart());
        Save();
        unsetenv("PS4RT_SO_RESOLUTION");
        Load();
        assert(s.internal_width == 0 && s.render_resolution == 0 && s.upscaler == UpscalerFsr3 && s.jitter);
    }
    // A size from the caller that is not in the list runs too; the saved choice stays as it is.
    setenv("PS4RT_SO_RESOLUTION", "2880x1200", 1);
    Load();
    assert(s.internal_width == 2880 && s.internal_height == 1200 && s.render_resolution == 0);
    assert(s.upscaler == UpscalerOff && s.kept_upscaler == UpscalerFsr3);
    // The game's own size, a size narrower than 16:9, malformed text or an empty value is an
    // ordinary launch.
    for (const char* text : {"1920x1080", "1440x1080", "3440", "3440x1440x", "wide", ""}) {
        setenv("PS4RT_SO_RESOLUTION", text, 1);
        Load();
        assert(s.internal_width == 0 && s.internal_height == 0 && s.upscaler == UpscalerFsr3);
    }
    unsetenv("PS4RT_SO_RESOLUTION");
    unlink(path);
    std::puts("PASS: native-scene settings persistence, native-port defaults, the pending game-language protocol "
              "and the internal resolution option");
}
