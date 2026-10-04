#include "pi4v3d/pi4_v3d_render.h"

#include <math.h>
#include <stddef.h>
#include <string.h>

#include "pi4v3d/pi4_v3d_mmu.h"
#include "pi4v3d/pi4_v3d42_shader_package_adapter.h"
#include "pi4v3d/pi4_v3d42_texture_state.h"
#include "v3dcrt/output_response.h"
#include "v3dcrt/shaders/generated/v3d42/crt_output_edge_blur.h"
#include "v3dcrt/shaders/generated/v3d42/crt_output_edge_blur_post_pi4.h"
#include "v3dcrt/shaders/generated/v3d42/crt_output_early_post_pi4.h"
#include "v3dcrt/shaders/generated/v3d42/crt_output_edge_glow.h"
#include "v3dcrt/shaders/generated/v3d42/crt_output_edge_glow_pi4.h"
#include "v3dcrt/shaders/generated/v3d42/crt_output_bloom_pi4.h"
#include "v3dcrt/shaders/generated/v3d42/crt_output_glass_reflection.h"
#include "v3dcrt/shaders/generated/v3d42/crt_output_glass_reflection_pi4.h"
#include "v3dcrt/shaders/generated/v3d42/crt_output_geometry.h"
#include "v3dcrt/shaders/generated/v3d42/crt_output_late_effects_pi4.h"
#include "v3dcrt/shaders/generated/v3d42/crt_output_phosphor_mask.h"
#include "v3dcrt/shaders/generated/v3d42/crt_output_response.h"
#include "v3dcrt/shaders/generated/v3d42/crt_output_response_fast_cubic.h"
#include "v3dcrt/shaders/generated/v3d42/crt_output_rounded_screen_mask.h"
#include "v3dcrt/shaders/generated/v3d42/crt_output_scanlines_pi4.h"
#include "v3dcrt/shaders/generated/v3d42/crt_output_scanlines.h"
#include "v3dcrt/shaders/generated/v3d42/crt_output_uneven_illumination.h"
#include "v3dcrt/shaders/generated/v3d42/crt_output_uneven_illumination_pi4.h"
#include "v3dcrt/shaders/generated/v3d42/crt_output_vignette.h"
#include "v3dcrt/shaders/generated/v3d42/crt_source_composite.h"
#include "v3dcrt/shaders/generated/v3d42/crt_source_convergence.h"
#include "v3dcrt/shaders/generated/v3d42/crt_source_filter.h"
#include "v3dcrt/shaders/generated/v3d42/crt_source_noise.h"
#include "v3dcrt/shaders/generated/v3d42/crt_source_noise_pi4.h"
#include "v3dcrt/shaders/generated/v3d42/scanline_probe.h"

namespace pi4v3d {

namespace {

const uint32_t kBclOffset = 0x000U;
const uint32_t kBclLimitOffset = 0x100U;
const uint32_t kGenericOffset = 0x200U;
const uint32_t kShaderRecordOffset = 0x240U;
const uint32_t kAttribute0Offset = kShaderRecordOffset + 36U;
const uint32_t kAttribute1Offset = kAttribute0Offset + 16U;
const uint32_t kTextureStateOffset = 0x2c0U;
const uint32_t kSamplerStateOffset = 0x300U;
const uint32_t kSamplerStateBytes = 32U;
const uint32_t kProgramOffset = 0x340U;
const uint32_t kVertexDataBytes = 12U * sizeof(float);
const uint32_t kDefaultAttributeBytes = sizeof(uint32_t);
const uint32_t kVertexDataAlignment = 64U;
const uint32_t kRclAlignment = 256U;

static_assert(kSamplerStateOffset + kSamplerStateBytes <= kProgramOffset,
              "fixed render state overlaps the package program area");

struct RenderLayout {
  V3d42ShaderLayout shader;
  uint32_t vertex_data_offset;
  uint32_t default_attribute_offset;
  uint32_t rcl_offset;
};

const uint32_t kExpectedBclBytes = 124U;
const uint32_t kExpectedRclBytes = 106U;
const uint32_t kExpectedGenericBytes = 28U;
const uint32_t kTileSizePixels = 64U;
const uint32_t kTileStateBytesPerTile = 256U;

const uint8_t kFlush = 4U;
const uint8_t kStartTileBinning = 6U;
const uint8_t kEndOfRendering = 13U;
const uint8_t kReturnFromSubList = 18U;
const uint8_t kFlushVcdCache = 19U;
const uint8_t kStartGenericTileList = 20U;
const uint8_t kBranchToImplicitTileList = 21U;
const uint8_t kSupertileCoordinates = 23U;
const uint8_t kClearTileBuffers = 25U;
const uint8_t kEndOfLoads = 26U;
const uint8_t kEndOfTileMarker = 27U;
const uint8_t kStoreTileBufferGeneral = 29U;
const uint8_t kVertexArrayPrims = 36U;
const uint8_t kSetInstanceId = 54U;
const uint8_t kPrimListFormat = 56U;
const uint8_t kGlShaderState = 64U;
const uint8_t kVcmCacheSize = 71U;
const uint8_t kTransformFeedbackSpecs = 74U;
const uint8_t kBlendConstantColor = 86U;
const uint8_t kColorWriteMasks = 87U;
const uint8_t kZeroAllCentroidFlags = 88U;
const uint8_t kSampleState = 91U;
const uint8_t kOcclusionQueryCounter = 92U;
const uint8_t kCfgBits = 96U;
const uint8_t kZeroAllFlatShadeFlags = 97U;
const uint8_t kZeroAllNonPerspectiveFlags = 99U;
const uint8_t kPointSize = 104U;
const uint8_t kLineWidth = 105U;
const uint8_t kClipWindow = 107U;
const uint8_t kViewportOffset = 108U;
const uint8_t kClipperZMinMax = 109U;
const uint8_t kClipperXyScaling = 110U;
const uint8_t kClipperZScaleOffset = 111U;
const uint8_t kNumberOfLayers = 119U;
const uint8_t kTileBinningModeCfg = 120U;
const uint8_t kTileRenderingModeCfg = 121U;
const uint8_t kMulticoreSupertileCfg = 122U;
const uint8_t kMulticoreTileListSetBase = 123U;
const uint8_t kTileCoordinates = 124U;
const uint8_t kTileCoordinatesImplicit = 125U;
const uint8_t kTileListInitialBlockSize = 126U;

struct Writer {
  uint8_t *data;
  uint32_t capacity;
  uint32_t offset;
  bool ok;
};

void Put8(Writer *writer, uint32_t value) {
  if (writer == NULL || !writer->ok || writer->offset >= writer->capacity) {
    if (writer != NULL) {
      writer->ok = false;
    }
    return;
  }
  writer->data[writer->offset++] = static_cast<uint8_t>(value);
}

void Put16(Writer *writer, uint32_t value) {
  Put8(writer, value);
  Put8(writer, value >> 8);
}

void Put32(Writer *writer, uint32_t value) {
  Put16(writer, value);
  Put16(writer, value >> 16);
}

uint32_t FloatBits(float value) {
  uint32_t bits = 0U;
  memcpy(&bits, &value, sizeof bits);
  return bits;
}

void PutFloat(Writer *writer, float value) {
  Put32(writer, FloatBits(value));
}

void Store32(uint8_t *data, uint32_t value) {
  data[0] = static_cast<uint8_t>(value);
  data[1] = static_cast<uint8_t>(value >> 8);
  data[2] = static_cast<uint8_t>(value >> 16);
  data[3] = static_cast<uint8_t>(value >> 24);
}

uint32_t Load32(const uint8_t *data) {
  return static_cast<uint32_t>(data[0]) |
         static_cast<uint32_t>(data[1]) << 8U |
         static_cast<uint32_t>(data[2]) << 16U |
         static_cast<uint32_t>(data[3]) << 24U;
}

float ClampFloat(float value, float minimum, float maximum,
                 float fallback) {
  if (value != value) {
    return fallback;
  }
  if (value < minimum) {
    return minimum;
  }
  return value > maximum ? maximum : value;
}

void StoreBits(uint8_t *data, uint32_t start, uint32_t count,
               uint32_t value) {
  for (uint32_t bit = 0U; bit < count; ++bit) {
    const uint32_t mask = 1U << ((start + bit) & 7U);
    uint8_t &byte = data[(start + bit) >> 3U];
    if ((value & (1U << bit)) != 0U) {
      byte = static_cast<uint8_t>(byte | mask);
    } else {
      byte = static_cast<uint8_t>(byte & ~mask);
    }
  }
}

bool AddressRangeValid(uint32_t base, uint32_t size) {
  return base != 0U && size != 0U && base <= UINT32_MAX - size;
}

bool AlignOffset(uint32_t value, uint32_t alignment, uint32_t limit,
                 uint32_t *result) {
  if (result == NULL || alignment == 0U ||
      (alignment & (alignment - 1U)) != 0U ||
      value > UINT32_MAX - (alignment - 1U)) {
    return false;
  }
  const uint32_t aligned =
      (value + alignment - 1U) & ~(alignment - 1U);
  if (aligned > limit) {
    return false;
  }
  *result = aligned;
  return true;
}

bool BuildRenderLayout(const v3dcrt::shaders::ShaderPackage &package,
                       RenderLayout *layout) {
  if (layout == NULL) {
    return false;
  }
  RenderLayout planned = {};
  if (!PlanV3d42ShaderPackage(package, kProgramOffset,
                              kRenderControlBytes, &planned.shader, NULL) ||
      !AlignOffset(planned.shader.end_offset, kVertexDataAlignment,
                   kRenderControlBytes, &planned.vertex_data_offset) ||
      planned.vertex_data_offset >
          kRenderControlBytes - kVertexDataBytes ||
      !AlignOffset(planned.vertex_data_offset + kVertexDataBytes,
                   kVertexDataAlignment, kRenderControlBytes,
                   &planned.default_attribute_offset) ||
      planned.default_attribute_offset >
          kRenderControlBytes - kDefaultAttributeBytes ||
      !AlignOffset(planned.default_attribute_offset +
                       kDefaultAttributeBytes,
                   kRclAlignment, kRenderControlBytes,
                   &planned.rcl_offset) ||
      planned.rcl_offset >= kRenderControlBytes) {
    return false;
  }
  *layout = planned;
  return true;
}

const v3dcrt::shaders::ShaderPackage &ShaderPackageForGeometry(
    const RenderGeometry &geometry, const RenderPassConfig &pass) {
  // Keep compact one-TMU packages for the neutral Pi4 output path and the
  // bounded effects that can also carry its source-row scanline modulation.
  // Edge Output effects retain their proven package priority. Source-domain
  // effects are selected explicitly for the first multipass stage.
  const bool source_resolution =
      geometry.source_width == geometry.target_width &&
      geometry.source_height == geometry.target_height;
  const bool diagnostic_probe =
      !geometry.source_uses_hardware_tiling;
  if (pass.kind == kRenderPassSource) {
    if (geometry.standalone.noise_enabled ||
        geometry.standalone.horizontal_jitter_enabled) {
      return v3dcrt::shaders::generated::kV3dCrtSourceNoisePackage;
    }
    if (geometry.standalone.composite_artifacts_enabled) {
      return v3dcrt::shaders::generated::kV3dCrtSourceCompositePackage;
    }
    if (geometry.standalone.convergence_enabled) {
      return v3dcrt::shaders::generated::kV3dCrtSourceConvergencePackage;
    }
    return v3dcrt::shaders::generated::kV3dCrtSourceFilterPackage;
  }
  if (pass.package_class == kRenderPackagePi4PostEffects) {
    return v3dcrt::shaders::generated::
        kV3dCrtOutputLateEffectsPi4Package;
  }
  if (pass.package_class == kRenderPackagePi4Bloom) {
    return v3dcrt::shaders::generated::kV3dCrtOutputBloomPi4Package;
  }
  if (!geometry.geometry_enabled && geometry.scanline_weight > 0.0f &&
      (source_resolution || diagnostic_probe)) {
    return v3dcrt::shaders::generated::kV3dScanlineProbePackage;
  }
  if (geometry.output_response_enabled &&
      !source_resolution && !diagnostic_probe) {
    return geometry.output_response_fast &&
               geometry.output_level_mapping == 1U ?
        v3dcrt::shaders::generated::
            kV3dCrtOutputResponseFastCubicPackage :
        v3dcrt::shaders::generated::kV3dCrtOutputResponsePackage;
  }
  const uint32_t post_effect_count =
      (geometry.vignette_enabled ? 1U : 0U) +
      (geometry.uneven_illumination_enabled ? 1U : 0U) +
      (geometry.glass_reflection_enabled ? 1U : 0U) +
      (geometry.rounded_screen_mask_enabled ? 1U : 0U) +
      (geometry.edge_glow_enabled ? 1U : 0U);
  const bool late_effects_enabled =
      geometry.uneven_illumination_enabled ||
      geometry.glass_reflection_enabled ||
      geometry.rounded_screen_mask_enabled || geometry.edge_glow_enabled;
  const bool post_effects_enabled =
      geometry.vignette_enabled || late_effects_enabled;
  const uint32_t early_effect_count =
      (geometry.geometry_enabled ? 1U : 0U) +
      (geometry.edge_blur_enabled ? 1U : 0U) +
      (geometry.phosphor_mask_enabled ? 1U : 0U);
  const bool compact_early_post_effects =
      early_effect_count == 1U && post_effect_count == 1U &&
      geometry.scanline_weight <= 0.0f &&
      ((geometry.edge_blur_enabled && post_effects_enabled) ||
       (geometry.geometry_enabled && late_effects_enabled) ||
       (geometry.phosphor_mask_enabled &&
        (geometry.uneven_illumination_enabled ||
         geometry.glass_reflection_enabled || geometry.edge_glow_enabled)));
  const bool compact_post_effects =
      !geometry.standalone.bloom_enabled &&
      (compact_early_post_effects ||
       (post_effect_count >= 2U && !geometry.geometry_enabled &&
        !geometry.edge_blur_enabled && !geometry.phosphor_mask_enabled));
  // The shared late-effects fragment is also the cheaper Pi4 path for the
  // two remaining standalone post effects.  Besides using one TMU, it binds
  // the already host-precomputed Rounded Mask parameters and avoids carrying
  // the generic five-sample early-effect code through these common cases.
  const bool compact_standalone_post_effect =
      !geometry.standalone.bloom_enabled && early_effect_count == 0U &&
      post_effect_count == 1U &&
      (geometry.vignette_enabled ||
       geometry.rounded_screen_mask_enabled);
  const bool compact_specialized_post_effect =
      !geometry.standalone.bloom_enabled &&
      geometry.scanline_weight <= 0.0f && early_effect_count == 1U &&
      post_effect_count == 1U &&
      (geometry.geometry_enabled || geometry.edge_blur_enabled ||
       geometry.phosphor_mask_enabled) &&
      (geometry.uneven_illumination_enabled ||
       geometry.glass_reflection_enabled || geometry.edge_glow_enabled);
  if (compact_specialized_post_effect &&
      !source_resolution && !diagnostic_probe) {
    if (geometry.edge_glow_enabled) {
      return v3dcrt::shaders::generated::
          kV3dCrtOutputEdgeGlowPi4Package;
    }
    if (geometry.glass_reflection_enabled) {
      return v3dcrt::shaders::generated::
          kV3dCrtOutputGlassReflectionPi4Package;
    }
    return v3dcrt::shaders::generated::
        kV3dCrtOutputUnevenIlluminationPi4Package;
  }
  if ((compact_post_effects || compact_standalone_post_effect) &&
      !source_resolution && !diagnostic_probe) {
    return geometry.edge_blur_enabled ?
        v3dcrt::shaders::generated::
            kV3dCrtOutputEdgeBlurPostPi4Package :
        early_effect_count != 0U ?
        v3dcrt::shaders::generated::
            kV3dCrtOutputEarlyPostPi4Package :
        v3dcrt::shaders::generated::
            kV3dCrtOutputLateEffectsPi4Package;
  }
  if (geometry.edge_glow_enabled &&
      !source_resolution && !diagnostic_probe) {
    const bool standalone_edge_glow =
        !geometry.geometry_enabled &&
        !geometry.edge_blur_enabled && !geometry.phosphor_mask_enabled &&
        !geometry.vignette_enabled &&
        !geometry.uneven_illumination_enabled &&
        !geometry.glass_reflection_enabled &&
        !geometry.rounded_screen_mask_enabled;
    return standalone_edge_glow ?
        v3dcrt::shaders::generated::kV3dCrtOutputEdgeGlowPi4Package :
        v3dcrt::shaders::generated::kV3dCrtOutputEdgeGlowPackage;
  }
  if (geometry.rounded_screen_mask_enabled &&
      !source_resolution && !diagnostic_probe) {
    return v3dcrt::shaders::generated::
        kV3dCrtOutputRoundedScreenMaskPackage;
  }
  if (geometry.glass_reflection_enabled &&
      !source_resolution && !diagnostic_probe) {
    const bool standalone_glass =
        !geometry.geometry_enabled &&
        !geometry.edge_blur_enabled && !geometry.phosphor_mask_enabled &&
        !geometry.vignette_enabled &&
        !geometry.uneven_illumination_enabled;
    return standalone_glass ?
        v3dcrt::shaders::generated::
            kV3dCrtOutputGlassReflectionPi4Package :
        v3dcrt::shaders::generated::kV3dCrtOutputGlassReflectionPackage;
  }
  if (geometry.uneven_illumination_enabled &&
      !source_resolution && !diagnostic_probe) {
    const bool standalone_uneven =
        !geometry.geometry_enabled &&
        !geometry.edge_blur_enabled && !geometry.phosphor_mask_enabled &&
        !geometry.vignette_enabled;
    return standalone_uneven ?
        v3dcrt::shaders::generated::
            kV3dCrtOutputUnevenIlluminationPi4Package :
        v3dcrt::shaders::generated::
            kV3dCrtOutputUnevenIlluminationPackage;
  }
  if (geometry.vignette_enabled &&
      !source_resolution && !diagnostic_probe) {
    return v3dcrt::shaders::generated::kV3dCrtOutputVignettePackage;
  }
  if (geometry.phosphor_mask_enabled &&
      !source_resolution && !diagnostic_probe) {
    return v3dcrt::shaders::generated::
        kV3dCrtOutputPhosphorMaskPackage;
  }
  if (geometry.edge_blur_enabled &&
      !source_resolution && !diagnostic_probe) {
    return v3dcrt::shaders::generated::kV3dCrtOutputEdgeBlurPackage;
  }
  if (geometry.standalone.bloom_enabled &&
      !source_resolution && !diagnostic_probe) {
    return v3dcrt::shaders::generated::kV3dCrtOutputBloomPi4Package;
  }
  if (geometry.standalone.noise_enabled &&
      !source_resolution && !diagnostic_probe) {
    return v3dcrt::shaders::generated::kV3dCrtSourceNoisePi4Package;
  }
  if (geometry.standalone.horizontal_jitter_enabled &&
      !source_resolution && !diagnostic_probe) {
    return v3dcrt::shaders::generated::kV3dCrtSourceNoisePackage;
  }
  if (geometry.standalone.composite_artifacts_enabled &&
      !source_resolution && !diagnostic_probe) {
    return v3dcrt::shaders::generated::kV3dCrtSourceCompositePackage;
  }
  if (geometry.standalone.convergence_enabled &&
      !source_resolution && !diagnostic_probe) {
    return v3dcrt::shaders::generated::kV3dCrtSourceConvergencePackage;
  }
  if (geometry.standalone.horizontal_filtering_enabled &&
      !source_resolution && !diagnostic_probe) {
    return v3dcrt::shaders::generated::kV3dCrtSourceFilterPackage;
  }
  if (!geometry.geometry_enabled && !geometry.edge_blur_enabled &&
      !source_resolution && !diagnostic_probe) {
    return v3dcrt::shaders::generated::
        kV3dCrtOutputScanlinesPi4Package;
  }
  if (geometry.geometry_enabled && geometry.scanline_weight <= 0.0f &&
      !source_resolution && !diagnostic_probe) {
    return v3dcrt::shaders::generated::kV3dCrtOutputGeometryPackage;
  }
  return v3dcrt::shaders::generated::kV3dCrtOutputScanlinesPackage;
}

RenderPackageIdentity PackageIdentity(
    const v3dcrt::shaders::ShaderPackage &package) {
  using namespace v3dcrt::shaders::generated;
  if (&package == &kV3dCrtOutputEdgeGlowPi4Package) {
    return kRenderPackageIdentityOutputEdgeGlowPi4;
  }
  if (&package == &kV3dCrtOutputEdgeBlurPostPi4Package) {
    return kRenderPackageIdentityOutputEdgeBlurPostPi4;
  }
  if (&package == &kV3dCrtOutputLateEffectsPi4Package) {
    return kRenderPackageIdentityOutputLateEffectsPi4;
  }
  if (&package == &kV3dCrtOutputEdgeGlowPackage) {
    return kRenderPackageIdentityOutputEdgeGlow;
  }
  if (&package == &kV3dCrtOutputResponseFastCubicPackage) {
    return kRenderPackageIdentityOutputResponseFastCubic;
  }
  if (&package == &kV3dCrtOutputResponsePackage) {
    return kRenderPackageIdentityOutputResponse;
  }
  if (&package == &kV3dCrtSourceNoisePackage) {
    return kRenderPackageIdentitySourceNoise;
  }
  if (&package == &kV3dCrtSourceNoisePi4Package) {
    return kRenderPackageIdentitySourceNoisePi4;
  }
  return kRenderPackageIdentityOther;
}

bool HasEdgeGlowFrameColors(RenderPackageIdentity identity) {
  return identity >= kRenderPackageIdentityOutputEdgeGlowPi4 &&
         identity <= kRenderPackageIdentityOutputResponse;
}

bool HasTemporalFrame(RenderPackageIdentity identity) {
  return identity == kRenderPackageIdentitySourceNoise ||
         identity == kRenderPackageIdentitySourceNoisePi4;
}

bool AddDynamicUniform(const v3dcrt::shaders::ShaderPackage &package,
                       const RenderLayout &layout, const char *semantic,
                       const uint8_t *control, uint32_t control_size,
                       RenderJob *job) {
  uint32_t offset = 0U;
  if (job == NULL || job->dynamic_uniform_count >=
                         kRenderJobMaxDynamicUniforms ||
      !FindV3d42FragmentUniformOffset(
          package, layout.shader, semantic, &offset) ||
      offset > control_size - sizeof(uint32_t)) {
    return false;
  }
  RenderDynamicUniform &uniform =
      job->dynamic_uniforms[job->dynamic_uniform_count++];
  uniform.offset = offset;
  uniform.value = Load32(control + offset);
  if (job->dynamic_uniform_count == 1U ||
      offset < job->dynamic_uniform_min_offset) {
    job->dynamic_uniform_min_offset = offset;
  }
  if (offset + sizeof(uint32_t) > job->dynamic_uniform_end_offset) {
    job->dynamic_uniform_end_offset = offset + sizeof(uint32_t);
  }
  return true;
}

bool PrepareDynamicUniforms(
    const v3dcrt::shaders::ShaderPackage &package,
    const RenderLayout &layout, const uint8_t *control,
    uint32_t control_size, RenderJob *job) {
  if (job == NULL) {
    return false;
  }
  job->package_identity = PackageIdentity(package);
  static const char *const edge_semantics[12] = {
    "edge_glow_top_r", "edge_glow_top_g", "edge_glow_top_b",
    "edge_glow_bottom_r", "edge_glow_bottom_g", "edge_glow_bottom_b",
    "edge_glow_left_r", "edge_glow_left_g", "edge_glow_left_b",
    "edge_glow_right_r", "edge_glow_right_g", "edge_glow_right_b",
  };
  if (HasEdgeGlowFrameColors(job->package_identity)) {
    for (uint32_t i = 0U; i < 12U; ++i) {
      if (!AddDynamicUniform(package, layout, edge_semantics[i], control,
                             control_size, job)) {
        return false;
      }
    }
  } else if (HasTemporalFrame(job->package_identity) &&
             !AddDynamicUniform(package, layout, "temporal_frame", control,
                                control_size, job)) {
    return false;
  }
  return true;
}

uint32_t ImmutableControlHash(const RenderJob &job,
                              const uint8_t *control) {
  uint32_t offsets[kRenderJobMaxDynamicUniforms] = {};
  for (uint32_t i = 0U; i < job.dynamic_uniform_count; ++i) {
    offsets[i] = job.dynamic_uniforms[i].offset;
  }
  for (uint32_t i = 1U; i < job.dynamic_uniform_count; ++i) {
    const uint32_t offset = offsets[i];
    uint32_t insertion = i;
    while (insertion != 0U && offsets[insertion - 1U] > offset) {
      offsets[insertion] = offsets[insertion - 1U];
      --insertion;
    }
    offsets[insertion] = offset;
  }

  uint32_t hash = 2166136261U;
  uint32_t dynamic = 0U;
  for (uint32_t i = 0U; i < kRenderControlBytes; ++i) {
    while (dynamic < job.dynamic_uniform_count &&
           i >= offsets[dynamic] + sizeof(uint32_t)) {
      ++dynamic;
    }
    const bool mutable_byte =
        dynamic < job.dynamic_uniform_count && i >= offsets[dynamic];
    hash ^= mutable_byte ? 0U : control[i];
    hash *= 16777619U;
  }
  return hash;
}

void EmitFloatPacket(Writer *writer, uint8_t opcode, float value) {
  Put8(writer, opcode);
  PutFloat(writer, value);
}

void EmitClipperPacket(Writer *writer, uint8_t opcode,
                       float first, float second) {
  Put8(writer, opcode);
  PutFloat(writer, first);
  PutFloat(writer, second);
}

void EmitStoreConfigured(Writer *writer, uint32_t buffer, uint32_t address,
                         uint32_t memory_format,
                         uint32_t output_image_format,
                         uint32_t height_in_ub_or_stride,
                         uint32_t image_height, bool rb_swap) {
  Put8(writer, kStoreTileBufferGeneral);
  Put8(writer, (memory_format << 4U) | (buffer & 0xfU));
  Put8(writer, (output_image_format << 4U) & 0xffU);
  Put8(writer, (rb_swap ? 1U << 4U : 0U) |
                   ((output_image_format >> 4U) & 3U));
  Put8(writer, (height_in_ub_or_stride << 4U) & 0xffU);
  Put8(writer, (height_in_ub_or_stride >> 4U) & 0xffU);
  Put8(writer, (height_in_ub_or_stride >> 12U) & 0xffU);
  Put16(writer, image_height);
  Put32(writer, address);
}

void EmitRasterRgb565Store(Writer *writer, uint32_t buffer,
                           uint32_t address, uint32_t stride,
                           bool rb_swap) {
  EmitStoreConfigured(writer, buffer, address, 0U, 7U, stride, 0U,
                      rb_swap);
}

void EmitDummyStore(Writer *writer) {
  EmitRasterRgb565Store(writer, 8U, 0U, 0U, false);  // NONE.
}

bool BuildBcl(const RenderAddresses &addresses,
              const RenderGeometry &geometry, uint8_t *control,
              RenderCommandLists *lists) {
  Writer cl = {control + kBclOffset,
               kBclLimitOffset - kBclOffset, 0U, true};
  lists->bcl_start = addresses.control + kBclOffset;

  Put8(&cl, kNumberOfLayers);
  Put8(&cl, 0U);  // One layer.

  Put8(&cl, kTileBinningModeCfg);
  Put8(&cl, 0U);  // 64-byte allocation and initial blocks.
  Put8(&cl, 0U);  // One 32-bpp render target, no MSAA/double buffer.
  Put16(&cl, 0U);
  Put16(&cl, geometry.target_width - 1U);
  Put16(&cl, geometry.target_height - 1U);
  Put8(&cl, kFlushVcdCache);
  Put8(&cl, kOcclusionQueryCounter);
  Put32(&cl, 0U);
  Put8(&cl, kStartTileBinning);

  Put8(&cl, kClipWindow);
  Put16(&cl, 0U);
  Put16(&cl, 0U);
  Put16(&cl, geometry.target_width);
  Put16(&cl, geometry.target_height);

  Put8(&cl, kCfgBits);
  Put8(&cl, 7U);  // Both faces enabled; clockwise primitives.
  Put8(&cl, 7U << 4);  // Depth test ALWAYS, no depth update.
  Put8(&cl, 0U);  // No stencil, blending, or early-Z.
  EmitFloatPacket(&cl, kPointSize, 1.0f);
  EmitFloatPacket(&cl, kLineWidth, 1.0f);
  const float viewport_x = geometry.target_width * 128.0f;
  const float viewport_y = geometry.target_height * -128.0f;
  EmitClipperPacket(&cl, kClipperXyScaling,
                    viewport_x, viewport_y);
  EmitClipperPacket(&cl, kClipperZScaleOffset, 0.5f, 0.5f);
  EmitClipperPacket(&cl, kClipperZMinMax, 0.0f, 1.0f);

  Put8(&cl, kViewportOffset);
  Put32(&cl, geometry.target_width * 128U);
  Put32(&cl, geometry.target_height * 128U);

  Put8(&cl, kColorWriteMasks);
  Put32(&cl, 0U);  // Zero bits enable all components.
  Put8(&cl, kBlendConstantColor);
  Put32(&cl, 0U);
  Put32(&cl, 0U);
  Put8(&cl, kZeroAllFlatShadeFlags);
  Put8(&cl, kZeroAllNonPerspectiveFlags);
  Put8(&cl, kZeroAllCentroidFlags);
  Put8(&cl, kTransformFeedbackSpecs);
  Put8(&cl, 0U);
  Put8(&cl, kOcclusionQueryCounter);
  Put32(&cl, 0U);
  Put8(&cl, kSampleState);
  Put8(&cl, 15U);
  Put8(&cl, 0U);
  Put16(&cl, 0x3f80U);  // Coverage 1.0 in f18.7 encoding.
  Put8(&cl, kVcmCacheSize);
  Put8(&cl, 0x44U);  // Four 16-vertex batches for CS and VS.

  Put8(&cl, kGlShaderState);
  Put32(&cl, addresses.control + kShaderRecordOffset + 2U);
  Put8(&cl, kVertexArrayPrims);
  Put8(&cl, 4U);  // TRIANGLES.
  Put32(&cl, 3U);
  Put32(&cl, 0U);
  Put8(&cl, kFlush);

  lists->bcl_end = lists->bcl_start + cl.offset;
  return cl.ok && cl.offset == kExpectedBclBytes;
}

bool BuildGeneric(const RenderAddresses &addresses,
                  const RenderGeometry &geometry,
                  const RenderPassConfig &pass, uint8_t *control,
                  RenderCommandLists *lists) {
  Writer cl = {control + kGenericOffset,
               kShaderRecordOffset - kGenericOffset, 0U, true};
  lists->generic_start = addresses.control + kGenericOffset;
  Put8(&cl, kTileCoordinatesImplicit);
  Put8(&cl, kEndOfLoads);
  Put8(&cl, kPrimListFormat);
  Put8(&cl, 2U);  // List triangles.
  Put8(&cl, kSetInstanceId);
  Put32(&cl, 0U);
  Put8(&cl, kBranchToImplicitTileList);
  Put8(&cl, 0U);
  if (pass.target_format == kRenderTargetRgba8Tiled) {
    EmitStoreConfigured(
        &cl, 0U, addresses.target, pass.target_memory_format, 27U,
        pass.target_height_in_ub_or_stride, 0U, false);
  } else {
    EmitRasterRgb565Store(
        &cl, 0U, addresses.target, geometry.target_stride, true);
  }
  Put8(&cl, kClearTileBuffers);
  Put8(&cl, 3U);  // All render targets and Z/stencil.
  Put8(&cl, kEndOfTileMarker);
  Put8(&cl, kReturnFromSubList);
  lists->generic_end = lists->generic_start + cl.offset;
  return cl.ok && cl.offset == kExpectedGenericBytes;
}

bool BuildRcl(const RenderAddresses &addresses,
              const RenderGeometry &geometry, const RenderLayout &layout,
              uint8_t *control,
              RenderCommandLists *lists) {
  Writer cl = {control + layout.rcl_offset,
               kRenderControlBytes - layout.rcl_offset, 0U, true};
  lists->rcl_start = addresses.control + layout.rcl_offset;

  Put8(&cl, kTileRenderingModeCfg);
  Put8(&cl, 0U);  // Common config, one render target.
  Put16(&cl, geometry.target_width);
  Put16(&cl, geometry.target_height);
  Put8(&cl, 1U << 6);  // Early-Z disabled, 32-bpp target.
  Put16(&cl, 0U);

  Put8(&cl, kTileRenderingModeCfg);
  Put8(&cl, 3U);  // Clear colors part 1, render target 0.
  Put32(&cl, 0U);
  Put8(&cl, 0U);
  Put8(&cl, 0U);
  Put8(&cl, 0U);

  Put8(&cl, kTileRenderingModeCfg);
  Put8(&cl, 0x81U);  // Color config: RT0 internal type 8, 32 bpp.
  Put8(&cl, 0U);     // No clamp; remaining render targets disabled.
  Put16(&cl, 0U);
  Put32(&cl, 0U);

  Put8(&cl, kTileRenderingModeCfg);
  Put8(&cl, 2U);  // Z/stencil clear values.
  Put8(&cl, 0U);
  PutFloat(&cl, 1.0f);
  Put16(&cl, 0U);

  Put8(&cl, kTileListInitialBlockSize);
  Put8(&cl, 1U << 2);  // Auto-chained 64-byte blocks.
  Put8(&cl, kMulticoreTileListSetBase);
  Put32(&cl, addresses.tile_allocation);

  Put8(&cl, kMulticoreSupertileCfg);
  Put8(&cl, 0U);  // One-tile supertile width minus one.
  Put8(&cl, 0U);  // One-tile supertile height minus one.
  const uint32_t tiles_x =
      (geometry.target_width + kTileSizePixels - 1U) / kTileSizePixels;
  const uint32_t tiles_y =
      (geometry.target_height + kTileSizePixels - 1U) / kTileSizePixels;
  Put8(&cl, tiles_x);  // Frame width in one-tile supertiles.
  Put8(&cl, tiles_y);  // Frame height in one-tile supertiles.
  Put8(&cl, tiles_x & 0xffU);
  Put8(&cl, ((tiles_y << 4U) & 0xffU) |
               ((tiles_x >> 8U) & 0x0fU));
  Put8(&cl, (tiles_y >> 4U) & 0xffU);
  Put8(&cl, 0U);  // One bin tile list; single core.

  // GFXH-1742: V3D 4.x requires two dummy stores after changing the
  // tile-buffer internal type. Mesa clears after the first store only.
  for (uint32_t i = 0U; i < 2U; ++i) {
    Put8(&cl, kTileCoordinates);
    Put8(&cl, 0U);
    Put8(&cl, 0U);
    Put8(&cl, 0U);
    Put8(&cl, kEndOfLoads);
    EmitDummyStore(&cl);
    if (i == 0U) {
      Put8(&cl, kClearTileBuffers);
      Put8(&cl, 3U);
    }
    Put8(&cl, kEndOfTileMarker);
  }
  Put8(&cl, kFlushVcdCache);

  Put8(&cl, kStartGenericTileList);
  Put32(&cl, lists->generic_start);
  Put32(&cl, lists->generic_end);
  for (uint32_t y = 0U; y < tiles_y; ++y) {
    for (uint32_t x = 0U; x < tiles_x; ++x) {
      Put8(&cl, kSupertileCoordinates);
      Put8(&cl, x);
      Put8(&cl, y);
    }
  }
  Put8(&cl, kEndOfRendering);

  lists->rcl_end = lists->rcl_start + cl.offset;
  const uint32_t expected_bytes =
      kExpectedRclBytes + 3U * (tiles_x * tiles_y - 1U);
  return cl.ok && cl.offset == expected_bytes;
}

void BuildShaderState(const RenderAddresses &addresses,
                      const RenderLayout &layout, uint8_t *control,
                      RenderCommandLists *lists) {
  uint8_t *record = control + kShaderRecordOffset;
  record[0] = 1U << 1;  // Enable clipping.
  record[1] = 1U << 4;  // Fragment shader uses real pixel-centre W.
  record[2] = 1U << 2;  // No implicit point/line varyings.
  record[3] = static_cast<uint8_t>(layout.shader.fragment_varying_count);
  record[4] = static_cast<uint8_t>(
      layout.shader.coordinate_vpm_output_size);
  record[5] = 1U;       // Shared coordinate input segment size.
  record[6] = static_cast<uint8_t>(layout.shader.vertex_vpm_output_size);
  record[7] = 1U;       // Shared vertex input segment size.
  Store32(record + 8U,
          addresses.control + layout.default_attribute_offset);
  Store32(record + 12U,
          addresses.control + layout.shader.fragment_code_offset +
              layout.shader.fragment_code_flags);
  Store32(record + 16U,
          addresses.control + layout.shader.fragment_uniform_offset);
  Store32(record + 20U,
          addresses.control + layout.shader.vertex_code_offset +
              layout.shader.vertex_code_flags);
  Store32(record + 24U,
          addresses.control + layout.shader.vertex_uniform_offset);
  Store32(record + 28U,
          addresses.control + layout.shader.coordinate_code_offset +
              layout.shader.coordinate_code_flags);
  Store32(record + 32U,
          addresses.control + layout.shader.coordinate_uniform_offset);

  uint8_t *attribute = control + kAttribute0Offset;
  Store32(attribute, addresses.control + layout.vertex_data_offset);
  attribute[4] = 2U | (2U << 2);  // vec2 float.
  attribute[5] = 2U | (2U << 4);  // CS and VS each read two values.
  Store32(attribute + 8U, 16U);
  Store32(attribute + 12U, 0x00ffffffU);

  attribute = control + kAttribute1Offset;
  Store32(attribute, addresses.control + layout.vertex_data_offset + 8U);
  attribute[4] = 2U | (2U << 2);  // vec2 float.
  attribute[5] = 2U << 4;         // Only VS reads texture coordinates.
  Store32(attribute + 8U, 16U);
  Store32(attribute + 12U, 0x00ffffffU);

  lists->shader_record = addresses.control + kShaderRecordOffset;
}

bool BuildTextureState(const RenderAddresses &addresses,
                       const RenderGeometry &geometry, uint8_t *control,
                       RenderCommandLists *lists) {
  uint8_t *texture = control + kTextureStateOffset;
  if (geometry.source_uses_hardware_tiling) {
    v3d42::Rgba8TextureLayout layout = {};
    if (!v3d42::ComputeRgba8TextureLayout(
            geometry.source_width, geometry.source_height, &layout) ||
        !v3d42::PackRgba8TextureShaderState(
            texture, v3d42::kTextureShaderStateBytes,
            addresses.source_texture, layout, true)) {
      return false;
    }
  } else {
    // Preserve the byte-exact descriptor used by the hardware-confirmed M2
    // 4x4 probe. A single utile does not require the full layout metadata.
    Store32(texture, addresses.source_texture);
    StoreBits(texture, 58U, 14U, geometry.source_width);
    StoreBits(texture, 72U, 14U, geometry.source_height);
    StoreBits(texture, 86U, 14U, 1U);
    StoreBits(texture, 100U, 7U, 4U);  // RGBA8 texture type.
    StoreBits(texture, 108U, 3U, 2U);  // R.
    StoreBits(texture, 111U, 3U, 3U);  // G.
    StoreBits(texture, 114U, 3U, 4U);  // B.
    StoreBits(texture, 117U, 3U, 5U);  // A.
  }

  uint8_t *sampler = control + kSamplerStateOffset;
  sampler[0] = geometry.source_linear_filter ? 4U : 7U;
  // Bits 0/1 select nearest magnification/minification when set. Keep nearest
  // mip selection in bit 2 for both modes; runtime frames have only level 0.
  sampler[6] = 1U | (1U << 3) | (1U << 6);  // Clamp S/T/R.

  lists->texture_state = addresses.control + kTextureStateOffset;
  lists->sampler_state = addresses.control + kSamplerStateOffset;
  return true;
}

bool BuildProgramsAndUniforms(const RenderAddresses &addresses,
                              const RenderGeometry &geometry,
                              const v3dcrt::shaders::ShaderPackage &package,
                              const RenderLayout &layout,
                              uint8_t *control) {
  const V3d42ShaderBindings bindings = {
    geometry.source_width,
    geometry.source_height,
    geometry.target_width,
    geometry.target_height,
    geometry.scanline_weight,
    geometry.scanline_gap_brightness,
    geometry.edge_blur_enabled,
    geometry.edge_blur_strength,
    geometry.edge_blur_radius,
    geometry.phosphor_mask_enabled,
    geometry.phosphor_mask_pattern,
    geometry.phosphor_mask_brightness,
    geometry.vignette_enabled,
    geometry.vignette_strength,
    geometry.vignette_scale,
    geometry.vignette_softness,
    geometry.uneven_illumination_enabled,
    geometry.uneven_illumination_strength,
    geometry.uneven_illumination_scale,
    geometry.glass_reflection_enabled,
    geometry.glass_reflection_angle,
    geometry.glass_reflection_width,
    geometry.glass_reflection_position,
    geometry.rounded_screen_mask_enabled,
    geometry.rounded_corner_radius,
    geometry.rounded_border_softness,
    geometry.edge_glow_enabled,
    geometry.edge_glow_strength,
    geometry.edge_glow_width,
    geometry.output_response_enabled,
    geometry.output_response_fast,
    geometry.output_level_mapping,
    geometry.input_gamma,
    geometry.output_gamma,
    geometry.output_saturation,
    geometry.black_level,
    geometry.white_clip,
    geometry.geometry_enabled,
    geometry.curvature_x,
    geometry.curvature_y,
    geometry.skew_x,
    geometry.skew_y,
    geometry.trapezoid,
    geometry.rotation_degrees,
    geometry.overscan_scale,
    geometry.standalone.scanline_multisample,
    geometry.standalone.convergence_enabled,
    geometry.standalone.red_offset_x,
    geometry.standalone.red_offset_y,
    geometry.standalone.blue_offset_x,
    geometry.standalone.blue_offset_y,
    geometry.standalone.convergence_radial_strength,
    geometry.standalone.horizontal_filtering_enabled,
    geometry.standalone.horizontal_sigma_x,
    geometry.standalone.bloom_enabled,
    geometry.standalone.bloom_factor,
    geometry.standalone.horizontal_jitter_enabled,
    geometry.standalone.horizontal_jitter_strength,
    geometry.standalone.horizontal_jitter_frequency,
    geometry.standalone.horizontal_jitter_speed,
    geometry.standalone.composite_artifacts_enabled,
    geometry.standalone.composite_chroma_blur,
    geometry.standalone.composite_luma_sharpen,
    geometry.standalone.composite_color_bleed,
    geometry.standalone.noise_enabled,
    geometry.standalone.luminance_noise,
    geometry.standalone.chroma_noise,
    geometry.standalone.noise_speed,
    geometry.standalone.temporal_frame,
    addresses.control + kTextureStateOffset,
    addresses.control + kSamplerStateOffset
  };
  if (!MaterializeV3d42ShaderPackage(
          package, bindings, layout.shader, control,
          kRenderControlBytes, NULL)) {
    return false;
  }

  const float vertices[] = {
    -1.0f, -1.0f, 0.0f, 0.0f,
     3.0f, -1.0f, 2.0f, 0.0f,
    -1.0f,  3.0f, 0.0f, 2.0f,
  };
  memcpy(control + layout.vertex_data_offset, vertices, sizeof vertices);
  return true;
}

}  // namespace

bool BuildScanlineProbeRender(const RenderAddresses &addresses,
                              uint8_t *control, uint32_t control_size,
                              RenderCommandLists *lists) {
  const RenderGeometry geometry = {
    4U,
    4U,
    false,
    false,
    kScanlineProbeWidth,
    kScanlineProbeHeight,
    kScanlineProbeWidth * sizeof(uint16_t),
    0.75f,
    0.35f,
    false,
    0.0f,
    0.2f,
    false,
    1U,
    1.0f,
    false,
    0.0f,
    1.0f,
    0.02f,
    false,
    0.0f,
    0.02f,
    false,
    0.0f,
    0.02f,
    0.0f,
    false,
    0.0f,
    0.0f,
    false,
    0.0f,
    0.01f,
    false,
    false,
    1U,
    1.0f,
    1.0f,
    1.0f,
    0.0f,
    1.0f,
    false,
    0.0f,
    0.0f,
    0.0f,
    0.0f,
    0.0f,
    0.0f,
    1.0f,
    {}
  };
  return BuildFullscreenRender(addresses, geometry, control, control_size,
                               lists);
}

bool BuildFullscreenRender(const RenderAddresses &addresses,
                           const RenderGeometry &geometry,
                           uint8_t *control, uint32_t control_size,
                           RenderCommandLists *lists) {
  const RenderPassConfig pass = {
    kRenderPassAutomatic, kRenderTargetRgb565Raster, 0U, 0U,
    kRenderPackageAutomatic
  };
  return BuildFullscreenRenderPass(addresses, geometry, pass, control,
                                   control_size, lists);
}

bool BuildFullscreenRenderPass(const RenderAddresses &addresses,
                               const RenderGeometry &geometry,
                               const RenderPassConfig &pass,
                               uint8_t *control, uint32_t control_size,
                               RenderCommandLists *lists) {
  RenderLayout layout = {};
  const v3dcrt::shaders::ShaderPackage &package =
      ShaderPackageForGeometry(geometry, pass);
  const uint32_t tile_count =
      RenderTileCount(geometry.target_width, geometry.target_height);
  const bool rgba8_target =
      pass.target_format == kRenderTargetRgba8Tiled;
  const bool valid_target_format =
      pass.target_format == kRenderTargetRgb565Raster || rgba8_target;
  const bool valid_target_layout = rgba8_target ?
      ((addresses.target & 63U) == 0U &&
       pass.target_memory_format >= 1U &&
       pass.target_memory_format <= 5U &&
       pass.target_height_in_ub_or_stride < (1U << 20U)) :
      (geometry.target_width <= UINT32_MAX / sizeof(uint16_t) &&
       geometry.target_stride >=
           geometry.target_width * sizeof(uint16_t) &&
       geometry.target_stride < (1U << 20U));
  if (!BuildRenderLayout(package, &layout) ||
      control == NULL || lists == NULL || control_size < kRenderControlBytes ||
      !AddressRangeValid(addresses.control, kRenderControlBytes) ||
      (addresses.control & 0xfffU) != 0U ||
      addresses.source_texture == 0U ||
      (addresses.source_texture & 63U) != 0U ||
      addresses.target == 0U || (addresses.target & 15U) != 0U ||
      addresses.tile_allocation == 0U ||
      (addresses.tile_allocation & 0xfffU) != 0U ||
      addresses.tile_state !=
          addresses.tile_allocation + kRenderTileStateOffset ||
      (addresses.tile_state & 0xfffU) != 0U ||
      geometry.source_width == 0U || geometry.source_width >= (1U << 14U) ||
      geometry.source_height == 0U ||
      geometry.source_height >= (1U << 14U) ||
      geometry.target_width == 0U || geometry.target_width > UINT16_MAX ||
      geometry.target_height == 0U || geometry.target_height > UINT16_MAX ||
      !valid_target_format || !valid_target_layout ||
      tile_count == 0U ||
      kExpectedRclBytes + 3U * (tile_count - 1U) >
          kRenderControlBytes - layout.rcl_offset) {
    return false;
  }

  memset(control, 0, kRenderControlBytes);
  memset(lists, 0, sizeof *lists);
  if (!BuildProgramsAndUniforms(
          addresses, geometry, package, layout, control)) {
    return false;
  }
  BuildShaderState(addresses, layout, control, lists);
  if (!BuildTextureState(addresses, geometry, control, lists) ||
      !BuildGeneric(addresses, geometry, pass, control, lists) ||
      !BuildRcl(addresses, geometry, layout, control, lists) ||
      !BuildBcl(addresses, geometry, control, lists)) {
    memset(lists, 0, sizeof *lists);
    return false;
  }
  return true;
}

bool PrepareScanlineProbeRenderJob(const RenderAddresses &addresses,
                                   uint8_t *control,
                                   uint32_t control_size,
                                   uint32_t slot,
                                   RenderJob *job) {
  const RenderGeometry geometry = {
    4U,
    4U,
    false,
    false,
    kScanlineProbeWidth,
    kScanlineProbeHeight,
    kScanlineProbeWidth * sizeof(uint16_t),
    0.75f,
    0.35f,
    false,
    0.0f,
    0.2f,
    false,
    1U,
    1.0f,
    false,
    0.0f,
    1.0f,
    0.02f,
    false,
    0.0f,
    0.02f,
    false,
    0.0f,
    0.02f,
    0.0f,
    false,
    0.0f,
    0.0f,
    false,
    0.0f,
    0.01f,
    false,
    false,
    1U,
    1.0f,
    1.0f,
    1.0f,
    0.0f,
    1.0f,
    false,
    0.0f,
    0.0f,
    0.0f,
    0.0f,
    0.0f,
    0.0f,
    1.0f,
    {}
  };
  return PrepareFullscreenRenderJob(addresses, geometry, control,
                                    control_size, slot, job);
}

bool PrepareFullscreenRenderJob(const RenderAddresses &addresses,
                                const RenderGeometry &geometry,
                                uint8_t *control,
                                uint32_t control_size,
                                uint32_t slot,
                                RenderJob *job) {
  const RenderPassConfig pass = {
    kRenderPassAutomatic, kRenderTargetRgb565Raster, 0U, 0U,
    kRenderPackageAutomatic
  };
  return PrepareFullscreenRenderPassJob(
      addresses, geometry, pass, control, control_size, slot, job);
}

bool PrepareFullscreenRenderPassJob(const RenderAddresses &addresses,
                                    const RenderGeometry &geometry,
                                    const RenderPassConfig &pass,
                                    uint8_t *control,
                                    uint32_t control_size,
                                    uint32_t slot,
                                    RenderJob *job) {
  if (job == NULL) {
    return false;
  }
  ResetRenderJob(job);
  if (!BuildFullscreenRenderPass(addresses, geometry, pass, control,
                                 control_size, &job->lists)) {
    return false;
  }
  const v3dcrt::shaders::ShaderPackage &package =
      ShaderPackageForGeometry(geometry, pass);
  RenderLayout layout = {};
  if (!BuildRenderLayout(package, &layout) ||
      !PrepareDynamicUniforms(package, layout, control, control_size, job)) {
    ResetRenderJob(job);
    return false;
  }
  job->slot = slot;
  job->control_address = addresses.control;
  job->target_address = addresses.target;
  job->control_hash = ImmutableControlHash(*job, control);
  job->state = kRenderJobPrepared;
  return true;
}

RenderGeometryParams ResolveRenderGeometryParams(
    bool enabled, float curvature_x, float curvature_y, float skew_x,
    float skew_y, float trapezoid, float rotation_degrees,
    float overscan_scale) {
  RenderGeometryParams params = {
    false, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f
  };
  if (!enabled) {
    return params;
  }
  params.enabled = true;
  params.curvature_x =
      ClampFloat(curvature_x, 0.0f, 100.0f / 600.0f, 0.0f);
  params.curvature_y =
      ClampFloat(curvature_y, 0.0f, 100.0f / 600.0f, 0.0f);
  params.skew_x = ClampFloat(skew_x, -0.08f, 0.08f, 0.0f);
  params.skew_y = ClampFloat(skew_y, -0.08f, 0.08f, 0.0f);
  params.trapezoid = ClampFloat(trapezoid, -0.15f, 0.15f, 0.0f);
  params.rotation_degrees =
      ClampFloat(rotation_degrees, -3.0f, 3.0f, 0.0f);
  params.overscan_scale =
      ClampFloat(overscan_scale, 1.0f, 1.2f, 1.0f);
  return params;
}

RenderScanlineParams ResolveRenderScanlineParams(bool enabled,
                                                 float menu_weight,
                                                 float gap_brightness) {
  RenderScanlineParams params = {0.0f, 1.0f};
  if (!enabled) {
    return params;
  }

  params.weight = ClampFloat(menu_weight, 0.0f, 15.0f, 0.0f) / 15.0f;
  params.gap_brightness =
      ClampFloat(gap_brightness, 0.0f, 1.0f, 1.0f);
  return params;
}

RenderEdgeBlurParams ResolveRenderEdgeBlurParams(bool enabled,
                                                 float strength,
                                                 float radius) {
  RenderEdgeBlurParams params = {false, 0.0f, 0.2f};
  if (!enabled) {
    return params;
  }
  params.enabled = true;
  params.strength = ClampFloat(strength, 0.0f, 1.0f, 0.0f);
  params.radius = ClampFloat(radius, 0.2f, 1.0f, 0.2f);
  return params;
}

RenderPhosphorMaskParams ResolveRenderPhosphorMaskParams(
    bool enabled, uint32_t pattern, float brightness) {
  RenderPhosphorMaskParams params = {false, 1U, 1.0f};
  if (!enabled) {
    return params;
  }
  params.enabled = true;
  params.pattern = pattern < 1U ? 1U : (pattern > 2U ? 2U : pattern);
  params.brightness = ClampFloat(brightness, 0.0f, 1.0f, 1.0f);
  return params;
}

RenderVignetteParams ResolveRenderVignetteParams(
    bool enabled, float strength, float scale, float softness) {
  RenderVignetteParams params = {false, 0.0f, 1.0f, 0.02f};
  if (!enabled) {
    return params;
  }
  params.enabled = true;
  params.strength = ClampFloat(strength, 0.0f, 1.0f, 0.0f);
  params.scale = ClampFloat(scale, 0.2f, 1.0f, 1.0f);
  params.softness = ClampFloat(softness, 0.02f, 1.0f, 0.02f);
  return params;
}

RenderUnevenIlluminationParams ResolveRenderUnevenIlluminationParams(
    bool enabled, float strength, float scale) {
  RenderUnevenIlluminationParams params = {false, 0.0f, 0.02f};
  if (!enabled) {
    return params;
  }
  params.enabled = true;
  params.strength = ClampFloat(strength, 0.0f, 0.35f, 0.0f);
  params.scale = ClampFloat(scale, 0.02f, 0.25f, 0.02f);
  return params;
}

RenderGlassReflectionParams ResolveRenderGlassReflectionParams(
    bool enabled, float angle, float width, float position) {
  RenderGlassReflectionParams params = {false, 0.0f, 0.02f, 0.0f};
  if (!enabled) {
    return params;
  }
  params.enabled = true;
  params.angle = ClampFloat(angle, -60.0f, 60.0f, 0.0f);
  params.width = ClampFloat(width, 0.02f, 0.6f, 0.02f);
  params.position = ClampFloat(position, 0.0f, 1.0f, 0.0f);
  return params;
}

RenderRoundedScreenMaskParams ResolveRenderRoundedScreenMaskParams(
    bool enabled, float corner_radius, float border_softness) {
  RenderRoundedScreenMaskParams params = {false, 0.0f, 0.0f};
  if (!enabled) {
    return params;
  }
  params.enabled = true;
  params.corner_radius = ClampFloat(corner_radius, 0.0f, 0.2f, 0.0f);
  params.border_softness = ClampFloat(border_softness, 0.0f, 0.08f, 0.0f);
  return params;
}

RenderEdgeGlowParams ResolveRenderEdgeGlowParams(
    bool enabled, float strength, float width) {
  RenderEdgeGlowParams params = {false, 0.0f, 0.01f};
  if (!enabled) {
    return params;
  }
  params.enabled = true;
  params.strength = ClampFloat(strength, 0.0f, 0.35f, 0.0f);
  params.width = ClampFloat(width, 0.01f, 0.35f, 0.01f);
  return params;
}

RenderOutputResponseParams ResolveRenderOutputResponseParams(
    bool enabled, bool fast, uint32_t level_mapping, float input_gamma,
    float output_gamma, float saturation, float black_level,
    float white_clip) {
  const v3dcrt::OutputResponseParams shared =
      v3dcrt::ResolveOutputResponseParams(
          enabled, fast, level_mapping, input_gamma, output_gamma,
          saturation, black_level, white_clip);
  const RenderOutputResponseParams params = {
    shared.enabled, shared.fast, shared.level_mapping,
    shared.input_gamma, shared.output_gamma, shared.saturation,
    shared.black_level, shared.white_clip
  };
  return params;
}

RenderConvergenceParams ResolveRenderConvergenceParams(
    bool enabled, float red_offset_x, float red_offset_y,
    float blue_offset_x, float blue_offset_y, float radial_strength) {
  RenderConvergenceParams params = {
    false, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f
  };
  if (!enabled) {
    return params;
  }
  params.enabled = true;
  params.red_offset_x = ClampFloat(red_offset_x, -1.0f, 1.0f, 0.0f);
  params.red_offset_y = ClampFloat(red_offset_y, -1.0f, 1.0f, 0.0f);
  params.blue_offset_x = ClampFloat(blue_offset_x, -1.0f, 1.0f, 0.0f);
  params.blue_offset_y = ClampFloat(blue_offset_y, -1.0f, 1.0f, 0.0f);
  params.radial_strength =
      ClampFloat(radial_strength, 0.0f, 2.0f, 0.0f);
  return params;
}

RenderHorizontalFilteringParams ResolveRenderHorizontalFilteringParams(
    bool enabled, float sigma_x) {
  RenderHorizontalFilteringParams params = {false, 0.0f};
  if (!enabled) {
    return params;
  }
  params.enabled = true;
  params.sigma_x = ClampFloat(sigma_x, 0.0f, 1.0f, 0.0f);
  return params;
}

bool ResolveRenderSourceLinearFilter(bool interpolation_enabled) {
  return interpolation_enabled;
}

RenderBloomParams ResolveRenderBloomParams(bool enabled, float factor) {
  RenderBloomParams params = {false, 0.0f};
  if (!enabled) {
    return params;
  }
  params.enabled = true;
  params.factor = ClampFloat(factor, 0.0f, 5.0f, 0.0f);
  return params;
}

RenderHorizontalJitterParams ResolveRenderHorizontalJitterParams(
    bool enabled, float strength, float frequency, float speed) {
  RenderHorizontalJitterParams params = {false, 0.0f, 0.01f, 0.0f};
  if (!enabled) {
    return params;
  }
  params.enabled = true;
  params.strength = ClampFloat(strength, 0.0f, 6.0f, 0.0f);
  params.frequency = ClampFloat(frequency, 0.01f, 0.4f, 0.01f);
  params.speed = ClampFloat(speed, 0.0f, 1.0f, 0.0f);
  return params;
}

RenderCompositeArtifactsParams ResolveRenderCompositeArtifactsParams(
    bool enabled, float chroma_blur, float luma_sharpen,
    float color_bleed) {
  RenderCompositeArtifactsParams params = {
    false, 0.0f, 0.0f, 0.0f
  };
  if (!enabled) {
    return params;
  }
  params.enabled = true;
  params.chroma_blur = ClampFloat(chroma_blur, 0.0f, 2.0f, 0.0f);
  params.luma_sharpen = ClampFloat(luma_sharpen, 0.0f, 1.0f, 0.0f);
  params.color_bleed = ClampFloat(color_bleed, 0.0f, 0.6f, 0.0f);
  return params;
}

RenderNoiseParams ResolveRenderNoiseParams(
    bool enabled, float luminance, float chroma, float speed) {
  RenderNoiseParams params = {false, 0.0f, 0.0f, 0.0f};
  if (!enabled) {
    return params;
  }
  params.enabled = true;
  params.luminance = ClampFloat(luminance, 0.0f, 0.1f, 0.0f);
  params.chroma = ClampFloat(chroma, 0.0f, 0.08f, 0.0f);
  params.speed = ClampFloat(speed, 0.0f, 1.0f, 0.0f);
  return params;
}

uint16_t ResolveRenderOutputResponseRgb565(
    uint16_t pixel, const RenderOutputResponseParams &params) {
  const v3dcrt::OutputResponseParams shared = {
    params.enabled, params.fast, params.level_mapping,
    params.input_gamma, params.output_gamma, params.saturation,
    params.black_level, params.white_clip
  };
  return v3dcrt::ResolveOutputResponseRgb565(pixel, shared);
}

bool BuildRenderOutputResponseRgb565Lut(
    const RenderOutputResponseParams &params, uint16_t *lut,
    uint32_t lut_entries) {
  const v3dcrt::OutputResponseParams shared = {
    params.enabled, params.fast, params.level_mapping,
    params.input_gamma, params.output_gamma, params.saturation,
    params.black_level, params.white_clip
  };
  return v3dcrt::BuildOutputResponseRgb565Lut(
      shared, lut, lut_entries);
}

bool PatchRenderJobEdgeGlowFrameColors(
    const RenderEdgeGlowFrameColors &colors,
    uint8_t *control, uint32_t control_size, RenderJob *job) {
  if (control == NULL || control_size < kRenderControlBytes || job == NULL ||
      (job->state != kRenderJobPrepared &&
       job->state != kRenderJobCompleted) ||
      !RenderJobControlIntact(*job, control, control_size)) {
    return false;
  }
  if (!HasEdgeGlowFrameColors(job->package_identity) ||
      job->dynamic_uniform_count != 12U) {
    return false;
  }
  for (uint32_t edge = 0U; edge < 4U; ++edge) {
    for (uint32_t channel = 0U; channel < 3U; ++channel) {
      RenderDynamicUniform &uniform =
          job->dynamic_uniforms[edge * 3U + channel];
      uniform.value = FloatBits(ClampFloat(
          colors.color[edge][channel], 0.0f, 1.0f, 0.0f));
      Store32(control + uniform.offset, uniform.value);
    }
  }
  return true;
}

bool PatchRenderJobTemporalFrame(float temporal_frame,
                                 uint8_t *control,
                                 uint32_t control_size,
                                 RenderJob *job) {
  if (control == NULL || control_size < kRenderControlBytes || job == NULL ||
      (job->state != kRenderJobPrepared &&
       job->state != kRenderJobCompleted) ||
      !RenderJobControlIntact(*job, control, control_size)) {
    return false;
  }
  if (!HasTemporalFrame(job->package_identity) ||
      job->dynamic_uniform_count != 1U) {
    return false;
  }
  RenderDynamicUniform &uniform = job->dynamic_uniforms[0];
  uniform.value =
      FloatBits(ClampFloat(temporal_frame, 0.0f, 1023.0f, 0.0f));
  Store32(control + uniform.offset, uniform.value);
  return true;
}

bool ResolveScaledSourceCoordinate(uint32_t target_coordinate,
                                   uint32_t source_size,
                                   uint32_t target_size,
                                   ScaledSourceCoordinate *coordinate) {
  if (coordinate == NULL || source_size == 0U ||
      source_size >= (1U << 14U) || target_size == 0U ||
      target_size > UINT16_MAX || target_coordinate >= target_size) {
    return false;
  }
  const uint64_t numerator =
      (static_cast<uint64_t>(target_coordinate) * 2U + 1U) * source_size;
  const uint64_t denominator = static_cast<uint64_t>(target_size) * 2U;
  coordinate->direct = static_cast<uint32_t>(numerator / denominator);
  coordinate->exact_boundary =
      numerator % denominator == 0U && coordinate->direct > 0U;
  coordinate->alternate = coordinate->exact_boundary ?
      coordinate->direct - 1U : coordinate->direct;
  return coordinate->direct < source_size;
}

bool ResolveLeftEdgePaddedSourceCoordinate(uint32_t output_coordinate,
                                           uint32_t source_width,
                                           uint32_t left_edge_padding,
                                           uint32_t *source_coordinate) {
  if (source_coordinate == NULL || source_width == 0U ||
      output_coordinate >= source_width ||
      left_edge_padding >= source_width) {
    return false;
  }
  *source_coordinate = output_coordinate < left_edge_padding ?
      0U : output_coordinate - left_edge_padding;
  return true;
}

uint32_t RenderTileCount(uint32_t width, uint32_t height) {
  if (width == 0U || height == 0U ||
      width > UINT32_MAX - (kTileSizePixels - 1U) ||
      height > UINT32_MAX - (kTileSizePixels - 1U)) {
    return 0U;
  }
  const uint32_t tiles_x =
      (width + kTileSizePixels - 1U) / kTileSizePixels;
  const uint32_t tiles_y =
      (height + kTileSizePixels - 1U) / kTileSizePixels;
  return tiles_x > UINT32_MAX / tiles_y ? 0U : tiles_x * tiles_y;
}

uint32_t RenderTileStateBytes(uint32_t width, uint32_t height) {
  const uint32_t tile_count = RenderTileCount(width, height);
  if (tile_count == 0U ||
      tile_count > UINT32_MAX / kTileStateBytesPerTile) {
    return 0U;
  }
  const uint32_t bytes = tile_count * kTileStateBytesPerTile;
  if (bytes > UINT32_MAX - 4095U) {
    return 0U;
  }
  return (bytes + 4095U) & ~4095U;
}

bool StartRenderJob(RenderJob *job) {
  if (job == NULL ||
      (job->state != kRenderJobPrepared &&
       job->state != kRenderJobCompleted) ||
      job->submission_count == UINT32_MAX) {
    return false;
  }
  ++job->submission_count;
  job->state = kRenderJobSubmitted;
  return true;
}

bool FinishRenderJob(RenderJob *job, bool succeeded) {
  if (job == NULL || job->state != kRenderJobSubmitted) {
    return false;
  }
  job->state = succeeded ? kRenderJobCompleted : kRenderJobFailed;
  return true;
}

bool RenderJobControlIntact(const RenderJob &job, const uint8_t *control,
                            uint32_t control_size) {
  if (control == NULL || control_size < kRenderControlBytes ||
      job.state == kRenderJobEmpty || job.control_address == 0U ||
      job.dynamic_uniform_count > kRenderJobMaxDynamicUniforms ||
      job.control_hash != ImmutableControlHash(job, control)) {
    return false;
  }
  for (uint32_t i = 0U; i < job.dynamic_uniform_count; ++i) {
    const RenderDynamicUniform &uniform = job.dynamic_uniforms[i];
    if (uniform.offset > control_size - sizeof(uint32_t) ||
        Load32(control + uniform.offset) != uniform.value) {
      return false;
    }
  }
  return true;
}

bool RenderJobDynamicUniformRange(const RenderJob &job, uint32_t *offset,
                                  uint32_t *size) {
  if (offset == NULL || size == NULL || job.dynamic_uniform_count == 0U ||
      job.dynamic_uniform_count > kRenderJobMaxDynamicUniforms ||
      job.dynamic_uniform_min_offset >= job.dynamic_uniform_end_offset ||
      job.dynamic_uniform_end_offset > kRenderControlBytes) {
    return false;
  }
  *offset = job.dynamic_uniform_min_offset;
  *size = job.dynamic_uniform_end_offset - job.dynamic_uniform_min_offset;
  return true;
}

void ResetRenderJob(RenderJob *job) {
  if (job != NULL) {
    memset(job, 0, sizeof *job);
  }
}

void FillScanlineProbeSource(uint8_t *source, uint32_t source_size) {
  if (source == NULL || source_size < kScanlineProbeSourceBytes) {
    return;
  }
  memset(source, 0, source_size);
  for (uint32_t y = 0U; y < 4U; ++y) {
    for (uint32_t x = 0U; x < 4U; ++x) {
      uint8_t *pixel = source + (y * 4U + x) * 4U;
      pixel[0] = static_cast<uint8_t>(32U + x * 48U);
      pixel[1] = static_cast<uint8_t>(48U + y * 48U);
      pixel[2] = static_cast<uint8_t>(96U + ((x + y) & 3U) * 32U);
      pixel[3] = 255U;
    }
  }
}

void FillScanlineProbeTarget(uint8_t *target, uint32_t target_size) {
  if (target == NULL) {
    return;
  }
  uint16_t *words = reinterpret_cast<uint16_t *>(target);
  for (uint32_t i = 0U; i < target_size / sizeof(uint16_t); ++i) {
    words[i] = kScanlineProbeTargetSentinel;
  }
  if ((target_size & 1U) != 0U) {
    target[target_size - 1U] =
        static_cast<uint8_t>(kScanlineProbeTargetSentinel);
  }
}

bool AnalyzeScanlineProbeTarget(const uint8_t *target, uint32_t target_size,
                                RenderReadback *readback) {
  if (target == NULL || readback == NULL ||
      target_size < kScanlineProbeTargetBytes) {
    return false;
  }
  memset(readback, 0, sizeof *readback);
  const uint16_t *pixels = reinterpret_cast<const uint16_t *>(target);
  const uint32_t pixel_count =
      kScanlineProbeWidth * kScanlineProbeHeight;
  for (uint32_t i = 0U; i < pixel_count; ++i) {
    if (pixels[i] != kScanlineProbeTargetSentinel) {
      ++readback->changed_pixels;
    }
    if (pixels[i] != 0U) {
      ++readback->nonzero_pixels;
    }
    bool seen = false;
    for (uint32_t j = 0U; j < i; ++j) {
      if (pixels[j] == pixels[i]) {
        seen = true;
        break;
      }
    }
    if (!seen) {
      ++readback->unique_colors;
    }
  }

  for (uint32_t i = pixel_count;
       i < target_size / sizeof(uint16_t); ++i) {
    if (pixels[i] != kScanlineProbeTargetSentinel) {
      ++readback->guard_mismatches;
    }
  }
  readback->hash = Fnv1a32(target, kScanlineProbeTargetBytes);
  readback->first = pixels[0];
  readback->middle = pixels[pixel_count / 2U];
  readback->last = pixels[pixel_count - 1U];
  return readback->changed_pixels >= 240U &&
         readback->nonzero_pixels >= 240U &&
         readback->unique_colors >= 8U &&
         readback->guard_mismatches == 0U;
}

bool CopyScanlineProbeToScanout(const uint8_t *target,
                                uint32_t target_size,
                                uint8_t *scanout,
                                uint32_t scanout_width,
                                uint32_t scanout_height,
                                uint32_t scanout_pitch,
                                uint32_t *scanout_hash) {
  const uint32_t row_bytes = kScanlineProbeWidth * sizeof(uint16_t);
  if (target == NULL || target_size < kScanlineProbeTargetBytes ||
      scanout == NULL || scanout_width < kScanlineProbeWidth ||
      scanout_height < kScanlineProbeHeight ||
      scanout_pitch < row_bytes || scanout_hash == NULL) {
    return false;
  }

  for (uint32_t y = 0U; y < kScanlineProbeHeight; ++y) {
    memcpy(scanout + y * scanout_pitch, target + y * row_bytes, row_bytes);
  }
  *scanout_hash = Fnv1a32(target, kScanlineProbeTargetBytes);
  return true;
}

}  // namespace pi4v3d
