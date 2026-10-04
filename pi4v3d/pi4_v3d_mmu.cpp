#include "pi4v3d/pi4_v3d_mmu.h"

#include <stddef.h>

namespace pi4v3d {

namespace {

const uint32_t kTfuInputFormatRaster = 0U;
const uint32_t kTfuInputFormatShift = 18U;
const uint32_t kTfuTextureTypeR32F = 29U;
const uint32_t kTfuTextureTypeShift = 9U;
const uint32_t kTfuInterruptOnCompletion = 1U << 0;
const uint32_t kTfuOutputFormatLineartile = 3U;
const uint32_t kTfuOutputFormatShift = 3U;
const uint32_t kTfuMaxEncodedDimension = 0x3fffU;

bool IsPageAligned(uint64_t value) {
  return (value & (kMmuPageSize - 1U)) == 0U;
}

}  // namespace

bool InsertContiguousPtes(uint32_t *page_table, uint32_t table_entries,
                          uint32_t v3d_address, uint64_t dma_address,
                          uint32_t size, MmuMapping *mapping) {
  if (page_table == NULL || mapping == NULL || size == 0U ||
      v3d_address == 0U || !IsPageAligned(v3d_address) ||
      !IsPageAligned(dma_address) || size > UINT32_MAX - (kMmuPageSize - 1U)) {
    return false;
  }

  const uint32_t page_count =
      (size + kMmuPageSize - 1U) >> kMmuPageShift;
  const uint32_t first_page = v3d_address >> kMmuPageShift;
  const uint64_t first_dma_page = dma_address >> kMmuPageShift;
  const uint64_t end_v3d = static_cast<uint64_t>(v3d_address) +
                           (static_cast<uint64_t>(page_count) <<
                            kMmuPageShift);

  if (page_count == 0U || first_page >= table_entries ||
      page_count > table_entries - first_page || end_v3d > (1ULL << 32) ||
      first_dma_page >= (1ULL << 24) ||
      page_count - 1U >= (1ULL << 24) - first_dma_page) {
    return false;
  }

  for (uint32_t i = 0; i < page_count; ++i) {
    page_table[first_page + i] =
        kMmuPteValid | kMmuPteWriteable |
        static_cast<uint32_t>(first_dma_page + i);
  }

  mapping->v3d_address = v3d_address;
  mapping->first_page = first_page;
  mapping->page_count = page_count;
  mapping->first_pte = page_table[first_page];
  mapping->last_pte = page_table[first_page + page_count - 1U];
  return true;
}

bool ClearMmuMapping(uint32_t *page_table, uint32_t table_entries,
                     MmuMapping *mapping) {
  if (mapping == NULL) {
    return false;
  }
  if (mapping->page_count == 0U) {
    *mapping = MmuMapping();
    return true;
  }
  if (page_table == NULL || mapping->first_page >= table_entries ||
      mapping->page_count > table_entries - mapping->first_page) {
    return false;
  }

  for (uint32_t i = 0U; i < mapping->page_count; ++i) {
    page_table[mapping->first_page + i] = 0U;
  }
  *mapping = MmuMapping();
  return true;
}

bool BuildTfuR32Copy(uint32_t source_v3d_address,
                     uint32_t target_v3d_address,
                     uint32_t width, uint32_t height,
                     TfuCopyRegisters *registers) {
  if (registers == NULL || source_v3d_address == 0U ||
      target_v3d_address == 0U ||
      (source_v3d_address & 63U) != 0U ||
      (target_v3d_address & 63U) != 0U || width == 0U || height == 0U ||
      width > kTfuMaxEncodedDimension ||
      height > kTfuMaxEncodedDimension) {
    return false;
  }

  registers->icfg =
      (kTfuInputFormatRaster << kTfuInputFormatShift) |
      (kTfuTextureTypeR32F << kTfuTextureTypeShift) |
      kTfuInterruptOnCompletion;
  registers->iia = source_v3d_address;
  registers->iis = width;
  registers->ica = 0U;
  registers->iua = 0U;
  registers->ioa = target_v3d_address |
                   (kTfuOutputFormatLineartile << kTfuOutputFormatShift);
  registers->ios = (height << 16) | width;
  return true;
}

uint32_t Fnv1a32(const void *data, uint32_t size) {
  if (data == NULL && size != 0U) {
    return 0U;
  }

  const uint8_t *bytes = static_cast<const uint8_t *>(data);
  uint32_t hash = 2166136261U;
  for (uint32_t i = 0; i < size; ++i) {
    hash ^= bytes[i];
    hash *= 16777619U;
  }
  return hash;
}

}  // namespace pi4v3d
