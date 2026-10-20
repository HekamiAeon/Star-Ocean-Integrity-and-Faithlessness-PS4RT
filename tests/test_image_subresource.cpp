// SPDX-License-Identifier: GPL-2.0-or-later
// Physical mip/slice selection after a texture-cache overlap scan. Synthetic
// surfaces only; exercises the runtime's real layout and subresource methods.
#include <algorithm>
#include <cassert>
#include <cstdio>

#include "video_core/amdgpu/resource.h"
#include "video_core/texture_cache/image_info.h"

using namespace VideoCore;

static ImageInfo Chain(u32 layers = 1, bool pow2 = false) {
    ImageInfo info{};
    info.guest_address = 0x10000000;
    info.pixel_format = vk::Format::eR16G16Sfloat;
    info.type = AmdGpu::ImageType::Color2D;
    info.tile_mode = AmdGpu::TileMode::Thin1DThin;
    info.array_mode = AmdGpu::ArrayMode::Array1DTiledThin1;
    info.props.is_tiled = true;
    info.props.is_pow2 = pow2;
    info.num_bits = 32;
    info.size = {1920, 1152, 1};
    info.pitch = 2048;
    info.resources = {8, layers};
    info.UpdateSize();
    return info;
}

static ImageInfo Request(const ImageInfo& parent, u32 mip, u32 layer = 0) {
    ImageInfo info = parent;
    const auto& layout = parent.mips_layout[mip];
    const u32 slice_bytes = layout.size / parent.resources.layers;
    info.guest_address += layout.offset + layer * slice_bytes;
    info.guest_size = slice_bytes;
    info.size = {std::max(parent.size.width >> mip, 1u),
                 std::max(parent.size.height >> mip, 1u), 1};
    info.pitch = layout.pitch;
    info.resources = {1, 1};
    info.props.is_pow2 = false; // Color-buffer requests describe the physical allocation.
    info.mips_layout[0] = {slice_bytes, layout.pitch, layout.height, 0};
    return info;
}

int main() {
    const auto parent = Chain();
    // A scan may first resolve a child to mip N, then compare the merged parent
    // with later overlapping images and overwrite that intermediate match with
    // mip 0. Resolve the ORIGINAL request against the FINAL backing instead.
    for (bool pow2 : {false, true}) {
        const auto chain = Chain(1, pow2);
        if (pow2) {
            assert(chain.mips_layout[1].offset == 0x1000000);
            assert(chain.mips_layout[2].offset == 0x1400000);
        }
        for (u32 mip = 0; mip < chain.resources.levels; ++mip) {
            const auto request = Request(chain, mip);
            const auto base = request.SubresourceOf(chain);
            assert(base && *base == (SubresourceBase{mip, 0}));
            if (mip) {
                assert(request.guest_address != chain.guest_address);
                assert(base->level != 0);
            }
        }
        // A single-mip T# needs only complete active microtile rows, while the
        // parent chain's pow2 padding reserves larger physical mip allocations.
        // Both address the same texels; using a separate image loses GPU writes.
        if (pow2) {
            for (u32 mip = 0; mip < chain.resources.levels; ++mip) {
                auto tight = Request(chain, mip);
                tight.UpdateSize();
                assert(tight.guest_size <= chain.mips_layout[mip].size);
                const auto base = tight.SubresourceOf(chain);
                assert(base && *base == (SubresourceBase{mip, 0}));
            }
        }
    }
    // Match remains relative to the full backing after an intermediate child
    // image has been expanded/merged; each request preserves its own location.
    for (u32 layers : {2u, 4u}) {
        const auto array = Chain(layers);
        for (u32 mip = 0; mip < array.resources.levels; ++mip) {
            for (u32 layer = 0; layer < layers; ++layer) {
                const auto base = Request(array, mip, layer).SubresourceOf(array);
                assert(base && *base == (SubresourceBase{mip, layer}));
            }
        }
    }
    {
        const auto padded = Chain(4, true);
        auto tight = Request(padded, 1, 2);
        tight.UpdateSize();
        assert(tight.SubresourceOf(padded) == (SubresourceBase{1, 2}));
        // Missing an active tile row, a different tile layout or an offset
        // inside a slice cannot borrow the larger backing's trailing padding.
        auto invalid = tight;
        --invalid.guest_size;
        assert(!invalid.SubresourceOf(padded));
        invalid = tight;
        invalid.array_mode = AmdGpu::ArrayMode::Array2DTiledThin1;
        assert(!invalid.SubresourceOf(padded));
        invalid = tight;
        invalid.guest_address += 256;
        assert(!invalid.SubresourceOf(padded));
        invalid = tight;
        invalid.props.is_depth = true;
        assert(!invalid.SubresourceOf(padded));
        invalid = tight;
        invalid.meta_info.htile_addr = 0x20000000;
        assert(!invalid.SubresourceOf(padded));
    }
    auto mismatch = Request(parent, 1);
    ++mismatch.guest_address;
    assert(!mismatch.SubresourceOf(parent));
    mismatch = Request(parent, 1);
    ++mismatch.pitch;
    assert(!mismatch.SubresourceOf(parent));
    mismatch = Request(parent, 1);
    --mismatch.size.height;
    assert(!mismatch.SubresourceOf(parent));
    mismatch = Request(parent, 1);
    mismatch.pixel_format = vk::Format::eR16G16B16A16Sfloat;
    assert(!mismatch.SubresourceOf(parent));
    mismatch = Request(parent, 1);
    mismatch.resources.levels = 2; // Native multi-mip view keeps its own base.
    assert(!mismatch.SubresourceOf(parent));
    mismatch = Request(parent, 1);
    mismatch.guest_size = 0;
    assert(!mismatch.SubresourceOf(parent));
    std::puts("PASS: image subresource selection");
}
