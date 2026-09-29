#include "pi5_kms_probe.h"

#include <circle/bcm2835.h>
#include <circle/bcmpropertytags.h>
#include <circle/memio.h>
#include <circle/timer.h>

namespace pi5kms {

namespace {
const char kFrom[] = "pi5kms";

constexpr uintptr kPv0Base = ARM_IO_BASE + 0x410000;
constexpr uintptr kHvsBase = ARM_IO_BASE + 0x580000;
constexpr uintptr kHdmi0HdBase = ARM_IO_BASE + 0x720000;
constexpr uintptr kHdmi0Base = ARM_IO_BASE + 0x701400;
constexpr uintptr kHdmi0DvpBase = ARM_IO_BASE + 0x701000;
constexpr uintptr kHdmi0PhyBase = ARM_IO_BASE + 0x701D00;
constexpr uintptr kHdmi0RmBase = ARM_IO_BASE + 0x702000;

constexpr u32 kPropGetClockState = 0x00030001;

constexpr unsigned kClockCore = 4;
constexpr unsigned kClockPixel = 10;
constexpr unsigned kClockM2mc = 13;
constexpr unsigned kClockPixelBvb = 14;
constexpr unsigned kClockDisp = 16;

constexpr u32 kPvControl = 0x000;
constexpr u32 kPvVControl = 0x004;
constexpr u32 kPvHorza = 0x00C;
constexpr u32 kPvHorzb = 0x010;
constexpr u32 kPvVerta = 0x014;
constexpr u32 kPvVertb = 0x018;
constexpr u32 kPvMuxCfg = 0x034;
constexpr u32 kPvPipeInitCtrl = 0x040;

constexpr u32 kPvControlEnable = 1U << 0;
constexpr u32 kPvControlFifoClear = 1U << 1;
constexpr u32 kPvVControlVideoEnable = 1U << 0;

constexpr u32 kHdVidCtl = 0x044;
constexpr u32 kHdmiFifoCtl = 0x07C;
constexpr u32 kHdmiSchedulerControl = 0x0E8;
constexpr u32 kHdmiHorza = 0x0EC;
constexpr u32 kHdmiHorzb = 0x0F0;
constexpr u32 kHdmiVerta0 = 0x0F4;
constexpr u32 kHdmiVertb0 = 0x0F8;
constexpr u32 kHdmiVerta1 = 0x100;
constexpr u32 kHdmiVertb1 = 0x104;
constexpr u32 kHdmiMiscControl = 0x114;
constexpr u32 kHdmiClockStop = 0x0BC;

constexpr u32 kPhyResetCtl = 0x000;
constexpr u32 kPhyPowerupCtl = 0x004;
constexpr u32 kPhyCtl0 = 0x008;
constexpr u32 kPhyCtl1 = 0x00C;
constexpr u32 kPhyCtl2 = 0x010;
constexpr u32 kPhyCtlCk = 0x014;
constexpr u32 kPhyPllRefclk = 0x01C;
constexpr u32 kPhyPllPostKdiv = 0x028;
constexpr u32 kPhyPllVcoclkDiv = 0x02C;
constexpr u32 kPhyPllCfg = 0x044;
constexpr u32 kPhyTmdsClkWordSel = 0x054;
constexpr u32 kPhyPllMisc0 = 0x060;
constexpr u32 kPhyPllMisc1 = 0x064;
constexpr u32 kPhyPllMisc2 = 0x068;
constexpr u32 kPhyPllMisc3 = 0x06C;
constexpr u32 kPhyPllMisc4 = 0x070;
constexpr u32 kPhyPllMisc5 = 0x074;
constexpr u32 kPhyPllMisc6 = 0x078;
constexpr u32 kPhyPllMisc7 = 0x07C;
constexpr u32 kPhyPllMisc8 = 0x080;
constexpr u32 kPhyPllResetCtl = 0x190;
constexpr u32 kPhyPllPowerupCtl = 0x194;

constexpr u32 kRmOffset = 0x018;

constexpr u32 kHdmiSchedulerManualFormat = 1U << 15;
constexpr u32 kHdmiSchedulerIgnoreVsyncPredicts = 1U << 5;
constexpr u32 kHdmiFifoRecenter = 1U << 6;
constexpr u32 kHdmiFifoValidWriteMask = 0x0000EFFF;

constexpr u32 kPhyPllPostKdivBypass = 1U << 4;
constexpr u32 kPhyPllResetPllResetb = 1U << 0;

constexpr unsigned long long kVc6VcoMin = 8000000000ULL;
constexpr unsigned long long kVc6VcoMax = 12000000000ULL;
constexpr unsigned long long kOscillatorFrequency = 54000000ULL;

struct HdmiMode {
  const char *name;
  const char *source;
  u32 width;
  u32 height;
  u32 refresh_millihz;
  u32 pixel_clock;
  u16 h_front_porch;
  u16 h_sync;
  u16 h_back_porch;
  u16 v_front_porch;
  u16 v_sync;
  u16 v_back_porch;
  bool h_sync_positive;
  bool v_sync_positive;
};

constexpr HdmiMode kModes[] = {
    // CEA 16:9 modes used by BMC64 HDMI profiles.
    {"CEA 1280x720@60 16:9", "hdmi_group=1 hdmi_mode=4", 1280, 720,
     60000, 74250000, 110, 40, 220, 5, 5, 20, true, true},
    {"CEA 1280x720@50 16:9", "hdmi_group=1 hdmi_mode=19", 1280, 720,
     50000, 74250000, 440, 40, 220, 5, 5, 20, true, true},
    {"CEA 1920x1080@60 16:9", "hdmi_group=1 hdmi_mode=16", 1920, 1080,
     60000, 148500000, 88, 44, 148, 4, 5, 36, true, true},
    {"CEA 1920x1080@50 16:9", "hdmi_group=1 hdmi_mode=31", 1920, 1080,
     50000, 148500000, 528, 44, 148, 4, 5, 36, true, true},

    // 16:10 monitor modes for native widescreen computer displays.
    {"CVT 1280x800@60 16:10", "generated from cvt 1280 800 60", 1280,
     800, 59810, 83500000, 72, 128, 200, 3, 6, 22, false, true},
    {"CVT 1280x800@50 16:10", "generated from cvt 1280 800 50", 1280,
     800, 49950, 68000000, 56, 128, 184, 3, 6, 17, false, true},
    {"CVT 1920x1200@60 16:10", "generated from cvt 1920 1200 60", 1920,
     1200, 59880, 193250000, 136, 200, 336, 3, 6, 36, false, true},
    {"CVT 1920x1200@50 16:10", "generated from cvt 1920 1200 50", 1920,
     1200, 49930, 158250000, 120, 200, 320, 3, 6, 29, false, true},

    // BMX custom HDMI timings from sdcard/machines.ini. These are
    // machine-specific because BMC64 nudges the pixel clock to match each
    // emulated machine's real cadence.
    {"BMC VIC20 768x525@60.285", "VIC20 NTSC hdmi_timings", 768, 525,
     60285, 31656857, 24, 72, 96, 3, 10, 9, false, true},
    {"BMC C64/C128 768x525@59.827", "C64/C128 NTSC hdmi_timings", 768,
     525, 59827, 31416828, 24, 72, 96, 3, 10, 9, false, true},
    {"BMC PLUS4 768x525@59.923", "PLUS4 NTSC hdmi_timings", 768, 525,
     59923, 31467501, 24, 72, 96, 3, 10, 9, false, true},
    {"BMC PET 768x525@60.060", "PET NTSC hdmi_timings", 768, 525,
     60060, 31538738, 24, 72, 96, 3, 10, 9, false, true},
    {"BMC VIC20 768x545@50.037", "VIC20 PAL hdmi_timings", 768, 545,
     50037, 27043926, 24, 72, 96, 3, 2, 13, false, true},
    {"BMC C64/C128 768x545@50.125", "C64/C128 PAL hdmi_timings", 768,
     545, 50125, 27091697, 24, 72, 96, 3, 2, 13, false, true},
    {"BMC PLUS4 768x545@49.860", "PLUS4 PAL hdmi_timings", 768, 545,
     49860, 26948856, 24, 72, 96, 3, 2, 13, false, true},
    {"BMC PET 768x545@49.875", "PET PAL hdmi_timings", 768, 545,
     49875, 26956913, 24, 72, 96, 3, 2, 13, false, true},
};

constexpr unsigned kModeCount = sizeof(kModes) / sizeof(kModes[0]);

struct TPropertyTagClockState {
  TPropertyTag Tag;
  u32 nClockId;
  u32 nState;
} PACKED;

u32 MakeHdmiHorza(const HdmiMode &mode) {
  const u32 sync_flags = (mode.h_sync_positive ? (1U << 15) : 0) |
                         (mode.v_sync_positive ? (1U << 14) : 0);
  return ((u32)mode.h_front_porch << 16) | sync_flags | mode.width;
}

u32 MakeHdmiHorzb(const HdmiMode &mode) {
  return ((u32)mode.h_back_porch << 16) | mode.h_sync;
}

u32 MakeHdmiVerta(const HdmiMode &mode) {
  return ((u32)mode.v_sync << 24) | ((u32)mode.v_front_porch << 16) |
         mode.height;
}

u32 MakeHdmiVertb(const HdmiMode &mode) {
  return mode.v_back_porch;
}

u32 MakePvHorza(const HdmiMode &mode) {
  return ((u32)(mode.h_back_porch / 2) << 16) | (mode.h_sync / 2);
}

u32 MakePvHorzb(const HdmiMode &mode) {
  return ((u32)(mode.h_front_porch / 2) << 16) | (mode.width / 2);
}

u32 MakePvVerta(const HdmiMode &mode) {
  return ((u32)mode.v_back_porch << 16) | mode.v_sync;
}

u32 MakePvVertb(const HdmiMode &mode) {
  return ((u32)mode.v_front_porch << 16) | mode.height;
}

u32 GetHsmClock(u32 tmds_rate) {
  const unsigned long long hsm =
      ((unsigned long long)tmds_rate * 101ULL + 99ULL) / 100ULL;
  return hsm < 120000000ULL ? 120000000 : (u32)hsm;
}

u32 GetBvbClock(u32 tmds_rate) {
  if (tmds_rate > 297000000) {
    return 300000000;
  }
  if (tmds_rate > 148500000) {
    return 150000000;
  }
  return 75000000;
}

unsigned long long GetVc6VcoFreq(u32 tmds_rate, u32 *vco_div) {
  u32 div = 0;
  while ((unsigned long long)tmds_rate * div * 10ULL < kVc6VcoMin) {
    ++div;
  }
  const u32 min_div = div;

  while ((unsigned long long)tmds_rate * (div + 1U) * 10ULL < kVc6VcoMax) {
    ++div;
  }
  const u32 max_div = div;

  div = min_div + (max_div - min_div) / 2U;
  *vco_div = div;
  return (unsigned long long)tmds_rate * div * 10ULL;
}

u32 GetRmOffset(unsigned long long vco_freq) {
  unsigned long long offset = vco_freq * 2ULL;
  offset <<= 22;
  offset /= kOscillatorFrequency;
  offset >>= 2;
  return (u32)offset;
}
}  // namespace

u32 Probe::ReadReg(uintptr base, u32 offset) {
  return read32(base + offset);
}

void Probe::WriteReg(uintptr base, u32 offset, u32 value) {
  write32(base + offset, value);
}

bool Probe::GetClockRate(unsigned clock_id, u32 *rate) {
  CBcmPropertyTags tags;
  TPropertyTagClockRate tag;
  tag.nClockId = clock_id;
  tag.nRate = 0;
  if (!tags.GetTag(PROPTAG_GET_CLOCK_RATE, &tag, sizeof tag, 4)) {
    return false;
  }
  *rate = tag.nRate;
  return true;
}

bool Probe::SetClockRate(unsigned clock_id, u32 rate) {
  CBcmPropertyTags tags;
  TPropertyTagSetClockRate tag;
  tag.nClockId = clock_id;
  tag.nRate = rate;
  tag.nSkipSettingTurbo = 0;
  return tags.GetTag(PROPTAG_SET_CLOCK_RATE, &tag, sizeof tag, 12);
}

bool Probe::GetDisplayDimensions(u32 *width, u32 *height) {
  CBcmPropertyTags tags;
  TPropertyTagDisplayDimensions tag;
  tag.nWidth = 0;
  tag.nHeight = 0;
  if (!tags.GetTag(PROPTAG_GET_DISPLAY_DIMENSIONS, &tag, sizeof tag)) {
    return false;
  }
  *width = tag.nWidth;
  *height = tag.nHeight;
  return true;
}

void Probe::DumpClock(CLogger &logger, const char *name, unsigned clock_id) {
  u32 rate = 0;
  CBcmPropertyTags tags;
  TPropertyTagClockState state;
  state.nClockId = clock_id;
  state.nState = 0;
  bool have_state = tags.GetTag(kPropGetClockState, &state, sizeof state, 4);

  if (GetClockRate(clock_id, &rate)) {
    logger.Write(kFrom, LogNotice, "clock %-9s id %u rate %u state %s%08X",
                 name, clock_id, rate, have_state ? "0x" : "?", state.nState);
  } else {
    logger.Write(kFrom, LogError, "clock %-9s id %u rate read failed",
                 name, clock_id);
  }
}

void Probe::DumpReg(CLogger &logger, const char *block, uintptr base,
                    const char *name, u32 offset) {
  const u32 value = ReadReg(base, offset);
  logger.Write(kFrom, LogNotice, "%s %-16s +0x%03X = 0x%08X",
               block, name, offset, value);
}

void Probe::SetClockAndLog(CLogger &logger, const char *name,
                           unsigned clock_id, u32 rate) {
  logger.Write(kFrom, LogNotice, "set clock %-9s id %u -> %u",
               name, clock_id, rate);

  if (!SetClockRate(clock_id, rate)) {
    logger.Write(kFrom, LogError, "set clock %-9s failed", name);
  }

  DumpClock(logger, name, clock_id);
}

void Probe::InitVc6Phy(CLogger &logger, u32 tmds_rate) {
  u32 vco_div = 0;
  const unsigned long long vco_freq = GetVc6VcoFreq(tmds_rate, &vco_div);
  const u32 rm_offset = 0x80000000U | GetRmOffset(vco_freq);
  const u32 vco_mhz = (u32)(vco_freq / 1000000ULL);
  const u32 vco_khz_frac = (u32)((vco_freq / 1000ULL) % 1000ULL);

  logger.Write(kFrom, LogNotice, "init VC6 HDMI PHY for TMDS %u", tmds_rate);
  logger.Write(kFrom, LogNotice, "VC6 PHY vco=%u.%03u MHz div=%u rm=0x%08X",
               vco_mhz, vco_khz_frac, vco_div, rm_offset);

  if (tmds_rate > 222000000) {
    logger.Write(kFrom, LogWarning,
                 "TMDS rate above validated 222 MHz lane table; using max "
                 "known setting");
  }

  WriteReg(kHdmi0PhyBase, kPhyResetCtl, 0);
  WriteReg(kHdmi0PhyBase, kPhyPowerupCtl, 0);
  WriteReg(kHdmi0PhyBase, kPhyPllPostKdiv, kPhyPllPostKdivBypass);

  WriteReg(kHdmi0PhyBase, kPhyPllMisc0, 0x810C6000);
  WriteReg(kHdmi0PhyBase, kPhyPllMisc1, 0x00B8C451);
  WriteReg(kHdmi0PhyBase, kPhyPllMisc2, 0x46402E31);
  WriteReg(kHdmi0PhyBase, kPhyPllMisc3, 0x00B8C005);
  WriteReg(kHdmi0PhyBase, kPhyPllMisc4, 0x42410261);
  WriteReg(kHdmi0PhyBase, kPhyPllMisc5, 0xCC021001);
  WriteReg(kHdmi0PhyBase, kPhyPllMisc6, 0xC8301C80);
  WriteReg(kHdmi0PhyBase, kPhyPllMisc7, 0xB0804444);
  WriteReg(kHdmi0PhyBase, kPhyPllMisc8, 0xF80F8000);

  WriteReg(kHdmi0PhyBase, kPhyPllRefclk, 0x00002036);
  WriteReg(kHdmi0PhyBase, kPhyResetCtl, 0x0000007F);
  WriteReg(kHdmi0RmBase, kRmOffset, rm_offset);
  WriteReg(kHdmi0PhyBase, kPhyPllVcoclkDiv, 0x00000400 | vco_div);
  WriteReg(kHdmi0PhyBase, kPhyPllCfg, 0);
  WriteReg(kHdmi0PhyBase, kPhyPllPostKdiv, 0x00000009);

  // Linux vc6_hdmi_phy_settings[0] for TMDS <= 222 MHz, lane mapping 0/1/2/CK.
  WriteReg(kHdmi0PhyBase, kPhyCtl0, 0x80828700);
  WriteReg(kHdmi0PhyBase, kPhyCtl1, 0x80828700);
  WriteReg(kHdmi0PhyBase, kPhyCtl2, 0x80828700);
  WriteReg(kHdmi0PhyBase, kPhyCtlCk, 0x80828700);
  WriteReg(kHdmi0PhyBase, kPhyTmdsClkWordSel, 0);

  WriteReg(kHdmi0PhyBase, kPhyPowerupCtl, 0x000001CF);
  WriteReg(kHdmi0PhyBase, kPhyPllPowerupCtl, 0x00000001);
  WriteReg(kHdmi0PhyBase, kPhyPllResetCtl,
           ReadReg(kHdmi0PhyBase, kPhyPllResetCtl) & ~kPhyPllResetPllResetb);
  WriteReg(kHdmi0PhyBase, kPhyPllResetCtl,
           ReadReg(kHdmi0PhyBase, kPhyPllResetCtl) | kPhyPllResetPllResetb);

  DumpReg(logger, "PHY0", kHdmi0PhyBase, "RESET_CTL", kPhyResetCtl);
  DumpReg(logger, "PHY0", kHdmi0PhyBase, "POWERUP_CTL", kPhyPowerupCtl);
  DumpReg(logger, "PHY0", kHdmi0PhyBase, "CTL0", kPhyCtl0);
  DumpReg(logger, "PHY0", kHdmi0PhyBase, "CTL1", kPhyCtl1);
  DumpReg(logger, "PHY0", kHdmi0PhyBase, "CTL2", kPhyCtl2);
  DumpReg(logger, "PHY0", kHdmi0PhyBase, "CTL_CK", kPhyCtlCk);
  DumpReg(logger, "PHY0", kHdmi0PhyBase, "PLL_REFCLK", kPhyPllRefclk);
  DumpReg(logger, "PHY0", kHdmi0PhyBase, "PLL_POST_KDIV", kPhyPllPostKdiv);
  DumpReg(logger, "PHY0", kHdmi0PhyBase, "PLL_VCOCLK_DIV", kPhyPllVcoclkDiv);
  DumpReg(logger, "PHY0", kHdmi0PhyBase, "PLL_CFG", kPhyPllCfg);
  DumpReg(logger, "PHY0", kHdmi0PhyBase, "TMDS_WORD_SEL", kPhyTmdsClkWordSel);
  DumpReg(logger, "PHY0", kHdmi0PhyBase, "PLL_RESET_CTL", kPhyPllResetCtl);
  DumpReg(logger, "PHY0", kHdmi0PhyBase, "PLL_POWERUP_CTL", kPhyPllPowerupCtl);
  DumpReg(logger, "RM0", kHdmi0RmBase, "RM_OFFSET", kRmOffset);
}

void Probe::Dump(CLogger &logger) const {
  u32 width = 0;
  u32 height = 0;

  logger.Write(kFrom, LogNotice, "Pi5 KMS probe start");
  logger.Write(kFrom, LogNotice, "ARM_IO_BASE high=0x%08X low=0x%08X",
               (u32)(ARM_IO_BASE >> 32), (u32)ARM_IO_BASE);

  if (GetDisplayDimensions(&width, &height)) {
    logger.Write(kFrom, LogNotice, "firmware display dimensions %ux%u",
                 width, height);
  } else {
    logger.Write(kFrom, LogError, "firmware display dimensions read failed");
  }

  DumpClock(logger, "core", kClockCore);
  DumpClock(logger, "pixel", kClockPixel);
  DumpClock(logger, "m2mc/hdmi", kClockM2mc);
  DumpClock(logger, "pixel-bvb", kClockPixelBvb);
  DumpClock(logger, "disp", kClockDisp);

  DumpReg(logger, "PV0", kPv0Base, "CONTROL", 0x000);
  DumpReg(logger, "PV0", kPv0Base, "V_CONTROL", 0x004);
  DumpReg(logger, "PV0", kPv0Base, "HORZA", 0x00C);
  DumpReg(logger, "PV0", kPv0Base, "HORZB", 0x010);
  DumpReg(logger, "PV0", kPv0Base, "VERTA", 0x014);
  DumpReg(logger, "PV0", kPv0Base, "VERTB", 0x018);
  DumpReg(logger, "PV0", kPv0Base, "STAT", 0x02C);
  DumpReg(logger, "PV0", kPv0Base, "MUX_CFG", 0x034);
  DumpReg(logger, "PV0", kPv0Base, "PIPE_INIT_CTRL", 0x040);

  DumpReg(logger, "HVS", kHvsBase, "SCALER6_CONTROL", 0x020);
  DumpReg(logger, "HVS", kHvsBase, "DISP0_CTRL0", 0x030);
  DumpReg(logger, "HVS", kHvsBase, "DISP0_CTRL1", 0x034);
  DumpReg(logger, "HVS", kHvsBase, "DISP0_LPTRS", 0x03C);
  DumpReg(logger, "HVS", kHvsBase, "DISP0_STATUS", 0x044);

  DumpReg(logger, "HD", kHdmi0HdBase, "VID_CTL", 0x044);
  DumpReg(logger, "HD", kHdmi0HdBase, "FRAME_COUNT", 0x060);

  DumpReg(logger, "HDMI0", kHdmi0Base, "FIFO_CTL", 0x07C);
  DumpReg(logger, "HDMI0", kHdmi0Base, "SCHED_CTL", 0x0E8);
  DumpReg(logger, "HDMI0", kHdmi0Base, "HORZA", 0x0EC);
  DumpReg(logger, "HDMI0", kHdmi0Base, "HORZB", 0x0F0);
  DumpReg(logger, "HDMI0", kHdmi0Base, "VERTA0", 0x0F4);
  DumpReg(logger, "HDMI0", kHdmi0Base, "VERTB0", 0x0F8);
  DumpReg(logger, "HDMI0", kHdmi0Base, "MISC_CONTROL", 0x114);
  DumpReg(logger, "HDMI0", kHdmi0Base, "HOTPLUG", 0x1C8);
  DumpReg(logger, "HDMI0", kHdmi0Base, "SCRAMBLER_CTL", 0x1E4);

  DumpReg(logger, "DVP0", kHdmi0DvpBase, "CLOCK_STOP", 0x0BC);
  DumpReg(logger, "PHY0", kHdmi0PhyBase, "RESET_CTL", 0x000);
  DumpReg(logger, "PHY0", kHdmi0PhyBase, "POWERUP_CTL", 0x004);
  DumpReg(logger, "PHY0", kHdmi0PhyBase, "PLL_REFCLK", 0x01C);
  DumpReg(logger, "PHY0", kHdmi0PhyBase, "PLL_POST_KDIV", 0x028);
  DumpReg(logger, "PHY0", kHdmi0PhyBase, "PLL_VCOCLK_DIV", 0x02C);
  DumpReg(logger, "PHY0", kHdmi0PhyBase, "PLL_CFG", 0x044);
  DumpReg(logger, "RM0", kHdmi0RmBase, "RM_CONTROL", 0x000);
  DumpReg(logger, "RM0", kHdmi0RmBase, "RM_OFFSET", 0x018);

  logger.Write(kFrom, LogNotice, "Pi5 KMS probe complete");
}

void Probe::SetMode(CLogger &logger, unsigned mode_index) {
  if (mode_index >= kModeCount) {
    logger.Write(kFrom, LogError, "invalid mode index %u", mode_index);
    return;
  }

  const HdmiMode &mode = kModes[mode_index];
  const u32 h_total =
      mode.width + mode.h_front_porch + mode.h_sync + mode.h_back_porch;
  const u32 v_total =
      mode.height + mode.v_front_porch + mode.v_sync + mode.v_back_porch;
  const u32 hsm_clock = GetHsmClock(mode.pixel_clock);
  const u32 bvb_clock = GetBvbClock(mode.pixel_clock);

  logger.Write(kFrom, LogNotice, "Pi5 HDMI mode %u/%u: %s", mode_index + 1,
               kModeCount, mode.name);
  logger.Write(kFrom, LogNotice, "source %s", mode.source);
  logger.Write(kFrom, LogNotice,
               "target HDMI %ux%u refresh %u.%03u total %ux%u pclk %u",
               mode.width, mode.height, mode.refresh_millihz / 1000,
               mode.refresh_millihz % 1000, h_total, v_total,
               mode.pixel_clock);
  logger.Write(kFrom, LogNotice,
               "target PV horizontal uses half HDMI counts");

  const u32 pv_control_before = ReadReg(kPv0Base, kPvControl);
  logger.Write(kFrom, LogNotice, "disable PV0 control before 0x%08X",
               pv_control_before);
  WriteReg(kPv0Base, kPvControl, pv_control_before & ~kPvControlEnable);
  WriteReg(kPv0Base, kPvControl,
           (pv_control_before & ~kPvControlEnable) | kPvControlFifoClear);

  // Linux uses a 1% HSM margin, minimum 120 MHz. Pixel-BVB is 75 MHz up
  // to 148.5 MHz TMDS, 150 MHz above that, and 300 MHz above 297 MHz.
  SetClockAndLog(logger, "pixel", kClockPixel, mode.pixel_clock);
  SetClockAndLog(logger, "m2mc/hdmi", kClockM2mc, hsm_clock);
  SetClockAndLog(logger, "pixel-bvb", kClockPixelBvb, bvb_clock);
  SetClockAndLog(logger, "disp", kClockDisp, hsm_clock);

  InitVc6Phy(logger, mode.pixel_clock);

  logger.Write(kFrom, LogNotice, "program DVP clock stop");
  WriteReg(kHdmi0DvpBase, kHdmiClockStop, 0);

  logger.Write(kFrom, LogNotice, "program HDMI0 timings");
  WriteReg(kHdmi0Base, kHdmiSchedulerControl,
           ReadReg(kHdmi0Base, kHdmiSchedulerControl) |
               kHdmiSchedulerManualFormat |
               kHdmiSchedulerIgnoreVsyncPredicts);
  WriteReg(kHdmi0Base, kHdmiHorza, MakeHdmiHorza(mode));
  WriteReg(kHdmi0Base, kHdmiHorzb, MakeHdmiHorzb(mode));
  WriteReg(kHdmi0Base, kHdmiVerta0, MakeHdmiVerta(mode));
  WriteReg(kHdmi0Base, kHdmiVertb0, MakeHdmiVertb(mode));
  WriteReg(kHdmi0Base, kHdmiVerta1, MakeHdmiVerta(mode));
  WriteReg(kHdmi0Base, kHdmiVertb1, MakeHdmiVertb(mode));
  WriteReg(kHdmi0Base, kHdmiMiscControl,
           ReadReg(kHdmi0Base, kHdmiMiscControl) & ~0xFU);

  logger.Write(kFrom, LogNotice, "program PV0 timings");
  WriteReg(kPv0Base, kPvHorza, MakePvHorza(mode));
  WriteReg(kPv0Base, kPvHorzb, MakePvHorzb(mode));
  WriteReg(kPv0Base, kPvVerta, MakePvVerta(mode));
  WriteReg(kPv0Base, kPvVertb, MakePvVertb(mode));
  WriteReg(kPv0Base, kPvVControl,
           ReadReg(kPv0Base, kPvVControl) | kPvVControlVideoEnable);
  WriteReg(kPv0Base, kPvMuxCfg, 0x00000020);
  WriteReg(kPv0Base, kPvPipeInitCtrl, 0x00030002);

  logger.Write(kFrom, LogNotice, "recenter HDMI FIFO");
  const u32 fifo = ReadReg(kHdmi0Base, kHdmiFifoCtl) & kHdmiFifoValidWriteMask;
  WriteReg(kHdmi0Base, kHdmiFifoCtl, fifo & ~kHdmiFifoRecenter);
  WriteReg(kHdmi0Base, kHdmiFifoCtl, fifo | kHdmiFifoRecenter);

  logger.Write(kFrom, LogNotice, "enable PV0");
  WriteReg(kPv0Base, kPvControl, (pv_control_before | kPvControlEnable) &
                                     ~kPvControlFifoClear);

  logger.Write(kFrom, LogNotice, "mode registers after write");
  DumpReg(logger, "PV0", kPv0Base, "CONTROL", kPvControl);
  DumpReg(logger, "PV0", kPv0Base, "V_CONTROL", kPvVControl);
  DumpReg(logger, "PV0", kPv0Base, "HORZA", kPvHorza);
  DumpReg(logger, "PV0", kPv0Base, "HORZB", kPvHorzb);
  DumpReg(logger, "PV0", kPv0Base, "VERTA", kPvVerta);
  DumpReg(logger, "PV0", kPv0Base, "VERTB", kPvVertb);
  DumpReg(logger, "HD", kHdmi0HdBase, "VID_CTL", kHdVidCtl);
  DumpReg(logger, "HDMI0", kHdmi0Base, "SCHED_CTL", kHdmiSchedulerControl);
  DumpReg(logger, "HDMI0", kHdmi0Base, "HORZA", kHdmiHorza);
  DumpReg(logger, "HDMI0", kHdmi0Base, "HORZB", kHdmiHorzb);
  DumpReg(logger, "HDMI0", kHdmi0Base, "VERTA0", kHdmiVerta0);
  DumpReg(logger, "HDMI0", kHdmi0Base, "VERTB0", kHdmiVertb0);
  DumpReg(logger, "HDMI0", kHdmi0Base, "MISC_CONTROL", kHdmiMiscControl);
  DumpReg(logger, "DVP0", kHdmi0DvpBase, "CLOCK_STOP", kHdmiClockStop);

  logger.Write(kFrom, LogNotice, "Pi5 HDMI mode complete: %s", mode.name);
}

void Probe::RunModeSweep(CLogger &logger, unsigned hold_ms) const {
  logger.Write(kFrom, LogNotice, "Pi5 HDMI mode sweep start, %u modes, hold %u ms",
               kModeCount, hold_ms);

  for (unsigned i = 0; i < kModeCount; ++i) {
    SetMode(logger, i);
    logger.Write(kFrom, LogNotice, "holding mode %u/%u for %u ms", i + 1,
                 kModeCount, hold_ms);
    CTimer::Get()->MsDelay(hold_ms);
  }

  logger.Write(kFrom, LogNotice, "Pi5 HDMI mode sweep complete");
}

}  // namespace pi5kms
