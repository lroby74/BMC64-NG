#ifndef PI4KMS_PI4_KMS_MODE_H
#define PI4KMS_PI4_KMS_MODE_H

#include "kms/kms_types.h"

#include <stdint.h>

namespace pi4kms {

using Mode = bmxkms::Mode;

struct ModeRegisterPlan {
  Mode mode;
  uint32_t horizontal_total;
  uint32_t vertical_total;
  uint32_t hsm_clock;
  uint32_t pixel_bvb_clock;
  uint32_t hdmi_horizontal_a;
  uint32_t hdmi_horizontal_b;
  uint32_t hdmi_vertical_a;
  uint32_t hdmi_vertical_b_even;
  uint32_t hdmi_vertical_b_odd;
  uint32_t pixel_valve_control;
  uint32_t pixel_valve_vertical_control;
  uint32_t pixel_valve_horizontal_a;
  uint32_t pixel_valve_horizontal_b;
  uint32_t pixel_valve_vertical_a;
  uint32_t pixel_valve_vertical_b;
  uint32_t hvs_channel_control;
  uint32_t phy_reset_control;
  uint32_t phy_powerdown_control;
  uint32_t phy_control0;
  uint32_t phy_control1;
  uint32_t phy_control2;
  uint32_t phy_control3;
  uint32_t phy_pll_control0;
  uint32_t phy_pll_control1;
  uint32_t phy_clock_divider;
  uint32_t phy_pll_config;
  uint32_t phy_tmds_clock_word_select;
  uint32_t phy_channel_swap;
  uint32_t phy_pll_calibration1;
  uint32_t phy_pll_calibration2;
  uint32_t phy_pll_calibration4;
  uint32_t rm_control;
  uint32_t rm_offset;
  uint32_t rm_format;
};

struct ModeTimingSignature {
  uint32_t hdmi_horizontal_a;
  uint32_t hdmi_horizontal_b;
  uint32_t hdmi_vertical_a;
  uint32_t hdmi_vertical_b;
  uint32_t pixel_valve_horizontal_a;
  uint32_t pixel_valve_horizontal_b;
  uint32_t pixel_valve_vertical_a;
  uint32_t pixel_valve_vertical_b;
};

bool ResolveBmxMode(unsigned hdmi_group, unsigned hdmi_mode,
                    const char *hdmi_timings, const char *named_mode,
                    Mode *mode);
bool ValidateMode(const Mode &mode);


bool BuildModeRegisterPlan(const Mode &mode, ModeRegisterPlan *plan,
                           unsigned hdmi_port = 0U);
bool ResolveKnownModeTiming(const ModeTimingSignature &signature,
                            ModeRegisterPlan *plan,
                            const char **mode_name);

}  // namespace pi4kms

#endif  // PI4KMS_PI4_KMS_MODE_H
