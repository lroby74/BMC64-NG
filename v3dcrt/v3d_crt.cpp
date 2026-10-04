#include "v3dcrt/v3d_crt.h"

#if RASPPI == 5
#include "pi5v3d/pi5_v3d.h"
#elif RASPPI == 4
#include "pi4v3d/pi4_v3d.h"
#endif

#include <string.h>

namespace v3dcrt {

namespace {

#if RASPPI == 5
pi5v3d::ShaderPreset ToPi5ShaderPreset(ShaderPreset preset) {
  switch (preset) {
    case kShaderSharp:
      return pi5v3d::kShaderSharp;
    case kShaderCrt:
      return pi5v3d::kShaderCrt;
    case kShaderCrtSoft:
      return pi5v3d::kShaderCrtSoft;
    case kShaderFrameCopy:
      return pi5v3d::kShaderFrameCopy;
    case kShaderScanlines:
      return pi5v3d::kShaderScanlines;
    case kShaderFragmentProbe:
      return pi5v3d::kShaderFragmentProbe;
    case kShaderOff:
    default:
      return pi5v3d::kShaderOff;
  }
}

pi5v3d::BootTestMode ToPi5BootTestMode(BootTestMode mode) {
  switch (mode) {
    case kBootTestMmu:
      return pi5v3d::kBootTestMmu;
    case kBootTestSolid:
      return pi5v3d::kBootTestSolid;
    case kBootTestSource:
      return pi5v3d::kBootTestSource;
    case kBootTestQpu:
      return pi5v3d::kBootTestQpu;
    case kBootTestQpuFill:
      return pi5v3d::kBootTestQpuFill;
    case kBootTestFragmentArtifact:
      return pi5v3d::kBootTestFragmentArtifact;
    case kBootTestFragmentReplay:
      return pi5v3d::kBootTestFragmentReplay;
    case kBootTestFragmentLifecycle:
      return pi5v3d::kBootTestOff;
    case kBootTestFragmentScanout:
      return pi5v3d::kBootTestFragmentScanout;
    case kBootTestFragmentFullscreen:
      return pi5v3d::kBootTestFragmentFullscreen;
    case kBootTestFragmentSource:
      return pi5v3d::kBootTestFragmentSource;
    case kBootTestOff:
    default:
      return pi5v3d::kBootTestOff;
  }
}

pi5v3d::FragmentPackageMode ToPi5FragmentPackageMode(
    FragmentPackageMode mode) {
  switch (mode) {
    case kFragmentPackageMinimalDiagnostic:
      return pi5v3d::kFragmentPackageMinimalDiagnostic;
    case kFragmentPackageCoreDiagnostic:
      return pi5v3d::kFragmentPackageCoreDiagnostic;
    case kFragmentPackageConvergenceDiagnostic:
      return pi5v3d::kFragmentPackageConvergenceDiagnostic;
    case kFragmentPackageEdgeBlurDiagnostic:
      return pi5v3d::kFragmentPackageEdgeBlurDiagnostic;
    case kFragmentPackageEdgeGlowDiagnostic:
      return pi5v3d::kFragmentPackageEdgeGlowDiagnostic;
    case kFragmentPackageSurfaceResponseDiagnostic:
      return pi5v3d::kFragmentPackageSurfaceResponseDiagnostic;
    case kFragmentPackageMaskVignetteDiagnostic:
      return pi5v3d::kFragmentPackageMaskVignetteDiagnostic;
    case kFragmentPackageIlluminationJitterDiagnostic:
      return pi5v3d::kFragmentPackageIlluminationJitterDiagnostic;
    case kFragmentPackageCompositeDiagnostic:
      return pi5v3d::kFragmentPackageCompositeDiagnostic;
    case kFragmentPackageNoiseDiagnostic:
      return pi5v3d::kFragmentPackageNoiseDiagnostic;
    case kFragmentPackageDefault:
    default:
      return pi5v3d::kFragmentPackageDefault;
  }
}

pi5v3d::RenderResolution ToPi5RenderResolution(
    RenderResolution resolution) {
  return resolution == kRenderResolutionOutput
             ? pi5v3d::kRenderResolutionOutput
             : pi5v3d::kRenderResolutionSource;
}

pi5v3d::Rect ToPi5Rect(const Rect &rect) {
  pi5v3d::Rect pi5_rect = {
    rect.x,
    rect.y,
    rect.width,
    rect.height
  };
  return pi5_rect;
}

u32 ToPi5PixelMode(PixelFormat format) {
  return format == kPixelFormatRgb565 ? 1U : 0U;
}

#elif RASPPI == 4
pi4v3d::FrameSourceFormat ToPi4SourceFormat(PixelFormat format) {
  return format == kPixelFormatRgb565 ? pi4v3d::kFrameSourceRgb565
                                      : pi4v3d::kFrameSourceIndexed8;
}
#elif RASPPI != 4
bool g_requested = false;
ShaderPreset g_shader_preset = kShaderOff;
BootTestMode g_boot_test_mode = kBootTestOff;
#endif

}  // namespace

namespace {

template <typename Value>
struct NameAlias {
  const char *name;
  Value value;
};

template <typename Value, unsigned Count>
Value ParseName(const char *name, const NameAlias<Value> (&aliases)[Count],
                Value fallback) {
  if (name == nullptr || name[0] == '\0') {
    return fallback;
  }
  for (unsigned i = 0; i < Count; ++i) {
    if (strcmp(name, aliases[i].name) == 0) {
      return aliases[i].value;
    }
  }
  return fallback;
}

template <typename Value, unsigned Count>
const char *CanonicalName(Value value, const char *const (&names)[Count],
                          const char *fallback) {
  const unsigned index = static_cast<unsigned>(value);
  return index < Count ? names[index] : fallback;
}

#define BMX_ALIAS(name, value) {name, value}

const NameAlias<ShaderPreset> kShaderPresetAliases[] = {
  BMX_ALIAS("off", kShaderOff), BMX_ALIAS("sharp", kShaderSharp),
  BMX_ALIAS("crt", kShaderCrt), BMX_ALIAS("crt_soft", kShaderCrtSoft),
  BMX_ALIAS("crt-soft", kShaderCrtSoft),
  BMX_ALIAS("frame_copy", kShaderFrameCopy),
  BMX_ALIAS("frame-copy", kShaderFrameCopy),
  BMX_ALIAS("qpu_copy", kShaderFrameCopy),
  BMX_ALIAS("qpu-copy", kShaderFrameCopy),
  BMX_ALIAS("scanlines", kShaderScanlines),
  BMX_ALIAS("scanline", kShaderScanlines),
  BMX_ALIAS("qpu_scanlines", kShaderScanlines),
  BMX_ALIAS("qpu-scanlines", kShaderScanlines),
  BMX_ALIAS("fragment_probe", kShaderFragmentProbe),
  BMX_ALIAS("fragment-probe", kShaderFragmentProbe),
  BMX_ALIAS("fragment", kShaderFragmentProbe),
  BMX_ALIAS("qpu_fragment", kShaderFragmentProbe),
  BMX_ALIAS("qpu-fragment", kShaderFragmentProbe),
};
const char *const kShaderPresetNames[] = {
  "off", "sharp", "crt", "crt_soft", "frame_copy", "scanlines",
  "fragment_probe"
};

const NameAlias<BootTestMode> kBootTestAliases[] = {
  BMX_ALIAS("off", kBootTestOff), BMX_ALIAS("mmu", kBootTestMmu),
  BMX_ALIAS("solid", kBootTestSolid),
  BMX_ALIAS("solid_color", kBootTestSolid),
  BMX_ALIAS("solid-colour", kBootTestSolid),
  BMX_ALIAS("solid-color", kBootTestSolid),
  BMX_ALIAS("source", kBootTestSource),
  BMX_ALIAS("source_buffer", kBootTestSource),
  BMX_ALIAS("source-buffer", kBootTestSource),
  BMX_ALIAS("texture_source", kBootTestSource),
  BMX_ALIAS("texture-source", kBootTestSource),
  BMX_ALIAS("qpu", kBootTestQpu), BMX_ALIAS("csd", kBootTestQpu),
  BMX_ALIAS("compute", kBootTestQpu), BMX_ALIAS("qpu-csd", kBootTestQpu),
  BMX_ALIAS("qpu_csd", kBootTestQpu),
  BMX_ALIAS("qpu_fill", kBootTestQpuFill),
  BMX_ALIAS("qpu-fill", kBootTestQpuFill),
  BMX_ALIAS("qpu_pattern", kBootTestQpuFill),
  BMX_ALIAS("qpu-pattern", kBootTestQpuFill),
  BMX_ALIAS("compute-fill", kBootTestQpuFill),
  BMX_ALIAS("compute_fill", kBootTestQpuFill),
  BMX_ALIAS("fragment_artifact", kBootTestFragmentArtifact),
  BMX_ALIAS("fragment-artifact", kBootTestFragmentArtifact),
  BMX_ALIAS("mesa_fragment", kBootTestFragmentArtifact),
  BMX_ALIAS("mesa-fragment", kBootTestFragmentArtifact),
  BMX_ALIAS("shader_artifact", kBootTestFragmentArtifact),
  BMX_ALIAS("shader-artifact", kBootTestFragmentArtifact),
  BMX_ALIAS("fragment_replay", kBootTestFragmentReplay),
  BMX_ALIAS("fragment-replay", kBootTestFragmentReplay),
  BMX_ALIAS("mesa_replay", kBootTestFragmentReplay),
  BMX_ALIAS("mesa-replay", kBootTestFragmentReplay),
  BMX_ALIAS("shader_replay", kBootTestFragmentReplay),
  BMX_ALIAS("shader-replay", kBootTestFragmentReplay),
  BMX_ALIAS("fragment_lifecycle", kBootTestFragmentLifecycle),
  BMX_ALIAS("fragment-lifecycle", kBootTestFragmentLifecycle),
  BMX_ALIAS("render_lifecycle", kBootTestFragmentLifecycle),
  BMX_ALIAS("render-lifecycle", kBootTestFragmentLifecycle),
  BMX_ALIAS("fragment_scanout", kBootTestFragmentScanout),
  BMX_ALIAS("fragment-scanout", kBootTestFragmentScanout),
  BMX_ALIAS("mesa_scanout", kBootTestFragmentScanout),
  BMX_ALIAS("mesa-scanout", kBootTestFragmentScanout),
  BMX_ALIAS("shader_scanout", kBootTestFragmentScanout),
  BMX_ALIAS("shader-scanout", kBootTestFragmentScanout),
  BMX_ALIAS("fragment_fullscreen", kBootTestFragmentFullscreen),
  BMX_ALIAS("fragment-fullscreen", kBootTestFragmentFullscreen),
  BMX_ALIAS("mesa_fullscreen", kBootTestFragmentFullscreen),
  BMX_ALIAS("mesa-fullscreen", kBootTestFragmentFullscreen),
  BMX_ALIAS("shader_fullscreen", kBootTestFragmentFullscreen),
  BMX_ALIAS("shader-fullscreen", kBootTestFragmentFullscreen),
  BMX_ALIAS("fragment_source", kBootTestFragmentSource),
  BMX_ALIAS("fragment-source", kBootTestFragmentSource),
  BMX_ALIAS("fragment_texture", kBootTestFragmentSource),
  BMX_ALIAS("fragment-texture", kBootTestFragmentSource),
  BMX_ALIAS("fragment_source_texture", kBootTestFragmentSource),
  BMX_ALIAS("fragment-source-texture", kBootTestFragmentSource),
  BMX_ALIAS("mesa_source", kBootTestFragmentSource),
  BMX_ALIAS("mesa-source", kBootTestFragmentSource),
  BMX_ALIAS("shader_source", kBootTestFragmentSource),
  BMX_ALIAS("shader-source", kBootTestFragmentSource),
};
const char *const kBootTestNames[] = {
  "off", "mmu", "solid", "source", "qpu", "qpu_fill",
  "fragment_artifact", "fragment_replay", "fragment_lifecycle",
  "fragment_scanout", "fragment_fullscreen", "fragment_source"
};

const NameAlias<FragmentPackageMode> kFragmentPackageAliases[] = {
  BMX_ALIAS("default", kFragmentPackageDefault),
  BMX_ALIAS("full", kFragmentPackageDefault),
  BMX_ALIAS("minimal", kFragmentPackageMinimalDiagnostic),
  BMX_ALIAS("scanline_probe", kFragmentPackageMinimalDiagnostic),
  BMX_ALIAS("scanline-probe", kFragmentPackageMinimalDiagnostic),
  BMX_ALIAS("core", kFragmentPackageCoreDiagnostic),
  BMX_ALIAS("crt_core_probe", kFragmentPackageCoreDiagnostic),
  BMX_ALIAS("crt-core-probe", kFragmentPackageCoreDiagnostic),
  BMX_ALIAS("convergence", kFragmentPackageConvergenceDiagnostic),
  BMX_ALIAS("crt_convergence_probe", kFragmentPackageConvergenceDiagnostic),
  BMX_ALIAS("crt-convergence-probe", kFragmentPackageConvergenceDiagnostic),
  BMX_ALIAS("edge-blur", kFragmentPackageEdgeBlurDiagnostic),
  BMX_ALIAS("edge_blur", kFragmentPackageEdgeBlurDiagnostic),
  BMX_ALIAS("crt_edge_blur_probe", kFragmentPackageEdgeBlurDiagnostic),
  BMX_ALIAS("crt-edge-blur-probe", kFragmentPackageEdgeBlurDiagnostic),
  BMX_ALIAS("edge-glow", kFragmentPackageEdgeGlowDiagnostic),
  BMX_ALIAS("edge_glow", kFragmentPackageEdgeGlowDiagnostic),
  BMX_ALIAS("crt_edge_glow_probe", kFragmentPackageEdgeGlowDiagnostic),
  BMX_ALIAS("crt-edge-glow-probe", kFragmentPackageEdgeGlowDiagnostic),
  BMX_ALIAS("surface-response", kFragmentPackageSurfaceResponseDiagnostic),
  BMX_ALIAS("surface_response", kFragmentPackageSurfaceResponseDiagnostic),
  BMX_ALIAS("crt_surface_response_probe",
            kFragmentPackageSurfaceResponseDiagnostic),
  BMX_ALIAS("crt-surface-response-probe",
            kFragmentPackageSurfaceResponseDiagnostic),
  BMX_ALIAS("mask-vignette", kFragmentPackageMaskVignetteDiagnostic),
  BMX_ALIAS("mask_vignette", kFragmentPackageMaskVignetteDiagnostic),
  BMX_ALIAS("crt_mask_vignette_probe",
            kFragmentPackageMaskVignetteDiagnostic),
  BMX_ALIAS("crt-mask-vignette-probe",
            kFragmentPackageMaskVignetteDiagnostic),
  BMX_ALIAS("illumination-jitter",
            kFragmentPackageIlluminationJitterDiagnostic),
  BMX_ALIAS("illumination_jitter",
            kFragmentPackageIlluminationJitterDiagnostic),
  BMX_ALIAS("crt_illumination_jitter_probe",
            kFragmentPackageIlluminationJitterDiagnostic),
  BMX_ALIAS("crt-illumination-jitter-probe",
            kFragmentPackageIlluminationJitterDiagnostic),
  BMX_ALIAS("composite", kFragmentPackageCompositeDiagnostic),
  BMX_ALIAS("crt_composite_probe", kFragmentPackageCompositeDiagnostic),
  BMX_ALIAS("crt-composite-probe", kFragmentPackageCompositeDiagnostic),
  BMX_ALIAS("noise", kFragmentPackageNoiseDiagnostic),
  BMX_ALIAS("crt_noise_probe", kFragmentPackageNoiseDiagnostic),
  BMX_ALIAS("crt-noise-probe", kFragmentPackageNoiseDiagnostic),
};
const char *const kFragmentPackageNames[] = {
  "default", "minimal", "core", "convergence", "edge-blur", "edge-glow",
  "surface-response", "mask-vignette", "illumination-jitter", "composite",
  "noise"
};

const NameAlias<RenderResolution> kRenderResolutionAliases[] = {
  BMX_ALIAS("source", kRenderResolutionSource),
  BMX_ALIAS("legacy", kRenderResolutionSource),
  BMX_ALIAS("output", kRenderResolutionOutput),
  BMX_ALIAS("destination", kRenderResolutionOutput),
  BMX_ALIAS("native", kRenderResolutionOutput),
};
const char *const kRenderResolutionNames[] = {"source", "output"};

#undef BMX_ALIAS

}  // namespace

ShaderPreset ParseShaderPreset(const char *name) {
  return ParseName(name, kShaderPresetAliases, kShaderOff);
}

const char *ShaderPresetName(ShaderPreset preset) {
  return CanonicalName(preset, kShaderPresetNames, "off");
}

BootTestMode ParseBootTestMode(const char *name) {
  return ParseName(name, kBootTestAliases, kBootTestOff);
}

const char *BootTestModeName(BootTestMode mode) {
  return CanonicalName(mode, kBootTestNames, "off");
}

FragmentPackageMode ParseFragmentPackageMode(const char *name) {
  return ParseName(name, kFragmentPackageAliases, kFragmentPackageDefault);
}

const char *FragmentPackageModeName(FragmentPackageMode mode) {
  return CanonicalName(mode, kFragmentPackageNames, "default");
}

RenderResolution ParseRenderResolution(const char *name) {
  return ParseName(name, kRenderResolutionAliases, kRenderResolutionSource);
}

const char *RenderResolutionName(RenderResolution resolution) {
  return CanonicalName(resolution, kRenderResolutionNames, "source");
}

void Configure(bool requested, bool scanout_ready, ShaderPreset preset,
               BootTestMode boot_test_mode,
               bool fragment_probe_wait_for_vblank,
               FragmentPackageMode fragment_package_mode,
               RenderResolution render_resolution) {
#if RASPPI == 5
  pi5v3d::Configure(requested, scanout_ready, ToPi5ShaderPreset(preset),
                    ToPi5BootTestMode(boot_test_mode),
                    fragment_probe_wait_for_vblank,
                    ToPi5FragmentPackageMode(fragment_package_mode),
                    ToPi5RenderResolution(render_resolution));
#elif RASPPI == 4
  (void)scanout_ready;
  (void)fragment_probe_wait_for_vblank;
  (void)fragment_package_mode;
  (void)render_resolution;
  pi4v3d::Configure(requested, ShaderPresetName(preset),
                    BootTestModeName(boot_test_mode));
#else
  (void)scanout_ready;
  (void)fragment_probe_wait_for_vblank;
  (void)fragment_package_mode;
  (void)render_resolution;
  g_requested = requested;
  g_shader_preset = preset;
  g_boot_test_mode = boot_test_mode;
#endif
}

bool Initialize() {
#if RASPPI == 5
  return pi5v3d::Initialize();
#elif RASPPI == 4
  return pi4v3d::Initialize();
#else
  return false;
#endif
}

bool Requested() {
#if RASPPI == 5
  return pi5v3d::Requested();
#elif RASPPI == 4
  return pi4v3d::Requested();
#else
  return g_requested;
#endif
}

bool IsAvailable() {
#if RASPPI == 5
  return pi5v3d::IsAvailable();
#elif RASPPI == 4
  return pi4v3d::IsAvailable();
#else
  return false;
#endif
}

bool RenderFrame(const InputFramebuffer &source,
                 const OutputFramebuffer &target,
                 const EffectParams &params) {
  if (target.presented != nullptr) {
    *target.presented = false;
  }
#if RASPPI == 5
  if (source.left_edge_padding != 0U) {
    return false;
  }
  pi5v3d::TextureSource pi5_source = {
    source.pixels,
    source.width,
    source.height,
    source.pitch,
    ToPi5PixelMode(source.format),
    source.palette_rgb565,
    source.palette_generation,
    source.palette_signature,
    ToPi5Rect(source.source_rect)
  };
  pi5v3d::OutputTarget pi5_target = {
    static_cast<pi5kms::Framebuffer *>(target.native_framebuffer),
    target.display_width,
    target.display_height,
    ToPi5Rect(target.destination_rect),
    target.wait_for_vblank,
    target.presented,
    target.allow_direct_scanout,
    static_cast<pi5kms::Plane *>(target.native_rendered_plane)
  };
  return pi5v3d::RenderFullscreen(pi5_source, pi5_target,
                                  params);
#elif RASPPI == 4
  pi4v3d::FrameSource pi4_source = {
    source.pixels,
    source.width,
    source.height,
    source.pitch,
    ToPi4SourceFormat(source.format),
    source.palette_rgb565,
    source.source_rect.x,
    source.source_rect.y,
    source.source_rect.width,
    source.source_rect.height,
    source.left_edge_padding
  };
  pi4v3d::FrameTarget pi4_target = {
    target.pixels,
    target.width,
    target.height,
    target.pitch,
    target.native_rendered_plane == nullptr
  };
  return target.depth == 16U &&
         target.format == kPixelFormatRgb565 &&
         pi4v3d::RenderFrame(pi4_source, pi4_target, params);
#else
  (void)source;
  (void)target;
  (void)params;
  return false;
#endif
}

bool ReadCompletedFrame(OutputReadback *readback) {
  if (readback == nullptr) {
    return false;
  }
  memset(readback, 0, sizeof *readback);
#if RASPPI == 5
  pi5v3d::RenderReadback board_readback = {};
  if (!pi5v3d::ReadCompletedRenderTarget(&board_readback)) {
    return false;
  }
  readback->pixels = board_readback.pixels;
  readback->width = board_readback.width;
  readback->height = board_readback.height;
  readback->pitch = board_readback.pitch;
  readback->depth = board_readback.depth;
  return true;
#elif RASPPI == 4
  pi4v3d::RenderedFrame frame = {};
  if (!pi4v3d::GetLastRenderedFrame(&frame) || frame.pixels == nullptr) {
    return false;
  }
  readback->pixels = frame.pixels;
  readback->framebuffer_bus_address = frame.framebuffer_bus_address;
  readback->width = frame.width;
  readback->height = frame.height;
  readback->pitch = frame.pitch;
  readback->depth = 16U;
  return true;
#else
  return false;
#endif
}

bool GetBootTestOutputLayout(BootTestMode mode,
                             BootTestOutputLayout *layout) {
  if (layout == nullptr) {
    return false;
  }
  memset(layout, 0, sizeof *layout);
#if RASPPI == 4
  if (mode == kBootTestFragmentScanout) {
    layout->width = pi4v3d::kBootScanoutWidth;
    layout->height = pi4v3d::kBootScanoutHeight;
    layout->pitch = pi4v3d::kBootScanoutPitch;
    layout->depth = 16U;
    layout->format = kPixelFormatRgb565;
    return true;
  }
#else
  (void)mode;
#endif
  return false;
}

bool RunBootTest(const OutputFramebuffer &target) {
#if RASPPI == 5
  pi5v3d::OutputTarget pi5_target = {
    static_cast<pi5kms::Framebuffer *>(target.native_framebuffer),
    target.display_width,
    target.display_height,
    ToPi5Rect(target.destination_rect),
    target.wait_for_vblank,
    target.presented,
    target.allow_direct_scanout,
    static_cast<pi5kms::Plane *>(target.native_rendered_plane)
  };
  return pi5v3d::RunBootTest(pi5_target);
#elif RASPPI == 4
  pi4v3d::ScanoutTarget pi4_target = {};
  if (target.format == kPixelFormatRgb565) {
    pi4_target.pixels = target.pixels;
    pi4_target.width = target.width;
    pi4_target.height = target.height;
    pi4_target.pitch = target.pitch;
  }
  return pi4v3d::RunBootTest(pi4_target);
#else
  (void)target;
  return false;
#endif
}

uint32_t LastFrameSequence() {
#if RASPPI == 4
  return pi4v3d::LastFrameSequence();
#else
  return 0U;
#endif
}

bool LastFrameChangedEffect() {
#if RASPPI == 4
  return pi4v3d::LastFrameChangedEffect();
#else
  return false;
#endif
}

void Shutdown() {
#if RASPPI == 5
  pi5v3d::Shutdown();
#elif RASPPI == 4
  pi4v3d::Shutdown();
#else
  g_requested = false;
  g_shader_preset = kShaderOff;
  g_boot_test_mode = kBootTestOff;
#endif
}

}  // namespace v3dcrt
