#ifndef PI5V3D_SHADERS_PI5_SHADER_PACKAGE_ADAPTER_H
#define PI5V3D_SHADERS_PI5_SHADER_PACKAGE_ADAPTER_H

#include "pi5v3d/pi5_render_params.h"
#include "pi5v3d/shaders/shader_artifact.h"
#include "v3dcrt/semantic_values.h"
#include "v3dcrt/shaders/shader_package.h"

#include <stdint.h>

namespace pi5v3d {

namespace shader_artifacts {

enum FragmentShaderPackageKind {
  kFragmentShaderPackageScanlineProbe = 0,
  kFragmentShaderPackageCrtSourceFilter,
  kFragmentShaderPackageCrtSourceConvergence,
  kFragmentShaderPackageCrtSourceComposite,
  kFragmentShaderPackageCrtSourceHorizontalJitter,
  kFragmentShaderPackageCrtSourceNoise,
  kFragmentShaderPackageCrtOutputGeometry,
  kFragmentShaderPackageCrtOutputScanlines,
  kFragmentShaderPackageCrtOutputEdgeBlur,
  kFragmentShaderPackageCrtOutputPhosphorMask,
  kFragmentShaderPackageCrtOutputVignette,
  kFragmentShaderPackageCrtOutputUnevenIllumination,
  kFragmentShaderPackageCrtOutputGlassReflection,
  kFragmentShaderPackageCrtOutputRoundedScreenMask,
  kFragmentShaderPackageCrtOutputEdgeGlow,
  kFragmentShaderPackageCrtOutputResponse,
  kFragmentShaderPackageCrtOutputResponseFastCubic,
  kFragmentShaderPackageCrtBloomBlurHorizontal,
  kFragmentShaderPackageCrtBloomBlurVertical,
  kFragmentShaderPackageCrtBloomComposite,
  kFragmentShaderPackageCrtCoreProbe,
  kFragmentShaderPackageCrtConvergenceProbe,
  kFragmentShaderPackageCrtEdgeBlurProbe,
  kFragmentShaderPackageCrtEdgeGlowProbe,
  kFragmentShaderPackageCrtSurfaceResponseProbe,
  kFragmentShaderPackageCrtMaskVignetteProbe,
  kFragmentShaderPackageCrtIlluminationJitterProbe,
  kFragmentShaderPackageCrtCompositeProbe,
  kFragmentShaderPackageCrtNoiseProbe,
  kFragmentShaderPackageCrtSinglePass,
  kFragmentShaderPackageCount
};

struct FragmentShaderRuntimeLayout {
  uint32_t fragment_code_offset;
  uint32_t vertex_code_offset;
  uint32_t coordinate_code_offset;
  uint32_t code_bytes;
  uint32_t fragment_uniform_offset;
  uint32_t vertex_uniform_offset;
  uint32_t coordinate_uniform_offset;
  uint32_t uniform_bytes;
};

constexpr uint32_t kPreparedFragmentStageCount = 3U;
constexpr uint32_t kPreparedFragmentMaxStageUniformWords = 256U;
constexpr uint32_t kPreparedFragmentPatchPointCount = 13U;
constexpr uint32_t kPreparedFragmentSemanticSlotCount = 256U;

// Owns all metadata referenced by artifact. Do not copy a prepared instance.
struct PreparedFragmentShaderPackage {
  ShaderArtifactStageCode stage_codes[kPreparedFragmentStageCount];
  ShaderArtifactUniformBlock uniform_blocks[kPreparedFragmentStageCount];
  uint32_t uniform_words[kPreparedFragmentStageCount]
                        [kPreparedFragmentMaxStageUniformWords];
  ShaderArtifactPatchPoint patch_points[kPreparedFragmentPatchPointCount];
  ShaderArtifact artifact;
  FragmentShaderRuntimeLayout layout;
  const char *fragment_semantic_keys[kPreparedFragmentSemanticSlotCount];
  uint16_t fragment_semantic_heads[kPreparedFragmentSemanticSlotCount];
  uint16_t fragment_semantic_next[kPreparedFragmentMaxStageUniformWords];
  uint16_t fragment_uniform_count;
  uint16_t first_tmu_p0_index;
  uint16_t first_tmu_p1_index;
  const v3dcrt::shaders::ShaderPackage *package;
  FragmentShaderPackageKind kind;
  bool prepared;
};

using CrtFragmentUniformValues = v3dcrt::EffectUniformValues;

void ResetPreparedFragmentShaderPackage(
    PreparedFragmentShaderPackage *prepared);

bool PrepareFragmentShaderPackage(PreparedFragmentShaderPackage *prepared,
                                  FragmentShaderPackageKind kind,
                                  const char **reason);

const ShaderArtifact *GetPreparedFragmentShaderArtifact(
    const PreparedFragmentShaderPackage *prepared);

const v3dcrt::shaders::ShaderPackage *GetPreparedFragmentShaderPackage(
    const PreparedFragmentShaderPackage *prepared);

FragmentShaderPackageKind GetPreparedFragmentShaderPackageKind(
    const PreparedFragmentShaderPackage *prepared);

bool GetPreparedFragmentShaderRuntimeLayout(
    const PreparedFragmentShaderPackage *prepared,
    FragmentShaderRuntimeLayout *layout);

bool FindPreparedFragmentUniformIndex(
    const PreparedFragmentShaderPackage *prepared,
    const char *semantic,
    uint32_t *uniform_index);

bool PatchPreparedFragmentTextureUniforms(
    const PreparedFragmentShaderPackage *prepared,
    uint8_t *fragment_uniforms,
    uint32_t uniform_word_count);

bool PatchPreparedFragmentTextureUniformsForSemantic(
    const PreparedFragmentShaderPackage *prepared,
    uint8_t *fragment_uniforms,
    uint32_t uniform_word_count,
    const char *semantic,
    uint32_t p0_base,
    uint32_t p1_base);

bool PatchPreparedFragmentUniform(
    const PreparedFragmentShaderPackage *prepared,
    uint8_t *fragment_uniforms,
    uint32_t uniform_word_count,
    const char *semantic,
    uint32_t value);

bool PatchFragmentUniformStream(
    const v3dcrt::shaders::ShaderUniformSpec *uniforms,
    uint32_t stream_uniform_count,
    uint8_t *fragment_uniforms,
    uint32_t uniform_word_count,
    const char *semantic,
    uint32_t value);

bool BuildCrtFragmentUniformValues(
    uint32_t source_width,
    uint32_t source_height,
    uint32_t output_height,
    const RenderParams &params,
    CrtFragmentUniformValues *values);

}  // namespace shader_artifacts
}  // namespace pi5v3d

#endif  // PI5V3D_SHADERS_PI5_SHADER_PACKAGE_ADAPTER_H
