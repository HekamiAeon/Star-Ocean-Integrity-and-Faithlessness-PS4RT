// SPDX-License-Identifier: GPL-2.0-or-later
// Pool reuse of a GPU-written image the guest has overwritten, and of a depth
// target's stencil memory by another depth image. Synthetic surfaces only, with
// the sizes and addresses of Star Ocean's environment cube (and the compressed
// cube its loader placed in the same memory) and of its sun shadow maps;
// exercises the runtime's real layout, overlap and subresource methods.
#include <cassert>
#include <cstdio>

#include "video_core/amdgpu/regs_depth.h"
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

// A sun shadow map as the game binds it: 32-bit depth and 8-bit stencil, one to
// three 2048x2048 tiles stacked vertically.
static ImageInfo ShadowMap(VAddr depth, VAddr stencil, u32 tiles) {
    AmdGpu::DepthBuffer buffer{};
    buffer.z_info.format = AmdGpu::DepthBuffer::ZFormat::Z32Float;
    buffer.z_info.tile_mode_index = u32(AmdGpu::TileMode::Depth2DThin64);
    buffer.stencil_info.format = AmdGpu::DepthBuffer::StencilFormat::Stencil8;
    buffer.z_read_base = u32(depth >> 8);
    buffer.z_write_base = u32(depth >> 8);
    buffer.stencil_read_base = u32(stencil >> 8);
    buffer.stencil_write_base = u32(stencil >> 8);
    buffer.depth_size.pitch_tile_max = 2048 / 8 - 1;
    buffer.depth_size.height_tile_max = 2048 * tiles / 8 - 1;
    buffer.depth_slice.tile_max = 2048 * 2048 * tiles / 64 - 1;
    return ImageInfo(buffer, 1, 0, AmdGpu::CbDbExtent{});
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

    // The draw of that first face looks up its target before it binds the loaded cube, so
    // the pool-reuse rule above meets the old cube as the draw's target and keeps it. At the
    // target lookup the old cube is not in use yet (logged: drawn, then written by the loader);
    // it cannot hold the face. A texture read inside it, an image in use, and one whose GPU
    // content is still current keep the slice rule.
    assert(!CanHoldTarget(true, true, true, false));
    assert(CanHoldTarget(false, true, true, false));
    assert(CanHoldTarget(true, true, true, true));
    assert(CanHoldTarget(true, true, false, false));
    assert(CanHoldTarget(true, false, true, false));

    // The three-tile shadow map keeps its stencil where the one-tile map, used at
    // other times, keeps its depth (addresses from the game).
    const auto three_tiles = ShadowMap(0x11164e8000, 0x10de498000, 3);
    const auto one_tile = ShadowMap(0x10de498000, 0x10eb028000, 1);
    assert(three_tiles.props.is_depth && three_tiles.size.height == 6144);
    assert(one_tile.props.is_depth && one_tile.size.height == 2048);
    assert(three_tiles.stencil_addr == one_tile.guest_address);
    // That depth image is not the three-tile map's stencil plane: linked as one,
    // reads of the one-tile map went to the three-tile map.
    assert(!CanBeStencilPlane(one_tile));
    // The stencil image the cache makes for a target, and a texture the guest
    // reads from stencil memory, are.
    ImageInfo stencil{};
    stencil.guest_address = three_tiles.stencil_addr;
    stencil.guest_size = three_tiles.stencil_size;
    stencil.size = three_tiles.size;
    assert(CanBeStencilPlane(stencil));
    auto stencil_texture = Cube(three_tiles.stencil_addr, vk::Format::eR8Uint, 8, false);
    assert(CanBeStencilPlane(stencil_texture));

    std::puts("PASS: overwritten GPU image at pool reuse");
    std::puts("PASS: a render target is never placed inside an overwritten GPU image");
    std::puts("PASS: a depth image at another target's stencil address stays its own image");
}
