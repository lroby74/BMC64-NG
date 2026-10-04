#ifndef V3DCRT_OUTPUT_RESPONSE_H
#define V3DCRT_OUTPUT_RESPONSE_H

#include <stdint.h>

namespace v3dcrt {

struct OutputResponseParams {
  bool enabled;
  bool fast;
  uint32_t level_mapping;
  float input_gamma;
  float output_gamma;
  float saturation;
  float black_level;
  float white_clip;
};

OutputResponseParams ResolveOutputResponseParams(
    bool enabled, bool fast, uint32_t level_mapping, float input_gamma,
    float output_gamma, float saturation, float black_level,
    float white_clip);

bool OutputResponseParamsEqual(const OutputResponseParams &a,
                               const OutputResponseParams &b);

uint16_t ResolveOutputResponseRgb565(
    uint16_t pixel, const OutputResponseParams &params);

// Builds the complete RGB565 source-response table. Accurate gamma uses
// channel quantization thresholds so the expensive curve is evaluated once
// per output level instead of once per source color.
bool BuildOutputResponseRgb565Lut(
    const OutputResponseParams &params, uint16_t *lut,
    uint32_t lut_entries);

}  // namespace v3dcrt

#endif
