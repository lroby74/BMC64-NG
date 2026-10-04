//
// fbl_native_kms.cpp
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "fbl.h"
#if RASPPI == 5
#include "pi5kms/framebuffer_reuse.h"
#include "pi5kms/pi5_kms.h"
#elif RASPPI != 4 || BMX_PI4_LEGACY_DISPLAY
#error fbl_native_kms.cpp requires Pi5 or native-only Pi4
#endif



extern "C" {
#include "third_party/common/circle.h"
}
#include "v3dcrt/v3d_crt.h"
#include "viceoptions.h"

#include <circle/timer.h>
#include <circle/multicore.h>

#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if RASPPI == 4
#define BMX_NATIVE_BOARD_LOG "pi4"
#define BMX_NATIVE_KMS_LOG "pi4kms"
#else
#define BMX_NATIVE_BOARD_LOG "pi5"
#define BMX_NATIVE_KMS_LOG "pi5kms"
#endif

extern "C" {
#include "third_party/common/circle.h"
}

#ifndef ALIGN_UP
#define ALIGN_UP(x,y)  ((x + (y)-1) & ~((y)-1))
#endif

#define RGB565(r,g,b) (((r)>>3)<<11 | ((g)>>2)<<5 | (b)>>3)
#define ARGB(a,r,g,b) ((((uint32_t)(uint8_t)(a)) << 24) | \
                       (((uint32_t)(uint8_t)(r)) << 16) | \
                       (((uint32_t)(uint8_t)(g)) << 8) | \
                       ((uint32_t)(uint8_t)(b)))

#define PI5_FRAMEBUFFER_DEFAULT_DEPTH 16

namespace {

FrameBufferLayer *g_layers[FB_NUM_LAYERS];
uint8_t *g_compose_pixels = nullptr;
uint8_t *g_compose_grezzo = nullptr;
unsigned g_compose_pitch_bytes = 0;
unsigned g_framebuffer_bytes_per_pixel = 2;
int g_effective_width = 0;
int g_effective_height = 0;
bool g_kms_active = false;
bool g_interpolation_enabled = false;
bool g_v3d_crt_enabled = true;
bool g_v3d_crt_enabled_initialized = false;
bool g_v3d_scanline_weight_override_enabled = false;
bool g_v3d_scanline_gap_override_enabled = false;
float g_v3d_scanline_weight_override = 0.0f;
float g_v3d_scanline_gap_override = 0.0f;

v3dcrt::EffectParams g_v3d_effect_params = v3dcrt::DefaultEffectParams();


v3dcrt::ShaderPreset g_bmc_v3d_preset = v3dcrt::kShaderOff;
int g_bmc_v3d_saltata = -1;
FrameBufferLayer *g_capture_v3d_layer = nullptr;
constexpr unsigned kKmsBufferCount = 2;
pi5kms::Framebuffer g_kms_framebuffers[kKmsBufferCount] = {};
pi5kms::Framebuffer g_kms_hwscale_framebuffers[FB_NUM_LAYERS][kKmsBufferCount] = {};
bool g_kms_hwscale_framebuffer_valid[FB_NUM_LAYERS][kKmsBufferCount] = {};
unsigned g_kms_hwscale_front_buffer_index[FB_NUM_LAYERS] = {};
bool g_kms_hwscale_front_buffer_valid[FB_NUM_LAYERS] = {};
unsigned g_kms_front_buffer_index = 0;

struct DirtyRect {
  int left;
  int top;
  int right;
  int bottom;
  bool valid;
};

struct SoftwareLayerSnapshot {
  FrameBufferLayer *layer;
  uint32_t generation;
  int z_order;
  int dst_x;
  int dst_y;
  int dst_w;
  int dst_h;
  bool showing;
};

SoftwareLayerSnapshot g_software_layers[FB_NUM_LAYERS] = {};
DirtyRect g_software_kms_pending[kKmsBufferCount] = {};
bool g_software_composition_valid = false;
bool g_software_composition_interpolation = false;

void AddDirtyRect(DirtyRect *dirty, int x, int y, int width, int height) {
  if (dirty == nullptr || width <= 0 || height <= 0) {
    return;
  }
  const int left = x > 0 ? x : 0;
  const int top = y > 0 ? y : 0;
  const int right = x + width < g_effective_width
                        ? x + width : g_effective_width;
  const int bottom = y + height < g_effective_height
                         ? y + height : g_effective_height;
  if (left >= right || top >= bottom) {
    return;
  }
  if (!dirty->valid) {
    *dirty = {left, top, right, bottom, true};
    return;
  }
  if (left < dirty->left) dirty->left = left;
  if (top < dirty->top) dirty->top = top;
  if (right > dirty->right) dirty->right = right;
  if (bottom > dirty->bottom) dirty->bottom = bottom;
}

void InvalidateSoftwareComposition() {
  g_software_composition_valid = false;
}

struct IndexedLayerShadow {
  uint8_t *pixels;
  u32 width;
  u32 height;
  s32 source_x;
  s32 source_y;
  u32 palette_signature;
  bool valid;
};

IndexedLayerShadow
    g_kms_indexed_layer_shadows[FB_NUM_LAYERS][kKmsBufferCount] = {};

constexpr u32 kV3dPresentStatsFrames = 60;

struct V3dPresentStats {
  bool active;
  bool complete;
  u32 overlay_mask;
  u64 window_start_us;
  u32 frames;
  u32 overlay_prep_min_us;
  u32 overlay_prep_max_us;
  u64 overlay_prep_total_us;
  u32 render_min_us;
  u32 render_max_us;
  u64 render_total_us;
  u32 present_min_us;
  u32 present_max_us;
  u64 present_total_us;
  u32 total_min_us;
  u32 total_max_us;
  u64 total_total_us;
};

V3dPresentStats g_v3d_present_stats = {};

enum V3dFallbackReason {
  kV3dFallbackKmsInactive = 0,
  kV3dFallbackUnavailable,
  kV3dFallbackInvalidScreen,
  kV3dFallbackLayerCount,
  kV3dFallbackOverlayActive,
  kV3dFallbackUnsupportedLayer,
  kV3dFallbackUnsupportedFormat,
  kV3dFallbackSourceGeometry,
  kV3dFallbackDestinationGeometry,
  kV3dFallbackRenderFailed,
  kV3dFallbackPresentFailed,
  kV3dFallbackCount
};

struct V3dFallbackStats {
  u32 total;
  u32 count[kV3dFallbackCount];
  bool logged[kV3dFallbackCount];
};

V3dFallbackStats g_v3d_fallback_stats = {};

void accumulate_present_stat(u32 value, u32 count,
                             u32 *minimum, u32 *maximum, u64 *total) {
  if (count == 0 || value < *minimum) {
    *minimum = value;
  }
  if (count == 0 || value > *maximum) {
    *maximum = value;
  }
  *total += value;
}

u32 present_stats_average(u64 total, u32 frames) {
  return frames == 0 ? 0 : (u32)(total / frames);
}

u32 present_stats_fps_x100(u32 frames, u64 elapsed_us) {
  if (frames == 0 || elapsed_us == 0) {
    return 0;
  }
  const u64 fps_x100 =
      (u64)frames * 100U * 1000000U / elapsed_us;
  return fps_x100 > 0xFFFFFFFFULL ? 0xFFFFFFFFU : (u32)fps_x100;
}

void reset_v3d_present_stats() {
  memset(&g_v3d_present_stats, 0, sizeof g_v3d_present_stats);
}

void record_v3d_present_stats(u32 overlay_mask,
                              u64 frame_start_us,
                              u64 overlay_prep_done_us,
                              u64 render_done_us,
                              u64 frame_done_us) {
  if ((overlay_mask & FB_LAYER_MASK(FB_LAYER_STATUS)) == 0) {
    if (g_v3d_present_stats.active ||
        g_v3d_present_stats.complete ||
        g_v3d_present_stats.overlay_mask != 0) {
      reset_v3d_present_stats();
    }
    return;
  }
  if (g_v3d_present_stats.overlay_mask != overlay_mask ||
      (!g_v3d_present_stats.active &&
       !g_v3d_present_stats.complete)) {
    reset_v3d_present_stats();
    g_v3d_present_stats.active = true;
    g_v3d_present_stats.overlay_mask = overlay_mask;
    g_v3d_present_stats.window_start_us = frame_done_us;
    return;
  }
  if (!g_v3d_present_stats.active) {
    return;
  }

  const u32 count = g_v3d_present_stats.frames;
  const u32 overlay_prep_us =
      (u32)(overlay_prep_done_us - frame_start_us);
  const u32 render_us =
      (u32)(render_done_us - overlay_prep_done_us);
  const u32 present_us =
      (u32)(frame_done_us - render_done_us);
  const u32 total_us = (u32)(frame_done_us - frame_start_us);
  accumulate_present_stat(
      overlay_prep_us, count,
      &g_v3d_present_stats.overlay_prep_min_us,
      &g_v3d_present_stats.overlay_prep_max_us,
      &g_v3d_present_stats.overlay_prep_total_us);
  accumulate_present_stat(
      render_us, count,
      &g_v3d_present_stats.render_min_us,
      &g_v3d_present_stats.render_max_us,
      &g_v3d_present_stats.render_total_us);
  accumulate_present_stat(
      present_us, count,
      &g_v3d_present_stats.present_min_us,
      &g_v3d_present_stats.present_max_us,
      &g_v3d_present_stats.present_total_us);
  accumulate_present_stat(
      total_us, count,
      &g_v3d_present_stats.total_min_us,
      &g_v3d_present_stats.total_max_us,
      &g_v3d_present_stats.total_total_us);

  ++g_v3d_present_stats.frames;
  if (g_v3d_present_stats.frames < kV3dPresentStatsFrames) {
    return;
  }

  u64 elapsed_us =
      frame_done_us - g_v3d_present_stats.window_start_us;
  if (elapsed_us == 0) {
    elapsed_us = 1;
  }
  printf("boot: " BMX_NATIVE_BOARD_LOG " fbl v3d stats frames=%u overlay_mask=0x%02x "
         "status=%u ui=%u scope=end-to-end elapsed_ms=%u fps_x100=%u "
         "overlay_prep_us=%u/%u/%u render_us=%u/%u/%u "
         "present_us=%u/%u/%u total_us=%u/%u/%u\r\n",
         g_v3d_present_stats.frames,
         g_v3d_present_stats.overlay_mask,
         (g_v3d_present_stats.overlay_mask &
          FB_LAYER_MASK(FB_LAYER_STATUS)) != 0 ? 1U : 0U,
         (g_v3d_present_stats.overlay_mask &
          FB_LAYER_MASK(FB_LAYER_UI)) != 0 ? 1U : 0U,
         (u32)(elapsed_us / 1000U),
         present_stats_fps_x100(
             g_v3d_present_stats.frames, elapsed_us),
         g_v3d_present_stats.overlay_prep_min_us,
         present_stats_average(
             g_v3d_present_stats.overlay_prep_total_us,
             g_v3d_present_stats.frames),
         g_v3d_present_stats.overlay_prep_max_us,
         g_v3d_present_stats.render_min_us,
         present_stats_average(
             g_v3d_present_stats.render_total_us,
             g_v3d_present_stats.frames),
         g_v3d_present_stats.render_max_us,
         g_v3d_present_stats.present_min_us,
         present_stats_average(
             g_v3d_present_stats.present_total_us,
             g_v3d_present_stats.frames),
         g_v3d_present_stats.present_max_us,
         g_v3d_present_stats.total_min_us,
         present_stats_average(
             g_v3d_present_stats.total_total_us,
             g_v3d_present_stats.frames),
         g_v3d_present_stats.total_max_us);
  g_v3d_present_stats.active = false;
  g_v3d_present_stats.complete = true;
}

uint32_t palette_signature_rgb565(const uint16_t *palette) {
  uint32_t signature = 2166136261U;
  for (unsigned i = 0; i < 256; ++i) {
    signature ^= palette[i];
    signature *= 16777619U;
  }
  return signature;
}

uint32_t palette_signature_argb(const uint32_t *palette) {
  uint32_t signature = 2166136261U;
  for (unsigned i = 0; i < 256; ++i) {
    signature ^= palette[i];
    signature *= 16777619U;
  }
  return signature;
}

void reset_indexed_layer_shadow(unsigned slot, unsigned buffer_index) {
  if (slot >= FB_NUM_LAYERS || buffer_index >= kKmsBufferCount) {
    return;
  }

  IndexedLayerShadow *shadow =
      &g_kms_indexed_layer_shadows[slot][buffer_index];
  free(shadow->pixels);
  memset(shadow, 0, sizeof *shadow);
}

bool ensure_indexed_layer_shadow(unsigned slot, unsigned buffer_index,
                                 u32 width, u32 height) {
  if (slot >= FB_NUM_LAYERS || buffer_index >= kKmsBufferCount ||
      width == 0 || height == 0) {
    return false;
  }

  IndexedLayerShadow *shadow =
      &g_kms_indexed_layer_shadows[slot][buffer_index];
  if (shadow->pixels != nullptr &&
      shadow->width == width &&
      shadow->height == height) {
    return true;
  }

  reset_indexed_layer_shadow(slot, buffer_index);
  shadow->pixels =
      (uint8_t *)malloc((size_t)width * (size_t)height);
  if (shadow->pixels == nullptr) {
    return false;
  }
  shadow->width = width;
  shadow->height = height;
  return true;
}

void convert_indexed_argb_rows(const uint8_t *source,
                               u32 source_pitch,
                               u32 source_x,
                               u32 source_y,
                               u32 width,
                               u32 first_row,
                               u32 row_count,
                               const uint32_t *palette,
                               pi5kms::Framebuffer *framebuffer) {
  for (u32 y = first_row; y < first_row + row_count; ++y) {
    const uint8_t *src_row =
        source + (source_y + y) * source_pitch + source_x;
    uint32_t *dst_row =
        (uint32_t *)(framebuffer->pixels + y * framebuffer->pitch);
    for (u32 x = 0; x < width; ++x) {
      dst_row[x] = palette[src_row[x]];
    }
  }
}

bool copy_status_layer_incremental(
    unsigned slot,
    unsigned buffer_index,
    const uint8_t *source,
    u32 source_pitch,
    u32 source_x,
    u32 source_y,
    u32 width,
    u32 height,
    const uint32_t *palette,
    u32 palette_signature,
    bool force_full,
    pi5kms::Framebuffer *framebuffer) {
  // Each KMS back buffer keeps its own indexed snapshot. This lets the
  // status plane alternate safely while converting only rows that changed.
  if (!ensure_indexed_layer_shadow(
          slot, buffer_index, width, height)) {
    convert_indexed_argb_rows(
        source, source_pitch, source_x, source_y, width, 0, height,
        palette, framebuffer);
    pi5kms::FlushFramebuffer(*framebuffer);
    return true;
  }

  IndexedLayerShadow *shadow =
      &g_kms_indexed_layer_shadows[slot][buffer_index];
  force_full = force_full || !shadow->valid ||
      shadow->source_x != (s32)source_x ||
      shadow->source_y != (s32)source_y ||
      shadow->palette_signature != palette_signature;
  if (force_full) {
    convert_indexed_argb_rows(
        source, source_pitch, source_x, source_y, width, 0, height,
        palette, framebuffer);
    for (u32 y = 0; y < height; ++y) {
      memcpy(shadow->pixels + y * width,
             source + (source_y + y) * source_pitch + source_x,
             width);
    }
    pi5kms::FlushFramebuffer(*framebuffer);
  } else {
    u32 flush_start = height;
    for (u32 y = 0; y < height; ++y) {
      const uint8_t *src_row =
          source + (source_y + y) * source_pitch + source_x;
      uint8_t *shadow_row = shadow->pixels + y * width;
      if (memcmp(src_row, shadow_row, width) == 0) {
        if (flush_start != height) {
          pi5kms::FlushFramebufferRows(
              *framebuffer, flush_start, y - flush_start);
          flush_start = height;
        }
        continue;
      }

      convert_indexed_argb_rows(
          source, source_pitch, source_x, source_y, width, y, 1,
          palette, framebuffer);
      memcpy(shadow_row, src_row, width);
      if (flush_start == height) {
        flush_start = y;
      }
    }
    if (flush_start != height) {
      pi5kms::FlushFramebufferRows(
          *framebuffer, flush_start, height - flush_start);
    }
  }

  shadow->source_x = (s32)source_x;
  shadow->source_y = (s32)source_y;
  shadow->palette_signature = palette_signature;
  shadow->valid = true;
  return true;
}

unsigned param_x100(float value) {
  if (value <= 0.0f) {
    return 0;
  }
  if (value > 1000000.0f) {
    return 100000000U;
  }
  return (unsigned)(value * 100.0f + 0.5f);
}

struct V3dMenuParamLogState {
  bool valid;
  struct bmx_crt_effect_params params;
};

V3dMenuParamLogState g_v3d_menu_param_log = {};

bool should_log_v3d_menu_params(
    const struct bmx_crt_effect_params &params) {
  if (g_v3d_menu_param_log.valid &&
      memcmp(&g_v3d_menu_param_log.params, &params, sizeof params) == 0) {
    return false;
  }

  g_v3d_menu_param_log.valid = true;
  memcpy(&g_v3d_menu_param_log.params, &params, sizeof params);
  return true;
}

static uint16_t default_pal_565[256] = {
  RGB565(0x00, 0x00, 0x00),
  RGB565(0xFF, 0xFF, 0xFF),
  RGB565(0xFF, 0x00, 0x00),
  RGB565(0x70, 0xa4, 0xb2),
  RGB565(0x6f, 0x3d, 0x86),
  RGB565(0x58, 0x8d, 0x43),
  RGB565(0x35, 0x28, 0x79),
  RGB565(0xb8, 0xc7, 0x6f),
  RGB565(0x6f, 0x4f, 0x25),
  RGB565(0x43, 0x39, 0x00),
  RGB565(0x9a, 0x67, 0x59),
  RGB565(0x44, 0x44, 0x44),
  RGB565(0x6c, 0x6c, 0x6c),
  RGB565(0x9a, 0xd2, 0x84),
  RGB565(0x6c, 0x5e, 0xb5),
  RGB565(0x95, 0x95, 0x95),
};

static uint32_t default_pal_argb[256] = {
  ARGB(0xFF, 0x00, 0x00, 0x00),
  ARGB(0xFF, 0xFF, 0xFF, 0xFF),
  ARGB(0xFF, 0xFF, 0x00, 0x00),
  ARGB(0xFF, 0x70, 0xa4, 0xb2),
  ARGB(0xFF, 0x6f, 0x3d, 0x86),
  ARGB(0xFF, 0x58, 0x8d, 0x43),
  ARGB(0xFF, 0x35, 0x28, 0x79),
  ARGB(0xFF, 0xb8, 0xc7, 0x6f),
  ARGB(0xFF, 0x6f, 0x4f, 0x25),
  ARGB(0xFF, 0x43, 0x39, 0x00),
  ARGB(0xFF, 0x9a, 0x67, 0x59),
  ARGB(0xFF, 0x44, 0x44, 0x44),
  ARGB(0xFF, 0x6c, 0x6c, 0x6c),
  ARGB(0xFF, 0x9a, 0xd2, 0x84),
  ARGB(0xFF, 0x6c, 0x5e, 0xb5),
  ARGB(0xFF, 0x95, 0x95, 0x95),
  ARGB(0x00, 0x00, 0x00, 0x00),
};

int min_int(int a, int b) {
  return a < b ? a : b;
}

int max_int(int a, int b) {
  return a > b ? a : b;
}

const char *v3d_fallback_reason_name(V3dFallbackReason reason) {
  switch (reason) {
    case kV3dFallbackKmsInactive:
      return "kms-inactive";
    case kV3dFallbackUnavailable:
      return "v3d-unavailable";
    case kV3dFallbackInvalidScreen:
      return "invalid-screen";
    case kV3dFallbackLayerCount:
      return "layer-count";
    case kV3dFallbackOverlayActive:
      return "overlay-active";
    case kV3dFallbackUnsupportedLayer:
      return "unsupported-layer";
    case kV3dFallbackUnsupportedFormat:
      return "unsupported-format";
    case kV3dFallbackSourceGeometry:
      return "source-geometry";
    case kV3dFallbackDestinationGeometry:
      return "destination-geometry";
    case kV3dFallbackRenderFailed:
      return "render-failed";
    case kV3dFallbackPresentFailed:
      return "present-failed";
    default:
      return "unknown";
  }
}

void record_v3d_fallback(V3dFallbackReason reason) {
  if (reason < 0 || reason >= kV3dFallbackCount) {
    return;
  }

  ++g_v3d_fallback_stats.total;
  ++g_v3d_fallback_stats.count[reason];
  if (!g_v3d_fallback_stats.logged[reason]) {
    printf("boot: v3dcrt fallback reason=%s count=%u total=%u\r\n",
           v3d_fallback_reason_name(reason),
           g_v3d_fallback_stats.count[reason],
           g_v3d_fallback_stats.total);
    g_v3d_fallback_stats.logged[reason] = true;
  }
}

void fit_to_available_preserving_aspect(int *width, int *height,
                                        int avail_width, int avail_height) {
  if (*width <= 0 || *height <= 0 ||
      avail_width <= 0 || avail_height <= 0) {
    return;
  }

  int64_t height_for_full_width =
      ((int64_t) avail_width * (int64_t) *height) / (int64_t) *width;
  if (height_for_full_width <= avail_height) {
    *height = (int) height_for_full_width;
    *width = avail_width;
  } else {
    *width = (int) (((int64_t) avail_height * (int64_t) *width) /
                    (int64_t) *height);
    *height = avail_height;
  }
}

unsigned sanitize_framebuffer_depth(unsigned depth) {
  if (depth == 32) {
    return 32;
  }
  return PI5_FRAMEBUFFER_DEFAULT_DEPTH;
}

unsigned bytes_per_pixel_from_depth(unsigned depth) {
  return sanitize_framebuffer_depth(depth) / 8;
}

unsigned framebuffer_bytes_per_pixel(CBcmFrameBuffer *screen,
                                     unsigned requested_depth) {
  unsigned width = screen->GetWidth();
  unsigned pitch = screen->GetPitch();
  if (width != 0 && pitch != 0) {
    unsigned pitch_bytes_per_pixel = pitch / width;
    if (pitch_bytes_per_pixel == 2 || pitch_bytes_per_pixel == 4) {
      return pitch_bytes_per_pixel;
    }
  }

  unsigned depth_bytes_per_pixel = screen->GetDepth() / 8;
  if (depth_bytes_per_pixel == 2 || depth_bytes_per_pixel == 4) {
    return depth_bytes_per_pixel;
  }

  return bytes_per_pixel_from_depth(requested_depth);
}

uint16_t argb_to_rgb565(uint32_t argb) {
  uint8_t r = (uint8_t) (argb >> 16);
  uint8_t g = (uint8_t) (argb >> 8);
  uint8_t b = (uint8_t) argb;

  return ((uint16_t) (r >> 3) << 11) |
         ((uint16_t) (g >> 2) << 5) |
         (uint16_t) (b >> 3);
}

uint32_t rgb565_to_argb8888(uint16_t rgb) {
  uint32_t r = (uint32_t) (rgb >> 11) & 0x1F;
  uint32_t g = (uint32_t) (rgb >> 5) & 0x3F;
  uint32_t b = (uint32_t) rgb & 0x1F;

  r = (r << 3) | (r >> 2);
  g = (g << 2) | (g >> 4);
  b = (b << 3) | (b >> 2);

  return 0xFF000000u | (r << 16) | (g << 8) | b;
}

static inline unsigned lerp_channel(unsigned a, unsigned b, unsigned frac) {
  return ((a * (256 - frac)) + (b * frac) + 128) >> 8;
}

static inline unsigned argb_a(uint32_t argb) {
  return (argb >> 24) & 0xFF;
}

static inline unsigned argb_r(uint32_t argb) {
  return (argb >> 16) & 0xFF;
}

static inline unsigned argb_g(uint32_t argb) {
  return (argb >> 8) & 0xFF;
}

static inline unsigned argb_b(uint32_t argb) {
  return argb & 0xFF;
}

struct Rgb565Channels {
  unsigned r;
  unsigned g;
  unsigned b;
};

static inline Rgb565Channels unpack_rgb565(uint16_t rgb) {
  return {
    (unsigned) (rgb >> 11) & 0x1F,
    (unsigned) (rgb >> 5) & 0x3F,
    (unsigned) rgb & 0x1F
  };
}

uint16_t bilinear_rgb565(uint16_t c00,
                         uint16_t c10,
                         uint16_t c01,
                         uint16_t c11,
                         unsigned frac_x,
                         unsigned frac_y) {
  Rgb565Channels p00 = unpack_rgb565(c00);
  Rgb565Channels p10 = unpack_rgb565(c10);
  Rgb565Channels p01 = unpack_rgb565(c01);
  Rgb565Channels p11 = unpack_rgb565(c11);

  unsigned r0 = lerp_channel(p00.r, p10.r, frac_x);
  unsigned g0 = lerp_channel(p00.g, p10.g, frac_x);
  unsigned b0 = lerp_channel(p00.b, p10.b, frac_x);
  unsigned r1 = lerp_channel(p01.r, p11.r, frac_x);
  unsigned g1 = lerp_channel(p01.g, p11.g, frac_x);
  unsigned b1 = lerp_channel(p01.b, p11.b, frac_x);

  return (uint16_t) ((lerp_channel(r0, r1, frac_y) << 11) |
                     (lerp_channel(g0, g1, frac_y) << 5) |
                     lerp_channel(b0, b1, frac_y));
}

uint32_t bilinear_argb8888(uint32_t c00,
                           uint32_t c10,
                           uint32_t c01,
                           uint32_t c11,
                           unsigned frac_x,
                           unsigned frac_y) {
  unsigned a0 = lerp_channel(argb_a(c00), argb_a(c10), frac_x);
  unsigned r0 = lerp_channel(argb_r(c00), argb_r(c10), frac_x);
  unsigned g0 = lerp_channel(argb_g(c00), argb_g(c10), frac_x);
  unsigned b0 = lerp_channel(argb_b(c00), argb_b(c10), frac_x);
  unsigned a1 = lerp_channel(argb_a(c01), argb_a(c11), frac_x);
  unsigned r1 = lerp_channel(argb_r(c01), argb_r(c11), frac_x);
  unsigned g1 = lerp_channel(argb_g(c01), argb_g(c11), frac_x);
  unsigned b1 = lerp_channel(argb_b(c01), argb_b(c11), frac_x);

  unsigned a = lerp_channel(a0, a1, frac_y);
  unsigned r = lerp_channel(r0, r1, frac_y);
  unsigned g = lerp_channel(g0, g1, frac_y);
  unsigned b = lerp_channel(b0, b1, frac_y);

  return (a << 24) | (r << 16) | (g << 8) | b;
}

void write_rgb565_pixel(uint8_t *row, int x, uint16_t rgb) {
  if (g_framebuffer_bytes_per_pixel == 2) {
    ((uint16_t *) row)[x] = rgb;
  } else {
    ((uint32_t *) row)[x] = rgb565_to_argb8888(rgb);
  }
}

void write_argb_pixel(uint8_t *row, int x, uint32_t argb) {
  if (g_framebuffer_bytes_per_pixel == 2) {
    ((uint16_t *) row)[x] = argb_to_rgb565(argb);
  } else {
    ((uint32_t *) row)[x] = argb;
  }
}

uint32_t read_argb_pixel(const uint8_t *row, int x) {
  if (g_framebuffer_bytes_per_pixel == 2) {
    return rgb565_to_argb8888(((const uint16_t *) row)[x]);
  }
  return ((const uint32_t *) row)[x];
}

static inline unsigned alpha_blend_channel(unsigned src,
                                           unsigned dst,
                                           unsigned alpha) {
  return (src * alpha + dst * (255 - alpha) + 127) / 255;
}

void blend_argb_pixel(uint8_t *row, int x, uint32_t argb) {
  unsigned alpha = argb_a(argb);
  if (alpha == 0) {
    return;
  }
  if (alpha == 255) {
    write_argb_pixel(row, x, argb);
    return;
  }

  uint32_t dst = read_argb_pixel(row, x);
  unsigned r = alpha_blend_channel(argb_r(argb), argb_r(dst), alpha);
  unsigned g = alpha_blend_channel(argb_g(argb), argb_g(dst), alpha);
  unsigned b = alpha_blend_channel(argb_b(argb), argb_b(dst), alpha);
  write_argb_pixel(row, x, 0xFF000000u | (r << 16) | (g << 8) | b);
}

int effective_screen_width(CBcmFrameBuffer *screen,
                           unsigned bytes_per_pixel) {
  int width = (int) screen->GetWidth();
  if (screen->GetPitch() != 0 && bytes_per_pixel != 0) {
    width = min_int(width, (int) (screen->GetPitch() / bytes_per_pixel));
  }
  return width;
}

int effective_screen_height(CBcmFrameBuffer *screen) {
  int height = (int) screen->GetHeight();
  if (screen->GetPitch() != 0 && screen->GetSize() != 0) {
    height = min_int(height, (int) (screen->GetSize() / screen->GetPitch()));
  }
  return height;
}

void sort_layers(FrameBufferLayer **layers, unsigned count) {
  for (unsigned i = 0; i < count; i++) {
    for (unsigned j = i + 1; j < count; j++) {
      if (layers[j]->GetLayer() < layers[i]->GetLayer()) {
        FrameBufferLayer *tmp = layers[i];
        layers[i] = layers[j];
        layers[j] = tmp;
      }
    }
  }
}

unsigned kms_back_buffer_index() {
  return g_kms_front_buffer_index ^ 1U;
}

pi5kms::Plane make_kms_framebuffer_plane(const pi5kms::Framebuffer &fb) {
  pi5kms::Plane plane = {
    (u32)(uintptr)fb.pixels,
    fb.pitch,
    fb.width,
    fb.height,
    fb.depth,
    pi5kms::kPixelFormatRgb565,
    pi5kms::kScaleFilterNearest,
    {0, 0, fb.width, fb.height},
    {0, 0, fb.width, fb.height}
  };

  return plane;
}

#if RASPPI == 4
bool prepare_pi4_v3d_target(pi5kms::Framebuffer &fb,
                            u32 destination_x,
                            u32 destination_y,
                            u32 destination_width,
                            u32 destination_height,
                            uint8_t **render_pixels,
                            u32 *render_width,
                            u32 *render_height) {
  if (render_pixels == nullptr || render_width == nullptr ||
      render_height == nullptr || fb.pixels == nullptr || fb.depth != 16U ||
      fb.pitch < fb.width * sizeof(u16) || destination_width == 0U ||
      destination_height == 0U || destination_x >= fb.width ||
      destination_y >= fb.height ||
      destination_width > fb.width - destination_x ||
      destination_height > fb.height - destination_y) {
    return false;
  }

  const u32 row_bytes = fb.width * sizeof(u16);
  const u32 left_bytes = destination_x * sizeof(u16);
  const u32 right_x = destination_x + destination_width;
  const u32 right_bytes = (fb.width - right_x) * sizeof(u16);
  const u32 bottom_y = destination_y + destination_height;

  for (u32 y = 0U; y < destination_y; ++y) {
    memset(fb.pixels + y * fb.pitch, 0, row_bytes);
  }
  for (u32 y = destination_y; y < bottom_y; ++y) {
    uint8_t *row = fb.pixels + y * fb.pitch;
    if (left_bytes != 0U) {
      memset(row, 0, left_bytes);
    }
    if (right_bytes != 0U) {
      memset(row + right_x * sizeof(u16), 0, right_bytes);
    }
  }
  for (u32 y = bottom_y; y < fb.height; ++y) {
    memset(fb.pixels + y * fb.pitch, 0, row_bytes);
  }

  *render_pixels = fb.pixels + destination_y * fb.pitch +
                   destination_x * sizeof(u16);
  *render_width = destination_width;
  *render_height = destination_height;
  return true;
}

bool build_pi4_v3d_plane(const v3dcrt::OutputReadback &readback,
                         s32 destination_x,
                         s32 destination_y,
                         u32 destination_width,
                         u32 destination_height,
                         int screen_w,
                         int screen_h,
                         pi5kms::Plane *plane) {
  if (plane == nullptr ||
      readback.framebuffer_bus_address == 0U ||
      readback.pixels == nullptr || readback.depth != 16U ||
      readback.width == 0U || readback.height == 0U ||
      readback.pitch < readback.width * sizeof(u16) ||
      destination_x < 0 || destination_y < 0 ||
      destination_width == 0U || destination_height == 0U ||
      screen_w <= 0 || screen_h <= 0 ||
      static_cast<u32>(destination_x) > static_cast<u32>(screen_w) ||
      static_cast<u32>(destination_y) > static_cast<u32>(screen_h) ||
      destination_width >
          static_cast<u32>(screen_w) - static_cast<u32>(destination_x) ||
      destination_height >
          static_cast<u32>(screen_h) - static_cast<u32>(destination_y)) {
    return false;
  }

  *plane = {
    readback.framebuffer_bus_address,
    readback.pitch,
    readback.width,
    readback.height,
    readback.depth,
    pi5kms::kPixelFormatRgb565,
    g_interpolation_enabled ? pi5kms::kScaleFilterMitchell
                            : pi5kms::kScaleFilterNearest,
    {0, 0, readback.width, readback.height},
    {destination_x, destination_y,
     destination_width, destination_height}
  };
  return true;
}

bool copy_pi4_v3d_to_kms_fallback(
    const v3dcrt::OutputReadback &readback,
    pi5kms::Framebuffer &fb,
    u32 destination_x,
    u32 destination_y,
    u32 destination_width,
    u32 destination_height) {
  if (readback.pixels == nullptr ||
      readback.depth != 16U ||
      readback.width != destination_width ||
      readback.height != destination_height ||
      readback.pitch < readback.width * sizeof(u16)) {
    return false;
  }

  uint8_t *destination = nullptr;
  u32 prepared_width = 0U;
  u32 prepared_height = 0U;
  if (!prepare_pi4_v3d_target(
          fb,
          destination_x, destination_y,
          destination_width, destination_height,
          &destination, &prepared_width, &prepared_height) ||
      prepared_width != readback.width ||
      prepared_height != readback.height) {
    return false;
  }

  const u32 row_bytes = readback.width * sizeof(u16);
  for (u32 y = 0U; y < readback.height; ++y) {
    memcpy(destination + y * fb.pitch,
           readback.pixels + y * readback.pitch, row_bytes);
  }
  pi5kms::FlushFramebuffer(fb);
  return true;
}
#endif







pi5kms::Plane g_bmc_piani[FB_NUM_LAYERS] = {};
volatile unsigned g_bmc_quanti_piani = 0;

bool presenta_e_ricorda(const pi5kms::Plane *planes, unsigned plane_count,
                        u32 display_width, u32 display_height,
                        bool wait_for_vblank) {
  if (!pi5kms::PresentScanout(planes, plane_count, display_width,
                              display_height, wait_for_vblank)) {
    return false;
  }
  const unsigned n =
      plane_count < (unsigned)FB_NUM_LAYERS ? plane_count
                                            : (unsigned)FB_NUM_LAYERS;
  for (unsigned i = 0; i < n; ++i) {
    g_bmc_piani[i] = planes[i];
  }
  g_bmc_quanti_piani = n;
  return true;
}

bool present_kms_framebuffer(unsigned buffer_index, bool wait_for_vblank) {
  if (buffer_index >= kKmsBufferCount) {
    return false;
  }

  pi5kms::Framebuffer &fb = g_kms_framebuffers[buffer_index];
  pi5kms::Plane plane = make_kms_framebuffer_plane(fb);
  if (!presenta_e_ricorda(    &plane, 1, fb.width, fb.height,
                              wait_for_vblank)) {
    return false;
  }

  g_kms_front_buffer_index = buffer_index;
  g_capture_v3d_layer = nullptr;
  return true;
}

void run_v3d_boot_test(v3dcrt::BootTestMode mode) {
  if (!g_kms_active || mode == v3dcrt::kBootTestOff) {
    return;
  }

  const unsigned buffer_index = kms_back_buffer_index();
  pi5kms::Framebuffer &fb = g_kms_framebuffers[buffer_index];
  v3dcrt::OutputFramebuffer target = {
    &fb,
    fb.pixels,
    fb.width,
    fb.height,
    fb.pitch,
    fb.depth,
    v3dcrt::kPixelFormatRgb565,
    fb.width,
    fb.height,
    {0, 0, fb.width, fb.height},
    false,
    nullptr,
    true,
    nullptr
  };

  if (v3dcrt::RunBootTest(target)) {
    present_kms_framebuffer(buffer_index, true);
  }
}

void destroy_kms_scanout_framebuffers() {
  for (unsigned i = 0; i < kKmsBufferCount; ++i) {
    pi5kms::DestroyFramebuffer(&g_kms_framebuffers[i]);
  }
  g_kms_front_buffer_index = 0;
}

bool ensure_hwscale_framebuffer(unsigned slot, unsigned buffer_index,
                                u32 width, u32 height,
                                u32 depth, bool *recreated) {
  if (slot >= FB_NUM_LAYERS || buffer_index >= kKmsBufferCount) {
    return false;
  }

  if (recreated != nullptr) {
    *recreated = false;
  }

  pi5kms::Framebuffer *fb = &g_kms_hwscale_framebuffers[slot][buffer_index];
  if (fb->pixels != nullptr &&
      pi5kms::CanReuseFramebuffer(fb->width, fb->height, fb->depth,
                                  width, height, depth)) {
    return true;
  }

  u32 allocation_width = width;
  u32 allocation_height = height;
  if (fb->pixels != nullptr && fb->depth == depth) {
    // Never trade one existing dimension for another. Keeping capacity
    // monotonic prevents alternating aspect ratios from recreating large DMA
    // blocks that Circle cannot return to its bucket allocator.
    allocation_width = pi5kms::ExpandedFramebufferDimension(
        fb->width, allocation_width);
    allocation_height = pi5kms::ExpandedFramebufferDimension(
        fb->height, allocation_height);
  }

  if (fb->pixels != nullptr) {
    pi5kms::DestroyFramebuffer(fb);
    g_kms_hwscale_framebuffer_valid[slot][buffer_index] = false;
    reset_indexed_layer_shadow(slot, buffer_index);
  }

  if (!pi5kms::CreateFramebuffer(allocation_width, allocation_height,
                                 depth, fb)) {
    return false;
  }
  g_kms_hwscale_framebuffer_valid[slot][buffer_index] = false;
  if (recreated != nullptr) {
    *recreated = true;
  }
  return true;
}

} // namespace



//



//














#include <circle/chargenerator.h>
#include <circle/synchronize.h>

extern "C" {



volatile unsigned fbl_composizioni = 0;





uint32_t fbl_job = 0;
uint32_t fbl_done = 0;
volatile int fbl_worker_alive = 0;
void fbl_worker_ready(void) { fbl_worker_alive = 1; }


void fbl_worker_run(void);


unsigned raspi_disp_w = 0, raspi_disp_h = 0;
unsigned raspi_fb_ask_w = 0, raspi_fb_ask_h = 0;
unsigned raspi_fb_got_w = 0, raspi_fb_got_h = 0;
unsigned raspi_fb_pitch = 0, raspi_fb_size = 0;
unsigned raspi_fb_displays = 0;
unsigned raspi_fb_display = 0;


volatile unsigned long fbl_periodo_macchina_us = 0;
unsigned raspi_fb_pages = 0;
unsigned raspi_fb_bpp = 0;
unsigned raspi_fb_worker = 0;
unsigned raspi_fb_dma = 0;


unsigned raspi_disp_fisico_milli = 0;






int fbl_stato_scalatore(void) { return g_kms_active ? 1 : 0; }
int fbl_stato_v3d(void) { return g_capture_v3d_layer != nullptr ? 1 : 0; }

}

static void bmc_leggi_forma_edid(void);

namespace {



uint8_t *g_bmc_fw_pixels = nullptr;
unsigned g_bmc_fw_pitch = 0;
unsigned g_bmc_fw_bpp = 0;
unsigned g_bmc_fw_w = 0;
unsigned g_bmc_fw_h = 0;



unsigned g_bmc_disp_vero_w = 0;
unsigned g_bmc_disp_vero_h = 0;



unsigned g_bmc_display_chiesto = 0;
unsigned g_bmc_display_primo = 0;



volatile int g_bmc_via_chiesta = 0;












unsigned long g_bmc_fw_quadro_us = 0;

void bmc_misura_quadro_firmware(CBcmFrameBuffer *s) {
  if (s == nullptr) {
    return;
  }
  s->WaitForVerticalSync();
  const u64 t0 = CTimer::GetClockTicks64();
  for (int i = 0; i < 5; i++) {
    s->WaitForVerticalSync();
  }
  const u64 t1 = CTimer::GetClockTicks64();
  g_bmc_fw_quadro_us = (unsigned long)((t1 - t0) / 5U);
  printf("boot: fbl firmware: ritorno di quadro del display ogni %lu us\r\n",
         g_bmc_fw_quadro_us);
}


void bmc_attendi_quadro_firmware(CBcmFrameBuffer *s) {
  const unsigned long m = fbl_periodo_macchina_us;
  const unsigned long d = g_bmc_fw_quadro_us;
  if (m != 0 && d != 0) {
    const unsigned long scarto = d > m ? d - m : m - d;
    if (scarto * 100UL > m) {
      static bool detto = false;
      if (!detto) {
        detto = true;
        printf("boot: fbl firmware: il display (%lu us) non va al passo "
               "della macchina (%lu us): il suo ritorno di quadro non si "
               "aspetta\r\n", d, m);
      }
      return;
    }
  }
  s->WaitForVerticalSync();
}

void bmc_dopo_initialize(CBcmFrameBuffer *screen, uint8_t *pixels,
                         unsigned pitch, unsigned bpp,
                         unsigned requested_width,
                         unsigned requested_height) {
  raspi_disp_w = (unsigned)g_effective_width;
  raspi_disp_h = (unsigned)g_effective_height;
  bmc_leggi_forma_edid();




  if (!g_kms_active && g_bmc_disp_vero_w != 0 && g_bmc_disp_vero_h != 0) {
    raspi_disp_w = g_bmc_disp_vero_w;
    raspi_disp_h = g_bmc_disp_vero_h;
  }
  raspi_fb_ask_w = requested_width;
  raspi_fb_ask_h = requested_height;
  raspi_fb_got_w = (unsigned)g_effective_width;
  raspi_fb_got_h = (unsigned)g_effective_height;
  raspi_fb_pitch = pitch;
  raspi_fb_bpp = bpp;


  raspi_fb_display = g_bmc_display_primo;
  if (g_kms_active) {
    raspi_fb_size = g_kms_framebuffers[0].size;
    raspi_fb_pages = kKmsBufferCount;


    raspi_fb_displays = 1;
    g_bmc_fw_pixels = nullptr;
  } else {
    raspi_fb_size = screen != nullptr ? screen->GetSize() : 0;
    raspi_fb_pages = 1;
    raspi_fb_displays = CBcmFrameBuffer::GetNumDisplays();
    g_bmc_fw_pixels = pixels;
    g_bmc_fw_pitch = pitch;
    g_bmc_fw_bpp = bpp;
    g_bmc_fw_w = (unsigned)g_effective_width;
    g_bmc_fw_h = (unsigned)g_effective_height;
  }
}



void bmc_scatola_via_se_chiesta(void) {
  if (!g_bmc_via_chiesta) {
    return;
  }
  g_bmc_via_chiesta = 0;
  DataMemBarrier();
  for (unsigned s = 0; s < FB_NUM_LAYERS; ++s) {
    for (unsigned i = 0; i < kKmsBufferCount; ++i) {
      g_kms_hwscale_framebuffer_valid[s][i] = false;
    }
    g_kms_hwscale_front_buffer_valid[s] = false;
  }
  InvalidateSoftwareComposition();
}



uint8_t *bmc_pixel_del_piano(const pi5kms::Plane &piano,
                             const v3dcrt::OutputReadback *uscita_v3d) {
  const u32 a = piano.framebuffer_bus_address;
  if (a == 0U) {
    return nullptr;
  }
  for (unsigned i = 0; i < kKmsBufferCount; ++i) {
    if (g_kms_framebuffers[i].pixels != nullptr &&
        (u32)(uintptr)g_kms_framebuffers[i].pixels == a) {
      return g_kms_framebuffers[i].pixels;
    }
  }
  for (unsigned s = 0; s < FB_NUM_LAYERS; ++s) {
    for (unsigned i = 0; i < kKmsBufferCount; ++i) {
      const pi5kms::Framebuffer &fb = g_kms_hwscale_framebuffers[s][i];
      if (fb.pixels != nullptr && (u32)(uintptr)fb.pixels == a) {
        return fb.pixels;
      }
    }
  }
  if (uscita_v3d != nullptr && uscita_v3d->pixels != nullptr &&
      (uscita_v3d->framebuffer_bus_address == a ||
       (u32)(uintptr)uscita_v3d->pixels == a)) {
    return const_cast<uint8_t *>(uscita_v3d->pixels);
  }
  return nullptr;
}














struct BmcScatolaRighe {
  const char *pezzo[64];
  unsigned lung[64];
  unsigned rientro[64];
  unsigned np;
  unsigned colonne;
  unsigned liberi_a_capo;
};

void bmc_scatola_a_capo(const char *const *righe, int n, unsigned colonne_max,
                        BmcScatolaRighe *t) {
  t->np = 0;
  t->colonne = 0;
  t->liberi_a_capo = 0;
  for (int i = 0; i < n && t->np < 64; i++) {
    const char *s = righe[i] != nullptr ? righe[i] : "";
    const unsigned L = (unsigned)strlen(s);
    unsigned sotto = 2;
    bool etichetta = false;
    for (unsigned k = 0; k + 1 < L && k < 14; k++) {
      if (s[k] == ':' && s[k + 1] == ' ') {
        sotto = k + 2;
        etichetta = true;
        break;
      }
    }
    if (sotto * 2 > colonne_max) {
      sotto = 2;
    }
    const unsigned prima_riga = t->np;
    unsigned pos = 0;
    bool primo = true;
    do {
      const unsigned ind = primo ? 0 : sotto;
      const unsigned spazio = colonne_max - ind;
      unsigned taglio = L - pos;
      if (taglio > spazio) {
        taglio = spazio;
        const unsigned minimo = primo ? sotto : 1;
        for (unsigned k = spazio; k > minimo; k--) {
          if (s[pos + k] == ' ') {
            taglio = k;
            break;
          }
        }
      }
      t->pezzo[t->np] = s + pos;
      t->lung[t->np] = taglio;
      t->rientro[t->np] = ind;
      if (ind + taglio > t->colonne) {
        t->colonne = ind + taglio;
      }
      t->np++;
      pos += taglio;
      while (pos < L && s[pos] == ' ') {
        pos++;
      }
      primo = false;
    } while (pos < L && t->np < 64);
    if (!etichetta && t->np - prima_riga > 1) {
      t->liberi_a_capo++;
    }
  }
}

void bmc_scatola_disegna(uint8_t *pixel, unsigned passo_riga, unsigned larga,
                         unsigned alta, unsigned bpp,
                         const char *const *righe, int n) {
  if (pixel == nullptr || larga == 0 || alta == 0 ||
      (bpp != 2 && bpp != 4) || righe == nullptr || n <= 0) {
    return;
  }
  if (n > 24) {
    n = 24;
  }
  struct Prova {
    const TFont *carattere;
    unsigned scala;
  };
  static const Prova prove[] = {
      {&Font8x16, 4}, {&Font8x16, 3}, {&Font8x16, 2},
      {&Font8x8, 2},  {&Font8x16, 1}, {&Font8x8, 1},
  };
  const unsigned quante = sizeof(prove) / sizeof(prove[0]);
  static BmcScatolaRighe t;
  unsigned scelta = quante;
  unsigned passo = 0;


  for (unsigned giro = 0; giro < 2 * quante && scelta == quante; giro++) {
    const unsigned p = giro % quante;
    CCharGenerator f(*prove[p].carattere);
    const unsigned s = prove[p].scala;
    const unsigned margine = 4 * s;
    const unsigned cw = f.GetCharWidth();
    if (larga <= 2 * margine + 16 * cw * s || alta <= 2 * margine) {
      continue;
    }
    bmc_scatola_a_capo(righe, n, (larga - 2 * margine) / (cw * s), &t);
    if (giro < quante && t.liberi_a_capo != 0) {
      continue;
    }
    if (t.np * f.GetCharHeight() * s + 2 * margine <= alta) {
      scelta = p;
      passo = f.GetCharHeight();
    } else if (t.np * f.GetUnderline() * s + 2 * margine <= alta) {
      scelta = p;
      passo = f.GetUnderline();
    }
  }
  if (scelta == quante) {


    scelta = quante - 1;
    CCharGenerator f(*prove[scelta].carattere);
    if (larga <= 8 + 8 * f.GetCharWidth() || alta <= 8) {
      return;
    }
    bmc_scatola_a_capo(righe, n, (larga - 8) / f.GetCharWidth(), &t);
    passo = f.GetUnderline();
    const unsigned max_righe = (alta - 8) / passo;
    if (t.np > max_righe) {
      t.np = max_righe;
    }
  }
  if (t.np == 0 || passo == 0) {
    return;
  }
  CCharGenerator font(*prove[scelta].carattere);
  const unsigned scala = prove[scelta].scala;
  const unsigned margine = 4 * scala;
  const unsigned cw = font.GetCharWidth();
  unsigned w = t.colonne * cw * scala + 2 * margine;
  unsigned h = t.np * passo * scala + 2 * margine;
  if (w > larga) {
    w = larga;
  }
  if (h > alta) {
    h = alta;
  }
  const unsigned x0 = (larga - w) / 2;
  const unsigned y0 = (alta - h) / 2;

  for (unsigned y = 0; y < alta; y++) {
    uint8_t *riga = pixel + (size_t)y * passo_riga;
    const bool in_y = y >= y0 + margine && y < y0 + h;
    for (unsigned x = 0; x < larga; x++) {
      bool acceso = false;
      if (in_y && x >= x0 + margine && x < x0 + w) {
        const unsigned cx = (x - x0 - margine) / scala;
        const unsigned cy = (y - y0 - margine) / scala;
        const unsigned r = cy / passo;
        const unsigned col = cx / cw;
        if (r < t.np && col >= t.rientro[r] &&
            col - t.rientro[r] < t.lung[r]) {
          acceso = font.GetPixel(t.pezzo[r][col - t.rientro[r]], cx % cw,
                                 cy % passo);
        }
      }
      if (bpp == 2) {
        ((volatile uint16_t *)riga)[x] = acceso ? 0xFFFF : 0x0000;
      } else {
        ((volatile uint32_t *)riga)[x] = acceso ? 0xFFFFFFFFu : 0xFF000000u;
      }
    }
  }
}

}  // namespace

extern "C" {







void fbl_scatola_a_schermo(const char *const *righe, int n) {
  if (righe == nullptr || n <= 0) {
    return;
  }
  if (g_kms_active) {
    v3dcrt::OutputReadback uscita = {};
    const bool uscita_ok =
        v3dcrt::ReadCompletedFrame(&uscita) && uscita.pixels != nullptr;
    const unsigned quanti = g_bmc_quanti_piani;
    for (unsigned i = 0; i < quanti && i < (unsigned)FB_NUM_LAYERS; ++i) {
      const pi5kms::Plane &piano = g_bmc_piani[i];
      if (piano.source.height < 100U || piano.source.width < 100U) {
        continue;
      }
      uint8_t *pixel =
          bmc_pixel_del_piano(piano, uscita_ok ? &uscita : nullptr);
      if (pixel == nullptr) {
        continue;
      }
      bmc_scatola_disegna(pixel, piano.pitch, piano.source.width,
                          piano.source.height, piano.depth / 8U, righe, n);
      CleanAndInvalidateDataCacheRange(
          (uintptr)pixel, (size_t)piano.pitch * piano.source.height);
    }
  } else if (g_bmc_fw_pixels != nullptr) {

    bmc_scatola_disegna(g_bmc_fw_pixels, g_bmc_fw_pitch, g_bmc_fw_w,
                        g_bmc_fw_h, g_bmc_fw_bpp, righe, n);
  }
}


void fbl_scatola_via(void) {
  g_bmc_via_chiesta = 1;
  DataMemBarrier();
}

}




//









//







#include <circle/bcmpropertytags.h>











static void bmc_leggi_forma_edid(void) {
  static bool letta = false;
  if (letta) {
    return;
  }
  letta = true;
  TPropertyTagEDIDBlock blocco;
  memset(&blocco, 0, sizeof blocco);
  blocco.nBlockNumber = EDID_FIRST_BLOCK;
  unsigned w = 0, h = 0;
  const char *da = "niente";
  CBcmPropertyTags Tags;
  if (Tags.GetTag(PROPTAG_GET_EDID_BLOCK, &blocco, sizeof blocco, 4) &&
      blocco.nStatus == EDID_STATUS_SUCCESS && blocco.Block[0] == 0x00 &&
      blocco.Block[1] == 0xFF && blocco.Block[6] == 0xFF &&
      blocco.Block[7] == 0x00) {
    const u8 *e = blocco.Block;
    if (e[54] != 0 || e[55] != 0) {
      w = e[66] | ((unsigned)(e[68] & 0xF0) << 4);
      h = e[67] | ((unsigned)(e[68] & 0x0F) << 8);
      da = "modo dettagliato, mm";
    }
    if (w == 0 || h == 0) {
      w = e[21];
      h = e[22];
      da = "blocco base, cm";
    }
  }
  if (w != 0 && h != 0) {
    const unsigned forma = (w * 1000U + h / 2U) / h;
    if (forma >= 1000U && forma <= 2500U) {
      raspi_disp_fisico_milli = forma;
    }
  }
  printf("boot: bmc64 edid immagine %ux%u (%s), forma %u (0 = non si sa)\r\n",
         w, h, da, raspi_disp_fisico_milli);
}

namespace {

void bmc_v3d_aspetta();

bool g_bmc_doppio_avvio = false;



bool g_bmc_fw_per_scelta = false;

bool g_bmc_composito = false;
CBcmFrameBuffer *s_fb2 = nullptr;
int s_strato2 = -1;
uint8_t *s_base2 = nullptr;
unsigned s_pitch2 = 0;
unsigned s_bpp2 = 1;
unsigned s_fb2_w = 0;
unsigned s_fb2_h = 0;
uint16_t s_pal2_16[256];
int32_t s_xmap2[2048];
uint16_t s_line2_16[2048];
uint64_t s_firma2 = 0;
int s_secondo_forza = 1;
int s_secondo_nero = 0;
int s_d2x = 0;
int s_d2y = 0;
int s_d2w = -1;
int s_d2h = -1;










bool bmc_hdmi1_chiesta(ViceOptions *options) {
  g_bmc_display_chiesto =
      (options != nullptr && options->GetFBDisplay() == 1) ? 1 : 0;
  if (g_bmc_display_chiesto) {
    printf("boot: fb_display=1: lo schermo principale chiesto sulla HDMI "
           "1\r\n");
  }
  return g_bmc_display_chiesto != 0;
}


unsigned bmc_display_principale(void) {
  if (!g_bmc_display_chiesto) {
    return 0;
  }
  const unsigned n = CBcmFrameBuffer::GetNumDisplays();
  if (n < 2) {
    printf("boot: fb_display=1, ma il firmware conta %u display: resto sulla "
           "HDMI 0\r\n", n);
    return 0;
  }
  return 1;
}


bool bmc_doppio_hdmi_chiesto(ViceOptions *options) {
#if defined(RASPI_C128)
  if (options != nullptr && options->DoppioHdmi()) {
    g_bmc_doppio_avvio = true;
    printf("boot: doppio_hdmi=1: le due HDMI del C128, lo schermo resta al "
           "firmware (niente scalatore, niente shader)\r\n");
    return true;
  }
#else
  (void)options;
#endif
  return false;
}






//











void bmc_misura_doppio(unsigned *w, unsigned *h, const char *chi) {
  g_bmc_fw_per_scelta = true;
  unsigned disp_w = 640, disp_h = 480;
  CBcmPropertyTags Tags;
  TPropertyTagDisplayDimensions Dimensions;
  if (Tags.GetTag(PROPTAG_GET_DISPLAY_DIMENSIONS, &Dimensions,
                  sizeof Dimensions)) {
    if (Dimensions.nWidth >= 64 && Dimensions.nHeight >= 64) {
      disp_w = Dimensions.nWidth;
      disp_h = Dimensions.nHeight;
    }
  }
  g_bmc_disp_vero_w = disp_w;
  g_bmc_disp_vero_h = disp_h;
  if (*w != 0 && *h != 0) {
    return;
  }
  unsigned fw = disp_w, fh = disp_h;
#if RASPPI == 4
  if (fw / 2 >= 320 && fh / 2 >= 240) {
    fw /= 2;
    fh /= 2;
    if (disp_w >= 1280 && fh % 272 != 0) {
      unsigned su = ((fh / 272) + 1) * 272;
      if (su <= disp_h) {
        fh = su;

        fw = (unsigned)((((unsigned long)fh * disp_w) + disp_h) /
                        (2UL * disp_h)) * 2u;
        if (fw > disp_w) {
          fw = disp_w;
        }
      }
    }
  }
#endif
  if (fw > 2048) {
    fw = 2048;
  }
  *w = fw & ~1u;
  *h = fh & ~1u;
  printf("boot: %s: display %ux%u, tela %ux%u\r\n", chi, disp_w, disp_h,
         *w, *h);
}

uint64_t firma_della_tela(const uint8_t *p, int pitch, int x, int y, int w,
                          int h) {
  uint64_t f = 14695981039346656037ULL;
  for (int r = 0; r < h; r++) {
    const uint8_t *q = p + (size_t)(y + r) * (size_t)pitch + (size_t)x;
    int n = w;
    while (n >= 8) {
      uint64_t v;
      memcpy(&v, q, sizeof(v));
      f = (f ^ v) * 1099511628211ULL;
      q += 8;
      n -= 8;
    }
    while (n-- > 0) {
      f = (f ^ (uint64_t)*q++) * 1099511628211ULL;
    }
  }
  return f;
}

}  // namespace

extern "C" {
unsigned raspi_fb2_w = 0;
unsigned raspi_fb2_h = 0;
int raspi_fb2_strato = -1;
}

namespace {

void secondo_chiudi(void) {
  bmc_v3d_aspetta();
  if (s_fb2 != nullptr) {
    delete s_fb2;
    s_fb2 = nullptr;
  }
  s_base2 = nullptr;
  s_pitch2 = 0;
  s_bpp2 = 1;
  s_fb2_w = 0;
  s_fb2_h = 0;
  s_d2w = -1;
  s_d2h = -1;
  s_strato2 = -1;
  raspi_fb2_w = 0;
  raspi_fb2_h = 0;
  raspi_fb2_strato = -1;
}


int secondo_apri(int strato, int forza) {
  bmc_v3d_aspetta();
  if (g_kms_active || g_bmc_fw_pixels == nullptr || g_bmc_fw_w == 0 ||
      g_bmc_fw_h == 0) {
    return 0;
  }



  if (forza) {
    CBcmFrameBuffer::InvalidateNumDisplays();
  }
  if (CBcmFrameBuffer::GetNumDisplays() < 2) {
    return 0;
  }

  unsigned altra = raspi_fb_display == 0 ? 1 : 0;
  if (altra >= CBcmFrameBuffer::GetNumDisplays()) {
    return 0;
  }

  secondo_chiudi();


  s_fb2 = new CBcmFrameBuffer(g_bmc_fw_w, g_bmc_fw_h, 8, g_bmc_fw_w,
                              g_bmc_fw_h, altra, FALSE);
  if (s_fb2 == nullptr) {
    return 0;
  }
  for (unsigned i = 0; i < 256; i++) {
    s_fb2->SetPalette((uint8_t)i, (uint16_t)0);
    s_pal2_16[i] = 0;
  }
  if (!s_fb2->Initialize()) {
    delete s_fb2;
    s_fb2 = nullptr;
    return 0;
  }

  s_base2 = (uint8_t *)(uintptr_t)s_fb2->GetBuffer();
  s_pitch2 = s_fb2->GetPitch();
  s_fb2_w = s_fb2->GetWidth();
  s_fb2_h = s_fb2->GetHeight();


  s_bpp2 = (s_fb2_w > 0 && s_pitch2 / s_fb2_w >= 2) ? 2 : 1;

  if (s_base2 == nullptr || s_fb2_w == 0 || s_fb2_h == 0) {
    secondo_chiudi();
    return 0;
  }
  memset(s_base2, 0, (size_t)s_pitch2 * s_fb2_h);

  s_strato2 = strato;
  s_secondo_forza = 1;
  s_secondo_nero = 0;
  raspi_fb2_w = s_fb2_w;
  raspi_fb2_h = s_fb2_h;
  raspi_fb2_strato = strato;
  printf("[VID] secondo schermo aperto: display %u, %ux%u, %u byte per pixel, "
         "strato %d\n", altra, s_fb2_w, s_fb2_h, s_bpp2, strato);
  return 1;
}



void secondo_tavolozza(const uint16_t *pal) {
  bmc_v3d_aspetta();
  if (s_fb2 == nullptr || pal == nullptr) {
    return;
  }
  for (unsigned i = 0; i < 256; i++) {
    s_pal2_16[i] = pal[i];
    s_fb2->SetPalette((uint8_t)i, pal[i]);
  }
  s_fb2->UpdatePalette();


  s_secondo_forza = 1;
}

FrameBufferLayer *secondo_strato(void) {
  if (s_fb2 == nullptr || s_strato2 < 0) {
    return nullptr;
  }
  for (unsigned i = 0; i < FB_NUM_LAYERS; i++) {
    if (g_layers[i] != nullptr && g_layers[i]->GetLogicalLayer() == s_strato2) {
      return g_layers[i];
    }
  }
  return nullptr;
}



bool bmc_sul_secondo(FrameBufferLayer *layer) {
  return s_fb2 != nullptr && layer != nullptr &&
         layer->GetLogicalLayer() == s_strato2;
}



void bmc_secondo_disegna(void) {
  FrameBufferLayer *layer = secondo_strato();
  if (layer != nullptr) {
    layer->BlitIntoSecondScreen();
  }
}


void bmc_secondo_tavolozza_se(FrameBufferLayer *layer) {
  if (bmc_sul_secondo(layer)) {
    secondo_tavolozza(layer->GetPalette565());
  }
}

}  // namespace

extern "C" {





int circle_secondo_schermo(int strato, int forza) {
  if (strato < 0) {
    secondo_chiudi();
    InvalidateSoftwareComposition();
    return 0;
  }
  if (g_kms_active) {
    return -2;
  }
  if (!secondo_apri(strato, forza)) {
    return -1;
  }



  FrameBufferLayer *layer = secondo_strato();
  if (layer != nullptr) {
    secondo_tavolozza(layer->GetPalette565());
  }
  InvalidateSoftwareComposition();
  return 1;
}

int circle_secondo_schermo_info(int *w, int *h, int *displays) {
  if (w != nullptr) {
    *w = (int)s_fb2_w;
  }
  if (h != nullptr) {
    *h = (int)s_fb2_h;
  }
  if (displays != nullptr) {
    *displays = g_kms_active ? (int)raspi_fb_displays
                             : (int)CBcmFrameBuffer::GetNumDisplays();
  }
  return s_fb2 != nullptr ? s_strato2 : -1;
}



int circle_doppio_hdmi_all_avvio(void) { return g_bmc_doppio_avvio ? 1 : 0; }


int circle_fb_display_chiesto(void) { return (int)g_bmc_display_chiesto; }

int circle_composito_chiesto(void) { return g_bmc_composito ? 1 : 0; }

}


void FrameBufferLayer::BlitIntoSecondScreen(void) {
  if (s_fb2 == nullptr || s_base2 == nullptr) {
    return;
  }
  if (!allocated_ || pixels_ == nullptr || pixelmode_ != 0) {
    return;
  }
  if (src_w_ <= 0 || src_h_ <= 0) {
    return;
  }
  if (s_fb2_w > sizeof(s_xmap2) / sizeof(s_xmap2[0])) {
    return;
  }


  if (!showing_) {
    if (!s_secondo_nero) {
      memset(s_base2, 0, (size_t)s_pitch2 * s_fb2_h);
      s_secondo_nero = 1;
      s_secondo_forza = 1;
    }
    return;
  }
  s_secondo_nero = 0;




  int d2x, d2y, d2w, d2h;
  if (display_width_ > 0 && display_height_ > 0 && dst_w_ > 0 && dst_h_ > 0) {
    d2x = (int)((int64_t)dst_x_ * (int64_t)s_fb2_w / display_width_);
    d2y = (int)((int64_t)dst_y_ * (int64_t)s_fb2_h / display_height_);
    d2w = (int)((int64_t)dst_w_ * (int64_t)s_fb2_w / display_width_);
    d2h = (int)((int64_t)dst_h_ * (int64_t)s_fb2_h / display_height_);
  } else {
    d2x = 0;
    d2y = 0;
    d2w = (int)s_fb2_w;
    d2h = (int)s_fb2_h;
  }
  if (d2x < 0) { d2w += d2x; d2x = 0; }
  if (d2y < 0) { d2h += d2y; d2y = 0; }
  if (d2x + d2w > (int)s_fb2_w) { d2w = (int)s_fb2_w - d2x; }
  if (d2y + d2h > (int)s_fb2_h) { d2h = (int)s_fb2_h - d2y; }
  if (d2w <= 0 || d2h <= 0) {
    return;
  }


  {
    uint64_t firma = firma_della_tela(pixels_, fb_pitch_, src_x_, src_y_,
                                      src_w_, src_h_);
    firma ^= ((uint64_t)(uint32_t)src_x_ << 48) ^
             ((uint64_t)(uint32_t)src_y_ << 32) ^
             ((uint64_t)(uint32_t)src_w_ << 16) ^
              (uint64_t)(uint32_t)src_h_;
    firma ^= ((uint64_t)(uint32_t)d2x << 40) ^
             ((uint64_t)(uint32_t)d2y << 24) ^
             ((uint64_t)(uint32_t)d2w << 8) ^
              (uint64_t)(uint32_t)d2h;
    if (!s_secondo_forza && firma == s_firma2) {
      return;
    }
    s_firma2 = firma;
    s_secondo_forza = 0;
  }

  if (d2x != s_d2x || d2y != s_d2y || d2w != s_d2w || d2h != s_d2h) {
    memset(s_base2, 0, (size_t)s_pitch2 * s_fb2_h);
    s_d2x = d2x;
    s_d2y = d2y;
    s_d2w = d2w;
    s_d2h = d2h;
  }

  const uint32_t x_step = ((uint32_t)src_w_ << 16) / (uint32_t)d2w;
  const uint32_t y_step = ((uint32_t)src_h_ << 16) / (uint32_t)d2h;

  for (int i = 0; i < d2w; i++) {
    uint32_t sx = src_x_ + (((uint32_t)i * x_step) >> 16);
    if (sx >= (uint32_t)fb_width_) {
      sx = fb_width_ - 1;
    }
    s_xmap2[i] = (int32_t)sx;
  }

  for (int dy = 0; dy < d2h; dy++) {
    uint32_t sy = src_y_ + (((uint32_t)dy * y_step) >> 16);
    if (sy >= (uint32_t)fb_height_) {
      sy = fb_height_ - 1;
    }
    const uint8_t *srow = pixels_ + (size_t)sy * fb_pitch_;
    uint8_t *drow = s_base2 + (size_t)(d2y + dy) * s_pitch2;
    if (s_bpp2 == 2) {
      for (int i = 0; i < d2w; i++) {
        s_line2_16[i] = s_pal2_16[srow[s_xmap2[i]]];
      }
      memcpy(drow + (size_t)d2x * 2, s_line2_16, (size_t)d2w * 2);
    } else {
      for (int i = 0; i < d2w; i++) {
        drow[d2x + i] = srow[s_xmap2[i]];
      }
    }
  }
}














struct BmcRitaglio {
  int src_x, src_y, src_w, src_h;
  int dst_x, dst_y, dst_w, dst_h;
};


static bool bmc_ritaglia_lato(int src, int src_len, int dst, int dst_len,
                              int schermo, int *s0, int *s_len, int *d0,
                              int *d_len) {
  const int v0 = dst < 0 ? 0 : dst;
  const int v1 = dst + dst_len > schermo ? schermo : dst + dst_len;
  if (v0 == dst && v1 == dst + dst_len) {
    *s0 = src;
    *s_len = src_len;
    *d0 = dst;
    *d_len = dst_len;
    return true;
  }
  if (v1 - v0 < 8) {
    return false;
  }

  const int64_t a = src + ((int64_t)(v0 - dst) * src_len * 2 + dst_len) /
                              (2 * (int64_t)dst_len);
  const int64_t b = src + ((int64_t)(v1 - dst) * src_len * 2 + dst_len) /
                              (2 * (int64_t)dst_len);
  if (b - a < 1) {
    return false;
  }
  *s0 = (int)a;
  *s_len = (int)(b - a);
  *d0 = v0;
  *d_len = v1 - v0;
  return true;
}

static bool bmc_ritaglia_piano(int src_x, int src_y, int src_w, int src_h,
                               int dst_x, int dst_y, int dst_w, int dst_h,
                               int screen_w, int screen_h, BmcRitaglio *r) {
  if (src_w <= 0 || src_h <= 0 || dst_w <= 0 || dst_h <= 0 ||
      screen_w <= 0 || screen_h <= 0) {
    return false;
  }
  return bmc_ritaglia_lato(src_x, src_w, dst_x, dst_w, screen_w,
                           &r->src_x, &r->src_w, &r->dst_x, &r->dst_w) &&
         bmc_ritaglia_lato(src_y, src_h, dst_y, dst_h, screen_h,
                           &r->src_y, &r->src_h, &r->dst_y, &r->dst_h);
}





static const BmcRitaglio *s_bmc_ritaglio = nullptr;
static int s_bmc_copiato[FB_NUM_LAYERS][kKmsBufferCount][4];

static void bmc_copiato_segna(unsigned slot, unsigned buffer_index, int x,
                              int y, int w, int h) {
  s_bmc_copiato[slot][buffer_index][0] = x;
  s_bmc_copiato[slot][buffer_index][1] = y;
  s_bmc_copiato[slot][buffer_index][2] = w;
  s_bmc_copiato[slot][buffer_index][3] = h;
}








static const char *s_bmc_perche = "-";

static void bmc_strada_conta(int strada, bool macchina) {
  static u32 conti[3] = {0U, 0U, 0U};
  static u32 quadri = 0U;
  static bool prima = true;
  if (!macchina || strada < 0 || strada > 2) {
    return;
  }
  conti[strada]++;
  if (++quadri < 1000U) {
    return;
  }
  if (prima || conti[2] != 0U) {
    printf("boot: bmc64 v3d strada quadri=%u v3d=%u hvs=%u cpu=%u "
           "rifiuto=%s\r\n", quadri, conti[0], conti[1], conti[2],
           s_bmc_perche);
  }
  prima = false;
  quadri = 0U;
  conti[0] = conti[1] = conti[2] = 0U;
}




bool FrameBufferLayer::CopyLayerSourceToHwscaleFramebuffer(FrameBufferLayer *layer,
                                                           unsigned buffer_index) {
  if (layer == nullptr || layer->layer_ < 0 ||
      layer->layer_ >= FB_NUM_LAYERS ||
      buffer_index >= kKmsBufferCount) {
    return false;
  }

  const unsigned slot = (unsigned)layer->layer_;
  const u32 depth = layer->transparency_ ? 32U : 16U;


  const int cx = s_bmc_ritaglio != nullptr ? s_bmc_ritaglio->src_x
                                           : layer->src_x_;
  const int cy = s_bmc_ritaglio != nullptr ? s_bmc_ritaglio->src_y
                                           : layer->src_y_;
  const int cw = s_bmc_ritaglio != nullptr ? s_bmc_ritaglio->src_w
                                           : layer->src_w_;
  const int ch = s_bmc_ritaglio != nullptr ? s_bmc_ritaglio->src_h
                                           : layer->src_h_;
  const int *copiato = s_bmc_copiato[slot][buffer_index];
  const bool pezzo_nuovo = copiato[0] != cx || copiato[1] != cy ||
                           copiato[2] != cw || copiato[3] != ch;
  const u32 hw_width = (u32)cw;
  const u32 hw_height = (u32)ch;
  pi5kms::Framebuffer *fb = &g_kms_hwscale_framebuffers[slot][buffer_index];
  bool recreated = false;
  if (!ensure_hwscale_framebuffer(slot, buffer_index,
                                  hw_width, hw_height, depth,
                                  &recreated)) {
    return false;
  }
  if (!recreated && !layer->dirty_ && !pezzo_nuovo &&
      g_kms_hwscale_framebuffer_valid[slot][buffer_index]) {
    return true;
  }
  bmc_copiato_segna(slot, buffer_index, -1, -1, -1, -1);

  if (layer->transparency_) {
    if (layer->layer_ == FB_LAYER_STATUS &&
        layer->pixelmode_ == 0) {
      const bool force_full =
          recreated || pezzo_nuovo ||
          !g_kms_hwscale_framebuffer_valid[slot][buffer_index];
      if (!copy_status_layer_incremental(
              slot, buffer_index, layer->pixels_, (u32)layer->fb_pitch_,
              (u32)cx, (u32)cy,
              (u32)cw, (u32)ch,
              layer->pal_argb_,
              palette_signature_argb(layer->pal_argb_),
              force_full, fb)) {
        return false;
      }
      g_kms_hwscale_framebuffer_valid[slot][buffer_index] = true;
      bmc_copiato_segna(slot, buffer_index, cx, cy, cw, ch);
      layer->dirty_ = false;
      return true;
    }

    for (int y = 0; y < ch; ++y) {
      uint32_t *dst_row =
          (uint32_t *)(fb->pixels + (u32)y * fb->pitch);
      const int src_y = cy + y;
      const uint8_t *src_row = layer->pixels_ + src_y * layer->fb_pitch_;
      for (int x = 0; x < cw; ++x) {
        dst_row[x] = layer->pal_argb_[src_row[cx + x]];
      }
    }

    pi5kms::FlushFramebuffer(*fb);
    g_kms_hwscale_framebuffer_valid[slot][buffer_index] = true;
    bmc_copiato_segna(slot, buffer_index, cx, cy, cw, ch);
    layer->dirty_ = false;
    return true;
  }

  for (int y = 0; y < ch; ++y) {
    const int src_y = cy + y;

    if (layer->pixelmode_ == 0) {
      uint16_t *dst_row =
          (uint16_t *)(fb->pixels + (u32)y * fb->pitch);
      const uint8_t *src_row = layer->pixels_ + src_y * layer->fb_pitch_;
      for (int x = 0; x < cw; ++x) {
        dst_row[x] = layer->pal_565_[src_row[cx + x]];
      }
    } else {
      uint16_t *dst_row =
          (uint16_t *)(fb->pixels + (u32)y * fb->pitch);
      const uint16_t *src_row =
          (const uint16_t *)(layer->pixels_ + src_y * layer->fb_pitch_);
      memcpy(dst_row, src_row + cx,
             (size_t)cw * sizeof(uint16_t));
    }
  }

  pi5kms::FlushFramebuffer(*fb);
  g_kms_hwscale_framebuffer_valid[slot][buffer_index] = true;
  bmc_copiato_segna(slot, buffer_index, cx, cy, cw, ch);
  layer->dirty_ = false;
  return true;
}

bool FrameBufferLayer::CanUseKmsDirectScanout(FrameBufferLayer *layer,
                                              int screen_w,
                                              int screen_h) {

  if (!g_kms_active || layer == nullptr) {
    s_bmc_perche = "kms-spento";
    return false;
  }
  if (layer->transparency_ && layer->pixelmode_ != 0) {
    s_bmc_perche = "formato";
    return false;
  }
  if (!layer->transparency_ &&
      layer->pixelmode_ != 0 && layer->pixelmode_ != 1) {
    s_bmc_perche = "formato";
    return false;
  }
  if (layer->src_w_ <= 0 || layer->src_h_ <= 0 ||
      layer->dst_w_ <= 0 || layer->dst_h_ <= 0) {
    s_bmc_perche = "misure";
    return false;
  }
  if (layer->src_x_ < 0 || layer->src_y_ < 0 ||
      layer->src_x_ + layer->src_w_ > layer->fb_width_ ||
      layer->src_y_ + layer->src_h_ > layer->fb_height_) {
    s_bmc_perche = "sorgente-fuori-tela";
    return false;
  }


  BmcRitaglio r;
  if (!bmc_ritaglia_piano(layer->src_x_, layer->src_y_, layer->src_w_,
                          layer->src_h_, layer->dst_x_, layer->dst_y_,
                          layer->dst_w_, layer->dst_h_, screen_w, screen_h,
                          &r)) {
    s_bmc_perche = "fuori-schermo";
    return false;
  }
  return true;
}

bool FrameBufferLayer::BuildKmsLayerPlane(FrameBufferLayer *layer,
                                          unsigned buffer_index,
                                          int screen_w,
                                          int screen_h,
                                          pi5kms::Plane *plane) {
  if (plane == nullptr ||
      !CanUseKmsDirectScanout(layer, screen_w, screen_h)) {
    return false;
  }


  BmcRitaglio r;
  bmc_ritaglia_piano(layer->src_x_, layer->src_y_, layer->src_w_,
                     layer->src_h_, layer->dst_x_, layer->dst_y_,
                     layer->dst_w_, layer->dst_h_, screen_w, screen_h, &r);

  const unsigned slot = (unsigned)layer->layer_;
  unsigned selected_buffer_index = buffer_index;
  const bool independent_overlay_buffer =
      layer->transparency_ ||
      layer->layer_ == FB_LAYER_UI ||
      layer->layer_ == FB_LAYER_STATUS;
  if (independent_overlay_buffer &&
      g_kms_hwscale_front_buffer_valid[slot]) {
    const unsigned front_buffer_index =
        g_kms_hwscale_front_buffer_index[slot];
    if (!layer->dirty_ &&
        g_kms_hwscale_framebuffer_valid[slot][front_buffer_index]) {
      selected_buffer_index = front_buffer_index;
    } else if (layer->dirty_) {
      selected_buffer_index = front_buffer_index ^ 1U;
    }
  }

  s_bmc_ritaglio = &r;
  const bool copia_fatta = CopyLayerSourceToHwscaleFramebuffer(
      layer, selected_buffer_index);
  s_bmc_ritaglio = nullptr;
  if (!copia_fatta) {
    s_bmc_perche = "copia";
    return false;
  }
  if (independent_overlay_buffer) {
    g_kms_hwscale_front_buffer_index[slot] = selected_buffer_index;
    g_kms_hwscale_front_buffer_valid[slot] = true;
  }

  const pi5kms::Framebuffer &fb =
      g_kms_hwscale_framebuffers[slot][selected_buffer_index];
  const pi5kms::PixelFormat format =
      layer->transparency_ ? pi5kms::kPixelFormatArgb8888
                           : pi5kms::kPixelFormatRgb565;
  *plane = {
    (u32)(uintptr)fb.pixels,
    fb.pitch,
    fb.width,
    fb.height,
    fb.depth,
    format,
    g_interpolation_enabled ? pi5kms::kScaleFilterMitchell
                            : pi5kms::kScaleFilterNearest,
    {0, 0, (u32)r.src_w, (u32)r.src_h},
    {r.dst_x, r.dst_y, (u32)r.dst_w, (u32)r.dst_h}
  };
  return true;
}

bool FrameBufferLayer::TryKmsDirectScanout(FrameBufferLayer **layers,
                                           unsigned layer_count,
                                           int screen_w,
                                           int screen_h,
                                           bool wait_for_vblank) {
  if (layer_count == 0 || layer_count > FB_NUM_LAYERS) {
    return false;
  }

  const unsigned buffer_index = kms_back_buffer_index();
  pi5kms::Plane planes[FB_NUM_LAYERS];
  for (unsigned i = 0; i < layer_count; ++i) {
    if (!BuildKmsLayerPlane(layers[i], buffer_index, screen_w, screen_h,
                            &planes[i])) {
      return false;
    }
  }

  if (!presenta_e_ricorda(    planes, layer_count,
                              (u32)screen_w, (u32)screen_h,
                              wait_for_vblank)) {
    s_bmc_perche = "presentazione";
    return false;
  }

  g_kms_front_buffer_index = buffer_index;
  g_capture_v3d_layer = nullptr;
  return true;
}











static bool bmc_v3d_qualche_effetto(const v3dcrt::EffectParams &p) {
  return p.enable_geometry || p.enable_convergence ||
         p.enable_horizontal_filtering || p.enable_edge_blur ||
         p.enable_scanlines || p.enable_mask || p.enable_bloom ||
         p.enable_vignette || p.enable_uneven_illumination ||
         p.enable_horizontal_jitter || p.enable_composite_artifacts ||
         p.enable_glass_reflection || p.enable_rounded_screen_mask ||
         p.enable_edge_glow || p.enable_noise || p.enable_output_response;
}



//






//










extern "C" {
void sem_dec(uint32_t *semaphore);
void sem_inc(uint32_t *semaphore);
void passo_prima(const char *cosa);
void passo_fatto(const char *cosa);


int banco_v3d_finto(const uint8_t *pixels, unsigned pitch, unsigned x,
                    unsigned y, unsigned w, unsigned h) __attribute__((weak));
}

namespace {

struct BmcV3dLavoro {
  FrameBufferLayer *layer;
  s32 dst_x;
  s32 dst_y;
  u32 dst_w;
  u32 dst_h;
  int screen_w;
  int screen_h;
  bool wait_for_vblank;
  bool has_overlay;
  unsigned overlay_count;
  u32 overlay_mask;
  pi5kms::Plane overlay_planes[FB_NUM_LAYERS - 1];
  unsigned buffer_index;
  u64 present_start_us;
  u64 overlay_prep_done_us;
  v3dcrt::InputFramebuffer source;
  v3dcrt::OutputFramebuffer target;
  v3dcrt::EffectParams params;
  bool v3d_presented;
  pi5kms::Plane v3d_plane;
  u16 palette[256];
  bool esito;
  u32 durata_us;
};

BmcV3dLavoro s_bmc_lavoro;
uint8_t *s_bmc_foto = nullptr;
size_t s_bmc_foto_byte = 0;
volatile int s_bmc_in_volo = 0;
unsigned s_bmc_core_mandante = 0;
int s_bmc_core3 = 1;
int s_bmc_fallito = 0;
int s_bmc_bloccato = 0;




struct BmcV3dConti {
  u32 quadri;
  u64 core1_tot;
  u32 core1_min;
  u32 core1_max;
  u32 lavori;
  u64 lavoro_tot;
  u32 lavoro_min;
  u32 lavoro_max;
};
const BmcV3dConti kBmcContiVuoti = {0U, 0ULL, 0xFFFFFFFFU, 0U,
                                    0U, 0ULL, 0xFFFFFFFFU, 0U};
BmcV3dConti s_bmc_conti = kBmcContiVuoti;

bool bmc_v3d_finta() {
  return banco_v3d_finto != nullptr &&
         banco_v3d_finto(nullptr, 0U, 0U, 0U, 0U, 0U) > 0;
}

bool bmc_v3d_col_core3() {
  return s_bmc_core3 == 1 && fbl_worker_alive != 0 && !s_bmc_fallito &&
         !s_bmc_bloccato;
}

void bmc_v3d_conta_lavoro(u32 us) {
  BmcV3dConti &c = s_bmc_conti;
  c.lavori++;
  c.lavoro_tot += us;
  if (us < c.lavoro_min) {
    c.lavoro_min = us;
  }
  if (us > c.lavoro_max) {
    c.lavoro_max = us;
  }
}

void bmc_v3d_conta_core1(u32 us) {
  BmcV3dConti &c = s_bmc_conti;
  c.quadri++;
  c.core1_tot += us;
  if (us < c.core1_min) {
    c.core1_min = us;
  }
  if (us > c.core1_max) {
    c.core1_max = us;
  }
  if (c.quadri < 250U) {
    return;
  }
  printf("boot: bmc64 v3d %s quadri=%u core1_us=%u/%u/%u "
         "lavoro_us=%u/%u/%u\r\n",
         bmc_v3d_col_core3() ? "core3" : "core1", c.quadri,
         c.core1_min, static_cast<u32>(c.core1_tot / c.quadri), c.core1_max,
         c.lavori != 0U ? c.lavoro_min : 0U,
         c.lavori != 0U ? static_cast<u32>(c.lavoro_tot / c.lavori) : 0U,
         c.lavoro_max);
  c = kBmcContiVuoti;
}






















struct BmcFwStrato {
  FrameBufferLayer *ombra;
  int start_x;
  int end_x;
  int start_y;
  int end_y;
  int64_t x_step;
  int64_t y_step;
  bool bilineare;
};

struct BmcFwLavoro {
  int left;
  int top;
  int right;
  int bottom;
  bool valido;
  unsigned n;
  BmcFwStrato strati[FB_NUM_LAYERS];
  bool attendi;
  FrameBufferLayer *ombra2;
};

BmcFwLavoro s_bmc_fw_lavoro;
int s_bmc_tipo = 0;


alignas(FrameBufferLayer) unsigned char
    s_bmc_fw_ombre[FB_NUM_LAYERS + 1][sizeof(FrameBufferLayer)];
uint8_t *s_bmc_fw_pixel[FB_NUM_LAYERS + 1] = {};
size_t s_bmc_fw_byte[FB_NUM_LAYERS + 1] = {};
volatile u32 s_bmc_fw_durata_us = 0;
volatile u32 s_bmc_fw_disegno_us = 0;


u32 s_bmc_fw_attesa1_us = 0;

u32 s_bmc_fw_passo_us = 0;
size_t s_bmc_fw_foto_byte = 0;

struct BmcFwConti {
  u32 quadri;
  u64 core1_tot;
  u32 core1_min;
  u32 core1_max;
  u32 lavori;
  u64 lavoro_tot;
  u32 lavoro_min;
  u32 lavoro_max;
  u64 disegno_tot;
  u32 disegno_min;
  u32 disegno_max;
  u64 attesa_tot;
  u32 attesa_max;
  u64 foto_tot;
  u64 passo_tot;
};
const BmcFwConti kBmcFwContiVuoti = {0U, 0ULL, 0xFFFFFFFFU, 0U,
                                     0U, 0ULL, 0xFFFFFFFFU, 0U,
                                     0ULL, 0xFFFFFFFFU, 0U,
                                     0ULL, 0U, 0ULL, 0ULL};
BmcFwConti s_bmc_fw_conti = kBmcFwContiVuoti;



bool bmc_fw_col_core3() {
  return !g_kms_active && bmc_v3d_col_core3() && !bmc_v3d_finta();
}


void bmc_fw_conta_lavoro() {
  BmcFwConti &c = s_bmc_fw_conti;
  const u32 t = s_bmc_fw_durata_us;
  const u32 d = s_bmc_fw_disegno_us;
  c.lavori++;
  c.lavoro_tot += t;
  if (t < c.lavoro_min) {
    c.lavoro_min = t;
  }
  if (t > c.lavoro_max) {
    c.lavoro_max = t;
  }
  c.disegno_tot += d;
  if (d < c.disegno_min) {
    c.disegno_min = d;
  }
  if (d > c.disegno_max) {
    c.disegno_max = d;
  }
}




void bmc_fw_conta_core1(u32 us, bool col_core3) {
  BmcFwConti &c = s_bmc_fw_conti;
  c.quadri++;
  c.core1_tot += us;
  if (us < c.core1_min) {
    c.core1_min = us;
  }
  if (us > c.core1_max) {
    c.core1_max = us;
  }
  c.attesa_tot += s_bmc_fw_attesa1_us;
  if (s_bmc_fw_attesa1_us > c.attesa_max) {
    c.attesa_max = s_bmc_fw_attesa1_us;
  }
  c.foto_tot += col_core3 ? s_bmc_fw_foto_byte : 0U;
  c.passo_tot += s_bmc_fw_passo_us;
  if (c.quadri < 250U) {
    return;
  }
  if (g_bmc_fw_per_scelta) {
    printf("boot: bmc64 v3d firmware %s quadri=%u core1_us=%u/%u/%u "
           "lavoro_us=%u/%u/%u disegno_us=%u/%u/%u attesa_us=%u/%u "
           "foto_kb=%u passo_us=%u\r\n",
           col_core3 ? "core3" : "core1", c.quadri, c.core1_min,
           static_cast<u32>(c.core1_tot / c.quadri), c.core1_max,
           c.lavori != 0U ? c.lavoro_min : 0U,
           c.lavori != 0U ? static_cast<u32>(c.lavoro_tot / c.lavori) : 0U,
           c.lavoro_max,
           c.lavori != 0U ? c.disegno_min : 0U,
           c.lavori != 0U ? static_cast<u32>(c.disegno_tot / c.lavori) : 0U,
           c.disegno_max,
           static_cast<u32>(c.attesa_tot / c.quadri), c.attesa_max,
           static_cast<u32>(c.foto_tot / c.quadri / 1024U),
           static_cast<u32>(c.passo_tot / c.quadri));
  }
  c = kBmcFwContiVuoti;
}




void bmc_v3d_aspetta() {
  if (!s_bmc_in_volo ||
      CMultiCoreSupport::ThisCore() != s_bmc_core_mandante) {
    return;
  }
  const u64 inizio = CTimer::GetClockTicks64();
  while (__atomic_load_n(&fbl_done, __ATOMIC_ACQUIRE) == 0U) {
    if (CTimer::GetClockTicks64() - inizio > 2000000ULL) {



      s_bmc_bloccato = 1;
      s_bmc_in_volo = 0;
      printf("boot: bmc64 v3d core 3 fermo da 2 s: %s\r\n",
             s_bmc_tipo == 1
                 ? "il compositore del firmware torna al core 1"
                 : "V3D lasciata, si torna allo scalatore");
      return;
    }
    CTimer::SimpleusDelay(2);
  }
  sem_dec(&fbl_done);
  s_bmc_in_volo = 0;
  if (s_bmc_tipo == 1) {
    bmc_fw_conta_lavoro();
    return;
  }
  bmc_v3d_conta_lavoro(s_bmc_lavoro.durata_us);
  if (!s_bmc_lavoro.esito) {
    s_bmc_fallito = 1;
    printf("boot: bmc64 v3d core 3: il lavoro non e' andato, da qui lo fa "
           "il core 1 come BMX\r\n");
  }
}




bool bmc_v3d_fotografa(BmcV3dLavoro *j, const uint8_t *pixels, u32 pitch,
                       u32 altezza, const u16 *tavolozza) {
  const size_t serve = static_cast<size_t>(pitch) * altezza;
  if (pixels == nullptr || serve == 0U) {
    return false;
  }
  if (serve > s_bmc_foto_byte) {
    free(s_bmc_foto);
    s_bmc_foto = static_cast<uint8_t *>(malloc(serve));
    s_bmc_foto_byte = s_bmc_foto != nullptr ? serve : 0U;
    if (s_bmc_foto == nullptr) {
      printf("boot: bmc64 v3d core 3: niente memoria per la foto della "
             "tela (%u byte), resta il core 1\r\n",
             static_cast<unsigned>(serve));
      s_bmc_fallito = 1;
      return false;
    }
  }
  memcpy(s_bmc_foto, pixels, serve);
  j->source.pixels = s_bmc_foto;
  if (tavolozza != nullptr) {
    memcpy(j->palette, tavolozza, sizeof j->palette);
    j->source.palette_rgb565 = j->palette;
  }
  return true;
}



bool bmc_v3d_esegui(BmcV3dLavoro &j) {
  if (bmc_v3d_finta()) {
    const v3dcrt::InputFramebuffer &s = j.source;
    return banco_v3d_finto(s.pixels, s.pitch, s.source_rect.x,
                           s.source_rect.y, s.source_rect.width,
                           s.source_rect.height) != 0;
  }
  FrameBufferLayer *const layer = j.layer;
  const v3dcrt::InputFramebuffer &source = j.source;
  const v3dcrt::OutputFramebuffer &target = j.target;
  const v3dcrt::EffectParams &params = j.params;
  const unsigned buffer_index = j.buffer_index;
  pi5kms::Framebuffer &render_framebuffer = g_kms_framebuffers[buffer_index];
  const int screen_w = j.screen_w;
  const int screen_h = j.screen_h;
  const bool wait_for_vblank = j.wait_for_vblank;
  const bool has_overlay = j.has_overlay;
  const unsigned overlay_count = j.overlay_count;
  const u32 overlay_mask = j.overlay_mask;
  const pi5kms::Plane *const overlay_planes = j.overlay_planes;
  const u64 present_start_us = j.present_start_us;
  const u64 overlay_prep_done_us = j.overlay_prep_done_us;
  const bool &v3d_presented = j.v3d_presented;
  pi5kms::Plane &v3d_plane = j.v3d_plane;
  (void)render_framebuffer;
  (void)screen_w;
  (void)screen_h;
  (void)wait_for_vblank;
  (void)has_overlay;
  (void)overlay_count;
  (void)overlay_planes;
  (void)v3d_plane;

  if (!v3dcrt::RenderFrame(source, target, params)) {
    record_v3d_fallback(kV3dFallbackRenderFailed);
    return false;
  }
  const u64 render_done_us = CTimer::GetClockTicks64();

#if RASPPI == 4
  v3dcrt::OutputReadback pi4_readback = {};
  if (!v3dcrt::ReadCompletedFrame(&pi4_readback)) {
    printf("boot: pi4v3d completed target unavailable; "
           "using normal render fallback\r\n");
    record_v3d_fallback(kV3dFallbackRenderFailed);
    return false;
  }
  (void)build_pi4_v3d_plane(
      pi4_readback,
      static_cast<s32>(j.dst_x),
      static_cast<s32>(j.dst_y),
      static_cast<u32>(j.dst_w),
      static_cast<u32>(j.dst_h),
      screen_w, screen_h, &v3d_plane);
#endif

  if (v3d_presented) {
    g_capture_v3d_layer = layer;
    record_v3d_present_stats(
        overlay_mask, present_start_us, overlay_prep_done_us,
        render_done_us, render_done_us);
    return true;
  }

#if RASPPI == 4
  // RCL completion is synchronous, so the plane always describes this call's
  // completed frame.  Present it together with any overlays before the next
  // call is allowed to reuse the alternate render slot.
  if (v3d_plane.framebuffer_bus_address != 0U) {
    pi5kms::Plane planes[FB_NUM_LAYERS];
    unsigned plane_count = 0U;
    planes[plane_count++] = v3d_plane;
    for (unsigned i = 0; i < overlay_count; ++i) {
      planes[plane_count++] = overlay_planes[i];
    }

    if (!presenta_e_ricorda(    planes, plane_count,
                                (u32)screen_w, (u32)screen_h,
                                wait_for_vblank)) {
      static bool direct_present_failure_logged = false;
      if (!direct_present_failure_logged) {
        direct_present_failure_logged = true;
        printf("boot: pi4v3d direct HVS present failed; "
               "using CPU scanout copy\r\n");
      }
    } else {
      static bool direct_present_logged = false;
      if (!direct_present_logged) {
        direct_present_logged = true;
        printf("boot: pi4v3d direct scanout hvs=0x%08x pitch=%u "
               "src=0,0 %ux%u dst=%d,%d %dx%d display=%dx%d "
               "filter=%s slots=2\r\n",
               v3d_plane.framebuffer_bus_address, v3d_plane.pitch,
               v3d_plane.source.width, v3d_plane.source.height,
               j.dst_x, j.dst_y, j.dst_w, j.dst_h,
               screen_w, screen_h,
               g_interpolation_enabled ? "mitchell" : "nearest");
      }
      g_kms_front_buffer_index = buffer_index;
      g_capture_v3d_layer = layer;
      const u64 present_done_us = CTimer::GetClockTicks64();
      record_v3d_present_stats(
          overlay_mask, present_start_us, overlay_prep_done_us,
          render_done_us, present_done_us);
      return true;
    }
  }

  if (!copy_pi4_v3d_to_kms_fallback(
          pi4_readback, render_framebuffer,
          static_cast<u32>(j.dst_x),
          static_cast<u32>(j.dst_y),
          static_cast<u32>(j.dst_w),
          static_cast<u32>(j.dst_h))) {
    printf("boot: pi4v3d CPU scanout copy failed\r\n");
    record_v3d_fallback(kV3dFallbackPresentFailed);
    return false;
  }
  if (has_overlay) {
    pi5kms::Plane planes[FB_NUM_LAYERS];
    unsigned plane_count = 0U;
    planes[plane_count++] = make_kms_framebuffer_plane(render_framebuffer);
    for (unsigned i = 0U; i < overlay_count; ++i) {
      planes[plane_count++] = overlay_planes[i];
    }
    if (!presenta_e_ricorda(    planes, plane_count,
                                static_cast<u32>(screen_w),
                                static_cast<u32>(screen_h),
                                wait_for_vblank)) {
      printf("boot: pi4v3d fallback overlay present failed\r\n");
      record_v3d_fallback(kV3dFallbackPresentFailed);
      return false;
    }
    g_kms_front_buffer_index = buffer_index;
  } else if (!present_kms_framebuffer(buffer_index, wait_for_vblank)) {
    printf("boot: pi4v3d fallback present failed\r\n");
    record_v3d_fallback(kV3dFallbackPresentFailed);
    return false;
  }
#else
  if (has_overlay) {
    pi5kms::Plane planes[FB_NUM_LAYERS];
    unsigned plane_count = 0;
    if (v3d_plane.framebuffer_bus_address != 0) {
      planes[plane_count++] = v3d_plane;
    } else {
      planes[plane_count++] =
          make_kms_framebuffer_plane(g_kms_framebuffers[buffer_index]);
    }
    for (unsigned i = 0; i < overlay_count; ++i) {
      planes[plane_count++] = overlay_planes[i];
    }

    if (!presenta_e_ricorda(    planes, plane_count,
                                (u32)screen_w, (u32)screen_h,
                                wait_for_vblank)) {
      printf("boot: v3dcrt overlay present failed\r\n");
      record_v3d_fallback(kV3dFallbackPresentFailed);
      return false;
    }

    g_kms_front_buffer_index = buffer_index;
    g_capture_v3d_layer = layer;
    const u64 present_done_us = CTimer::GetClockTicks64();
    record_v3d_present_stats(
        overlay_mask, present_start_us, overlay_prep_done_us,
        render_done_us, present_done_us);
    return true;
  }

  if (!present_kms_framebuffer(buffer_index, wait_for_vblank)) {
    printf("boot: v3dcrt present failed\r\n");
    record_v3d_fallback(kV3dFallbackPresentFailed);
    return false;
  }
#endif

  g_capture_v3d_layer = layer;
  const u64 present_done_us = CTimer::GetClockTicks64();
  record_v3d_present_stats(
      overlay_mask, present_start_us, overlay_prep_done_us,
      render_done_us, present_done_us);
  return true;
}

}  // namespace





extern "C" int fbl_core3_libero(void) {
  return !s_bmc_in_volo || __atomic_load_n(&fbl_done, __ATOMIC_ACQUIRE) != 0U;
}

extern "C" void fbl_worker_run(void) {
  if (s_bmc_tipo == 1) {
    FrameBufferLayer::BmcLavoroFirmware();
    return;
  }
  BmcV3dLavoro &j = s_bmc_lavoro;
  const u64 inizio = CTimer::GetClockTicks64();
  j.esito = bmc_v3d_esegui(j);
  j.durata_us = static_cast<u32>(CTimer::GetClockTicks64() - inizio);
}

bool FrameBufferLayer::TryV3dPostprocess(FrameBufferLayer **layers,
                                         unsigned layer_count,
                                         int screen_w,
                                         int screen_h,
                                         bool wait_for_vblank) {


  const bool bmc_finta = bmc_v3d_finta();
  if (s_bmc_bloccato) {
    return false;
  }
  if (!bmc_finta && !g_v3d_crt_enabled) {
    return false;
  }
  if (!bmc_finta && !v3dcrt::Requested()) {
    return false;
  }
  if (!bmc_finta && !g_kms_active) {
    record_v3d_fallback(kV3dFallbackKmsInactive);
    return false;
  }
  if (!bmc_finta && !v3dcrt::IsAvailable()) {
    record_v3d_fallback(kV3dFallbackUnavailable);
    return false;
  }
  if (screen_w <= 0 || screen_h <= 0) {
    record_v3d_fallback(kV3dFallbackInvalidScreen);
    return false;
  }

  {
    const int saltata = g_bmc_v3d_preset == v3dcrt::kShaderCrt &&
                        !bmc_v3d_qualche_effetto(g_v3d_effect_params);
    if (saltata != g_bmc_v3d_saltata) {
      g_bmc_v3d_saltata = saltata;
      printf("boot: bmc64 v3d %s\r\n",
             saltata ? "saltata: shader crt senza effetti, scala l'HVS"
                     : "al lavoro: shader con effetti");
    }
    if (saltata && !bmc_finta) {
      return false;
    }
  }

  FrameBufferLayer *layer = nullptr;
  FrameBufferLayer *overlay_layers[FB_NUM_LAYERS];
  unsigned overlay_count = 0;
  u32 overlay_mask = 0;
  bool has_overlay = false;
  for (unsigned i = 0; i < layer_count && i < FB_NUM_LAYERS; ++i) {
    FrameBufferLayer *active_layer = layers[i];
    if (active_layer == nullptr) {
      continue;
    }

    const bool base_candidate =
        !active_layer->transparency_ &&
        (active_layer->layer_ == FB_LAYER_VIC ||
         active_layer->layer_ == FB_LAYER_VDC);
    if (base_candidate) {
      if (layer != nullptr) {
        record_v3d_fallback(kV3dFallbackLayerCount);
        return false;
      }
      layer = active_layer;
      continue;
    }

    has_overlay = true;
    if (active_layer->transparency_ ||
        active_layer->layer_ == FB_LAYER_UI ||
        active_layer->layer_ == FB_LAYER_STATUS) {
      if (overlay_count >= FB_NUM_LAYERS - 1) {
        record_v3d_fallback(kV3dFallbackLayerCount);
        return false;
      }
      overlay_layers[overlay_count++] = active_layer;
      overlay_mask |= FB_LAYER_MASK(active_layer->layer_);
      continue;
    }

    record_v3d_fallback(kV3dFallbackUnsupportedLayer);
    return false;
  }

  if (layer == nullptr || !layer->Showing()) {
    record_v3d_fallback(kV3dFallbackUnsupportedLayer);
    return false;
  }
  if (layer->transparency_ || layer->layer_ == FB_LAYER_UI) {
    record_v3d_fallback(kV3dFallbackOverlayActive);
    return false;
  }
  if (layer->layer_ != FB_LAYER_VIC && layer->layer_ != FB_LAYER_VDC) {
    record_v3d_fallback(kV3dFallbackUnsupportedLayer);
    return false;
  }
  if (layer->pixelmode_ != 0 && layer->pixelmode_ != 1) {
    record_v3d_fallback(kV3dFallbackUnsupportedFormat);
    return false;
  }
  if (layer->src_x_ < 0 || layer->src_y_ < 0 ||
      layer->src_w_ <= 0 || layer->src_h_ <= 0 ||
      layer->src_x_ + layer->src_w_ > layer->fb_width_ ||
      layer->src_y_ + layer->src_h_ > layer->fb_height_) {
    record_v3d_fallback(kV3dFallbackSourceGeometry);
    return false;
  }
  if (layer->dst_x_ < 0 || layer->dst_y_ < 0 ||
      layer->dst_w_ <= 0 || layer->dst_h_ <= 0 ||
      layer->dst_x_ + layer->dst_w_ > screen_w ||
      layer->dst_y_ + layer->dst_h_ > screen_h) {
    record_v3d_fallback(kV3dFallbackDestinationGeometry);
    return false;
  }

  const u64 present_start_us = CTimer::GetClockTicks64();
  const unsigned buffer_index = kms_back_buffer_index();
  pi5kms::Plane overlay_planes[FB_NUM_LAYERS - 1];
  for (unsigned i = 0; i < overlay_count; ++i) {
    if (!BuildKmsLayerPlane(overlay_layers[i], buffer_index,
                            screen_w, screen_h, &overlay_planes[i])) {
      record_v3d_fallback(kV3dFallbackOverlayActive);
      return false;
    }
  }
  const u64 overlay_prep_done_us = CTimer::GetClockTicks64();

  v3dcrt::InputFramebuffer source = {
    layer->pixels_,
    (u32)layer->fb_width_,
    (u32)layer->fb_height_,
    (u32)layer->fb_pitch_,
    layer->pixelmode_ == 1 ? v3dcrt::kPixelFormatRgb565
                           : v3dcrt::kPixelFormatIndexed8,
    layer->pal_565_,
    layer->palette_generation_,
    layer->pixelmode_ == 0 ? palette_signature_rgb565(layer->pal_565_) : 0U,
    {(u32)layer->src_x_, (u32)layer->src_y_,
     (u32)layer->src_w_, (u32)layer->src_h_},
    0U
  };
  bool v3d_presented = false;
  pi5kms::Plane v3d_plane = {};
  const bool allow_direct_scanout = overlay_count == 0;
  pi5kms::Framebuffer &render_framebuffer =
      g_kms_framebuffers[buffer_index];
  uint8_t *render_pixels = render_framebuffer.pixels;
  u32 render_width = render_framebuffer.width;
  u32 render_height = render_framebuffer.height;
  u32 render_pitch = render_framebuffer.pitch;
#if RASPPI == 4
  // Pi4 renders synchronously into one of its two private V3D targets.  The
  // completed target becomes the base HVS plane below, matching Pi5's direct
  // scanout ownership.  The KMS back buffer is touched only by the fallback.
  render_pixels = nullptr;
  render_width = static_cast<u32>(layer->dst_w_);
  render_height = static_cast<u32>(layer->dst_h_);
  render_pitch = render_width * sizeof(u16);
#endif
  v3dcrt::OutputFramebuffer target = {
    &render_framebuffer,
    render_pixels,
    render_width,
    render_height,
    render_pitch,
    render_framebuffer.depth,
    v3dcrt::kPixelFormatRgb565,
    (u32)screen_w,
    (u32)screen_h,
    {(u32)layer->dst_x_, (u32)layer->dst_y_,
     (u32)layer->dst_w_, (u32)layer->dst_h_},
    wait_for_vblank,
    &v3d_presented,
    allow_direct_scanout,
#if RASPPI == 4
    &v3d_plane
#else
    allow_direct_scanout ? nullptr : &v3d_plane
#endif
  };
  v3dcrt::EffectParams params = g_v3d_effect_params;
  params.enable_interpolation = g_interpolation_enabled;
  if (g_v3d_scanline_weight_override_enabled) {
    params.scanline_weight = g_v3d_scanline_weight_override;
  }
  if (g_v3d_scanline_gap_override_enabled) {
    params.scanline_gap_brightness = g_v3d_scanline_gap_override;
  }




  BmcV3dLavoro &j = s_bmc_lavoro;
  j.layer = layer;
  j.dst_x = layer->dst_x_;
  j.dst_y = layer->dst_y_;
  j.dst_w = static_cast<u32>(layer->dst_w_);
  j.dst_h = static_cast<u32>(layer->dst_h_);
  j.screen_w = screen_w;
  j.screen_h = screen_h;
  j.wait_for_vblank = wait_for_vblank;
  j.has_overlay = has_overlay;
  j.overlay_count = overlay_count;
  j.overlay_mask = overlay_mask;
  for (unsigned i = 0; i < overlay_count; ++i) {
    j.overlay_planes[i] = overlay_planes[i];
  }
  j.buffer_index = buffer_index;
  j.present_start_us = present_start_us;
  j.overlay_prep_done_us = overlay_prep_done_us;
  j.source = source;
  j.target = target;
  j.params = params;
  j.v3d_presented = false;
  j.v3d_plane = pi5kms::Plane();
  j.target.presented = &j.v3d_presented;
  if (target.native_rendered_plane != nullptr) {
    j.target.native_rendered_plane = &j.v3d_plane;
  }
  (void)v3d_presented;
  if (bmc_v3d_col_core3() &&
      bmc_v3d_fotografa(&j, layer->pixels_,
                        static_cast<u32>(layer->fb_pitch_),
                        static_cast<u32>(layer->fb_height_),
                        layer->pal_565_)) {
    s_bmc_tipo = 0;
    s_bmc_core_mandante = CMultiCoreSupport::ThisCore();
    s_bmc_in_volo = 1;
    sem_inc(&fbl_job);
    return true;
  }
  const u64 bmc_lavoro_da = CTimer::GetClockTicks64();
  const bool bmc_esito = bmc_v3d_esegui(j);
  bmc_v3d_conta_lavoro(
      static_cast<u32>(CTimer::GetClockTicks64() - bmc_lavoro_da));
  return bmc_esito;
}

bool FrameBufferLayer::initialized_ = false;
CBcmFrameBuffer *FrameBufferLayer::screen_ = nullptr;
uint8_t *FrameBufferLayer::screen_pixels_ = nullptr;
unsigned FrameBufferLayer::screen_pitch_bytes_ = 0;
unsigned FrameBufferLayer::screen_bytes_per_pixel_ = 2;

void FrameBufferLayer::DrawLayerNearest(FrameBufferLayer *layer,
                                        int start_x,
                                        int end_x,
                                        int start_y,
                                        int end_y,
                                        int64_t x_step,
                                        int64_t y_step) {
  for (int y = start_y; y < end_y; y++) {
    int src_y = layer->src_y_ + (int) ((((int64_t) (y - layer->dst_y_)) * y_step) >> 16);
    src_y = min_int(max_int(src_y, 0), layer->fb_height_ - 1);

    uint8_t *dst_row = g_compose_pixels + y * g_compose_pitch_bytes;
    int64_t x_acc = ((int64_t) (start_x - layer->dst_x_)) * x_step;

    for (int x = start_x; x < end_x; x++) {
      int src_x = layer->src_x_ + (int) (x_acc >> 16);
      x_acc += x_step;
      src_x = min_int(max_int(src_x, 0), layer->fb_width_ - 1);

      if (layer->pixelmode_ == 0) {
        const uint8_t *src_row = layer->pixels_ + src_y * layer->fb_pitch_;
        uint8_t index = src_row[src_x];
        if (layer->transparency_) {
          uint32_t argb = layer->pal_argb_[index];
          if ((argb >> 24) == 0) {
            continue;
          }
          write_argb_pixel(dst_row, x, argb);
        } else {
          write_rgb565_pixel(dst_row, x, layer->pal_565_[index]);
        }
      } else {
        const uint16_t *src_row =
            (const uint16_t *) (layer->pixels_ + src_y * layer->fb_pitch_);
        write_rgb565_pixel(dst_row, x, src_row[src_x]);
      }
    }
  }
}

void FrameBufferLayer::DrawLayerBilinear(FrameBufferLayer *layer,
                                         int start_x,
                                         int end_x,
                                         int start_y,
                                         int end_y,
                                         int64_t x_step,
                                         int64_t y_step) {
  int max_src_x = min_int(layer->src_x_ + layer->src_w_ - 1,
                          layer->fb_width_ - 1);
  int max_src_y = min_int(layer->src_y_ + layer->src_h_ - 1,
                          layer->fb_height_ - 1);

  for (int y = start_y; y < end_y; y++) {
    int64_t y_acc = ((int64_t) (y - layer->dst_y_)) * y_step;
    int src_y0 = layer->src_y_ + (int) (y_acc >> 16);
    unsigned frac_y = (unsigned) ((y_acc >> 8) & 0xFF);
    src_y0 = min_int(max_int(src_y0, layer->src_y_), max_src_y);
    int src_y1 = min_int(src_y0 + 1, max_src_y);

    uint8_t *dst_row = g_compose_pixels + y * g_compose_pitch_bytes;
    int64_t x_acc = ((int64_t) (start_x - layer->dst_x_)) * x_step;

    for (int x = start_x; x < end_x; x++) {
      int src_x0 = layer->src_x_ + (int) (x_acc >> 16);
      unsigned frac_x = (unsigned) ((x_acc >> 8) & 0xFF);
      x_acc += x_step;
      src_x0 = min_int(max_int(src_x0, layer->src_x_), max_src_x);
      int src_x1 = min_int(src_x0 + 1, max_src_x);

      if (layer->pixelmode_ == 0) {
        const uint8_t *src_row0 = layer->pixels_ + src_y0 * layer->fb_pitch_;
        const uint8_t *src_row1 = layer->pixels_ + src_y1 * layer->fb_pitch_;
        if (layer->transparency_) {
          uint32_t c00 = layer->pal_argb_[src_row0[src_x0]];
          uint32_t c10 = layer->pal_argb_[src_row0[src_x1]];
          uint32_t c01 = layer->pal_argb_[src_row1[src_x0]];
          uint32_t c11 = layer->pal_argb_[src_row1[src_x1]];
          blend_argb_pixel(dst_row, x,
                           bilinear_argb8888(c00, c10, c01, c11,
                                             frac_x, frac_y));
        } else {
          uint16_t c00 = layer->pal_565_[src_row0[src_x0]];
          uint16_t c10 = layer->pal_565_[src_row0[src_x1]];
          uint16_t c01 = layer->pal_565_[src_row1[src_x0]];
          uint16_t c11 = layer->pal_565_[src_row1[src_x1]];
          write_rgb565_pixel(dst_row, x,
                             bilinear_rgb565(c00, c10, c01, c11,
                                             frac_x, frac_y));
        }
      } else {
        const uint16_t *src_row0 =
            (const uint16_t *) (layer->pixels_ + src_y0 * layer->fb_pitch_);
        const uint16_t *src_row1 =
            (const uint16_t *) (layer->pixels_ + src_y1 * layer->fb_pitch_);
        write_rgb565_pixel(dst_row, x,
                           bilinear_rgb565(src_row0[src_x0],
                                           src_row0[src_x1],
                                           src_row1[src_x0],
                                           src_row1[src_x1],
                                           frac_x,
                                           frac_y));
      }
    }
  }
}

FrameBufferLayer::FrameBufferLayer()
    : pixels_(nullptr),
      fb_width_(0),
      fb_height_(0),
      fb_pitch_(0),
      logical_layer_(-1),
      layer_(0),
      transparency_(false),
      hstretch_(1.6),
      vstretch_(1.0),
      hintstr_(0),
      vintstr_(0),
      use_hintstr_(0),
      use_vintstr_(0),
      valign_(0),
      vpadding_(0),
      halign_(0),
      hpadding_(0),
      h_center_offset_(0),
      v_center_offset_(0),
      rnum_(0),
      leftPadding_(0),
      rightPadding_(0),
      topPadding_(0),
      bottomPadding_(0),
      display_width_(0),
      display_height_(0),
      src_x_(0),
      src_y_(0),
      src_w_(0),
      src_h_(0),
      dst_x_(0),
      dst_y_(0),
      dst_w_(0),
      dst_h_(0),
      showing_(false),
      allocated_(false),
      pixelmode_(0),
      bytes_per_pixel_(1),
      uses_shader_(false),
      dirty_(true),
      palette_generation_(0),
      content_generation_(1) {
  memcpy(pal_565_, default_pal_565, sizeof(pal_565_));
  memcpy(pal_argb_, default_pal_argb, sizeof(pal_argb_));
}

FrameBufferLayer::~FrameBufferLayer() {
  if (allocated_) {
    Free();
  }
}

void FrameBufferLayer::MarkDirty() {
  dirty_ = true;
  if (++content_generation_ == 0U) {
    ++content_generation_;
  }
  const bool independent_overlay_buffer =
      transparency_ ||
      layer_ == FB_LAYER_UI ||
      layer_ == FB_LAYER_STATUS;
  if (!independent_overlay_buffer &&
      layer_ >= 0 && layer_ < FB_NUM_LAYERS) {
    for (unsigned i = 0; i < kKmsBufferCount; ++i) {
      g_kms_hwscale_framebuffer_valid[layer_][i] = false;
    }
  }
}

bool FrameBufferLayer::Initialize() {
  if (initialized_) {
    return true;
  }

  ViceOptions *options = ViceOptions::Get();
  unsigned requested_width = options ? options->GetFramebufferWidth() : 0;
  unsigned requested_height = options ? options->GetFramebufferHeight() : 0;
  unsigned requested_depth = options ? options->GetFramebufferDepth() :
                                      PI5_FRAMEBUFFER_DEFAULT_DEPTH;
  requested_depth = sanitize_framebuffer_depth(requested_depth);

  pi5kms::Mode kms_mode;
  bool kms_mode_resolved = false;
  bool kms_requested = false;
#if RASPPI == 4
  // The AArch64 Pi4 target has no legacy display backend.  Its native KMS
  // ownership is therefore a build capability, independent of pi4kms=0 in a
  // configuration shared with a 32-bit installation.
  kms_requested = options != nullptr;
#else
  kms_requested = options && options->Pi5KmsEnabled();
#endif




  const bool composito = options != nullptr && options->Composito();
  g_bmc_composito = composito;
  const bool doppio_chiesto = !composito && bmc_doppio_hdmi_chiesto(options);
  const bool hdmi1_chiesta = !composito && bmc_hdmi1_chiesta(options);
  if (hdmi1_chiesta) {
    pi5kms::SondaHdmi1();
  }







  const unsigned chiesti_w = requested_width;
  const unsigned chiesti_h = requested_height;
  if (hdmi1_chiesta && kms_requested && !pi5kms::HdmiPortSupported(1U)) {
    printf("boot: fb_display=1: questo scalatore la HDMI 1 non la guida, "
           "lo schermo resta al firmware\r\n");
    kms_requested = false;
  }
  if (doppio_chiesto || (hdmi1_chiesta && !kms_requested)) {
    kms_requested = false;
    bmc_misura_doppio(&requested_width, &requested_height,
                      doppio_chiesto ? "doppio_hdmi" : "fb_display=1");
  }





  if (composito) {
    kms_requested = false;
    printf("boot: composito=1: l'uscita AV del Pi 4 B, coi tempi del "
           "firmware (niente modo nostro, niente shader)\r\n");
    unsigned misura_w = 0, misura_h = 0;
    bmc_misura_doppio(&misura_w, &misura_h, "composito");
    if (requested_width == 0 || requested_height == 0) {
      requested_width = g_bmc_disp_vero_w & ~1u;
      requested_height = g_bmc_disp_vero_h & ~1u;
    }
    printf("boot: composito: tela %ux%u (il display intero)\r\n",
           requested_width, requested_height);
  }
  if (kms_requested) {
    pi5kms::RequestHdmiPort(hdmi1_chiesta ? 1U : 0U);
    kms_mode_resolved =
        pi5kms::ResolveBmcMode(options->GetHdmiGroup(),
                               options->GetHdmiMode(),
                               options->GetPi5KmsTimings(),
                               options->GetPi5KmsMode(),
                               &kms_mode);
    if (kms_mode_resolved) {
      if (requested_width == 0) {
        requested_width = kms_mode.width;
      }
      if (requested_height == 0) {
        requested_height = kms_mode.height;
      }
    } else {
      printf("boot: " BMX_NATIVE_KMS_LOG
             " no matching mode, using firmware mode\r\n");
    }
  }

  printf("boot: " BMX_NATIVE_BOARD_LOG " fbl request %ux%u depth %u\r\n",
         requested_width, requested_height, requested_depth);

  if (kms_mode_resolved && requested_depth != 16) {
    printf("boot: " BMX_NATIVE_KMS_LOG
           " forcing framebuffer depth 16 from %u\r\n",
           requested_depth);
    requested_depth = 16;
  }

  if (kms_mode_resolved && !pi5kms::SetMode(kms_mode)) {
    printf("boot: " BMX_NATIVE_KMS_LOG
           " mode switch failed, using firmware mode\r\n");
    kms_mode_resolved = false;
  }


  if (hdmi1_chiesta && kms_requested && !kms_mode_resolved) {
    requested_width = chiesti_w;
    requested_height = chiesti_h;
    bmc_misura_doppio(&requested_width, &requested_height,
                      "fb_display=1, scalatore non preso");
  }

  memset(g_layers, 0, sizeof(g_layers));
  g_kms_active = false;
  screen_pixels_ = nullptr;
  screen_pitch_bytes_ = 0;

  if (kms_mode_resolved) {
    screen_ = nullptr;
    if (requested_depth != 16) {
      printf("boot: " BMX_NATIVE_KMS_LOG
             " unsupported framebuffer depth %u\r\n",
             requested_depth);
      return false;
    }

    g_effective_width = (int) kms_mode.width;
    g_effective_height = (int) kms_mode.height;
    if (g_effective_width <= 0 || g_effective_height <= 0) {
      printf("boot: " BMX_NATIVE_KMS_LOG
             " invalid framebuffer dimensions %dx%d\r\n",
             g_effective_width, g_effective_height);
      return false;
    }

    for (unsigned i = 0; i < kKmsBufferCount; ++i) {
      if (!pi5kms::CreateFramebuffer(kms_mode.width, kms_mode.height,
                                     requested_depth,
                                     &g_kms_framebuffers[i])) {
        printf("boot: " BMX_NATIVE_KMS_LOG
               " framebuffer %u allocation failed\r\n", i);
        destroy_kms_scanout_framebuffers();
        return false;
      }
    }
    g_kms_front_buffer_index = 0;
    screen_pixels_ = g_kms_framebuffers[g_kms_front_buffer_index].pixels;
    screen_pitch_bytes_ = g_kms_framebuffers[g_kms_front_buffer_index].pitch;
    screen_bytes_per_pixel_ =
        g_kms_framebuffers[g_kms_front_buffer_index].depth / 8;
    g_framebuffer_bytes_per_pixel = screen_bytes_per_pixel_;
    if (screen_pixels_ == nullptr || screen_pitch_bytes_ == 0 ||
        screen_bytes_per_pixel_ != 2) {
      printf("boot: " BMX_NATIVE_KMS_LOG
             " invalid framebuffer layout\r\n");
      destroy_kms_scanout_framebuffers();
      return false;
    }

    pi5kms::Plane initial_plane =
        make_kms_framebuffer_plane(g_kms_framebuffers[g_kms_front_buffer_index]);
    if (!pi5kms::ConfigureScanout(initial_plane, kms_mode.width, kms_mode.height)) {
      printf("boot: " BMX_NATIVE_KMS_LOG " scanout setup failed\r\n");
      destroy_kms_scanout_framebuffers();
      return false;
    }
    g_kms_active = true;

    g_bmc_display_primo = pi5kms::ActiveHdmiPort();
    if (hdmi1_chiesta && g_bmc_display_primo != 1U) {
      printf("boot: fb_display=1, ma all'avvio il firmware la HDMI 1 non "
             "l'ha accesa (nessun monitor?): resto sulla HDMI 0\r\n");
    }
    printf("boot: schermo principale sulla HDMI %u, allo scalatore\r\n",
           g_bmc_display_primo);
  } else {
    g_bmc_display_primo = bmc_display_principale();
    screen_ = new CBcmFrameBuffer(requested_width,
                                  requested_height,
                                  requested_depth,
                                  0, 0, g_bmc_display_primo);
    if (screen_ == nullptr) {
      printf("boot: " BMX_NATIVE_BOARD_LOG
             " firmware framebuffer allocation failed\r\n");
      return false;
    }
    if (!screen_->Initialize()) {
      printf("boot: " BMX_NATIVE_BOARD_LOG
             " firmware framebuffer initialization failed\r\n");
      delete screen_;
      screen_ = nullptr;
      return false;
    }

    bmc_misura_quadro_firmware(screen_);
    screen_pixels_ = (uint8_t *) (uintptr) screen_->GetBuffer();
    screen_pitch_bytes_ = screen_->GetPitch();
    screen_bytes_per_pixel_ =
        framebuffer_bytes_per_pixel(screen_, requested_depth);
    g_framebuffer_bytes_per_pixel = screen_bytes_per_pixel_;

    g_effective_width = effective_screen_width(screen_, screen_bytes_per_pixel_);
    g_effective_height = effective_screen_height(screen_);
    if (screen_pixels_ == nullptr || screen_pitch_bytes_ == 0 ||
        (screen_bytes_per_pixel_ != 2 && screen_bytes_per_pixel_ != 4) ||
        g_effective_width <= 0 || g_effective_height <= 0) {
      printf("boot: " BMX_NATIVE_BOARD_LOG
             " firmware framebuffer returned invalid layout\r\n");
      delete screen_;
      screen_ = nullptr;
      screen_pixels_ = nullptr;
      screen_pitch_bytes_ = 0;
      return false;
    }
  }

#if RASPPI == 4








  if (composito && screen_ != nullptr && !g_kms_active) {
    const u32 tela_w = (u32) g_effective_width;
    const u32 tela_h = (u32) g_effective_height;
    u32 canale_w = 0U;
    u32 canale_h = 0U;
    const bool chiesto = options->CompositoHvs();




    const bool quadro_vero = g_bmc_fw_quadro_us >= 12000UL &&
                             g_bmc_fw_quadro_us <= 25000UL;
    const bool provato = chiesto && quadro_vero &&
                         screen_bytes_per_pixel_ == 2;
    bool preso = provato &&
        pi5kms::SetCompositeMode(tela_w, tela_h, &canale_w, &canale_h);
    for (unsigned i = 0; preso && i < kKmsBufferCount; ++i) {
      preso = pi5kms::CreateFramebuffer(tela_w, tela_h, 16U,
                                        &g_kms_framebuffers[i]);
    }
    if (preso) {
      g_kms_front_buffer_index = 0;
      const pi5kms::Plane primo =
          make_kms_framebuffer_plane(g_kms_framebuffers[0]);
      preso = pi5kms::ConfigureScanout(primo, tela_w, tela_h);
    }
    if (preso) {
      g_kms_active = true;
      screen_pixels_ = g_kms_framebuffers[0].pixels;
      screen_pitch_bytes_ = g_kms_framebuffers[0].pitch;
      printf("boot: composito allo scalatore della GPU (HVS): tela %ux%u, "
             "canale %ux%u\r\n", tela_w, tela_h, canale_w, canale_h);
    } else {
      destroy_kms_scanout_framebuffers();
      if (provato) {
        pi5kms::RestoreComposite();
      }
      printf("boot: composito: %s, il core 3 compone come prima\r\n",
             !chiesto ? "composito_hvs=0"
             : !quadro_vero ? "il ritorno di quadro del firmware non e' "
                              "quello di un composito, l'HVS non si tocca"
                            : "lo scalatore non si prende");
    }
  }
#endif

  g_compose_pitch_bytes = (unsigned) g_effective_width *
                          screen_bytes_per_pixel_;



  g_compose_grezzo = (uint8_t *) malloc((size_t) g_compose_pitch_bytes *
                                        (size_t) g_effective_height + 64U);
  g_compose_pixels = g_compose_grezzo == nullptr ? nullptr :
      (uint8_t *) (((uintptr_t) g_compose_grezzo + 63U) & ~(uintptr_t) 63U);
  if (g_compose_pixels == nullptr) {
    printf("boot: " BMX_NATIVE_BOARD_LOG
           " composition framebuffer allocation failed\r\n");
    if (g_kms_active) {
      destroy_kms_scanout_framebuffers();
      g_kms_active = false;
    } else {
      delete screen_;
      screen_ = nullptr;
    }
    screen_pixels_ = nullptr;
    screen_pitch_bytes_ = 0;
    return false;
  }

  memset(g_compose_pixels, 0, (size_t) g_compose_pitch_bytes *
         (size_t) g_effective_height);
  printf("boot: fbl copia allineata: tela resto %u passo %u, schermo "
         "resto %u passo %u\r\n",
         (unsigned) ((uintptr_t) g_compose_pixels & 63U),
         g_compose_pitch_bytes,
         (unsigned) ((uintptr_t) screen_pixels_ & 63U),
         (unsigned) screen_pitch_bytes_);
  memset(g_software_layers, 0, sizeof g_software_layers);
  memset(g_software_kms_pending, 0, sizeof g_software_kms_pending);
  g_software_composition_valid = false;
  g_software_composition_interpolation = g_interpolation_enabled;
  memset(screen_pixels_, 0, (size_t) g_effective_height * screen_pitch_bytes_);

  if (g_kms_active) {
    for (unsigned i = 0; i < kKmsBufferCount; ++i) {
      pi5kms::FlushFramebuffer(g_kms_framebuffers[i]);
    }
    printf("boot: " BMX_NATIVE_BOARD_LOG
           " fbl kms hw %dx%d depth %u pitch %u size %u buffer 0x%08x\r\n",
           g_effective_width, g_effective_height, requested_depth,
           screen_pitch_bytes_, g_kms_framebuffers[g_kms_front_buffer_index].size,
           (u32) (uintptr) screen_pixels_);
  } else {
    printf("boot: " BMX_NATIVE_BOARD_LOG
           " fbl hw %ux%u virt %ux%u depth %u pitch %u size %u buffer 0x%08x\r\n",
           screen_->GetWidth(), screen_->GetHeight(),
           screen_->GetVirtWidth(), screen_->GetVirtHeight(),
           screen_->GetDepth(), screen_->GetPitch(), screen_->GetSize(),
           screen_->GetBuffer());
  }
  printf("boot: " BMX_NATIVE_BOARD_LOG
         " fbl direct bpp %u effective %dx%d compose_pitch %u\r\n",
         screen_bytes_per_pixel_, g_effective_width, g_effective_height,
         g_compose_pitch_bytes);
  const v3dcrt::ShaderPreset v3dcrt_shader =
      v3dcrt::ParseShaderPreset(options ? options->GetV3DCrtShader() : nullptr);
  g_bmc_v3d_preset = v3dcrt_shader;


  s_bmc_core3 = (options == nullptr || options->V3DCrtCore3()) ? 1 : 0;
  printf("boot: bmc64 v3d core 3 %s\r\n",
         s_bmc_core3 ? "si' (render e presentazione sul core 3)"
                     : "no (v3dcrt_core3=0: tutto sul core 1 come BMX)");
  const v3dcrt::BootTestMode v3dcrt_test =
      v3dcrt::ParseBootTestMode(options ? options->GetV3DCrtTest() : nullptr);
  const v3dcrt::FragmentPackageMode v3dcrt_fragment_package =
      v3dcrt::ParseFragmentPackageMode(
          options ? options->GetV3DCrtFragmentPackage() : nullptr);
  const v3dcrt::RenderResolution v3dcrt_render_resolution =
      v3dcrt::ParseRenderResolution(
          options ? options->GetV3DCrtRenderResolution() : nullptr);
  if (v3dcrt_shader == v3dcrt::kShaderScanlines) {
    g_v3d_effect_params.enable_scanlines = true;
  }
  const bool v3dcrt_requested =
      options && (options->V3DCrtEnabled() ||
                  v3dcrt_test != v3dcrt::kBootTestOff);
  g_v3d_scanline_weight_override_enabled =
      options && options->GetV3DCrtScanlineWeight(
          &g_v3d_scanline_weight_override);
  g_v3d_scanline_gap_override_enabled =
      options && options->GetV3DCrtScanlineGapBrightness(
          &g_v3d_scanline_gap_override);
  const bool v3dcrt_fragment_probe_wait_vblank =
      !options || options->GetV3DCrtFragmentProbeWaitVblank();
  if (g_v3d_scanline_weight_override_enabled) {
    printf("boot: v3dcrt option scanline_weight_x100=%u\r\n",
           param_x100(g_v3d_scanline_weight_override));
  }
  if (g_v3d_scanline_gap_override_enabled) {
    printf("boot: v3dcrt option scanline_gap_brightness_x100=%u\r\n",
           param_x100(g_v3d_scanline_gap_override));
  }
  if (v3dcrt_shader == v3dcrt::kShaderFragmentProbe) {
    printf("boot: v3dcrt option fragment_probe_wait_vblank=%u\r\n",
           v3dcrt_fragment_probe_wait_vblank ? 1U : 0U);
  }
  if (v3dcrt_fragment_package != v3dcrt::kFragmentPackageDefault) {
    printf("boot: v3dcrt option fragment_package=%s\r\n",
           v3dcrt::FragmentPackageModeName(v3dcrt_fragment_package));
  }
  printf("boot: v3dcrt option render_resolution=%s\r\n",
         v3dcrt::RenderResolutionName(v3dcrt_render_resolution));


  v3dcrt::Configure(v3dcrt_requested && !composito, g_kms_active,
                    v3dcrt_shader,
                    v3dcrt_test, v3dcrt_fragment_probe_wait_vblank,
                    v3dcrt_fragment_package, v3dcrt_render_resolution);
  v3dcrt::Initialize();
  run_v3d_boot_test(v3dcrt_test);

  bmc_dopo_initialize(screen_, screen_pixels_, screen_pitch_bytes_,
                      screen_bytes_per_pixel_, requested_width,
                      requested_height);
  initialized_ = true;
  return true;
}

void FrameBufferLayer::Shutdown() {
  bmc_v3d_aspetta();
  v3dcrt::Shutdown();

  free(g_compose_grezzo);
  g_compose_grezzo = nullptr;
  g_compose_pixels = nullptr;
  g_compose_pitch_bytes = 0;
  memset(g_software_layers, 0, sizeof g_software_layers);
  memset(g_software_kms_pending, 0, sizeof g_software_kms_pending);
  g_software_composition_valid = false;

  for (unsigned layer = 0; layer < FB_NUM_LAYERS; ++layer) {
    for (unsigned i = 0; i < kKmsBufferCount; ++i) {
      pi5kms::DestroyFramebuffer(&g_kms_hwscale_framebuffers[layer][i]);
      g_kms_hwscale_framebuffer_valid[layer][i] = false;
      reset_indexed_layer_shadow(layer, i);
    }
    g_kms_hwscale_front_buffer_index[layer] = 0;
    g_kms_hwscale_front_buffer_valid[layer] = false;
  }
  for (unsigned i = 0; i < kKmsBufferCount; ++i) {
    pi5kms::DestroyFramebuffer(&g_kms_framebuffers[i]);
  }

  delete screen_;
  screen_ = nullptr;
  screen_pixels_ = nullptr;
  screen_pitch_bytes_ = 0;
  screen_bytes_per_pixel_ = 2;
  g_framebuffer_bytes_per_pixel = 2;
  g_effective_width = 0;
  g_effective_height = 0;
  g_kms_active = false;
  g_kms_front_buffer_index = 0;
  g_interpolation_enabled = false;
  g_v3d_crt_enabled = true;
  g_v3d_crt_enabled_initialized = false;
  g_v3d_scanline_weight_override_enabled = false;
  g_v3d_scanline_gap_override_enabled = false;
  g_v3d_scanline_weight_override = 0.0f;
  g_v3d_scanline_gap_override = 0.0f;
  g_v3d_effect_params = v3dcrt::DefaultEffectParams();
  g_capture_v3d_layer = nullptr;
  memset(g_layers, 0, sizeof(g_layers));
  memset(&g_v3d_fallback_stats, 0, sizeof(g_v3d_fallback_stats));
  memset(&g_v3d_menu_param_log, 0, sizeof(g_v3d_menu_param_log));
  reset_v3d_present_stats();

  initialized_ = false;
}

bool FrameBufferLayer::OGLInit() { return false; }

bool FrameBufferLayer::ShaderBackendAvailable() {
  return v3dcrt::IsAvailable();
}

bool FrameBufferLayer::ShaderBackendAvailableForLayer(int logical_layer) {
  return v3dcrt::IsAvailable() &&
         (logical_layer == FB_LAYER_VIC || logical_layer == FB_LAYER_VDC);
}

int FrameBufferLayer::Allocate(int pixelmode, uint8_t **pixels,
                               int width, int height, int *pitch) {
  bmc_v3d_aspetta();
  assert(!allocated_);
  if (!Initialize()) {
    return -1;
  }

  pixelmode_ = pixelmode;
  bytes_per_pixel_ = pixelmode == 1 ? 2 : 1;
  fb_width_ = width;
  fb_height_ = height;
  fb_pitch_ = ALIGN_UP(width * bytes_per_pixel_, 32);

  display_width_ = g_effective_width;
  display_height_ = g_effective_height;

  if (pixels_ == nullptr) {
    pixels_ = (uint8_t *) malloc(fb_pitch_ * fb_height_);
  }
  assert(pixels_ != nullptr);

  if (pixels) {
    *pixels = pixels_;
  }
  if (pitch) {
    *pitch = fb_pitch_;
  }

  src_x_ = 0;
  src_y_ = 0;
  src_w_ = width;
  src_h_ = height;
  dst_x_ = 0;
  dst_y_ = 0;
  dst_w_ = width;
  dst_h_ = height;
  allocated_ = true;
  MarkDirty();

  if (layer_ >= 0 && layer_ < FB_NUM_LAYERS && g_layers[layer_] == nullptr) {
    g_layers[layer_] = this;
  }

  printf("boot: " BMX_NATIVE_BOARD_LOG
         " fbl alloc layer %d pixelmode %d fb %dx%d pitch %d display %ux%u\r\n",
         layer_, pixelmode_, fb_width_, fb_height_, fb_pitch_,
         display_width_, display_height_);

  return 0;
}

int FrameBufferLayer::ReAllocate(bool shader_enable) {
  bmc_v3d_aspetta();
  const bool enabled = shader_enable;
  const bool changed = !g_v3d_crt_enabled_initialized ||
                       g_v3d_crt_enabled != enabled;

  g_v3d_crt_enabled = enabled;
  g_v3d_crt_enabled_initialized = true;
  uses_shader_ = false;

  if (changed) {
    reset_v3d_present_stats();
    printf("boot: v3dcrt menu master=%s present=%s\r\n",
           enabled ? "on" : "off",
           enabled ? "v3d" : "framebuffer");
    if (initialized_) {


      passo_prima(enabled ? "shader acceso: presentazione"
                          : "shader spento: presentazione");
      PresentLayerList(false, nullptr, 0);
      passo_fatto(enabled ? "shader acceso: presentazione"
                          : "shader spento: presentazione");
    }
  }

  return 0;
}

void FrameBufferLayer::Clear() {
  bmc_v3d_aspetta();
  assert(allocated_);
  memset(pixels_, 0, fb_pitch_ * fb_height_);
  MarkDirty();
}

void FrameBufferLayer::FreeInternal(bool keepPixels) {
  bmc_v3d_aspetta();
  if (!allocated_) {
    return;
  }

  showing_ = false;
  allocated_ = false;

  if (!keepPixels) {
    free(pixels_);
    pixels_ = nullptr;
    fb_width_ = 0;
    fb_height_ = 0;
    fb_pitch_ = 0;
  }
}

void FrameBufferLayer::Free() {
  FreeInternal(false);
}

void FrameBufferLayer::Show() {
  if (showing_) {
    return;
  }

  assert(hstretch_ != 0);
  assert(vstretch_ != 0);

  int lpad_abs = display_width_ * leftPadding_;
  int rpad_abs = display_width_ * rightPadding_;
  int tpad_abs = display_height_ * topPadding_;
  int bpad_abs = display_height_ * bottomPadding_;

  int avail_width = display_width_ - lpad_abs - rpad_abs;
  int avail_height = display_height_ - tpad_abs - bpad_abs;

  int dst_w;
  int dst_h;

  if (hstretch_ < 0) {
    dst_w = avail_width * vstretch_;
    dst_h = avail_width / -hstretch_;
    if (dst_w > avail_width) {
      dst_w = avail_width;
    }
    if (dst_h > avail_height) {
      dst_h = avail_height;
    }
  } else {
    dst_h = avail_height * vstretch_;
    if (use_vintstr_) {
      dst_h = vintstr_;
    }
    dst_w = avail_height * hstretch_;
    if (use_hintstr_) {
      dst_w = hintstr_;
    }
    if (dst_h > avail_height) {
      dst_h = avail_height;
    }
    if (dst_w > avail_width) {
      dst_w = avail_width;
    }
  }

  if (use_hintstr_ && use_vintstr_) {
    fit_to_available_preserving_aspect(&dst_w, &dst_h,
                                       avail_width, avail_height);
  }











  const bool bmc_barre_288 = g_bmc_composito && layer_ == FB_LAYER_STATUS;
  if (bmc_barre_288) {
    dst_w = avail_width;
    dst_h = src_h_ > 0 ? src_h_ : fb_height_;
    if (dst_h > avail_height) {
      dst_h = avail_height;
    }
  }

  int oy;
  switch (valign_) {
    case 0:
      oy = (avail_height - dst_h) / 2 + v_center_offset_;
      break;
    case -1:
      oy = vpadding_;
      break;
    case 1:
      oy = avail_height - dst_h - vpadding_;
      break;
    default:
      oy = 0;
      break;
  }

  int ox;
  switch (halign_) {
    case 0:
      ox = (avail_width - dst_w) / 2 + h_center_offset_;
      break;
    case -1:
      ox = hpadding_;
      break;
    case 1:
      ox = avail_width - dst_w - hpadding_;
      break;
    default:
      ox = 0;
      break;
  }









  if (g_bmc_composito && display_height_ == 576) {
    ox += 8;
  }
  dst_x_ = ox + lpad_abs;
  dst_y_ = oy + tpad_abs;
  if (bmc_barre_288) {
    dst_y_ &= ~1;
  }
  dst_w_ = dst_w;
  dst_h_ = dst_h;
  showing_ = true;
  MarkDirty();

  printf("boot: " BMX_NATIVE_BOARD_LOG
         " fbl show layer %d src %d,%d %dx%d dst %d,%d %dx%d display %ux%u\r\n",
         layer_, src_x_, src_y_, src_w_, src_h_,
         dst_x_, dst_y_, dst_w_, dst_h_, display_width_, display_height_);

  PresentLayer(false, this);
}

void FrameBufferLayer::Hide() {
  if (!showing_) {
    return;
  }

  showing_ = false;
  PresentLayer(false, this);
}

void *FrameBufferLayer::GetPixels() {
  return pixels_;
}

void FrameBufferLayer::FrameReady(int to_offscreen) {
  (void) to_offscreen;
  MarkDirty();
}

void FrameBufferLayer::PresentLayer(bool sync, FrameBufferLayer *layer) {
  if (layer) {
    layer->MarkDirty();
  }
  PresentLayerList(sync, nullptr, 0);
}

void FrameBufferLayer::PresentLayers(bool sync, FrameBufferLayer *layers,
                                     uint32_t ready_mask) {
  fbl_composizioni++;
  if (layers) {
    for (unsigned i = 0; i < FB_NUM_LAYERS; i++) {
      if (ready_mask & FB_LAYER_MASK(i)) {
        layers[i].MarkDirty();
      }
    }
  }
  PresentLayerList(sync, nullptr, 0);
}















static void bmc_copia_righe_allineate(uint8_t *dst, unsigned dst_pitch,
                                      const uint8_t *src, unsigned src_pitch,
                                      int left, int right, int top,
                                      int bottom, unsigned bpp,
                                      int larghezza) {
  if (dst == nullptr || src == nullptr || left >= right || top >= bottom ||
      larghezza <= 0) {
    return;
  }
  size_t x0 = ((size_t) left * bpp) & ~(size_t) 31U;
  size_t x1 = ((size_t) right * bpp + 31U) & ~(size_t) 31U;
  const size_t fine = (size_t) larghezza * bpp;
  if (x1 > fine) {
    x1 = fine;
  }
  if (x0 >= x1) {
    return;
  }
  for (int y = top; y < bottom; ++y) {
    memcpy(dst + (size_t) y * dst_pitch + x0,
           src + (size_t) y * src_pitch + x0, x1 - x0);
  }
}

void FrameBufferLayer::PresentLayerList(bool sync, FrameBufferLayer **layers,
                                        unsigned layer_list_count) {
  (void) layers;
  (void) layer_list_count;

  if (!initialized_) {
    return;
  }



  const u64 bmc_inizio_us = CTimer::GetClockTicks64();
  bmc_v3d_aspetta();
  {
    static u64 bmc_prima_us = 0;
    s_bmc_fw_attesa1_us =
        static_cast<u32>(CTimer::GetClockTicks64() - bmc_inizio_us);
    s_bmc_fw_passo_us = bmc_prima_us != 0
        ? static_cast<u32>(bmc_inizio_us - bmc_prima_us) : 0U;
    bmc_prima_us = bmc_inizio_us;
  }

#if RASPPI == 4




  if (g_kms_active && screen_ != nullptr && pi5kms::CompositeArreso()) {
    pi5kms::CompositeLascia();
    g_kms_active = false;
    screen_pixels_ = (uint8_t *) (uintptr) screen_->GetBuffer();
    screen_pitch_bytes_ = screen_->GetPitch();
    raspi_fb_size = screen_->GetSize();
    raspi_fb_pages = 1;
    g_bmc_fw_pixels = screen_pixels_;
    g_bmc_fw_pitch = screen_pitch_bytes_;
    g_bmc_fw_bpp = screen_bytes_per_pixel_;
    g_bmc_fw_w = (unsigned) g_effective_width;
    g_bmc_fw_h = (unsigned) g_effective_height;
    memset(g_software_kms_pending, 0, sizeof g_software_kms_pending);
    InvalidateSoftwareComposition();
    printf("boot: composito: il firmware si riprende lo schermo, lo "
           "scalatore si lascia e il core 3 compone come prima\r\n");
  }
#endif

  bmc_scatola_via_se_chiesta();


  bool bmc_fw3 = bmc_fw_col_core3();
  if (!bmc_fw3) {
    bmc_secondo_disegna();
  }

  int screen_w = g_effective_width;
  int screen_h = g_effective_height;

  if (screen_w <= 0 || screen_h <= 0 || g_compose_pixels == nullptr) {
    return;
  }

#if RASPPI == 4
  // A layer lifecycle operation such as hiding the UI can request an
  // asynchronous present immediately after the previous list was submitted.
  // Fence that submission before selecting and writing the nominal back
  // buffer (or its alternate HVS display-list slot).  Waiting in
  // PresentScanout would be too late because rendering happens first.
  if (g_kms_active && !pi5kms::SynchronizePreviousPresent()) {
    printf("boot: pi4kms present status=fail phase=buffer-reuse-fence\r\n");
    return;
  }
#endif

  FrameBufferLayer *active[FB_NUM_LAYERS];
  unsigned active_count = 0;
  bool has_overlay_layer = false;
  bool has_ui_layer = false;
  bool has_vic_layer = false;
  for (unsigned i = 0; i < FB_NUM_LAYERS; i++) {
    if (g_layers[i] && g_layers[i]->Showing() &&
        !bmc_sul_secondo(g_layers[i])) {
      active[active_count++] = g_layers[i];
      has_overlay_layer = has_overlay_layer || g_layers[i]->transparency_;
      has_ui_layer = has_ui_layer || i == FB_LAYER_UI;
      has_vic_layer = has_vic_layer || i == FB_LAYER_VIC;
    }
  }

  sort_layers(active, active_count);
  const bool wait_for_vblank = sync || has_ui_layer;
  if (TryV3dPostprocess(active, active_count, screen_w, screen_h,
                        wait_for_vblank)) {
    bmc_v3d_conta_core1(
        static_cast<u32>(CTimer::GetClockTicks64() - bmc_inizio_us));
    bmc_strada_conta(0, has_vic_layer);
    InvalidateSoftwareComposition();
    return;
  }
  if (TryKmsDirectScanout(active, active_count, screen_w, screen_h,
                          wait_for_vblank)) {
    bmc_strada_conta(1, has_vic_layer);
    InvalidateSoftwareComposition();






    if (!g_kms_active && screen_ != nullptr &&
        (sync || has_overlay_layer)) {
      bmc_attendi_quadro_firmware(screen_);
    }
    return;
  }
  bmc_strada_conta(2, has_vic_layer);

  DirtyRect dirty = {};
  if (!g_software_composition_valid ||
      g_software_composition_interpolation != g_interpolation_enabled) {
    AddDirtyRect(&dirty, 0, 0, screen_w, screen_h);
  } else {
    for (unsigned i = 0; i < FB_NUM_LAYERS; ++i) {
      FrameBufferLayer *layer = g_layers[i];
      const bool showing = layer != nullptr && layer->showing_ &&
                           !bmc_sul_secondo(layer);
      const SoftwareLayerSnapshot &previous = g_software_layers[i];
      const bool changed = previous.layer != layer ||
          previous.showing != showing ||
          (layer != nullptr &&
           (previous.generation != layer->content_generation_ ||
            previous.z_order != layer->layer_ ||
            previous.dst_x != layer->dst_x_ ||
            previous.dst_y != layer->dst_y_ ||
            previous.dst_w != layer->dst_w_ ||
            previous.dst_h != layer->dst_h_));
      if (!changed) {
        continue;
      }
      if (previous.showing) {
        AddDirtyRect(&dirty, previous.dst_x, previous.dst_y,
                     previous.dst_w, previous.dst_h);
      }
      if (showing) {
        AddDirtyRect(&dirty, layer->dst_x_, layer->dst_y_,
                     layer->dst_w_, layer->dst_h_);
      }
    }
  }



  if (bmc_fw3 && !BmcFwPrepara(active, active_count)) {
    bmc_fw3 = false;
    bmc_secondo_disegna();
  }
  if (dirty.valid) {
    const size_t dirty_bytes =
        (size_t)(dirty.right - dirty.left) * screen_bytes_per_pixel_;

    for (int y = dirty.top; !bmc_fw3 && y < dirty.bottom; ++y) {
      memset(g_compose_pixels + y * g_compose_pitch_bytes +
                 dirty.left * screen_bytes_per_pixel_,
             0, dirty_bytes);
    }
    for (unsigned i = 0; i < kKmsBufferCount; ++i) {
      AddDirtyRect(&g_software_kms_pending[i], dirty.left, dirty.top,
                   dirty.right - dirty.left, dirty.bottom - dirty.top);
    }
  }
  g_capture_v3d_layer = nullptr;

  for (unsigned i = 0; dirty.valid && i < active_count; i++) {
    FrameBufferLayer *layer = active[i];

    int start_x = max_int(dirty.left, layer->dst_x_);
    int end_x = min_int(dirty.right, layer->dst_x_ + layer->dst_w_);
    int start_y = max_int(dirty.top, layer->dst_y_);
    int end_y = min_int(dirty.bottom, layer->dst_y_ + layer->dst_h_);

    if (start_x >= end_x || start_y >= end_y ||
        layer->src_w_ <= 0 || layer->src_h_ <= 0 ||
        layer->fb_width_ <= 0 || layer->fb_height_ <= 0) {
      continue;
    }

    int64_t x_step = ((int64_t) layer->src_w_ << 16) / layer->dst_w_;
    int64_t y_step = ((int64_t) layer->src_h_ << 16) / layer->dst_h_;

    bool scaled = layer->src_w_ != layer->dst_w_ ||
                  layer->src_h_ != layer->dst_h_;
    bool can_interpolate = !layer->transparency_ || layer->pixelmode_ == 0;
    if (bmc_fw3) {
      BmcFwMetti(layer, start_x, end_x, start_y, end_y, x_step, y_step,
                 g_interpolation_enabled && scaled && can_interpolate);
      continue;
    }
    if (g_interpolation_enabled && scaled && can_interpolate) {
      DrawLayerBilinear(layer, start_x, end_x, start_y, end_y,
                        x_step, y_step);
    } else {
      DrawLayerNearest(layer, start_x, end_x, start_y, end_y,
                       x_step, y_step);
    }
  }

  for (unsigned i = 0; i < FB_NUM_LAYERS; ++i) {
    FrameBufferLayer *layer = g_layers[i];
    g_software_layers[i] = {
      layer,
      layer != nullptr ? layer->content_generation_ : 0U,
      layer != nullptr ? layer->layer_ : 0,
      layer != nullptr ? layer->dst_x_ : 0,
      layer != nullptr ? layer->dst_y_ : 0,
      layer != nullptr ? layer->dst_w_ : 0,
      layer != nullptr ? layer->dst_h_ : 0,
      layer != nullptr && layer->showing_ &&
          !bmc_sul_secondo(layer),
    };
  }
  g_software_composition_valid = true;
  g_software_composition_interpolation = g_interpolation_enabled;



  if (bmc_fw3) {
    BmcFwManda(dirty.left, dirty.top, dirty.right, dirty.bottom, dirty.valid,
               sync && screen_ != nullptr, bmc_inizio_us);
    return;
  }

  if (!g_kms_active && sync && screen_ != nullptr) {
    bmc_attendi_quadro_firmware(screen_);
  }

  if (g_kms_active) {
    const unsigned buffer_index = kms_back_buffer_index();
    pi5kms::Framebuffer &fb = g_kms_framebuffers[buffer_index];
    const DirtyRect pending = g_software_kms_pending[buffer_index];
    if (pending.valid) {
      const size_t pending_bytes =
          (size_t)(pending.right - pending.left) * screen_bytes_per_pixel_;
      for (int y = pending.top; y < pending.bottom; ++y) {
        memcpy(fb.pixels + y * fb.pitch +
                   pending.left * screen_bytes_per_pixel_,
               g_compose_pixels + y * g_compose_pitch_bytes +
                   pending.left * screen_bytes_per_pixel_,
               pending_bytes);
      }
      pi5kms::FlushFramebufferRows(
          fb, (u32)pending.top, (u32)(pending.bottom - pending.top));
      g_software_kms_pending[buffer_index] = {};
    }
    if (!present_kms_framebuffer(buffer_index, wait_for_vblank)) {
      printf("boot: " BMX_NATIVE_KMS_LOG " software present failed\r\n");
    }
  } else if (dirty.valid) {

    bmc_copia_righe_allineate(screen_pixels_, screen_pitch_bytes_,
                              g_compose_pixels, g_compose_pitch_bytes,
                              dirty.left, dirty.right, dirty.top,
                              dirty.bottom, screen_bytes_per_pixel_,
                              g_effective_width);
  }
  if (!g_kms_active) {
    bmc_fw_conta_core1(
        static_cast<u32>(CTimer::GetClockTicks64() - bmc_inizio_us), false);
  }
}







bool FrameBufferLayer::BmcFwPrepara(FrameBufferLayer **layers,
                                    unsigned count) {
  s_bmc_fw_lavoro.n = 0;
  s_bmc_fw_foto_byte = 0;
  FrameBufferLayer *strati[FB_NUM_LAYERS + 1];
  unsigned posti[FB_NUM_LAYERS + 1];
  unsigned quanti = 0;
  for (unsigned i = 0; i < count && quanti < FB_NUM_LAYERS; ++i) {
    for (unsigned k = 0; k < FB_NUM_LAYERS; ++k) {
      if (layers[i] != nullptr && g_layers[k] == layers[i]) {
        strati[quanti] = layers[i];
        posti[quanti] = k;
        quanti++;
        break;
      }
    }
  }
  FrameBufferLayer *secondo = secondo_strato();
  if (secondo != nullptr) {
    strati[quanti] = secondo;
    posti[quanti] = FB_NUM_LAYERS;
    quanti++;
  }
  for (unsigned i = 0; i < quanti; ++i) {
    const FrameBufferLayer *l = strati[i];
    if (l->pixels_ == nullptr || l->fb_pitch_ <= 0 || l->fb_height_ <= 0) {
      continue;
    }
    const size_t serve = static_cast<size_t>(l->fb_pitch_) *
                         static_cast<size_t>(l->fb_height_);
    const unsigned p = posti[i];
    if (serve <= s_bmc_fw_byte[p]) {
      continue;
    }
    free(s_bmc_fw_pixel[p]);
    s_bmc_fw_pixel[p] = static_cast<uint8_t *>(malloc(serve));
    s_bmc_fw_byte[p] = s_bmc_fw_pixel[p] != nullptr ? serve : 0U;
    if (s_bmc_fw_pixel[p] == nullptr) {
      printf("boot: bmc64 v3d firmware: niente memoria per la foto dello "
             "strato %u (%u byte), resta il core 1\r\n", p,
             static_cast<unsigned>(serve));
      s_bmc_fallito = 1;
      return false;
    }
  }
  return true;
}



FrameBufferLayer *FrameBufferLayer::BmcFwFoto(unsigned posto,
                                              FrameBufferLayer *layer) {
  if (layer == nullptr || posto > FB_NUM_LAYERS || layer->pixels_ == nullptr ||
      layer->fb_pitch_ <= 0 || layer->fb_height_ <= 0) {
    return nullptr;
  }
  const size_t serve = static_cast<size_t>(layer->fb_pitch_) *
                       static_cast<size_t>(layer->fb_height_);
  if (s_bmc_fw_pixel[posto] == nullptr || serve > s_bmc_fw_byte[posto]) {
    return nullptr;
  }
  memcpy(s_bmc_fw_pixel[posto], layer->pixels_, serve);
  s_bmc_fw_foto_byte += serve;
  memcpy(static_cast<void *>(s_bmc_fw_ombre[posto]),
         static_cast<const void *>(layer), sizeof(FrameBufferLayer));
  FrameBufferLayer *ombra =
      reinterpret_cast<FrameBufferLayer *>(s_bmc_fw_ombre[posto]);
  ombra->pixels_ = s_bmc_fw_pixel[posto];
  return ombra;
}



void FrameBufferLayer::BmcFwMetti(FrameBufferLayer *layer, int start_x,
                                  int end_x, int start_y, int end_y,
                                  int64_t x_step, int64_t y_step,
                                  bool bilineare) {
  BmcFwLavoro &j = s_bmc_fw_lavoro;
  unsigned posto = FB_NUM_LAYERS;
  for (unsigned k = 0; k < FB_NUM_LAYERS; ++k) {
    if (g_layers[k] == layer) {
      posto = k;
      break;
    }
  }
  if (posto >= FB_NUM_LAYERS || j.n >= FB_NUM_LAYERS) {
    return;
  }
  FrameBufferLayer *ombra = BmcFwFoto(posto, layer);
  if (ombra == nullptr) {
    return;
  }
  BmcFwStrato &s = j.strati[j.n++];
  s.ombra = ombra;
  s.start_x = start_x;
  s.end_x = end_x;
  s.start_y = start_y;
  s.end_y = end_y;
  s.x_step = x_step;
  s.y_step = y_step;
  s.bilineare = bilineare;
}


void FrameBufferLayer::BmcFwManda(int left, int top, int right, int bottom,
                                  bool valido, bool attendi,
                                  uint64_t inizio_us) {
  BmcFwLavoro &j = s_bmc_fw_lavoro;
  j.left = left;
  j.top = top;
  j.right = right;
  j.bottom = bottom;
  j.valido = valido;
  j.attendi = attendi;
  j.ombra2 = BmcFwFoto(FB_NUM_LAYERS, secondo_strato());
  s_bmc_tipo = 1;
  s_bmc_core_mandante = CMultiCoreSupport::ThisCore();
  s_bmc_in_volo = 1;
  sem_inc(&fbl_job);
  bmc_fw_conta_core1(
      static_cast<u32>(CTimer::GetClockTicks64() - inizio_us), true);
}



void FrameBufferLayer::BmcLavoroFirmware(void) {
  BmcFwLavoro &j = s_bmc_fw_lavoro;
  const u64 inizio = CTimer::GetClockTicks64();
  if (j.valido && g_compose_pixels != nullptr) {
    const size_t dirty_bytes =
        static_cast<size_t>(j.right - j.left) * screen_bytes_per_pixel_;
    for (int y = j.top; y < j.bottom; ++y) {
      memset(g_compose_pixels + y * g_compose_pitch_bytes +
                 j.left * screen_bytes_per_pixel_,
             0, dirty_bytes);
    }
    for (unsigned k = 0; k < j.n; ++k) {
      const BmcFwStrato &s = j.strati[k];
      if (s.bilineare) {
        DrawLayerBilinear(s.ombra, s.start_x, s.end_x, s.start_y, s.end_y,
                          s.x_step, s.y_step);
      } else {
        DrawLayerNearest(s.ombra, s.start_x, s.end_x, s.start_y, s.end_y,
                         s.x_step, s.y_step);
      }
    }
  }
  u64 attesa = 0;
  if (j.attendi && screen_ != nullptr) {
    const u64 a0 = CTimer::GetClockTicks64();
    bmc_attendi_quadro_firmware(screen_);
    attesa = CTimer::GetClockTicks64() - a0;
  }
  if (j.valido && screen_pixels_ != nullptr && g_compose_pixels != nullptr) {

    bmc_copia_righe_allineate(screen_pixels_, screen_pitch_bytes_,
                              g_compose_pixels, g_compose_pitch_bytes,
                              j.left, j.right, j.top, j.bottom,
                              screen_bytes_per_pixel_, g_effective_width);
  }
  if (j.ombra2 != nullptr) {
    j.ombra2->BlitIntoSecondScreen();
  }
  const u64 fine = CTimer::GetClockTicks64();
  s_bmc_fw_durata_us = static_cast<u32>(fine - inizio);
  s_bmc_fw_disegno_us = static_cast<u32>(fine - inizio - attesa);
}

bool FrameBufferLayer::CaptureDimensions(int *width, int *height) {
  if (width == nullptr || height == nullptr || !initialized_) return false;
  *width = g_effective_width;
  *height = g_effective_height;
  return *width > 0 && *height > 0;
}

bool FrameBufferLayer::CaptureRgb888(uint8_t *output, int width, int height,
                                     unsigned pitch) {
  bmc_v3d_aspetta();
  if (output == nullptr || width <= 0 || height <= 0 ||
      pitch < (unsigned)width * 3U || !initialized_ ||
      g_effective_width <= 0 || g_effective_height <= 0 ||
      g_compose_pixels == nullptr) return false;

  FrameBufferLayer *active[FB_NUM_LAYERS];
  unsigned active_count = 0U;
  for (unsigned i = 0U; i < FB_NUM_LAYERS; ++i) {
    if (g_layers[i] != nullptr && g_layers[i]->Showing()) {
      active[active_count++] = g_layers[i];
    }
  }
  sort_layers(active, active_count);
  memset(g_compose_pixels, 0,
         static_cast<size_t>(g_compose_pitch_bytes) * g_effective_height);

  v3dcrt::OutputReadback readback = {};
  const bool readback_valid =
      g_capture_v3d_layer != nullptr &&
      v3dcrt::ReadCompletedFrame(&readback) &&
      readback.pixels != nullptr && readback.depth == 16U &&
      readback.width != 0U && readback.height != 0U &&
      readback.pitch >= readback.width * sizeof(uint16_t);

  for (unsigned i = 0U; i < active_count; ++i) {
    FrameBufferLayer *layer = active[i];
    const int start_x = max_int(0, layer->dst_x_);
    const int end_x = min_int(g_effective_width,
                              layer->dst_x_ + layer->dst_w_);
    const int start_y = max_int(0, layer->dst_y_);
    const int end_y = min_int(g_effective_height,
                              layer->dst_y_ + layer->dst_h_);
    if (start_x >= end_x || start_y >= end_y ||
        layer->dst_w_ <= 0 || layer->dst_h_ <= 0) {
      continue;
    }

    if (readback_valid && layer == g_capture_v3d_layer) {
      for (int y = start_y; y < end_y; ++y) {
        const uint32_t source_y = static_cast<uint32_t>(
            (static_cast<uint64_t>(y - layer->dst_y_) * readback.height) /
            static_cast<uint32_t>(layer->dst_h_));
        const uint16_t *source = reinterpret_cast<const uint16_t *>(
            readback.pixels + source_y * readback.pitch);
        uint8_t *destination =
            g_compose_pixels + y * g_compose_pitch_bytes;
        for (int x = start_x; x < end_x; ++x) {
          const uint32_t source_x = static_cast<uint32_t>(
              (static_cast<uint64_t>(x - layer->dst_x_) * readback.width) /
              static_cast<uint32_t>(layer->dst_w_));
          write_rgb565_pixel(destination, x, source[source_x]);
        }
      }
      continue;
    }

    if (layer->src_w_ <= 0 || layer->src_h_ <= 0 ||
        layer->fb_width_ <= 0 || layer->fb_height_ <= 0) {
      continue;
    }
    const int64_t x_step =
        (static_cast<int64_t>(layer->src_w_) << 16) / layer->dst_w_;
    const int64_t y_step =
        (static_cast<int64_t>(layer->src_h_) << 16) / layer->dst_h_;
    const bool scaled = layer->src_w_ != layer->dst_w_ ||
                        layer->src_h_ != layer->dst_h_;
    const bool can_interpolate =
        !layer->transparency_ || layer->pixelmode_ == 0;
    if (g_interpolation_enabled && scaled && can_interpolate) {
      DrawLayerBilinear(layer, start_x, end_x, start_y, end_y,
                        x_step, y_step);
    } else {
      DrawLayerNearest(layer, start_x, end_x, start_y, end_y,
                       x_step, y_step);
    }
  }

  for (int y = 0; y < height; ++y) {
    const int source_y = (int)(((int64_t)y * g_effective_height) / height);
    const uint8_t *source =
        g_compose_pixels + source_y * g_compose_pitch_bytes;
    uint8_t *destination = output + (size_t)y * pitch;
    for (int x = 0; x < width; ++x) {
      const int source_x = (int)(((int64_t)x * g_effective_width) / width);
      const uint32_t argb = read_argb_pixel(source, source_x);
      destination[x * 3] = (uint8_t)(argb >> 16);
      destination[x * 3 + 1] = (uint8_t)(argb >> 8);
      destination[x * 3 + 2] = (uint8_t)argb;
    }
  }
  InvalidateSoftwareComposition();
  return true;
}

void FrameBufferLayer::SetPalette(uint8_t index, uint16_t rgb565) {
  pal_565_[index] = rgb565;
  MarkDirty();
}

void FrameBufferLayer::SetPalette(uint8_t index, uint32_t argb) {
  pal_argb_[index] = argb;
  MarkDirty();
}

void FrameBufferLayer::UpdatePalette() {
  ++palette_generation_;
  MarkDirty();
  bmc_secondo_tavolozza_se(this);
  if (v3dcrt::Requested()) {
    printf("boot: v3dcrt palette update layer=%d generation=%u rgb565_sig=0x%08x showing=%u\r\n",
           layer_,
           palette_generation_,
           palette_signature_rgb565(pal_565_),
           showing_ ? 1U : 0U);
  }
  if (showing_) {
    PresentLayer(false, this);
  }
}

void FrameBufferLayer::SetLayer(int layer) {
  if (logical_layer_ < 0) {
    logical_layer_ = layer;
  }
  layer_ = layer;
}

int FrameBufferLayer::GetLayer() {
  return layer_;
}

bool FrameBufferLayer::UsesShader() {
  return false;
}

bool FrameBufferLayer::Showing() {
  return showing_;
}

void FrameBufferLayer::SetTransparency(bool transparency) {
  transparency_ = transparency;
}

void FrameBufferLayer::SetSrcRect(int x, int y, int w, int h) {
  src_x_ = x;
  src_y_ = y;
  src_w_ = w;
  src_h_ = h;
  MarkDirty();
}

void FrameBufferLayer::SetStretch(double hstretch, double vstretch,
                                  int hintstr, int vintstr,
                                  int use_hintstr, int use_vintstr) {
  hstretch_ = hstretch;
  vstretch_ = vstretch;
  hintstr_ = hintstr;
  vintstr_ = vintstr;
  use_hintstr_ = use_hintstr;
  use_vintstr_ = use_vintstr;
}

void FrameBufferLayer::SetVerticalAlignment(int alignment, int padding) {
  valign_ = alignment;
  vpadding_ = padding;
}

void FrameBufferLayer::SetHorizontalAlignment(int alignment, int padding) {
  halign_ = alignment;
  hpadding_ = padding;
}

void FrameBufferLayer::SetPadding(double leftPadding,
                                  double rightPadding,
                                  double topPadding,
                                  double bottomPadding) {
  leftPadding_ = leftPadding;
  rightPadding_ = rightPadding;
  topPadding_ = topPadding;
  bottomPadding_ = bottomPadding;
}

void FrameBufferLayer::SetCenterOffset(int cx, int cy) {
  h_center_offset_ = cx;
  v_center_offset_ = cy;
}

void FrameBufferLayer::GetDimensions(int *display_w, int *display_h,
                                     int *fb_w, int *fb_h,
                                     int *src_w, int *src_h,
                                     int *dst_w, int *dst_h) {
  *display_w = display_width_;
  *display_h = display_height_;
  *fb_w = fb_width_;
  *fb_h = fb_height_;
  *src_w = src_w_;
  *src_h = src_h_;
  *dst_w = dst_w_;
  *dst_h = dst_h_;
}

void FrameBufferLayer::SetInterpolation(int enable) {
  bool new_value = enable != 0;
  if (g_interpolation_enabled == new_value) {
    return;
  }

  g_interpolation_enabled = new_value;
  if (initialized_) {
    PresentLayerList(false, nullptr, 0);
  }
}

void FrameBufferLayer::SetUsesShader(bool enable) {
  (void) enable;
  uses_shader_ = false;
}

void FrameBufferLayer::SetShaderParams(
    const struct bmx_crt_effect_params &params) {
  g_v3d_effect_params = v3dcrt::EffectParamsFromBmx(params);

  if (should_log_v3d_menu_params(params)) {
    printf("boot: v3dcrt menu effects geometry=%u convergence=%u "
           "horizontal_filter=%u edge_blur=%u scanlines=%u mask=%u "
           "bloom=%u vignette=%u uneven=%u jitter=%u composite=%u "
           "glass=%u rounded_mask=%u edge_glow=%u noise=%u response=%u "
           "multisample=%u sigma_x100=%u weight_x100=%u gap_x100=%u "
           "mask_pattern=%u mask_x100=%u jitter_speed_x100=%u "
           "noise_speed_x100=%u response_fast=%u level_mapping=%u\r\n",
           params.geometry_enabled ? 1U : 0U,
           params.convergence_enabled ? 1U : 0U,
           params.horizontal_filtering_enabled ? 1U : 0U,
           params.edge_blur_enabled ? 1U : 0U,
           params.scanlines_enabled ? 1U : 0U,
           params.phosphor_mask_enabled ? 1U : 0U,
           params.bloom_enabled ? 1U : 0U,
           params.vignette_enabled ? 1U : 0U,
           params.uneven_illumination_enabled ? 1U : 0U,
           params.horizontal_jitter_enabled ? 1U : 0U,
           params.composite_artifacts_enabled ? 1U : 0U,
           params.glass_reflection_enabled ? 1U : 0U,
           params.rounded_screen_mask_enabled ? 1U : 0U,
           params.edge_glow_enabled ? 1U : 0U,
           params.noise_enabled ? 1U : 0U,
           params.output_response_enabled ? 1U : 0U,
           params.scanline_multisample ? 1U : 0U,
           param_x100(params.horizontal_sigma_x),
           param_x100(params.scanline_weight),
           param_x100(params.scanline_gap_brightness),
           (unsigned)g_v3d_effect_params.phosphor_mask_pattern,
           param_x100(params.phosphor_mask_brightness),
           param_x100(params.horizontal_jitter_speed),
           param_x100(params.noise_speed),
           params.output_response_fast ? 1U : 0U,
           (unsigned)g_v3d_effect_params.output_level_mapping);
  }

  uses_shader_ = false;
  if (initialized_) {
    PresentLayerList(false, nullptr, 0);
  }
}




unsigned long bmc_fw_quadro_misurato_us(void) { return g_bmc_fw_quadro_us; }
