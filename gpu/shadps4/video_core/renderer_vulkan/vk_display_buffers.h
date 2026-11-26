// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <array>
#include <atomic>
#include <cstdint>

namespace Vulkan {

/// ps4rt: guest addresses of the game's display buffers. Added when the game registers them (on
/// its own thread), asked where pipelines are selected (GPU and draw preparation threads).
class DisplayBuffers {
public:
    static void Add(uint64_t address) {
        for (auto& slot : slots) {
            uint64_t expected = 0;
            if (slot.compare_exchange_strong(expected, address, std::memory_order_acq_rel) ||
                expected == address) {
                return;
            }
        }
        // Full: a later buffer is not known as one.
    }

    static bool Contains(uint64_t address) {
        for (const auto& slot : slots) {
            // Slots fill in order and keep their address: the first empty one ends the list.
            const uint64_t value = slot.load(std::memory_order_acquire);
            if (value == 0) {
                return false;
            }
            if (value == address) {
                return true;
            }
        }
        return false;
    }

    static constexpr size_t Capacity = 64;

private:
    static inline std::array<std::atomic<uint64_t>, Capacity> slots{};
};

} // namespace Vulkan
