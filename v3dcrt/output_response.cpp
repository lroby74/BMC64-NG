#include "v3dcrt/output_response.h"

#include <math.h>
#include <stddef.h>

namespace v3dcrt {

namespace {

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

void ResolveOutputResponseRgb565Color(
    uint16_t pixel, const OutputResponseParams &params, float color[3]) {
  color[0] =
      static_cast<float>(((pixel >> 11U) & 0x1fU) * 255U / 31U) / 255.0f;
  color[1] =
      static_cast<float>(((pixel >> 5U) & 0x3fU) * 255U / 63U) / 255.0f;
  color[2] =
      static_cast<float>((pixel & 0x1fU) * 255U / 31U) / 255.0f;
  const float source_luma =
      color[0] * 0.2126f + color[1] * 0.7152f + color[2] * 0.0722f;
  float output_luma = source_luma;
  if (params.level_mapping < 2U) {
    float black = params.black_level;
    float white = params.white_clip;
    if (params.level_mapping == 1U) {
      black = black * black * black;
      const float headroom = 1.0f - white;
      white = 1.0f - headroom * headroom * headroom;
    }
    if (white < black + 0.00390625f) {
      white = black + 0.00390625f;
    }
    output_luma = ClampFloat((source_luma - black) / (white - black),
                             0.0f, 1.0f, 0.0f);
  } else {
    const float luma_squared = source_luma * source_luma;
    const float shadow_target = luma_squared * luma_squared;
    const float toe_luma = source_luma +
        (shadow_target - source_luma) * params.black_level;
    const float headroom = 1.0f - toe_luma;
    const float headroom_squared = headroom * headroom;
    const float highlight_target =
        1.0f - headroom_squared * headroom_squared;
    output_luma = toe_luma +
        (highlight_target - toe_luma) * (1.0f - params.white_clip);
  }

  float chroma[3] = {};
  float positive_chroma = 0.0f;
  float negative_chroma = 0.0f;
  for (uint32_t channel = 0U; channel < 3U; ++channel) {
    chroma[channel] =
        (color[channel] - source_luma) * params.saturation;
    if (chroma[channel] > positive_chroma) {
      positive_chroma = chroma[channel];
    }
    if (-chroma[channel] > negative_chroma) {
      negative_chroma = -chroma[channel];
    }
  }
  float chroma_scale = 1.0f;
  const float positive_scale =
      (1.0f - output_luma) /
      (positive_chroma > 0.000001f ? positive_chroma : 0.000001f);
  const float negative_scale =
      output_luma /
      (negative_chroma > 0.000001f ? negative_chroma : 0.000001f);
  if (positive_scale < chroma_scale) {
    chroma_scale = positive_scale;
  }
  if (negative_scale < chroma_scale) {
    chroma_scale = negative_scale;
  }
  if (chroma_scale < 0.0f) {
    chroma_scale = 0.0f;
  }
  for (uint32_t channel = 0U; channel < 3U; ++channel) {
    color[channel] = ClampFloat(
        output_luma + chroma[channel] * chroma_scale,
        0.0f, 1.0f, 0.0f);
  }
}

void BuildGammaQuantizationThresholds(
    uint32_t maximum, float inverse_gamma, float *thresholds) {
  for (uint32_t boundary = 0U; boundary < maximum; ++boundary) {
    const float encoded =
        (static_cast<float>(boundary) + 0.5f) /
        static_cast<float>(maximum);
    thresholds[boundary] = powf(encoded, inverse_gamma);
  }
}

uint32_t QuantizeGammaChannel(float value, uint32_t maximum,
                              const float *thresholds) {
  uint32_t low = 0U;
  uint32_t high = maximum;
  while (low < high) {
    const uint32_t middle = low + (high - low) / 2U;
    if (value >= thresholds[middle]) {
      low = middle + 1U;
    } else {
      high = middle;
    }
  }
  return low;
}

}  // namespace

OutputResponseParams ResolveOutputResponseParams(
    bool enabled, bool fast, uint32_t level_mapping, float input_gamma,
    float output_gamma, float saturation, float black_level,
    float white_clip) {
  OutputResponseParams params = {
    false, false, 1U, 1.0f, 1.0f, 1.0f, 0.0f, 1.0f
  };
  if (!enabled) {
    return params;
  }
  params.enabled = true;
  params.fast = fast;
  params.level_mapping = level_mapping <= 2U ? level_mapping : 1U;
  params.input_gamma = ClampFloat(input_gamma, 0.1f, 5.0f, 1.0f);
  params.output_gamma = ClampFloat(output_gamma, 0.1f, 5.0f, 1.0f);
  params.saturation = ClampFloat(saturation, 0.0f, 1.0f, 1.0f);
  params.black_level = ClampFloat(black_level, 0.0f, 1.0f, 0.0f);
  params.white_clip = ClampFloat(white_clip, 0.0f, 1.0f, 1.0f);
  return params;
}

bool OutputResponseParamsEqual(const OutputResponseParams &a,
                               const OutputResponseParams &b) {
  return a.enabled == b.enabled && a.fast == b.fast &&
      a.level_mapping == b.level_mapping &&
      a.input_gamma == b.input_gamma &&
      a.output_gamma == b.output_gamma &&
      a.saturation == b.saturation &&
      a.black_level == b.black_level &&
      a.white_clip == b.white_clip;
}

uint16_t ResolveOutputResponseRgb565(
    uint16_t pixel, const OutputResponseParams &params) {
  if (!params.enabled) {
    return pixel;
  }

  float color[3] = {};
  ResolveOutputResponseRgb565Color(pixel, params, color);
  if (!params.fast) {
    const float gamma = params.input_gamma / params.output_gamma;
    for (uint32_t channel = 0U; channel < 3U; ++channel) {
      color[channel] = powf(color[channel], gamma);
    }
  }

  const uint32_t red = static_cast<uint32_t>(color[0] * 31.0f + 0.5f);
  const uint32_t green = static_cast<uint32_t>(color[1] * 63.0f + 0.5f);
  const uint32_t blue = static_cast<uint32_t>(color[2] * 31.0f + 0.5f);
  return static_cast<uint16_t>((red << 11U) | (green << 5U) | blue);
}

bool BuildOutputResponseRgb565Lut(
    const OutputResponseParams &params, uint16_t *lut,
    uint32_t lut_entries) {
  if (lut == NULL || lut_entries < 65536U) {
    return false;
  }
  if (!params.enabled || params.fast) {
    for (uint32_t pixel = 0U; pixel < 65536U; ++pixel) {
      lut[pixel] = ResolveOutputResponseRgb565(
          static_cast<uint16_t>(pixel), params);
    }
    return true;
  }

  const float gamma = params.input_gamma / params.output_gamma;
  const float inverse_gamma = 1.0f / gamma;
  float red_blue_thresholds[31] = {};
  float green_thresholds[63] = {};
  BuildGammaQuantizationThresholds(
      31U, inverse_gamma, red_blue_thresholds);
  BuildGammaQuantizationThresholds(
      63U, inverse_gamma, green_thresholds);
  for (uint32_t pixel = 0U; pixel < 65536U; ++pixel) {
    float color[3] = {};
    ResolveOutputResponseRgb565Color(
        static_cast<uint16_t>(pixel), params, color);
    const uint32_t red = QuantizeGammaChannel(
        color[0], 31U, red_blue_thresholds);
    const uint32_t green = QuantizeGammaChannel(
        color[1], 63U, green_thresholds);
    const uint32_t blue = QuantizeGammaChannel(
        color[2], 31U, red_blue_thresholds);
    lut[pixel] = static_cast<uint16_t>(
        (red << 11U) | (green << 5U) | blue);
  }
  return true;
}

}  // namespace v3dcrt
