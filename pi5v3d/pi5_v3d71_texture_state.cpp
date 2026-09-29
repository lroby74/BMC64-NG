#include "pi5v3d/pi5_v3d71_texture_state.h"

#include <string.h>

namespace pi5v3d {
namespace v3d71 {
namespace {

constexpr uint32_t kTextureAddressAlignment = 64U;
constexpr uint32_t kMaxArrayStride = (1U << 24U) - 1U;
constexpr uint32_t kTextureTypeRgba8 = 4U;
constexpr uint32_t kSwizzleRed = 2U;
constexpr uint32_t kSwizzleGreen = 3U;
constexpr uint32_t kSwizzleBlue = 4U;
constexpr uint32_t kSwizzleAlpha = 5U;
constexpr uint8_t kSamplerMipNearest = 1U << 2U;
constexpr uint8_t kSamplerMinNearest = 1U << 1U;
constexpr uint8_t kSamplerMagNearest = 1U;
constexpr uint8_t kWrapClamp = 1U;

bool SetBits(uint8_t *state,
             uint32_t state_size,
             uint32_t start,
             uint32_t bit_count,
             uint32_t value) {
  if (state == nullptr || bit_count == 0U || bit_count > 32U ||
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

bool ComputeRgba8TextureLayout(uint32_t width,
                               uint32_t height,
                               Rgba8TextureLayout *layout) {
  return v3dcrt::ComputeRgba8TextureLayout(
      width, height, kMaxArrayStride, layout);
}

bool Rgba8TexturePixelOffset(const Rgba8TextureLayout &layout,
                             uint32_t x,
                             uint32_t y,
                             uint32_t *offset) {
  return v3dcrt::Rgba8TexturePixelOffset(layout, x, y, offset);
}

bool BuildRgba8TexturePixelOffsetTable(const Rgba8TextureLayout &layout,
                                       uint32_t *offsets,
                                       uint32_t offset_count) {
  return v3dcrt::BuildRgba8TexturePixelOffsetTable(
      layout, kMaxArrayStride, false, offsets, offset_count);
}

bool UploadRgba8TexturePhysical(uint8_t *destination,
                                uint32_t destination_size,
                                const Rgba8TextureLayout &layout,
                                Rgba8TextureRegionSource source,
                                void *source_context) {
  return v3dcrt::UploadRgba8TexturePhysical(
      destination, destination_size, layout, source, source_context);
}

uint32_t Rgba8TextureWordFromRgb565(uint16_t rgb) {
  uint32_t red = (rgb >> 11U) & 0x1FU;
  uint32_t green = (rgb >> 5U) & 0x3FU;
  uint32_t blue = rgb & 0x1FU;
  red = (red << 3U) | (red >> 2U);
  green = (green << 2U) | (green >> 4U);
  blue = (blue << 3U) | (blue >> 2U);
  return 0xFF000000U | (blue << 16U) | (green << 8U) | red;
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

bool PackRgba8TextureShaderState(uint8_t *state,
                                 uint32_t state_size,
                                 uint32_t texture_address,
                                 const Rgba8TextureLayout &layout,
                                 bool flip_y) {
  if (state == nullptr || state_size < kTextureShaderStateBytes ||
      (texture_address & (kTextureAddressAlignment - 1U)) != 0U ||
      !LayoutMatchesComputed(layout)) {
    return false;
  }
  const bool strictly_uif =
      layout.tiling == kRgba8TextureUifNoXor ||
      layout.tiling == kRgba8TextureUifXor;
  const bool xor_enable = layout.tiling == kRgba8TextureUifXor;

  memset(state, 0, kTextureShaderStateBytes);
  const uint32_t chroma_pointer = texture_address >> 6U;
  return SetBits(state, state_size, 0U, 32U, texture_address) &&
         SetBits(state, state_size, 1U, 1U, flip_y ? 1U : 0U) &&
         SetBits(state, state_size, 33U, 24U,
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
                 strictly_uif ? 1U : 0U) &&
         SetBits(state, state_size, 136U, 1U, 1U) &&
         SetBits(state, state_size, 137U, 1U, 1U) &&
         SetBits(state, state_size, 138U, 26U, chroma_pointer) &&
         SetBits(state, state_size, 164U, 26U, chroma_pointer);
}

const char *Rgba8TextureTilingName(Rgba8TextureTiling tiling) {
  return v3dcrt::Rgba8TextureTilingName(tiling);
}

bool PackSamplerState(uint8_t *state,
                      uint32_t state_size,
                      bool linear_filter) {
  if (state == nullptr || state_size < kSamplerStateBytes) {
    return false;
  }
  memset(state, 0, kSamplerStateBytes);
  state[0] = kSamplerMipNearest;
  if (!linear_filter) {
    state[0] |= kSamplerMinNearest | kSamplerMagNearest;
  }
  state[6] = kWrapClamp |
             static_cast<uint8_t>(kWrapClamp << 3U) |
             static_cast<uint8_t>(kWrapClamp << 6U);
  return true;
}

}  // namespace v3d71
}  // namespace pi5v3d
