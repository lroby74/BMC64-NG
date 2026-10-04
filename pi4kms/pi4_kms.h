#ifndef PI4KMS_PI4_KMS_H
#define PI4KMS_PI4_KMS_H

#include <stdint.h>

#include "pi4kms/pi4_kms_mode.h"

namespace pi4kms {

enum PlaneFormat {
  kPlaneFormatRgb565 = 0,
  kPlaneFormatArgb8888
};

enum ScaleFilter {
  kScaleFilterNearest = 0,
  kScaleFilterMitchell
};

struct Plane {
  // BCM2711 HVS5-visible DMA/physical address.
  uint32_t framebuffer_bus_address;
  uint32_t pitch;
  uint32_t width;
  uint32_t height;
  PlaneFormat format;
  ScaleFilter filter;
  uint32_t destination_x;
  uint32_t destination_y;
  uint32_t destination_width;
  uint32_t destination_height;
};

bool ProbeFirmwareScanout();
bool ConfigureNativeMode(unsigned hdmi_group, unsigned hdmi_mode,
                         const char *hdmi_timings,
                         const char *named_mode);
bool ConfigureNativeMode(const Mode &mode);
void ConfigureTakeover(bool requested);
bool GetPlannedDisplaySize(uint32_t *width, uint32_t *height);
bool TakeoverReady();
bool TakeoverActive();
bool FirmwareDisplayClaimed();
bool NativeScanoutCommitted();
#if BMX_V3D_RENDER_TEST_KERNEL
bool ReconfigureCommittedNativeMode(const Mode &mode);
#endif
bool SynchronizePreviousPresent(bool wait_for_vblank);
bool PresentPlanes(const Plane *planes, uint32_t plane_count,
                   uint32_t display_width, uint32_t display_height,
                   bool wait_for_vblank);
bool RestoreFirmwareScanout(bool wait_for_vblank);
void Shutdown();


void RequestHdmiPort(unsigned port);
unsigned ActiveHdmiPort();





void RequestComposite(bool composite);


bool CompositeHoldTest(unsigned duration_ms);
void LogCompositeState(const char *phase);



bool CompositeArreso();
void CompositeLascia();

}  // namespace pi4kms

#endif  // PI4KMS_PI4_KMS_H
