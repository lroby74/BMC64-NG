#ifndef V3DCRT_EFFECT_PARAMS_H
#define V3DCRT_EFFECT_PARAMS_H

#include <stdint.h>

struct bmx_crt_effect_params;

namespace v3dcrt {

enum PhosphorMaskPattern {
  kPhosphorMaskNone = 0,
  kPhosphorMaskGreenMagenta = 1,
  kPhosphorMaskRgbApertureGrille = 2
};

enum OutputLevelMapping {
  kOutputLevelMappingLinear = 0,
  kOutputLevelMappingCubic,
  kOutputLevelMappingToeShoulder
};

struct EffectParams {
  bool enable_interpolation;

  bool enable_geometry;
  float curvature_x;
  float curvature_y;
  float skew_x;
  float skew_y;
  float trapezoid;
  float rotation_degrees;
  float overscan_scale;

  bool enable_convergence;
  float red_offset_x;
  float red_offset_y;
  float blue_offset_x;
  float blue_offset_y;
  float convergence_radial_strength;

  bool enable_horizontal_filtering;
  float horizontal_sigma_x;

  bool enable_edge_blur;
  float edge_blur_strength;
  float edge_blur_radius;

  bool enable_scanlines;
  bool enable_scanline_multisample;
  float scanline_weight;
  float scanline_gap_brightness;

  bool enable_mask;
  uint32_t phosphor_mask_pattern;
  float mask_brightness;

  bool enable_bloom;
  float bloom_factor;

  bool enable_vignette;
  float vignette_strength;
  float vignette_scale;
  float vignette_softness;

  bool enable_uneven_illumination;
  float uneven_illumination_strength;
  float uneven_illumination_scale;

  bool enable_horizontal_jitter;
  float horizontal_jitter_strength;
  float horizontal_jitter_frequency;
  float horizontal_jitter_speed;

  bool enable_composite_artifacts;
  float composite_chroma_blur;
  float composite_luma_sharpen;
  float composite_color_bleed;

  bool enable_glass_reflection;
  float glass_reflection_angle;
  float glass_reflection_width;
  float glass_reflection_position;

  bool enable_rounded_screen_mask;
  float rounded_corner_radius;
  float rounded_border_softness;

  bool enable_edge_glow;
  float edge_glow_strength;
  float edge_glow_width;

  bool enable_noise;
  float luminance_noise;
  float chroma_noise;
  float noise_speed;
  float temporal_frame;

  bool enable_output_response;
  bool fast_output_response;
  uint32_t output_level_mapping;
  float input_gamma;
  float output_gamma;
  float output_saturation;
  float black_level;
  float white_clip;
};

EffectParams DefaultEffectParams();
EffectParams EffectParamsFromBmx(const ::bmx_crt_effect_params &params);

}  // namespace v3dcrt

#endif  // V3DCRT_EFFECT_PARAMS_H
