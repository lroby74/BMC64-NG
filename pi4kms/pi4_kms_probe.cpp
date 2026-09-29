#include "pi4kms/pi4_kms_probe.h"

#include "pi4kms/pi4_kms_dlist.h"

#include <string.h>

namespace pi4kms {

namespace {

const uint32_t kHvsEnable = 1U << 31U;
const uint32_t kPixelValveEnable = 1U;
const uint32_t kHvsChannelEnable = 1U << 31U;
}  // namespace

int32_t ResolveFirmwareHdmiChannel(const ProbeSnapshot &snapshot,
                                   int32_t *pixel_valve) {
  if (pixel_valve != nullptr) {
    *pixel_valve = -1;
  }

  uint32_t route = 3U;



  if (snapshot.prefer_hdmi1 &&
      (snapshot.pixel_valve4.control & kPixelValveEnable) != 0U) {
    route = snapshot.hvs_output5_mux >> 30U;
    if (pixel_valve != nullptr) {
      *pixel_valve = 4;
    }
  } else if ((snapshot.pixel_valve2.control & kPixelValveEnable) != 0U) {
    route = snapshot.hvs_output4_mux >> 30U;
    if (pixel_valve != nullptr) {
      *pixel_valve = 2;
    }
  } else if ((snapshot.pixel_valve4.control & kPixelValveEnable) != 0U) {
    route = snapshot.hvs_output5_mux >> 30U;
    if (pixel_valve != nullptr) {
      *pixel_valve = 4;
    }
  }

  route &= 3U;
  return route < kHvsChannelCount ? static_cast<int32_t>(route) : -1;
}

bool AnalyzeProbeSnapshot(const ProbeSnapshot &snapshot, ProbeInfo *info) {
  if (info == nullptr) {
    return false;
  }
  memset(info, 0, sizeof *info);
  info->selected_pixel_valve = -1;
  info->selected_channel = ResolveFirmwareHdmiChannel(
      snapshot, &info->selected_pixel_valve);
  info->hvs_enabled = (snapshot.hvs_control & kHvsEnable) != 0U;
  info->hdmi0_enabled =
      (snapshot.pixel_valve2.control & kPixelValveEnable) != 0U;
  info->hdmi1_enabled =
      (snapshot.pixel_valve4.control & kPixelValveEnable) != 0U;
  info->route_valid =
      info->selected_channel >= 0 &&
      static_cast<uint32_t>(info->selected_channel) < kHvsChannelCount;
  if (!info->route_valid) {
    return false;
  }

  const HvsChannelSnapshot &channel =
      snapshot.channels[info->selected_channel];
  info->channel_enabled = (channel.control & kHvsChannelEnable) != 0U;
  info->width = (channel.control >> 16U) & 0x1fffU;
  info->height = channel.control & 0x1fffU;
  info->mode = channel.status >> 30U;
  info->line = channel.status & 0xfffU;
  info->pending_dlist = channel.pending_dlist;
  info->active_dlist = channel.active_dlist;
  info->dlist_pointer_valid =
      channel.pending_dlist < kHvs5DlistWordCapacity &&
      channel.active_dlist < kHvs5DlistWordCapacity;

  const uint32_t dlist_count =
      snapshot.dlist_word_count < kProbeDlistWordCapacity ?
          snapshot.dlist_word_count : kProbeDlistWordCapacity;
  Hvs5DlistInfo dlist_info = {};
  const bool dlist_valid = InspectHvs5Dlist(
      snapshot.dlist_words, dlist_count, &dlist_info);
  info->dlist_end_found = dlist_info.end_found;
  info->dlist_plane_count = dlist_info.plane_count;
  info->dlist_used_words = dlist_info.used_words;
  info->dlist_hash = dlist_info.hash;

  return info->hvs_enabled && info->channel_enabled &&
         info->width != 0U && info->height != 0U &&
         info->dlist_pointer_valid && dlist_valid;
}

}  // namespace pi4kms
