// SPDX-License-Identifier: GPL-2.0-or-later
// The barrier between shader translations, which read guest memory beside the command
// stream, and the writes the guest can see (GuestReads): such a write waits for every
// translation started before it, and costs nothing when none is running.
#include <atomic>
#include <cassert>
#include <chrono>
#include <cstdio>
#include <thread>

#include "video_core/renderer_vulkan/vk_async_compiler.h"

using Vulkan::AsyncCompiler;
using Vulkan::AsyncJob;
using Vulkan::GuestReads;
using namespace std::chrono_literals;

int main() {
    // Nothing reads guest memory: no wait, no time counted.
    GuestReads::Wait();
    assert(GuestReads::Pending() == 0);
    assert(GuestReads::wait_ns == 0);

    // Two translations: the write waits for the last of them.
    GuestReads::Begin();
    GuestReads::Begin();
    std::atomic<bool> written{};
    std::thread writer{[&] {
        GuestReads::Wait();
        written = true;
    }};
    std::this_thread::sleep_for(20ms);
    assert(!written);
    GuestReads::End();
    std::this_thread::sleep_for(20ms);
    assert(!written);
    GuestReads::End();
    writer.join();
    assert(written && GuestReads::Pending() == 0);
    assert(GuestReads::wait_ns > 0);

    // On the compiler threads, as the pipeline cache uses it: the read ends inside the job,
    // the rest of the job goes on after the write was let through.
    AsyncCompiler compiler{4, 16};
    std::atomic<unsigned> reads_done{}, jobs_done{};
    for (unsigned round = 0; round < 200; ++round) {
        AsyncJob<bool> jobs[3];
        for (auto& job : jobs) {
            GuestReads::Begin();
            const bool queued = job.Start(compiler, [&] {
                std::this_thread::sleep_for(50us);
                ++reads_done;
                GuestReads::End();
                std::this_thread::sleep_for(50us);
                ++jobs_done;
                return true;
            });
            assert(queued);
        }
        GuestReads::Wait();
        assert(reads_done == (round + 1) * 3);
        for (auto& job : jobs) {
            job.Wait();
        }
    }
    assert(jobs_done == 600 && GuestReads::Pending() == 0);

    // A job the queue refuses gives its read back (PipelineCache::QueueShader).
    AsyncCompiler full{1, 0};
    AsyncJob<bool> refused;
    GuestReads::Begin();
    if (!refused.Start(full, [] { return true; })) {
        GuestReads::End();
    }
    GuestReads::Wait();
    assert(GuestReads::Pending() == 0);

    std::puts("guest reads: ok");
    return 0;
}
