#ifndef PI4V3D_PI4_V3D42_SHADER_PACKAGE_ADAPTER_H
#define PI4V3D_PI4_V3D42_SHADER_PACKAGE_ADAPTER_H

#include <stdint.h>

#include "v3dcrt/shaders/shader_package.h"

namespace pi4v3d {

struct V3d42ShaderLayout {
  uint32_t coordinate_code_offset;
  uint32_t vertex_code_offset;
  uint32_t fragment_code_offset;
  uint32_t coordinate_uniform_offset;
  uint32_t vertex_uniform_offset;
  uint32_t fragment_uniform_offset;
  uint32_t end_offset;
  uint32_t coordinate_code_flags;
  uint32_t vertex_code_flags;
  uint32_t fragment_code_flags;
  uint32_t coordinate_vpm_output_size;
  uint32_t vertex_vpm_output_size;
  uint32_t fragment_varying_count;
};

struct V3d42ShaderBindings {
  uint32_t source_width;
  uint32_t source_height;
  uint32_t target_width;
  uint32_t target_height;
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
  bool scanline_multisample;
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
  uint32_t texture_state_address;
  uint32_t sampler_state_address;
};

// Validates a generated V3D 4.2 package and derives non-overlapping code and
// uniform-stream slices beginning at first_offset. Each uniform stream has a
// trailing zero word so V3D prefetch never reaches the following slice.
bool PlanV3d42ShaderPackage(
    const v3dcrt::shaders::ShaderPackage &package,
    uint32_t first_offset, uint32_t buffer_size,
    V3d42ShaderLayout *layout, const char **reason);

// Copies the planned QPU stages and resolves every uniform from its declared
// kind and semantic. The caller owns fixed-function state and command lists.
bool MaterializeV3d42ShaderPackage(
    const v3dcrt::shaders::ShaderPackage &package,
    const V3d42ShaderBindings &bindings,
    const V3d42ShaderLayout &layout,
    uint8_t *buffer, uint32_t buffer_size, const char **reason);

// Resolves a fragment uniform by behavior semantic, avoiding generated-order
// assumptions in callers and tests.
bool FindV3d42FragmentUniformOffset(
    const v3dcrt::shaders::ShaderPackage &package,
    const V3d42ShaderLayout &layout, const char *semantic,
    uint32_t *offset);

}  // namespace pi4v3d

#endif  // PI4V3D_PI4_V3D42_SHADER_PACKAGE_ADAPTER_H
