#ifndef V3DCRT_BLOOM_PIPELINE_H
#define V3DCRT_BLOOM_PIPELINE_H

#include <stdint.h>

namespace v3dcrt {

constexpr uint32_t kBloomDownsampleFactor = 4U;
constexpr float kBloomMaximumFactor = 5.0f;

struct BloomPassPlan {
  bool enabled;
  uint32_t output_width;
  uint32_t output_height;
  uint32_t horizontal_width;
  uint32_t horizontal_height;
  uint32_t blur_width;
  uint32_t blur_height;
  float factor;
};

inline bool BuildBloomPassPlan(uint32_t output_width,
                               uint32_t output_height,
                               bool enabled,
                               float factor,
                               BloomPassPlan *plan) {
  if (plan == nullptr || output_width == 0U || output_height == 0U) {
    return false;
  }

  if (!(factor >= 0.0f)) {
    factor = 0.0f;
  } else if (factor > kBloomMaximumFactor) {
    factor = kBloomMaximumFactor;
  }

  plan->enabled = enabled && factor > 0.0f;
  plan->output_width = output_width;
  plan->output_height = output_height;
  plan->blur_width =
      (output_width + kBloomDownsampleFactor - 1U) /
      kBloomDownsampleFactor;
  plan->blur_height =
      (output_height + kBloomDownsampleFactor - 1U) /
      kBloomDownsampleFactor;
  plan->horizontal_width = plan->blur_width;
  plan->horizontal_height = output_height;
  plan->factor = factor;
  return plan->horizontal_width != 0U &&
      plan->horizontal_height != 0U &&
      plan->blur_width != 0U && plan->blur_height != 0U;
}

}  // namespace v3dcrt

#endif  // V3DCRT_BLOOM_PIPELINE_H
