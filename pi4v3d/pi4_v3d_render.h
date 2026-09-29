#ifndef PI4V3D_PI4_V3D_RENDER_H
#define PI4V3D_PI4_V3D_RENDER_H

#include <stdint.h>

namespace pi4v3d {

static const uint32_t kScanlineProbeWidth = 16U;
static const uint32_t kScanlineProbeHeight = 16U;
static const uint32_t kScanlineProbeSourceBytes = 4U * 4U * 4U;
static const uint32_t kScanlineProbeTargetBytes =
    kScanlineProbeWidth * kScanlineProbeHeight * sizeof(uint16_t);
static const uint16_t kScanlineProbeTargetSentinel = 0xa55aU;

// The largest Pi4 output package starts its 256-byte-aligned RCL at 0x1a00.
// A 1080p target adds 510 supertile coordinates, so the widest supported RCL
// no longer fits in two 4 KiB pages.  Keep one additional page available for
// that bounded control stream; it is allocated only with active V3D frame
// resources and is reused by every frame.
static const uint32_t kRenderControlBytes = 12288U;
static const uint32_t kRenderTileAllocationBytes = 0x00083000U;
static const uint32_t kRenderTileStateOffset = kRenderTileAllocationBytes;
static const uint32_t kRenderTileStateBytes = 4096U;
static const uint32_t kRenderTileScratchBytes =
    kRenderTileAllocationBytes + kRenderTileStateBytes;

struct RenderAddresses {
  uint32_t control;
  uint32_t source_texture;
  uint32_t target;
  uint32_t tile_allocation;
  uint32_t tile_state;
};

struct RenderStandaloneEffects {
  bool convergence_enabled;
  float red_offset_x;
  float red_offset_y;
  float blue_offset_x;
  float blue_offset_y;
  float convergence_radial_strength;
  bool horizontal_filtering_enabled;
  float horizontal_sigma_x;
  bool bloom_enabled;
  float bloom_factor;
  bool horizontal_jitter_enabled;
  float horizontal_jitter_strength;
  float horizontal_jitter_frequency;
  float horizontal_jitter_speed;
  bool composite_artifacts_enabled;
  float composite_chroma_blur;
  float composite_luma_sharpen;
  float composite_color_bleed;
  bool noise_enabled;
  float luminance_noise;
  float chroma_noise;
  float noise_speed;
  float temporal_frame;
  bool scanline_multisample;
};

enum RenderPassKind {
  kRenderPassAutomatic = 0,
  kRenderPassSource,
  kRenderPassOutput
};

enum RenderTargetFormat {
  kRenderTargetRgb565Raster = 0,
  kRenderTargetRgba8Tiled
};

enum RenderPackageClass {
  kRenderPackageAutomatic = 0,
  kRenderPackagePi4PostEffects,
  kRenderPackagePi4Bloom
};

struct RenderGeometry {
  uint32_t source_width;
  uint32_t source_height;
  // The small M2 probe deliberately preserves its proven 4x4 descriptor.
  // Full-frame sources use the V3D hardware-selected UB/UIF layout.
  bool source_uses_hardware_tiling;
  bool source_linear_filter;
  uint32_t target_width;
  uint32_t target_height;
  uint32_t target_stride;
  float scanline_weight;
  float scanline_gap_brightness;
  bool edge_blur_enabled;
  float edge_blur_strength;
  float edge_blur_radius;
  bool phosphor_mask_enabled;
  uint32_t phosphor_mask_pattern;
  float phosphor_mask_brightness;
  bool vignette_enabled;
  float vignette_strength;
  float vignette_scale;
  float vignette_softness;
  bool uneven_illumination_enabled;
  float uneven_illumination_strength;
  float uneven_illumination_scale;
  bool glass_reflection_enabled;
  float glass_reflection_angle;
  float glass_reflection_width;
  float glass_reflection_position;
  bool rounded_screen_mask_enabled;
  float rounded_corner_radius;
  float rounded_border_softness;
  bool edge_glow_enabled;
  float edge_glow_strength;
  float edge_glow_width;
  bool output_response_enabled;
  bool output_response_fast;
  uint32_t output_level_mapping;
  float input_gamma;
  float output_gamma;
  float output_saturation;
  float black_level;
  float white_clip;
  bool geometry_enabled;
  float curvature_x;
  float curvature_y;
  float skew_x;
  float skew_y;
  float trapezoid;
  float rotation_degrees;
  float overscan_scale;
  RenderStandaloneEffects standalone;
};

struct RenderPassConfig {
  RenderPassKind kind;
  RenderTargetFormat target_format;
  uint32_t target_memory_format;
  uint32_t target_height_in_ub_or_stride;
  RenderPackageClass package_class;
};

struct RenderGeometryParams {
  bool enabled;
  float curvature_x;
  float curvature_y;
  float skew_x;
  float skew_y;
  float trapezoid;
  float rotation_degrees;
  float overscan_scale;
};

struct RenderScanlineParams {
  float weight;
  float gap_brightness;
};

struct RenderEdgeBlurParams {
  bool enabled;
  float strength;
  float radius;
};

struct RenderPhosphorMaskParams {
  bool enabled;
  uint32_t pattern;
  float brightness;
};

struct RenderVignetteParams {
  bool enabled;
  float strength;
  float scale;
  float softness;
};

struct RenderUnevenIlluminationParams {
  bool enabled;
  float strength;
  float scale;
};

struct RenderGlassReflectionParams {
  bool enabled;
  float angle;
  float width;
  float position;
};

struct RenderRoundedScreenMaskParams {
  bool enabled;
  float corner_radius;
  float border_softness;
};

struct RenderEdgeGlowParams {
  bool enabled;
  float strength;
  float width;
};

struct RenderOutputResponseParams {
  bool enabled;
  bool fast;
  uint32_t level_mapping;
  float input_gamma;
  float output_gamma;
  float saturation;
  float black_level;
  float white_clip;
};

struct RenderConvergenceParams {
  bool enabled;
  float red_offset_x;
  float red_offset_y;
  float blue_offset_x;
  float blue_offset_y;
  float radial_strength;
};

struct RenderHorizontalFilteringParams {
  bool enabled;
  float sigma_x;
};

struct RenderBloomParams {
  bool enabled;
  float factor;
};

struct RenderHorizontalJitterParams {
  bool enabled;
  float strength;
  float frequency;
  float speed;
};

struct RenderCompositeArtifactsParams {
  bool enabled;
  float chroma_blur;
  float luma_sharpen;
  float color_bleed;
};

struct RenderNoiseParams {
  bool enabled;
  float luminance;
  float chroma;
  float speed;
};

struct RenderEdgeGlowFrameColors {
  float color[4][3];
};

struct ScaledSourceCoordinate {
  uint32_t direct;
  uint32_t alternate;
  bool exact_boundary;
};

struct RenderCommandLists {
  uint32_t bcl_start;
  uint32_t bcl_end;
  uint32_t rcl_start;
  uint32_t rcl_end;
  uint32_t generic_start;
  uint32_t generic_end;
  uint32_t shader_record;
  uint32_t texture_state;
  uint32_t sampler_state;
};

enum RenderJobState {
  kRenderJobEmpty = 0,
  kRenderJobPrepared,
  kRenderJobSubmitted,
  kRenderJobCompleted,
  kRenderJobFailed
};

enum RenderPackageIdentity {
  kRenderPackageIdentityOther = 0,
  kRenderPackageIdentityOutputEdgeGlowPi4,
  kRenderPackageIdentityOutputEdgeBlurPostPi4,
  kRenderPackageIdentityOutputLateEffectsPi4,
  kRenderPackageIdentityOutputEdgeGlow,
  kRenderPackageIdentityOutputResponseFastCubic,
  kRenderPackageIdentityOutputResponse,
  kRenderPackageIdentitySourceNoise,
  kRenderPackageIdentitySourceNoisePi4
};

static const uint32_t kRenderJobMaxDynamicUniforms = 13U;

struct RenderDynamicUniform {
  uint32_t offset;
  uint32_t value;
};

struct RenderJob {
  RenderCommandLists lists;
  uint32_t slot;
  uint32_t control_address;
  uint32_t target_address;
  uint32_t control_hash;
  uint32_t submission_count;
  RenderPackageIdentity package_identity;
  RenderDynamicUniform dynamic_uniforms[kRenderJobMaxDynamicUniforms];
  uint32_t dynamic_uniform_count;
  uint32_t dynamic_uniform_min_offset;
  uint32_t dynamic_uniform_end_offset;
  RenderJobState state;
};

struct RenderReadback {
  uint32_t hash;
  uint32_t changed_pixels;
  uint32_t nonzero_pixels;
  uint32_t unique_colors;
  uint32_t guard_mismatches;
  uint16_t first;
  uint16_t middle;
  uint16_t last;
};

// Builds the V3D 4.2 control/indirect state for a single full-screen triangle
// rendered through the generated source-resolution scanline reference. All
// addresses are V3D MMU virtual addresses.
bool BuildScanlineProbeRender(const RenderAddresses &addresses,
                              uint8_t *control, uint32_t control_size,
                              RenderCommandLists *lists);

// Builds an arbitrary single-tile or multi-tile RGBA8-to-RGB565 frame through
// the V3D 4.2 package adapter. Exact pass-through and scaled output use the
// production cumulative output packages. Geometry selects its generated
// output package when it is the highest active effect. Edge Blur selects the
// original five-sample crt_output_edge_blur package. Phosphor Mask, Vignette, Uneven
// Illumination, Glass Reflection, Rounded Screen Mask, and Edge Glow select
// cumulative successors while preserving preceding effect values. Any two or
// more of those four late effects select the shared compact Pi4 package when
// no earlier output effect is active; standalone Vignette and Rounded Screen
// Mask use that package too. Their scaled, geometry-off disabled state uses
// the compact crt_output_scanlines_pi4 package.
// Visible 1:1 source-resolution modulation keeps
// scanline_probe as a compatibility fallback. Full-frame sources use the V3D
// 4.2 UB/UIF texture layout when
// source_uses_hardware_tiling is true. The RGB565 target is raster-order with
// target_stride bytes per row.
bool BuildFullscreenRender(const RenderAddresses &addresses,
                           const RenderGeometry &geometry,
                           uint8_t *control, uint32_t control_size,
                           RenderCommandLists *lists);
bool BuildFullscreenRenderPass(const RenderAddresses &addresses,
                               const RenderGeometry &geometry,
                               const RenderPassConfig &pass,
                               uint8_t *control, uint32_t control_size,
                               RenderCommandLists *lists);

// Prepares immutable control state for one reusable render slot. Completed
// jobs may be submitted again; failed jobs require a fresh prepare.
bool PrepareScanlineProbeRenderJob(const RenderAddresses &addresses,
                                   uint8_t *control,
                                   uint32_t control_size,
                                   uint32_t slot,
                                   RenderJob *job);
bool PrepareFullscreenRenderJob(const RenderAddresses &addresses,
                                const RenderGeometry &geometry,
                                uint8_t *control,
                                uint32_t control_size,
                                uint32_t slot,
                                RenderJob *job);
bool PrepareFullscreenRenderPassJob(const RenderAddresses &addresses,
                                    const RenderGeometry &geometry,
                                    const RenderPassConfig &pass,
                                    uint8_t *control,
                                    uint32_t control_size,
                                    uint32_t slot,
                                    RenderJob *job);

// Normalizes shared Geometry controls to the same contract used by Pi5.
// Disabled Geometry has one canonical neutral state so inactive slider
// changes do not rebuild immutable frame jobs.
RenderGeometryParams ResolveRenderGeometryParams(
    bool enabled, float curvature_x, float curvature_y, float skew_x,
    float skew_y, float trapezoid, float rotation_degrees,
    float overscan_scale);

// Converts the shared BMX menu scale into the normalized values consumed by
// the generated scanline shader. Disabled scanlines always resolve to the
// byte-exact pass-through state used by the full-frame safety test.
RenderScanlineParams ResolveRenderScanlineParams(bool enabled,
                                                 float menu_weight,
                                                 float gap_brightness);

// Converts the shared Edge Blur menu scale into the generated shader contract.
// Disabled Edge Blur uses a canonical state so inactive slider changes do not
// rebuild immutable frame controls.
RenderEdgeBlurParams ResolveRenderEdgeBlurParams(bool enabled,
                                                 float strength,
                                                 float radius);

// Normalizes the two shared mask patterns and their brightness. Disabled
// masks use one canonical state so inactive menu values do not rebuild jobs.
RenderPhosphorMaskParams ResolveRenderPhosphorMaskParams(
    bool enabled, uint32_t pattern, float brightness);

// Normalizes the shared Vignette controls. Disabled Vignette uses a canonical
// no-op state so inactive menu values do not rebuild immutable jobs.
RenderVignetteParams ResolveRenderVignetteParams(
    bool enabled, float strength, float scale, float softness);

// Normalizes the shared Uneven Illumination controls. The disabled state is
// canonical so inactive menu values do not rebuild immutable frame jobs.
RenderUnevenIlluminationParams ResolveRenderUnevenIlluminationParams(
    bool enabled, float strength, float scale);

// Normalizes the shared Glass Reflection controls in menu units. The adapter
// converts the clamped degree angle to the generated shader's radians.
RenderGlassReflectionParams ResolveRenderGlassReflectionParams(
    bool enabled, float angle, float width, float position);

// Normalizes the shared Rounded Screen Mask controls to the generated
// signed-distance mask contract.
RenderRoundedScreenMaskParams ResolveRenderRoundedScreenMaskParams(
    bool enabled, float corner_radius, float border_softness);

// Normalizes the shared Edge Glow controls. Frame-local edge colors are
// patched separately immediately before the corresponding render slot runs.
RenderEdgeGlowParams ResolveRenderEdgeGlowParams(
    bool enabled, float strength, float width);

// Normalizes the shared Output Response controls. Disabled response uses one
// canonical state so inactive menu values do not rebuild immutable jobs.
RenderOutputResponseParams ResolveRenderOutputResponseParams(
    bool enabled, bool fast, uint32_t level_mapping, float input_gamma,
    float output_gamma, float saturation, float black_level,
    float white_clip);

RenderConvergenceParams ResolveRenderConvergenceParams(
    bool enabled, float red_offset_x, float red_offset_y,
    float blue_offset_x, float blue_offset_y, float radial_strength);

RenderHorizontalFilteringParams ResolveRenderHorizontalFilteringParams(
    bool enabled, float sigma_x);

// Keep the source sampler tied exclusively to the user-facing scaling
// interpolation control, matching the native Pi5 KMS renderer.
bool ResolveRenderSourceLinearFilter(bool interpolation_enabled);

RenderBloomParams ResolveRenderBloomParams(bool enabled, float factor);

RenderHorizontalJitterParams ResolveRenderHorizontalJitterParams(
    bool enabled, float strength, float frequency, float speed);

RenderCompositeArtifactsParams ResolveRenderCompositeArtifactsParams(
    bool enabled, float chroma_blur, float luma_sharpen,
    float color_bleed);

RenderNoiseParams ResolveRenderNoiseParams(
    bool enabled, float luminance, float chroma, float speed);

// Applies Output Response to one RGB565 emulator pixel. The Pi4 source-upload
// LUT path uses the resolved color before scaling and all CRT effects.
uint16_t ResolveRenderOutputResponseRgb565(
    uint16_t pixel, const RenderOutputResponseParams &params);

// Builds the complete source-response RGB565 table. Accurate gamma uses
// precomputed quantization thresholds so the per-color loop does not call
// powf for every channel.
bool BuildRenderOutputResponseRgb565Lut(
    const RenderOutputResponseParams &params, uint16_t *lut,
    uint32_t lut_entries);

// Updates the twelve frame-local edge colors through offsets recorded while
// the cumulative Edge Glow or Output Response job was prepared.
bool PatchRenderJobEdgeGlowFrameColors(
    const RenderEdgeGlowFrameColors &colors,
    uint8_t *control, uint32_t control_size, RenderJob *job);

// Updates the animated source-effect frame uniform through the offset recorded
// while the cumulative Noise package was prepared.
bool PatchRenderJobTemporalFrame(float temporal_frame,
                                 uint8_t *control,
                                 uint32_t control_size,
                                 RenderJob *job);

// Resolves the nearest source texel selected at an output pixel centre. At an
// exact source-texel boundary, raster interpolation may select either adjacent
// texel; alternate records the other mathematically equivalent result.
bool ResolveScaledSourceCoordinate(uint32_t target_coordinate,
                                   uint32_t source_size,
                                   uint32_t target_size,
                                   ScaledSourceCoordinate *coordinate);

// Maps a source-texture coordinate after extending its left edge by a fixed
// number of pixels. The output width is unchanged, so the same number of
// pixels is dropped at the right edge.
bool ResolveLeftEdgePaddedSourceCoordinate(uint32_t output_coordinate,
                                           uint32_t source_width,
                                           uint32_t left_edge_padding,
                                           uint32_t *source_coordinate);

uint32_t RenderTileCount(uint32_t width, uint32_t height);
uint32_t RenderTileStateBytes(uint32_t width, uint32_t height);
bool StartRenderJob(RenderJob *job);
bool FinishRenderJob(RenderJob *job, bool succeeded);
bool RenderJobControlIntact(const RenderJob &job, const uint8_t *control,
                            uint32_t control_size);
bool RenderJobDynamicUniformRange(const RenderJob &job, uint32_t *offset,
                                  uint32_t *size);
void ResetRenderJob(RenderJob *job);

void FillScanlineProbeSource(uint8_t *source, uint32_t source_size);
void FillScanlineProbeTarget(uint8_t *target, uint32_t target_size);
bool AnalyzeScanlineProbeTarget(const uint8_t *target, uint32_t target_size,
                                RenderReadback *readback);

// Copies the tightly packed RGB565 diagnostic target into a scanout staging
// surface. Destination padding and rows outside the 16x16 probe are untouched.
bool CopyScanlineProbeToScanout(const uint8_t *target,
                                uint32_t target_size,
                                uint8_t *scanout,
                                uint32_t scanout_width,
                                uint32_t scanout_height,
                                uint32_t scanout_pitch,
                                uint32_t *scanout_hash);

}  // namespace pi4v3d

#endif  // PI4V3D_PI4_V3D_RENDER_H
