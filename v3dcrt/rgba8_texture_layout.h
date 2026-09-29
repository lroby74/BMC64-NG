#ifndef V3DCRT_RGBA8_TEXTURE_LAYOUT_H
#define V3DCRT_RGBA8_TEXTURE_LAYOUT_H

#include <stdint.h>

namespace v3dcrt {

enum Rgba8TextureTiling {
  kRgba8TextureLinearTile = 0,
  kRgba8TextureUbLinear1,
  kRgba8TextureUbLinear2,
  kRgba8TextureUifNoXor,
  kRgba8TextureUifXor
};

struct Rgba8TextureLayout {
  uint32_t width;
  uint32_t height;
  uint32_t padded_width;
  uint32_t padded_height;
  uint32_t padded_row_bytes;
  uint32_t size_bytes;
  uint32_t array_stride_64_bytes;
  uint32_t level_0_ub_pad;
  Rgba8TextureTiling tiling;
};

struct Rgba8RenderTargetStoreConfig {
  uint32_t memory_format;
  uint32_t height_in_ub_or_stride;
};

struct RgbFloat {
  float red;
  float green;
  float blue;
};

using Rgba8TextureRegionSource = bool (*)(void *context,
                                          uint32_t origin_x,
                                          uint32_t origin_y,
                                          uint32_t width,
                                          uint32_t height,
                                          uint32_t *rgba8,
                                          uint32_t stride_words);

bool ComputeRgba8TextureLayout(uint32_t width,
                               uint32_t height,
                               uint32_t max_array_stride,
                               Rgba8TextureLayout *layout);

bool Rgba8TextureLayoutMatchesComputed(const Rgba8TextureLayout &layout,
                                       uint32_t max_array_stride);

bool Rgba8TexturePixelOffset(const Rgba8TextureLayout &layout,
                             uint32_t x,
                             uint32_t y,
                             uint32_t *offset);

bool BuildRgba8TexturePixelOffsetTable(const Rgba8TextureLayout &layout,
                                       uint32_t max_array_stride,
                                       bool validate_layout,
                                       uint32_t *offsets,
                                       uint32_t offset_count);

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
    uint32_t max_array_stride,
    Rgba8RenderTargetStoreConfig *config);

// Writes every padded destination word in increasing physical-address order.
// Logical pixels come from source; padding is deterministically zero-filled.
bool UploadRgba8TexturePhysical(uint8_t *destination,
                                uint32_t destination_size,
                                const Rgba8TextureLayout &layout,
                                Rgba8TextureRegionSource source,
                                void *source_context);

const char *Rgba8TextureTilingName(Rgba8TextureTiling tiling);

}  // namespace v3dcrt

#endif  // V3DCRT_RGBA8_TEXTURE_LAYOUT_H
