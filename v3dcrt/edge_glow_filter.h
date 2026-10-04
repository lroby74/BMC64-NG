#ifndef V3DCRT_EDGE_GLOW_FILTER_H
#define V3DCRT_EDGE_GLOW_FILTER_H

#include <stdint.h>

namespace v3dcrt {

enum EdgeGlowFieldRegion {
  kEdgeGlowFieldTop = 0,
  kEdgeGlowFieldBottom,
  kEdgeGlowFieldLeft,
  kEdgeGlowFieldRight,
  kEdgeGlowFieldSampleCount
};

struct EdgeGlowFieldColor {
  float red;
  float green;
  float blue;
};

struct EdgeGlowTemporalFilter {
  bool valid;
  uint64_t last_update_us;
  EdgeGlowFieldColor color[kEdgeGlowFieldSampleCount];
};

inline void ResetEdgeGlowTemporalFilter(EdgeGlowTemporalFilter *filter) {
  if (filter == nullptr) {
    return;
  }

  filter->valid = false;
  filter->last_update_us = 0U;
  for (uint32_t i = 0; i < kEdgeGlowFieldSampleCount; ++i) {
    filter->color[i].red = 0.0f;
    filter->color[i].green = 0.0f;
    filter->color[i].blue = 0.0f;
  }
}

inline bool UpdateEdgeGlowTemporalFilter(
    const EdgeGlowFieldColor current[kEdgeGlowFieldSampleCount],
    uint64_t now_us,
    uint64_t time_constant_us,
    uint64_t reset_gap_us,
    EdgeGlowTemporalFilter *filter,
    EdgeGlowFieldColor output[kEdgeGlowFieldSampleCount]) {
  if (current == nullptr || filter == nullptr || output == nullptr) {
    return false;
  }

  bool reset = !filter->valid || now_us < filter->last_update_us ||
      time_constant_us == 0U;
  uint64_t elapsed_us = 0U;
  if (!reset) {
    elapsed_us = now_us - filter->last_update_us;
    reset = reset_gap_us != 0U && elapsed_us >= reset_gap_us;
  }

  float alpha = 1.0f;
  if (!reset) {
    alpha = static_cast<float>(elapsed_us) /
        static_cast<float>(time_constant_us + elapsed_us);
  }

  for (uint32_t i = 0; i < kEdgeGlowFieldSampleCount; ++i) {
    EdgeGlowFieldColor &filtered = filter->color[i];
    filtered.red += (current[i].red - filtered.red) * alpha;
    filtered.green += (current[i].green - filtered.green) * alpha;
    filtered.blue += (current[i].blue - filtered.blue) * alpha;
    output[i] = filtered;
  }
  filter->valid = true;
  filter->last_update_us = now_us;
  return true;
}

}  // namespace v3dcrt

#endif  // V3DCRT_EDGE_GLOW_FILTER_H
