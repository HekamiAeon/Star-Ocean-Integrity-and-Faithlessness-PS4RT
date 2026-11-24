// SPDX-License-Identifier: GPL-2.0-or-later
// The pipeline-binary disk cache choice: off on NVIDIA by default, PS4RT_PIPELINE_BINARIES wins.
// The pipelines saved by earlier sessions, recognized by their key names.
#include <cassert>
#include <cstdio>
#include "video_core/renderer_vulkan/vk_pipeline_binary_policy.h"
#include "video_core/renderer_vulkan/vk_saved_pipelines.h"

using Vulkan::ChoosePipelineBinaries;
using Vulkan::PipelineBinaryChoice;

int main() {
    constexpr std::uint32_t amd = 0x1002, intel = 0x8086, nvidia = 0x10de;
    // Unset (or empty): on, except NVIDIA.
    assert(ChoosePipelineBinaries(nullptr, amd) == PipelineBinaryChoice::On);
    assert(ChoosePipelineBinaries(nullptr, intel) == PipelineBinaryChoice::On);
    assert(ChoosePipelineBinaries(nullptr, nvidia) == PipelineBinaryChoice::OffOnNvidia);
    assert(ChoosePipelineBinaries("", nvidia) == PipelineBinaryChoice::OffOnNvidia);
    assert(ChoosePipelineBinaries("", amd) == PipelineBinaryChoice::On);
    // 0: off on any GPU.
    assert(ChoosePipelineBinaries("0", amd) == PipelineBinaryChoice::OffBySetting);
    assert(ChoosePipelineBinaries("0", nvidia) == PipelineBinaryChoice::OffBySetting);
    // Any other value forces it on, NVIDIA included.
    assert(ChoosePipelineBinaries("1", nvidia) == PipelineBinaryChoice::On);
    assert(ChoosePipelineBinaries("1", amd) == PipelineBinaryChoice::On);
    std::puts("pipeline binary policy: ok");

    // Saved graphics keys are named g_ and the pipeline hash as fmt's {:#018x}.
    using Vulkan::SavedGraphicsKeyHash;
    assert(SavedGraphicsKeyHash("g_0x00000ffef04f2d59") == 0x00000ffef04f2d59ull);
    assert(SavedGraphicsKeyHash("g_0xfedcba9876543210") == 0xfedcba9876543210ull);
    assert(!SavedGraphicsKeyHash("c_0x00000ffef04f2d59")); // compute pipeline
    assert(!SavedGraphicsKeyHash("0x00000ffef04f2d59"));   // shader meta / SPIR-V
    assert(!SavedGraphicsKeyHash("0x00000ffef04f2d59_1"));
    assert(!SavedGraphicsKeyHash("profile"));
    assert(!SavedGraphicsKeyHash("g_0x0ffef04f2d59"));      // too short
    assert(!SavedGraphicsKeyHash("g_0x00000ffef04f2d59a"));  // too long
    assert(!SavedGraphicsKeyHash("g_0x00000ffef04f2d5z"));   // not hex
    assert(!SavedGraphicsKeyHash("g_0x-0000ffef04f2d59"));
    Vulkan::SavedPipelines saved;
    for (const char* name : {"g_0x00000ffef04f2d59", "c_0x00000000000000aa", "profile",
                             "0x00000ffef04f2d59_0", "g_0x00000ffef04f2d59"}) {
        saved.Add(name);
    }
    assert(saved.Size() == 1);
    assert(saved.Contains(0x00000ffef04f2d59ull) && !saved.Contains(0xaaull));
    std::puts("saved pipelines: ok");
    return 0;
}
