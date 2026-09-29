#ifndef PI4KMS_PI4_NATIVE_KMS_H
#define PI4KMS_PI4_NATIVE_KMS_H

#include "kms/framebuffer_reuse.h"
#include "kms/kms_types.h"
#include "pi4kms/pi4_kms_mode.h"

#include <circle/types.h>
#include <stdint.h>

namespace pi4nativekms {

using Mode = pi4kms::Mode;
using Framebuffer = bmxkms::Framebuffer;
using Rect = bmxkms::Rect;
using PixelFormat = bmxkms::PixelFormat;
using ScaleFilter = bmxkms::ScaleFilter;
using Plane = bmxkms::Plane;
using PresentTiming = bmxkms::PresentTiming;
using bmxkms::CanReuseFramebuffer;
using bmxkms::ExpandedFramebufferDimension;

constexpr PixelFormat kPixelFormatRgb565 = bmxkms::kPixelFormatRgb565;
constexpr PixelFormat kPixelFormatArgb8888 = bmxkms::kPixelFormatArgb8888;
constexpr ScaleFilter kScaleFilterNearest = bmxkms::kScaleFilterNearest;
constexpr ScaleFilter kScaleFilterMitchell = bmxkms::kScaleFilterMitchell;

bool ResolveBmcMode(unsigned hdmi_group, unsigned hdmi_mode,
                    const char *hdmi_timings, const char *named_mode,
                    Mode *mode);
bool SetMode(const Mode &mode);
bool CreateFramebuffer(u32 width, u32 height, u32 depth, Framebuffer *fb);
void DestroyFramebuffer(Framebuffer *fb);
void ClearFramebuffer(const Framebuffer &fb);
void FlushFramebuffer(const Framebuffer &fb);
void FlushFramebufferRows(const Framebuffer &fb, u32 first_row,
                          u32 row_count);
bool SynchronizePreviousPresent();
bool WaitForVBlank(unsigned timeout_us = 50000U);
bool WaitForNextVBlank(unsigned timeout_us = 50000U);
bool ConfigureScanout(const Framebuffer &fb);
bool ConfigureScanout(u32 framebuffer_bus_address, u32 pitch, u32 width,
                      u32 height, u32 depth);
bool ConfigureScanout(const Plane &plane, u32 display_width,
                      u32 display_height);
bool ConfigureScanout(const Plane *planes, unsigned plane_count,
                      u32 display_width, u32 display_height);
bool PresentScanout(const Plane *planes, unsigned plane_count,
                    u32 display_width, u32 display_height,
                    bool wait_for_vblank);
bool GetLastPresentTiming(PresentTiming *timing);

void RequestHdmiPort(unsigned port);
unsigned ActiveHdmiPort();
bool HdmiPortSupported(unsigned port);
void SondaHdmi1();

}  // namespace pi4nativekms

#endif  // PI4KMS_PI4_NATIVE_KMS_H
