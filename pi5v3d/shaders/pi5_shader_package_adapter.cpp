#include "pi5v3d/shaders/pi5_shader_package_adapter.h"

#include "pi5v3d/shaders/generated/scanline_probe_artifact.h"
#include "v3dcrt/shaders/generated/v3d71/crt_bloom_blur_horizontal.h"
#include "v3dcrt/shaders/generated/v3d71/crt_bloom_blur_vertical.h"
#include "v3dcrt/shaders/generated/v3d71/crt_bloom_composite.h"
#include "v3dcrt/shaders/generated/v3d71/crt_composite_probe.h"
#include "v3dcrt/shaders/generated/v3d71/crt_convergence_probe.h"
#include "v3dcrt/shaders/generated/v3d71/crt_core_probe.h"
#include "v3dcrt/shaders/generated/v3d71/crt_edge_blur_probe.h"
#include "v3dcrt/shaders/generated/v3d71/crt_edge_glow_probe.h"
#include "v3dcrt/shaders/generated/v3d71/crt_illumination_jitter_probe.h"
#include "v3dcrt/shaders/generated/v3d71/crt_mask_vignette_probe.h"
#include "v3dcrt/shaders/generated/v3d71/crt_noise_probe.h"
#include "v3dcrt/shaders/generated/v3d71/crt_output_edge_blur.h"
#include "v3dcrt/shaders/generated/v3d71/crt_output_edge_glow.h"
#include "v3dcrt/shaders/generated/v3d71/crt_output_response.h"
#include "v3dcrt/shaders/generated/v3d71/crt_output_response_fast_cubic.h"
#include "v3dcrt/shaders/generated/v3d71/crt_output_glass_reflection.h"
#include "v3dcrt/shaders/generated/v3d71/crt_output_geometry.h"
#include "v3dcrt/shaders/generated/v3d71/crt_output_phosphor_mask.h"
#include "v3dcrt/shaders/generated/v3d71/crt_output_rounded_screen_mask.h"
#include "v3dcrt/shaders/generated/v3d71/crt_output_scanlines.h"
#include "v3dcrt/shaders/generated/v3d71/crt_output_uneven_illumination.h"
#include "v3dcrt/shaders/generated/v3d71/crt_output_vignette.h"
#include "v3dcrt/shaders/generated/v3d71/crt_single_pass.h"
#include "v3dcrt/shaders/generated/v3d71/crt_source_composite.h"
#include "v3dcrt/shaders/generated/v3d71/crt_source_convergence.h"
#include "v3dcrt/shaders/generated/v3d71/crt_source_filter.h"
#include "v3dcrt/shaders/generated/v3d71/crt_source_horizontal_jitter.h"
#include "v3dcrt/shaders/generated/v3d71/crt_source_noise.h"
#include "v3dcrt/shaders/generated/v3d71/crt_surface_response_probe.h"
#include "v3dcrt/shaders/generated/v3d71/scanline_probe.h"

#include <string.h>

namespace pi5v3d {
namespace shader_artifacts {

namespace {

using v3dcrt::shaders::FindShaderStage;
using v3dcrt::shaders::ShaderBackendCapabilities;
using v3dcrt::shaders::ShaderPackage;
using v3dcrt::shaders::ShaderStageKind;
using v3dcrt::shaders::ShaderStageProgram;
using v3dcrt::shaders::ShaderUniformSpec;
using v3dcrt::shaders::ValidateShaderPackage;

constexpr uint32_t kStageCount = kPreparedFragmentStageCount;
constexpr uint32_t kMaxStageUniformWords =
    kPreparedFragmentMaxStageUniformWords;
constexpr uint32_t kMaxTextureSamples = 16U;
constexpr uint32_t kPatchPointCount = kPreparedFragmentPatchPointCount;
constexpr uint32_t kShaderCodeSliceBytes = 8U * 1024U;
constexpr uint32_t kShaderUniformSliceBytes = 4U * 1024U;
constexpr uint32_t kCodeLayoutBase = 0x100U;
constexpr uint32_t kUniformLayoutBase = 0x100U;
constexpr uint32_t kCodeAlignment = 64U;
constexpr uint32_t kUniformAlignment = 16U;
constexpr uint16_t kInvalidFragmentUniformIndex = 0xffffU;

static_assert((kPreparedFragmentSemanticSlotCount &
               (kPreparedFragmentSemanticSlotCount - 1U)) == 0U,
              "semantic cache size must be a power of two");

bool StringEquals(const char *left, const char *right) {
  return left != nullptr && right != nullptr && strcmp(left, right) == 0;
}

bool Fail(const char *text, const char **reason);

uint32_t SemanticHash(const char *semantic) {
  uint32_t hash = 2166136261U;
  if (semantic == nullptr) {
    return hash;
  }
  while (*semantic != '\0') {
    hash ^= static_cast<uint8_t>(*semantic++);
    hash *= 16777619U;
  }
  return hash;
}

bool FindPreparedSemanticSlot(
    const PreparedFragmentShaderPackage &prepared,
    const char *semantic,
    uint32_t *slot) {
  if (semantic == nullptr || slot == nullptr) {
    return false;
  }
  uint32_t candidate =
      SemanticHash(semantic) & (kPreparedFragmentSemanticSlotCount - 1U);
  for (uint32_t probe = 0;
       probe < kPreparedFragmentSemanticSlotCount; ++probe) {
    const char *key = prepared.fragment_semantic_keys[candidate];
    if (key == nullptr) {
      return false;
    }
    if (StringEquals(key, semantic)) {
      *slot = candidate;
      return true;
    }
    candidate = (candidate + 1U) &
        (kPreparedFragmentSemanticSlotCount - 1U);
  }
  return false;
}

bool FindOrInsertPreparedSemanticSlot(
    PreparedFragmentShaderPackage *prepared,
    const char *semantic,
    uint32_t *slot) {
  if (prepared == nullptr || semantic == nullptr || slot == nullptr) {
    return false;
  }
  uint32_t candidate =
      SemanticHash(semantic) & (kPreparedFragmentSemanticSlotCount - 1U);
  for (uint32_t probe = 0;
       probe < kPreparedFragmentSemanticSlotCount; ++probe) {
    const char *key = prepared->fragment_semantic_keys[candidate];
    if (key == nullptr) {
      prepared->fragment_semantic_keys[candidate] = semantic;
      prepared->fragment_semantic_heads[candidate] =
          kInvalidFragmentUniformIndex;
      *slot = candidate;
      return true;
    }
    if (StringEquals(key, semantic)) {
      *slot = candidate;
      return true;
    }
    candidate = (candidate + 1U) &
        (kPreparedFragmentSemanticSlotCount - 1U);
  }
  return false;
}

bool BuildPreparedFragmentSemanticIndex(
    PreparedFragmentShaderPackage *prepared,
    const char **reason) {
  if (prepared == nullptr || prepared->package == nullptr) {
    return Fail("pi5-semantic-index-package-missing", reason);
  }
  const ShaderStageProgram *fragment = FindShaderStage(
      *prepared->package, v3dcrt::shaders::kShaderStageFragment);
  if (fragment == nullptr || fragment->uniforms == nullptr ||
      fragment->uniform_count > kPreparedFragmentMaxStageUniformWords) {
    return Fail("pi5-semantic-index-uniform-count", reason);
  }

  for (uint32_t i = 0; i < kPreparedFragmentSemanticSlotCount; ++i) {
    prepared->fragment_semantic_keys[i] = nullptr;
    prepared->fragment_semantic_heads[i] = kInvalidFragmentUniformIndex;
  }
  for (uint32_t i = 0; i < kPreparedFragmentMaxStageUniformWords; ++i) {
    prepared->fragment_semantic_next[i] = kInvalidFragmentUniformIndex;
  }
  prepared->fragment_uniform_count =
      static_cast<uint16_t>(fragment->uniform_count);
  prepared->first_tmu_p0_index = kInvalidFragmentUniformIndex;
  prepared->first_tmu_p1_index = kInvalidFragmentUniformIndex;

  for (uint32_t i = 0; i < fragment->uniform_count; ++i) {
    const ShaderUniformSpec &uniform = fragment->uniforms[i];
    if (prepared->first_tmu_p0_index == kInvalidFragmentUniformIndex &&
        StringEquals(uniform.kind, "QUNIFORM_TMU_CONFIG_P0")) {
      prepared->first_tmu_p0_index = static_cast<uint16_t>(i);
    }
    if (prepared->first_tmu_p1_index == kInvalidFragmentUniformIndex &&
        StringEquals(uniform.kind, "QUNIFORM_TMU_CONFIG_P1")) {
      prepared->first_tmu_p1_index = static_cast<uint16_t>(i);
    }
    uint32_t slot = 0;
    if (!FindOrInsertPreparedSemanticSlot(
            prepared, uniform.semantic, &slot)) {
      return Fail("pi5-semantic-index-capacity", reason);
    }
    prepared->fragment_semantic_next[i] =
        prepared->fragment_semantic_heads[slot];
    prepared->fragment_semantic_heads[slot] = static_cast<uint16_t>(i);
  }
  return true;
}

enum FragmentContractGroup : uint64_t {
  kContractReplay = 1ULL << 0,
  kContractSourceFilter = 1ULL << 1,
  kContractSourceConvergence = 1ULL << 2,
  kContractSourceComposite = 1ULL << 3,
  kContractSourceJitter = 1ULL << 4,
  kContractSourceNoise = 1ULL << 5,
  kContractOutputGeometry = 1ULL << 6,
  kContractOutputRotation = 1ULL << 7,
  kContractOutputFrameRotation = 1ULL << 8,
  kContractOutputScanlines = 1ULL << 9,
  kContractOutputEdgeBlur = 1ULL << 10,
  kContractOutputMask = 1ULL << 11,
  kContractOutputVignette = 1ULL << 12,
  kContractOutputIllumination = 1ULL << 13,
  kContractOutputReflection = 1ULL << 14,
  kContractOutputGlassAngle = 1ULL << 15,
  kContractOutputFrameGlass = 1ULL << 16,
  kContractOutputRoundedMask = 1ULL << 17,
  kContractOutputEdgeGlow = 1ULL << 18,
  kContractOutputResponse = 1ULL << 19,
  kContractOutputResponseGeneric = 1ULL << 20,
  kContractOutputResponsePrecomputed = 1ULL << 21,
  kContractBloomHorizontal = 1ULL << 22,
  kContractBloomVertical = 1ULL << 23,
  kContractBloomComposite = 1ULL << 24,
  kContractProbeCore = 1ULL << 25,
  kContractProbeConvergence = 1ULL << 26,
  kContractProbeEdgeBlur = 1ULL << 27,
  kContractProbeEdgeGlow = 1ULL << 28,
  kContractProbeSurfaceResponse = 1ULL << 29,
  kContractProbeMaskVignette = 1ULL << 30,
  kContractProbeIlluminationJitter = 1ULL << 31,
  kContractProbeComposite = 1ULL << 32,
  kContractProbeNoise = 1ULL << 33,
};

enum FragmentContractFlag : uint8_t {
  kContractFlagNone = 0,
  kContractFlagFastCubic = 1U << 0,
  kContractFlagBloomComposite = 1U << 1,
};

struct FragmentPackageDescriptor {
  const ShaderPackage *package;
  const char *expected_id;
  const char *artifact_name;
  uint64_t contract_groups;
  const char *resource_reason;
  uint8_t expected_tmus;
  uint8_t flags;
};

constexpr uint64_t kSourceFilterGroups = kContractSourceFilter;
constexpr uint64_t kSourceConvergenceGroups =
    kSourceFilterGroups | kContractSourceConvergence;
constexpr uint64_t kSourceCompositeGroups =
    kSourceConvergenceGroups | kContractSourceComposite;
constexpr uint64_t kSourceJitterGroups =
    kSourceCompositeGroups | kContractSourceJitter;
constexpr uint64_t kSourceNoiseGroups =
    kSourceJitterGroups | kContractSourceNoise;
constexpr uint64_t kOutputGeometryGroups =
    kContractOutputGeometry | kContractOutputRotation;
constexpr uint64_t kOutputScanlineGroups =
    kOutputGeometryGroups | kContractOutputScanlines;
constexpr uint64_t kOutputEdgeBlurGroups =
    kOutputScanlineGroups | kContractOutputEdgeBlur;
constexpr uint64_t kOutputMaskGroups =
    kOutputEdgeBlurGroups | kContractOutputMask;
constexpr uint64_t kOutputVignetteGroups =
    kOutputMaskGroups | kContractOutputVignette;
constexpr uint64_t kOutputIlluminationGroups =
    kOutputVignetteGroups | kContractOutputIllumination;
constexpr uint64_t kOutputReflectionGroups =
    kOutputIlluminationGroups | kContractOutputReflection |
    kContractOutputGlassAngle;
constexpr uint64_t kOutputRoundedMaskGroups =
    kOutputReflectionGroups | kContractOutputRoundedMask;
constexpr uint64_t kOutputEdgeGlowGroups =
    kOutputRoundedMaskGroups | kContractOutputEdgeGlow;
constexpr uint64_t kOutputResponseGroups =
    (kOutputEdgeGlowGroups &
     ~(kContractOutputRotation | kContractOutputGlassAngle)) |
    kContractOutputFrameRotation | kContractOutputFrameGlass |
    kContractOutputResponse;
constexpr uint64_t kProbeCoreGroups =
    kContractReplay | kContractProbeCore;
constexpr uint64_t kProbeConvergenceGroups =
    kProbeCoreGroups | kContractProbeConvergence;
constexpr uint64_t kProbeEdgeBlurGroups =
    kProbeConvergenceGroups | kContractProbeEdgeBlur;
constexpr uint64_t kProbeEdgeGlowGroups =
    kProbeEdgeBlurGroups | kContractProbeEdgeGlow;
constexpr uint64_t kProbeSurfaceResponseGroups =
    kProbeEdgeGlowGroups | kContractProbeSurfaceResponse;
constexpr uint64_t kProbeMaskVignetteGroups =
    kProbeSurfaceResponseGroups | kContractProbeMaskVignette;
constexpr uint64_t kProbeIlluminationJitterGroups =
    kProbeMaskVignetteGroups | kContractProbeIlluminationJitter;
constexpr uint64_t kProbeCompositeGroups =
    kProbeIlluminationJitterGroups | kContractProbeComposite;
constexpr uint64_t kProbeNoiseGroups =
    kProbeCompositeGroups | kContractProbeNoise;

const FragmentPackageDescriptor
    kFragmentPackageDescriptors[kFragmentShaderPackageCount] = {
  {&v3dcrt::shaders::generated::kV3dScanlineProbePackage,
   "scanline_probe", "scanline-probe-dynamic-v3d71",
   kContractReplay, "pi5-scanline-resource-contract", 1U,
   kContractFlagNone},
  {&v3dcrt::shaders::generated::kV3dCrtSourceFilterPackage,
   "crt_source_filter", "crt-source-filter-dynamic-v3d71",
   kSourceFilterGroups, "pi5-source-filter-resource-contract", 3U,
   kContractFlagNone},
  {&v3dcrt::shaders::generated::kV3dCrtSourceConvergencePackage,
   "crt_source_convergence", "crt-source-convergence-dynamic-v3d71",
   kSourceConvergenceGroups, "pi5-source-convergence-resource-contract", 5U,
   kContractFlagNone},
  {&v3dcrt::shaders::generated::kV3dCrtSourceCompositePackage,
   "crt_source_composite", "crt-source-composite-dynamic-v3d71",
   kSourceCompositeGroups, "pi5-source-composite-resource-contract", 5U,
   kContractFlagNone},
  {&v3dcrt::shaders::generated::kV3dCrtSourceHorizontalJitterPackage,
   "crt_source_horizontal_jitter",
   "crt-source-horizontal-jitter-dynamic-v3d71", kSourceJitterGroups,
   "pi5-source-horizontal-jitter-resource-contract", 5U,
   kContractFlagNone},
  {&v3dcrt::shaders::generated::kV3dCrtSourceNoisePackage,
   "crt_source_noise", "crt-source-noise-dynamic-v3d71",
   kSourceNoiseGroups, "pi5-source-noise-resource-contract", 5U,
   kContractFlagNone},
  {&v3dcrt::shaders::generated::kV3dCrtOutputGeometryPackage,
   "crt_output_geometry", "crt-output-geometry-dynamic-v3d71",
   kOutputGeometryGroups, "pi5-output-geometry-resource-contract", 1U,
   kContractFlagNone},
  {&v3dcrt::shaders::generated::kV3dCrtOutputScanlinesPackage,
   "crt_output_scanlines", "crt-output-scanlines-dynamic-v3d71",
   kOutputScanlineGroups, "pi5-output-scanlines-resource-contract", 1U,
   kContractFlagNone},
  {&v3dcrt::shaders::generated::kV3dCrtOutputEdgeBlurPackage,
   "crt_output_edge_blur", "crt-output-edge-blur-dynamic-v3d71",
   kOutputEdgeBlurGroups, "pi5-output-edge-blur-resource-contract", 5U,
   kContractFlagNone},
  {&v3dcrt::shaders::generated::kV3dCrtOutputPhosphorMaskPackage,
   "crt_output_phosphor_mask",
   "crt-output-phosphor-mask-dynamic-v3d71", kOutputMaskGroups,
   "pi5-output-phosphor-mask-resource-contract", 5U, kContractFlagNone},
  {&v3dcrt::shaders::generated::kV3dCrtOutputVignettePackage,
   "crt_output_vignette", "crt-output-vignette-dynamic-v3d71",
   kOutputVignetteGroups, "pi5-output-vignette-resource-contract", 5U,
   kContractFlagNone},
  {&v3dcrt::shaders::generated::kV3dCrtOutputUnevenIlluminationPackage,
   "crt_output_uneven_illumination",
   "crt-output-uneven-illumination-dynamic-v3d71",
   kOutputIlluminationGroups,
   "pi5-output-uneven-illumination-resource-contract", 5U,
   kContractFlagNone},
  {&v3dcrt::shaders::generated::kV3dCrtOutputGlassReflectionPackage,
   "crt_output_glass_reflection",
   "crt-output-glass-reflection-dynamic-v3d71", kOutputReflectionGroups,
   "pi5-output-glass-reflection-resource-contract", 5U,
   kContractFlagNone},
  {&v3dcrt::shaders::generated::kV3dCrtOutputRoundedScreenMaskPackage,
   "crt_output_rounded_screen_mask",
   "crt-output-rounded-screen-mask-dynamic-v3d71",
   kOutputRoundedMaskGroups,
   "pi5-output-rounded-screen-mask-resource-contract", 5U,
   kContractFlagNone},
  {&v3dcrt::shaders::generated::kV3dCrtOutputEdgeGlowPackage,
   "crt_output_edge_glow", "crt-output-edge-glow-dynamic-v3d71",
   kOutputEdgeGlowGroups, "pi5-output-edge-glow-resource-contract", 5U,
   kContractFlagNone},
  {&v3dcrt::shaders::generated::kV3dCrtOutputResponsePackage,
   "crt_output_response", "crt-output-response-dynamic-v3d71",
   kOutputResponseGroups | kContractOutputResponseGeneric,
   "pi5-output-response-resource-contract", 5U, kContractFlagNone},
  {&v3dcrt::shaders::generated::kV3dCrtOutputResponseFastCubicPackage,
   "crt_output_response_fast_cubic",
   "crt-output-response-fast-cubic-dynamic-v3d71",
   kOutputResponseGroups | kContractOutputResponsePrecomputed,
   "pi5-output-response-resource-contract", 5U, kContractFlagFastCubic},
  {&v3dcrt::shaders::generated::kV3dCrtBloomBlurHorizontalPackage,
   "crt_bloom_blur_horizontal", "crt-bloom-horizontal-dynamic-v3d71",
   kContractBloomHorizontal, "pi5-bloom-horizontal-contract", 3U,
   kContractFlagNone},
  {&v3dcrt::shaders::generated::kV3dCrtBloomBlurVerticalPackage,
   "crt_bloom_blur_vertical", "crt-bloom-vertical-dynamic-v3d71",
   kContractBloomVertical, "pi5-bloom-vertical-contract", 11U,
   kContractFlagNone},
  {&v3dcrt::shaders::generated::kV3dCrtBloomCompositePackage,
   "crt_bloom_composite", "crt-bloom-composite-dynamic-v3d71",
   kContractBloomComposite, "pi5-bloom-composite-resource-contract", 2U,
   kContractFlagBloomComposite},
  {&v3dcrt::shaders::generated::kV3dCrtCoreProbePackage,
   "crt_core_probe", "crt-core-probe-dynamic-v3d71", kProbeCoreGroups,
   "pi5-core-probe-resource-contract", 3U, kContractFlagNone},
  {&v3dcrt::shaders::generated::kV3dCrtConvergenceProbePackage,
   "crt_convergence_probe", "crt-convergence-probe-dynamic-v3d71",
   kProbeConvergenceGroups, "pi5-convergence-probe-resource-contract", 5U,
   kContractFlagNone},
  {&v3dcrt::shaders::generated::kV3dCrtEdgeBlurProbePackage,
   "crt_edge_blur_probe", "crt-edge-blur-probe-dynamic-v3d71",
   kProbeEdgeBlurGroups, "pi5-edge-blur-probe-resource-contract", 9U,
   kContractFlagNone},
  {&v3dcrt::shaders::generated::kV3dCrtEdgeGlowProbePackage,
   "crt_edge_glow_probe", "crt-edge-glow-probe-dynamic-v3d71",
   kProbeEdgeGlowGroups, "pi5-edge-glow-probe-resource-contract", 13U,
   kContractFlagNone},
  {&v3dcrt::shaders::generated::kV3dCrtSurfaceResponseProbePackage,
   "crt_surface_response_probe",
   "crt-surface-response-probe-dynamic-v3d71", kProbeSurfaceResponseGroups,
   "pi5-surface-response-probe-resource-contract", 13U,
   kContractFlagNone},
  {&v3dcrt::shaders::generated::kV3dCrtMaskVignetteProbePackage,
   "crt_mask_vignette_probe", "crt-mask-vignette-probe-dynamic-v3d71",
   kProbeMaskVignetteGroups, "pi5-mask-vignette-probe-resource-contract", 13U,
   kContractFlagNone},
  {&v3dcrt::shaders::generated::kV3dCrtIlluminationJitterProbePackage,
   "crt_illumination_jitter_probe",
   "crt-illumination-jitter-probe-dynamic-v3d71",
   kProbeIlluminationJitterGroups,
   "pi5-illumination-jitter-probe-resource-contract", 13U,
   kContractFlagNone},
  {&v3dcrt::shaders::generated::kV3dCrtCompositeProbePackage,
   "crt_composite_probe", "crt-composite-probe-dynamic-v3d71",
   kProbeCompositeGroups, "pi5-composite-probe-resource-contract", 13U,
   kContractFlagNone},
  {&v3dcrt::shaders::generated::kV3dCrtNoiseProbePackage,
   "crt_noise_probe", "crt-noise-probe-dynamic-v3d71",
   kProbeNoiseGroups, "pi5-noise-probe-resource-contract", 13U,
   kContractFlagNone},
  {&v3dcrt::shaders::generated::kV3dCrtSinglePassPackage,
   "crt_single_pass", "crt-single-pass-dynamic-v3d71",
   kProbeNoiseGroups, "pi5-single-pass-resource-contract", 13U,
   kContractFlagNone},
};

static_assert(sizeof kFragmentPackageDescriptors /
                  sizeof kFragmentPackageDescriptors[0] ==
              kFragmentShaderPackageCount,
              "every Pi5 fragment package needs a descriptor");

const FragmentPackageDescriptor &DescriptorForKind(
    FragmentShaderPackageKind kind) {
  const uint32_t index = static_cast<uint32_t>(kind);
  return kFragmentPackageDescriptors[
      index < kFragmentShaderPackageCount ? index : 0U];
}

bool Fail(const char *text, const char **reason) {
  if (reason != nullptr) {
    *reason = text;
  }
  return false;
}

uint32_t AlignUp(uint32_t value, uint32_t alignment) {
  return (value + alignment - 1U) & ~(alignment - 1U);
}

uint32_t FloatBits(float value) {
  uint32_t bits = 0;
  memcpy(&bits, &value, sizeof bits);
  return bits;
}

ShaderStageKind PackageStageKind(ShaderArtifactStage stage) {
  switch (stage) {
    case kArtifactStageCoordinate:
      return v3dcrt::shaders::kShaderStageCoordinate;
    case kArtifactStageVertex:
      return v3dcrt::shaders::kShaderStageVertex;
    case kArtifactStageFragment:
    case kArtifactStageNone:
    default:
      return v3dcrt::shaders::kShaderStageFragment;
  }
}

const ShaderPackage *PackageForKind(FragmentShaderPackageKind kind) {
  return DescriptorForKind(kind).package;
}

const ShaderArtifactUniformBlock *FindFixtureUniform(
    ShaderArtifactStage stage) {
  const ShaderArtifact &fixture = generated::kMesaScanlineProbeArtifact;
  for (uint32_t i = 0; i < fixture.uniform_count; ++i) {
    if (fixture.uniforms[i].stage == stage) {
      return &fixture.uniforms[i];
    }
  }
  return nullptr;
}

bool ValidateFixedStageUniforms(const ShaderStageProgram &stage,
                                ShaderStageKind kind,
                                const char **reason) {
  static const char *const coordinate_kinds[] = {
    "QUNIFORM_CONSTANT",
    "QUNIFORM_VIEWPORT_X_SCALE",
    "QUNIFORM_VIEWPORT_Y_SCALE",
    "QUNIFORM_CONSTANT",
  };
  static const uint32_t coordinate_data[] = {
    0U, 0U, 0U, 0x3f800000U,
  };
  static const char *const vertex_kinds[] = {
    "QUNIFORM_VIEWPORT_X_SCALE",
    "QUNIFORM_VIEWPORT_Y_SCALE",
    "QUNIFORM_VIEWPORT_Z_OFFSET",
    "QUNIFORM_CONSTANT",
  };
  static const uint32_t vertex_data[] = {
    0U, 0U, 0U, 0x3f800000U,
  };

  const char *const *expected_kinds = kind ==
      v3dcrt::shaders::kShaderStageCoordinate ? coordinate_kinds :
                                                vertex_kinds;
  const uint32_t *expected_data = kind ==
      v3dcrt::shaders::kShaderStageCoordinate ? coordinate_data :
                                                vertex_data;
  if (stage.uniform_count != 4U || stage.uniforms == nullptr) {
    return Fail("pi5-fixed-stage-uniform-count", reason);
  }
  for (uint32_t i = 0; i < stage.uniform_count; ++i) {
    if (!StringEquals(stage.uniforms[i].kind, expected_kinds[i]) ||
        stage.uniforms[i].data != expected_data[i] ||
        !StringEquals(stage.uniforms[i].semantic, "")) {
      return Fail("pi5-fixed-stage-uniform-contract", reason);
    }
  }
  return true;
}

bool HasFragmentUniform(const ShaderStageProgram &fragment,
                        const char *kind,
                        const char *semantic) {
  for (uint32_t i = 0; i < fragment.uniform_count; ++i) {
    if (StringEquals(fragment.uniforms[i].kind, kind) &&
        StringEquals(fragment.uniforms[i].semantic, semantic)) {
      return true;
    }
  }
  return false;
}

uint32_t CountFragmentUniforms(const ShaderStageProgram &fragment,
                               const char *kind,
                               const char *semantic) {
  uint32_t matches = 0;
  for (uint32_t i = 0; i < fragment.uniform_count; ++i) {
    if (StringEquals(fragment.uniforms[i].kind, kind) &&
        StringEquals(fragment.uniforms[i].semantic, semantic)) {
      ++matches;
    }
  }
  return matches;
}

struct SemanticContract {
  uint64_t group;
  const char *const *semantics;
  const char *reason;
  uint32_t count;
  uint32_t fast_cubic_optional_mask;
};

bool ValidateSemanticContracts(
    const ShaderStageProgram &fragment,
    const FragmentPackageDescriptor &descriptor,
    const char **reason) {
  static const char *const replay[] = {
    "fragcoord_y_scale", "fragcoord_y_bias",
    "scanline_gap_brightness", "scanline_weight",
  };
  static const char *const source_filter[] = {
    "source_texel_x", "source_texel_y",
    "horizontal_filter_enable", "horizontal_sigma_x",
  };
  static const char *const source_convergence[] = {
    "convergence_enable", "red_offset_x", "red_offset_y",
    "blue_offset_x", "blue_offset_y", "convergence_radial_strength",
  };
  static const char *const source_composite[] = {
    "composite_artifacts_enable", "composite_chroma_blur",
    "composite_luma_sharpen", "composite_color_bleed",
  };
  static const char *const source_jitter[] = {
    "horizontal_jitter_enable", "horizontal_jitter_strength",
    "horizontal_jitter_frequency",
  };
  static const char *const source_noise[] = {
    "noise_enable", "luminance_noise", "chroma_noise",
    "horizontal_jitter_speed", "noise_speed", "temporal_frame",
  };
  static const char *const output_geometry[] = {
    "source_texel_x", "source_texel_y", "geometry_enable",
    "curvature_x", "curvature_y", "skew_x", "skew_y", "trapezoid",
    "overscan_scale", "fragcoord_y_scale",
  };
  static const char *const output_rotation[] = {"rotation_radians"};
  static const char *const output_frame_rotation[] = {
    "rotation_cosine", "rotation_sine",
  };
  static const char *const output_scanlines[] = {
    "scanline_weight", "scanline_gap_brightness", "scanline_multisample",
  };
  static const char *const output_edge_blur[] = {
    "edge_blur_enable", "edge_blur_strength", "edge_blur_radius",
  };
  static const char *const output_mask[] = {
    "phosphor_mask_enable", "phosphor_mask_pattern",
    "phosphor_mask_brightness",
  };
  static const char *const output_vignette[] = {
    "vignette_enable", "vignette_strength",
    "vignette_scale", "vignette_softness",
  };
  static const char *const output_illumination[] = {
    "uneven_illumination_enable", "uneven_illumination_strength",
    "uneven_illumination_scale",
  };
  static const char *const output_reflection[] = {
    "glass_reflection_enable", "glass_reflection_width",
    "glass_reflection_position",
  };
  static const char *const output_glass_angle[] = {
    "glass_reflection_angle",
  };
  static const char *const output_frame_glass[] = {
    "glass_reflection_cosine", "glass_reflection_sine",
  };
  static const char *const output_rounded_mask[] = {
    "rounded_screen_mask_enable", "rounded_corner_radius",
    "rounded_border_softness",
  };
  static const char *const output_edge_glow[] = {
    "edge_glow_enable", "edge_glow_strength", "edge_glow_width",
  };
  static const char *const output_edge_glow_frame[] = {
    "edge_glow_top_r", "edge_glow_top_g", "edge_glow_top_b",
    "edge_glow_bottom_r", "edge_glow_bottom_g", "edge_glow_bottom_b",
    "edge_glow_left_r", "edge_glow_left_g", "edge_glow_left_b",
    "edge_glow_right_r", "edge_glow_right_g", "edge_glow_right_b",
  };
  static const char *const output_response[] = {
    "output_response_enable", "saturation", "black_level", "white_clip",
  };
  static const char *const output_response_generic[] = {
    "output_response_fast", "input_gamma",
    "inverse_output_gamma", "level_mapping",
  };
  static const char *const output_response_precomputed[] = {
    "precomputed_black_point", "precomputed_level_scale",
    "precomputed_edge_blur_offset_scale",
    "precomputed_edge_blur_inner_radius", "precomputed_scanline_gap",
    "precomputed_vignette_outer", "precomputed_uneven_sx",
    "precomputed_uneven_sy", "precomputed_uneven_sx_diagonal",
    "precomputed_uneven_sy_diagonal", "precomputed_glass_position",
    "precomputed_glass_outer_width", "precomputed_rounded_radius",
    "precomputed_rounded_box", "precomputed_rounded_softness",
  };
  static const char *const bloom_horizontal[] = {"source_texel_x"};
  static const char *const bloom_vertical[] = {"source_texel_y"};
  static const char *const bloom_composite[] = {
    "bloom_factor", "rounded_screen_mask_enable",
    "rounded_corner_radius", "rounded_border_softness",
  };
  static const char *const probe_core[] = {
    "source_texel_x", "source_texel_y", "geometry_enable",
    "curvature_x", "curvature_y", "skew_x", "skew_y", "trapezoid",
    "rotation_radians", "overscan_scale", "horizontal_filter_enable",
    "horizontal_sigma_x", "scanline_multisample",
  };
  static const char *const probe_convergence[] = {
    "convergence_enable", "red_offset_x", "red_offset_y",
    "blue_offset_x", "blue_offset_y", "convergence_radial_strength",
  };
  static const char *const probe_edge_blur[] = {
    "edge_blur_enable", "edge_blur_strength", "edge_blur_radius",
  };
  static const char *const probe_edge_glow[] = {
    "edge_glow_enable", "edge_glow_strength", "edge_glow_width",
  };
  static const char *const probe_surface_response[] = {
    "output_response_enable", "output_response_fast", "input_gamma",
    "inverse_output_gamma", "saturation", "black_level", "white_clip",
    "glass_reflection_enable", "glass_reflection_angle",
    "glass_reflection_width", "glass_reflection_position",
    "rounded_screen_mask_enable", "rounded_corner_radius",
    "rounded_border_softness",
  };
  static const char *const probe_mask_vignette[] = {
    "phosphor_mask_enable", "phosphor_mask_pattern",
    "phosphor_mask_brightness", "vignette_enable", "vignette_strength",
    "vignette_scale", "vignette_softness",
  };
  static const char *const probe_illumination_jitter[] = {
    "uneven_illumination_enable", "uneven_illumination_strength",
    "uneven_illumination_scale", "horizontal_jitter_enable",
    "horizontal_jitter_strength", "horizontal_jitter_frequency",
  };
  static const char *const probe_composite[] = {
    "composite_artifacts_enable", "composite_chroma_blur",
    "composite_luma_sharpen", "composite_color_bleed",
  };
  static const char *const probe_noise[] = {
    "noise_enable", "luminance_noise", "chroma_noise",
    "horizontal_jitter_speed", "noise_speed", "temporal_frame",
    "level_mapping",
  };

#define SEMANTIC_CONTRACT(group, values, failure, optional_mask) \
  {group, values, failure, \
   static_cast<uint32_t>(sizeof values / sizeof values[0]), optional_mask}
  static const SemanticContract contracts[] = {
    SEMANTIC_CONTRACT(kContractReplay, replay,
                      "pi5-fragment-uniform-contract", 0U),
    SEMANTIC_CONTRACT(kContractSourceFilter, source_filter,
                      "pi5-source-filter-uniform-contract", 0U),
    SEMANTIC_CONTRACT(kContractSourceConvergence, source_convergence,
                      "pi5-source-convergence-uniform-contract", 0U),
    SEMANTIC_CONTRACT(kContractSourceComposite, source_composite,
                      "pi5-source-composite-uniform-contract", 0U),
    SEMANTIC_CONTRACT(kContractSourceJitter, source_jitter,
                      "pi5-source-horizontal-jitter-uniform-contract", 0U),
    SEMANTIC_CONTRACT(kContractSourceNoise, source_noise,
                      "pi5-source-noise-uniform-contract", 0U),
    SEMANTIC_CONTRACT(kContractOutputGeometry, output_geometry,
                      "pi5-output-geometry-uniform-contract", 0U),
    SEMANTIC_CONTRACT(kContractOutputRotation, output_rotation,
                      "pi5-output-rotation-uniform-contract", 0U),
    SEMANTIC_CONTRACT(kContractOutputFrameRotation, output_frame_rotation,
                      "pi5-output-frame-rotation-uniform-contract", 0U),
    SEMANTIC_CONTRACT(kContractOutputScanlines, output_scanlines,
                      "pi5-output-scanline-uniform-contract", 0x3U),
    SEMANTIC_CONTRACT(kContractOutputEdgeBlur, output_edge_blur,
                      "pi5-output-edge-blur-uniform-contract", 0U),
    SEMANTIC_CONTRACT(kContractOutputMask, output_mask,
                      "pi5-output-phosphor-mask-uniform-contract", 0U),
    SEMANTIC_CONTRACT(kContractOutputVignette, output_vignette,
                      "pi5-output-vignette-uniform-contract", 0x8U),
    SEMANTIC_CONTRACT(kContractOutputIllumination, output_illumination,
                      "pi5-output-uneven-illumination-uniform-contract", 0x4U),
    SEMANTIC_CONTRACT(kContractOutputReflection, output_reflection,
                      "pi5-output-glass-reflection-uniform-contract", 0x4U),
    SEMANTIC_CONTRACT(kContractOutputGlassAngle, output_glass_angle,
                      "pi5-output-glass-angle-uniform-contract", 0U),
    SEMANTIC_CONTRACT(kContractOutputFrameGlass, output_frame_glass,
                      "pi5-output-frame-glass-uniform-contract", 0U),
    SEMANTIC_CONTRACT(kContractOutputRoundedMask, output_rounded_mask,
                      "pi5-output-rounded-screen-mask-uniform-contract", 0x6U),
    SEMANTIC_CONTRACT(kContractOutputEdgeGlow, output_edge_glow,
                      "pi5-output-edge-glow-uniform-contract", 0U),
    SEMANTIC_CONTRACT(kContractOutputEdgeGlow, output_edge_glow_frame,
                      "pi5-output-edge-glow-frame-uniform-contract", 0U),
    SEMANTIC_CONTRACT(kContractOutputResponse, output_response,
                      "pi5-output-response-uniform-contract", 0xcU),
    SEMANTIC_CONTRACT(kContractOutputResponseGeneric,
                      output_response_generic,
                      "pi5-output-response-uniform-contract", 0U),
    SEMANTIC_CONTRACT(kContractOutputResponsePrecomputed,
                      output_response_precomputed,
                      "pi5-output-response-precomputed-uniform-contract", 0U),
    SEMANTIC_CONTRACT(kContractBloomHorizontal, bloom_horizontal,
                      "pi5-bloom-horizontal-contract", 0U),
    SEMANTIC_CONTRACT(kContractBloomVertical, bloom_vertical,
                      "pi5-bloom-vertical-contract", 0U),
    SEMANTIC_CONTRACT(kContractBloomComposite, bloom_composite,
                      "pi5-bloom-composite-uniform-contract", 0U),
    SEMANTIC_CONTRACT(kContractProbeCore, probe_core,
                      "pi5-core-probe-uniform-contract", 0U),
    SEMANTIC_CONTRACT(kContractProbeConvergence, probe_convergence,
                      "pi5-convergence-probe-uniform-contract", 0U),
    SEMANTIC_CONTRACT(kContractProbeEdgeBlur, probe_edge_blur,
                      "pi5-edge-blur-probe-uniform-contract", 0U),
    SEMANTIC_CONTRACT(kContractProbeEdgeGlow, probe_edge_glow,
                      "pi5-edge-glow-probe-uniform-contract", 0U),
    SEMANTIC_CONTRACT(kContractProbeSurfaceResponse,
                      probe_surface_response,
                      "pi5-surface-response-probe-uniform-contract", 0U),
    SEMANTIC_CONTRACT(kContractProbeMaskVignette, probe_mask_vignette,
                      "pi5-mask-vignette-probe-uniform-contract", 0U),
    SEMANTIC_CONTRACT(kContractProbeIlluminationJitter,
                      probe_illumination_jitter,
                      "pi5-illumination-jitter-uniform-contract", 0U),
    SEMANTIC_CONTRACT(kContractProbeComposite, probe_composite,
                      "pi5-composite-probe-uniform-contract", 0U),
    SEMANTIC_CONTRACT(kContractProbeNoise, probe_noise,
                      "pi5-noise-probe-uniform-contract", 0U),
  };
#undef SEMANTIC_CONTRACT

  const bool fast_cubic =
      (descriptor.flags & kContractFlagFastCubic) != 0;
  for (uint32_t contract_index = 0;
       contract_index < sizeof contracts / sizeof contracts[0];
       ++contract_index) {
    const SemanticContract &contract = contracts[contract_index];
    if ((descriptor.contract_groups & contract.group) == 0) {
      continue;
    }
    for (uint32_t semantic_index = 0;
         semantic_index < contract.count; ++semantic_index) {
      if (fast_cubic &&
          (contract.fast_cubic_optional_mask &
           (1U << semantic_index)) != 0) {
        continue;
      }
      if (!HasFragmentUniform(fragment, "QUNIFORM_UNIFORM",
                              contract.semantics[semantic_index])) {
        return Fail(contract.reason, reason);
      }
    }
  }

  if (fast_cubic) {
    for (uint32_t i = 0;
         i < sizeof output_response_generic /
                 sizeof output_response_generic[0]; ++i) {
      if (HasFragmentUniform(fragment, "QUNIFORM_UNIFORM",
                             output_response_generic[i])) {
        return Fail(
            "pi5-output-response-fast-cubic-specialization-contract",
            reason);
      }
    }
  }
  return true;
}

bool ValidatePi5FragmentContract(const ShaderPackage &package,
                                 FragmentShaderPackageKind kind,
                                 const char **reason) {
  const FragmentPackageDescriptor &descriptor = DescriptorForKind(kind);
  if (!StringEquals(package.id, descriptor.expected_id)) {
    return Fail("pi5-shader-id-mismatch", reason);
  }

  const ShaderStageProgram *coordinate = FindShaderStage(
      package, v3dcrt::shaders::kShaderStageCoordinate);
  const ShaderStageProgram *vertex = FindShaderStage(
      package, v3dcrt::shaders::kShaderStageVertex);
  const ShaderStageProgram *fragment = FindShaderStage(
      package, v3dcrt::shaders::kShaderStageFragment);
  if (coordinate == nullptr || vertex == nullptr || fragment == nullptr) {
    return Fail("pi5-stage-missing", reason);
  }
  if (coordinate->requirements.threads != 4U ||
      !coordinate->requirements.single_segment ||
      coordinate->requirements.vpm_input_size != 0U ||
      coordinate->requirements.vpm_output_size != 1U ||
      coordinate->requirements.tmu_count != 0U ||
      vertex->requirements.threads != 4U ||
      !vertex->requirements.single_segment ||
      vertex->requirements.vpm_input_size != 0U ||
      vertex->requirements.vpm_output_size != 1U ||
      vertex->requirements.tmu_count != 0U ||
      fragment->requirements.threads != 4U ||
      fragment->requirements.single_segment ||
      fragment->requirements.varying_count != 2U ||
      !ValidateFixedStageUniforms(
          *coordinate, v3dcrt::shaders::kShaderStageCoordinate, reason) ||
      !ValidateFixedStageUniforms(
          *vertex, v3dcrt::shaders::kShaderStageVertex, reason)) {
    return Fail("pi5-stage-requirements-mismatch", reason);
  }
  if (fragment->requirements.tmu_count != descriptor.expected_tmus ||
      fragment->requirements.spill_size != 0U) {
    return Fail(descriptor.resource_reason, reason);
  }

  const uint32_t texture_p0_count = CountFragmentUniforms(
      *fragment, "QUNIFORM_TMU_CONFIG_P0", "source_texture");
  const uint32_t texture_p1_count = CountFragmentUniforms(
      *fragment, "QUNIFORM_TMU_CONFIG_P1", "source_texture");
  const uint32_t bloom_p0_count = CountFragmentUniforms(
      *fragment, "QUNIFORM_TMU_CONFIG_P0", "bloom_texture");
  const uint32_t bloom_p1_count = CountFragmentUniforms(
      *fragment, "QUNIFORM_TMU_CONFIG_P1", "bloom_texture");
  const bool bloom_composite =
      (descriptor.flags & kContractFlagBloomComposite) != 0;
  if (texture_p0_count != texture_p1_count ||
      bloom_p0_count != bloom_p1_count ||
      texture_p0_count + bloom_p0_count != descriptor.expected_tmus ||
      (bloom_composite ?
           (texture_p0_count != 1U || bloom_p0_count != 1U) :
           (texture_p0_count != descriptor.expected_tmus ||
            bloom_p0_count != 0U))) {
    return Fail("pi5-fragment-texture-p1-contract", reason);
  }
  if (!ValidateSemanticContracts(*fragment, descriptor, reason)) {
    return false;
  }
  return true;
}
uint32_t InitialUniformWord(const ShaderUniformSpec &uniform) {
  if (StringEquals(uniform.kind, "QUNIFORM_CONSTANT")) {
    return uniform.data;
  }
  if (!StringEquals(uniform.kind, "QUNIFORM_UNIFORM")) {
    return 0U;
  }
  if (StringEquals(uniform.semantic, "fragcoord_y_scale")) {
    return FloatBits(-1.0f);
  }
  if (StringEquals(uniform.semantic, "fragcoord_y_bias")) {
    return FloatBits(16.0f);
  }
  if (StringEquals(uniform.semantic, "scanline_gap_brightness")) {
    return FloatBits(0.35f);
  }
  if (StringEquals(uniform.semantic, "scanline_weight")) {
    return FloatBits(0.75f);
  }
  if (StringEquals(uniform.semantic, "output_response_enable") ||
      StringEquals(uniform.semantic, "output_response_fast") ||
      StringEquals(uniform.semantic, "level_mapping") ||
      StringEquals(uniform.semantic, "saturation") ||
      StringEquals(uniform.semantic, "white_clip") ||
      StringEquals(uniform.semantic, "phosphor_mask_enable")) {
    return FloatBits(1.0f);
  }
  if (StringEquals(uniform.semantic, "input_gamma")) {
    return FloatBits(2.4f);
  }
  if (StringEquals(uniform.semantic, "inverse_output_gamma")) {
    return FloatBits(1.0f / 2.2f);
  }
  if (StringEquals(uniform.semantic, "phosphor_mask_pattern")) {
    return FloatBits(2.0f);
  }
  if (StringEquals(uniform.semantic, "phosphor_mask_brightness")) {
    return FloatBits(0.7f);
  }
  return 0U;
}

bool FindStageArrayIndex(const PreparedFragmentShaderPackage &prepared,
                         ShaderArtifactStage stage,
                         uint32_t *index) {
  if (index == nullptr) {
    return false;
  }
  for (uint32_t i = 0; i < kStageCount; ++i) {
    if (prepared.stage_codes[i].stage == stage) {
      *index = i;
      return true;
    }
  }
  return false;
}

bool FindFragmentUniformIndexByKind(
    const PreparedFragmentShaderPackage &prepared,
    const char *kind,
    uint32_t *uniform_index) {
  if (prepared.package == nullptr || uniform_index == nullptr) {
    return false;
  }
  const ShaderStageProgram *fragment = FindShaderStage(
      *prepared.package, v3dcrt::shaders::kShaderStageFragment);
  if (fragment == nullptr) {
    return false;
  }
  for (uint32_t i = 0; i < fragment->uniform_count; ++i) {
    if (StringEquals(fragment->uniforms[i].kind, kind) &&
        StringEquals(fragment->uniforms[i].semantic, "source_texture")) {
      *uniform_index = i;
      return true;
    }
  }
  return false;
}

const char *ArtifactNameForKind(FragmentShaderPackageKind kind) {
  return DescriptorForKind(kind).artifact_name;
}

bool BuildRuntimeArtifact(PreparedFragmentShaderPackage *prepared,
                          const ShaderPackage &package,
                          const char **reason) {
  if (prepared == nullptr) {
    return Fail("pi5-prepared-package-missing", reason);
  }
  const ShaderArtifact &fixture = generated::kMesaScanlineProbeArtifact;
  if (fixture.stage_count != kStageCount ||
      fixture.uniform_count != kStageCount ||
      fixture.patch_point_count != kPatchPointCount) {
    return Fail("pi5-replay-fixture-shape", reason);
  }

  uint32_t next_code = kCodeLayoutBase;
  uint32_t next_uniform = kUniformLayoutBase;
  for (uint32_t i = 0; i < kStageCount; ++i) {
    const ShaderArtifactStageCode &fixture_stage = fixture.stages[i];
    const ShaderStageProgram *program = FindShaderStage(
        package, PackageStageKind(fixture_stage.stage));
    const ShaderArtifactUniformBlock *fixture_uniform =
        FindFixtureUniform(fixture_stage.stage);
    if (program == nullptr || fixture_uniform == nullptr ||
        program->uniform_count > kMaxStageUniformWords) {
      return Fail("pi5-package-stage-unavailable", reason);
    }

    next_code = AlignUp(next_code, kCodeAlignment);
    const uint32_t code_bytes = program->qpu_word_count * sizeof(uint64_t);
    if (next_code > kShaderCodeSliceBytes ||
        code_bytes > kShaderCodeSliceBytes - next_code) {
      return Fail("pi5-dynamic-code-layout-overflow", reason);
    }
    prepared->stage_codes[i] = fixture_stage;
    prepared->stage_codes[i].code_offset = next_code;
    prepared->stage_codes[i].qpu_words = program->qpu_words;
    prepared->stage_codes[i].qpu_word_count = program->qpu_word_count;
    next_code += code_bytes;

    next_uniform = AlignUp(next_uniform, kUniformAlignment);
    const uint32_t uniform_bytes =
        program->uniform_count * sizeof(uint32_t);
    if (next_uniform > kShaderUniformSliceBytes ||
        uniform_bytes > kShaderUniformSliceBytes - next_uniform) {
      return Fail("pi5-dynamic-uniform-layout-overflow", reason);
    }
    memset(prepared->uniform_words[i], 0,
           sizeof prepared->uniform_words[i]);
    for (uint32_t word = 0; word < program->uniform_count; ++word) {
      prepared->uniform_words[i][word] =
          InitialUniformWord(program->uniforms[word]);
    }
    prepared->uniform_blocks[i] = *fixture_uniform;
    prepared->uniform_blocks[i].offset = next_uniform;
    prepared->uniform_blocks[i].words = prepared->uniform_words[i];
    prepared->uniform_blocks[i].word_count = program->uniform_count;
    next_uniform += uniform_bytes;
  }

  memset(&prepared->layout, 0, sizeof prepared->layout);
  uint32_t fragment_index = 0;
  uint32_t vertex_index = 0;
  uint32_t coordinate_index = 0;
  if (!FindStageArrayIndex(
          *prepared, kArtifactStageFragment, &fragment_index) ||
      !FindStageArrayIndex(*prepared, kArtifactStageVertex, &vertex_index) ||
      !FindStageArrayIndex(
          *prepared, kArtifactStageCoordinate, &coordinate_index)) {
    return Fail("pi5-dynamic-stage-layout-missing", reason);
  }
  prepared->layout.fragment_code_offset =
      prepared->stage_codes[fragment_index].code_offset;
  prepared->layout.vertex_code_offset =
      prepared->stage_codes[vertex_index].code_offset;
  prepared->layout.coordinate_code_offset =
      prepared->stage_codes[coordinate_index].code_offset;
  prepared->layout.code_bytes = AlignUp(next_code, kCodeAlignment);
  prepared->layout.fragment_uniform_offset =
      prepared->uniform_blocks[fragment_index].offset;
  prepared->layout.vertex_uniform_offset =
      prepared->uniform_blocks[vertex_index].offset;
  prepared->layout.coordinate_uniform_offset =
      prepared->uniform_blocks[coordinate_index].offset;
  prepared->layout.uniform_bytes = AlignUp(next_uniform, kUniformAlignment);

  memcpy(prepared->patch_points, fixture.patch_points,
         sizeof fixture.patch_points[0] * fixture.patch_point_count);
  uint32_t texture_p0_index = 0;
  uint32_t texture_p1_index = 0;
  if (!FindFragmentUniformIndexByKind(*prepared, "QUNIFORM_TMU_CONFIG_P0",
                                      &texture_p0_index) ||
      !FindFragmentUniformIndexByKind(*prepared, "QUNIFORM_TMU_CONFIG_P1",
                                      &texture_p1_index)) {
    return Fail("pi5-dynamic-texture-uniform-missing", reason);
  }
  for (uint32_t i = 0; i < kPatchPointCount; ++i) {
    ShaderArtifactPatchPoint &patch = prepared->patch_points[i];
    if (patch.kind == kArtifactPatchShaderCode) {
      uint32_t stage_index = 0;
      if (!FindStageArrayIndex(*prepared, patch.stage, &stage_index)) {
        return Fail("pi5-dynamic-code-patch-stage", reason);
      }
      patch.offset = prepared->stage_codes[stage_index].code_offset;
    } else if (patch.kind == kArtifactPatchShaderUniform) {
      uint32_t stage_index = 0;
      if (!FindStageArrayIndex(*prepared, patch.stage, &stage_index)) {
        return Fail("pi5-dynamic-uniform-patch-stage", reason);
      }
      patch.offset = prepared->uniform_blocks[stage_index].offset;
    } else if (patch.kind == kArtifactPatchUniformWordAddressCandidate &&
               patch.stage == kArtifactStageFragment) {
      if (StringEquals(patch.buffer_kind, "sampler")) {
        patch.word_index = texture_p0_index;
      } else if (StringEquals(patch.buffer_kind, "resource")) {
        patch.word_index = texture_p1_index;
      }
    }
  }

  prepared->artifact = fixture;
  prepared->artifact.name = ArtifactNameForKind(prepared->kind);
  prepared->artifact.provenance = package.provenance;
  prepared->artifact.stages = prepared->stage_codes;
  prepared->artifact.uniforms = prepared->uniform_blocks;
  prepared->artifact.patch_points = prepared->patch_points;
  prepared->artifact.comparison_compatible = false;
  return true;
}

}  // namespace

void ResetPreparedFragmentShaderPackage(
    PreparedFragmentShaderPackage *prepared) {
  if (prepared == nullptr) {
    return;
  }
  memset(prepared, 0, sizeof *prepared);
  prepared->kind = kFragmentShaderPackageScanlineProbe;
}

bool PrepareFragmentShaderPackage(PreparedFragmentShaderPackage *prepared,
                                  FragmentShaderPackageKind kind,
                                  const char **reason) {
  if (reason != nullptr) {
    *reason = nullptr;
  }
  if (prepared == nullptr) {
    return Fail("pi5-prepared-package-missing", reason);
  }
  if (prepared->prepared && prepared->kind == kind) {
    if (reason != nullptr) {
      *reason = "ok";
    }
    return true;
  }

  ResetPreparedFragmentShaderPackage(prepared);
  prepared->kind = kind;
  prepared->package = PackageForKind(kind);
  const ShaderBackendCapabilities capabilities = {
    "bcm2712-v3d71",
    71U,
    10U,
    4U,
    kMaxTextureSamples,
    16U,
    false,
  };
  if (prepared->package == nullptr ||
      !ValidateShaderPackage(*prepared->package, capabilities, reason) ||
      !ValidatePi5FragmentContract(*prepared->package, kind, reason) ||
      !BuildRuntimeArtifact(prepared, *prepared->package, reason) ||
      !BuildPreparedFragmentSemanticIndex(prepared, reason)) {
    prepared->prepared = false;
    prepared->package = nullptr;
    return false;
  }

  prepared->prepared = true;
  if (reason != nullptr) {
    *reason = "ok";
  }
  return true;
}

const ShaderArtifact *GetPreparedFragmentShaderArtifact(
    const PreparedFragmentShaderPackage *prepared) {
  return prepared != nullptr && prepared->prepared ?
      &prepared->artifact : nullptr;
}

const ShaderPackage *GetPreparedFragmentShaderPackage(
    const PreparedFragmentShaderPackage *prepared) {
  return prepared != nullptr && prepared->prepared ?
      prepared->package : nullptr;
}

FragmentShaderPackageKind GetPreparedFragmentShaderPackageKind(
    const PreparedFragmentShaderPackage *prepared) {
  return prepared != nullptr ? prepared->kind :
                               kFragmentShaderPackageScanlineProbe;
}

bool GetPreparedFragmentShaderRuntimeLayout(
    const PreparedFragmentShaderPackage *prepared,
    FragmentShaderRuntimeLayout *layout) {
  if (prepared == nullptr || !prepared->prepared || layout == nullptr) {
    return false;
  }
  *layout = prepared->layout;
  return true;
}

bool FindPreparedFragmentUniformIndex(
    const PreparedFragmentShaderPackage *prepared,
    const char *semantic,
    uint32_t *uniform_index) {
  if (prepared == nullptr || !prepared->prepared ||
      prepared->package == nullptr || semantic == nullptr ||
      uniform_index == nullptr) {
    return false;
  }

  uint32_t slot = 0;
  if (!FindPreparedSemanticSlot(*prepared, semantic, &slot)) {
    return false;
  }
  uint16_t index = prepared->fragment_semantic_heads[slot];
  if (index == kInvalidFragmentUniformIndex ||
      index >= prepared->fragment_uniform_count) {
    return false;
  }
  while (prepared->fragment_semantic_next[index] !=
         kInvalidFragmentUniformIndex) {
    index = prepared->fragment_semantic_next[index];
    if (index >= prepared->fragment_uniform_count) {
      return false;
    }
  }
  *uniform_index = index;
  return true;
}

bool PatchPreparedFragmentTextureUniformsForSemantic(
    const PreparedFragmentShaderPackage *prepared,
    uint8_t *fragment_uniforms,
    uint32_t uniform_word_count,
    const char *semantic,
    uint32_t p0_base,
    uint32_t p1_base) {
  if (prepared == nullptr || !prepared->prepared ||
      prepared->package == nullptr || fragment_uniforms == nullptr ||
      semantic == nullptr || p0_base == 0U || p1_base == 0U) {
    return false;
  }
  const ShaderStageProgram *fragment = FindShaderStage(
      *prepared->package, v3dcrt::shaders::kShaderStageFragment);
  if (fragment == nullptr || fragment->uniforms == nullptr ||
      fragment->uniform_count > uniform_word_count) {
    return false;
  }

  uint32_t slot = 0;
  if (!FindPreparedSemanticSlot(*prepared, semantic, &slot)) {
    return false;
  }
  uint32_t p0_matches = 0U;
  uint32_t p1_matches = 0U;
  uint16_t index = prepared->fragment_semantic_heads[slot];
  while (index != kInvalidFragmentUniformIndex) {
    if (index >= fragment->uniform_count ||
        index >= prepared->fragment_uniform_count) {
      return false;
    }
    const ShaderUniformSpec &uniform = fragment->uniforms[index];
    uint32_t base = 0;
    if (StringEquals(uniform.kind, "QUNIFORM_TMU_CONFIG_P0") &&
        StringEquals(uniform.semantic, semantic)) {
      base = p0_base;
      ++p0_matches;
    } else if (StringEquals(uniform.kind, "QUNIFORM_TMU_CONFIG_P1") &&
               StringEquals(uniform.semantic, semantic)) {
      base = p1_base;
      ++p1_matches;
    } else {
      index = prepared->fragment_semantic_next[index];
      continue;
    }
    const uint32_t state_offset = uniform.data & 0x00FFFFFFU;
    const uint32_t value = base + state_offset;
    if (value < base) {
      return false;
    }
    memcpy(fragment_uniforms + index * sizeof(uint32_t), &value,
           sizeof value);
    index = prepared->fragment_semantic_next[index];
  }
  return p0_matches != 0U && p0_matches == p1_matches;
}

bool PatchPreparedFragmentTextureUniforms(
    const PreparedFragmentShaderPackage *prepared,
    uint8_t *fragment_uniforms,
    uint32_t uniform_word_count) {
  if (prepared == nullptr || !prepared->prepared ||
      prepared->package == nullptr || fragment_uniforms == nullptr) {
    return false;
  }
  const ShaderStageProgram *fragment = FindShaderStage(
      *prepared->package, v3dcrt::shaders::kShaderStageFragment);
  const uint32_t p0_index = prepared->first_tmu_p0_index;
  const uint32_t p1_index = prepared->first_tmu_p1_index;
  if (fragment == nullptr || fragment->uniforms == nullptr ||
      fragment->uniform_count > uniform_word_count ||
      p0_index == kInvalidFragmentUniformIndex ||
      p1_index == kInvalidFragmentUniformIndex ||
      p0_index >= uniform_word_count || p1_index >= uniform_word_count) {
    return false;
  }

  uint32_t first_p0 = 0;
  uint32_t first_p1 = 0;
  memcpy(&first_p0, fragment_uniforms + p0_index * sizeof(uint32_t),
         sizeof first_p0);
  memcpy(&first_p1, fragment_uniforms + p1_index * sizeof(uint32_t),
         sizeof first_p1);
  const uint32_t first_p0_data = fragment->uniforms[p0_index].data;
  const uint32_t first_p1_data = fragment->uniforms[p1_index].data;
  if (first_p0 == 0U || first_p1 == 0U || first_p0 < first_p0_data ||
      first_p1 < first_p1_data) {
    return false;
  }
  const uint32_t p0_base = first_p0 - first_p0_data;
  const uint32_t p1_base = first_p1 - first_p1_data;
  return PatchPreparedFragmentTextureUniformsForSemantic(
      prepared, fragment_uniforms, uniform_word_count, "source_texture",
      p0_base, p1_base);
}

bool PatchFragmentUniformStream(
    const v3dcrt::shaders::ShaderUniformSpec *uniforms,
    uint32_t stream_uniform_count,
    uint8_t *fragment_uniforms,
    uint32_t uniform_word_count,
    const char *semantic,
    uint32_t value) {
  if (uniforms == nullptr || fragment_uniforms == nullptr ||
      semantic == nullptr || stream_uniform_count > uniform_word_count) {
    return false;
  }

  uint32_t matches = 0;
  for (uint32_t i = 0; i < stream_uniform_count; ++i) {
    const v3dcrt::shaders::ShaderUniformSpec &uniform = uniforms[i];
    if (StringEquals(uniform.kind, "QUNIFORM_UNIFORM") &&
        StringEquals(uniform.semantic, semantic)) {
      memcpy(fragment_uniforms + i * sizeof(uint32_t), &value, sizeof value);
      ++matches;
    }
  }
  return matches != 0U;
}

bool PatchPreparedFragmentUniform(
    const PreparedFragmentShaderPackage *prepared,
    uint8_t *fragment_uniforms,
    uint32_t uniform_word_count,
    const char *semantic,
    uint32_t value) {
  if (prepared == nullptr || !prepared->prepared ||
      prepared->package == nullptr) {
    return false;
  }
  const ShaderStageProgram *fragment = FindShaderStage(
      *prepared->package, v3dcrt::shaders::kShaderStageFragment);
  uint32_t slot = 0;
  if (fragment == nullptr || fragment->uniforms == nullptr ||
      fragment->uniform_count > uniform_word_count ||
      !FindPreparedSemanticSlot(*prepared, semantic, &slot)) {
    return false;
  }
  uint32_t matches = 0U;
  uint16_t index = prepared->fragment_semantic_heads[slot];
  while (index != kInvalidFragmentUniformIndex) {
    if (index >= fragment->uniform_count ||
        index >= prepared->fragment_uniform_count) {
      return false;
    }
    const ShaderUniformSpec &uniform = fragment->uniforms[index];
    if (StringEquals(uniform.kind, "QUNIFORM_UNIFORM")) {
      memcpy(fragment_uniforms + index * sizeof(uint32_t),
             &value, sizeof value);
      ++matches;
    }
    index = prepared->fragment_semantic_next[index];
  }
  return matches != 0U;
}

bool BuildCrtFragmentUniformValues(
    uint32_t source_width,
    uint32_t source_height,
    uint32_t output_height,
    const RenderParams &params,
    CrtFragmentUniformValues *values) {
  return v3dcrt::BuildEffectUniformValues(
      source_width, source_height, output_height, params, values);
}

}  // namespace shader_artifacts
}  // namespace pi5v3d
