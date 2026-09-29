#ifndef V3DCRT_SEMANTIC_VALUES_H
#define V3DCRT_SEMANTIC_VALUES_H

#include "v3dcrt/effect_params.h"

#include <stdint.h>

namespace v3dcrt {

struct EffectUniformValues {
  struct Patch {
    const char *semantic;
    float value;
  };

  float fragcoord_y_scale;
  float fragcoord_y_bias;
  float scanline_gap_brightness;
  float scanline_weight;
  float output_response_enable;
  float output_response_fast;
  float input_gamma;
  float inverse_output_gamma;
  float saturation;
  float black_level;
  float white_clip;
  float level_mapping;
  float phosphor_mask_enable;
  float phosphor_mask_pattern;
  float phosphor_mask_brightness;
  float precomputed_black_point;
  float precomputed_level_scale;
  float precomputed_edge_blur_offset_scale;
  float precomputed_edge_blur_inner_radius;
  float precomputed_scanline_gap;
  float precomputed_vignette_outer;
  float precomputed_uneven_sx;
  float precomputed_uneven_sy;
  float precomputed_uneven_sx_diagonal;
  float precomputed_uneven_sy_diagonal;
  float precomputed_glass_position;
  float precomputed_glass_outer_width;
  float precomputed_rounded_radius;
  float precomputed_rounded_box;
  float precomputed_rounded_softness;
  Patch patches[72];
  uint32_t patch_count;
  Patch derived_patches[40];
  uint32_t derived_patch_count;
};

constexpr uint32_t kPrecomputedEffectUniformPatchCount = 15U;
constexpr uint32_t kFrameEffectUniformPatchOffset =
    kPrecomputedEffectUniformPatchCount;
constexpr uint32_t kFrameEffectUniformPatchCount = 4U;

// Resolves menu-scale EffectParams into the exact values consumed by the
// generated shader packages. The primary patch list intentionally retains its
// historical 67-entry order because it is also used for diagnostic signatures.
bool BuildEffectUniformValues(uint32_t source_width,
                              uint32_t source_height,
                              uint32_t output_height,
                              const EffectParams &params,
                              EffectUniformValues *values);

// Pi4 has already normalized scanline weight and all public parameters before
// package materialization. This variant preserves those resolved values rather
// than applying the menu-scale conversion a second time.
bool BuildResolvedEffectUniformValues(uint32_t source_width,
                                      uint32_t source_height,
                                      uint32_t output_height,
                                      const EffectParams &params,
                                      EffectUniformValues *values);

bool FindEffectUniformValue(const EffectUniformValues &values,
                            const char *semantic,
                            float *value);

}  // namespace v3dcrt

#endif  // V3DCRT_SEMANTIC_VALUES_H
