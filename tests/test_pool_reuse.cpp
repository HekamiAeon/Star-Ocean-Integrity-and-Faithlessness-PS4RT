// SPDX-License-Identifier: GPL-2.0-or-later
// Pool reuse of a GPU-written image the guest has overwritten. Synthetic
// surfaces only, with the sizes of Star Ocean's environment cube and of the
// compressed cube its loader placed in the same memory; exercises the
// runtime's real layout, overlap and subresource methods.
#include <cassert>
#include <cstdio>

#include "video_core/amdgpu/resource.h"
#include "video_core/texture_cache/aliasing.h"
#include "video_core/texture_cache/image_info.h"

using namespace VideoCore;

static ImageInfo Cube(VAddr address, vk::Format format, u32 bits, bool block) {
    ImageInfo info{};
    info.guest_address = address;
    info.pixel_format = format;
    info.type = AmdGpu::ImageType::Color2D;
    info.tile_mode = AmdGpu::TileMode::Thin1DThin;
    info.array_mode = AmdGpu::ArrayMode::Array1DTiledThin1;
    info.props.is_tiled = true;
    info.props.is_block = block;
    info.num_bits = bits;
    info.size = {64, 64, 1};
    info.pitch = 64;
    info.resources = {7, 8}; // Six faces in a pow2-padded array, full mip chain.
    info.UpdateSize();
    return info;
}

// A render-target request for a run of faces, as the game binds them while it
// builds a cube: one mip, `layers` faces starting at `address`.
static ImageInfo Faces(const ImageInfo& cube, VAddr address, u32 layers) {
    ImageInfo info = cube;
    info.guest_address = address;
    info.resources = {1, layers};
    info.UpdateSize();
    return info;
}

int main() {
    // Only a GPU-written image whose memory the CPU has overwritten, and that
    // the current draw does not use, is freed while still recently used.
    assert(IsOverwrittenPoolImage(true, true, false));
    assert(!IsOverwrittenPoolImage(true, true, true));   // Bound or a target in this draw.
    assert(!IsOverwrittenPoolImage(true, false, false)); // GPU content is still current.
    assert(!IsOverwrittenPoolImage(false, true, false)); // CPU texture: a refresh is enough.
    assert(!IsOverwrittenPoolImage(false, false, false));

    const VAddr base = 0x10c7a40100;
    const auto old_cube = Cube(base, vk::Format::eR16G16B16A16Sfloat, 64, false);
    assert(old_cube.guest_size == 0x58000);
    assert(old_cube.mips_layout[0].size == 0x40000);

    // The loader's compressed cube at the old cube's address is the pool reuse
    // that reaches the rule: same address, incompatible dimensions.
    const auto loaded = Cube(base, vk::Format::eBc6HUfloatBlock, 128, true);
    assert(loaded.guest_size == 0x14000);
    assert(!HaveCompatibleOverlapDimensions(loaded, old_cube));

    // Why the old cube cannot stay cached. The next cube is built face by face
    // four faces into it. Its first face is, by address alone, slice 4 of the old
    // cube and would be drawn there, while a three-face run is no slice of it and
    // starts a second image: the new cube ends up split between the two.
    const VAddr rebuilt = base + 4 * 0x8000;
    const auto one_face = Faces(old_cube, rebuilt, 1);
    assert(one_face.guest_size == 0x8000);
    assert(one_face.MipOf(old_cube) == 0 && one_face.SliceOf(old_cube, 0) == 4);
    const auto three_faces = Faces(old_cube, rebuilt, 3);
    assert(three_faces.guest_size == 0x18000);
    assert(three_faces.MipOf(old_cube) != 0 || three_faces.SliceOf(old_cube, 0) < 0);

    std::puts("PASS: overwritten GPU image at pool reuse");
}
