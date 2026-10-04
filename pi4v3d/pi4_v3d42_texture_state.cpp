#include "pi4v3d/pi4_v3d42_texture_state.h"

#include <string.h>

namespace pi4v3d {
namespace v3d42 {
namespace {

const uint32_t kTextureAddressAlignment = 64U;
const uint32_t kMaxArrayStride = (1U << 26U) - 1U;
const uint32_t kTextureTypeRgba8 = 4U;
const uint32_t kSwizzleRed = 2U;
const uint32_t kSwizzleGreen = 3U;
const uint32_t kSwizzleBlue = 4U;
const uint32_t kSwizzleAlpha = 5U;

bool SetBits(uint8_t *state, uint32_t state_size, uint32_t start,
             uint32_t bit_count, uint32_t value) {
  if (state == NULL || bit_count == 0U || bit_count > 32U ||
      start + bit_count > state_size * 8U ||
      (bit_count < 32U && value >= (1U << bit_count))) {
    return false;
  }
  for (uint32_t bit = 0U; bit < bit_count; ++bit) {
    const uint32_t output_bit = start + bit;
    const uint8_t mask = static_cast<uint8_t>(1U << (output_bit & 7U));
    if ((value & (1U << bit)) != 0U) {
      state[output_bit >> 3U] |= mask;
    } else {
      state[output_bit >> 3U] &= static_cast<uint8_t>(~mask);
    }
  }
  return true;
}

bool LayoutMatchesComputed(const Rgba8TextureLayout &layout) {
  return v3dcrt::Rgba8TextureLayoutMatchesComputed(
      layout, kMaxArrayStride);
}

}  // namespace

bool ComputeRgba8TextureLayout(uint32_t width, uint32_t height,
                               Rgba8TextureLayout *layout) {
  return v3dcrt::ComputeRgba8TextureLayout(
      width, height, kMaxArrayStride, layout);
}

bool Rgba8TexturePixelOffset(const Rgba8TextureLayout &layout,
                             uint32_t x, uint32_t y, uint32_t *offset) {
  return v3dcrt::Rgba8TexturePixelOffset(layout, x, y, offset);
}

bool BuildRgba8TexturePixelOffsetTable(const Rgba8TextureLayout &layout,
                                       uint32_t *offsets,
                                       uint32_t offset_count) {
  return v3dcrt::BuildRgba8TexturePixelOffsetTable(
      layout, kMaxArrayStride, true, offsets, offset_count);
}

bool UploadRgba8TexturePhysical(uint8_t *destination,
                                uint32_t destination_size,
                                const Rgba8TextureLayout &layout,
                                Rgba8TextureRegionSource source,
                                void *source_context) {
  return v3dcrt::UploadRgba8TexturePhysical(
      destination, destination_size, layout, source, source_context);
}

bool SampleRgba8Texture(const uint8_t *pixels,
                        uint32_t pixels_size,
                        const Rgba8TextureLayout &layout,
                        float u,
                        float v,
                        bool linear_filter,
                        bool flip_y,
                        RgbFloat *sample) {
  return v3dcrt::SampleRgba8Texture(
      pixels, pixels_size, layout, u, v, linear_filter, flip_y, sample);
}

bool GetRgba8RenderTargetStoreConfig(
    const Rgba8TextureLayout &layout,
    Rgba8RenderTargetStoreConfig *config) {
  return v3dcrt::GetRgba8RenderTargetStoreConfig(
      layout, kMaxArrayStride, config);
}

bool PackRgba8TextureShaderState(uint8_t *state, uint32_t state_size,
                                 uint32_t texture_address,
                                 const Rgba8TextureLayout &layout,
                                 bool flip_y) {
  if (state == NULL || state_size < kTextureShaderStateBytes ||
      (texture_address & (kTextureAddressAlignment - 1U)) != 0U ||
      !LayoutMatchesComputed(layout)) {
    return false;
  }
  const bool strictly_uif =
      layout.tiling == kRgba8TextureUifNoXor ||
      layout.tiling == kRgba8TextureUifXor;
  const bool xor_enable = layout.tiling == kRgba8TextureUifXor;

  memset(state, 0, kTextureShaderStateBytes);
  return SetBits(state, state_size, 0U, 32U, texture_address) &&
         SetBits(state, state_size, 1U, 1U, flip_y ? 1U : 0U) &&
         SetBits(state, state_size, 32U, 26U,
                 layout.array_stride_64_bytes) &&
         SetBits(state, state_size, 58U, 14U, layout.width) &&
         SetBits(state, state_size, 72U, 14U, layout.height) &&
         SetBits(state, state_size, 86U, 14U, 1U) &&
         SetBits(state, state_size, 100U, 7U, kTextureTypeRgba8) &&
         SetBits(state, state_size, 107U, 1U,
                 strictly_uif ? 1U : 0U) &&
         SetBits(state, state_size, 108U, 3U, kSwizzleRed) &&
         SetBits(state, state_size, 111U, 3U, kSwizzleGreen) &&
         SetBits(state, state_size, 114U, 3U, kSwizzleBlue) &&
         SetBits(state, state_size, 117U, 3U, kSwizzleAlpha) &&
         SetBits(state, state_size, 128U, 4U, layout.level_0_ub_pad) &&
         SetBits(state, state_size, 132U, 1U, xor_enable ? 1U : 0U) &&
         SetBits(state, state_size, 134U, 1U,
                 strictly_uif ? 1U : 0U);
}

const char *Rgba8TextureTilingName(Rgba8TextureTiling tiling) {
  return v3dcrt::Rgba8TextureTilingName(tiling);
}

}  // namespace v3d42
}  // namespace pi4v3d
