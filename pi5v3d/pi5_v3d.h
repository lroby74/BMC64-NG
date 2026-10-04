#ifndef PI5V3D_PI5_V3D_H
#define PI5V3D_PI5_V3D_H

#include "pi5kms/pi5_kms.h"
#include "pi5v3d/pi5_render_params.h"

#include <circle/types.h>
#include <stdint.h>

namespace pi5v3d {

enum ShaderPreset {
  kShaderOff = 0,
  kShaderSharp,
  kShaderCrt,
  kShaderCrtSoft,
  kShaderFrameCopy,
  kShaderScanlines,
  kShaderFragmentProbe
};

enum BufferUsage {
  kBufferUsageSource = 0,
  kBufferUsageRenderTarget,
  kBufferUsageControl
};

enum BootTestMode {
  kBootTestOff = 0,
  kBootTestMmu,
  kBootTestSolid,
  kBootTestSource,
  kBootTestQpu,
  kBootTestQpuFill,
  kBootTestFragmentArtifact,
  kBootTestFragmentReplay,
  kBootTestFragmentScanout,
  kBootTestFragmentFullscreen,
  kBootTestFragmentSource
};

enum FragmentPackageMode {
  kFragmentPackageDefault = 0,
  kFragmentPackageMinimalDiagnostic,
  kFragmentPackageCoreDiagnostic,
  kFragmentPackageConvergenceDiagnostic,
  kFragmentPackageEdgeBlurDiagnostic,
  kFragmentPackageEdgeGlowDiagnostic,
  kFragmentPackageSurfaceResponseDiagnostic,
  kFragmentPackageMaskVignetteDiagnostic,
  kFragmentPackageIlluminationJitterDiagnostic,
  kFragmentPackageCompositeDiagnostic,
  kFragmentPackageNoiseDiagnostic
};

enum RenderResolution {
  kRenderResolutionSource = 0,
  kRenderResolutionOutput
};

struct Rect {
  u32 x;
  u32 y;
  u32 width;
  u32 height;
};

struct Buffer {
  uint8_t *allocation;
  uint8_t *cpu;
  u64 axi_bus_address;
  u32 v3d_address;
  u32 hvs_bus_address;
  u32 legacy_gpu_bus_address;
  u32 allocation_size;
  u32 size;
  u32 alignment;
  u32 width;
  u32 height;
  u32 pitch;
  u32 depth;
  BufferUsage usage;
};

struct TextureSource {
  const uint8_t *pixels;
  u32 width;
  u32 height;
  u32 pitch;
  u32 pixelmode;
  const u16 *pal565;
  u32 palette_generation;
  u32 palette_signature;
  Rect source_rect;
};

struct OutputTarget {
  pi5kms::Framebuffer *scanout;
  u32 display_width;
  u32 display_height;
  Rect destination_rect;
  bool wait_for_vblank;
  bool *presented;
  bool allow_direct_scanout;
  pi5kms::Plane *rendered_plane;
};

struct RenderReadback {
  const uint8_t *pixels;
  u32 width;
  u32 height;
  u32 pitch;
  u32 depth;
  u32 buffer_size;
  u32 target_index;
};

struct DiagnosticStatus {
  u32 hub_interrupt_status;
  u32 core_interrupt_status;
  u32 mmu_faults;
  u32 mmu_violation_id;
  u32 mmu_violation_address;
  bool runtime_failed;
};

const char *ShaderPresetName(ShaderPreset preset);

void Configure(bool requested, bool kms_active, ShaderPreset preset,
               BootTestMode boot_test_mode,
               bool fragment_probe_wait_for_vblank,
               FragmentPackageMode fragment_package_mode,
               RenderResolution render_resolution);
bool Initialize();
bool Requested();
bool IsAvailable();
bool AllocateBuffer(BufferUsage usage, u32 size, u32 alignment, Buffer *buffer);
void FreeBuffer(Buffer *buffer);
void CleanBufferForV3D(const Buffer &buffer);
void InvalidateBufferFromV3D(const Buffer &buffer);
bool RenderFullscreen(const TextureSource &source,
                      const OutputTarget &target,
                      const RenderParams &params);
bool ReadCompletedRenderTarget(RenderReadback *readback);
bool GetDiagnosticStatus(DiagnosticStatus *status);
void ResetDiagnosticFrameState();
bool RunBootTest(const OutputTarget &target);
void Shutdown();

}  // namespace pi5v3d

#endif  // PI5V3D_PI5_V3D_H
