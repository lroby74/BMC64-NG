#ifndef PI4KMS_PI4_KMS_PROBE_H
#define PI4KMS_PI4_KMS_PROBE_H

#include <stdint.h>

namespace pi4kms {

static const uint32_t kHvsChannelCount = 3U;
static const uint32_t kProbeDlistWordCapacity = 64U;

struct HvsChannelSnapshot {
  uint32_t control;
  uint32_t background;
  uint32_t status;
  uint32_t pending_dlist;
  uint32_t active_dlist;
  uint32_t fifo_base;
};

struct PixelValveSnapshot {
  uint32_t control;
  uint32_t horizontal_a;
  uint32_t horizontal_b;
  uint32_t vertical_a;
  uint32_t vertical_b;
};

struct ProbeSnapshot {
  uint32_t hvs_control;
  uint32_t hvs_status;
  uint32_t hvs_identity;
  uint32_t hvs_output2_mux;
  uint32_t hvs_output5_mux;
  uint32_t hvs_output4_mux;
  HvsChannelSnapshot channels[kHvsChannelCount];
  PixelValveSnapshot pixel_valve2;
  PixelValveSnapshot pixel_valve4;


  PixelValveSnapshot pixel_valve3;
  uint32_t dlist_words[kProbeDlistWordCapacity];
  uint32_t dlist_word_count;

  bool prefer_hdmi1;

  bool prefer_composite;
};

struct ProbeInfo {
  bool hvs_enabled;
  bool hdmi0_enabled;
  bool hdmi1_enabled;
  int32_t selected_pixel_valve;
  int32_t selected_channel;
  bool route_valid;
  bool channel_enabled;
  uint32_t width;
  uint32_t height;
  uint32_t mode;
  uint32_t line;
  uint32_t pending_dlist;
  uint32_t active_dlist;
  bool dlist_pointer_valid;
  bool dlist_end_found;
  uint32_t dlist_plane_count;
  uint32_t dlist_used_words;
  uint32_t dlist_hash;
};

int32_t ResolveFirmwareHdmiChannel(const ProbeSnapshot &snapshot,
                                   int32_t *pixel_valve);
bool AnalyzeProbeSnapshot(const ProbeSnapshot &snapshot, ProbeInfo *info);

}  // namespace pi4kms

#endif  // PI4KMS_PI4_KMS_PROBE_H
