#ifndef PI4V3D_PI4_V3D42_TEXTURE_STATE_H
#define PI4V3D_PI4_V3D42_TEXTURE_STATE_H

#include <stdint.h>

#include "v3dcrt/rgba8_texture_layout.h"

namespace pi4v3d {
namespace v3d42 {

static const uint32_t kTextureShaderStateBytes = 24U;

using v3dcrt::RgbFloat;
using v3dcrt::Rgba8RenderTargetStoreConfig;
using v3dcrt::Rgba8TextureLayout;
using v3dcrt::Rgba8TextureRegionSource;
using v3dcrt::Rgba8TextureTiling;

static const Rgba8TextureTiling kRgba8TextureLinearTile =
    v3dcrt::kRgba8TextureLinearTile;
static const Rgba8TextureTiling kRgba8TextureUbLinear1 =
    v3dcrt::kRgba8TextureUbLinear1;
static const Rgba8TextureTiling kRgba8TextureUbLinear2 =
    v3dcrt::kRgba8TextureUbLinear2;
static const Rgba8TextureTiling kRgba8TextureUifNoXor =
    v3dcrt::kRgba8TextureUifNoXor;
static const Rgba8TextureTiling kRgba8TextureUifXor =
    v3dcrt::kRgba8TextureUifXor;

bool ComputeRgba8TextureLayout(uint32_t width, uint32_t height,
                               Rgba8TextureLayout *layout);

bool Rgba8TexturePixelOffset(const Rgba8TextureLayout &layout,
                             uint32_t x, uint32_t y, uint32_t *offset);

bool BuildRgba8TexturePixelOffsetTable(const Rgba8TextureLayout &layout,
                                       uint32_t *offsets,
                                       uint32_t offset_count);

bool UploadRgba8TexturePhysical(uint8_t *destination,
                                uint32_t destination_size,
                                const Rgba8TextureLayout &layout,
                                Rgba8TextureRegionSource source,
                                void *source_context);

bool SampleRgba8Texture(const uint8_t *pixels,
                        uint32_t pixels_size,
                        const Rgba8TextureLayout &layout,
                        float u,
                        float v,
                        bool linear_filter,
                        bool flip_y,
                        RgbFloat *sample);

bool GetRgba8RenderTargetStoreConfig(
    const Rgba8TextureLayout &layout,
    Rgba8RenderTargetStoreConfig *config);

bool PackRgba8TextureShaderState(uint8_t *state, uint32_t state_size,
                                 uint32_t texture_address,
                                 const Rgba8TextureLayout &layout,
                                 bool flip_y);

const char *Rgba8TextureTilingName(Rgba8TextureTiling tiling);

}  // namespace v3d42
}  // namespace pi4v3d

#endif  // PI4V3D_PI4_V3D42_TEXTURE_STATE_H
