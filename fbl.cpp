//
// fbl.cpp
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

// Notes: This class started out being just a wrapper around the
// dispmanx API to get frame buffer pixels to the screen using just
// an element, src rect and dest rect.  It now includes code that
// essentially tries to get the same pixels to the screen but
// through an gles shader instead.  So when uses_shader_ flag is true,
// (for the emulated machine's display, for example) gles is used.
// Otherwise, it's not (for the UI or overlays, for example).

#include "fbl.h"

#include <stdio.h>
#include <circle/bcmframebuffer.h>
#include <circle/timer.h>

#include "crt_pi_idx.h"
#include "crt_pi_rgb.h"
#include "third_party/common/circle.h"

#if RASPPI == 4
#include <circle/new.h>
#include <circle/synchronize.h>
#include "machines/machine_descriptor.h"
#include "pi4kms/pi4_kms.h"
#include "v3dcrt/v3d_crt.h"
#include "viceoptions.h"
#endif

#ifndef ALIGN_UP
#define ALIGN_UP(x,y)  ((x + (y)-1) & ~((y)-1))
#endif

#define RGB565(r,g,b) (((r)>>3)<<11 | ((g)>>2)<<5 | (b)>>3)
#define ARGB(a,r,g,b) ((uint32_t)((uint8_t)(a)<<24 | (uint8_t)(r)<<16 | (uint8_t)(g)<<8 | (uint8_t)(b)))

// Default palette used for canvases with no transparency
static uint16_t pal_565[256] = {
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

// Default palette used for canvases with transparency. This palette
// is identical to pal_565 except we include an additional
// entry for a fully transparent pixel (index 16).
static uint32_t pal_argb[256] = {
  // First 16 colors are opaque
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

  // Use index 16 for fully transparent pixels
  ARGB(0x00, 0x00, 0x00, 0x00),
};

static const char sNoInt[] = "scaling_kernel 0 0 0 0 0 0 0 0 1 1 1 1 255 255 255 255 255 255 255 255 1 1 1 1 0 0 0 0 0 0 0 0   1";

static char config_scaling_kernel[1024];

static void bcm_get_sclker(char *buffer, size_t buffer_size) {
  if (buffer_size == 0) {
    return;
  }

  buffer[0] = '\0';
  if (vc_gencmd(buffer, (int)buffer_size, "scaling_kernel") != 0) {
    buffer[0] = '\0';
  }
}

static void bcm_set_sclker(const char *command) {
  char response[1024];

  if (command == nullptr || command[0] == '\0') {
    return;
  }

  vc_gencmd(response, (int)sizeof(response), "%s", command);
}

bool FrameBufferLayer::initialized_ = false;
DISPMANX_DISPLAY_HANDLE_T FrameBufferLayer::dispman_display_;
bool FrameBufferLayer::egl_initialized_ = false;
EGLDisplay FrameBufferLayer::egl_display_;
EGLContext FrameBufferLayer::egl_context_;
static char* fshader_txt_;
static char* vshader_txt_;

#if RASPPI == 4
#ifndef DISPMANX_ID_HDMI
#define DISPMANX_ID_HDMI 2
#endif
#define BMC64_DISPMANX_DISPLAY_ID DISPMANX_ID_HDMI

namespace {

const uint32_t kPi4V3dBootScanoutMaxBytes = 4096U;
const unsigned kPi4V3dBootScanoutHoldMs = 250U;

uint8_t g_pi4_v3d_boot_scanout[kPi4V3dBootScanoutMaxBytes]
    __attribute__((aligned(32)));
uint8_t g_pi4_v3d_boot_scanout_readback[kPi4V3dBootScanoutMaxBytes]
    __attribute__((aligned(32)));
bool g_pi4_v3d_crt_enabled = true;
bool g_pi4_v3d_crt_enabled_initialized = false;
bool g_pi4_v3d_scanline_weight_override_enabled = false;
bool g_pi4_v3d_scanline_gap_override_enabled = false;
bool g_pi4_v3d_output_resolution = false;
bool g_pi4_kms_interpolation_enabled = false;
float g_pi4_v3d_scanline_weight_override = 0.0f;
float g_pi4_v3d_scanline_gap_override = 1.0f;
FrameBufferLayer *g_pi4_layers[FB_NUM_LAYERS] = {};

struct Pi4V3dPipelineTiming {
  bool valid;
  bool effect_change;
  bool sample_after_effect_change;
  FrameBufferLayer *layer;
  u32 sequence;
  u64 frame_start_us;
  u64 render_done_us;
  u64 write_done_us;
};

Pi4V3dPipelineTiming g_pi4_v3d_pipeline_timing = {};
u32 g_pi4_v3d_pipeline_samples_remaining = 0U;
const u32 kPi4V3dPipelineEffectSampleCount = 32U;

struct Pi4V3dPipelineAggregate {
  u32 samples;
  u64 render_call_us;
  u64 dispmanx_write_us;
  u64 handoff_us;
  u64 present_sync_us;
  u64 total_us;
  u32 total_min_us;
  u32 total_max_us;
};

Pi4V3dPipelineAggregate g_pi4_v3d_pipeline_aggregate = {};

uint32_t Pi4CaptureRgb565ToArgb(uint16_t rgb) {
  uint32_t red = (rgb >> 11U) & 0x1fU;
  uint32_t green = (rgb >> 5U) & 0x3fU;
  uint32_t blue = rgb & 0x1fU;
  red = (red << 3U) | (red >> 2U);
  green = (green << 2U) | (green >> 4U);
  blue = (blue << 3U) | (blue >> 2U);
  return 0xff000000U | (red << 16U) | (green << 8U) | blue;
}

uint32_t Pi4CaptureBlendArgb(uint32_t destination, uint32_t source) {
  const uint32_t alpha = source >> 24U;
  if (alpha == 0U) {
    return destination;
  }
  if (alpha >= 255U) {
    return source;
  }
  const uint32_t inverse = 255U - alpha;
  const uint32_t red =
      ((((source >> 16U) & 0xffU) * alpha) +
       (((destination >> 16U) & 0xffU) * inverse) + 127U) / 255U;
  const uint32_t green =
      ((((source >> 8U) & 0xffU) * alpha) +
       (((destination >> 8U) & 0xffU) * inverse) + 127U) / 255U;
  const uint32_t blue =
      (((source & 0xffU) * alpha) +
       ((destination & 0xffU) * inverse) + 127U) / 255U;
  return 0xff000000U | (red << 16U) | (green << 8U) | blue;
}

void RecordPi4V3dPipelinePresent(u64 present_start_us,
                                 u64 present_done_us,
                                 unsigned layer_count,
                                 const char *backend) {
  if (!g_pi4_v3d_pipeline_timing.valid) {
    return;
  }
  const u32 sequence = g_pi4_v3d_pipeline_timing.sequence;
  const bool effect_change = g_pi4_v3d_pipeline_timing.effect_change;
  const bool sample_after_effect_change =
      g_pi4_v3d_pipeline_timing.sample_after_effect_change;
  const bool stable_after_effect_change =
      sample_after_effect_change && sequence >= 3U;
  const bool startup_sample = sequence == 3U;
  const u32 render_call_us = static_cast<u32>(
      g_pi4_v3d_pipeline_timing.render_done_us -
      g_pi4_v3d_pipeline_timing.frame_start_us);
  const u32 dispmanx_write_us = static_cast<u32>(
      g_pi4_v3d_pipeline_timing.write_done_us -
      g_pi4_v3d_pipeline_timing.render_done_us);
  const u32 handoff_us = static_cast<u32>(
      present_start_us - g_pi4_v3d_pipeline_timing.write_done_us);
  const u32 present_sync_us =
      static_cast<u32>(present_done_us - present_start_us);
  const u32 total_us = static_cast<u32>(
      present_done_us - g_pi4_v3d_pipeline_timing.frame_start_us);
  if (startup_sample) {
    printf("boot: pi4v3d frame pipeline sequence=%u "
           "render_call_us=%u dispmanx_write_us=%u handoff_us=%u "
           "present_sync_us=%u total_us=%u effect_change=%u "
           "layers=%u backend=%s\r\n",
           static_cast<unsigned>(sequence),
           static_cast<unsigned>(render_call_us),
           static_cast<unsigned>(dispmanx_write_us),
           static_cast<unsigned>(handoff_us),
           static_cast<unsigned>(present_sync_us),
           static_cast<unsigned>(total_us),
           effect_change ? 1U : 0U, layer_count, backend);
  }
  if (stable_after_effect_change) {
    Pi4V3dPipelineAggregate &aggregate = g_pi4_v3d_pipeline_aggregate;
    ++aggregate.samples;
    aggregate.render_call_us += render_call_us;
    aggregate.dispmanx_write_us += dispmanx_write_us;
    aggregate.handoff_us += handoff_us;
    aggregate.present_sync_us += present_sync_us;
    aggregate.total_us += total_us;
    if (aggregate.samples == 1U || total_us < aggregate.total_min_us) {
      aggregate.total_min_us = total_us;
    }
    if (aggregate.samples == 1U || total_us > aggregate.total_max_us) {
      aggregate.total_max_us = total_us;
    }
    --g_pi4_v3d_pipeline_samples_remaining;
    if (g_pi4_v3d_pipeline_samples_remaining == 0U) {
      printf("boot: pi4v3d frame pipeline aggregate "
             "end_sequence=%u samples=%u render_avg_us=%u "
             "dispmanx_write_avg_us=%u handoff_avg_us=%u "
             "present_sync_avg_us=%u total_avg_us=%u "
             "total_min_us=%u total_max_us=%u layers=%u backend=%s\r\n",
             static_cast<unsigned>(sequence),
             static_cast<unsigned>(aggregate.samples),
             static_cast<unsigned>(aggregate.render_call_us /
                                   aggregate.samples),
             static_cast<unsigned>(aggregate.dispmanx_write_us /
                                   aggregate.samples),
             static_cast<unsigned>(aggregate.handoff_us /
                                   aggregate.samples),
             static_cast<unsigned>(aggregate.present_sync_us /
                                   aggregate.samples),
             static_cast<unsigned>(aggregate.total_us /
                                   aggregate.samples),
             static_cast<unsigned>(aggregate.total_min_us),
             static_cast<unsigned>(aggregate.total_max_us),
             layer_count, backend);
    }
  }
  g_pi4_v3d_pipeline_timing = {};
}

struct Pi4V3dDispmanxResult {
  bool upload;
  bool resource_readback;
  bool element_added;
  bool present_sync;
  bool cleanup;
};

bool PresentPi4V3dBootScanout(
    DISPMANX_DISPLAY_HANDLE_T display,
    const v3dcrt::BootTestOutputLayout &layout,
    const uint8_t *pixels,
    Pi4V3dDispmanxResult *result) {
  if (result == nullptr) {
    return false;
  }
  memset(result, 0, sizeof *result);
  if (display == 0 || pixels == nullptr || layout.width == 0U ||
      layout.height == 0U || layout.pitch == 0U ||
      layout.width > kPi4V3dBootScanoutMaxBytes / 2U ||
      layout.pitch < layout.width * 2U ||
      layout.depth != 16U || layout.format != v3dcrt::kPixelFormatRgb565 ||
      layout.height > kPi4V3dBootScanoutMaxBytes / layout.pitch) {
    return false;
  }
  const uint32_t buffer_bytes = layout.pitch * layout.height;

  DISPMANX_MODEINFO_T info = {};
  if (vc_dispmanx_display_get_info(display, &info) != 0 ||
      info.width == 0U || info.height == 0U) {
    return false;
  }

  uint32_t native_image = 0U;
  const DISPMANX_RESOURCE_HANDLE_T resource =
      vc_dispmanx_resource_create(VC_IMAGE_RGB565, layout.width,
                                  layout.height, &native_image);
  if (resource == 0) {
    return false;
  }

  VC_RECT_T resource_rect = {};
  vc_dispmanx_rect_set(&resource_rect, 0U, 0U,
                       layout.width, layout.height);
  result->upload =
      vc_dispmanx_resource_write_data(resource, VC_IMAGE_RGB565,
                                      static_cast<int>(layout.pitch),
                                      const_cast<uint8_t *>(pixels),
                                      &resource_rect) == 0;
  memset(g_pi4_v3d_boot_scanout_readback, 0,
         sizeof g_pi4_v3d_boot_scanout_readback);
  result->resource_readback =
      result->upload &&
      vc_dispmanx_resource_read_data(
          resource, &resource_rect, g_pi4_v3d_boot_scanout_readback,
          layout.pitch) == 0 &&
      memcmp(pixels, g_pi4_v3d_boot_scanout_readback,
             buffer_bytes) == 0;

  DISPMANX_ELEMENT_HANDLE_T element = 0;
  if (result->resource_readback) {
    const uint32_t side = info.width < info.height ? info.width : info.height;
    VC_RECT_T source_rect = {};
    VC_RECT_T destination_rect = {};
    vc_dispmanx_rect_set(&source_rect, 0U, 0U,
                         layout.width << 16U, layout.height << 16U);
    vc_dispmanx_rect_set(&destination_rect,
                         (info.width - side) / 2U,
                         (info.height - side) / 2U,
                         side, side);
    VC_DISPMANX_ALPHA_T alpha = {
      static_cast<DISPMANX_FLAGS_ALPHA_T>(
          DISPMANX_FLAGS_ALPHA_FROM_SOURCE |
          DISPMANX_FLAGS_ALPHA_FIXED_ALL_PIXELS),
      255,
      0
    };
    const DISPMANX_UPDATE_HANDLE_T update = vc_dispmanx_update_start(10);
    if (update != 0) {
      element = vc_dispmanx_element_add(
          update, display, 2000, &destination_rect, resource, &source_rect,
          DISPMANX_PROTECTION_NONE, &alpha, nullptr, DISPMANX_NO_ROTATE);
      result->element_added = element != 0;
      result->present_sync =
          vc_dispmanx_update_submit_sync(update) == 0 &&
          result->element_added;
    }
  }

  bool removed = element == 0;
  if (element != 0) {
    if (result->present_sync) {
      CTimer::SimpleMsDelay(kPi4V3dBootScanoutHoldMs);
    }
    const DISPMANX_UPDATE_HANDLE_T update = vc_dispmanx_update_start(10);
    if (update != 0) {
      const bool remove_queued =
          vc_dispmanx_element_remove(update, element) == 0;
      const bool remove_sync = vc_dispmanx_update_submit_sync(update) == 0;
      removed = remove_queued && remove_sync;
    } else {
      removed = false;
    }
  }
  const bool resource_deleted =
      removed && vc_dispmanx_resource_delete(resource) == 0;
  result->cleanup = removed && resource_deleted;

  printf("boot: pi4v3d dispmanx handoff resource=%u native=%u "
         "upload=%u resource_readback=%u element=%u present_sync=%u "
         "hold_ms=%u cleanup=%u display=%ux%u\r\n",
         static_cast<unsigned>(resource),
         static_cast<unsigned>(native_image),
         result->upload ? 1U : 0U,
         result->resource_readback ? 1U : 0U,
         result->element_added ? 1U : 0U,
         result->present_sync ? 1U : 0U,
         kPi4V3dBootScanoutHoldMs,
         result->cleanup ? 1U : 0U,
         static_cast<unsigned>(info.width),
         static_cast<unsigned>(info.height));
  return result->upload && result->resource_readback &&
         result->element_added && result->present_sync && result->cleanup;
}

}  // namespace
#else
#define BMC64_DISPMANX_DISPLAY_ID 0
#endif

// For testing.
//#define LOAD_SHADER_FROM_FILE

#ifdef LOAD_SHADER_FROM_FILE
static char *file_shader_txt_;
#endif

/*
static void check(const char* msg) {
	int g = glGetError();
	if (g != 0) {
		FILE *fp = fopen("errors.txt","w");
		fprintf (fp,"%s %d\n",msg, g);
		fclose(fp);
		assert(false);
	}
}
*/

FrameBufferLayer::FrameBufferLayer() :
		pixels_(nullptr), dispman_element_(0),egl_config_(nullptr),egl_surface_(nullptr),
        fb_width_(0), fb_height_(0), fb_pitch_(0), logical_layer_(-1),
        layer_(0), transparency_(false),
        hstretch_(1.6), vstretch_(1.0), hintstr_(0), vintstr_(0),
        use_hintstr_(0), use_vintstr_(0),
        valign_(0), vpadding_(0), halign_(0), hpadding_(0),
        h_center_offset_(0), v_center_offset_(0),
        rnum_(0), leftPadding_(0), rightPadding_(0), topPadding_(0),
        bottomPadding_(0),
        display_width_(0), display_height_(0),
        src_x_(0), src_y_(0), src_w_(0), src_h_(0),
        dst_x_(0), dst_y_(0), dst_w_(0), dst_h_(0),
        showing_(false), allocated_(false),
        mode_(VC_IMAGE_8BPP), bytes_per_pixel_(1), uses_shader_(false),
        shader_init_(false),
        vshader_(-1), fshader_(-1), shader_program_(-1),
        vbo_(-1),
        attr_vertex_(-1), attr_texcoord_(-1),
        texture_sampler_(-1), palette_sampler_(-1),
        tex_(-1), pal_(-1), mvp_(0),
        input_size_(0), output_size_(0), texture_size_(0), texel_size_(0),
        need_cpu_crop_(true), cropped_pixels_(0),
        curvature_(false), curvature_x_(0.0f), curvature_y_(0.0f),
        skew_x_(0.0f), skew_y_(0.0f), trapezoid_(0.0f),
        rotation_degrees_(0.0f), overscan_scale_(1.0f),
        convergence_(false), red_offset_x_(0.0f), red_offset_y_(0.0f),
        blue_offset_x_(0.0f), blue_offset_y_(0.0f),
        convergence_radial_strength_(0.0f), horizontal_filtering_(false),
        horizontal_sigma_x_(0.0f), mask_(0),
        mask_brightness_(1.0f), gamma_(false), fake_gamma_(false),
        output_level_mapping_(1U), output_saturation_(1.0f),
        black_level_(0.0f), white_clip_(1.0f),
        scanlines_(false), multisample_(false), scanline_weight_(0.0f),
        scanline_gap_brightness_(1.0f), edge_blur_(false),
        edge_blur_strength_(0.0f), edge_blur_radius_(0.2f),
        vignette_(false), vignette_strength_(0.0f), vignette_scale_(1.0f),
        vignette_softness_(0.02f),
        uneven_illumination_(false), uneven_illumination_strength_(0.0f),
        uneven_illumination_scale_(0.02f),
        glass_reflection_(false), glass_reflection_angle_(0.0f),
        glass_reflection_width_(0.02f), glass_reflection_position_(0.0f),
        rounded_screen_mask_(false), rounded_corner_radius_(0.0f),
        rounded_border_softness_(0.0f),
        edge_glow_(false), edge_glow_strength_(0.0f),
        edge_glow_width_(0.01f),
        bloom_(false), bloom_factor_(1.0f),
        horizontal_jitter_(false), horizontal_jitter_strength_(0.0f),
        horizontal_jitter_frequency_(0.01f),
        horizontal_jitter_speed_(0.0f),
        composite_artifacts_(false), composite_chroma_blur_(0.0f),
        composite_luma_sharpen_(0.0f), composite_color_bleed_(0.0f),
        noise_(false), luminance_noise_(0.0f), chroma_noise_(0.0f),
        noise_speed_(0.0f),
        input_gamma_(1.0f), output_gamma_(1.0f), sharper_(true),
        bilinear_interpolation_(false) {
  alpha_.flags = DISPMANX_FLAGS_ALPHA_FROM_SOURCE;
  alpha_.opacity = 255;
  alpha_.mask = 0;

  memcpy (pal_565_, pal_565, sizeof(pal_565));
  memcpy (pal_argb_, pal_argb, sizeof(pal_argb));
  memset(dispman_resource_, 0, sizeof dispman_resource_);
#if RASPPI == 4
  memset(pi4_v3d_resource_, 0, sizeof pi4_v3d_resource_);
  memset(pi4_v3d_allocation_, 0, sizeof pi4_v3d_allocation_);
  memset(pi4_v3d_pixels_, 0, sizeof pi4_v3d_pixels_);
  memset(pi4_v3d_ready_, 0, sizeof pi4_v3d_ready_);
  memset(pi4_v3d_scanout_, 0, sizeof pi4_v3d_scanout_);
  pi4_v3d_pitch_ = 0U;
  pi4_v3d_width_ = 0U;
  pi4_v3d_height_ = 0U;
  memset(&pi4_v3d_copy_dst_rect_, 0, sizeof pi4_v3d_copy_dst_rect_);
  memset(&pi4_v3d_src_rect_, 0, sizeof pi4_v3d_src_rect_);
  memset(pi4_kms_overlay_allocation_, 0,
         sizeof pi4_kms_overlay_allocation_);
  memset(pi4_kms_overlay_pixels_, 0, sizeof pi4_kms_overlay_pixels_);
  pi4_kms_overlay_pitch_ = 0U;
  pi4_kms_overlay_width_ = 0U;
  pi4_kms_overlay_height_ = 0U;
  pi4_kms_overlay_front_ = 0U;
  pi4_kms_overlay_front_valid_ = false;
#endif
}

FrameBufferLayer::~FrameBufferLayer() {
  if (showing_) {
    Hide();
  }
  if (allocated_) {
    Free();
  }
#if RASPPI == 4
  if (logical_layer_ >= 0 && logical_layer_ < FB_NUM_LAYERS &&
      g_pi4_layers[logical_layer_] == this) {
    g_pi4_layers[logical_layer_] = nullptr;
  }
#endif
}

bool FrameBufferLayer::OGLInit() {
  if (egl_initialized_) {
     return true;
  }

  EGLBoolean result;

  printf("boot: fbl egl init enter\r\n");
  printf("boot: fbl egl get display enter\r\n");
  egl_display_ = eglGetDisplay(EGL_DEFAULT_DISPLAY);
  if (egl_display_ == EGL_NO_DISPLAY) {
    printf("boot: fbl egl get display failed\r\n");
    return false;
  }
  printf("boot: fbl egl get display ready %p\r\n", egl_display_);
  printf("boot: fbl egl initialize enter\r\n");
  result = eglInitialize(egl_display_, NULL, NULL);
  if (result == EGL_FALSE) {
    printf("boot: fbl egl initialize failed 0x%x\r\n", (unsigned)eglGetError());
    return false;
  }
  printf("boot: fbl egl initialize ready\r\n");
  printf("boot: fbl egl bind api enter\r\n");
  result = eglBindAPI(EGL_OPENGL_ES_API);
  if (result == EGL_FALSE) {
    printf("boot: fbl egl bind api failed 0x%x\r\n", (unsigned)eglGetError());
    return false;
  }
  egl_initialized_ = true;
  printf("boot: fbl egl init ready\r\n");
  return true;
}

bool FrameBufferLayer::ShaderBackendAvailable() {
#if RASPPI == 4
  return v3dcrt::IsAvailable();
#else
  return false;
#endif
}

bool FrameBufferLayer::ShaderBackendAvailableForLayer(int logical_layer) {
#if RASPPI == 4
  return v3dcrt::IsAvailable() && logical_layer == FB_LAYER_VIC;
#else
  (void) logical_layer;
  return false;
#endif
}

void FrameBufferLayer::CreateTexture() {
  // These values make sense but I'm not sure what the difference between
  // inputSize and textureSize is.  Perhaps it has something to do with
  // the fact these shaders are meant to be chained together over different
  // passes and the next shader needs to know what the output size of the
  // previous was?
  float inputSize[2] = { (float) src_w_, (float)src_h_ };
  float outputSize[2] = { (float) dst_w_, (float)dst_h_ };

  int tx = need_cpu_crop_ ? src_w_ : fb_pitch_ / bytes_per_pixel_;
  int ty = need_cpu_crop_ ? src_h_ : fb_height_;
  float textureSize[2] = { (float) tx, (float) ty };
  float texelSize[2] = { 1.0f / (float) tx, 1.0f / (float) ty };

  glUniform2fv(input_size_, 1, inputSize);
  glUniform2fv(output_size_, 1, outputSize);
  glUniform2fv(texture_size_, 1, textureSize);
  glUniform2fv(texel_size_, 1, texelSize);

  glBindTexture(GL_TEXTURE_2D,tex_);
  if (mode_ == VC_IMAGE_8BPP) {
     glTexImage2D(GL_TEXTURE_2D,0,GL_LUMINANCE,
        need_cpu_crop_ ? src_w_ : fb_pitch_ / bytes_per_pixel_,
        need_cpu_crop_ ? src_h_ : fb_height_,
        0,GL_LUMINANCE,GL_UNSIGNED_BYTE, 0);
  } else {
     glTexImage2D(GL_TEXTURE_2D,0,GL_RGB,
        need_cpu_crop_ ? src_w_ : fb_pitch_ / bytes_per_pixel_,
        need_cpu_crop_ ? src_h_ : fb_height_,
        0,GL_RGB,GL_UNSIGNED_SHORT_5_6_5, 0);
  }

  glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
  glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
}

void FrameBufferLayer::ReCreateTexture() {
  if (shader_init_) {
	  CreateTexture();
  }
}

void FrameBufferLayer::ConcatShaderDefines(char *dst) {
  char scratch[64];
  if (curvature_) {
	  strcat(dst,"#define CURVATURE\n");
  }

  sprintf (scratch, "#define CURVATURE_X %f\n", curvature_x_);
  strcat(dst, scratch);

  sprintf (scratch, "#define CURVATURE_Y %f\n", curvature_y_);
  strcat(dst, scratch);

  sprintf (scratch, "#define MASK_TYPE %d\n", mask_);
  strcat(dst, scratch);

  sprintf (scratch, "#define MASK_BRIGHTNESS %f\n", mask_brightness_);
  strcat(dst, scratch);

  if (gamma_) {
	  strcat (dst, "#define GAMMA\n");
	  if (fake_gamma_) {
		  strcat (dst, "#define FAKE_GAMMA\n");
	  }
  }

  if (scanlines_) {
	  strcat (dst, "#define SCANLINES\n");
  }

  if (multisample_) {
	  strcat (dst, "#define MULTISAMPLE\n");
  }

  sprintf (scratch, "#define SCANLINE_WEIGHT %f\n", scanline_weight_);
  strcat(dst, scratch);

  sprintf (scratch, "#define SCANLINE_GAP_BRIGHTNESS %f\n", scanline_gap_brightness_);
  strcat(dst, scratch);

  sprintf (scratch, "#define BLOOM_FACTOR %f\n", bloom_factor_);
  strcat(dst, scratch);

  sprintf (scratch, "#define INPUT_GAMMA %f\n", input_gamma_);
  strcat(dst, scratch);

  sprintf (scratch, "#define OUTPUT_GAMMA %f\n", output_gamma_);
  strcat(dst, scratch);

  if (sharper_) {
	  strcat (dst, "#define SHARPER\n");
  }
  if (bilinear_interpolation_) {
	  strcat (dst, "#define BILINEAR_INTERPOLATION\n");
  }
}

bool FrameBufferLayer::ShaderInit() {

	// Used to for size to reserve enough space at
	// beginning of shader for dynamic defines.
	static const char* header_template =
           "#define FRAGMENT                    "
           "#define CURVATURE                   "
           "#define CURVATURE_X 0.10            "
           "#define CURVATURE_Y 0.25            "
           "#define MASK 1                      "
           "#define MASK_BRIGHTNESS 0.70        "
           "#define GAMMA                       "
           "#define FAKE_GAMMA                  "
           "#define SCANLINES                   "
           "#define MULTISAMPLE                 "
           "#define SCANLINE_WEIGHT 6.0         "
           "#define SCANLINE_GAP_BRIGHTNESS 0.12"
           "#define BLOOM_FACTOR 1.5            "
           "#define INPUT_GAMMA 2.4             "
           "#define OUTPUT_GAMMA 2.2            "
           "#define SHARPER                     "
           "#define BILINEAR_INTERPOLATION      ";

  // orthographic projection matrix
  static const GLfloat mvp_ortho[16] = { 2.0f,  0.0f,  0.0f,  0.0f,
                                         0.0f,  2.0f,  0.0f,  0.0f,
                                         0.0f,  0.0f, -1.0f,  0.0f,
                                         -1.0f, -1.0f,  0.0f,  1.0f };

  if (shader_init_) {
      return true;
  }

  const char *shader_txt;
  int len;

#ifdef LOAD_SHADER_FROM_FILE
  if (!file_shader_txt_) {
     FILE *f;
     if (mode_ == VC_IMAGE_8BPP) {
        // Use indexed texture version
        f = fopen("crt-pi-idx.gls", "r");
     } else {
        // Use rgb texture version
        f = fopen("crt-pi-rgb.gls", "r");
     }
     fseek(f, 0, SEEK_END);
     len = ftell(f);
     fseek(f, 0, SEEK_SET);
     // Never freed.
     file_shader_txt_ = (char*) malloc(len + 1);
     fread(file_shader_txt_, 1, len, f);
     fclose(f);
     file_shader_txt_[len] = 0;
  }
  shader_txt = file_shader_txt_;
#else
  // Use statically linked shader txt.
  if (mode_ == VC_IMAGE_8BPP) {
     shader_txt = idx_shader;
  } else {
     shader_txt = rgb_shader;
  }
#endif

  len = strlen(shader_txt);
  if (vshader_txt_) {
     free(vshader_txt_);
  }
  char vheader[] = "#define VERTEX\n";
  vshader_txt_ = (char*) malloc(len + 1 +
                                strlen(vheader) +
                                strlen(header_template));
  vshader_txt_[0] = '\0';
  strcpy(vshader_txt_, vheader);
  ConcatShaderDefines(vshader_txt_);
  strcat(vshader_txt_, shader_txt);

  if (fshader_txt_) {
     free(fshader_txt_);
  }
  char fheader[] = "#define FRAGMENT\n";
  fshader_txt_ = (char*) malloc(len + 1 +
                                strlen(fheader) +
                                strlen(header_template));
  fshader_txt_[0] = '\0';
  strcpy(fshader_txt_, fheader);
  ConcatShaderDefines(fshader_txt_);
  strcat(fshader_txt_, shader_txt);

  const GLchar *vshader_source = (const GLchar*) vshader_txt_;
  vshader_ = glCreateShader(GL_VERTEX_SHADER);
  if (!vshader_) {
    printf("boot: fbl vertex shader create failed 0x%x\r\n", (unsigned)glGetError());
    return false;
  }
  glShaderSource(vshader_, 1, &vshader_source, 0);
  glCompileShader(vshader_);
  GLint status = GL_FALSE;
  glGetShaderiv(vshader_, GL_COMPILE_STATUS, &status);
  if (status != GL_TRUE) {
    char log[1024];
    log[0] = '\0';
    glGetShaderInfoLog(vshader_, sizeof log, NULL, log);
    printf("boot: fbl vertex shader compile failed: %s\r\n", log);
    return false;
  }

  const GLchar *fshader_source = (const GLchar*) fshader_txt_;
  fshader_ = glCreateShader(GL_FRAGMENT_SHADER);
  if (!fshader_) {
    printf("boot: fbl fragment shader create failed 0x%x\r\n", (unsigned)glGetError());
    return false;
  }
  glShaderSource(fshader_, 1, &fshader_source, 0);
  glCompileShader(fshader_);
  status = GL_FALSE;
  glGetShaderiv(fshader_, GL_COMPILE_STATUS, &status);
  if (status != GL_TRUE) {
    char log[1024];
    log[0] = '\0';
    glGetShaderInfoLog(fshader_, sizeof log, NULL, log);
    printf("boot: fbl fragment shader compile failed: %s\r\n", log);
    return false;
  }

  shader_program_ = glCreateProgram();
  if (!shader_program_) {
    printf("boot: fbl shader program create failed 0x%x\r\n", (unsigned)glGetError());
    return false;
  }
  glAttachShader(shader_program_, vshader_);
  glAttachShader(shader_program_, fshader_);
  glLinkProgram(shader_program_);
  status = GL_FALSE;
  glGetProgramiv (shader_program_, GL_LINK_STATUS, &status);
  if (status != GL_TRUE) {
    char log[1024];
    log[0] = '\0';
    glGetProgramInfoLog(shader_program_, sizeof log, NULL, log);
    printf("boot: fbl shader link failed: %s\r\n", log);
    return false;
  }

  glUseProgram (shader_program_);

  GLint attr_vertex = glGetAttribLocation(shader_program_, "VertexCoord");
  GLint attr_texcoord = glGetAttribLocation(shader_program_, "TexCoord");
  if (attr_vertex < 0 || attr_texcoord < 0) {
    printf("boot: fbl shader attrib lookup failed vertex %d texcoord %d\r\n",
           (int)attr_vertex, (int)attr_texcoord);
    return false;
  }
  attr_vertex_ = (GLuint)attr_vertex;
  attr_texcoord_ = (GLuint)attr_texcoord;
  texture_sampler_ = glGetUniformLocation(shader_program_, "Texture");
  if (mode_ == VC_IMAGE_8BPP) {
     palette_sampler_ = glGetUniformLocation(shader_program_, "Palette");
  }
  input_size_ = glGetUniformLocation (shader_program_, "InputSize");
  output_size_= glGetUniformLocation (shader_program_, "OutputSize");
  texture_size_ = glGetUniformLocation (shader_program_, "TextureSize");
  texel_size_ = glGetUniformLocation (shader_program_, "TexelSize");

  mvp_ = glGetUniformLocation (shader_program_, "MVPMatrix");
  if (mvp_ > -1) {
	  glUniformMatrix4fv(mvp_, 1, GL_FALSE, mvp_ortho);
  }

  // This texture is the indexed bitmap data so we use LUMINANCE which
  // will show up as coordinate x in the shader. Then we use the palette
  // 256x1 texture (below) to lookup the color.
  glGenTextures(1, &tex_);
  CreateTexture();

  if (mode_ == VC_IMAGE_8BPP) {
     glGenTextures(1, &pal_);
     glBindTexture(GL_TEXTURE_2D,pal_);
     glTexImage2D(GL_TEXTURE_2D,0,GL_RGB,256,1,0,GL_RGB,GL_UNSIGNED_SHORT_5_6_5, pal_565_);
     glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
     glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
  }

  // Use full screen viewport.
  glViewport (0, 0, display_width_, display_height_);

  // For our vertex and texture coordinates.
  glGenBuffers(1, &vbo_);

  shader_init_ = true;
  return true;
}

void FrameBufferLayer::ShaderDestroy() {
  if (shader_init_) {
    glDeleteBuffers(1, &vbo_);
    glDeleteTextures(1, &tex_);
    glUseProgram (0);
    glDetachShader(shader_program_, vshader_);
    glDetachShader(shader_program_, fshader_);
    glDeleteProgram(shader_program_);
    glDeleteShader(vshader_);
    glDeleteShader(fshader_);
    shader_init_ = false;
  }
}

void FrameBufferLayer::ShaderUpdate() {
  // Update any shader inputs that may need to change because of
  // new values coming in after a Show().

  // Futz with the vertex coordinates so we draw into a quad
  // that matches our calculated destination rect.  These
  // need to be updates every time they can change from a
  // call to Show()

  // Bottom left
  tex_coords_[0] = (float)dst_x_ / (float)display_width_;
  tex_coords_[1] = (float)dst_y_ / (float)display_height_;

  // Bottom right
  tex_coords_[2] = (float)(dst_x_+dst_w_) / (float)display_width_;
  tex_coords_[3] = (float)dst_y_ / (float)display_height_;

  // Top left
  tex_coords_[4] = (float)dst_x_ / (float)display_width_;
  tex_coords_[5] = (float)(dst_y_ + dst_h_) / (float)display_height_;

  // Top right
  tex_coords_[6] = (float)(dst_x_ + dst_w_) / (float)display_width_;
  tex_coords_[7] = (float)(dst_y_ + dst_h_) / (float)display_height_;

  if (!need_cpu_crop_) {
    // Now futz with the texture coordinates (for triangle strip)
    // to crop out only the portion of the frame buffer we want to see.
    // When curvature is requested, we can't do this because the
    // shader seems to want the texture to be only visible pixels
    // for its curvature calculation.

    // Top left
    tex_coords_[8] = (float)src_x_ / (float)(fb_pitch_ / bytes_per_pixel_);
    tex_coords_[9] = (float)(src_y_ + src_h_) / (float)fb_height_;

    // Top right
    tex_coords_[10] = (float)(src_x_ + src_w_) / (float)(fb_pitch_ / bytes_per_pixel_);
    tex_coords_[11] = (float)(src_y_ + src_h_) / (float)fb_height_;

    // Bottom left
    tex_coords_[12] = (float)src_x_ / (float)(fb_pitch_ / bytes_per_pixel_);
    tex_coords_[13] = (float)src_y_ / (float)fb_height_;

    // Bottom right
    tex_coords_[14] = (float)(src_x_+src_w_) / (float)(fb_pitch_ / bytes_per_pixel_);
    tex_coords_[15] = (float)src_y_ / (float)fb_height_;
  } else {
    tex_coords_[8] = 0.0f;
    tex_coords_[9] = 1.0f;
    tex_coords_[10] = 1.0f;
    tex_coords_[11] = 1.0f;
    tex_coords_[12] = 0.0f;
    tex_coords_[13] = 0.0f;
    tex_coords_[14] = 1.0f;
    tex_coords_[15] = 0.0f;
  }

  glBindBuffer(GL_ARRAY_BUFFER, vbo_);
  glBufferData(GL_ARRAY_BUFFER, sizeof (GLfloat) * 16, tex_coords_, GL_STATIC_DRAW);

  glEnableVertexAttribArray(attr_texcoord_);
  glVertexAttribPointer(attr_texcoord_, 2, GL_FLOAT, GL_FALSE, 0, (void *)(sizeof (float) * 8));

  glEnableVertexAttribArray(attr_vertex_);
  glVertexAttribPointer(attr_vertex_, 2, GL_FLOAT, GL_FALSE, 0, 0);

  glUseProgram (0);
}

void FrameBufferLayer::SetUsesShader(bool enabled) {
#if RASPPI == 4
  if (v3dcrt::IsAvailable()) {
    const bool supported = CanUsePi4V3d();
    if (enabled && !supported) {
      printf("boot: pi4v3d menu rejected logical_layer=%d z_layer=%d "
             "reason=unsupported-layer\r\n", logical_layer_, layer_);
    }
    g_pi4_v3d_crt_enabled = enabled && supported;
    g_pi4_v3d_crt_enabled_initialized = true;
    uses_shader_ = false;
    return;
  }
#endif
  assert(!allocated_);
  uses_shader_ = enabled;
}

void FrameBufferLayer::SetShaderParams(
    const struct bmx_crt_effect_params &params) {
  if (uses_shader_) {
     ShaderDestroy();
  }
  curvature_ = params.geometry_enabled != 0;
  need_cpu_crop_ = curvature_;
  curvature_x_ = params.curvature_x;
  curvature_y_ = params.curvature_y;
  skew_x_ = params.skew_x;
  skew_y_ = params.skew_y;
  trapezoid_ = params.trapezoid;
  rotation_degrees_ = params.rotation_degrees;
  overscan_scale_ = params.overscan_scale;
  convergence_ = params.convergence_enabled != 0;
  red_offset_x_ = params.red_offset_x;
  red_offset_y_ = params.red_offset_y;
  blue_offset_x_ = params.blue_offset_x;
  blue_offset_y_ = params.blue_offset_y;
  convergence_radial_strength_ = params.convergence_radial_strength;
  horizontal_filtering_ = params.horizontal_filtering_enabled != 0;
  horizontal_sigma_x_ = params.horizontal_sigma_x;
  mask_ = params.phosphor_mask_enabled ? params.phosphor_mask_type : 0;
  mask_brightness_ = params.phosphor_mask_brightness;
  gamma_ = params.output_response_enabled != 0;
  fake_gamma_ = params.output_response_fast != 0;
  output_level_mapping_ = static_cast<unsigned>(params.output_level_mapping);
  output_saturation_ = params.output_saturation;
  black_level_ = params.black_level;
  white_clip_ = params.white_clip;
  scanlines_ = params.scanlines_enabled != 0;
  multisample_ = params.scanline_multisample != 0;
  scanline_weight_ = params.scanline_weight;
  scanline_gap_brightness_ = params.scanline_gap_brightness;
  edge_blur_ = params.edge_blur_enabled != 0;
  edge_blur_strength_ = params.edge_blur_strength;
  edge_blur_radius_ = params.edge_blur_radius;
  vignette_ = params.vignette_enabled != 0;
  vignette_strength_ = params.vignette_strength;
  vignette_scale_ = params.vignette_scale;
  vignette_softness_ = params.vignette_softness;
  uneven_illumination_ = params.uneven_illumination_enabled != 0;
  uneven_illumination_strength_ = params.uneven_illumination_strength;
  uneven_illumination_scale_ = params.uneven_illumination_scale;
  glass_reflection_ = params.glass_reflection_enabled != 0;
  glass_reflection_angle_ = params.glass_reflection_angle;
  glass_reflection_width_ = params.glass_reflection_width;
  glass_reflection_position_ = params.glass_reflection_position;
  rounded_screen_mask_ = params.rounded_screen_mask_enabled != 0;
  rounded_corner_radius_ = params.rounded_corner_radius;
  rounded_border_softness_ = params.rounded_border_softness;
  edge_glow_ = params.edge_glow_enabled != 0;
  edge_glow_strength_ = params.edge_glow_strength;
  edge_glow_width_ = params.edge_glow_width;
  bloom_ = params.bloom_enabled != 0;
  bloom_factor_ = bloom_ ? params.bloom_factor : 1.0f;
  horizontal_jitter_ = params.horizontal_jitter_enabled != 0;
  horizontal_jitter_strength_ = params.horizontal_jitter_strength;
  horizontal_jitter_frequency_ = params.horizontal_jitter_frequency;
  horizontal_jitter_speed_ = params.horizontal_jitter_speed;
  composite_artifacts_ = params.composite_artifacts_enabled != 0;
  composite_chroma_blur_ = params.composite_chroma_blur;
  composite_luma_sharpen_ = params.composite_luma_sharpen;
  composite_color_bleed_ = params.composite_color_bleed;
  noise_ = params.noise_enabled != 0;
  luminance_noise_ = params.luminance_noise;
  chroma_noise_ = params.chroma_noise;
  noise_speed_ = params.noise_speed;
  input_gamma_ = params.input_gamma;
  output_gamma_ = params.output_gamma;
  sharper_ = !params.horizontal_filtering_enabled ||
             params.horizontal_sigma_x < 0.5f;
  bilinear_interpolation_ = params.bilinear_interpolation != 0;
#if RASPPI == 4
  if (v3dcrt::IsAvailable()) {
    const bool preserve_v3d_source = CanUsePi4V3d();
    const char *live_preview = "deferred";
    if (preserve_v3d_source && allocated_ && showing_ &&
        g_pi4_v3d_crt_enabled) {
      const unsigned preview_slot = static_cast<unsigned>(1 - rnum_);
      FrameReady(1);
      PresentLayer(true, this);
      live_preview = pi4_v3d_ready_[preview_slot] ?
          "presented" : "fallback";
    }
    printf("boot: pi4v3d menu params geometry=%u curvature_x10000=%u,%u "
           "skew_x10000=%d,%d trapezoid_x10000=%d "
           "rotation_x100=%d overscan_x100=%u "
           "scanlines=%u multisample=%u weight_x100=%u "
           "gap_x100=%u edge_blur=%u edge_strength_x100=%u "
           "edge_radius_x100=%u phosphor_mask=%u mask_pattern=%u "
           "mask_brightness_x100=%u vignette=%u vignette_strength_x100=%u "
           "vignette_scale_x100=%u vignette_softness_x100=%u "
           "uneven_illumination=%u uneven_strength_x100=%u "
           "uneven_scale_x100=%u glass_reflection=%u "
           "glass_angle_x100=%d glass_width_x100=%u "
           "glass_position_x100=%u rounded_screen_mask=%u "
           "rounded_radius_x100=%u rounded_softness_x100=%u "
           "edge_glow=%u edge_glow_strength_x100=%u "
           "edge_glow_width_x100=%u "
           "output_response=%u response_fast=%u level_mapping=%u "
           "input_gamma_x100=%u output_gamma_x100=%u saturation_x100=%u "
           "black_level_x100=%u white_clip_x100=%u "
           "display_source=%s live_preview=%s\r\n",
           curvature_ ? 1U : 0U,
           static_cast<unsigned>(curvature_x_ * 10000.0f + 0.5f),
           static_cast<unsigned>(curvature_y_ * 10000.0f + 0.5f),
           static_cast<int>(skew_x_ * 10000.0f),
           static_cast<int>(skew_y_ * 10000.0f),
           static_cast<int>(trapezoid_ * 10000.0f),
           static_cast<int>(rotation_degrees_ * 100.0f),
           static_cast<unsigned>(overscan_scale_ * 100.0f + 0.5f),
           scanlines_ ? 1U : 0U,
           multisample_ ? 1U : 0U,
           static_cast<unsigned>(scanline_weight_ * 100.0f + 0.5f),
           static_cast<unsigned>(scanline_gap_brightness_ * 100.0f + 0.5f),
           edge_blur_ ? 1U : 0U,
           static_cast<unsigned>(edge_blur_strength_ * 100.0f + 0.5f),
           static_cast<unsigned>(edge_blur_radius_ * 100.0f + 0.5f),
           mask_ != 0 ? 1U : 0U,
           static_cast<unsigned>(mask_),
           static_cast<unsigned>(mask_brightness_ * 100.0f + 0.5f),
           vignette_ ? 1U : 0U,
           static_cast<unsigned>(vignette_strength_ * 100.0f + 0.5f),
           static_cast<unsigned>(vignette_scale_ * 100.0f + 0.5f),
           static_cast<unsigned>(vignette_softness_ * 100.0f + 0.5f),
           uneven_illumination_ ? 1U : 0U,
           static_cast<unsigned>(
               uneven_illumination_strength_ * 100.0f + 0.5f),
           static_cast<unsigned>(
               uneven_illumination_scale_ * 100.0f + 0.5f),
           glass_reflection_ ? 1U : 0U,
           static_cast<int>(glass_reflection_angle_ * 100.0f),
           static_cast<unsigned>(glass_reflection_width_ * 100.0f + 0.5f),
           static_cast<unsigned>(
               glass_reflection_position_ * 100.0f + 0.5f),
           rounded_screen_mask_ ? 1U : 0U,
           static_cast<unsigned>(rounded_corner_radius_ * 100.0f + 0.5f),
           static_cast<unsigned>(rounded_border_softness_ * 100.0f + 0.5f),
           edge_glow_ ? 1U : 0U,
           static_cast<unsigned>(edge_glow_strength_ * 100.0f + 0.5f),
           static_cast<unsigned>(edge_glow_width_ * 100.0f + 0.5f),
           gamma_ ? 1U : 0U,
           fake_gamma_ ? 1U : 0U,
           output_level_mapping_,
           static_cast<unsigned>(input_gamma_ * 100.0f + 0.5f),
           static_cast<unsigned>(output_gamma_ * 100.0f + 0.5f),
           static_cast<unsigned>(output_saturation_ * 100.0f + 0.5f),
           static_cast<unsigned>(black_level_ * 100.0f + 0.5f),
           static_cast<unsigned>(white_clip_ * 100.0f + 0.5f),
           preserve_v3d_source ? "preserved" : "recreated",
           live_preview);
    if (preserve_v3d_source) {
      // Pi4 V3D consumes these values as per-frame uniforms/package state.
      // Recreating the DispmanX element would temporarily bind the normal
      // framebuffer resource and can leave the dynamic preview on that
      // unprocessed source until a later synchronous source swap.
      return;
    }
  }
#endif
  Hide();
}

// static
bool FrameBufferLayer::Initialize() {
  if (initialized_)
     return true;

  printf("boot: fbl bcm_host init enter\r\n");
  bcm_host_init();
  printf("boot: fbl bcm_host init ready\r\n");

  printf("boot: fbl dispman display open %u enter\r\n",
         (unsigned)BMC64_DISPMANX_DISPLAY_ID);
  dispman_display_ = vc_dispmanx_display_open(BMC64_DISPMANX_DISPLAY_ID);
  if (dispman_display_ == 0) {
    printf("boot: fbl dispman display open failed\r\n");
    return false;
  }
  printf("boot: fbl dispman display open ready\r\n");

  printf("boot: fbl scaling kernel read enter\r\n");
  bcm_get_sclker(config_scaling_kernel, sizeof(config_scaling_kernel));
  printf("boot: fbl scaling kernel read ready\r\n");
  // We have to remove the '=' or else what we send back
  // won't work.
  for (unsigned int i=0;i<strlen(config_scaling_kernel);i++) {
    if (config_scaling_kernel[i] == '=') {
       config_scaling_kernel[i] = ' ';
       break;
    }
  }

#if RASPPI == 4
  ViceOptions *options = ViceOptions::Get();
  const v3dcrt::ShaderPreset v3dcrt_shader =
      v3dcrt::ParseShaderPreset(options ? options->GetV3DCrtShader() : nullptr);
  const v3dcrt::BootTestMode v3dcrt_test =
      v3dcrt::ParseBootTestMode(options ? options->GetV3DCrtTest() : nullptr);
  const v3dcrt::FragmentPackageMode v3dcrt_fragment_package =
      v3dcrt::ParseFragmentPackageMode(
          options ? options->GetV3DCrtFragmentPackage() : nullptr);
  const v3dcrt::RenderResolution v3dcrt_render_resolution =
      v3dcrt::ParseRenderResolution(
          options ? options->GetV3DCrtRenderResolution() : nullptr);
  const bool v3dcrt_requested =
      options && (options->V3DCrtEnabled() ||
                  v3dcrt_test != v3dcrt::kBootTestOff);
  const bool pi4kms_requested = options && options->Pi4KmsEnabled();
  const bool v3dcrt_fragment_probe_wait_vblank =
      !options || options->GetV3DCrtFragmentProbeWaitVblank();
  g_pi4_v3d_output_resolution =
      v3dcrt_render_resolution == v3dcrt::kRenderResolutionOutput;
  g_pi4_v3d_scanline_weight_override_enabled =
      options && options->GetV3DCrtScanlineWeight(
          &g_pi4_v3d_scanline_weight_override);
  g_pi4_v3d_scanline_gap_override_enabled =
      options && options->GetV3DCrtScanlineGapBrightness(
          &g_pi4_v3d_scanline_gap_override);
  v3dcrt::Configure(v3dcrt_requested, true, v3dcrt_shader, v3dcrt_test,
                    v3dcrt_fragment_probe_wait_vblank,
                    v3dcrt_fragment_package, v3dcrt_render_resolution);
  bool pi4kms_probe_usable = false;
  if (pi4kms_requested && v3dcrt_requested) {
    pi4kms_probe_usable = pi4kms::ProbeFirmwareScanout();
    if (pi4kms_probe_usable) {
      (void)pi4kms::ConfigureNativeMode(
          options->GetHdmiGroup(), options->GetHdmiMode(),
          options->GetPi5KmsTimings(), options->GetPi5KmsMode());
    }
  }
  if (v3dcrt_requested) {
    printf("boot: pi4 v3dcrt option shader=%s test=%s resolution=%s\r\n",
           v3dcrt::ShaderPresetName(v3dcrt_shader),
           v3dcrt::BootTestModeName(v3dcrt_test),
           v3dcrt::RenderResolutionName(v3dcrt_render_resolution));
    if (g_pi4_v3d_scanline_weight_override_enabled) {
      printf("boot: pi4v3d option scanline_weight_x100=%u\r\n",
             static_cast<unsigned>(
                 g_pi4_v3d_scanline_weight_override * 100.0f + 0.5f));
    }
    if (g_pi4_v3d_scanline_gap_override_enabled) {
      printf("boot: pi4v3d option scanline_gap_brightness_x100=%u\r\n",
             static_cast<unsigned>(
                 g_pi4_v3d_scanline_gap_override * 100.0f + 0.5f));
    }
    v3dcrt::Initialize();
    if (v3dcrt_test != v3dcrt::kBootTestOff) {
      v3dcrt::BootTestOutputLayout layout = {};
      if (v3dcrt::GetBootTestOutputLayout(v3dcrt_test, &layout)) {
        const bool layout_valid =
            layout.pitch != 0U &&
            layout.height <= kPi4V3dBootScanoutMaxBytes / layout.pitch;
        const uint32_t buffer_bytes =
            layout_valid ? layout.pitch * layout.height : 0U;
        memset(g_pi4_v3d_boot_scanout, 0,
               sizeof g_pi4_v3d_boot_scanout);
        bool presented = false;
        v3dcrt::OutputFramebuffer target = {};
        target.pixels = layout_valid ? g_pi4_v3d_boot_scanout : nullptr;
        target.width = layout.width;
        target.height = layout.height;
        target.pitch = layout.pitch;
        target.depth = layout.depth;
        target.format = layout.format;
        target.presented = &presented;
        const bool render_ready =
            layout_valid && buffer_bytes <= kPi4V3dBootScanoutMaxBytes &&
            v3dcrt::RunBootTest(target);
        Pi4V3dDispmanxResult handoff_result = {};
        const bool handoff =
            render_ready && PresentPi4V3dBootScanout(
                                dispman_display_, layout, target.pixels,
                                &handoff_result);
        presented = handoff;
        printf("boot: pi4v3d test=fragment_scanout status=%s "
               "m4=%s dispmanx_upload=%u resource_readback=%u "
               "present_sync=%u cleanup=%u normal_fallback=preserved\r\n",
               handoff ? "pass" : "fail",
               render_ready ? "pass" : "fail",
               handoff_result.upload ? 1U : 0U,
               handoff_result.resource_readback ? 1U : 0U,
               handoff_result.present_sync ? 1U : 0U,
               handoff_result.cleanup ? 1U : 0U);
      } else {
        v3dcrt::OutputFramebuffer no_target = {};
        v3dcrt::RunBootTest(no_target);
      }
    }
  }
  pi4kms::ConfigureTakeover(
      pi4kms_requested && v3dcrt_requested && pi4kms_probe_usable &&
      v3dcrt_render_resolution == v3dcrt::kRenderResolutionOutput);
  if (pi4kms_requested &&
      (!v3dcrt_requested || !pi4kms_probe_usable ||
       v3dcrt_render_resolution != v3dcrt::kRenderResolutionOutput)) {
    printf("boot: pi4kms takeover status=disabled reason=%s\r\n",
           !v3dcrt_requested ? "v3dcrt-required" :
           !pi4kms_probe_usable ? "probe-unusable" :
                                  "output-resolution-required");
  }
#endif

  initialized_ = true;
  return true;
}

// static
void FrameBufferLayer::Shutdown() {
#if RASPPI == 4
  pi4kms::Shutdown();
  v3dcrt::Shutdown();
  g_pi4_v3d_crt_enabled = true;
  g_pi4_v3d_crt_enabled_initialized = false;
  g_pi4_v3d_scanline_weight_override_enabled = false;
  g_pi4_v3d_scanline_gap_override_enabled = false;
  g_pi4_v3d_output_resolution = false;
  g_pi4_v3d_scanline_weight_override = 0.0f;
  g_pi4_v3d_scanline_gap_override = 1.0f;
  memset(g_pi4_layers, 0, sizeof g_pi4_layers);
#endif
}

bool FrameBufferLayer::CaptureDimensions(int *width, int *height) {
  if (width == nullptr || height == nullptr || !initialized_) {
    return false;
  }
#if RASPPI == 4
  uint32_t planned_width = 0U;
  uint32_t planned_height = 0U;
  if (pi4kms::GetPlannedDisplaySize(&planned_width, &planned_height)) {
    *width = static_cast<int>(planned_width);
    *height = static_cast<int>(planned_height);
    return true;
  }
#endif
  DISPMANX_MODEINFO_T info;
  if (vc_dispmanx_display_get_info(dispman_display_, &info) != 0) {
    return false;
  }
  *width = (int)info.width;
  *height = (int)info.height;
  return *width > 0 && *height > 0;
}

bool FrameBufferLayer::CaptureRgb888(uint8_t *output, int width, int height,
                                     unsigned pitch) {
  if (output == nullptr || width <= 0 || height <= 0 ||
      pitch < (unsigned)width * 3U || !initialized_) return false;
#if RASPPI == 4
  if (pi4kms::NativeScanoutCommitted()) {
    uint32_t display_width = 0U;
    uint32_t display_height = 0U;
    FrameBufferLayer *base =
        FB_LAYER_VIC < FB_NUM_LAYERS ? g_pi4_layers[FB_LAYER_VIC] : nullptr;
    v3dcrt::OutputReadback readback = {};
    if (!pi4kms::GetPlannedDisplaySize(&display_width, &display_height) ||
        display_width == 0U || display_height == 0U || base == nullptr ||
        !base->showing_ || base->dst_x_ < 0 || base->dst_y_ < 0 ||
        base->dst_w_ <= 0 || base->dst_h_ <= 0 ||
        !v3dcrt::ReadCompletedFrame(&readback) ||
        readback.pixels == nullptr || readback.depth != 16U ||
        readback.width == 0U || readback.height == 0U ||
        readback.pitch < readback.width * sizeof(uint16_t)) {
      return false;
    }

    FrameBufferLayer *overlays[FB_NUM_LAYERS] = {};
    unsigned overlay_count = 0U;
    for (unsigned layer = 0U; layer < FB_NUM_LAYERS; ++layer) {
      FrameBufferLayer *candidate = g_pi4_layers[layer];
      if (candidate == nullptr || candidate == base ||
          !candidate->showing_ || !candidate->CanUsePi4KmsOverlay() ||
          !candidate->pi4_kms_overlay_front_valid_) {
        continue;
      }
      unsigned position = overlay_count;
      while (position != 0U &&
             overlays[position - 1U]->layer_ > candidate->layer_) {
        overlays[position] = overlays[position - 1U];
        --position;
      }
      overlays[position] = candidate;
      ++overlay_count;
    }

    for (int y = 0; y < height; ++y) {
      const uint32_t display_y = static_cast<uint32_t>(
          (static_cast<uint64_t>(y) * display_height) /
          static_cast<uint32_t>(height));
      uint8_t *destination = output + static_cast<size_t>(y) * pitch;
      for (int x = 0; x < width; ++x) {
        const uint32_t display_x = static_cast<uint32_t>(
            (static_cast<uint64_t>(x) * display_width) /
            static_cast<uint32_t>(width));
        uint32_t argb = 0xff000000U;
        if (display_x >= static_cast<uint32_t>(base->dst_x_) &&
            display_x < static_cast<uint32_t>(base->dst_x_ + base->dst_w_) &&
            display_y >= static_cast<uint32_t>(base->dst_y_) &&
            display_y < static_cast<uint32_t>(base->dst_y_ + base->dst_h_)) {
          const uint32_t source_x = static_cast<uint32_t>(
              (static_cast<uint64_t>(display_x - base->dst_x_) *
               readback.width) / static_cast<uint32_t>(base->dst_w_));
          const uint32_t source_y = static_cast<uint32_t>(
              (static_cast<uint64_t>(display_y - base->dst_y_) *
               readback.height) / static_cast<uint32_t>(base->dst_h_));
          const uint16_t *source = reinterpret_cast<const uint16_t *>(
              readback.pixels + source_y * readback.pitch);
          argb = Pi4CaptureRgb565ToArgb(source[source_x]);
        }

        for (unsigned overlay_index = 0U;
             overlay_index < overlay_count; ++overlay_index) {
          const FrameBufferLayer *overlay = overlays[overlay_index];
          if (display_x < static_cast<uint32_t>(overlay->dst_x_) ||
              display_x >= static_cast<uint32_t>(
                  overlay->dst_x_ + overlay->dst_w_) ||
              display_y < static_cast<uint32_t>(overlay->dst_y_) ||
              display_y >= static_cast<uint32_t>(
                  overlay->dst_y_ + overlay->dst_h_)) {
            continue;
          }
          const uint32_t source_x = static_cast<uint32_t>(
              (static_cast<uint64_t>(display_x - overlay->dst_x_) *
               overlay->pi4_kms_overlay_width_) /
              static_cast<uint32_t>(overlay->dst_w_));
          const uint32_t source_y = static_cast<uint32_t>(
              (static_cast<uint64_t>(display_y - overlay->dst_y_) *
               overlay->pi4_kms_overlay_height_) /
              static_cast<uint32_t>(overlay->dst_h_));
          const uint32_t *source = reinterpret_cast<const uint32_t *>(
              overlay->pi4_kms_overlay_pixels_[
                  overlay->pi4_kms_overlay_front_] +
              source_y * overlay->pi4_kms_overlay_pitch_);
          argb = Pi4CaptureBlendArgb(argb, source[source_x]);
        }
        destination[x * 3] = static_cast<uint8_t>(argb >> 16U);
        destination[x * 3 + 1] = static_cast<uint8_t>(argb >> 8U);
        destination[x * 3 + 2] = static_cast<uint8_t>(argb);
      }
    }
    return true;
  }
#endif
  uint32_t native_image = 0U;
  DISPMANX_RESOURCE_HANDLE_T resource = vc_dispmanx_resource_create(
      VC_IMAGE_RGB888, (uint32_t)width, (uint32_t)height, &native_image);
  if (resource == 0) return false;
  VC_RECT_T rect;
  vc_dispmanx_rect_set(&rect, 0U, 0U, (uint32_t)width, (uint32_t)height);
  const int snapshot = vc_dispmanx_snapshot(
      dispman_display_, resource, DISPMANX_NO_ROTATE);
  const int read = snapshot == 0
                       ? vc_dispmanx_resource_read_data(resource, &rect,
                                                        output, pitch)
                       : -1;
  vc_dispmanx_resource_delete(resource);
  return snapshot == 0 && read == 0;
}

bool FrameBufferLayer::EnsureDispmanResources() {
#if RASPPI == 4
  // NOTIFY_DISPLAY_DONE makes the firmware display service unavailable.  A
  // committed native path must never recreate a hidden DispmanX fallback.
  if (pi4kms::NativeScanoutCommitted()) {
    return false;
  }
#endif
  if (dispman_resource_[0] != 0U && dispman_resource_[1] != 0U) {
    return true;
  }
  for (unsigned slot = 0U; slot < 2U; ++slot) {
    if (dispman_resource_[slot] != 0U) {
      (void)vc_dispmanx_resource_delete(dispman_resource_[slot]);
      dispman_resource_[slot] = 0U;
    }
  }
  if (fb_width_ <= 0 || fb_height_ <= 0) {
    return false;
  }

  uint32_t native_image = 0U;
  for (unsigned slot = 0U; slot < 2U; ++slot) {
    dispman_resource_[slot] = vc_dispmanx_resource_create(
        mode_, static_cast<uint32_t>(fb_width_),
        static_cast<uint32_t>(fb_height_), &native_image);
    if (dispman_resource_[slot] == 0U) {
      for (unsigned cleanup = 0U; cleanup < 2U; ++cleanup) {
        if (dispman_resource_[cleanup] != 0U) {
          (void)vc_dispmanx_resource_delete(dispman_resource_[cleanup]);
          dispman_resource_[cleanup] = 0U;
        }
      }
      return false;
    }
  }
  vc_dispmanx_rect_set(&copy_dst_rect_, 0, 0,
                       static_cast<uint32_t>(fb_width_),
                       static_cast<uint32_t>(fb_height_));
  if (mode_ == VC_IMAGE_8BPP) {
    const void *palette = transparency_
        ? static_cast<const void *>(pal_argb_)
        : static_cast<const void *>(pal_565_);
    const int palette_bytes = transparency_
        ? static_cast<int>(sizeof pal_argb_)
        : static_cast<int>(sizeof pal_565_);
    for (unsigned slot = 0U; slot < 2U; ++slot) {
      if (vc_dispmanx_resource_set_palette(
              dispman_resource_[slot], const_cast<void *>(palette),
              0, palette_bytes) != 0) {
        for (unsigned cleanup = 0U; cleanup < 2U; ++cleanup) {
          (void)vc_dispmanx_resource_delete(dispman_resource_[cleanup]);
          dispman_resource_[cleanup] = 0U;
        }
        return false;
      }
    }
  }
  printf("boot: fbl dispman resources status=ready logical_layer=%d "
         "z_layer=%d size=%dx%d mode=%u\r\n",
         logical_layer_, layer_, fb_width_, fb_height_,
         static_cast<unsigned>(mode_));
  return true;
}

int FrameBufferLayer::Allocate(int pixelmode, uint8_t **pixels,
                               int width, int height, int *pitch) {
  int ret;
  DISPMANX_MODEINFO_T dispman_info = {};

  assert(!allocated_);
  if (!Initialize()) {
    return -1;
  }

  allocated_ = true;

  switch (pixelmode) {
     case 0:
        mode_ = VC_IMAGE_8BPP;
        bytes_per_pixel_ = 1;
        break;
     case 1:
        mode_ = VC_IMAGE_RGB565;
        bytes_per_pixel_ = 2;
        break;
     default:
        mode_ = VC_IMAGE_8BPP;
        bytes_per_pixel_ = 1;
        break;
  }

  // pitch is in bytes
  if (pitch) {
     *pitch = fb_pitch_ = ALIGN_UP(width * bytes_per_pixel_, 32);
  }

  fb_width_ = width;
  fb_height_ = height;

#if RASPPI == 4
  uint32_t planned_width = 0U;
  uint32_t planned_height = 0U;
  const bool native_layout = pi4kms::NativeScanoutCommitted() &&
      pi4kms::GetPlannedDisplaySize(&planned_width, &planned_height);
  if (native_layout) {
    display_width_ = static_cast<int>(planned_width);
    display_height_ = static_cast<int>(planned_height);
    printf("boot: pi4kms output layout logical_layer=%d "
           "owner=core1 planned=%ux%u\r\n",
           logical_layer_,
           static_cast<unsigned>(planned_width),
           static_cast<unsigned>(planned_height));
  } else
#endif
  {
    ret = vc_dispmanx_display_get_info(dispman_display_, &dispman_info);
    assert(ret == 0);
    display_width_ = dispman_info.width;
    display_height_ = dispman_info.height;
#if RASPPI == 4
    if (pi4kms::GetPlannedDisplaySize(&planned_width, &planned_height)) {
      display_width_ = static_cast<int>(planned_width);
      display_height_ = static_cast<int>(planned_height);
      printf("boot: pi4kms output layout logical_layer=%d firmware=%ux%u "
             "planned=%ux%u\r\n",
             logical_layer_, static_cast<unsigned>(dispman_info.width),
             static_cast<unsigned>(dispman_info.height),
             static_cast<unsigned>(planned_width),
             static_cast<unsigned>(planned_height));
    }
#endif
  }

  if (pixels) {
     pixels_ = (uint8_t*) malloc(fb_pitch_ * height);
     cropped_pixels_ = (uint8_t*) malloc(fb_pitch_ * fb_height_);
     *pixels = pixels_;
  }

  vc_dispmanx_rect_set(&copy_dst_rect_, 0, 0, width, height);

  bool defer_dispman_resources = false;
#if RASPPI == 4
  defer_dispman_resources = ShouldDeferPi4DispmanResources();
#endif
  if (!defer_dispman_resources && !EnsureDispmanResources()) {
    printf("boot: fbl dispman resources status=fail logical_layer=%d "
           "z_layer=%d\r\n", logical_layer_, layer_);
    return -1;
  }
#if RASPPI == 4
  if (defer_dispman_resources) {
    printf("boot: pi4kms dispman resources status=deferred "
           "logical_layer=%d z_layer=%d reason=%s\r\n",
           logical_layer_, layer_,
           pi4kms::NativeScanoutCommitted() ? "native-committed" :
                                              "native-overlay");
  }
#endif

#if RASPPI == 4
  if (v3dcrt::IsAvailable()) {
    if (logical_layer_ == FB_LAYER_VIC || logical_layer_ == FB_LAYER_VDC) {
      printf("boot: pi4v3d frame route logical_layer=%d z_layer=%d "
             "eligible=%u transparency=%u\r\n",
             logical_layer_, layer_, CanUsePi4V3d() ? 1U : 0U,
             transparency_ ? 1U : 0U);
    }
    if (CanUsePi4V3d() && !g_pi4_v3d_output_resolution &&
        !AllocatePi4V3dResources(
            static_cast<unsigned>(fb_width_),
            static_cast<unsigned>(fb_height_))) {
      printf("boot: pi4v3d frame scanout resources status=fail; "
             "normal-fallback-active\r\n");
    }
  }
#endif

  if (pixels) {
     // Don't clobber these on realloc.
     dst_x_ = 0;
     dst_y_ = 0;
     dst_w_ = width;
     dst_h_ = height;

     src_x_ = 0;
     src_y_ = 0;
     src_w_ = width;
     src_h_ = height;
  }

  EGLBoolean result;
  EGLint num_config;

  static const EGLint context_attributes[] =
  {
	 EGL_CONTEXT_CLIENT_VERSION, 2,
	 EGL_NONE
  };

  static const EGLint attribute_list[] =
  {
	EGL_RED_SIZE, 8,
	EGL_GREEN_SIZE, 8,
	EGL_BLUE_SIZE, 8,
	EGL_ALPHA_SIZE, 8,
	EGL_SURFACE_TYPE, EGL_WINDOW_BIT,
	EGL_NONE
  };

  if (uses_shader_) {
     if (!OGLInit()) {
       printf("boot: fbl disabling shader after egl init failure\r\n");
       uses_shader_ = false;
       return -1;
     }
     result = eglChooseConfig(egl_display_, attribute_list,
                               &egl_config_, 1, &num_config);
     if (result == EGL_FALSE || num_config < 1) {
       printf("boot: fbl egl choose config failed result %d count %d err 0x%x\r\n",
              (int)result, (int)num_config, (unsigned)eglGetError());
       uses_shader_ = false;
       return -1;
     }
     egl_context_ = eglCreateContext(egl_display_, egl_config_,
                                        EGL_NO_CONTEXT, context_attributes);
     if (egl_context_ == EGL_NO_CONTEXT) {
       printf("boot: fbl egl create context failed 0x%x\r\n", (unsigned)eglGetError());
       uses_shader_ = false;
       return -1;
     }
  }

  return 0;
}

int FrameBufferLayer::ReAllocate(bool shader_enable) {
  assert(allocated_);

#if RASPPI == 4
  if (v3dcrt::IsAvailable()) {
    if (shader_enable && !CanUsePi4V3d()) {
      uses_shader_ = false;
      printf("boot: pi4v3d menu rejected logical_layer=%d z_layer=%d "
             "reason=unsupported-layer\r\n", logical_layer_, layer_);
      return -1;
    }
    const bool changed = !g_pi4_v3d_crt_enabled_initialized ||
                         g_pi4_v3d_crt_enabled != shader_enable;
    g_pi4_v3d_crt_enabled = shader_enable;
    g_pi4_v3d_crt_enabled_initialized = true;
    uses_shader_ = false;
    if (changed) {
      printf("boot: pi4v3d menu master=%s present=%s "
             "logical_layer=%d z_layer=%d\r\n",
             shader_enable ? "on" : "off",
             shader_enable ? "v3d" : "framebuffer",
             logical_layer_, layer_);
    }
    return 0;
  }
#endif

  if (uses_shader_ == shader_enable) {
     // No need to realloc if nothing changed;
     return 0;
  }

  // Free layer but keep pixels.
  FreeInternal(true);

  // Change the uses shader flag.
  SetUsesShader(shader_enable);

  int pixelmode = 0;
  if (mode_ == VC_IMAGE_RGB565) pixelmode = 1;

  // Reallocate with same params.
  return Allocate(pixelmode, nullptr, fb_width_, fb_height_, nullptr);
}

void FrameBufferLayer::Clear() {
  assert (allocated_);

  memset(pixels_, 0, fb_height_ * fb_pitch_);
}

// When keepPixels is true, don't clobber any dimensions or delete
// buffers. Only tear down gl and dispmanx resources.
void FrameBufferLayer::FreeInternal(bool keepPixels) {
  int ret;

  if (!allocated_) return;

  if (uses_shader_) {
     ShaderDestroy();
  }
 
  if (showing_) {
     Hide();
  }

  if (uses_shader_) {
     eglDestroyContext(egl_display_, egl_context_);
  }

  if (!keepPixels) {
     fb_width_ = 0;
     fb_height_ = 0;
     fb_pitch_ = 0;
     free(pixels_);
     free(cropped_pixels_);
  }

#if RASPPI == 4
  FreePi4V3dResources();
  FreePi4KmsOverlayResources();
#endif

  for (unsigned slot = 0U; slot < 2U; ++slot) {
    if (dispman_resource_[slot] != 0U) {
#if RASPPI == 4
      if (pi4kms::NativeScanoutCommitted()) {
        // DISPLAY_DONE retired the firmware service.  The first native
        // present already attempted to release these recovery anchors on
        // Core 0; never re-enter DispmanX later from the Core-1 owner.
        dispman_resource_[slot] = 0U;
        continue;
      }
#endif
      ret = vc_dispmanx_resource_delete(dispman_resource_[slot]);
      if (ret == 0) {
        dispman_resource_[slot] = 0U;
      }
#if RASPPI == 4
      else if (pi4kms::NativeScanoutCommitted()) {
        printf("boot: pi4kms dispman resource cleanup status=fail "
               "logical_layer=%d slot=%u result=%d\r\n",
               logical_layer_, slot, ret);
      }
#endif
      else {
        assert(ret == 0);
      }
    }
  }
  dispman_element_ = 0U;

  allocated_ = false;
}

void FrameBufferLayer::Free() {
  FreeInternal(false);
}

void FrameBufferLayer::Show() {
  static bool first_show_logged = false;
  int ret;
  DISPMANX_UPDATE_HANDLE_T dispman_update;

  if (showing_) return;

  int dst_w;
  int dst_h;
  assert (hstretch_ != 0);
  assert (vstretch_ != 0);

  int lpad_abs = display_width_ * leftPadding_;
  int rpad_abs = display_width_ * rightPadding_;
  int tpad_abs = display_height_ * topPadding_;
  int bpad_abs = display_height_ * bottomPadding_;

  int avail_width = display_width_ - lpad_abs - rpad_abs;
  int avail_height = display_height_ - tpad_abs - bpad_abs;

  if (hstretch_ < 0) {
     // Stretch horizontally to fill width * vstretch and then set height
     // based on hstretch.  This mode doesn't support integer stretch.
     dst_w = avail_width * vstretch_;
     dst_h = avail_width / -hstretch_;
     if (dst_w > avail_width) {
        dst_w = avail_width;
     }
     if (dst_h > avail_height) {
        dst_h = avail_height;
     }
  } else {
     // Stretch vertically to fill height * vstretch and then set width
     // based on hstretch.
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

#if RASPPI == 4
  const bmc64::MachineId output_machine = bmc64::CurrentMachine().id;
  if (g_pi4_v3d_output_resolution &&
      output_machine == bmc64::MachineId::VIC20 &&
      fb_width_ >= 1108 && fb_height_ >= 312) {
    src_x_ = 540;
    src_y_ = 28;
    src_w_ = 568;
    src_h_ = 284;
    dst_w = 1242;
    dst_h = 720;
    printf("boot: pi4v3d output-geometry machine=VIC20 "
           "source=540,28 568x284 target=1242x720\r\n");
  } else if (g_pi4_v3d_output_resolution &&
             output_machine == bmc64::MachineId::PET &&
             fb_height_ >= 272) {
    if (fb_width_ >= 704) {
      src_x_ = 4;
      src_y_ = 8;
      src_w_ = 696;
      src_h_ = 256;
      dst_w = 1100;
      dst_h = 720;
      printf("boot: pi4v3d output-geometry machine=PET80 "
             "source=4,8 696x256 target=1100x720\r\n");
    } else if (fb_width_ >= 384) {
      src_x_ = 4;
      src_y_ = 8;
      src_w_ = 376;
      src_h_ = 256;
      dst_w = 1003;
      dst_h = 720;
      printf("boot: pi4v3d output-geometry machine=PET40 "
             "source=4,8 376x256 target=1003x720\r\n");
    }
  }
#endif

  // Resulting image is centered
  int oy;
  switch (valign_) {
     case 0:
        // Center
        oy = (avail_height - dst_h) / 2 + v_center_offset_;
        break;
     case -1:
        // Top
        oy = vpadding_;
        break;
     case 1:
        // Bottom
        oy = avail_height - dst_h - vpadding_;
        break;
     default:
        oy = 0;
        break;
  }

  int ox;
  switch (halign_) {
     case 0:
        // Center
        ox = (avail_width - dst_w) / 2 + h_center_offset_;
        break;
     case -1:
        // Left
        ox = hpadding_;
        break;
     case 1:
        // Right
        ox = avail_width - dst_w - hpadding_;
        break;
     default:
        ox = 0;
        break;
  }


  dst_x_ = ox + lpad_abs;
  dst_y_ = oy + tpad_abs;
  dst_w_ = dst_w;
  dst_h_ = dst_h;

#if RASPPI == 4
  if (v3dcrt::IsAvailable() && CanUsePi4V3d()) {
    const unsigned render_width = static_cast<unsigned>(
        g_pi4_v3d_output_resolution ? dst_w_ : fb_width_);
    const unsigned render_height = static_cast<unsigned>(
        g_pi4_v3d_output_resolution ? dst_h_ : fb_height_);
    if (!AllocatePi4V3dResources(render_width, render_height)) {
      printf("boot: pi4v3d frame scanout resources status=fail "
             "resolution=%s target=%ux%u; normal-fallback-active\r\n",
             g_pi4_v3d_output_resolution ? "output" : "source",
             render_width, render_height);
    }
  }
#endif

  if (uses_shader_) {
     // When we use opengl + shader the source rect needs to
     // be the dest rect because the shader ends up doing the
     // cropping/scaling. So we don't want any scaling done
     // by the dispman layer.
     vc_dispmanx_rect_set(&src_rect_,
                       dst_x_ << 16,
                       dst_y_ << 16,
                       dst_w_ << 16,
                       dst_h_ << 16);
  } else {
     // When we're using just dispmanx, we isolate and crop
     // the region in the layer we want to scale up to the
     // dest rect.
     vc_dispmanx_rect_set(&src_rect_,
                       src_x_ << 16,
                       src_y_ << 16,
                       src_w_ << 16,
                       src_h_ << 16);
  }

  vc_dispmanx_rect_set(&scale_dst_rect_,
                       dst_x_,
                       dst_y_,
                       dst_w_,
                       dst_h_);

#if RASPPI == 4
  if (g_pi4_v3d_output_resolution) {
    vc_dispmanx_rect_set(&pi4_v3d_src_rect_, 0, 0,
                         pi4_v3d_width_ << 16,
                         pi4_v3d_height_ << 16);
  } else {
    pi4_v3d_src_rect_ = src_rect_;
  }
#endif

#if RASPPI == 4
  if (pi4kms::NativeScanoutCommitted() && CanUsePi4V3d()) {
    showing_ = true;
    FrameReady(0);
    PresentLayer(false, this);
    return;
  }
  if (pi4kms::FirmwareDisplayClaimed() && CanUsePi4KmsOverlay()) {
    showing_ = true;
    printf("boot: pi4kms overlay show logical_layer=%d z_layer=%d "
           "src=%d,%d %dx%d dst=%d,%d %dx%d\r\n",
           logical_layer_, layer_, src_x_, src_y_, src_w_, src_h_,
           dst_x_, dst_y_, dst_w_, dst_h_);
    PresentLayer(false, this);
    return;
  }
#endif

  if (!EnsureDispmanResources()) {
    printf("boot: fbl show status=fail reason=dispman-resource "
           "logical_layer=%d z_layer=%d\r\n", logical_layer_, layer_);
    return;
  }
  dispman_update = vc_dispmanx_update_start(0);
  assert( dispman_update );

  rnum_ = 0;
  dispman_element_ = vc_dispmanx_element_add(dispman_update,
                                            dispman_display_,
                                            layer_, // layer
                                            &scale_dst_rect_,
                                            dispman_resource_[rnum_],
                                            &src_rect_,
                                            DISPMANX_PROTECTION_NONE,
                                            &alpha_,
                                            NULL,             // clamp
                                            DISPMANX_NO_ROTATE);

  ret = vc_dispmanx_update_submit(dispman_update, NULL, NULL);
  assert( ret == 0 );

  if (uses_shader_) {
    bool shader_ready = false;
    egl_native_window_.element = dispman_element_;
    egl_native_window_.width = display_width_;
    egl_native_window_.height = display_height_;
    egl_surface_ = eglCreateWindowSurface(egl_display_, egl_config_, &egl_native_window_, NULL );
    if (egl_surface_ == EGL_NO_SURFACE) {
      printf("boot: fbl egl create window surface failed 0x%x\r\n",
             (unsigned)eglGetError());
    } else {
      EGLBoolean result;
      result = eglMakeCurrent(egl_display_, egl_surface_, egl_surface_, egl_context_);
      if (result == EGL_FALSE) {
        printf("boot: fbl egl make current failed 0x%x\r\n", (unsigned)eglGetError());
      } else if (ShaderInit()) {
        ShaderUpdate();

        glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);

        eglSwapInterval(egl_display_, 0);
        eglSwapBuffers(egl_display_, egl_surface_);
        shader_ready = true;
      }
    }

    if (!shader_ready) {
      printf("boot: fbl shader show failed; falling back to dispmanx\r\n");
      if (egl_surface_ != EGL_NO_SURFACE) {
        eglMakeCurrent(egl_display_, EGL_NO_SURFACE, EGL_NO_SURFACE, egl_context_);
        eglDestroySurface(egl_display_, egl_surface_);
        egl_surface_ = EGL_NO_SURFACE;
      }
      dispman_update = vc_dispmanx_update_start(0);
      if (dispman_update) {
        ret = vc_dispmanx_element_remove(dispman_update, dispman_element_);
        if (ret != 0) {
          printf("boot: fbl fallback remove failed %d\r\n", ret);
        }
        ret = vc_dispmanx_update_submit(dispman_update, NULL, NULL);
        if (ret != 0) {
          printf("boot: fbl fallback submit failed %d\r\n", ret);
        }
      }
      showing_ = false;
      uses_shader_ = false;
      Show();
      return;
    }
  }

  if (mode_ == VC_IMAGE_8BPP) {
    UpdatePalette();
  }

  if (!first_show_logged) {
    first_show_logged = true;
    printf("bootprof: %10u us fbl first show layer %d src %d,%d %dx%d dst %d,%d %dx%d display %dx%d\r\n",
           CTimer::GetClockTicks(), layer_, src_x_, src_y_, src_w_, src_h_,
           dst_x_, dst_y_, dst_w_, dst_h_, display_width_, display_height_);
  }

  FrameReady(0);
  showing_ = true;
  PresentLayer(false, this);
}

void FrameBufferLayer::Hide() {
  int ret;
  DISPMANX_UPDATE_HANDLE_T dispman_update;

  if (!showing_) return;

#if RASPPI == 4
  if (pi4kms::NativeScanoutCommitted() && !CanUsePi4KmsOverlay()) {
    showing_ = false;
    printf("boot: pi4kms layer hide logical_layer=%d z_layer=%d "
           "action=hold-last-native-frame\r\n",
           logical_layer_, layer_);
    return;
  }
  if (pi4kms::FirmwareDisplayClaimed() && CanUsePi4KmsOverlay()) {
    showing_ = false;
    printf("boot: pi4kms overlay hide logical_layer=%d z_layer=%d\r\n",
           logical_layer_, layer_);
    PresentLayer(false, this);
    return;
  }
  if (pi4kms::TakeoverActive()) {
    (void)pi4kms::RestoreFirmwareScanout(true);
  }
#endif

  if (dispman_element_ == 0U) {
    showing_ = false;
    return;
  }

  if (uses_shader_) {
    eglMakeCurrent(egl_display_, EGL_NO_SURFACE, EGL_NO_SURFACE, egl_context_);
    eglDestroySurface(egl_display_, egl_surface_);
  }

  dispman_update = vc_dispmanx_update_start(0);
  ret = vc_dispmanx_element_remove(dispman_update, dispman_element_);
  assert(ret == 0);
  ret = vc_dispmanx_update_submit(dispman_update, NULL, NULL);
  assert(ret == 0);
  dispman_element_ = 0U;
  showing_ = false;
}

void* FrameBufferLayer::GetPixels() {
  return pixels_;
}

#if RASPPI == 4
bool FrameBufferLayer::CanUsePi4V3d() const {
  return logical_layer_ == FB_LAYER_VIC && !transparency_;
}

bool FrameBufferLayer::ShouldDeferPi4DispmanResources() const {
  return pi4kms::NativeScanoutCommitted() ||
         (pi4kms::TakeoverReady() &&
         (logical_layer_ == FB_LAYER_UI ||
          logical_layer_ == FB_LAYER_STATUS) &&
         transparency_ && mode_ == VC_IMAGE_8BPP);
}

bool FrameBufferLayer::ReleasePi4KmsDispmanResources() {
  if (!pi4kms::NativeScanoutCommitted()) {
    return false;
  }

  const bool had_element = dispman_element_ != 0U;
  unsigned released = 0U;
  unsigned failures = 0U;
  // DISPLAY_DONE has already detached the firmware-owned composition.  The
  // stale element handle must not be submitted to DispmanX again.
  dispman_element_ = 0U;
  for (unsigned slot = 0U; slot < 2U; ++slot) {
    if (dispman_resource_[slot] == 0U) {
      continue;
    }
    const int result = vc_dispmanx_resource_delete(dispman_resource_[slot]);
    if (result == 0) {
      dispman_resource_[slot] = 0U;
      ++released;
    } else {
      ++failures;
    }
  }

  static bool failure_logged = false;
  if (failures != 0U && !failure_logged) {
    failure_logged = true;
    printf("boot: pi4kms dispman recovery-anchor status=retry "
           "logical_layer=%d released=%u failures=%u\r\n",
           logical_layer_, released, failures);
  } else if (failures == 0U && (had_element || released != 0U)) {
    printf("boot: pi4kms dispman recovery-anchor status=released "
           "logical_layer=%d resources=%u element=retired\r\n",
           logical_layer_, released);
  }
  return failures == 0U;
}

bool FrameBufferLayer::AllocatePi4V3dResources(unsigned width,
                                               unsigned height) {
  if (pi4_v3d_width_ == width && pi4_v3d_height_ == height &&
      pi4_v3d_pitch_ != 0U && pi4_v3d_pixels_[0] != nullptr &&
      pi4_v3d_pixels_[1] != nullptr) {
    return pi4kms::TakeoverReady() || EnsurePi4V3dDispmanResources();
  }
  FreePi4V3dResources();
  if (width == 0U || height == 0U || width > UINT32_MAX / 2U) {
    return false;
  }

  pi4_v3d_pitch_ = ALIGN_UP(width * 2U, 32U);
  if (height > UINT32_MAX / pi4_v3d_pitch_) {
    pi4_v3d_pitch_ = 0U;
    return false;
  }
  const size_t bytes =
      static_cast<size_t>(pi4_v3d_pitch_) * height;
  if (bytes > UINT32_MAX - 31U) {
    pi4_v3d_pitch_ = 0U;
    return false;
  }
  for (unsigned slot = 0U; slot < 2U; ++slot) {
    uint8_t *allocation =
        new (HEAP_DMA30) uint8_t[static_cast<unsigned>(bytes) + 31U];
    if (allocation == nullptr) {
      FreePi4V3dResources();
      return false;
    }
    const uintptr_t aligned =
        (reinterpret_cast<uintptr_t>(allocation) + 31U) &
        ~static_cast<uintptr_t>(31U);
    pi4_v3d_allocation_[slot] = allocation;
    pi4_v3d_pixels_[slot] = reinterpret_cast<uint8_t *>(aligned);
    memset(pi4_v3d_pixels_[slot], 0, bytes);
    CleanAndInvalidateDataCacheRange(static_cast<u32>(aligned), bytes);
  }
  pi4_v3d_width_ = width;
  pi4_v3d_height_ = height;
  vc_dispmanx_rect_set(&pi4_v3d_copy_dst_rect_, 0, 0, width, height);
  const bool direct = pi4kms::TakeoverReady();
  if (!direct && !EnsurePi4V3dDispmanResources()) {
    FreePi4V3dResources();
    return false;
  }
  printf("boot: pi4v3d frame scanout resources status=ready slots=2 "
         "logical_layer=%d z_layer=%d resolution=%s size=%ux%u pitch=%u "
         "format=rgb565 backend=%s\r\n",
         logical_layer_, layer_,
         g_pi4_v3d_output_resolution ? "output" : "source",
         width, height, pi4_v3d_pitch_,
         direct ? "pi4kms-native" : "dispmanx-staged");
  return true;
}

bool FrameBufferLayer::EnsurePi4V3dDispmanResources() {
  if (pi4_v3d_resource_[0] != 0U && pi4_v3d_resource_[1] != 0U) {
    return true;
  }
  for (unsigned slot = 0U; slot < 2U; ++slot) {
    if (pi4_v3d_resource_[slot] != 0U) {
      (void)vc_dispmanx_resource_delete(pi4_v3d_resource_[slot]);
      pi4_v3d_resource_[slot] = 0U;
    }
  }
  if (pi4_v3d_width_ == 0U || pi4_v3d_height_ == 0U ||
      pi4_v3d_pixels_[0] == nullptr || pi4_v3d_pixels_[1] == nullptr) {
    return false;
  }
  uint32_t native_image = 0U;
  for (unsigned slot = 0U; slot < 2U; ++slot) {
    pi4_v3d_resource_[slot] = vc_dispmanx_resource_create(
        VC_IMAGE_RGB565, pi4_v3d_width_, pi4_v3d_height_, &native_image);
    if (pi4_v3d_resource_[slot] == 0U) {
      for (unsigned cleanup = 0U; cleanup < 2U; ++cleanup) {
        if (pi4_v3d_resource_[cleanup] != 0U) {
          (void)vc_dispmanx_resource_delete(pi4_v3d_resource_[cleanup]);
          pi4_v3d_resource_[cleanup] = 0U;
        }
      }
      return false;
    }
  }
  printf("boot: pi4v3d frame dispmanx staging status=ready "
         "logical_layer=%d size=%ux%u\r\n",
         logical_layer_, pi4_v3d_width_, pi4_v3d_height_);
  return true;
}

bool FrameBufferLayer::UploadPi4V3dDispmanResource(
    unsigned resource_index) {
  return resource_index < 2U && EnsurePi4V3dDispmanResources() &&
         vc_dispmanx_resource_write_data(
             pi4_v3d_resource_[resource_index], VC_IMAGE_RGB565,
             static_cast<int>(pi4_v3d_pitch_),
             pi4_v3d_pixels_[resource_index],
             &pi4_v3d_copy_dst_rect_) == 0;
}

void FrameBufferLayer::FreePi4V3dResources() {
  if (g_pi4_v3d_pipeline_timing.layer == this) {
    g_pi4_v3d_pipeline_timing = {};
  }
  g_pi4_v3d_pipeline_samples_remaining = 0U;
  g_pi4_v3d_pipeline_aggregate = {};
  for (unsigned slot = 0U; slot < 2U; ++slot) {
    if (pi4_v3d_resource_[slot] != 0U) {
      if (!pi4kms::NativeScanoutCommitted()) {
        const int result =
            vc_dispmanx_resource_delete(pi4_v3d_resource_[slot]);
        if (result != 0) {
          printf("boot: pi4v3d frame dispmanx delete failed "
                 "slot=%u result=%d\r\n", slot, result);
        }
      }
    }
    pi4_v3d_resource_[slot] = 0U;
    delete[] pi4_v3d_allocation_[slot];
    pi4_v3d_allocation_[slot] = nullptr;
    pi4_v3d_pixels_[slot] = nullptr;
    pi4_v3d_ready_[slot] = false;
    pi4_v3d_scanout_[slot] = {};
  }
  pi4_v3d_pitch_ = 0U;
  pi4_v3d_width_ = 0U;
  pi4_v3d_height_ = 0U;
  memset(&pi4_v3d_copy_dst_rect_, 0, sizeof pi4_v3d_copy_dst_rect_);
  memset(&pi4_v3d_src_rect_, 0, sizeof pi4_v3d_src_rect_);
}

bool FrameBufferLayer::CanUsePi4KmsOverlay() const {
  return (logical_layer_ == FB_LAYER_UI ||
          logical_layer_ == FB_LAYER_STATUS) &&
         transparency_ && mode_ == VC_IMAGE_8BPP &&
         pixels_ != nullptr && fb_width_ > 0 && fb_height_ > 0 &&
         fb_pitch_ >= fb_width_ &&
         src_x_ >= 0 && src_y_ >= 0 && src_w_ > 0 && src_h_ > 0 &&
         src_x_ + src_w_ <= fb_width_ &&
         src_y_ + src_h_ <= fb_height_ &&
         dst_x_ >= 0 && dst_y_ >= 0 && dst_w_ > 0 && dst_h_ > 0 &&
         dst_x_ + dst_w_ <= display_width_ &&
         dst_y_ + dst_h_ <= display_height_;
}

bool FrameBufferLayer::AllocatePi4KmsOverlayResources(unsigned width,
                                                       unsigned height) {
  if (pi4_kms_overlay_width_ == width &&
      pi4_kms_overlay_height_ == height &&
      pi4_kms_overlay_pitch_ != 0U &&
      pi4_kms_overlay_pixels_[0] != nullptr &&
      pi4_kms_overlay_pixels_[1] != nullptr) {
    return true;
  }
  FreePi4KmsOverlayResources();
  if (width == 0U || height == 0U || width > UINT32_MAX / 4U) {
    return false;
  }

  const unsigned pitch = ALIGN_UP(width * 4U, 32U);
  if (height > UINT32_MAX / pitch) {
    return false;
  }
  const unsigned bytes = pitch * height;
  if (bytes > UINT32_MAX - 31U) {
    return false;
  }
  for (unsigned slot = 0U; slot < 2U; ++slot) {
    uint8_t *allocation = new (HEAP_DMA30) uint8_t[bytes + 31U];
    if (allocation == nullptr) {
      FreePi4KmsOverlayResources();
      return false;
    }
    const uintptr_t aligned =
        (reinterpret_cast<uintptr_t>(allocation) + 31U) &
        ~static_cast<uintptr_t>(31U);
    pi4_kms_overlay_allocation_[slot] = allocation;
    pi4_kms_overlay_pixels_[slot] = reinterpret_cast<uint8_t *>(aligned);
    memset(pi4_kms_overlay_pixels_[slot], 0, bytes);
    CleanAndInvalidateDataCacheRange(
        static_cast<u32>(aligned), bytes);
  }
  pi4_kms_overlay_pitch_ = pitch;
  pi4_kms_overlay_width_ = width;
  pi4_kms_overlay_height_ = height;
  pi4_kms_overlay_front_ = 0U;
  pi4_kms_overlay_front_valid_ = false;
  printf("boot: pi4kms overlay buffers status=ready logical_layer=%d "
         "size=%ux%u pitch=%u slots=2 format=argb8888\r\n",
         logical_layer_, width, height, pitch);
  return true;
}

void FrameBufferLayer::FreePi4KmsOverlayResources() {
  for (unsigned slot = 0U; slot < 2U; ++slot) {
    delete[] pi4_kms_overlay_allocation_[slot];
    pi4_kms_overlay_allocation_[slot] = nullptr;
    pi4_kms_overlay_pixels_[slot] = nullptr;
  }
  pi4_kms_overlay_pitch_ = 0U;
  pi4_kms_overlay_width_ = 0U;
  pi4_kms_overlay_height_ = 0U;
  pi4_kms_overlay_front_ = 0U;
  pi4_kms_overlay_front_valid_ = false;
}

bool FrameBufferLayer::BuildPi4KmsOverlayPlane(unsigned resource_index,
                                               pi4kms::Plane *plane) {
  if (resource_index >= 2U || plane == nullptr ||
      !CanUsePi4KmsOverlay() ||
      !AllocatePi4KmsOverlayResources(
          static_cast<unsigned>(src_w_), static_cast<unsigned>(src_h_))) {
    return false;
  }

  uint8_t *destination = pi4_kms_overlay_pixels_[resource_index];
  for (int y = 0; y < src_h_; ++y) {
    const uint8_t *source_row =
        pixels_ + (src_y_ + y) * fb_pitch_ + src_x_;
    uint32_t *destination_row = reinterpret_cast<uint32_t *>(
        destination + static_cast<unsigned>(y) * pi4_kms_overlay_pitch_);
    for (int x = 0; x < src_w_; ++x) {
      destination_row[x] = pal_argb_[source_row[x]];
    }
  }
  const unsigned bytes = pi4_kms_overlay_pitch_ * pi4_kms_overlay_height_;
  CleanAndInvalidateDataCacheRange(
      reinterpret_cast<u32>(destination), bytes);

  *plane = {
    reinterpret_cast<u32>(destination),
    pi4_kms_overlay_pitch_,
    pi4_kms_overlay_width_,
    pi4_kms_overlay_height_,
    pi4kms::kPlaneFormatArgb8888,
    g_pi4_kms_interpolation_enabled ? pi4kms::kScaleFilterMitchell
                                    : pi4kms::kScaleFilterNearest,
    static_cast<uint32_t>(dst_x_),
    static_cast<uint32_t>(dst_y_),
    static_cast<uint32_t>(dst_w_),
    static_cast<uint32_t>(dst_h_)
  };
  return true;
}

bool FrameBufferLayer::RenderPi4V3dFrame(unsigned resource_index) {
  if (g_pi4_v3d_pipeline_timing.layer == this) {
    g_pi4_v3d_pipeline_timing = {};
  }
  if (resource_index >= 2U || pi4_v3d_pixels_[resource_index] == nullptr ||
      pi4_v3d_pitch_ == 0U || pi4_v3d_width_ == 0U ||
      pi4_v3d_height_ == 0U ||
      !CanUsePi4V3d() ||
      !g_pi4_v3d_crt_enabled ||
      !v3dcrt::IsAvailable()) {
    return false;
  }
  pi4_v3d_scanout_[resource_index] = {};

  const u32 render_source_x = g_pi4_v3d_output_resolution ?
      static_cast<u32>(src_x_) : 0U;
  const u32 render_source_y = g_pi4_v3d_output_resolution ?
      static_cast<u32>(src_y_) : 0U;
  const u32 render_source_width = g_pi4_v3d_output_resolution ?
      static_cast<u32>(src_w_) : static_cast<u32>(fb_width_);
  const u32 render_source_height = g_pi4_v3d_output_resolution ?
      static_cast<u32>(src_h_) : static_cast<u32>(fb_height_);
  const u32 render_source_left_edge_padding =
      g_pi4_v3d_output_resolution &&
      bmc64::CurrentMachine().id == bmc64::MachineId::VIC20 &&
      render_source_width == 568U && render_source_height == 284U ?
          12U : 0U;
  if (render_source_left_edge_padding != 0U) {
    static bool vic20_centering_logged = false;
    if (!vic20_centering_logged) {
      vic20_centering_logged = true;
      printf("boot: pi4v3d source-centering machine=VIC20 "
             "left_edge_padding=%u source=%u,%u %ux%u\r\n",
             static_cast<unsigned>(render_source_left_edge_padding),
             static_cast<unsigned>(render_source_x),
             static_cast<unsigned>(render_source_y),
             static_cast<unsigned>(render_source_width),
             static_cast<unsigned>(render_source_height));
    }
  }
  v3dcrt::InputFramebuffer source = {
    pixels_,
    static_cast<u32>(fb_width_),
    static_cast<u32>(fb_height_),
    static_cast<u32>(fb_pitch_),
    mode_ == VC_IMAGE_RGB565 ? v3dcrt::kPixelFormatRgb565
                             : v3dcrt::kPixelFormatIndexed8,
    pal_565_,
    0U,
    0U,
    {render_source_x, render_source_y,
     render_source_width, render_source_height},
    render_source_left_edge_padding
  };
  v3dcrt::OutputFramebuffer target = {};
  target.pixels = pi4_v3d_pixels_[resource_index];
  target.width = static_cast<u32>(pi4_v3d_width_);
  target.height = static_cast<u32>(pi4_v3d_height_);
  target.pitch = pi4_v3d_pitch_;
  target.depth = 16U;
  target.format = v3dcrt::kPixelFormatRgb565;
  v3dcrt::EffectParams params = {};
  params.enable_interpolation = bilinear_interpolation_;
  params.enable_geometry = curvature_;
  params.curvature_x = curvature_x_;
  params.curvature_y = curvature_y_;
  params.skew_x = skew_x_;
  params.skew_y = skew_y_;
  params.trapezoid = trapezoid_;
  params.rotation_degrees = rotation_degrees_;
  params.overscan_scale = overscan_scale_;
  params.enable_convergence = convergence_;
  params.red_offset_x = red_offset_x_;
  params.red_offset_y = red_offset_y_;
  params.blue_offset_x = blue_offset_x_;
  params.blue_offset_y = blue_offset_y_;
  params.convergence_radial_strength = convergence_radial_strength_;
  params.enable_horizontal_filtering = horizontal_filtering_;
  params.horizontal_sigma_x = horizontal_sigma_x_;
  params.enable_scanlines = scanlines_;
  params.enable_scanline_multisample = multisample_;
  params.scanline_weight = g_pi4_v3d_scanline_weight_override_enabled ?
      g_pi4_v3d_scanline_weight_override : scanline_weight_;
  params.scanline_gap_brightness =
      g_pi4_v3d_scanline_gap_override_enabled ?
          g_pi4_v3d_scanline_gap_override : scanline_gap_brightness_;
  params.enable_edge_blur = edge_blur_;
  params.edge_blur_strength = edge_blur_strength_;
  params.edge_blur_radius = edge_blur_radius_;
  params.enable_mask = mask_ != 0;
  params.phosphor_mask_pattern =
      static_cast<v3dcrt::PhosphorMaskPattern>(mask_);
  params.mask_brightness = mask_brightness_;
  params.enable_vignette = vignette_;
  params.vignette_strength = vignette_strength_;
  params.vignette_scale = vignette_scale_;
  params.vignette_softness = vignette_softness_;
  params.enable_uneven_illumination = uneven_illumination_;
  params.uneven_illumination_strength = uneven_illumination_strength_;
  params.uneven_illumination_scale = uneven_illumination_scale_;
  params.enable_glass_reflection = glass_reflection_;
  params.glass_reflection_angle = glass_reflection_angle_;
  params.glass_reflection_width = glass_reflection_width_;
  params.glass_reflection_position = glass_reflection_position_;
  params.enable_rounded_screen_mask = rounded_screen_mask_;
  params.rounded_corner_radius = rounded_corner_radius_;
  params.rounded_border_softness = rounded_border_softness_;
  params.enable_edge_glow = edge_glow_;
  params.edge_glow_strength = edge_glow_strength_;
  params.edge_glow_width = edge_glow_width_;
  params.enable_bloom = bloom_;
  params.bloom_factor = bloom_factor_;
  params.enable_horizontal_jitter = horizontal_jitter_;
  params.horizontal_jitter_strength = horizontal_jitter_strength_;
  params.horizontal_jitter_frequency = horizontal_jitter_frequency_;
  params.horizontal_jitter_speed = horizontal_jitter_speed_;
  params.enable_composite_artifacts = composite_artifacts_;
  params.composite_chroma_blur = composite_chroma_blur_;
  params.composite_luma_sharpen = composite_luma_sharpen_;
  params.composite_color_bleed = composite_color_bleed_;
  params.enable_noise = noise_;
  params.luminance_noise = luminance_noise_;
  params.chroma_noise = chroma_noise_;
  params.noise_speed = noise_speed_;
  params.enable_output_response = gamma_;
  params.fast_output_response = fake_gamma_;
  params.output_level_mapping =
      static_cast<v3dcrt::OutputLevelMapping>(output_level_mapping_);
  params.input_gamma = input_gamma_;
  params.output_gamma = output_gamma_;
  params.output_saturation = output_saturation_;
  params.black_level = black_level_;
  params.white_clip = white_clip_;
  const u64 frame_start_us = CTimer::GetClockTicks64();
  if (!v3dcrt::RenderFrame(source, target, params)) {
    return false;
  }
  const u64 render_done_us = CTimer::GetClockTicks64();
  if (!pi4v3d::GetLastRenderedFrame(
          &pi4_v3d_scanout_[resource_index])) {
    return false;
  }
  static bool pi4kms_native_logged = false;
  bool write_ok = true;
  if (!pi4kms::TakeoverReady()) {
    write_ok = UploadPi4V3dDispmanResource(resource_index);
  } else if (!pi4kms_native_logged) {
    pi4kms_native_logged = true;
    printf("boot: pi4kms frame resources status=native "
           "dispmanx_staging=unallocated\r\n");
  }
  const u64 write_done_us = CTimer::GetClockTicks64();
  if (!write_ok) {
    return false;
  }
  g_pi4_v3d_pipeline_timing.valid = true;
  g_pi4_v3d_pipeline_timing.effect_change =
      v3dcrt::LastFrameChangedEffect();
  g_pi4_v3d_pipeline_timing.sample_after_effect_change =
      g_pi4_v3d_pipeline_samples_remaining != 0U &&
      !g_pi4_v3d_pipeline_timing.effect_change;
  if (g_pi4_v3d_pipeline_timing.effect_change) {
    g_pi4_v3d_pipeline_samples_remaining =
        kPi4V3dPipelineEffectSampleCount;
    g_pi4_v3d_pipeline_aggregate = {};
  }
  g_pi4_v3d_pipeline_timing.layer = this;
  g_pi4_v3d_pipeline_timing.sequence = v3dcrt::LastFrameSequence();
  g_pi4_v3d_pipeline_timing.frame_start_us = frame_start_us;
  g_pi4_v3d_pipeline_timing.render_done_us = render_done_us;
  g_pi4_v3d_pipeline_timing.write_done_us = write_done_us;
  return true;
}

bool FrameBufferLayer::TryPi4KmsPresent(bool sync,
                                        FrameBufferLayer **layers,
                                        unsigned count) {
  (void)sync;
  FrameBufferLayer *vic =
      FB_LAYER_VIC < FB_NUM_LAYERS ? g_pi4_layers[FB_LAYER_VIC] : nullptr;
  bool vic_ready = false;
  for (unsigned i = 0U; i < count; ++i) {
    vic_ready = vic_ready || layers[i] == vic;
  }

  FrameBufferLayer *overlays[FB_NUM_LAYERS - 1U] = {};
  unsigned overlay_count = 0U;
  bool supported_layers = vic != nullptr && vic->showing_;
  for (unsigned i = 0U; i < FB_NUM_LAYERS; ++i) {
    FrameBufferLayer *active = g_pi4_layers[i];
    if (active == nullptr || !active->showing_ || active == vic) {
      continue;
    }
    if (!active->CanUsePi4KmsOverlay() ||
        overlay_count >= FB_NUM_LAYERS - 1U) {
      supported_layers = false;
      break;
    }
    overlays[overlay_count++] = active;
  }
  for (unsigned i = 1U; i < overlay_count; ++i) {
    FrameBufferLayer *current = overlays[i];
    unsigned j = i;
    while (j != 0U && overlays[j - 1U]->layer_ > current->layer_) {
      overlays[j] = overlays[j - 1U];
      --j;
    }
    overlays[j] = current;
  }
  const bool eligible = pi4kms::TakeoverReady() &&
                        g_pi4_v3d_output_resolution && supported_layers &&
                        !vic->uses_shader_ &&
                        vic->CanUsePi4V3d();
  if (!eligible) {
    if (pi4kms::FirmwareDisplayClaimed()) {
      static bool unsupported_layers_logged = false;
      if (!unsupported_layers_logged) {
        unsupported_layers_logged = true;
        printf("boot: pi4kms presentation held reason=unsupported-layers "
               "firmware-display-stopped overlays=%u\r\n",
               overlay_count);
      }
      return pi4kms::TakeoverActive();
    }
    if (pi4kms::TakeoverActive() &&
        !pi4kms::RestoreFirmwareScanout(true)) {
      printf("boot: pi4kms fallback restore failed\r\n");
    }
    return false;
  }

  unsigned next_resource = vic_ready
      ? 1U - static_cast<unsigned>(vic->rnum_)
      : static_cast<unsigned>(vic->rnum_);
  if (!vic->pi4_v3d_ready_[next_resource]) {
    const unsigned alternate = next_resource ^ 1U;
    if (!vic->pi4_v3d_ready_[alternate]) {
      return false;
    }
    next_resource = alternate;
  }
  const pi4v3d::RenderedFrame &frame =
      vic->pi4_v3d_scanout_[next_resource];
  if (!vic->pi4_v3d_ready_[next_resource] || !frame.valid ||
      frame.framebuffer_bus_address == 0U ||
      frame.width != static_cast<unsigned>(vic->dst_w_) ||
      frame.height != static_cast<unsigned>(vic->dst_h_) ||
      vic->dst_x_ < 0 || vic->dst_y_ < 0 ||
      vic->display_width_ <= 0 || vic->display_height_ <= 0) {
    return false;
  }

  pi4kms::Plane kms_planes[FB_NUM_LAYERS] = {};
  kms_planes[0] = {
    frame.framebuffer_bus_address,
    frame.pitch,
    frame.width,
    frame.height,
    pi4kms::kPlaneFormatRgb565,
    g_pi4_kms_interpolation_enabled ? pi4kms::kScaleFilterMitchell
                                    : pi4kms::kScaleFilterNearest,
    static_cast<uint32_t>(vic->dst_x_),
    static_cast<uint32_t>(vic->dst_y_),
    frame.width,
    frame.height
  };
  unsigned overlay_resources[FB_NUM_LAYERS - 1U] = {};
  for (unsigned i = 0U; i < overlay_count; ++i) {
    overlay_resources[i] = overlays[i]->pi4_kms_overlay_front_valid_
        ? overlays[i]->pi4_kms_overlay_front_ ^ 1U : 0U;
    if (!overlays[i]->BuildPi4KmsOverlayPlane(
            overlay_resources[i], &kms_planes[i + 1U])) {
      static bool overlay_build_failure_logged = false;
      if (!overlay_build_failure_logged) {
        overlay_build_failure_logged = true;
        printf("boot: pi4kms overlay build status=fail logical_layer=%d "
               "src=%d,%d %dx%d dst=%d,%d %dx%d\r\n",
               overlays[i]->logical_layer_,
               overlays[i]->src_x_, overlays[i]->src_y_,
               overlays[i]->src_w_, overlays[i]->src_h_,
               overlays[i]->dst_x_, overlays[i]->dst_y_,
               overlays[i]->dst_w_, overlays[i]->dst_h_);
      }
      return pi4kms::TakeoverActive();
    }
  }
  if (!pi4kms::PresentPlanes(
          kms_planes, overlay_count + 1U,
          static_cast<uint32_t>(vic->display_width_),
          static_cast<uint32_t>(vic->display_height_), true)) {
    if (!pi4kms::NativeScanoutCommitted()) {
      (void)pi4kms::RestoreFirmwareScanout(true);
    }
    return pi4kms::FirmwareDisplayClaimed();
  }
  (void)vic->ReleasePi4KmsDispmanResources();
  vic->rnum_ = static_cast<int>(next_resource);
  for (unsigned i = 0U; i < overlay_count; ++i) {
    overlays[i]->pi4_kms_overlay_front_ = overlay_resources[i];
    overlays[i]->pi4_kms_overlay_front_valid_ = true;
  }
  static unsigned last_overlay_count = UINT32_MAX;
  if (last_overlay_count != overlay_count) {
    last_overlay_count = overlay_count;
    printf("boot: pi4kms composition status=active base=vic overlays=%u "
           "backend=hvs5-planes\r\n", overlay_count);
  }
  return true;
}
#endif

void FrameBufferLayer::FrameReady(int to_offscreen) {
  int rnum = to_offscreen ? 1 - rnum_ : rnum_;

  // Copy data into either the offscreen resource (if swap) or the
  // on screen resource (if !swap).
  if (!uses_shader_) {
#if RASPPI == 4
      if (pi4kms::FirmwareDisplayClaimed() &&
          CanUsePi4KmsOverlay()) {
        return;
      }
      const bool v3d_ready = RenderPi4V3dFrame(rnum);
      pi4_v3d_ready_[rnum] = v3d_ready;
      if (!v3d_ready || !to_offscreen) {
        if (pi4kms::NativeScanoutCommitted()) {
          return;
        }
#endif
        if (EnsureDispmanResources()) {
          vc_dispmanx_resource_write_data(dispman_resource_[rnum],
                                          mode_,
                                          fb_pitch_,
                                          pixels_,
                                          &copy_dst_rect_);
        }
#if RASPPI == 4
      }
#endif
  } else {
      RenderGL();
  }
}

// Private function to change the source of this frame buffer's
// element to the off screen resource and toggle the resource
// index in preparation for the off screen data to be shown.
void FrameBufferLayer::Swap(DISPMANX_UPDATE_HANDLE_T& dispman_update) {
  if (uses_shader_ || !showing_)
     return;

  rnum_ = 1 - rnum_;

  DISPMANX_RESOURCE_HANDLE_T resource = dispman_resource_[rnum_];
#if RASPPI == 4
  bool use_pi4_v3d = pi4_v3d_ready_[rnum_];
  if (use_pi4_v3d && pi4_v3d_resource_[rnum_] == 0U &&
      !UploadPi4V3dDispmanResource(static_cast<unsigned>(rnum_))) {
    use_pi4_v3d = false;
  }
  if (use_pi4_v3d) {
    resource = pi4_v3d_resource_[rnum_];
  }
#endif
  if (resource == 0U) {
    if (!EnsureDispmanResources()) {
      return;
    }
    resource = dispman_resource_[rnum_];
  }
  vc_dispmanx_element_change_source(dispman_update,
                                    dispman_element_, resource);
#if RASPPI == 4
  if (g_pi4_v3d_output_resolution) {
    const VC_RECT_T *source_rect =
        use_pi4_v3d ? &pi4_v3d_src_rect_ : &src_rect_;
    const int attributes_result = vc_dispmanx_element_change_attributes(
        dispman_update, dispman_element_, 0U, layer_, alpha_.opacity,
        &scale_dst_rect_, source_rect, 0U, DISPMANX_NO_ROTATE);
    if (attributes_result != 0) {
      static bool attributes_failure_logged = false;
      if (!attributes_failure_logged) {
        attributes_failure_logged = true;
        printf("boot: pi4v3d frame source-rect switch failed result=%d\r\n",
               attributes_result);
      }
    }
  }
#endif
}

void FrameBufferLayer::SwapGL(bool sync) {
  if (!uses_shader_) {
    return;
  }

  eglSwapInterval(egl_display_, sync ? 1 : 0);
  eglSwapBuffers(egl_display_, egl_surface_);
}

void FrameBufferLayer::RenderGL() {
    // Our pixels_ framebuffer includes a lot of black border area around
    // the visible pixels we want to see. When shader curvature is needed,
    // we need to provide a texture with only the pixels we actually want
    // to see, otherwise the curvature gets applied incorrectly to the
    // larger area. So we crop on the CPU rather than by texture coords.
    if (need_cpu_crop_) {
       int wid = ALIGN_UP(src_w_ * bytes_per_pixel_, 4);
       uint8_t *src_col = pixels_ + src_x_ * bytes_per_pixel_;
       for (int yy=src_y_; yy < src_y_ + src_h_;yy++) {
          memcpy (cropped_pixels_ + (yy - src_y_) * wid,
                  src_col + yy * fb_pitch_, wid);
       }
    }

    glBindTexture(GL_TEXTURE_2D,tex_);

    if (mode_ == VC_IMAGE_8BPP) {
        glTexSubImage2D(GL_TEXTURE_2D,
        0,
        0,
        0,
        need_cpu_crop_ ? src_w_ : fb_pitch_ / bytes_per_pixel_,
        need_cpu_crop_ ? src_h_ : fb_height_,
        GL_LUMINANCE,
        GL_UNSIGNED_BYTE,
        need_cpu_crop_ ? cropped_pixels_ : pixels_);
    } else {
        glTexSubImage2D(GL_TEXTURE_2D,
        0,
        0,
        0,
        need_cpu_crop_ ? src_w_ : fb_pitch_ / bytes_per_pixel_,
        need_cpu_crop_ ? src_h_ : fb_height_,
        GL_RGB,
        GL_UNSIGNED_SHORT_5_6_5,
        need_cpu_crop_ ? cropped_pixels_ : pixels_);
    }

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glBindBuffer(GL_ARRAY_BUFFER, vbo_);
    glUseProgram (shader_program_);

    glActiveTexture(GL_TEXTURE0 + 0);
    glBindTexture(GL_TEXTURE_2D, tex_);
    glUniform1i(texture_sampler_, 0);

    if (mode_ == VC_IMAGE_8BPP) {
       glActiveTexture(GL_TEXTURE0 + 1);
       glBindTexture(GL_TEXTURE_2D, pal_);
       glUniform1i(palette_sampler_, 1);
    }

    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
}


void FrameBufferLayer::PresentLayer(bool sync, FrameBufferLayer *layer) {
  if (!layer) {
    return;
  }

  FrameBufferLayer *layers[] = { layer };
  PresentLayerList(sync, layers, 1);
}

void FrameBufferLayer::PresentLayers(bool sync, FrameBufferLayer *layers,
                                     uint32_t ready_mask) {
  FrameBufferLayer *ready_layers[FB_NUM_LAYERS];
  unsigned count = 0;

  if (!layers || ready_mask == 0) {
    return;
  }

  for (unsigned i = 0; i < FB_NUM_LAYERS; i++) {
    if (ready_mask & FB_LAYER_MASK(i)) {
      ready_layers[count++] = &layers[i];
    }
  }

  PresentLayerList(sync, ready_layers, count);
}

void FrameBufferLayer::PresentLayerList(bool sync, FrameBufferLayer **layers,
                                        unsigned count) {
  static bool first_sync_swap_logged = false;
  bool dispman_swap_will_sync = false;
  bool gl_sync_done = false;

  if (!layers || count == 0) {
    return;
  }

#if RASPPI == 4
  const u64 pi4kms_present_start_us = CTimer::GetClockTicks64();
  if (TryPi4KmsPresent(sync, layers, count)) {
    const u64 pi4kms_present_done_us = CTimer::GetClockTicks64();
    RecordPi4V3dPipelinePresent(
        pi4kms_present_start_us, pi4kms_present_done_us,
        count, "pi4kms-direct");
    if (sync && !first_sync_swap_logged) {
      first_sync_swap_logged = true;
      printf("bootprof: %10u us fbl first sync swap %u layer(s) "
             "backend=pi4kms-direct\r\n",
             CTimer::GetClockTicks(), count);
    }
    return;
  }
#endif

  // We need to know whether the dispmanx code below
  // is actually going to cause a sync. It turns out if we
  // don't actually change resources on any layer,
  // the start/submit doesn't perform the sync.  So we
  // predict what will happen based on the same conditions
  // in Swap() above. If sync is requested but the code below
  // won't sync, let SwapGL take care of it.
  for (unsigned i = 0; i < count; i++) {
    if (!layers[i]->UsesShader() && layers[i]->Showing()) {
      dispman_swap_will_sync = true;
      break;
    }
  }

  for (unsigned i = 0; i < count; i++) {
    if (layers[i]->UsesShader()) {
      layers[i]->SwapGL(sync && !dispman_swap_will_sync && !gl_sync_done);
      if (sync && !dispman_swap_will_sync) {
        gl_sync_done = true;
      }
    }
  }

  if (sync) {
#if RASPPI == 4
    bool presents_timed_layer = false;
    for (unsigned i = 0; i < count; ++i) {
      if (g_pi4_v3d_pipeline_timing.valid &&
          layers[i] == g_pi4_v3d_pipeline_timing.layer) {
        presents_timed_layer = true;
        break;
      }
    }
    const u64 present_start_us = CTimer::GetClockTicks64();
#endif
    DISPMANX_UPDATE_HANDLE_T dispman_update;
    dispman_update = vc_dispmanx_update_start(0);
    for (unsigned i = 0; i < count; i++) {
      layers[i]->Swap(dispman_update);
    }
    vc_dispmanx_update_submit_sync(dispman_update);
#if RASPPI == 4
    const u64 present_done_us = CTimer::GetClockTicks64();
    if (presents_timed_layer) {
      RecordPi4V3dPipelinePresent(
          present_start_us, present_done_us, count, "dispmanx");
    }
#endif
    if (!first_sync_swap_logged) {
      first_sync_swap_logged = true;
      printf("bootprof: %10u us fbl first sync swap %u layer(s)\r\n",
             CTimer::GetClockTicks(), count);
    }
  }
}

void FrameBufferLayer::SetPalette(uint8_t index, uint16_t rgb565) {
  assert(!transparency_);
  assert (mode_ == VC_IMAGE_8BPP);
  pal_565_[index] = rgb565;
}

void FrameBufferLayer::SetPalette(uint8_t index, uint32_t argb) {
  assert(transparency_);
  assert (mode_ == VC_IMAGE_8BPP);
  pal_argb_[index] = argb;
}

void FrameBufferLayer::UpdatePalette() {
  if (!allocated_) return;
  assert (mode_ == VC_IMAGE_8BPP);

#if RASPPI == 4
  if (pi4kms::FirmwareDisplayClaimed() && CanUsePi4KmsOverlay()) {
    return;
  }
  if (pi4kms::NativeScanoutCommitted()) {
    return;
  }
  if (ShouldDeferPi4DispmanResources() &&
      dispman_resource_[0] == 0U && dispman_resource_[1] == 0U) {
    return;
  }
#endif

  if (!EnsureDispmanResources()) return;

  int ret;
  if (transparency_) {
     ret = vc_dispmanx_resource_set_palette(dispman_resource_[0],
                                            pal_argb_, 0, sizeof pal_argb_);
     ret = vc_dispmanx_resource_set_palette(dispman_resource_[1],
                                            pal_argb_, 0, sizeof pal_argb_);
  } else {
     ret = vc_dispmanx_resource_set_palette(dispman_resource_[0],
                                            pal_565_, 0, sizeof pal_565_);
     ret = vc_dispmanx_resource_set_palette(dispman_resource_[1],
                                            pal_565_, 0, sizeof pal_565_);
  }
  assert( ret == 0 );

  if (uses_shader_ & shader_init_) {
    if (transparency_) {
      // Not supported yet.
      assert(false);
    } else {
      glBindTexture(GL_TEXTURE_2D, pal_);
      glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 256, 1, GL_RGB,
                      GL_UNSIGNED_SHORT_5_6_5, pal_565_);
      RenderGL();
      PresentLayer(false, this);
    }
  }
}

void FrameBufferLayer::SetLayer(int layer) {
  if (logical_layer_ < 0) {
    logical_layer_ = layer;
#if RASPPI == 4
    if (logical_layer_ >= 0 && logical_layer_ < FB_NUM_LAYERS) {
      g_pi4_layers[logical_layer_] = this;
    }
#endif
  }
#if RASPPI == 4
  else if (layer_ != layer && v3dcrt::Requested()) {
    printf("boot: pi4v3d frame z-order logical_layer=%d old=%d new=%d\r\n",
           logical_layer_, layer_, layer);
  }
#endif
  layer_ = layer;
}

int FrameBufferLayer::GetLayer() {
  return layer_;
}

bool FrameBufferLayer::UsesShader() {
	return uses_shader_;
}

bool FrameBufferLayer::Showing() {
	return showing_;
}

void FrameBufferLayer::SetTransparency(bool transparency) {
  assert (mode_ == VC_IMAGE_8BPP);
  transparency_ = transparency;
}

void FrameBufferLayer::SetSrcRect(int x, int y, int w, int h) {
  bool has_changed = x != src_x_ || y != src_y_ || w != src_w_ || h != src_h_;
  src_x_ = x;
  src_y_ = y;
  src_w_ = w;
  src_h_ = h;

  if (has_changed && need_cpu_crop_) {
      // When using cpu crop, we have to resize our texture.
      ReCreateTexture();
  }
}

// Set horizontal/vertical multipliers
void FrameBufferLayer::SetStretch(double hstretch, double vstretch, int hintstr, int vintstr, int use_hintstr, int use_vintstr) {
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
#if RASPPI == 4
  const bool interpolation_enabled = enable != 0;
  if (g_pi4_kms_interpolation_enabled != interpolation_enabled) {
    g_pi4_kms_interpolation_enabled = interpolation_enabled;
    printf("boot: pi4kms scaling interpolation=%s plane_filter=%s\r\n",
           interpolation_enabled ? "on" : "off",
           interpolation_enabled ? "mitchell" : "nearest");
  }
  if (pi4kms::NativeScanoutCommitted()) {
    return;
  }
#endif
  if (enable) {
     bcm_set_sclker(config_scaling_kernel);
  } else {
     bcm_set_sclker(sNoInt);
  }
}
