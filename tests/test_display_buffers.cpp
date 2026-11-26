// SPDX-License-Identifier: GPL-2.0-or-later
// The list of display buffer addresses (DisplayBuffers): the game adds to it from its own
// thread while the threads that select pipelines ask it.
#include <atomic>
#include <cassert>
#include <cstdio>
#include <thread>
#include <vector>

#include "video_core/renderer_vulkan/vk_display_buffers.h"

using Vulkan::DisplayBuffers;

int main() {
    // Empty: nothing is a display buffer, and neither is "no target".
    assert(!DisplayBuffers::Contains(0x10a4c88000));
    assert(!DisplayBuffers::Contains(0));

    DisplayBuffers::Add(0x10a4c88000);
    DisplayBuffers::Add(0x10a5058000);
    DisplayBuffers::Add(0x10a4c88000); // registered again
    assert(DisplayBuffers::Contains(0x10a4c88000));
    assert(DisplayBuffers::Contains(0x10a5058000));
    assert(!DisplayBuffers::Contains(0x10a5ca8000)); // a scene target
    assert(!DisplayBuffers::Contains(0));

    // Several threads add the same and different addresses while others ask.
    std::atomic<bool> stop{};
    std::vector<std::thread> readers;
    for (int i = 0; i < 4; ++i) {
        readers.emplace_back([&] {
            while (!stop) {
                assert(DisplayBuffers::Contains(0x10a4c88000));
                assert(!DisplayBuffers::Contains(0x10a5ca8000));
            }
        });
    }
    std::vector<std::thread> writers;
    for (int thread = 0; thread < 4; ++thread) {
        writers.emplace_back([thread] {
            for (uint64_t i = 0; i < 10; ++i) {
                DisplayBuffers::Add(0x2000000000 + i * 0x1000);            // shared by all
                DisplayBuffers::Add(0x3000000000 + thread * 0x100000 + i); // its own
            }
        });
    }
    for (auto& writer : writers) writer.join();
    stop = true;
    for (auto& reader : readers) reader.join();
    // 2 + 10 shared + 40 others fit: each one is in.
    for (uint64_t i = 0; i < 10; ++i) {
        assert(DisplayBuffers::Contains(0x2000000000 + i * 0x1000));
        for (int thread = 0; thread < 4; ++thread) {
            assert(DisplayBuffers::Contains(0x3000000000 + thread * 0x100000 + i));
        }
    }

    // Each address took one slot: exactly the free ones are left, and one more is left out.
    for (uint64_t i = 0; i < DisplayBuffers::Capacity - 52; ++i) {
        DisplayBuffers::Add(0x4000000000 + i);
        assert(DisplayBuffers::Contains(0x4000000000 + i));
    }
    DisplayBuffers::Add(0x5000000000);
    assert(!DisplayBuffers::Contains(0x5000000000));
    assert(DisplayBuffers::Contains(0x10a5058000));

    std::puts("display buffers: ok");
    return 0;
}
