#include "v3dcrt/rgba8_texture_layout.h"

#include <string.h>

namespace v3dcrt {
namespace {

constexpr uint32_t kTextureAddressAlignment = 64U;
constexpr uint32_t kMaxImageDimension = (1U << 14U) - 1U;
constexpr uint32_t kRgba8BytesPerPixel = 4U;
constexpr uint32_t kUtileWidth = 4U;
constexpr uint32_t kUtileHeight = 4U;
constexpr uint32_t kUifBlockWidth = kUtileWidth * 2U;
constexpr uint32_t kUifBlockHeight = kUtileHeight * 2U;
constexpr uint32_t kUifBlocksPerColumn = 4U;
constexpr uint32_t kUifBlockBytes = 256U;
constexpr uint32_t kUifBlockRowBytes =
    kUifBlocksPerColumn * kUifBlockBytes;
constexpr uint32_t kUifPageBytes = 4096U;
constexpr uint32_t kUifBanks = 8U;
constexpr uint32_t kUifPageCacheBytes = kUifPageBytes * kUifBanks;

enum MemoryFormat {
  kMemoryFormatLinearTile = 1U,
  kMemoryFormatUbLinear1 = 2U,
  kMemoryFormatUbLinear2 = 3U,
  kMemoryFormatUifNoXor = 4U,
  kMemoryFormatUifXor = 5U
};

bool AlignUp(uint32_t value, uint32_t alignment, uint32_t *aligned) {
  if (aligned == nullptr || alignment == 0U ||
      value > UINT32_MAX - (alignment - 1U)) {
    return false;
  }
  *aligned = (value + alignment - 1U) & ~(alignment - 1U);
  return true;
}

uint32_t ComputeUifBlockPadding(uint32_t padded_height) {
  constexpr uint32_t kPageRows = kUifPageBytes / kUifBlockRowBytes;
  constexpr uint32_t kPageRowsTimesThreeHalves = (kPageRows * 3U) >> 1U;
  constexpr uint32_t kPageCacheRows =
      kUifPageCacheBytes / kUifBlockRowBytes;
  constexpr uint32_t kPageCacheMinusThreeHalves =
      kPageCacheRows - kPageRowsTimesThreeHalves;
  const uint32_t height_blocks = padded_height / kUifBlockHeight;
  const uint32_t cache_offset = height_blocks % kPageCacheRows;
  if (cache_offset == 0U) {
    return 0U;
  }
  if (cache_offset < kPageRowsTimesThreeHalves) {
    return height_blocks < kPageCacheRows ? 0U :
        kPageRowsTimesThreeHalves - cache_offset;
  }
  return cache_offset > kPageCacheMinusThreeHalves ?
      kPageCacheRows - cache_offset : 0U;
}

uint32_t UtilePixelOffset(uint32_t x, uint32_t y) {
  return (y * kUtileWidth + x) * kRgba8BytesPerPixel;
}

void WriteUtileValues(uint8_t *destination,
                      const uint32_t *values,
                      uint32_t stride_words) {
  for (uint32_t y = 0U; y < kUtileHeight; ++y) {
    memcpy(destination + y * kUtileWidth * kRgba8BytesPerPixel,
           values + y * stride_words,
           kUtileWidth * kRgba8BytesPerPixel);
  }
}

uint32_t ValidRegionExtent(uint32_t origin,
                           uint32_t logical_extent,
                           uint32_t region_extent) {
  if (origin >= logical_extent) {
    return 0U;
  }
  const uint32_t remaining = logical_extent - origin;
  return remaining < region_extent ? remaining : region_extent;
}

}  // namespace

bool ComputeRgba8TextureLayout(uint32_t width,
                               uint32_t height,
                               uint32_t max_array_stride,
                               Rgba8TextureLayout *layout) {
  if (layout == nullptr || width == 0U || width > kMaxImageDimension ||
      height == 0U || height > kMaxImageDimension ||
      max_array_stride == 0U) {
    return false;
  }

  Rgba8TextureLayout result = {};
  result.width = width;
  result.height = height;
  if (width <= kUtileWidth || height <= kUtileHeight) {
    result.tiling = kRgba8TextureLinearTile;
    if (!AlignUp(width, kUtileWidth, &result.padded_width) ||
        !AlignUp(height, kUtileHeight, &result.padded_height)) {
      return false;
    }
  } else if (width <= kUifBlockWidth) {
    result.tiling = kRgba8TextureUbLinear1;
    if (!AlignUp(width, kUifBlockWidth, &result.padded_width) ||
        !AlignUp(height, kUifBlockHeight, &result.padded_height)) {
      return false;
    }
  } else if (width <= 2U * kUifBlockWidth) {
    result.tiling = kRgba8TextureUbLinear2;
    if (!AlignUp(width, 2U * kUifBlockWidth, &result.padded_width) ||
        !AlignUp(height, kUifBlockHeight, &result.padded_height)) {
      return false;
    }
  } else {
    if (!AlignUp(width, kUifBlocksPerColumn * kUifBlockWidth,
                 &result.padded_width) ||
        !AlignUp(height, kUifBlockHeight, &result.padded_height)) {
      return false;
    }
    result.level_0_ub_pad = ComputeUifBlockPadding(result.padded_height);
    if (result.level_0_ub_pad >
        (UINT32_MAX - result.padded_height) / kUifBlockHeight) {
      return false;
    }
    result.padded_height += result.level_0_ub_pad * kUifBlockHeight;
    const uint32_t padded_block_rows =
        result.padded_height / kUifBlockHeight;
    result.tiling =
        padded_block_rows % (kUifPageCacheBytes / kUifBlockRowBytes) == 0U ?
            kRgba8TextureUifXor : kRgba8TextureUifNoXor;
  }

  const uint64_t row_bytes =
      static_cast<uint64_t>(result.padded_width) * kRgba8BytesPerPixel;
  const uint64_t size_bytes = row_bytes * result.padded_height;
  if (row_bytes > UINT32_MAX || size_bytes > UINT32_MAX ||
      size_bytes == 0U ||
      (size_bytes & (kTextureAddressAlignment - 1U)) != 0U) {
    return false;
  }
  const uint64_t array_stride = size_bytes / kTextureAddressAlignment;
  if (array_stride == 0U || array_stride > max_array_stride) {
    return false;
  }
  result.padded_row_bytes = static_cast<uint32_t>(row_bytes);
  result.size_bytes = static_cast<uint32_t>(size_bytes);
  result.array_stride_64_bytes = static_cast<uint32_t>(array_stride);
  *layout = result;
  return true;
}

bool Rgba8TextureLayoutMatchesComputed(const Rgba8TextureLayout &layout,
                                       uint32_t max_array_stride) {
  Rgba8TextureLayout expected = {};
  return ComputeRgba8TextureLayout(
             layout.width, layout.height, max_array_stride, &expected) &&
         layout.padded_width == expected.padded_width &&
         layout.padded_height == expected.padded_height &&
         layout.padded_row_bytes == expected.padded_row_bytes &&
         layout.size_bytes == expected.size_bytes &&
         layout.array_stride_64_bytes == expected.array_stride_64_bytes &&
         layout.level_0_ub_pad == expected.level_0_ub_pad &&
         layout.tiling == expected.tiling;
}

bool Rgba8TexturePixelOffset(const Rgba8TextureLayout &layout,
                             uint32_t x,
                             uint32_t y,
                             uint32_t *offset) {
  if (offset == nullptr || x >= layout.width || y >= layout.height ||
      layout.padded_width < layout.width ||
      layout.padded_height < layout.height || layout.size_bytes == 0U) {
    return false;
  }
  uint32_t result = 0U;
  const uint32_t utile_x = x & (kUtileWidth - 1U);
  const uint32_t utile_y = y & (kUtileHeight - 1U);
  switch (layout.tiling) {
    case kRgba8TextureLinearTile: {
      const uint32_t utile_index_x = x / kUtileWidth;
      const uint32_t utile_index_y = y / kUtileHeight;
      if (utile_index_x != 0U && utile_index_y != 0U) {
        return false;
      }
      result = 64U * (utile_index_x + utile_index_y) +
               UtilePixelOffset(utile_x, utile_y);
      break;
    }
    case kRgba8TextureUbLinear1:
    case kRgba8TextureUbLinear2: {
      const uint32_t columns =
          layout.tiling == kRgba8TextureUbLinear1 ? 1U : 2U;
      result = kUifBlockBytes *
                   ((y / kUifBlockHeight) * columns +
                    x / kUifBlockWidth) +
               ((x & kUtileWidth) != 0U ? 64U : 0U) +
               ((y & kUtileHeight) != 0U ? 128U : 0U) +
               UtilePixelOffset(utile_x, utile_y);
      break;
    }
    case kRgba8TextureUifNoXor:
    case kRgba8TextureUifXor: {
      const uint32_t block_x = x / kUifBlockWidth;
      uint32_t block_y = y / kUifBlockHeight;
      if (layout.tiling == kRgba8TextureUifXor &&
          ((block_x / kUifBlocksPerColumn) & 1U) != 0U) {
        block_y ^= 0x10U;
      }
      const uint32_t block_rows =
          layout.padded_height / kUifBlockHeight;
      const uint32_t block_id =
          (block_x / kUifBlocksPerColumn) *
              ((block_rows - 1U) * kUifBlocksPerColumn) +
          block_x + block_y * kUifBlocksPerColumn;
      result = block_id * kUifBlockBytes +
               ((x & kUtileWidth) != 0U ? 64U : 0U) +
               ((y & kUtileHeight) != 0U ? 128U : 0U) +
               UtilePixelOffset(utile_x, utile_y);
      break;
    }
    default:
      return false;
  }
  if (result > layout.size_bytes - kRgba8BytesPerPixel) {
    return false;
  }
  *offset = result;
  return true;
}

bool BuildRgba8TexturePixelOffsetTable(const Rgba8TextureLayout &layout,
                                       uint32_t max_array_stride,
                                       bool validate_layout,
                                       uint32_t *offsets,
                                       uint32_t offset_count) {
  const uint64_t pixel_count =
      static_cast<uint64_t>(layout.width) * layout.height;
  if (offsets == nullptr || pixel_count == 0U || pixel_count > UINT32_MAX ||
      offset_count < pixel_count ||
      (validate_layout && !Rgba8TextureLayoutMatchesComputed(
          layout, max_array_stride))) {
    return false;
  }
  uint32_t index = 0U;
  for (uint32_t y = 0U; y < layout.height; ++y) {
    for (uint32_t x = 0U; x < layout.width; ++x) {
      if (!Rgba8TexturePixelOffset(layout, x, y, &offsets[index++])) {
        return false;
      }
    }
  }
  return true;
}

bool SampleRgba8Texture(const uint8_t *pixels,
                        uint32_t pixels_size,
                        const Rgba8TextureLayout &layout,
                        float u,
                        float v,
                        bool linear_filter,
                        bool flip_y,
                        RgbFloat *sample) {
  if (pixels == nullptr || sample == nullptr ||
      pixels_size < layout.size_bytes ||
      layout.width == 0U || layout.height == 0U ||
      !(u >= 0.0f && u <= 1.0f) || !(v >= 0.0f && v <= 1.0f)) {
    return false;
  }
  const float texture_x = u * static_cast<float>(layout.width) -
      (linear_filter ? 0.5f : 0.0f);
  const float texture_y = v * static_cast<float>(layout.height) -
      (linear_filter ? 0.5f : 0.0f);
  uint32_t x0 = texture_x > 0.0f ? static_cast<uint32_t>(texture_x) : 0U;
  uint32_t y0 = texture_y > 0.0f ? static_cast<uint32_t>(texture_y) : 0U;
  if (x0 >= layout.width) x0 = layout.width - 1U;
  if (y0 >= layout.height) y0 = layout.height - 1U;
  const uint32_t x1 =
      linear_filter && x0 + 1U < layout.width ? x0 + 1U : x0;
  const uint32_t y1 =
      linear_filter && y0 + 1U < layout.height ? y0 + 1U : y0;
  const float x_weight = texture_x > 0.0f && x1 != x0 ?
      texture_x - static_cast<float>(x0) : 0.0f;
  const float y_weight = texture_y > 0.0f && y1 != y0 ?
      texture_y - static_cast<float>(y0) : 0.0f;
  const uint32_t sample_x[2] = {x0, x1};
  const uint32_t sample_y[2] = {
    flip_y ? layout.height - 1U - y0 : y0,
    flip_y ? layout.height - 1U - y1 : y1,
  };
  RgbFloat texels[2][2] = {};
  for (uint32_t yi = 0U; yi < 2U; ++yi) {
    for (uint32_t xi = 0U; xi < 2U; ++xi) {
      uint32_t offset = 0U;
      if (!Rgba8TexturePixelOffset(
              layout, sample_x[xi], sample_y[yi], &offset) ||
          offset > pixels_size - kRgba8BytesPerPixel) {
        return false;
      }
      texels[yi][xi].red = static_cast<float>(pixels[offset]) / 255.0f;
      texels[yi][xi].green =
          static_cast<float>(pixels[offset + 1U]) / 255.0f;
      texels[yi][xi].blue =
          static_cast<float>(pixels[offset + 2U]) / 255.0f;
    }
  }
  const float inverse_x = 1.0f - x_weight;
  const float inverse_y = 1.0f - y_weight;
  sample->red =
      (texels[0][0].red * inverse_x + texels[0][1].red * x_weight) *
          inverse_y +
      (texels[1][0].red * inverse_x + texels[1][1].red * x_weight) *
          y_weight;
  sample->green =
      (texels[0][0].green * inverse_x + texels[0][1].green * x_weight) *
          inverse_y +
      (texels[1][0].green * inverse_x + texels[1][1].green * x_weight) *
          y_weight;
  sample->blue =
      (texels[0][0].blue * inverse_x + texels[0][1].blue * x_weight) *
          inverse_y +
      (texels[1][0].blue * inverse_x + texels[1][1].blue * x_weight) *
          y_weight;
  return true;
}

bool GetRgba8RenderTargetStoreConfig(
    const Rgba8TextureLayout &layout,
    uint32_t max_array_stride,
    Rgba8RenderTargetStoreConfig *config) {
  if (config == nullptr ||
      !Rgba8TextureLayoutMatchesComputed(layout, max_array_stride)) {
    return false;
  }
  Rgba8RenderTargetStoreConfig result = {};
  switch (layout.tiling) {
    case kRgba8TextureLinearTile:
      result.memory_format = kMemoryFormatLinearTile;
      break;
    case kRgba8TextureUbLinear1:
      result.memory_format = kMemoryFormatUbLinear1;
      break;
    case kRgba8TextureUbLinear2:
      result.memory_format = kMemoryFormatUbLinear2;
      break;
    case kRgba8TextureUifNoXor:
      result.memory_format = kMemoryFormatUifNoXor;
      result.height_in_ub_or_stride =
          layout.padded_height / kUifBlockHeight;
      break;
    case kRgba8TextureUifXor:
      result.memory_format = kMemoryFormatUifXor;
      result.height_in_ub_or_stride =
          layout.padded_height / kUifBlockHeight;
      break;
    default:
      return false;
  }
  *config = result;
  return true;
}

bool UploadRgba8TexturePhysical(uint8_t *destination,
                                uint32_t destination_size,
                                const Rgba8TextureLayout &layout,
                                Rgba8TextureRegionSource source,
                                void *source_context) {
  if (destination == nullptr || source == nullptr ||
      destination_size < layout.size_bytes || layout.size_bytes == 0U) {
    return false;
  }
  uint8_t *output = destination;
  switch (layout.tiling) {
    case kRgba8TextureLinearTile: {
      const uint32_t utiles_x = layout.padded_width / kUtileWidth;
      const uint32_t utiles_y = layout.padded_height / kUtileHeight;
      for (uint32_t utile_y = 0U; utile_y < utiles_y; ++utile_y) {
        for (uint32_t utile_x = 0U; utile_x < utiles_x; ++utile_x) {
          const uint32_t origin_x = utile_x * kUtileWidth;
          const uint32_t origin_y = utile_y * kUtileHeight;
          uint32_t values[kUtileWidth * kUtileHeight] = {};
          const uint32_t width = ValidRegionExtent(
              origin_x, layout.width, kUtileWidth);
          const uint32_t height = ValidRegionExtent(
              origin_y, layout.height, kUtileHeight);
          if ((width != 0U && height != 0U &&
               !source(source_context, origin_x, origin_y, width, height,
                       values, kUtileWidth))) {
            return false;
          }
          WriteUtileValues(output, values, kUtileWidth);
          output += 64U;
        }
      }
      break;
    }
    case kRgba8TextureUbLinear1:
    case kRgba8TextureUbLinear2: {
      const uint32_t columns =
          layout.tiling == kRgba8TextureUbLinear1 ? 1U : 2U;
      const uint32_t rows = layout.padded_height / kUifBlockHeight;
      for (uint32_t block_y = 0U; block_y < rows; ++block_y) {
        for (uint32_t block_x = 0U; block_x < columns; ++block_x) {
          const uint32_t origin_x = block_x * kUifBlockWidth;
          const uint32_t origin_y = block_y * kUifBlockHeight;
          uint32_t values[kUifBlockWidth * kUifBlockHeight] = {};
          const uint32_t width = ValidRegionExtent(
              origin_x, layout.width, kUifBlockWidth);
          const uint32_t height = ValidRegionExtent(
              origin_y, layout.height, kUifBlockHeight);
          if ((width != 0U && height != 0U &&
               !source(source_context, origin_x, origin_y, width, height,
                       values, kUifBlockWidth))) {
            return false;
          }
          WriteUtileValues(output, values, kUifBlockWidth);
          WriteUtileValues(output + 64U, values + kUtileWidth,
                           kUifBlockWidth);
          WriteUtileValues(output + 128U,
                           values + kUtileHeight * kUifBlockWidth,
                           kUifBlockWidth);
          WriteUtileValues(output + 192U,
                           values + kUtileHeight * kUifBlockWidth +
                               kUtileWidth,
                           kUifBlockWidth);
          output += kUifBlockBytes;
        }
      }
      break;
    }
    case kRgba8TextureUifNoXor:
    case kRgba8TextureUifXor: {
      const uint32_t groups =
          layout.padded_width /
          (kUifBlocksPerColumn * kUifBlockWidth);
      const uint32_t rows = layout.padded_height / kUifBlockHeight;
      for (uint32_t group = 0U; group < groups; ++group) {
        for (uint32_t physical_y = 0U; physical_y < rows; ++physical_y) {
          const uint32_t logical_y =
              layout.tiling == kRgba8TextureUifXor && (group & 1U) != 0U ?
                  physical_y ^ 0x10U : physical_y;
          for (uint32_t column = 0U;
               column < kUifBlocksPerColumn; ++column) {
            const uint32_t origin_x =
                (group * kUifBlocksPerColumn + column) * kUifBlockWidth;
            const uint32_t origin_y = logical_y * kUifBlockHeight;
            uint32_t values[kUifBlockWidth * kUifBlockHeight] = {};
            const uint32_t width = ValidRegionExtent(
                origin_x, layout.width, kUifBlockWidth);
            const uint32_t height = ValidRegionExtent(
                origin_y, layout.height, kUifBlockHeight);
            if ((width != 0U && height != 0U &&
                 !source(source_context, origin_x, origin_y, width, height,
                         values, kUifBlockWidth))) {
              return false;
            }
            WriteUtileValues(output, values, kUifBlockWidth);
            WriteUtileValues(output + 64U, values + kUtileWidth,
                             kUifBlockWidth);
            WriteUtileValues(output + 128U,
                             values + kUtileHeight * kUifBlockWidth,
                             kUifBlockWidth);
            WriteUtileValues(output + 192U,
                             values + kUtileHeight * kUifBlockWidth +
                                 kUtileWidth,
                             kUifBlockWidth);
            output += kUifBlockBytes;
          }
        }
      }
      break;
    }
    default:
      return false;
  }
  return output == destination + layout.size_bytes;
}

const char *Rgba8TextureTilingName(Rgba8TextureTiling tiling) {
  switch (tiling) {
    case kRgba8TextureLinearTile: return "lineartile";
    case kRgba8TextureUbLinear1: return "ublinear-1";
    case kRgba8TextureUbLinear2: return "ublinear-2";
    case kRgba8TextureUifNoXor: return "uif";
    case kRgba8TextureUifXor: return "uif-xor";
    default: return "unknown";
  }
}

}  // namespace v3dcrt
