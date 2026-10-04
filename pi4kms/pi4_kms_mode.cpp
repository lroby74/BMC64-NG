#include "pi4kms/pi4_kms_mode.h"

#include "kms/kms_mode.h"

namespace pi4kms {

namespace {

const uint32_t kMinimumPixelClock = 25000000U;
// Native Pi4KMS does not configure HDMI 2.0 scrambling yet.
const uint32_t kMaximumPixelClock = 340000000U;
const uint64_t kOscillatorFrequency = 54000000ULL;

const uint32_t kPixelValveEnable = 1U << 0U;
const uint32_t kPixelValveFifoClear = 1U << 1U;
const uint32_t kPixelValveWaitHstart = 1U << 12U;
const uint32_t kPixelValveTriggerUnderflow = 1U << 13U;
const uint32_t kPixelValveClearAtStart = 1U << 14U;
const uint32_t kPixelValveFifoLevelShift = 15U;
const uint32_t kPixelValveFifoLevelHighShift = 25U;
const uint32_t kPixelValveVideoEnable = 1U << 0U;
const uint32_t kPixelValveContinuous = 1U << 1U;
const uint32_t kHvsChannelEnable = 1U << 31U;

struct PhyLaneSettings {
  uint8_t preemphasis;
  uint8_t main_driver;
  uint8_t resistance;
  uint8_t termination;
};

struct PhySettings {
  uint32_t minimum_rate;
  uint32_t maximum_rate;
  PhyLaneSettings data;
  PhyLaneSettings clock;
};

// BCM2711 HDMI0 lane settings from vc5_hdmi_phy_settings in Linux VC4 DRM.
const PhySettings kPhySettings[] = {
  {0U, 50000000U, {0x0U, 0x0aU, 0x12U, 0x0U},
                       {0x0U, 0x0aU, 0x18U, 0x0U}},
  {50000001U, 75000000U, {0x0U, 0x09U, 0x12U, 0x0U},
                              {0x0U, 0x0cU, 0x18U, 0x3U}},
  {75000001U, 165000000U, {0x0U, 0x09U, 0x12U, 0x0U},
                               {0x0U, 0x0cU, 0x18U, 0x3U}},
  {165000001U, 250000000U, {0x0U, 0x0fU, 0x12U, 0x1U},
                                {0x0U, 0x0cU, 0x18U, 0x3U}},
  {250000001U, 340000000U, {0x2U, 0x0dU, 0x12U, 0x1U},
                                {0x0U, 0x0cU, 0x18U, 0x0fU}},
};

uint32_t Field(uint32_t value, unsigned shift) {
  return value << shift;
}

const PhySettings &SelectPhySettings(uint32_t pixel_clock) {
  for (unsigned i = 0U;
       i < sizeof kPhySettings / sizeof kPhySettings[0]; ++i) {
    if (pixel_clock >= kPhySettings[i].minimum_rate &&
        pixel_clock <= kPhySettings[i].maximum_rate) {
      return kPhySettings[i];
    }
  }
  return kPhySettings[sizeof kPhySettings / sizeof kPhySettings[0] - 1U];
}

uint64_t GetVcoFrequency(uint32_t pixel_clock, uint8_t *vco_select,
                         uint8_t *vco_divider) {
  uint32_t divider = 0U;
  uint64_t frequency = pixel_clock;
  while (frequency < 3000000000ULL && divider < 255U) {
    ++divider;
    frequency = static_cast<uint64_t>(pixel_clock) * divider * 10ULL;
  }
  *vco_select = frequency > 4500000000ULL ? 1U : 0U;
  *vco_divider = static_cast<uint8_t>(divider);
  return frequency;
}

uint32_t GetRmOffset(uint64_t vco_frequency) {
  uint64_t offset = vco_frequency * 2ULL;
  offset <<= 22U;
  offset /= kOscillatorFrequency;
  offset >>= 2U;
  return static_cast<uint32_t>(offset);
}

uint32_t GetVcoGain(uint64_t vco_frequency) {
  if (vco_frequency < 3350000000ULL) return 0x0fU;
  if (vco_frequency < 3700000000ULL) return 0x0cU;
  if (vco_frequency < 4050000000ULL) return 0x06U;
  if (vco_frequency < 4800000000ULL) return 0x05U;
  if (vco_frequency < 5200000000ULL) return 0x07U;
  return 0x02U;
}

uint32_t GetChargePumpCurrent(uint64_t vco_frequency) {
  return vco_frequency < 3700000000ULL ? 0x1cU : 0x18U;
}

uint32_t MakePhyControl0(const PhyLaneSettings &data,
                         const PhyLaneSettings &clock) {
  return Field(clock.preemphasis, 5U) | Field(clock.main_driver, 0U) |
      Field(data.preemphasis, 13U) | Field(data.main_driver, 8U) |
      Field(data.preemphasis, 21U) | Field(data.main_driver, 16U) |
      Field(data.preemphasis, 29U) | Field(data.main_driver, 24U);
}

uint32_t MakePhyControl1(const PhyLaneSettings &data,
                         const PhyLaneSettings &clock) {
  return Field(clock.resistance, 0U) | Field(data.resistance, 5U) |
      Field(data.resistance, 10U) | Field(data.resistance, 15U);
}

uint32_t MakePhyControl2(const PhyLaneSettings &data,
                         const PhyLaneSettings &clock,
                         uint64_t vco_frequency) {
  return Field(clock.termination, 0U) | Field(data.termination, 4U) |
      Field(data.termination, 8U) | Field(data.termination, 12U) |
      Field(GetVcoGain(vco_frequency), 16U);
}






uint32_t MakeHdmi1PhyControl0(const PhyLaneSettings &data,
                              const PhyLaneSettings &clock) {
  return Field(data.preemphasis, 5U) | Field(data.main_driver, 0U) |
      Field(data.preemphasis, 13U) | Field(data.main_driver, 8U) |
      Field(data.preemphasis, 21U) | Field(data.main_driver, 16U) |
      Field(clock.preemphasis, 29U) | Field(clock.main_driver, 24U);
}

uint32_t MakeHdmi1PhyControl1(const PhyLaneSettings &data,
                              const PhyLaneSettings &clock) {
  return Field(data.resistance, 0U) | Field(data.resistance, 5U) |
      Field(data.resistance, 10U) | Field(clock.resistance, 15U);
}

uint32_t MakeHdmi1PhyControl2(const PhyLaneSettings &data,
                              const PhyLaneSettings &clock,
                              uint64_t vco_frequency) {
  return Field(data.termination, 0U) | Field(data.termination, 4U) |
      Field(data.termination, 8U) | Field(clock.termination, 12U) |
      Field(GetVcoGain(vco_frequency), 16U);
}

}  // namespace

bool ResolveBmxMode(unsigned hdmi_group, unsigned hdmi_mode,
                    const char *hdmi_timings, const char *named_mode,
                    Mode *mode) {
  Mode resolved = {};
  if (!bmxkms::ResolveBmcMode(
          hdmi_group, hdmi_mode, hdmi_timings, named_mode,
          bmxkms::kTimingParseProgressiveStrict, &resolved) ||
      !ValidateMode(resolved)) {
    return false;
  }
  *mode = resolved;
  return true;
}

bool ValidateMode(const Mode &mode) {
  if (mode.width == 0U || mode.width > 0x1fffU ||
      mode.height == 0U || mode.height > 0x1fffU ||
      mode.pixel_clock < kMinimumPixelClock ||
      mode.pixel_clock > kMaximumPixelClock ||
      mode.h_sync == 0U || mode.h_sync > 0x07ffU ||
      mode.h_front_porch > 0x1fffU || mode.h_back_porch > 0x07ffU ||
      mode.v_sync == 0U || mode.v_sync > 0x1fU ||
      mode.v_front_porch > 0x7fU || mode.v_back_porch > 0x1ffU) {
    return false;
  }
  // BCM2711 HDMI PixelValve2 emits two pixels per clock.
  if ((mode.width & 1U) != 0U || (mode.h_front_porch & 1U) != 0U ||
      (mode.h_sync & 1U) != 0U || (mode.h_back_porch & 1U) != 0U) {
    return false;
  }
  const uint64_t horizontal_total = static_cast<uint64_t>(mode.width) +
      mode.h_front_porch + mode.h_sync + mode.h_back_porch;
  const uint64_t vertical_total = static_cast<uint64_t>(mode.height) +
      mode.v_front_porch + mode.v_sync + mode.v_back_porch;
  return horizontal_total <= 0x3fffU && vertical_total <= 0x1fffU;
}

bool BuildModeRegisterPlan(const Mode &mode, ModeRegisterPlan *plan,
                           unsigned hdmi_port) {
  if (plan == nullptr || !ValidateMode(mode)) {
    return false;
  }

  ModeRegisterPlan result = {};
  result.mode = mode;
  result.horizontal_total = mode.width + mode.h_front_porch +
      mode.h_sync + mode.h_back_porch;
  result.vertical_total = mode.height + mode.v_front_porch +
      mode.v_sync + mode.v_back_porch;
  const uint64_t hsm =
      (static_cast<uint64_t>(mode.pixel_clock) * 101ULL + 99ULL) / 100ULL;
  result.hsm_clock = hsm < 120000000ULL ? 120000000U :
      static_cast<uint32_t>(hsm);
  result.pixel_bvb_clock = mode.pixel_clock > 297000000U ? 300000000U :
      mode.pixel_clock > 148500000U ? 150000000U : 75000000U;

  result.hdmi_horizontal_a = Field(mode.h_front_porch, 16U) |
      (mode.v_sync_positive ? (1U << 15U) : 0U) |
      (mode.h_sync_positive ? (1U << 14U) : 0U) | mode.width;
  result.hdmi_horizontal_b = Field(mode.h_back_porch, 16U) | mode.h_sync;
  result.hdmi_vertical_a = Field(mode.v_sync, 24U) |
      Field(mode.v_front_porch, 16U) | mode.height;
  result.hdmi_vertical_b_even = mode.v_back_porch;
  // VC5 programs the field-1 VSPO even for progressive modes.  It is ignored
  // by progressive scanout but keeping it exact makes the plan reusable if
  // interlaced support is added later.
  result.hdmi_vertical_b_odd =
      Field(result.horizontal_total / 2U, 16U) | mode.v_back_porch;




  const uint32_t fifo_level = hdmi_port == 1U ? 32U : 256U - 3U * 6U;
  result.pixel_valve_control = kPixelValveFifoClear |
      Field(fifo_level >> 6U, kPixelValveFifoLevelHighShift) |
      Field(fifo_level & 0x3fU, kPixelValveFifoLevelShift) |
      kPixelValveClearAtStart | kPixelValveTriggerUnderflow |
      kPixelValveWaitHstart;
  result.pixel_valve_vertical_control =
      kPixelValveContinuous | kPixelValveVideoEnable;
  result.pixel_valve_horizontal_a =
      Field(mode.h_back_porch / 2U, 16U) | mode.h_sync / 2U;
  result.pixel_valve_horizontal_b =
      Field(mode.h_front_porch / 2U, 16U) | mode.width / 2U;
  result.pixel_valve_vertical_a =
      Field(mode.v_back_porch, 16U) | mode.v_sync;
  result.pixel_valve_vertical_b =
      Field(mode.v_front_porch, 16U) | mode.height;
  result.hvs_channel_control = kHvsChannelEnable |
      Field(mode.width, 16U) | mode.height;

  uint8_t vco_select = 0U;
  uint8_t vco_divider = 0U;
  const uint64_t vco_frequency = GetVcoFrequency(
      mode.pixel_clock, &vco_select, &vco_divider);
  if (vco_divider == 0U || vco_frequency < 3000000000ULL) {
    return false;
  }
  const PhySettings &settings = SelectPhySettings(mode.pixel_clock);
  result.phy_reset_control = (1U << 5U) | (1U << 4U);
  result.phy_powerdown_control = 1U << 4U;
  result.phy_control0 = MakePhyControl0(settings.data, settings.clock);
  result.phy_control1 = MakePhyControl1(settings.data, settings.clock);
  result.phy_control2 = MakePhyControl2(
      settings.data, settings.clock, vco_frequency);
  if (hdmi_port == 1U) {
    result.phy_control0 = MakeHdmi1PhyControl0(settings.data, settings.clock);
    result.phy_control1 = MakeHdmi1PhyControl1(settings.data, settings.clock);
    result.phy_control2 = MakeHdmi1PhyControl2(
        settings.data, settings.clock, vco_frequency);
  }
  result.phy_control3 = Field(4U, 17U) | Field(6U, 12U) |
      Field(1U, 10U) | Field(1U, 8U) | Field(3U, 6U) |
      GetChargePumpCurrent(vco_frequency);
  result.phy_pll_control0 = (1U << 13U) | Field(vco_select, 9U) |
      (1U << 6U) | (1U << 5U);
  result.phy_pll_control1 = Field(0x8aU, 16U) | Field(1U, 14U) |
      (1U << 13U) | Field(3U, 11U);
  result.phy_clock_divider = Field(vco_divider, 8U);
  result.phy_pll_config = 1U;
  result.phy_tmds_clock_word_select =
      mode.pixel_clock >= 340000000U ? 3U : 0U;
  result.phy_channel_swap = Field(3U, 12U) | Field(2U, 8U) |
      Field(1U, 4U);
  if (hdmi_port == 1U) {
    result.phy_channel_swap = Field(2U, 12U) | Field(3U, 8U) |
        Field(0U, 4U) | Field(1U, 0U);
  }
  result.phy_pll_calibration1 = 0U;
  result.phy_pll_calibration2 = 0U;
  result.phy_pll_calibration4 = Field(0x0e14U, 16U) | 0xe147U;
  result.rm_control = (1U << 19U) | (1U << 17U) | (1U << 4U);
  result.rm_offset = (1U << 31U) | GetRmOffset(vco_frequency);
  result.rm_format = Field(2U, 24U);

  // Keep the enable bit out of the reset/configuration value. The caller
  // asserts it only after HVS and timing registers are ready.
  result.pixel_valve_control &= ~kPixelValveEnable;
  *plan = result;
  return true;
}

bool ResolveKnownModeTiming(const ModeTimingSignature &signature,
                            ModeRegisterPlan *plan,
                            const char **mode_name) {
  if (plan == nullptr || mode_name == nullptr) {
    return false;
  }
  struct Candidate {
    unsigned group;
    unsigned mode;
    const char *named_mode;
    const char *name;
  };
  static const Candidate candidates[] = {
    {1U, 4U, nullptr, "1280x720@60"},
    {1U, 19U, nullptr, "1280x720@50"},
    {1U, 16U, nullptr, "1920x1080@60"},
    {1U, 31U, nullptr, "1920x1080@50"},
    {0U, 0U, "1280x800@60", "1280x800@60"},
    {0U, 0U, "1280x800@50", "1280x800@50"},
    {0U, 0U, "1920x1200@60", "1920x1200@60"},
    {0U, 0U, "1920x1200@50", "1920x1200@50"}
  };
  for (unsigned i = 0U; i < sizeof candidates / sizeof candidates[0]; ++i) {
    Mode mode = {};
    ModeRegisterPlan candidate_plan = {};
    if (!ResolveBmxMode(candidates[i].group, candidates[i].mode,
                        nullptr, candidates[i].named_mode, &mode) ||
        !BuildModeRegisterPlan(mode, &candidate_plan)) {
      continue;
    }
    if (signature.hdmi_horizontal_a !=
            candidate_plan.hdmi_horizontal_a ||
        signature.hdmi_horizontal_b !=
            candidate_plan.hdmi_horizontal_b ||
        signature.hdmi_vertical_a != candidate_plan.hdmi_vertical_a ||
        signature.hdmi_vertical_b !=
            candidate_plan.hdmi_vertical_b_even ||
        signature.pixel_valve_horizontal_a !=
            candidate_plan.pixel_valve_horizontal_a ||
        signature.pixel_valve_horizontal_b !=
            candidate_plan.pixel_valve_horizontal_b ||
        signature.pixel_valve_vertical_a !=
            candidate_plan.pixel_valve_vertical_a ||
        signature.pixel_valve_vertical_b !=
            candidate_plan.pixel_valve_vertical_b) {
      continue;
    }
    *plan = candidate_plan;
    *mode_name = candidates[i].name;
    return true;
  }
  return false;
}

}  // namespace pi4kms
