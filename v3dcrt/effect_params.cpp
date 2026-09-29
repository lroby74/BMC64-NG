#include "v3dcrt/effect_params.h"

extern "C" {
#include "third_party/common/circle.h"
}

namespace v3dcrt {

EffectParams DefaultEffectParams() {
  EffectParams params = {};
  params.overscan_scale = 1.0f;
  params.edge_blur_radius = 0.7f;
  params.scanline_gap_brightness = 0.5f;
  params.phosphor_mask_pattern = kPhosphorMaskNone;
  params.mask_brightness = 1.0f;
  params.bloom_factor = 1.0f;
  params.vignette_scale = 0.8f;
  params.vignette_softness = 0.45f;
  params.uneven_illumination_scale = 0.08f;
  params.horizontal_jitter_frequency = 0.08f;
  params.glass_reflection_angle = -20.0f;
  params.glass_reflection_width = 0.18f;
  params.glass_reflection_position = 0.35f;
  params.edge_glow_width = 0.08f;
  params.input_gamma = 1.0f;
  params.output_gamma = 1.0f;
  params.output_saturation = 1.0f;
  params.output_level_mapping = kOutputLevelMappingCubic;
  params.white_clip = 1.0f;
  return params;
}

EffectParams EffectParamsFromBmx(const ::bmx_crt_effect_params &source) {
  EffectParams params = {};
  params.enable_interpolation = source.bilinear_interpolation != 0;
  params.enable_geometry = source.geometry_enabled != 0;
  params.curvature_x = source.curvature_x;
  params.curvature_y = source.curvature_y;
  params.skew_x = source.skew_x;
  params.skew_y = source.skew_y;
  params.trapezoid = source.trapezoid;
  params.rotation_degrees = source.rotation_degrees;
  params.overscan_scale = source.overscan_scale;
  params.enable_convergence = source.convergence_enabled != 0;
  params.red_offset_x = source.red_offset_x;
  params.red_offset_y = source.red_offset_y;
  params.blue_offset_x = source.blue_offset_x;
  params.blue_offset_y = source.blue_offset_y;
  params.convergence_radial_strength = source.convergence_radial_strength;
  params.enable_horizontal_filtering =
      source.horizontal_filtering_enabled != 0;
  params.horizontal_sigma_x = source.horizontal_sigma_x;
  params.enable_edge_blur = source.edge_blur_enabled != 0;
  params.edge_blur_strength = source.edge_blur_strength;
  params.edge_blur_radius = source.edge_blur_radius;
  params.enable_scanlines = source.scanlines_enabled != 0;
  params.enable_scanline_multisample = source.scanline_multisample != 0;
  params.scanline_weight = source.scanline_weight;
  params.scanline_gap_brightness = source.scanline_gap_brightness;
  params.enable_mask = source.phosphor_mask_enabled != 0;
  params.phosphor_mask_pattern = source.phosphor_mask_type == 2 ?
      kPhosphorMaskRgbApertureGrille :
      (source.phosphor_mask_type == 1 ?
           kPhosphorMaskGreenMagenta : kPhosphorMaskNone);
  params.mask_brightness = source.phosphor_mask_brightness;
  params.enable_bloom = source.bloom_enabled != 0;
  params.bloom_factor = source.bloom_factor;
  params.enable_vignette = source.vignette_enabled != 0;
  params.vignette_strength = source.vignette_strength;
  params.vignette_scale = source.vignette_scale;
  params.vignette_softness = source.vignette_softness;
  params.enable_uneven_illumination =
      source.uneven_illumination_enabled != 0;
  params.uneven_illumination_strength = source.uneven_illumination_strength;
  params.uneven_illumination_scale = source.uneven_illumination_scale;
  params.enable_horizontal_jitter = source.horizontal_jitter_enabled != 0;
  params.horizontal_jitter_strength = source.horizontal_jitter_strength;
  params.horizontal_jitter_frequency = source.horizontal_jitter_frequency;
  params.horizontal_jitter_speed = source.horizontal_jitter_speed;
  params.enable_composite_artifacts =
      source.composite_artifacts_enabled != 0;
  params.composite_chroma_blur = source.composite_chroma_blur;
  params.composite_luma_sharpen = source.composite_luma_sharpen;
  params.composite_color_bleed = source.composite_color_bleed;
  params.enable_glass_reflection = source.glass_reflection_enabled != 0;
  params.glass_reflection_angle = source.glass_reflection_angle;
  params.glass_reflection_width = source.glass_reflection_width;
  params.glass_reflection_position = source.glass_reflection_position;
  params.enable_rounded_screen_mask =
      source.rounded_screen_mask_enabled != 0;
  params.rounded_corner_radius = source.rounded_corner_radius;
  params.rounded_border_softness = source.rounded_border_softness;
  params.enable_edge_glow = source.edge_glow_enabled != 0;
  params.edge_glow_strength = source.edge_glow_strength;
  params.edge_glow_width = source.edge_glow_width;
  params.enable_noise = source.noise_enabled != 0;
  params.luminance_noise = source.luminance_noise;
  params.chroma_noise = source.chroma_noise;
  params.noise_speed = source.noise_speed;
  params.enable_output_response = source.output_response_enabled != 0;
  params.fast_output_response = source.output_response_fast != 0;
  switch (source.output_level_mapping) {
    case BMX_OUTPUT_LEVEL_MAPPING_LINEAR:
      params.output_level_mapping = kOutputLevelMappingLinear;
      break;
    case BMX_OUTPUT_LEVEL_MAPPING_TOE_SHOULDER:
      params.output_level_mapping = kOutputLevelMappingToeShoulder;
      break;
    case BMX_OUTPUT_LEVEL_MAPPING_CUBIC:
    default:
      params.output_level_mapping = kOutputLevelMappingCubic;
      break;
  }
  params.input_gamma = source.input_gamma;
  params.output_gamma = source.output_gamma;
  params.output_saturation = source.output_saturation;
  params.black_level = source.black_level;
  params.white_clip = source.white_clip;
  return params;
}

}  // namespace v3dcrt
