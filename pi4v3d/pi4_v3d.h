#ifndef PI4V3D_PI4_V3D_H
#define PI4V3D_PI4_V3D_H

#include <stdint.h>

#include "v3dcrt/effect_params.h"

namespace pi4v3d {

static const uint32_t kBootScanoutWidth = 16U;
static const uint32_t kBootScanoutHeight = 16U;
static const uint32_t kBootScanoutPitch =
    kBootScanoutWidth * sizeof(uint16_t);

struct ScanoutTarget {
  uint8_t *pixels;
  uint32_t width;
  uint32_t height;
  uint32_t pitch;
};

enum FrameSourceFormat {
  kFrameSourceIndexed8 = 0,
  kFrameSourceRgb565
};

struct FrameSource {
  const uint8_t *pixels;
  uint32_t width;
  uint32_t height;
  uint32_t pitch;
  FrameSourceFormat format;
  const uint16_t *palette_rgb565;
  uint32_t source_x;
  uint32_t source_y;
  uint32_t source_width;
  uint32_t source_height;
  uint32_t left_edge_padding;
};

struct FrameTarget {
  uint8_t *pixels;
  uint32_t width;
  uint32_t height;
  uint32_t pitch;
  // Native KMS can scan out the completed V3D allocation directly.  Legacy
  // display paths still request a CPU-visible copy into their staging target.
  bool copy_to_target;
};

struct RenderedFrame {
  bool valid;
  const uint8_t *pixels;
  // HVS5-visible DMA/physical address, without the legacy VideoCore alias.
  uint32_t framebuffer_bus_address;
  uint32_t width;
  uint32_t height;
  uint32_t pitch;
  uint32_t sequence;
  uint32_t slot;
};

using FrameParams = v3dcrt::EffectParams;

void Configure(bool requested, const char *shader_preset,
               const char *boot_test);
bool Initialize();
bool Requested();
bool IsAvailable();
bool RenderFrame(const FrameSource &source, const FrameTarget &target,
                 const FrameParams &params);
bool GetLastRenderedFrame(RenderedFrame *frame);
uint32_t LastFrameSequence();
bool LastFrameChangedEffect();
bool RunBootTest(const ScanoutTarget &target);
void Shutdown();

}  // namespace pi4v3d

#endif  // PI4V3D_PI4_V3D_H
