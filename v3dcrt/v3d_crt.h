#ifndef V3DCRT_V3D_CRT_H
#define V3DCRT_V3D_CRT_H

#include <circle/types.h>
#include <stdint.h>

#include "v3dcrt/effect_params.h"

namespace v3dcrt {

enum ShaderPreset {
  kShaderOff = 0,
  kShaderSharp,
  kShaderCrt,
  kShaderCrtSoft,
  kShaderFrameCopy,
  kShaderScanlines,
  kShaderFragmentProbe
};

enum PixelFormat {
  kPixelFormatIndexed8 = 0,
  kPixelFormatRgb565
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
  kBootTestFragmentLifecycle,
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

struct InputFramebuffer {
  const uint8_t *pixels;
  u32 width;
  u32 height;
  u32 pitch;
  PixelFormat format;
  const u16 *palette_rgb565;
  u32 palette_generation;
  u32 palette_signature;
  Rect source_rect;
  u32 left_edge_padding;
};

struct OutputFramebuffer {
  // Board-specific scanout object owned by the board framebuffer backend.
  // The common interface treats it as opaque.
  void *native_framebuffer;
  uint8_t *pixels;
  u32 width;
  u32 height;
  u32 pitch;
  u32 depth;
  PixelFormat format;
  u32 display_width;
  u32 display_height;
  Rect destination_rect;
  bool wait_for_vblank;
  bool *presented;
  bool allow_direct_scanout;
  void *native_rendered_plane;
};

// CPU-readable view of the most recently completed V3D render target. The
// storage remains owned by the board-specific renderer and is valid only
// until that renderer reuses or releases the target.
struct OutputReadback {
  const uint8_t *pixels;
  // HVS-visible address of the same completed render target, when the board
  // renderer exposes one for native scanout.
  u32 framebuffer_bus_address;
  u32 width;
  u32 height;
  u32 pitch;
  u32 depth;
};

struct BootTestOutputLayout {
  u32 width;
  u32 height;
  u32 pitch;
  u32 depth;
  PixelFormat format;
};

ShaderPreset ParseShaderPreset(const char *name);
const char *ShaderPresetName(ShaderPreset preset);
BootTestMode ParseBootTestMode(const char *name);
const char *BootTestModeName(BootTestMode mode);
FragmentPackageMode ParseFragmentPackageMode(const char *name);
const char *FragmentPackageModeName(FragmentPackageMode mode);
RenderResolution ParseRenderResolution(const char *name);
const char *RenderResolutionName(RenderResolution resolution);

void Configure(bool requested, bool scanout_ready, ShaderPreset preset,
               BootTestMode boot_test_mode,
               bool fragment_probe_wait_for_vblank,
               FragmentPackageMode fragment_package_mode,
               RenderResolution render_resolution);
bool Initialize();
bool Requested();
bool IsAvailable();
bool RenderFrame(const InputFramebuffer &source,
                 const OutputFramebuffer &target,
                 const EffectParams &params);
bool ReadCompletedFrame(OutputReadback *readback);
uint32_t LastFrameSequence();
bool LastFrameChangedEffect();
bool GetBootTestOutputLayout(BootTestMode mode,
                             BootTestOutputLayout *layout);
bool RunBootTest(const OutputFramebuffer &target);
void Shutdown();

}  // namespace v3dcrt

#endif  // V3DCRT_V3D_CRT_H
