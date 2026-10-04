#ifndef BMX_KMS_KMS_TYPES_H
#define BMX_KMS_KMS_TYPES_H

#include <stdint.h>

namespace bmxkms {

struct Mode {
  uint32_t width;
  uint32_t height;
  uint32_t pixel_clock;
  uint16_t h_front_porch;
  uint16_t h_sync;
  uint16_t h_back_porch;
  uint16_t v_front_porch;
  uint16_t v_sync;
  uint16_t v_back_porch;
  bool h_sync_positive;
  bool v_sync_positive;
};

struct Framebuffer {
  uint8_t *allocation;
  uint8_t *pixels;
  uint32_t width;
  uint32_t height;
  uint32_t pitch;
  uint32_t depth;
  uint32_t size;
};

struct Rect {
  int32_t x;
  int32_t y;
  uint32_t width;
  uint32_t height;
};

enum PixelFormat {
  kPixelFormatRgb565 = 0,
  kPixelFormatArgb8888 = 1,
};

enum ScaleFilter {
  kScaleFilterNearest = 0,
  kScaleFilterMitchell = 1,
};

struct Plane {
  uint32_t framebuffer_bus_address;
  uint32_t pitch;
  uint32_t width;
  uint32_t height;
  uint32_t depth;
  PixelFormat format;
  ScaleFilter filter;
  Rect source;
  Rect destination;
};

struct PresentTiming {
  bool valid;
  bool wait_requested;
  uint32_t sequence;
  uint32_t wait_us;
  uint32_t total_us;
};

}  // namespace bmxkms

#endif  // BMX_KMS_KMS_TYPES_H
