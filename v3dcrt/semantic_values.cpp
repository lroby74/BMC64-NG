#include "v3dcrt/semantic_values.h"

#include <math.h>
#include <string.h>

namespace v3dcrt {
namespace {

constexpr float kMaxCrtCurvature = 100.0f / 600.0f;
constexpr float kDegreesToRadians = 0.0174532925f;

float ClampFloat(float value, float minimum, float maximum) {
  if (!(value >= minimum)) {
    return minimum;
  }
  return value > maximum ? maximum : value;
}

bool AddPatch(EffectUniformValues::Patch *patches,
              uint32_t capacity,
              uint32_t *count,
              const char *semantic,
              float value) {
  if (patches == nullptr || count == nullptr || semantic == nullptr ||
      *count >= capacity) {
    return false;
  }
  patches[*count].semantic = semantic;
  patches[*count].value = value;
  ++*count;
  return true;
}

bool BuildEffectUniformValuesInternal(uint32_t source_width,
                                      uint32_t source_height,
                                      uint32_t output_height,
                                      const EffectParams &params,
                                      bool scanline_weight_is_normalized,
                                      EffectUniformValues *values) {
  if (source_width == 0U || source_height == 0U || output_height == 0U ||
      values == nullptr) {
    return false;
  }

  memset(values, 0, sizeof *values);
  const float normalized_weight = scanline_weight_is_normalized ?
      params.scanline_weight :
      ClampFloat(params.scanline_weight, 0.0f, 15.0f) / 15.0f;
  const float clamped_output_gamma =
      ClampFloat(params.output_gamma, 0.1f, 5.0f);
  values->fragcoord_y_scale = -1.0f;
  values->fragcoord_y_bias = static_cast<float>(output_height);
  values->scanline_gap_brightness = scanline_weight_is_normalized ?
      params.scanline_gap_brightness :
      ClampFloat(params.scanline_gap_brightness, 0.0f, 1.0f);
  values->scanline_weight = params.enable_scanlines ? normalized_weight : 0.0f;
  values->output_response_enable =
      params.enable_output_response ? 1.0f : 0.0f;
  values->output_response_fast = params.fast_output_response ? 1.0f : 0.0f;
  values->input_gamma = ClampFloat(params.input_gamma, 0.1f, 5.0f);
  values->inverse_output_gamma = 1.0f / clamped_output_gamma;
  values->saturation = ClampFloat(params.output_saturation, 0.0f, 1.0f);
  values->black_level = ClampFloat(params.black_level, 0.0f, 1.0f);
  values->white_clip = ClampFloat(params.white_clip, 0.0f, 1.0f);
  values->level_mapping = params.output_level_mapping <= 2U ?
      static_cast<float>(params.output_level_mapping) : 1.0f;
  values->phosphor_mask_enable = params.enable_mask ? 1.0f : 0.0f;
  values->phosphor_mask_pattern =
      params.phosphor_mask_pattern == 2U ? 2.0f : 1.0f;
  values->phosphor_mask_brightness =
      ClampFloat(params.mask_brightness, 0.0f, 1.0f);

  const float black_squared = values->black_level * values->black_level;
  values->precomputed_black_point = black_squared * values->black_level;
  const float white_headroom = 1.0f - values->white_clip;
  const float white_headroom_squared = white_headroom * white_headroom;
  float white_point = 1.0f - white_headroom_squared * white_headroom;
  const float minimum_white_point =
      values->precomputed_black_point + 0.00390625f;
  if (white_point < minimum_white_point) {
    white_point = minimum_white_point;
  }
  values->precomputed_level_scale =
      1.0f / (white_point - values->precomputed_black_point);

  const float edge_blur_strength =
      ClampFloat(params.edge_blur_strength, 0.0f, 1.0f);
  const float edge_blur_radius =
      ClampFloat(params.edge_blur_radius, 0.2f, 1.0f);
  values->precomputed_edge_blur_offset_scale =
      1.0f + edge_blur_strength * 3.0f;
  values->precomputed_edge_blur_inner_radius = edge_blur_radius * 0.55f;
  values->precomputed_scanline_gap =
      (1.0f - values->scanline_weight) +
      values->scanline_gap_brightness * values->scanline_weight;

  const float vignette_scale =
      ClampFloat(params.vignette_scale, 0.2f, 1.0f);
  const float vignette_softness =
      ClampFloat(params.vignette_softness, 0.02f, 1.0f);
  values->precomputed_vignette_outer = vignette_scale + vignette_softness;

  const float uneven_scale =
      ClampFloat(params.uneven_illumination_scale, 0.02f, 0.25f);
  values->precomputed_uneven_sx = 1.2f + uneven_scale * 8.0f;
  values->precomputed_uneven_sy = 0.9f + uneven_scale * 7.0f;
  values->precomputed_uneven_sx_diagonal =
      values->precomputed_uneven_sx * 0.8f;
  values->precomputed_uneven_sy_diagonal =
      values->precomputed_uneven_sy * 0.7f;

  values->precomputed_glass_position =
      ClampFloat(params.glass_reflection_position, 0.0f, 1.0f) * 2.0f - 1.0f;
  const float glass_width =
      ClampFloat(params.glass_reflection_width, 0.02f, 0.6f);
  values->precomputed_glass_outer_width = glass_width * 2.5f;
  values->precomputed_rounded_radius =
      ClampFloat(params.rounded_corner_radius, 0.0f, 0.2f) * 2.0f;
  values->precomputed_rounded_box =
      1.0f - values->precomputed_rounded_radius;
  const float rounded_softness =
      ClampFloat(params.rounded_border_softness, 0.0f, 0.08f) * 2.0f;
  values->precomputed_rounded_softness =
      rounded_softness > 0.0001f ? rounded_softness : 0.0001f;

#define ADD_EFFECT_PATCH(name, value)                                      \
  if (!AddPatch(values->patches,                                           \
                sizeof values->patches / sizeof values->patches[0],        \
                &values->patch_count, name, value)) return false

  ADD_EFFECT_PATCH("fragcoord_y_scale", values->fragcoord_y_scale);
  ADD_EFFECT_PATCH("fragcoord_y_bias", values->fragcoord_y_bias);
  ADD_EFFECT_PATCH("source_texel_x", 1.0f / static_cast<float>(source_width));
  ADD_EFFECT_PATCH("source_texel_y", 1.0f / static_cast<float>(source_height));
  ADD_EFFECT_PATCH("geometry_enable", params.enable_geometry ? 1.0f : 0.0f);
  ADD_EFFECT_PATCH("curvature_x",
      ClampFloat(params.curvature_x, 0.0f, kMaxCrtCurvature));
  ADD_EFFECT_PATCH("curvature_y",
      ClampFloat(params.curvature_y, 0.0f, kMaxCrtCurvature));
  ADD_EFFECT_PATCH("skew_x", ClampFloat(params.skew_x, -0.08f, 0.08f));
  ADD_EFFECT_PATCH("skew_y", ClampFloat(params.skew_y, -0.08f, 0.08f));
  ADD_EFFECT_PATCH("trapezoid",
      ClampFloat(params.trapezoid, -0.15f, 0.15f));
  ADD_EFFECT_PATCH("rotation_radians",
      ClampFloat(params.rotation_degrees, -3.0f, 3.0f) * kDegreesToRadians);
  ADD_EFFECT_PATCH("overscan_scale",
      ClampFloat(params.overscan_scale, 1.0f, 1.2f));
  ADD_EFFECT_PATCH("convergence_enable",
      params.enable_convergence ? 1.0f : 0.0f);
  ADD_EFFECT_PATCH("red_offset_x",
      ClampFloat(params.red_offset_x, -1.0f, 1.0f));
  ADD_EFFECT_PATCH("red_offset_y",
      ClampFloat(params.red_offset_y, -1.0f, 1.0f));
  ADD_EFFECT_PATCH("blue_offset_x",
      ClampFloat(params.blue_offset_x, -1.0f, 1.0f));
  ADD_EFFECT_PATCH("blue_offset_y",
      ClampFloat(params.blue_offset_y, -1.0f, 1.0f));
  ADD_EFFECT_PATCH("convergence_radial_strength",
      ClampFloat(params.convergence_radial_strength, 0.0f, 2.0f));
  ADD_EFFECT_PATCH("horizontal_filter_enable",
      params.enable_horizontal_filtering ? 1.0f : 0.0f);
  ADD_EFFECT_PATCH("horizontal_sigma_x",
      ClampFloat(params.horizontal_sigma_x, 0.0f, 1.0f));
  ADD_EFFECT_PATCH("edge_blur_enable", params.enable_edge_blur ? 1.0f : 0.0f);
  ADD_EFFECT_PATCH("edge_blur_strength", edge_blur_strength);
  ADD_EFFECT_PATCH("edge_blur_radius", edge_blur_radius);
  ADD_EFFECT_PATCH("scanline_weight", values->scanline_weight);
  ADD_EFFECT_PATCH("scanline_gap_brightness",
      values->scanline_gap_brightness);
  ADD_EFFECT_PATCH("scanline_multisample",
      params.enable_scanlines && params.enable_scanline_multisample ?
          1.0f : 0.0f);
  ADD_EFFECT_PATCH("output_response_enable",
      values->output_response_enable);
  ADD_EFFECT_PATCH("output_response_fast", values->output_response_fast);
  ADD_EFFECT_PATCH("input_gamma", values->input_gamma);
  ADD_EFFECT_PATCH("inverse_output_gamma", values->inverse_output_gamma);
  ADD_EFFECT_PATCH("saturation", values->saturation);
  ADD_EFFECT_PATCH("black_level", values->black_level);
  ADD_EFFECT_PATCH("white_clip", values->white_clip);
  ADD_EFFECT_PATCH("level_mapping", values->level_mapping);
  ADD_EFFECT_PATCH("phosphor_mask_enable", values->phosphor_mask_enable);
  ADD_EFFECT_PATCH("phosphor_mask_pattern", values->phosphor_mask_pattern);
  ADD_EFFECT_PATCH("phosphor_mask_brightness",
      values->phosphor_mask_brightness);
  ADD_EFFECT_PATCH("vignette_enable", params.enable_vignette ? 1.0f : 0.0f);
  ADD_EFFECT_PATCH("vignette_strength",
      ClampFloat(params.vignette_strength, 0.0f, 1.0f));
  ADD_EFFECT_PATCH("vignette_scale", vignette_scale);
  ADD_EFFECT_PATCH("vignette_softness", vignette_softness);
  ADD_EFFECT_PATCH("uneven_illumination_enable",
      params.enable_uneven_illumination ? 1.0f : 0.0f);
  ADD_EFFECT_PATCH("uneven_illumination_strength",
      ClampFloat(params.uneven_illumination_strength, 0.0f, 0.35f));
  ADD_EFFECT_PATCH("uneven_illumination_scale", uneven_scale);
  ADD_EFFECT_PATCH("horizontal_jitter_enable",
      params.enable_horizontal_jitter ? 1.0f : 0.0f);
  ADD_EFFECT_PATCH("horizontal_jitter_strength",
      ClampFloat(params.horizontal_jitter_strength, 0.0f, 6.0f));
  ADD_EFFECT_PATCH("horizontal_jitter_frequency",
      ClampFloat(params.horizontal_jitter_frequency, 0.01f, 0.4f));
  ADD_EFFECT_PATCH("composite_artifacts_enable",
      params.enable_composite_artifacts ? 1.0f : 0.0f);
  ADD_EFFECT_PATCH("composite_chroma_blur",
      ClampFloat(params.composite_chroma_blur, 0.0f, 2.0f));
  ADD_EFFECT_PATCH("composite_luma_sharpen",
      ClampFloat(params.composite_luma_sharpen, 0.0f, 1.0f));
  ADD_EFFECT_PATCH("composite_color_bleed",
      ClampFloat(params.composite_color_bleed, 0.0f, 0.6f));
  ADD_EFFECT_PATCH("glass_reflection_enable",
      params.enable_glass_reflection ? 1.0f : 0.0f);
  ADD_EFFECT_PATCH("glass_reflection_angle",
      ClampFloat(params.glass_reflection_angle, -60.0f, 60.0f) *
          kDegreesToRadians);
  ADD_EFFECT_PATCH("glass_reflection_width", glass_width);
  ADD_EFFECT_PATCH("glass_reflection_position",
      ClampFloat(params.glass_reflection_position, 0.0f, 1.0f));
  ADD_EFFECT_PATCH("rounded_screen_mask_enable",
      params.enable_rounded_screen_mask ? 1.0f : 0.0f);
  ADD_EFFECT_PATCH("rounded_corner_radius",
      ClampFloat(params.rounded_corner_radius, 0.0f, 0.2f));
  ADD_EFFECT_PATCH("rounded_border_softness",
      ClampFloat(params.rounded_border_softness, 0.0f, 0.08f));
  ADD_EFFECT_PATCH("edge_glow_enable", params.enable_edge_glow ? 1.0f : 0.0f);
  ADD_EFFECT_PATCH("edge_glow_strength",
      ClampFloat(params.edge_glow_strength, 0.0f, 0.35f));
  ADD_EFFECT_PATCH("edge_glow_width",
      ClampFloat(params.edge_glow_width, 0.01f, 0.35f));
  ADD_EFFECT_PATCH("noise_enable", params.enable_noise ? 1.0f : 0.0f);
  ADD_EFFECT_PATCH("luminance_noise",
      ClampFloat(params.luminance_noise, 0.0f, 0.1f));
  ADD_EFFECT_PATCH("chroma_noise",
      ClampFloat(params.chroma_noise, 0.0f, 0.08f));
  ADD_EFFECT_PATCH("horizontal_jitter_speed",
      ClampFloat(params.horizontal_jitter_speed, 0.0f, 1.0f));
  ADD_EFFECT_PATCH("noise_speed",
      ClampFloat(params.noise_speed, 0.0f, 1.0f));
  ADD_EFFECT_PATCH("temporal_frame",
      ClampFloat(params.temporal_frame, 0.0f, 1023.0f));

#undef ADD_EFFECT_PATCH

#define ADD_DERIVED_PATCH(name, value)                                     \
  if (!AddPatch(values->derived_patches,                                   \
                sizeof values->derived_patches /                           \
                    sizeof values->derived_patches[0],                     \
                &values->derived_patch_count, name, value)) return false

  ADD_DERIVED_PATCH("precomputed_black_point",
      values->precomputed_black_point);
  ADD_DERIVED_PATCH("precomputed_level_scale",
      values->precomputed_level_scale);
  ADD_DERIVED_PATCH("precomputed_edge_blur_offset_scale",
      values->precomputed_edge_blur_offset_scale);
  ADD_DERIVED_PATCH("precomputed_edge_blur_inner_radius",
      values->precomputed_edge_blur_inner_radius);
  ADD_DERIVED_PATCH("precomputed_scanline_gap",
      values->precomputed_scanline_gap);
  ADD_DERIVED_PATCH("precomputed_vignette_outer",
      values->precomputed_vignette_outer);
  ADD_DERIVED_PATCH("precomputed_uneven_sx", values->precomputed_uneven_sx);
  ADD_DERIVED_PATCH("precomputed_uneven_sy", values->precomputed_uneven_sy);
  ADD_DERIVED_PATCH("precomputed_uneven_sx_diagonal",
      values->precomputed_uneven_sx_diagonal);
  ADD_DERIVED_PATCH("precomputed_uneven_sy_diagonal",
      values->precomputed_uneven_sy_diagonal);
  ADD_DERIVED_PATCH("precomputed_glass_position",
      values->precomputed_glass_position);
  ADD_DERIVED_PATCH("precomputed_glass_outer_width",
      values->precomputed_glass_outer_width);
  ADD_DERIVED_PATCH("precomputed_rounded_radius",
      values->precomputed_rounded_radius);
  ADD_DERIVED_PATCH("precomputed_rounded_box",
      values->precomputed_rounded_box);
  ADD_DERIVED_PATCH("precomputed_rounded_softness",
      values->precomputed_rounded_softness);

  const float rotation_radians =
      ClampFloat(params.rotation_degrees, -3.0f, 3.0f) * kDegreesToRadians;
  const float glass_radians =
      ClampFloat(params.glass_reflection_angle, -60.0f, 60.0f) *
      kDegreesToRadians;
  ADD_DERIVED_PATCH("rotation_cosine", cosf(rotation_radians));
  ADD_DERIVED_PATCH("rotation_sine", sinf(rotation_radians));
  ADD_DERIVED_PATCH("glass_reflection_cosine", cosf(glass_radians));
  ADD_DERIVED_PATCH("glass_reflection_sine", sinf(glass_radians));

  const float edge_glow_width =
      ClampFloat(params.edge_glow_width, 0.01f, 0.35f);
  const float edge_glow_width_control =
      ClampFloat((edge_glow_width - 0.01f) / 0.34f, 0.0f, 1.0f);
  const float edge_glow_strength =
      ClampFloat(params.edge_glow_strength, 0.0f, 0.35f);
  const float edge_glow_strength_control =
      ClampFloat(edge_glow_strength / 0.35f, 0.0f, 1.0f);
  ADD_DERIVED_PATCH("precomputed_edge_glow_width",
      0.01f + sqrtf(edge_glow_width_control) * 0.34f);
  ADD_DERIVED_PATCH("precomputed_edge_glow_strength",
      sqrtf(edge_glow_strength_control) * 1.2f);
  ADD_DERIVED_PATCH("bloom_enable", params.enable_bloom ? 1.0f : 0.0f);
  ADD_DERIVED_PATCH("bloom_factor",
      ClampFloat(params.bloom_factor, 0.0f, 5.0f));

  static const char *const edge_glow_frame_semantics[] = {
    "edge_glow_top_r", "edge_glow_top_g", "edge_glow_top_b",
    "edge_glow_bottom_r", "edge_glow_bottom_g", "edge_glow_bottom_b",
    "edge_glow_left_r", "edge_glow_left_g", "edge_glow_left_b",
    "edge_glow_right_r", "edge_glow_right_g", "edge_glow_right_b",
  };
  for (uint32_t i = 0;
       i < sizeof edge_glow_frame_semantics /
               sizeof edge_glow_frame_semantics[0]; ++i) {
    ADD_DERIVED_PATCH(edge_glow_frame_semantics[i], 0.0f);
  }

#undef ADD_DERIVED_PATCH
  return values->patch_count == 67U && values->derived_patch_count == 35U;
}

}  // namespace

bool BuildEffectUniformValues(uint32_t source_width,
                              uint32_t source_height,
                              uint32_t output_height,
                              const EffectParams &params,
                              EffectUniformValues *values) {
  return BuildEffectUniformValuesInternal(
      source_width, source_height, output_height, params, false, values);
}

bool BuildResolvedEffectUniformValues(uint32_t source_width,
                                      uint32_t source_height,
                                      uint32_t output_height,
                                      const EffectParams &params,
                                      EffectUniformValues *values) {
  return BuildEffectUniformValuesInternal(
      source_width, source_height, output_height, params, true, values);
}

bool FindEffectUniformValue(const EffectUniformValues &values,
                            const char *semantic,
                            float *value) {
  if (semantic == nullptr || value == nullptr) {
    return false;
  }
  for (uint32_t i = 0; i < values.patch_count; ++i) {
    if (strcmp(values.patches[i].semantic, semantic) == 0) {
      *value = values.patches[i].value;
      return true;
    }
  }
  for (uint32_t i = 0; i < values.derived_patch_count; ++i) {
    if (strcmp(values.derived_patches[i].semantic, semantic) == 0) {
      *value = values.derived_patches[i].value;
      return true;
    }
  }
  return false;
}

}  // namespace v3dcrt
