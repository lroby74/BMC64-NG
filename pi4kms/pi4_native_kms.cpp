#include "pi4kms/pi4_native_kms.h"

#if RASPPI != 4 || AARCH != 64
#error pi4_native_kms.cpp requires a 64-bit Raspberry Pi 4 build
#endif

#include "pi4kms/pi4_kms.h"

#include <circle/new.h>
#include <circle/synchronize.h>
#include <circle/timer.h>

#include <limits.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>

namespace pi4nativekms {

namespace {

constexpr u32 kFramebufferAlignment = 32U;
PresentTiming g_last_present_timing = {};
u32 g_present_sequence = 0U;

u32 g_composite_canvas_height = 0U;
u32 g_composite_channel_height = 0U;

u32 AlignUp(u32 value, u32 alignment) {
  return (value + alignment - 1U) & ~(alignment - 1U);
}

bool LowAddress(const void *pointer, u32 *address) {
  if (pointer == nullptr || address == nullptr) {
    return false;
  }
  const uintptr value = reinterpret_cast<uintptr>(pointer);
  if (value > UINT32_MAX) {
    return false;
  }
  *address = static_cast<u32>(value);
  return true;
}

bool TranslatePlane(const Plane &source, pi4kms::Plane *target) {
  if (target == nullptr || source.framebuffer_bus_address == 0U ||
      source.pitch == 0U || source.width == 0U || source.height == 0U ||
      (source.depth != 16U && source.depth != 32U) ||
      source.source.x != 0 || source.source.y != 0 ||
      source.source.width == 0U || source.source.height == 0U ||
      source.source.width > source.width ||
      source.source.height > source.height ||
      source.destination.x < 0 || source.destination.y < 0 ||
      source.destination.width == 0U || source.destination.height == 0U) {
    return false;
  }
  if ((source.format == kPixelFormatRgb565 && source.depth != 16U) ||
      (source.format == kPixelFormatArgb8888 && source.depth != 32U)) {
    return false;
  }

  *target = {
    source.framebuffer_bus_address,
    source.pitch,
    source.source.width,
    source.source.height,
    source.format == kPixelFormatArgb8888
        ? pi4kms::kPlaneFormatArgb8888
        : pi4kms::kPlaneFormatRgb565,
    source.filter == kScaleFilterMitchell
        ? pi4kms::kScaleFilterMitchell
        : pi4kms::kScaleFilterNearest,
    static_cast<u32>(source.destination.x),
    static_cast<u32>(source.destination.y),
    source.destination.width,
    source.destination.height,
  };
  return true;
}

Plane FramebufferPlane(u32 framebuffer_bus_address, u32 pitch,
                       u32 width, u32 height, u32 depth) {
  return {
    framebuffer_bus_address,
    pitch,
    width,
    height,
    depth,
    depth == 32U ? kPixelFormatArgb8888 : kPixelFormatRgb565,
    kScaleFilterNearest,
    {0, 0, width, height},
    {0, 0, width, height},
  };
}

}  // namespace

bool ResolveBmcMode(unsigned hdmi_group, unsigned hdmi_mode,
                    const char *hdmi_timings, const char *named_mode,
                    Mode *mode) {
  return pi4kms::ResolveBmxMode(hdmi_group, hdmi_mode, hdmi_timings,
                                named_mode, mode);
}

bool SetMode(const Mode &mode) {
#if BMX_V3D_RENDER_TEST_KERNEL
  if (pi4kms::NativeScanoutCommitted()) {
    return pi4kms::ReconfigureCommittedNativeMode(mode);
  }
#endif
  if (!pi4kms::ProbeFirmwareScanout() ||
      !pi4kms::ConfigureNativeMode(mode)) {
    return false;
  }
  pi4kms::ConfigureTakeover(true);
  u32 width = 0U;
  u32 height = 0U;
  return pi4kms::TakeoverReady() &&
         pi4kms::GetPlannedDisplaySize(&width, &height) &&
         width == mode.width && height == mode.height;
}

bool CreateFramebuffer(u32 width, u32 height, u32 depth, Framebuffer *fb) {
  if (fb == nullptr || width == 0U || height == 0U ||
      (depth != 16U && depth != 32U)) {
    return false;
  }
  memset(fb, 0, sizeof *fb);
  const u32 bytes_per_pixel = depth / 8U;
  if (width > UINT32_MAX / bytes_per_pixel) {
    return false;
  }
  const u32 pitch = AlignUp(width * bytes_per_pixel, kFramebufferAlignment);
  if (height > UINT32_MAX / pitch) {
    return false;
  }
  const u32 size = pitch * height;
  if (size > UINT32_MAX - (kFramebufferAlignment - 1U)) {
    return false;
  }

  uint8_t *allocation =
      new (HEAP_DMA30) uint8_t[size + kFramebufferAlignment - 1U];
  if (allocation == nullptr) {
    return false;
  }
  const uintptr aligned =
      (reinterpret_cast<uintptr>(allocation) + kFramebufferAlignment - 1U) &
      ~static_cast<uintptr>(kFramebufferAlignment - 1U);
  uint8_t *pixels = reinterpret_cast<uint8_t *>(aligned);
  u32 bus_address = 0U;
  if (!LowAddress(pixels, &bus_address)) {
    delete[] allocation;
    return false;
  }

  memset(pixels, 0, size);
  CleanAndInvalidateDataCacheRange(aligned, size);
  fb->allocation = allocation;
  fb->pixels = pixels;
  fb->width = width;
  fb->height = height;
  fb->pitch = pitch;
  fb->depth = depth;
  fb->size = size;
  return true;
}

void DestroyFramebuffer(Framebuffer *fb) {
  if (fb == nullptr) {
    return;
  }
  delete[] fb->allocation;
  memset(fb, 0, sizeof *fb);
}

void ClearFramebuffer(const Framebuffer &fb) {
  if (fb.pixels == nullptr || fb.size == 0U) {
    return;
  }
  memset(fb.pixels, 0, fb.size);
  FlushFramebuffer(fb);
}

void FlushFramebuffer(const Framebuffer &fb) {
  if (fb.pixels != nullptr && fb.size != 0U) {
    CleanAndInvalidateDataCacheRange(
        reinterpret_cast<uintptr>(fb.pixels), fb.size);
  }
}

void FlushFramebufferRows(const Framebuffer &fb, u32 first_row,
                          u32 row_count) {
  if (fb.pixels == nullptr || fb.pitch == 0U || first_row >= fb.height ||
      row_count == 0U || row_count > fb.height - first_row) {
    return;
  }
  CleanAndInvalidateDataCacheRange(
      reinterpret_cast<uintptr>(fb.pixels + first_row * fb.pitch),
      static_cast<uintptr>(row_count) * fb.pitch);
}

bool SynchronizePreviousPresent() {
  return pi4kms::SynchronizePreviousPresent(true);
}

bool WaitForVBlank(unsigned timeout_us) {
  (void)timeout_us;
  return SynchronizePreviousPresent();
}

bool WaitForNextVBlank(unsigned timeout_us) {
  return WaitForVBlank(timeout_us);
}

bool ConfigureScanout(const Framebuffer &fb) {
  u32 address = 0U;
  if (!LowAddress(fb.pixels, &address)) {
    return false;
  }
  return ConfigureScanout(address, fb.pitch, fb.width, fb.height, fb.depth);
}

bool ConfigureScanout(u32 framebuffer_bus_address, u32 pitch, u32 width,
                      u32 height, u32 depth) {
  const Plane plane = FramebufferPlane(framebuffer_bus_address, pitch,
                                       width, height, depth);
  return ConfigureScanout(plane, width, height);
}

bool ConfigureScanout(const Plane &plane, u32 display_width,
                      u32 display_height) {
  return ConfigureScanout(&plane, 1U, display_width, display_height);
}

bool ConfigureScanout(const Plane *planes, unsigned plane_count,
                      u32 display_width, u32 display_height) {
  return PresentScanout(planes, plane_count, display_width, display_height,
                        true);
}

bool PresentScanout(const Plane *planes, unsigned plane_count,
                    u32 display_width, u32 display_height,
                    bool wait_for_vblank) {
  if (planes == nullptr || plane_count == 0U || plane_count > 4U) {
    return false;
  }
  pi4kms::Plane translated[4] = {};
  for (unsigned i = 0U; i < plane_count; ++i) {
    if (!TranslatePlane(planes[i], &translated[i])) {
      return false;
    }
  }





  if (g_composite_canvas_height != 0U &&
      g_composite_channel_height != g_composite_canvas_height &&
      display_height == g_composite_canvas_height) {
    for (unsigned i = 0U; i < plane_count; ++i) {
      const u32 top = translated[i].destination_y / 2U;
      const u32 bottom = (translated[i].destination_y +
                          translated[i].destination_height) / 2U;
      translated[i].destination_y = top;
      translated[i].destination_height = bottom > top ? bottom - top : 1U;
    }
    display_height = g_composite_channel_height;
  }

  const u64 started = CTimer::GetClockTicks64();
  const bool presented = pi4kms::PresentPlanes(
      translated, plane_count, display_width, display_height,
      wait_for_vblank);
  const u64 completed = CTimer::GetClockTicks64();
  if (presented) {
    ++g_present_sequence;
  }
  g_last_present_timing.valid = presented;
  g_last_present_timing.wait_requested = wait_for_vblank;
  g_last_present_timing.sequence = g_present_sequence;
  g_last_present_timing.wait_us = 0U;
  g_last_present_timing.total_us = static_cast<u32>(completed - started);
  return presented;
}

bool GetLastPresentTiming(PresentTiming *timing) {
  if (timing == nullptr || !g_last_present_timing.valid) {
    return false;
  }
  *timing = g_last_present_timing;
  return true;
}


void RequestHdmiPort(unsigned port) {
  pi4kms::RequestHdmiPort(port);
}

unsigned ActiveHdmiPort() {
  return pi4kms::ActiveHdmiPort();
}

bool HdmiPortSupported(unsigned port) {
  return port <= 1U;
}





bool SetCompositeMode(u32 canvas_width, u32 canvas_height,
                      u32 *channel_width, u32 *channel_height) {
  g_composite_canvas_height = 0U;
  g_composite_channel_height = 0U;
  if (channel_width == nullptr || channel_height == nullptr) {
    return false;
  }
  pi4kms::RequestComposite(true);
  const bool usable = pi4kms::ProbeFirmwareScanout();
  pi4kms::LogCompositeState("lettura");
  const char *rifiuto = nullptr;
  u32 width = 0U;
  u32 height = 0U;
  if (!usable) {
    rifiuto = "lettura";
  } else {
    pi4kms::ConfigureTakeover(true);
    if (!pi4kms::TakeoverReady() ||
        !pi4kms::GetPlannedDisplaySize(&width, &height)) {
      rifiuto = "presa";
    } else if (width != canvas_width ||
               (height != canvas_height && height * 2U != canvas_height)) {
      rifiuto = "misure";
    } else if (!pi4kms::CompositeHoldTest(600U)) {
      rifiuto = "tenuta";
    } else if (!pi4kms::GetPlannedDisplaySize(&width, &height) ||
               (height != canvas_height && height * 2U != canvas_height)) {


      rifiuto = "misure dopo la prova";
    }
  }
  if (rifiuto != nullptr) {
    pi4kms::ConfigureTakeover(false);
    pi4kms::RequestComposite(false);
    printf("boot: composito allo scalatore: no (rifiuto=%s, canale %ux%u, "
           "tela %ux%u)\r\n", rifiuto, static_cast<unsigned>(width),
           static_cast<unsigned>(height), static_cast<unsigned>(canvas_width),
           static_cast<unsigned>(canvas_height));
    return false;
  }
  g_composite_canvas_height = canvas_height;
  g_composite_channel_height = height;
  *channel_width = width;
  *channel_height = height;
  return true;
}

bool RestoreComposite() {
  const bool restored = pi4kms::RestoreFirmwareScanout(true);
  pi4kms::ConfigureTakeover(false);
  pi4kms::RequestComposite(false);
  g_composite_canvas_height = 0U;
  g_composite_channel_height = 0U;
  return restored;
}

bool CompositeArreso() {
  return pi4kms::CompositeArreso();
}

void CompositeLascia() {
  pi4kms::CompositeLascia();
  g_composite_canvas_height = 0U;
  g_composite_channel_height = 0U;
}



void SondaHdmi1() {
}

}  // namespace pi4nativekms
