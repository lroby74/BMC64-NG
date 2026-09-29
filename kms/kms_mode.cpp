#include "kms/kms_mode.h"

#include <stdlib.h>
#include <string.h>

namespace bmxkms {
namespace {

struct StandardMode {
  unsigned hdmi_group;
  unsigned hdmi_mode;
  Mode mode;
};

const StandardMode kStandardModes[] = {
  {1U, 4U, {1280U, 720U, 74250000U, 110U, 40U, 220U,
            5U, 5U, 20U, true, true}},
  {1U, 19U, {1280U, 720U, 74250000U, 440U, 40U, 220U,
             5U, 5U, 20U, true, true}},
  {1U, 16U, {1920U, 1080U, 148500000U, 88U, 44U, 148U,
             4U, 5U, 36U, true, true}},
  {1U, 31U, {1920U, 1080U, 148500000U, 528U, 44U, 148U,
             4U, 5U, 36U, true, true}},
};

struct NamedMode {
  const char *name;
  Mode mode;
};

const NamedMode kNamedModes[] = {
  {"1280x800@60", {1280U, 800U, 83500000U, 72U, 128U, 200U,
                    3U, 6U, 22U, false, true}},
  {"1280x800@50", {1280U, 800U, 68000000U, 56U, 128U, 184U,
                    3U, 6U, 17U, false, true}},
  {"1920x1200@60", {1920U, 1200U, 193250000U, 136U, 200U, 336U,
                     3U, 6U, 36U, false, true}},
  {"1920x1200@50", {1920U, 1200U, 158250000U, 120U, 200U, 320U,
                     3U, 6U, 29U, false, true}},
};

bool ResolveStandardMode(unsigned hdmi_group, unsigned hdmi_mode,
                         Mode *mode) {
  for (unsigned i = 0U;
       i < sizeof kStandardModes / sizeof kStandardModes[0]; ++i) {
    if (kStandardModes[i].hdmi_group == hdmi_group &&
        kStandardModes[i].hdmi_mode == hdmi_mode) {
      *mode = kStandardModes[i].mode;
      return true;
    }
  }
  return false;
}

bool ResolveNamedMode(const char *name, Mode *mode) {
  if (name == nullptr || *name == '\0') {
    return false;
  }
  for (unsigned i = 0U;
       i < sizeof kNamedModes / sizeof kNamedModes[0]; ++i) {
    if (strcmp(kNamedModes[i].name, name) == 0) {
      *mode = kNamedModes[i].mode;
      return true;
    }
  }
  return false;
}

bool ParseTimingsPermissive(const char *timings, Mode *mode) {
  if (timings == nullptr || *timings == '\0') {
    return false;
  }
  unsigned values[17];
  const char *current = timings;
  for (unsigned i = 0U; i < 17U; ++i) {
    char *end = nullptr;
    values[i] = strtoul(current, &end, 10);
    if (end == current) {
      return false;
    }
    current = end;
    while (*current == ',' || *current == ' ') {
      ++current;
    }
  }
  mode->width = values[0];
  mode->h_sync_positive = values[1] != 0U;
  mode->h_front_porch = values[2];
  mode->h_sync = values[3];
  mode->h_back_porch = values[4];
  mode->height = values[5];
  mode->v_sync_positive = values[6] != 0U;
  mode->v_front_porch = values[7];
  mode->v_sync = values[8];
  mode->v_back_porch = values[9];
  mode->pixel_clock = values[15];
  return mode->width != 0U && mode->height != 0U &&
         mode->pixel_clock != 0U;
}

bool ParseTimingsStrict(const char *timings, Mode *mode) {
  if (timings == nullptr || *timings == '\0') {
    return false;
  }
  unsigned long values[17] = {};
  const char *current = timings;
  for (unsigned i = 0U; i < 17U; ++i) {
    while (*current == ',' || *current == ' ' || *current == '\t') {
      ++current;
    }
    char *end = nullptr;
    values[i] = strtoul(current, &end, 10);
    if (end == current || values[i] > 0xffffffffUL) {
      return false;
    }
    current = end;
  }
  while (*current == ',' || *current == ' ' || *current == '\t') {
    ++current;
  }
  if (*current != '\0' || values[10] != 0UL || values[11] != 0UL ||
      values[12] != 0UL || values[14] != 0UL ||
      values[2] > 0xffffUL || values[3] > 0xffffUL ||
      values[4] > 0xffffUL || values[7] > 0xffffUL ||
      values[8] > 0xffffUL || values[9] > 0xffffUL) {
    return false;
  }
  mode->width = static_cast<uint32_t>(values[0]);
  mode->h_sync_positive = values[1] != 0UL;
  mode->h_front_porch = static_cast<uint16_t>(values[2]);
  mode->h_sync = static_cast<uint16_t>(values[3]);
  mode->h_back_porch = static_cast<uint16_t>(values[4]);
  mode->height = static_cast<uint32_t>(values[5]);
  mode->v_sync_positive = values[6] != 0UL;
  mode->v_front_porch = static_cast<uint16_t>(values[7]);
  mode->v_sync = static_cast<uint16_t>(values[8]);
  mode->v_back_porch = static_cast<uint16_t>(values[9]);
  mode->pixel_clock = static_cast<uint32_t>(values[15]);
  return true;
}

}  // namespace

bool ResolveBmcMode(unsigned hdmi_group, unsigned hdmi_mode,
                    const char *hdmi_timings, const char *named_mode,
                    TimingParsePolicy timing_policy, Mode *mode) {
  if (mode == nullptr) {
    return false;
  }
  Mode resolved = {};
  const bool custom_mode = hdmi_group == 2U && hdmi_mode == 87U;
  const bool parsed = custom_mode &&
      (timing_policy == kTimingParseProgressiveStrict
           ? ParseTimingsStrict(hdmi_timings, &resolved)
           : ParseTimingsPermissive(hdmi_timings, &resolved));
  if (!ResolveNamedMode(named_mode, &resolved) && !parsed &&
      !ResolveStandardMode(hdmi_group, hdmi_mode, &resolved)) {
    return false;
  }
  *mode = resolved;
  return true;
}

}  // namespace bmxkms
