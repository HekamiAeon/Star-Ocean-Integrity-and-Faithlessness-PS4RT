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
                            "PS4RT_SO_RESOLUTION"}) unsetenv(key);
    auto& s = Get();
    Load();
    assert(s.native_scene && s.CompactGraphicsMenu());
    assert(s.upscaler == UpscalerOff && s.output_res == OutputDefault);
    assert(!s.object_motion && !s.jitter && !s.rcas && s.temporal_problem);
    // Shaders always compile in the background now: an older config's async_shaders line loads
    // without error and is dropped on the next save.
    {
        FILE* old = std::fopen(path, "w");
        assert(old);
        std::fputs("async_shaders=0\nvsync=0\n", old);
        std::fclose(old);
        Load();
        assert(!s.vsync);
        Save();
        old = std::fopen(path, "r");
        assert(old);
        char line[256];
        while (std::fgets(line, sizeof(line), old)) assert(!std::strstr(line, "async_shaders"));
        std::fclose(old);
        s.vsync = true;
    }
    // Outputs only persist when larger than the internal resolution (same aspect): use 720p
    // (Resolutions index 1) so 1080p, 1440p and 4K outputs are all valid.
    s.render_resolution = 1;
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
            assert(s.render_resolution == 1 && s.startup_render_resolution == 1);
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

    // FSR output choices follow the internal resolution (the Sakura Wars port's rules): larger in
    // both dimensions, at most 3x and the same aspect; Native (AA only) always.
    for (int r = 0; r < int(Ps4RtGraphics::Resolutions.size()); ++r)
        assert(OutputAllowed(Ps4RtGraphics::ResolutionWidths[r], Ps4RtGraphics::ResolutionHeights[r], OutputNative));
    assert(OutputAllowed(1280, 720, 1) && OutputAllowed(1280, 720, 3) && !OutputAllowed(1280, 720, 4));
    assert(OutputAllowed(1024, 576, 1) && OutputAllowed(1024, 576, 2) && !OutputAllowed(1024, 576, 3));
    assert(!OutputAllowed(1920, 1080, 1) && OutputAllowed(1920, 1080, 2) && OutputAllowed(1920, 1080, 3));
    for (int o = 1; o < OutputCount; ++o) assert(!OutputAllowed(3840, 2160, o)); // 4K: AA only
    assert(OutputAllowed(2560, 1080, 4) && !OutputAllowed(2560, 1080, 2) && !OutputAllowed(2560, 1080, 6));
    assert(OutputAllowed(3840, 1080, 6) && !OutputAllowed(3840, 1080, 5));
    assert(!OutputAllowed(5120, 1440, 6) && !OutputAllowed(1920, 1080, -1) && !OutputAllowed(1920, 1080, OutputCount));
    // A saved output that does not fit the saved internal resolution becomes Native; Native is
    // saved as "native".
    s.render_resolution = Ps4RtGraphics::ParseResolution("2160p");
    s.output_res = 2;
    Save();
    Load();
    assert(s.output_res == OutputNative && s.render_resolution == 4);
    s.render_resolution = Ps4RtGraphics::ParseResolution("2560x1080");
    s.output_res = 4;
    Save();
    Load();
    assert(s.output_res == 4 && s.render_resolution == 5);
    s.output_res = OutputNative;
    Save();
    Load();
    assert(s.output_res == OutputNative);
    // An older ini's 1920x1080 output at the 1080p internal resolution: Native (the same image).
    {
        FILE* old = std::fopen(path, "w");
        assert(old);
        std::fputs("upscaler=fsr3\nrender_resolution=native\noutput_res=1920x1080\n", old);
        std::fclose(old);
        Load();
        assert(s.output_res == OutputNative && s.upscaler == UpscalerFsr3 && s.render_resolution == 0);
    }
    // A 1280x720 launch renders and displays at that size; the FSR output is the choice fitted
    // to the scene aspect, or the scene size for Native or a choice that does not fit it.
    setenv("PS4RT_SO_RESOLUTION", "1280x720", 1);
    Load();
    int w = 0, h = 0;
    SceneSize(w, h);
    assert(w == 1280 && h == 720);
    GameDisplaySize(w, h);
    assert(w == 1280 && h == 720);
    s.output_res = 2;
    FsrOutputSize(w, h);
    assert(w == 2560 && h == 1440);
    s.output_res = OutputNative;
    FsrOutputSize(w, h);
    assert(w == 1280 && h == 720);
    s.output_res = 4; // 3440x1440 is not 16:9
    FsrOutputSize(w, h);
    assert(w == 1280 && h == 720);
    // Ultrawide: 2560x1080 to 3440x1440 is fitted at the scene's aspect (3414x1440); 3840x1080
    // to 5120x1440 is exact.
    setenv("PS4RT_SO_RESOLUTION", "2560x1080", 1);
    Load();
    s.output_res = 4;
    FsrOutputSize(w, h);
    assert(w == 3414 && h == 1440);
    setenv("PS4RT_SO_RESOLUTION", "3840x1080", 1);
    Load();
    s.output_res = 6;
    FsrOutputSize(w, h);
    assert(w == 5120 && h == 1440);
    unsetenv("PS4RT_SO_RESOLUTION");
    Load();
    SceneSize(w, h);
    assert(w == 1920 && h == 1080);
    s.output_res = 3;
    FsrOutputSize(w, h);
    assert(w == 3840 && h == 2160);
    s.output_res = OutputNative;
    s.render_resolution = 0; // the sections below start from the game's own size
    Save();
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
    // saved upscaler, for an ultrawide size and for a 16:9 one, smaller or larger than 1080p.
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
        assert(s.upscaler == UpscalerFsr3 && s.jitter);
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
    assert(s.upscaler == UpscalerFsr3);
    // The game's own size, a size narrower than 16:9, malformed text or an empty value is an
    // ordinary launch.
    for (const char* text : {"1920x1080", "1440x1080", "3440", "3440x1440x", "wide", ""}) {
        setenv("PS4RT_SO_RESOLUTION", text, 1);
        Load();
        assert(s.internal_width == 0 && s.internal_height == 0 && s.upscaler == UpscalerFsr3);
    }
    unsetenv("PS4RT_SO_RESOLUTION");
    unlink(path);
    std::puts("PASS: native-scene settings persistence, native-port defaults, the pending game-language protocol, "
              "the internal resolution option and the FSR output choices");
}
