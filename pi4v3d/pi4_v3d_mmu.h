#ifndef PI4V3D_PI4_V3D_MMU_H
#define PI4V3D_PI4_V3D_MMU_H

#include <stdint.h>

namespace pi4v3d {

static const uint32_t kMmuPageShift = 12U;
static const uint32_t kMmuPageSize = 1U << kMmuPageShift;
static const uint32_t kMmuPageTableBytes = 4U * 1024U * 1024U;
static const uint32_t kMmuPageTableEntries =
    kMmuPageTableBytes / sizeof(uint32_t);
static const uint32_t kMmuPteValid = 1U << 28;
static const uint32_t kMmuPteWriteable = 1U << 29;

struct MmuMapping {
  uint32_t v3d_address;
  uint32_t first_page;
  uint32_t page_count;
  uint32_t first_pte;
  uint32_t last_pte;
};

// Inserts 4 KiB PTEs for one physically contiguous DMA range. The DMA page
// number is the low 24 bits of each V3D PTE; the top nibble contains flags.
bool InsertContiguousPtes(uint32_t *page_table, uint32_t table_entries,
                          uint32_t v3d_address, uint64_t dma_address,
                          uint32_t size, MmuMapping *mapping);

// Removes a previously inserted mapping and clears the mapping descriptor.
// An already empty descriptor is accepted so teardown remains idempotent.
bool ClearMmuMapping(uint32_t *page_table, uint32_t table_entries,
                     MmuMapping *mapping);

struct TfuCopyRegisters {
  uint32_t icfg;
  uint32_t iia;
  uint32_t iis;
  uint32_t ica;
  uint32_t iua;
  uint32_t ioa;
  uint32_t ios;
};

// Builds a V3D 4.2 TFU copy from raster R32F input to lineartile output.
// A single 4x4 R32F utile is byte-identical to a 4x4 raster image, which
// makes the first offscreen hardware write directly verifiable by the CPU.
bool BuildTfuR32Copy(uint32_t source_v3d_address,
                     uint32_t target_v3d_address,
                     uint32_t width, uint32_t height,
                     TfuCopyRegisters *registers);

uint32_t Fnv1a32(const void *data, uint32_t size);

}  // namespace pi4v3d

#endif  // PI4V3D_PI4_V3D_MMU_H
