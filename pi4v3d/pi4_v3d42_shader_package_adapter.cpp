#include "pi4v3d/pi4_v3d42_shader_package_adapter.h"

#include "v3dcrt/semantic_values.h"

#include <stddef.h>
#include <string.h>

namespace pi4v3d {

namespace {

using v3dcrt::shaders::FindShaderStage;
using v3dcrt::shaders::ShaderBackendCapabilities;
using v3dcrt::shaders::ShaderPackage;
using v3dcrt::shaders::ShaderStageKind;
using v3dcrt::shaders::ShaderStageProgram;
using v3dcrt::shaders::ShaderUniformSpec;
using v3dcrt::shaders::ValidateShaderPackage;

const uint32_t kSliceAlignment = 32U;
const uint32_t kUniformGuardWords = 1U;
const float kMaxCrtCurvature = 100.0f / 600.0f;

bool StringEquals(const char *left, const char *right) {
  return left != NULL && right != NULL && strcmp(left, right) == 0;
}

bool Fail(const char *text, const char **reason) {
  if (reason != NULL) {
    *reason = text;
  }
  return false;
}

uint32_t FloatBits(float value) {
  uint32_t bits = 0U;
  memcpy(&bits, &value, sizeof bits);
  return bits;
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

void Store32(uint8_t *data, uint32_t value) {
  data[0] = static_cast<uint8_t>(value);
  data[1] = static_cast<uint8_t>(value >> 8);
  data[2] = static_cast<uint8_t>(value >> 16);
  data[3] = static_cast<uint8_t>(value >> 24);
}

bool AddWithin(uint32_t value, uint32_t addition, uint32_t limit,
               uint32_t *result) {
  if (result == NULL || value > limit || addition > limit - value) {
    return false;
  }
  *result = value + addition;
  return true;
}

bool AlignWithin(uint32_t value, uint32_t alignment, uint32_t limit,
                 uint32_t *result) {
  if (alignment == 0U || (alignment & (alignment - 1U)) != 0U ||
      value > UINT32_MAX - (alignment - 1U)) {
    return false;
  }
  const uint32_t aligned =
      (value + alignment - 1U) & ~(alignment - 1U);
  if (aligned > limit || result == NULL) {
    return false;
  }
  *result = aligned;
  return true;
}

bool StageBytes(uint32_t word_count, uint32_t word_size,
                uint32_t *bytes) {
  if (bytes == NULL || word_count > UINT32_MAX / word_size) {
    return false;
  }
  *bytes = word_count * word_size;
  return true;
}

bool UniformWordCount(const ShaderStageProgram &stage,
                      uint32_t *word_count) {
  if (word_count == NULL ||
      stage.uniform_count > UINT32_MAX - kUniformGuardWords) {
    return false;
  }
  *word_count = stage.uniform_count + kUniformGuardWords;
  return true;
}

uint32_t CodeAddressFlags(const ShaderStageProgram &stage) {
  // V3D 4.2 embeds the four-thread and single-segment selectors in the low
  // bits of the aligned code address. Package validation below deliberately
  // limits this adapter to the proven four-thread contract.
  return 5U | (stage.requirements.single_segment ? 2U : 0U);
}

bool IsFixedUniformKind(ShaderStageKind stage, const char *kind) {
  if (StringEquals(kind, "QUNIFORM_CONSTANT")) {
    return true;
  }
  if (stage == v3dcrt::shaders::kShaderStageCoordinate ||
      stage == v3dcrt::shaders::kShaderStageVertex) {
    return StringEquals(kind, "QUNIFORM_VIEWPORT_X_SCALE") ||
           StringEquals(kind, "QUNIFORM_VIEWPORT_Y_SCALE") ||
           StringEquals(kind, "QUNIFORM_VIEWPORT_Z_OFFSET");
  }
  return StringEquals(kind, "QUNIFORM_UNIFORM") ||
         StringEquals(kind, "QUNIFORM_TMU_CONFIG_P0") ||
         StringEquals(kind, "QUNIFORM_TMU_CONFIG_P1");
}

bool ValidateAdapterContract(const ShaderPackage &package,
                             const ShaderStageProgram **coordinate,
                             const ShaderStageProgram **vertex,
                             const ShaderStageProgram **fragment,
                             const char **reason) {
  const ShaderBackendCapabilities capabilities = {
    "bcm2711-v3d42", 42U, 14U, 4U, 5U, 255U, false
  };
  if (!ValidateShaderPackage(package, capabilities, reason)) {
    return false;
  }

  *coordinate = FindShaderStage(
      package, v3dcrt::shaders::kShaderStageCoordinate);
  *vertex = FindShaderStage(package, v3dcrt::shaders::kShaderStageVertex);
  *fragment = FindShaderStage(
      package, v3dcrt::shaders::kShaderStageFragment);
  if (*coordinate == NULL || *vertex == NULL || *fragment == NULL) {
    return Fail("pi4-stage-missing", reason);
  }

  const ShaderStageProgram &cs = **coordinate;
  const ShaderStageProgram &vs = **vertex;
  const ShaderStageProgram &fs = **fragment;
  if (cs.requirements.threads != 4U ||
      !cs.requirements.single_segment ||
      cs.requirements.spill_size != 0U ||
      cs.requirements.tmu_count != 0U ||
      cs.requirements.vpm_input_size != 0U ||
      cs.requirements.vpm_output_size == 0U ||
      cs.requirements.varying_count != 0U ||
      vs.requirements.threads != 4U ||
      !vs.requirements.single_segment ||
      vs.requirements.spill_size != 0U ||
      vs.requirements.tmu_count != 0U ||
      vs.requirements.vpm_input_size != 0U ||
      vs.requirements.vpm_output_size == 0U ||
      vs.requirements.varying_count != 0U ||
      fs.requirements.threads != 4U ||
      fs.requirements.single_segment ||
      fs.requirements.spill_size != 0U ||
      fs.requirements.tmu_count == 0U ||
      fs.requirements.tmu_count > 5U ||
      fs.requirements.vpm_input_size != 0U ||
      fs.requirements.vpm_output_size != 0U ||
      fs.requirements.varying_count != 2U) {
    return Fail("pi4-stage-requirements-mismatch", reason);
  }

  uint32_t texture_p0_count = 0U;
  uint32_t texture_p1_count = 0U;
  for (uint32_t stage_index = 0U; stage_index < package.stage_count;
       ++stage_index) {
    const ShaderStageProgram &stage = package.stages[stage_index];
    for (uint32_t uniform_index = 0U;
         uniform_index < stage.uniform_count; ++uniform_index) {
      const ShaderUniformSpec &uniform = stage.uniforms[uniform_index];
      if (!IsFixedUniformKind(stage.stage, uniform.kind)) {
        return Fail("pi4-uniform-kind-unsupported", reason);
      }
      if (stage.stage != v3dcrt::shaders::kShaderStageFragment) {
        continue;
      }
      if (StringEquals(uniform.kind, "QUNIFORM_TMU_CONFIG_P0") &&
          StringEquals(uniform.semantic, "source_texture")) {
        ++texture_p0_count;
      } else if (StringEquals(uniform.kind, "QUNIFORM_TMU_CONFIG_P1") &&
                 StringEquals(uniform.semantic, "source_texture")) {
        ++texture_p1_count;
      }
    }
  }
  if (texture_p0_count != fs.requirements.tmu_count ||
      texture_p1_count != fs.requirements.tmu_count) {
    return Fail("pi4-fragment-texture-contract", reason);
  }
  return true;
}

bool PlanStage(uint32_t cursor, uint32_t word_count, uint32_t word_size,
               uint32_t buffer_size, uint32_t *offset,
               uint32_t *next_cursor) {
  uint32_t bytes = 0U;
  return AlignWithin(cursor, kSliceAlignment, buffer_size, offset) &&
         StageBytes(word_count, word_size, &bytes) &&
         AddWithin(*offset, bytes, buffer_size, next_cursor);
}

bool SameLayout(const V3d42ShaderLayout &left,
                const V3d42ShaderLayout &right) {
  return left.coordinate_code_offset == right.coordinate_code_offset &&
         left.vertex_code_offset == right.vertex_code_offset &&
         left.fragment_code_offset == right.fragment_code_offset &&
         left.coordinate_uniform_offset == right.coordinate_uniform_offset &&
         left.vertex_uniform_offset == right.vertex_uniform_offset &&
         left.fragment_uniform_offset == right.fragment_uniform_offset &&
         left.end_offset == right.end_offset &&
         left.coordinate_code_flags == right.coordinate_code_flags &&
         left.vertex_code_flags == right.vertex_code_flags &&
         left.fragment_code_flags == right.fragment_code_flags &&
         left.coordinate_vpm_output_size ==
             right.coordinate_vpm_output_size &&
         left.vertex_vpm_output_size == right.vertex_vpm_output_size &&
         left.fragment_varying_count == right.fragment_varying_count;
}

bool BuildEffectUniformValues(
    const V3d42ShaderBindings &bindings,
    v3dcrt::EffectUniformValues *values) {
  v3dcrt::EffectParams params = {};
  params.enable_geometry = bindings.geometry_enabled;
  params.curvature_x = ClampFloat(
      bindings.curvature_x, 0.0f, kMaxCrtCurvature, 0.0f);
  params.curvature_y = ClampFloat(
      bindings.curvature_y, 0.0f, kMaxCrtCurvature, 0.0f);
  params.skew_x = ClampFloat(bindings.skew_x, -0.08f, 0.08f, 0.0f);
  params.skew_y = ClampFloat(bindings.skew_y, -0.08f, 0.08f, 0.0f);
  params.trapezoid =
      ClampFloat(bindings.trapezoid, -0.15f, 0.15f, 0.0f);
  params.rotation_degrees =
      ClampFloat(bindings.rotation_degrees, -3.0f, 3.0f, 0.0f);
  params.overscan_scale =
      ClampFloat(bindings.overscan_scale, 1.0f, 1.2f, 1.0f);

  params.enable_convergence = bindings.convergence_enabled;
  params.red_offset_x =
      ClampFloat(bindings.red_offset_x, -1.0f, 1.0f, 0.0f);
  params.red_offset_y =
      ClampFloat(bindings.red_offset_y, -1.0f, 1.0f, 0.0f);
  params.blue_offset_x =
      ClampFloat(bindings.blue_offset_x, -1.0f, 1.0f, 0.0f);
  params.blue_offset_y =
      ClampFloat(bindings.blue_offset_y, -1.0f, 1.0f, 0.0f);
  params.convergence_radial_strength = ClampFloat(
      bindings.convergence_radial_strength, 0.0f, 2.0f, 0.0f);
  params.enable_horizontal_filtering =
      bindings.horizontal_filtering_enabled;
  params.horizontal_sigma_x =
      ClampFloat(bindings.horizontal_sigma_x, 0.0f, 1.0f, 0.0f);

  params.enable_edge_blur = bindings.edge_blur_enabled;
  params.edge_blur_strength =
      ClampFloat(bindings.edge_blur_strength, 0.0f, 1.0f, 0.0f);
  params.edge_blur_radius =
      ClampFloat(bindings.edge_blur_radius, 0.2f, 1.0f, 0.2f);
  params.enable_scanlines = true;
  params.enable_scanline_multisample = bindings.scanline_multisample;
  params.scanline_weight = bindings.scanline_weight;
  params.scanline_gap_brightness = bindings.scanline_gap_brightness;
  params.enable_mask = bindings.phosphor_mask_enabled;
  params.phosphor_mask_pattern = bindings.phosphor_mask_pattern < 1U ? 1U :
      (bindings.phosphor_mask_pattern > 2U ? 2U :
       bindings.phosphor_mask_pattern);
  params.mask_brightness = ClampFloat(
      bindings.phosphor_mask_brightness, 0.0f, 1.0f, 1.0f);
  params.enable_bloom = bindings.bloom_enabled;
  params.bloom_factor =
      ClampFloat(bindings.bloom_factor, 0.0f, 5.0f, 0.0f);

  params.enable_vignette = bindings.vignette_enabled;
  params.vignette_strength =
      ClampFloat(bindings.vignette_strength, 0.0f, 1.0f, 0.0f);
  params.vignette_scale =
      ClampFloat(bindings.vignette_scale, 0.2f, 1.0f, 1.0f);
  params.vignette_softness =
      ClampFloat(bindings.vignette_softness, 0.02f, 1.0f, 0.02f);
  params.enable_uneven_illumination =
      bindings.uneven_illumination_enabled;
  params.uneven_illumination_strength = ClampFloat(
      bindings.uneven_illumination_strength, 0.0f, 0.35f, 0.0f);
  params.uneven_illumination_scale = ClampFloat(
      bindings.uneven_illumination_scale, 0.02f, 0.25f, 0.02f);

  params.enable_horizontal_jitter =
      bindings.horizontal_jitter_enabled;
  params.horizontal_jitter_strength = ClampFloat(
      bindings.horizontal_jitter_strength, 0.0f, 6.0f, 0.0f);
  params.horizontal_jitter_frequency = ClampFloat(
      bindings.horizontal_jitter_frequency, 0.01f, 0.4f, 0.01f);
  params.horizontal_jitter_speed =
      ClampFloat(bindings.horizontal_jitter_speed, 0.0f, 1.0f, 0.0f);
  params.enable_composite_artifacts =
      bindings.composite_artifacts_enabled;
  params.composite_chroma_blur =
      ClampFloat(bindings.composite_chroma_blur, 0.0f, 2.0f, 0.0f);
  params.composite_luma_sharpen =
      ClampFloat(bindings.composite_luma_sharpen, 0.0f, 1.0f, 0.0f);
  params.composite_color_bleed =
      ClampFloat(bindings.composite_color_bleed, 0.0f, 0.6f, 0.0f);

  params.enable_glass_reflection = bindings.glass_reflection_enabled;
  params.glass_reflection_angle = ClampFloat(
      bindings.glass_reflection_angle, -60.0f, 60.0f, 0.0f);
  params.glass_reflection_width = ClampFloat(
      bindings.glass_reflection_width, 0.02f, 0.6f, 0.02f);
  params.glass_reflection_position = ClampFloat(
      bindings.glass_reflection_position, 0.0f, 1.0f, 0.0f);
  params.enable_rounded_screen_mask =
      bindings.rounded_screen_mask_enabled;
  params.rounded_corner_radius = ClampFloat(
      bindings.rounded_corner_radius, 0.0f, 0.2f, 0.0f);
  params.rounded_border_softness = ClampFloat(
      bindings.rounded_border_softness, 0.0f, 0.08f, 0.0f);
  params.enable_edge_glow = bindings.edge_glow_enabled;
  params.edge_glow_strength =
      ClampFloat(bindings.edge_glow_strength, 0.0f, 0.35f, 0.0f);
  params.edge_glow_width =
      ClampFloat(bindings.edge_glow_width, 0.01f, 0.35f, 0.01f);

  params.enable_noise = bindings.noise_enabled;
  params.luminance_noise =
      ClampFloat(bindings.luminance_noise, 0.0f, 0.1f, 0.0f);
  params.chroma_noise =
      ClampFloat(bindings.chroma_noise, 0.0f, 0.08f, 0.0f);
  params.noise_speed =
      ClampFloat(bindings.noise_speed, 0.0f, 1.0f, 0.0f);
  params.temporal_frame =
      ClampFloat(bindings.temporal_frame, 0.0f, 1023.0f, 0.0f);

  params.enable_output_response = bindings.output_response_enabled;
  params.fast_output_response = bindings.output_response_fast;
  params.output_level_mapping = bindings.output_level_mapping <= 2U ?
      bindings.output_level_mapping : 1U;
  params.input_gamma =
      ClampFloat(bindings.input_gamma, 0.1f, 5.0f, 1.0f);
  params.output_gamma =
      ClampFloat(bindings.output_gamma, 0.1f, 5.0f, 1.0f);
  params.output_saturation =
      ClampFloat(bindings.output_saturation, 0.0f, 1.0f, 1.0f);
  params.black_level =
      ClampFloat(bindings.black_level, 0.0f, 1.0f, 0.0f);
  params.white_clip =
      ClampFloat(bindings.white_clip, 0.0f, 1.0f, 1.0f);

  return v3dcrt::BuildResolvedEffectUniformValues(
      bindings.source_width, bindings.source_height,
      bindings.target_height, params, values);
}

bool ResolveSemantic(const char *semantic,
                     const v3dcrt::EffectUniformValues &values,
                     uint32_t *value) {
  if (value == NULL) {
    return false;
  }
  float resolved = 0.0f;
  if (!v3dcrt::FindEffectUniformValue(values, semantic, &resolved)) {
    return false;
  }
  *value = FloatBits(resolved);
  return true;
}

bool ResolveUniform(const ShaderStageProgram &stage,
                    const ShaderUniformSpec &uniform,
                    const V3d42ShaderBindings &bindings,
                    const v3dcrt::EffectUniformValues &effect_values,
                    uint32_t *value) {
  if (StringEquals(uniform.kind, "QUNIFORM_CONSTANT")) {
    *value = uniform.data;
    return true;
  }
  if (StringEquals(uniform.kind, "QUNIFORM_VIEWPORT_X_SCALE")) {
    *value = FloatBits(bindings.target_width * 128.0f);
    return true;
  }
  if (StringEquals(uniform.kind, "QUNIFORM_VIEWPORT_Y_SCALE")) {
    *value = FloatBits(bindings.target_height * -128.0f);
    return true;
  }
  if (StringEquals(uniform.kind, "QUNIFORM_VIEWPORT_Z_OFFSET")) {
    *value = FloatBits(0.5f);
    return true;
  }
  if (StringEquals(uniform.kind, "QUNIFORM_UNIFORM")) {
    return ResolveSemantic(uniform.semantic, effect_values, value);
  }
  uint32_t base = 0U;
  if (stage.stage == v3dcrt::shaders::kShaderStageFragment &&
      StringEquals(uniform.semantic, "source_texture") &&
      StringEquals(uniform.kind, "QUNIFORM_TMU_CONFIG_P0")) {
    base = bindings.texture_state_address;
  } else if (stage.stage == v3dcrt::shaders::kShaderStageFragment &&
             StringEquals(uniform.semantic, "source_texture") &&
             StringEquals(uniform.kind, "QUNIFORM_TMU_CONFIG_P1")) {
    base = bindings.sampler_state_address;
  } else {
    return false;
  }
  if (base == 0U || uniform.data > UINT32_MAX - base) {
    return false;
  }
  *value = base + uniform.data;
  return true;
}

bool MaterializeStage(const ShaderStageProgram &stage,
                      uint32_t code_offset, uint32_t uniform_offset,
                      const V3d42ShaderBindings &bindings,
                      const v3dcrt::EffectUniformValues &effect_values,
                      uint8_t *buffer, uint32_t buffer_size,
                      const char **reason) {
  uint32_t code_bytes = 0U;
  uint32_t uniform_bytes = 0U;
  uint32_t uniform_word_count = 0U;
  if (!StageBytes(stage.qpu_word_count, sizeof(uint64_t), &code_bytes) ||
      !UniformWordCount(stage, &uniform_word_count) ||
      !StageBytes(uniform_word_count, sizeof(uint32_t), &uniform_bytes) ||
      code_offset > buffer_size || code_bytes > buffer_size - code_offset ||
      uniform_offset > buffer_size ||
      uniform_bytes > buffer_size - uniform_offset) {
    return Fail("pi4-materialize-slice-overflow", reason);
  }
  memcpy(buffer + code_offset, stage.qpu_words, code_bytes);
  for (uint32_t i = 0U; i < stage.uniform_count; ++i) {
    uint32_t value = 0U;
    if (!ResolveUniform(stage, stage.uniforms[i], bindings,
                        effect_values, &value)) {
      return Fail("pi4-uniform-semantic-unsupported", reason);
    }
    Store32(buffer + uniform_offset + i * sizeof(uint32_t), value);
  }
  Store32(buffer + uniform_offset +
              stage.uniform_count * sizeof(uint32_t), 0U);
  return true;
}

}  // namespace

bool PlanV3d42ShaderPackage(const ShaderPackage &package,
                            uint32_t first_offset, uint32_t buffer_size,
                            V3d42ShaderLayout *layout,
                            const char **reason) {
  if (reason != NULL) {
    *reason = NULL;
  }
  if (layout == NULL || first_offset >= buffer_size) {
    return Fail("pi4-layout-arguments", reason);
  }
  const ShaderStageProgram *coordinate = NULL;
  const ShaderStageProgram *vertex = NULL;
  const ShaderStageProgram *fragment = NULL;
  if (!ValidateAdapterContract(package, &coordinate, &vertex, &fragment,
                               reason)) {
    return false;
  }

  V3d42ShaderLayout planned = {};
  uint32_t cursor = first_offset;
  uint32_t coordinate_uniform_words = 0U;
  uint32_t vertex_uniform_words = 0U;
  uint32_t fragment_uniform_words = 0U;
  if (!UniformWordCount(*coordinate, &coordinate_uniform_words) ||
      !UniformWordCount(*vertex, &vertex_uniform_words) ||
      !UniformWordCount(*fragment, &fragment_uniform_words)) {
    return Fail("pi4-layout-overflow", reason);
  }
  if (!PlanStage(cursor, coordinate->qpu_word_count, sizeof(uint64_t),
                 buffer_size, &planned.coordinate_code_offset, &cursor) ||
      !PlanStage(cursor, vertex->qpu_word_count, sizeof(uint64_t),
                 buffer_size, &planned.vertex_code_offset, &cursor) ||
      !PlanStage(cursor, fragment->qpu_word_count, sizeof(uint64_t),
                 buffer_size, &planned.fragment_code_offset, &cursor) ||
      !PlanStage(cursor, coordinate_uniform_words,
                 sizeof(uint32_t), buffer_size,
                 &planned.coordinate_uniform_offset, &cursor) ||
      !PlanStage(cursor, vertex_uniform_words,
                 sizeof(uint32_t), buffer_size,
                 &planned.vertex_uniform_offset, &cursor) ||
      !PlanStage(cursor, fragment_uniform_words,
                 sizeof(uint32_t), buffer_size,
                 &planned.fragment_uniform_offset, &cursor)) {
    return Fail("pi4-layout-overflow", reason);
  }
  planned.end_offset = cursor;
  planned.coordinate_code_flags = CodeAddressFlags(*coordinate);
  planned.vertex_code_flags = CodeAddressFlags(*vertex);
  planned.fragment_code_flags = CodeAddressFlags(*fragment);
  planned.coordinate_vpm_output_size =
      coordinate->requirements.vpm_output_size;
  planned.vertex_vpm_output_size = vertex->requirements.vpm_output_size;
  planned.fragment_varying_count = fragment->requirements.varying_count;
  *layout = planned;
  return true;
}

bool MaterializeV3d42ShaderPackage(
    const ShaderPackage &package, const V3d42ShaderBindings &bindings,
    const V3d42ShaderLayout &layout, uint8_t *buffer,
    uint32_t buffer_size, const char **reason) {
  if (reason != NULL) {
    *reason = NULL;
  }
  if (buffer == NULL || bindings.source_width == 0U ||
      bindings.source_height == 0U || bindings.target_width == 0U ||
      bindings.target_height == 0U ||
      bindings.texture_state_address == 0U ||
      bindings.sampler_state_address == 0U) {
    return Fail("pi4-materialize-arguments", reason);
  }
  V3d42ShaderLayout planned = {};
  if (!PlanV3d42ShaderPackage(package, layout.coordinate_code_offset,
                              buffer_size, &planned, reason) ||
      !SameLayout(layout, planned)) {
    return reason != NULL && *reason != NULL ? false :
        Fail("pi4-materialize-layout-mismatch", reason);
  }
  const ShaderStageProgram *coordinate = FindShaderStage(
      package, v3dcrt::shaders::kShaderStageCoordinate);
  const ShaderStageProgram *vertex = FindShaderStage(
      package, v3dcrt::shaders::kShaderStageVertex);
  const ShaderStageProgram *fragment = FindShaderStage(
      package, v3dcrt::shaders::kShaderStageFragment);
  v3dcrt::EffectUniformValues effect_values = {};
  if (!BuildEffectUniformValues(bindings, &effect_values)) {
    return Fail("pi4-uniform-values-invalid", reason);
  }
  return MaterializeStage(*coordinate, layout.coordinate_code_offset,
                          layout.coordinate_uniform_offset, bindings,
                          effect_values,
                          buffer, buffer_size, reason) &&
         MaterializeStage(*vertex, layout.vertex_code_offset,
                          layout.vertex_uniform_offset, bindings,
                          effect_values,
                          buffer, buffer_size, reason) &&
         MaterializeStage(*fragment, layout.fragment_code_offset,
                          layout.fragment_uniform_offset, bindings,
                          effect_values,
                          buffer, buffer_size, reason);
}

bool FindV3d42FragmentUniformOffset(
    const ShaderPackage &package, const V3d42ShaderLayout &layout,
    const char *semantic, uint32_t *offset) {
  if (semantic == NULL || offset == NULL) {
    return false;
  }
  const ShaderStageProgram *fragment = FindShaderStage(
      package, v3dcrt::shaders::kShaderStageFragment);
  if (fragment == NULL) {
    return false;
  }
  for (uint32_t i = 0U; i < fragment->uniform_count; ++i) {
    if (StringEquals(fragment->uniforms[i].semantic, semantic)) {
      if (i > (UINT32_MAX - layout.fragment_uniform_offset) /
                  sizeof(uint32_t)) {
        return false;
      }
      *offset = layout.fragment_uniform_offset + i * sizeof(uint32_t);
      return true;
    }
  }
  return false;
}

}  // namespace pi4v3d
