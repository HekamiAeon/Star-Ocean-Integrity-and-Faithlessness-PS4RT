// SPDX-License-Identifier: GPL-2.0-or-later
// ps4rt: graphics pipelines an earlier session built on this install. Each was saved as its key
// (g_<hash>.key, RegisterPipelineData) and, with the pipeline-binary cache, as the driver's
// binary. Restoring one from its binary took 0.02 to 0.08 ms in the driver against 10 to 20 ms
// to compile (AMD, RADV), so a draw that first needs a saved pipeline builds it at once. Built
// in the background instead, the draw was skipped while the pipeline was ready long before the
// next frame: the same effects flickered on their first use in every session.

#pragma once

#include <charconv>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string_view>
#include <system_error>
#include <unordered_set>

namespace Vulkan {

/// The pipeline hash in a saved graphics key's name ("g_0x" and 16 hex digits), if it is one.
inline std::optional<std::uint64_t> SavedGraphicsKeyHash(std::string_view name) {
    constexpr std::string_view prefix = "g_0x";
    if (!name.starts_with(prefix) || name.size() != prefix.size() + 16) {
        return std::nullopt;
    }
    std::uint64_t hash{};
    const char* last = name.data() + name.size();
    const auto [end, error] = std::from_chars(name.data() + prefix.size(), last, hash, 16);
    if (error != std::errc{} || end != last) {
        return std::nullopt;
    }
    return hash;
}

/// The graphics pipelines saved by earlier sessions, by std::hash<GraphicsPipelineKey>.
class SavedPipelines {
public:
    /// A blob name of the pipeline store, without its extension; other blobs are ignored.
    void Add(std::string_view name) {
        if (const auto hash = SavedGraphicsKeyHash(name)) {
            hashes.insert(*hash);
        }
    }
    [[nodiscard]] bool Contains(std::uint64_t hash) const {
        return hashes.contains(hash);
    }
    [[nodiscard]] std::size_t Size() const {
        return hashes.size();
    }

private:
    std::unordered_set<std::uint64_t> hashes;
};

} // namespace Vulkan
