// SPDX-License-Identifier: GPL-2.0-or-later
#include "ps4rt_perf_stats.h"

#include <chrono>
#include <mutex>
#include <thread>
#include <vector>
#include "video_core/renderer_vulkan/vk_instance.h"

namespace Ps4RtPerfStats {

namespace {

std::mutex mutex;
Sample latest;
std::once_flag started;

void Run(const Vulkan::Instance* instance) {
    while (true) {
        Sample s;
        s.gpu_name = std::string(instance->GetModelName());
        {
            std::scoped_lock lock{mutex};
            latest = std::move(s);
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
    }
}

} // namespace

void Start(const Vulkan::Instance& instance) {
    // ponytail: never joined; a thread that ended while the game ran crashed in ntdll
    // (vk_pipeline_serialization.cpp, StopWarmUp). It sleeps between samples.
    std::call_once(started, [&] { std::thread(Run, &instance).detach(); });
}

Sample Latest() {
    std::scoped_lock lock{mutex};
    return latest;
}

} // namespace Ps4RtPerfStats
