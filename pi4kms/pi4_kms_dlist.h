#ifndef PI4KMS_PI4_KMS_DLIST_H
#define PI4KMS_PI4_KMS_DLIST_H

#include <stdint.h>

namespace pi4kms {

static const uint32_t kHvs5DlistWordCapacity = 4096U;
static const uint32_t kHvs5Rgb565UnityPlaneWords = 8U;
static const uint32_t kHvs5Rgb565UnityDlistWords = 9U;
static const uint32_t kHvs5MaximumPlanes = 4U;
static const uint32_t kHvs5MaximumArmDlistWords = 64U;
static const uint32_t kHvs5MitchellKernelSlot = 3024U;
static const uint32_t kHvs5NearestKernelSlot = 3040U;
static const uint32_t kHvs5FilterKernelWords = 11U;

enum Hvs5PixelFormat {
  kHvs5PixelFormatRgb565 = 0,
  kHvs5PixelFormatArgb8888
};

enum Hvs5ScaleFilter {
  kHvs5ScaleFilterNearest = 0,
  kHvs5ScaleFilterMitchell
};

struct Hvs5Plane {
  // BCM2711 HVS5-visible DMA/physical address.
  uint32_t framebuffer_bus_address;
  uint32_t pitch;
  uint32_t width;
  uint32_t height;
  Hvs5PixelFormat format;
  Hvs5ScaleFilter filter;
  uint32_t destination_x;
  uint32_t destination_y;
  uint32_t destination_width;
  uint32_t destination_height;
};

struct Hvs5FilterKernelUsage {
  bool nearest;
  bool mitchell;
};

struct Hvs5DlistInfo {
  bool end_found;
  uint32_t plane_count;
  uint32_t used_words;
  uint32_t hash;
};

struct Hvs5Rgb565UnityPlane {
  // BCM2711 HVS5-visible DMA/physical address.
  uint32_t framebuffer_bus_address;
  uint32_t pitch;
  uint32_t width;
  uint32_t height;
  uint32_t destination_x;
  uint32_t destination_y;
  uint32_t display_width;
  uint32_t display_height;
};

// Walks only CTL0 boundaries derived from each plane's SIZE field. Bit 31 in
// pointer or context words must never be mistaken for the list terminator.
bool InspectHvs5Dlist(const uint32_t *words, uint32_t word_count,
                      Hvs5DlistInfo *info);

// Builds one opaque, linear RGB565 unity plane followed by END. This is a pure
// planner: it neither allocates display-list SRAM nor writes HVS registers.
bool BuildHvs5Rgb565UnityDlist(const Hvs5Rgb565UnityPlane &plane,
                              uint32_t *words, uint32_t capacity,
                              uint32_t *word_count);

// Builds a back-to-front RGB565/ARGB8888 plane list followed by END. Scaled
// PPF planes reference the selected nearest-neighbour or Mitchell kernel in
// the fixed ARM-owned filter-kernel slots above. This remains a pure planner.
bool BuildHvs5Dlist(const Hvs5Plane *planes, uint32_t plane_count,
                    uint32_t display_width, uint32_t display_height,
                    uint32_t *words, uint32_t capacity,
                    uint32_t *word_count,
                    Hvs5FilterKernelUsage *required_kernels);

// Expands six unique coefficients into the symmetric eleven-word HVS filter
// kernel representations.
void BuildHvs5NearestKernel(uint32_t words[kHvs5FilterKernelWords]);
void BuildHvs5MitchellKernel(uint32_t words[kHvs5FilterKernelWords]);

}  // namespace pi4kms

#endif  // PI4KMS_PI4_KMS_DLIST_H
