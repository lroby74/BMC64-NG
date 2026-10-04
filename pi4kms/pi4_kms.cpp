#include "pi4kms/pi4_kms.h"

#include "pi4kms/pi4_kms_dlist.h"
#include "pi4kms/pi4_kms_lifecycle.h"
#include "pi4kms/pi4_kms_mode.h"
#include "pi4kms/pi4_kms_probe.h"

#include <circle/bcm2835.h>
#include <circle/bcmpropertytags.h>
#include <circle/memio.h>
#include <circle/synchronize.h>
#include <circle/timer.h>

#include <stdio.h>
#include <string.h>

namespace pi4kms {

namespace {

const uintptr kHvsBase = ARM_IO_BASE + 0x400000U;
const uintptr kPixelValve2Base = ARM_IO_BASE + 0x20a000U;
const uintptr kPixelValve4Base = ARM_IO_BASE + 0x216000U;
const uintptr kHdmi0DvpGlobalBase = ARM_IO_BASE + 0xf00000U;



const uintptr kPixelValve3Base = ARM_IO_BASE + 0xc12000U;
const uintptr kVecBase = ARM_IO_BASE + 0xc13000U;
const uint32_t kHvsControlEnable = 1U << 31U;
bool g_requested_composite = false;
uint32_t g_composite_reasserts = 0U;





bool g_composite_progressivo = false;
int g_composite_variante = -1;
uint32_t g_composite_righe = 0U;
uint32_t g_composite_fw_control = 0U;
uint32_t g_composite_fw_background = 0U;
uint32_t g_composite_fw_pending = 0U;
uint32_t g_composite_control = 0U;
uint32_t g_composite_background = 0U;
uint32_t g_composite_canale_rimesso = 0U;



const unsigned kCompositeRiprese = 6U;
uint64_t g_composite_storia = 0U;
bool g_composite_arreso = false;









struct HdmiPortRegisters {
  uintptr core;
  uintptr dvp;
  uintptr phy;
  uintptr rm;
  uintptr packet_ram;
  uintptr csc;
  uint32_t hd_video_control;
  uint32_t hd_frame_count;
  uint32_t hd_mai_format;
  uint32_t software_init_bit;
  uint32_t hvs_mux;
};

const HdmiPortRegisters kHdmiPorts[2] = {
  {ARM_HDMI_BASE, ARM_IO_BASE + 0xf00300U, ARM_PHY_BASE,
   ARM_IO_BASE + 0xf00f80U, ARM_IO_BASE + 0xf01b00U, ARM_IO_BASE + 0xf00200U,
   0x0044U, 0x0060U, 0x0018U, 1U << 0U, 0x0018U},
  {ARM_IO_BASE + 0xf05700U, ARM_IO_BASE + 0xf05300U, ARM_IO_BASE + 0xf05f00U,
   ARM_IO_BASE + 0xf05f80U, ARM_IO_BASE + 0xf06b00U, ARM_IO_BASE + 0xf00280U,
   0x0048U, 0x0064U, 0x0038U, 1U << 1U, 0x0014U},
};



const HdmiPortRegisters *g_port = &kHdmiPorts[0];
unsigned g_requested_port = 0U;


const uint32_t kHvsControl = 0x0000U;
const uint32_t kHvsStatus = 0x0004U;
const uint32_t kHvsIdentity = 0x0008U;
const uint32_t kHvsOutput2Mux = 0x000cU;
const uint32_t kHvsOutput5Mux = 0x0014U;
const uint32_t kHvsOutput4Mux = 0x0018U;
const uint32_t kHvsPendingDlist0 = 0x0020U;
const uint32_t kHvsActiveDlist0 = 0x0030U;
const uint32_t kHvsChannelControl0 = 0x0040U;
const uint32_t kHvsChannelBackground0 = 0x0044U;
const uint32_t kHvsChannelStatus0 = 0x0048U;
const uint32_t kHvsChannelFifoBase0 = 0x004cU;
const uint32_t kHvsChannelStride = 0x0010U;
const uint32_t kHvs5ChannelEnable = 1U << 31U;
const uint32_t kHvs5ChannelReset = 1U << 30U;
const uint32_t kHvs5ChannelWidthShift = 16U;
const uint32_t kHvs5ChannelDimensionMask = 0x1fffU;
const uint32_t kHvs5BackgroundBackToBack = 1U << 31U;
const uint32_t kHvsBackgroundInterlace = 1U << 30U;
const uint32_t kHvsBackgroundGamma = 1U << 29U;
const uint32_t kHvs5DlistStart = 0x4000U;
// Keep ARM-owned lists well separated from the captured firmware lists and
// leave headroom at both ends of HVS5 dlist SRAM.
const uint32_t kTakeoverDlistSlots[2] = {3072U, 3136U};
const uint32_t kDlistSwitchTimeoutUs = 50000U;
const uint32_t kDlistWriteAttempts = 3U;
const uint32_t kDlistReadAttempts = 3U;
const uint32_t kFirmwareNotifyDisplayDone = 0x00030066U;

const uint32_t kPixelValveControl = 0x0000U;
const uint32_t kPixelValveVerticalControl = 0x0004U;
const uint32_t kPixelValveEnable = 1U << 0U;
const uint32_t kPixelValveFifoClear = 1U << 1U;
const uint32_t kPixelValveVideoEnable = 1U << 0U;
const uint32_t kPixelValveHorizontalA = 0x000cU;
const uint32_t kPixelValveHorizontalB = 0x0010U;
const uint32_t kPixelValveVerticalA = 0x0014U;
const uint32_t kPixelValveVerticalB = 0x0018U;
const uint32_t kPixelValveInterruptStatus = 0x0028U;
const uint32_t kPixelValveStatus = 0x002cU;
const uint32_t kPixelValveMuxConfig = 0x0034U;

const uint32_t kHdmiFifoControl = 0x0074U;
const uint32_t kHdmiFifoRecenter = 1U << 6U;
const uint32_t kHdmiFifoRecenterDone = 1U << 14U;
const uint32_t kHdmiFifoValidWriteMask = 0x0000efffU;
const uint32_t kHdmiRamPacketConfig = 0x00bcU;
const uint32_t kHdmiRamPacketStatus = 0x00c4U;
const uint32_t kHdmiRamPacketEnable = 1U << 16U;
const uint32_t kHdmiIngressStatus0 = 0x0078U;
const uint32_t kHdmiIngressStatus1 = 0x00a4U;
const uint32_t kHdmiIngressStatus2 = 0x0110U;
const uint32_t kHdmiFormatDetect7 = 0x014cU;
const uint32_t kHdmiSchedulerControl = 0x00e0U;
const uint32_t kHdmiSchedulerManualFormat = 1U << 15U;
const uint32_t kHdmiSchedulerIgnoreVsyncPredicts = 1U << 5U;
const uint32_t kHdmiSchedulerHdmiActive = 1U << 1U;
const uint32_t kHdmiSchedulerModeHdmi = 1U << 0U;
const uint32_t kHdmiHorizontalA = 0x00e4U;
const uint32_t kHdmiHorizontalB = 0x00e8U;
const uint32_t kHdmiVerticalA0 = 0x00ecU;
const uint32_t kHdmiVerticalB0 = 0x00f0U;
const uint32_t kHdmiVerticalA1 = 0x00f4U;
const uint32_t kHdmiVerticalB1 = 0x00f8U;
const uint32_t kHdmiMiscControl = 0x0100U;
const uint32_t kHdmiDeepColorConfig1 = 0x0170U;
const uint32_t kHdmiGcpConfig = 0x0178U;
const uint32_t kHdmiGcpWord1 = 0x017cU;
const uint32_t kHdmiGcpEnable = 1U << 31U;
const uint32_t kHdmiGcpSubpacketByte0Mask = 0xffU;
const uint32_t kHdmiGcpClearAvMute = 1U << 4U;
const uint32_t kHdmiDvpClockStop = 0x00bcU;
const uint32_t kHdmiDvpStatus = 0x00e8U;
const uint32_t kHdmiDvpInterfaceConfig = 0x00ecU;
const uint32_t kHdmiDvpInterfaceXbar = 0x00f0U;
const uint32_t kHdmiDvpSoftwareInit = 0x0004U;
const uint32_t kHdmiDvpMiscConfig = 0x0008U;
const uint32_t kHdmiDvpClockStopPixel = 1U << 1U;
const uint32_t kHdmiHdDvpControl = 0x0000U;
const uint32_t kHdmiHdVideoEnable = 1U << 31U;
const uint32_t kHdmiHdVideoUnderflowEnable = 1U << 30U;
const uint32_t kHdmiHdVideoFrameCounterReset = 1U << 29U;
const uint32_t kHdmiHdVideoVsyncLow = 1U << 28U;
const uint32_t kHdmiHdVideoHsyncLow = 1U << 27U;
const uint32_t kHdmiHdVideoClearRgb = 1U << 23U;
const uint32_t kHdmiHdVideoBlankPixel = 1U << 18U;
const uint32_t kHdmiHdVideoBlankInsertEnable = 1U << 16U;
const uint32_t kHdmiCscControl = 0x0000U;
const uint32_t kHdmiCsc12_11 = 0x0004U;
const uint32_t kHdmiCsc14_13 = 0x0008U;
const uint32_t kHdmiCsc22_21 = 0x000cU;
const uint32_t kHdmiCsc24_23 = 0x0010U;
const uint32_t kHdmiCsc32_31 = 0x0014U;
const uint32_t kHdmiCsc34_33 = 0x0018U;
const uint32_t kHdmiCscChannelControl = 0x002cU;
const uint32_t kHdmiPhyResetControl = 0x0000U;
const uint32_t kHdmiPhyPllResetMask = (1U << 5U) | (1U << 4U);
const uint32_t kHdmiPhyLaneResetMask = 0x0000000fU;
const uint32_t kHdmiPhyPowerdownControl = 0x0004U;
const uint32_t kHdmiPhyPowerdownLaneMask = 1U << 10U;
const uint32_t kHdmiPhyPowerdownRandomGenerator = 1U << 4U;
const uint32_t kHdmiPhyControl0 = 0x0008U;
const uint32_t kHdmiPhyControl1 = 0x000cU;
const uint32_t kHdmiPhyControl2 = 0x0010U;
const uint32_t kHdmiPhyControl3 = 0x0014U;
const uint32_t kHdmiPhyPllControl0 = 0x001cU;
const uint32_t kHdmiPhyPllControl1 = 0x0020U;
const uint32_t kHdmiPhyClockDivider = 0x0028U;
const uint32_t kHdmiPhyPllConfig = 0x0034U;
const uint32_t kHdmiPhyTmdsClockWordSelect = 0x0044U;
const uint32_t kHdmiPhyStatus0 = 0x0048U;
const uint32_t kHdmiPhyChannelSwap = 0x004cU;
const uint32_t kHdmiPhyPllCalibration1 = 0x0050U;
const uint32_t kHdmiPhyPllCalibration2 = 0x0054U;
const uint32_t kHdmiPhyPllCalibration4 = 0x005cU;
const uint32_t kHdmiPhyStatus1 = 0x0064U;
const uint32_t kHdmiRmControl = 0x0000U;
const uint32_t kHdmiRmOffset = 0x0018U;
const uint32_t kHdmiRmFormat = 0x001cU;

const uint32_t kFirmwareClockPixel = 9U;
const uint32_t kFirmwareClockM2mc = 13U;
const uint32_t kFirmwareClockPixelBvb = 14U;
const uint32_t kFirmwareClockDisplay = 16U;
const uint32_t kHdmiPacketRamWords = 0x0200U / sizeof(uint32_t);

ProbeSnapshot g_probe_snapshot = {};
ProbeInfo g_probe_info = {};
bool g_probe_usable = false;
bool g_takeover_requested = false;
bool g_takeover_active = false;
bool g_takeover_failed = false;
uint32_t g_present_sequence = 0U;
RecoveryLifecycle g_recovery_lifecycle;
uint32_t g_submitted_slot = 0U;
bool g_firmware_dlist_saved = false;
uint32_t g_firmware_dlist_pointer = 0U;
uint32_t g_firmware_dlist_word_count = 0U;
uint32_t g_firmware_dlist_words[kProbeDlistWordCapacity] = {};
uint32_t g_firmware_channel_control = 0U;
uint32_t g_firmware_channel_background = 0U;
uint32_t g_firmware_pixel_valve_control = 0U;
uint32_t g_firmware_pixel_valve_vertical_control = 0U;
bool g_firmware_packet_ram_saved = false;
uint32_t g_firmware_packet_ram[kHdmiPacketRamWords] = {};
bool g_firmware_clock_rates_saved = false;
uint32_t g_firmware_pixel_clock = 0U;
uint32_t g_firmware_m2mc_clock = 0U;
uint32_t g_firmware_pixel_bvb_clock = 0U;

struct DisplayPipelineSnapshot {
  bool valid;
  uint32_t hvs_control;
  uint32_t hvs_output4_mux;
  uint32_t pixel_valve_control;
  uint32_t pixel_valve_vertical_control;
  uint32_t pixel_valve_horizontal_a;
  uint32_t pixel_valve_horizontal_b;
  uint32_t pixel_valve_vertical_a;
  uint32_t pixel_valve_vertical_b;
  uint32_t pixel_valve_mux_config;
  uint32_t hd_dvp_control;
  uint32_t hd_mai_format;
  uint32_t hd_video_control;
  uint32_t hd_frame_count;
  uint32_t hdmi_fifo_control;
  uint32_t hdmi_ram_packet_config;
  uint32_t hdmi_ram_packet_status;
  uint32_t hdmi_ingress_status0;
  uint32_t hdmi_ingress_status1;
  uint32_t hdmi_ingress_status2;
  uint32_t hdmi_format_detect7;
  uint32_t hdmi_scheduler_control;
  uint32_t hdmi_horizontal_a;
  uint32_t hdmi_horizontal_b;
  uint32_t hdmi_vertical_a0;
  uint32_t hdmi_vertical_b0;
  uint32_t hdmi_vertical_a1;
  uint32_t hdmi_vertical_b1;
  uint32_t hdmi_misc_control;
  uint32_t hdmi_deep_color_config1;
  uint32_t hdmi_gcp_config;
  uint32_t hdmi_gcp_word1;
  uint32_t dvp_clock_stop;
  uint32_t dvp_status;
  uint32_t dvp_interface_config;
  uint32_t dvp_interface_xbar;
  uint32_t dvp_software_init;
  uint32_t dvp_misc_config;
  uint32_t csc_control;
  uint32_t csc_12_11;
  uint32_t csc_14_13;
  uint32_t csc_22_21;
  uint32_t csc_24_23;
  uint32_t csc_32_31;
  uint32_t csc_34_33;
  uint32_t csc_channel_control;
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
  uint32_t phy_status0;
  uint32_t phy_channel_swap;
  uint32_t phy_pll_calibration1;
  uint32_t phy_pll_calibration2;
  uint32_t phy_pll_calibration4;
  uint32_t phy_status1;
  uint32_t rm_control;
  uint32_t rm_offset;
  uint32_t rm_format;
};

DisplayPipelineSnapshot g_firmware_pipeline = {};
ModeRegisterPlan g_native_mode_plan = {};
bool g_native_mode_configured = false;
bool g_native_mode_active = false;
bool g_native_mode_failed = false;

struct DlistTemplate {
  bool valid;
  uint32_t plane_count;
  uint32_t display_width;
  uint32_t display_height;
  Plane planes[kHvs5MaximumPlanes];
  uint32_t words[kHvs5MaximumArmDlistWords];
  uint32_t word_count;
  uint32_t framebuffer_words[kHvs5MaximumPlanes];
  Hvs5FilterKernelUsage required_kernels;
};

DlistTemplate g_dlist_templates[2] = {};
bool g_dlist_cache_hit_logged[2] = {};

void ClearDlistTemplates() {
  memset(g_dlist_templates, 0, sizeof g_dlist_templates);
  memset(g_dlist_cache_hit_logged, 0, sizeof g_dlist_cache_hit_logged);
}

bool SamePlaneLayout(const Plane &left, const Plane &right) {
  return left.pitch == right.pitch && left.width == right.width &&
         left.height == right.height && left.format == right.format &&
         left.filter == right.filter &&
         left.destination_x == right.destination_x &&
         left.destination_y == right.destination_y &&
         left.destination_width == right.destination_width &&
         left.destination_height == right.destination_height;
}

bool MatchesDlistTemplate(const DlistTemplate &dlist_template,
                          const Plane *planes, uint32_t plane_count,
                          uint32_t display_width, uint32_t display_height) {
  if (!dlist_template.valid || dlist_template.plane_count != plane_count ||
      dlist_template.display_width != display_width ||
      dlist_template.display_height != display_height) {
    return false;
  }
  for (uint32_t i = 0U; i < plane_count; ++i) {
    if (planes[i].framebuffer_bus_address == 0U ||
        (planes[i].framebuffer_bus_address & 1U) != 0U ||
        !SamePlaneLayout(dlist_template.planes[i], planes[i])) {
      return false;
    }
  }
  return true;
}

void PatchDlistAddresses(DlistTemplate *dlist_template,
                         const Plane *planes) {
  for (uint32_t i = 0U; i < dlist_template->plane_count; ++i) {
    dlist_template->words[dlist_template->framebuffer_words[i]] =
        planes[i].framebuffer_bus_address;
  }
}

void ClearFirmwareRecoverySnapshot() {
  g_firmware_dlist_saved = false;
  g_firmware_dlist_pointer = 0U;
  g_firmware_dlist_word_count = 0U;
  memset(g_firmware_dlist_words, 0, sizeof g_firmware_dlist_words);
  g_firmware_channel_control = 0U;
  g_firmware_channel_background = 0U;
  g_firmware_pixel_valve_control = 0U;
  g_firmware_pixel_valve_vertical_control = 0U;
  g_firmware_packet_ram_saved = false;
  memset(g_firmware_packet_ram, 0, sizeof g_firmware_packet_ram);
  g_firmware_clock_rates_saved = false;
  g_firmware_pixel_clock = 0U;
  g_firmware_m2mc_clock = 0U;
  g_firmware_pixel_bvb_clock = 0U;
  g_firmware_pipeline = {};
}

#if BMX_V3D_RENDER_TEST_KERNEL
void RetireFirmwareRecoverySnapshot() {
  // The network render-matrix kernel deliberately exercises several native
  // timings during one boot.  The firmware list and clock values are no
  // longer valid recovery targets after the first committed ARM present, but
  // the captured pipeline template and packet RAM remain the input required
  // for later ARM-owned mode changes.
  g_firmware_dlist_saved = false;
  g_firmware_dlist_pointer = 0U;
  g_firmware_dlist_word_count = 0U;
  memset(g_firmware_dlist_words, 0, sizeof g_firmware_dlist_words);
  g_firmware_channel_control = 0U;
  g_firmware_channel_background = 0U;
  g_firmware_pixel_valve_control = 0U;
  g_firmware_pixel_valve_vertical_control = 0U;
  g_firmware_clock_rates_saved = false;
  g_firmware_pixel_clock = 0U;
  g_firmware_m2mc_clock = 0U;
  g_firmware_pixel_bvb_clock = 0U;
}
#endif

uint32_t ReadRegister(uintptr base, uint32_t offset) {
  return read32(base + offset);
}

uint32_t HashPacketRam(const uint32_t *saved_words) {
  uint32_t hash = 2166136261U;
  for (uint32_t i = 0U; i < kHdmiPacketRamWords; ++i) {
    const uint32_t word = saved_words != nullptr ? saved_words[i] :
        ReadRegister(g_port->packet_ram, i * sizeof(uint32_t));
    hash ^= word;
    hash *= 16777619U;
  }
  return hash;
}

void CaptureFirmwarePacketRam() {
  for (uint32_t i = 0U; i < kHdmiPacketRamWords; ++i) {
    g_firmware_packet_ram[i] = ReadRegister(
        g_port->packet_ram, i * sizeof(uint32_t));
  }
  g_firmware_packet_ram_saved = true;
  printf("boot: pi4kms packet-ram capture words=%u hash=0x%08x\r\n",
         static_cast<unsigned>(kHdmiPacketRamWords),
         static_cast<unsigned>(HashPacketRam(g_firmware_packet_ram)));
}

void CapturePixelValve(uintptr base, PixelValveSnapshot *snapshot) {
  snapshot->control = ReadRegister(base, kPixelValveControl);
  snapshot->horizontal_a = ReadRegister(base, kPixelValveHorizontalA);
  snapshot->horizontal_b = ReadRegister(base, kPixelValveHorizontalB);
  snapshot->vertical_a = ReadRegister(base, kPixelValveVerticalA);
  snapshot->vertical_b = ReadRegister(base, kPixelValveVerticalB);
}

const char *ModeName(uint32_t mode) {
  switch (mode) {
    case 0U:
      return "disabled";
    case 1U:
      return "init";
    case 2U:
      return "run";
    case 3U:
      return "eof";
    default:
      return "invalid";
  }
}

uintptr SelectedPixelValveBase() {
  if (g_probe_info.selected_pixel_valve == 2) {
    return kPixelValve2Base;
  }
  if (g_probe_info.selected_pixel_valve == 4) {
    return kPixelValve4Base;
  }
  if (g_probe_info.selected_pixel_valve == 3) {
    return kPixelValve3Base;
  }
  return 0U;
}

bool GetFirmwareClockRate(uint32_t clock_id, uint32_t *rate_hz) {
  if (rate_hz == nullptr) {
    return false;
  }
  TPropertyTagClockRate rate = {};
  rate.nClockId = clock_id;
  CBcmPropertyTags tags;
  if (!tags.GetTag(PROPTAG_GET_CLOCK_RATE, &rate, sizeof rate,
                   sizeof rate.nClockId) || rate.nRate == 0U) {
    *rate_hz = 0U;
    return false;
  }
  *rate_hz = rate.nRate;
  return true;
}

bool SetFirmwareClockRate(uint32_t clock_id, uint32_t rate_hz) {
  TPropertyTagSetClockRate rate = {};
  rate.nClockId = clock_id;
  rate.nRate = rate_hz;
  rate.nSkipSettingTurbo = 0U;
  CBcmPropertyTags tags;
  return tags.GetTag(PROPTAG_SET_CLOCK_RATE, &rate, sizeof rate, 12U) &&
      rate.nRate != 0U;
}

bool ProgramNativeModeClocks(const ModeRegisterPlan &plan) {
  const bool pixel = SetFirmwareClockRate(
      kFirmwareClockPixel, plan.mode.pixel_clock);
  const bool m2mc = SetFirmwareClockRate(
      kFirmwareClockM2mc, plan.hsm_clock);
  const bool pixel_bvb = SetFirmwareClockRate(
      kFirmwareClockPixelBvb, plan.pixel_bvb_clock);
  printf("boot: pi4kms native clocks pixel=%u/%u m2mc=%u/%u "
         "pixel_bvb=%u/%u status=%s\r\n",
         pixel ? 1U : 0U, static_cast<unsigned>(plan.mode.pixel_clock),
         m2mc ? 1U : 0U, static_cast<unsigned>(plan.hsm_clock),
         pixel_bvb ? 1U : 0U,
         static_cast<unsigned>(plan.pixel_bvb_clock),
         pixel && m2mc && pixel_bvb ? "pass" : "fail");
  return pixel && m2mc && pixel_bvb;
}

bool CaptureFirmwareClockRates() {
  const ModeTimingSignature signature = {
    g_firmware_pipeline.hdmi_horizontal_a,
    g_firmware_pipeline.hdmi_horizontal_b,
    g_firmware_pipeline.hdmi_vertical_a0,
    g_firmware_pipeline.hdmi_vertical_b0,
    g_firmware_pipeline.pixel_valve_horizontal_a,
    g_firmware_pipeline.pixel_valve_horizontal_b,
    g_firmware_pipeline.pixel_valve_vertical_a,
    g_firmware_pipeline.pixel_valve_vertical_b
  };
  ModeRegisterPlan plan = {};
  const char *mode_name = nullptr;
  if (g_firmware_pipeline.valid &&
      ResolveKnownModeTiming(signature, &plan, &mode_name)) {
    g_firmware_pixel_clock = plan.mode.pixel_clock;
    g_firmware_m2mc_clock = plan.hsm_clock;
    g_firmware_pixel_bvb_clock = plan.pixel_bvb_clock;
    g_firmware_clock_rates_saved = true;
    printf("boot: pi4kms firmware clocks capture source=timing-plan "
           "mode=%s pixel=%u m2mc=%u pixel_bvb=%u status=pass\r\n",
           mode_name,
           static_cast<unsigned>(g_firmware_pixel_clock),
           static_cast<unsigned>(g_firmware_m2mc_clock),
           static_cast<unsigned>(g_firmware_pixel_bvb_clock));
    return true;
  }
  g_firmware_clock_rates_saved = false;
  printf("boot: pi4kms firmware clocks capture source=timing-plan "
         "status=fail reason=unrecognized-firmware-timing\r\n");
  return false;
}

bool RestoreFirmwareClockRates() {
  if (!g_firmware_clock_rates_saved) {
    return false;
  }
  const bool pixel = SetFirmwareClockRate(
      kFirmwareClockPixel, g_firmware_pixel_clock);
  const bool m2mc = SetFirmwareClockRate(
      kFirmwareClockM2mc, g_firmware_m2mc_clock);
  const bool pixel_bvb = SetFirmwareClockRate(
      kFirmwareClockPixelBvb, g_firmware_pixel_bvb_clock);
  printf("boot: pi4kms firmware clocks restore pixel=%u/%u m2mc=%u/%u "
         "pixel_bvb=%u/%u status=%s\r\n",
         pixel ? 1U : 0U, static_cast<unsigned>(g_firmware_pixel_clock),
         m2mc ? 1U : 0U, static_cast<unsigned>(g_firmware_m2mc_clock),
         pixel_bvb ? 1U : 0U,
         static_cast<unsigned>(g_firmware_pixel_bvb_clock),
         pixel && m2mc && pixel_bvb ? "pass" : "fail");
  return pixel && m2mc && pixel_bvb;
}

bool NativeModeChangesGeometry() {
  return g_native_mode_configured &&
      (g_native_mode_plan.mode.width != g_probe_info.width ||
       g_native_mode_plan.mode.height != g_probe_info.height);
}

bool CaptureDisplayPipeline(DisplayPipelineSnapshot *snapshot) {
  if (snapshot == nullptr || SelectedPixelValveBase() == 0U) {
    return false;
  }
  *snapshot = {};
  const uintptr pixel_valve_base = SelectedPixelValveBase();
  snapshot->hvs_control = ReadRegister(kHvsBase, kHvsControl);
  snapshot->hvs_output4_mux = ReadRegister(kHvsBase, g_port->hvs_mux);
  snapshot->pixel_valve_control = ReadRegister(
      pixel_valve_base, kPixelValveControl);
  snapshot->pixel_valve_vertical_control = ReadRegister(
      pixel_valve_base, kPixelValveVerticalControl);
  snapshot->pixel_valve_horizontal_a = ReadRegister(
      pixel_valve_base, kPixelValveHorizontalA);
  snapshot->pixel_valve_horizontal_b = ReadRegister(
      pixel_valve_base, kPixelValveHorizontalB);
  snapshot->pixel_valve_vertical_a = ReadRegister(
      pixel_valve_base, kPixelValveVerticalA);
  snapshot->pixel_valve_vertical_b = ReadRegister(
      pixel_valve_base, kPixelValveVerticalB);
  snapshot->pixel_valve_mux_config = ReadRegister(
      pixel_valve_base, kPixelValveMuxConfig);
  snapshot->hd_dvp_control = ReadRegister(
      ARM_HD_BASE, kHdmiHdDvpControl);
  snapshot->hd_mai_format = ReadRegister(
      ARM_HD_BASE, g_port->hd_mai_format);
  snapshot->hd_video_control = ReadRegister(
      ARM_HD_BASE, g_port->hd_video_control);
  snapshot->hd_frame_count = ReadRegister(
      ARM_HD_BASE, g_port->hd_frame_count);
  snapshot->hdmi_fifo_control = ReadRegister(
      g_port->core, kHdmiFifoControl);
  snapshot->hdmi_ram_packet_config = ReadRegister(
      g_port->core, kHdmiRamPacketConfig);
  snapshot->hdmi_ram_packet_status = ReadRegister(
      g_port->core, kHdmiRamPacketStatus);
  snapshot->hdmi_ingress_status0 = ReadRegister(
      g_port->core, kHdmiIngressStatus0);
  snapshot->hdmi_ingress_status1 = ReadRegister(
      g_port->core, kHdmiIngressStatus1);
  snapshot->hdmi_ingress_status2 = ReadRegister(
      g_port->core, kHdmiIngressStatus2);
  snapshot->hdmi_format_detect7 = ReadRegister(
      g_port->core, kHdmiFormatDetect7);
  snapshot->hdmi_scheduler_control = ReadRegister(
      g_port->core, kHdmiSchedulerControl);
  snapshot->hdmi_horizontal_a = ReadRegister(
      g_port->core, kHdmiHorizontalA);
  snapshot->hdmi_horizontal_b = ReadRegister(
      g_port->core, kHdmiHorizontalB);
  snapshot->hdmi_vertical_a0 = ReadRegister(
      g_port->core, kHdmiVerticalA0);
  snapshot->hdmi_vertical_b0 = ReadRegister(
      g_port->core, kHdmiVerticalB0);
  snapshot->hdmi_vertical_a1 = ReadRegister(
      g_port->core, kHdmiVerticalA1);
  snapshot->hdmi_vertical_b1 = ReadRegister(
      g_port->core, kHdmiVerticalB1);
  snapshot->hdmi_misc_control = ReadRegister(
      g_port->core, kHdmiMiscControl);
  snapshot->hdmi_deep_color_config1 = ReadRegister(
      g_port->core, kHdmiDeepColorConfig1);
  snapshot->hdmi_gcp_config = ReadRegister(
      g_port->core, kHdmiGcpConfig);
  snapshot->hdmi_gcp_word1 = ReadRegister(
      g_port->core, kHdmiGcpWord1);
  snapshot->dvp_clock_stop = ReadRegister(
      g_port->dvp, kHdmiDvpClockStop);
  snapshot->dvp_status = ReadRegister(
      g_port->dvp, kHdmiDvpStatus);
  snapshot->dvp_interface_config = ReadRegister(
      g_port->dvp, kHdmiDvpInterfaceConfig);
  snapshot->dvp_interface_xbar = ReadRegister(
      g_port->dvp, kHdmiDvpInterfaceXbar);
  snapshot->dvp_software_init = ReadRegister(
      kHdmi0DvpGlobalBase, kHdmiDvpSoftwareInit);
  snapshot->dvp_misc_config = ReadRegister(
      kHdmi0DvpGlobalBase, kHdmiDvpMiscConfig);
  snapshot->csc_control = ReadRegister(g_port->csc, kHdmiCscControl);
  snapshot->csc_12_11 = ReadRegister(g_port->csc, kHdmiCsc12_11);
  snapshot->csc_14_13 = ReadRegister(g_port->csc, kHdmiCsc14_13);
  snapshot->csc_22_21 = ReadRegister(g_port->csc, kHdmiCsc22_21);
  snapshot->csc_24_23 = ReadRegister(g_port->csc, kHdmiCsc24_23);
  snapshot->csc_32_31 = ReadRegister(g_port->csc, kHdmiCsc32_31);
  snapshot->csc_34_33 = ReadRegister(g_port->csc, kHdmiCsc34_33);
  snapshot->csc_channel_control = ReadRegister(
      g_port->csc, kHdmiCscChannelControl);
  snapshot->phy_reset_control = ReadRegister(
      g_port->phy, kHdmiPhyResetControl);
  snapshot->phy_powerdown_control = ReadRegister(
      g_port->phy, kHdmiPhyPowerdownControl);
  snapshot->phy_control0 = ReadRegister(
      g_port->phy, kHdmiPhyControl0);
  snapshot->phy_control1 = ReadRegister(
      g_port->phy, kHdmiPhyControl1);
  snapshot->phy_control2 = ReadRegister(
      g_port->phy, kHdmiPhyControl2);
  snapshot->phy_control3 = ReadRegister(
      g_port->phy, kHdmiPhyControl3);
  snapshot->phy_pll_control0 = ReadRegister(
      g_port->phy, kHdmiPhyPllControl0);
  snapshot->phy_pll_control1 = ReadRegister(
      g_port->phy, kHdmiPhyPllControl1);
  snapshot->phy_clock_divider = ReadRegister(
      g_port->phy, kHdmiPhyClockDivider);
  snapshot->phy_pll_config = ReadRegister(
      g_port->phy, kHdmiPhyPllConfig);
  snapshot->phy_tmds_clock_word_select = ReadRegister(
      g_port->phy, kHdmiPhyTmdsClockWordSelect);
  snapshot->phy_status0 = ReadRegister(
      g_port->phy, kHdmiPhyStatus0);
  snapshot->phy_channel_swap = ReadRegister(
      g_port->phy, kHdmiPhyChannelSwap);
  snapshot->phy_pll_calibration1 = ReadRegister(
      g_port->phy, kHdmiPhyPllCalibration1);
  snapshot->phy_pll_calibration2 = ReadRegister(
      g_port->phy, kHdmiPhyPllCalibration2);
  snapshot->phy_pll_calibration4 = ReadRegister(
      g_port->phy, kHdmiPhyPllCalibration4);
  snapshot->phy_status1 = ReadRegister(
      g_port->phy, kHdmiPhyStatus1);
  snapshot->rm_control = ReadRegister(g_port->rm, kHdmiRmControl);
  snapshot->rm_offset = ReadRegister(g_port->rm, kHdmiRmOffset);
  snapshot->rm_format = ReadRegister(g_port->rm, kHdmiRmFormat);
  snapshot->valid = snapshot->hd_video_control != 0xffffffffU &&
      snapshot->pixel_valve_control != 0xffffffffU &&
      snapshot->phy_reset_control != 0xffffffffU;
  return snapshot->valid;
}

void LogDisplayPipeline(const char *phase,
                        const DisplayPipelineSnapshot &snapshot) {
  printf("boot: pi4kms pipeline phase=%s valid=%u hvs=0x%08x "
         "mux4=0x%08x pv=0x%08x pv_v=0x%08x "
         "pv_h=0x%08x/0x%08x pv_vt=0x%08x/0x%08x "
         "pv_mux=0x%08x\r\n",
         phase, snapshot.valid ? 1U : 0U,
         static_cast<unsigned>(snapshot.hvs_control),
         static_cast<unsigned>(snapshot.hvs_output4_mux),
         static_cast<unsigned>(snapshot.pixel_valve_control),
         static_cast<unsigned>(snapshot.pixel_valve_vertical_control),
         static_cast<unsigned>(snapshot.pixel_valve_horizontal_a),
         static_cast<unsigned>(snapshot.pixel_valve_horizontal_b),
         static_cast<unsigned>(snapshot.pixel_valve_vertical_a),
         static_cast<unsigned>(snapshot.pixel_valve_vertical_b),
         static_cast<unsigned>(snapshot.pixel_valve_mux_config));
  printf("boot: pi4kms pipeline phase=%s hd_dvp=0x%08x hd_mai=0x%08x "
         "hd_vid=0x%08x "
         "frame=0x%08x "
         "fifo=0x%08x packet=0x%08x/0x%08x sched=0x%08x "
         "hdmi_h=0x%08x/0x%08x "
         "hdmi_v=0x%08x/0x%08x/0x%08x/0x%08x misc=0x%08x\r\n",
         phase,
         static_cast<unsigned>(snapshot.hd_dvp_control),
         static_cast<unsigned>(snapshot.hd_mai_format),
         static_cast<unsigned>(snapshot.hd_video_control),
         static_cast<unsigned>(snapshot.hd_frame_count),
         static_cast<unsigned>(snapshot.hdmi_fifo_control),
         static_cast<unsigned>(snapshot.hdmi_ram_packet_config),
         static_cast<unsigned>(snapshot.hdmi_ram_packet_status),
         static_cast<unsigned>(snapshot.hdmi_scheduler_control),
         static_cast<unsigned>(snapshot.hdmi_horizontal_a),
         static_cast<unsigned>(snapshot.hdmi_horizontal_b),
         static_cast<unsigned>(snapshot.hdmi_vertical_a0),
         static_cast<unsigned>(snapshot.hdmi_vertical_b0),
         static_cast<unsigned>(snapshot.hdmi_vertical_a1),
         static_cast<unsigned>(snapshot.hdmi_vertical_b1),
         static_cast<unsigned>(snapshot.hdmi_misc_control));
  printf("boot: pi4kms pipeline phase=%s ingress=0x%08x/0x%08x/0x%08x "
         "format7=0x%08x dvp_status=0x%08x\r\n",
         phase,
         static_cast<unsigned>(snapshot.hdmi_ingress_status0),
         static_cast<unsigned>(snapshot.hdmi_ingress_status1),
         static_cast<unsigned>(snapshot.hdmi_ingress_status2),
         static_cast<unsigned>(snapshot.hdmi_format_detect7),
         static_cast<unsigned>(snapshot.dvp_status));
  printf("boot: pi4kms pipeline phase=%s deep=0x%08x gcp=0x%08x/0x%08x "
         "dvp=0x%08x/0x%08x/0x%08x global=0x%08x/0x%08x "
         "phy_reset=0x%08x "
         "phy_powerdown=0x%08x phy_ctl=0x%08x/0x%08x/0x%08x/0x%08x\r\n",
         phase,
         static_cast<unsigned>(snapshot.hdmi_deep_color_config1),
         static_cast<unsigned>(snapshot.hdmi_gcp_config),
         static_cast<unsigned>(snapshot.hdmi_gcp_word1),
         static_cast<unsigned>(snapshot.dvp_clock_stop),
         static_cast<unsigned>(snapshot.dvp_interface_config),
         static_cast<unsigned>(snapshot.dvp_interface_xbar),
         static_cast<unsigned>(snapshot.dvp_software_init),
         static_cast<unsigned>(snapshot.dvp_misc_config),
         static_cast<unsigned>(snapshot.phy_reset_control),
         static_cast<unsigned>(snapshot.phy_powerdown_control),
         static_cast<unsigned>(snapshot.phy_control0),
         static_cast<unsigned>(snapshot.phy_control1),
         static_cast<unsigned>(snapshot.phy_control2),
         static_cast<unsigned>(snapshot.phy_control3));
  printf("boot: pi4kms pipeline phase=%s pll=0x%08x/0x%08x "
         "div=0x%08x cfg=0x%08x word=0x%08x swap=0x%08x "
         "cal=0x%08x/0x%08x/0x%08x status=0x%08x/0x%08x "
         "rm=0x%08x/0x%08x/0x%08x\r\n",
         phase,
         static_cast<unsigned>(snapshot.phy_pll_control0),
         static_cast<unsigned>(snapshot.phy_pll_control1),
         static_cast<unsigned>(snapshot.phy_clock_divider),
         static_cast<unsigned>(snapshot.phy_pll_config),
         static_cast<unsigned>(snapshot.phy_tmds_clock_word_select),
         static_cast<unsigned>(snapshot.phy_channel_swap),
         static_cast<unsigned>(snapshot.phy_pll_calibration1),
         static_cast<unsigned>(snapshot.phy_pll_calibration2),
         static_cast<unsigned>(snapshot.phy_pll_calibration4),
         static_cast<unsigned>(snapshot.phy_status0),
         static_cast<unsigned>(snapshot.phy_status1),
         static_cast<unsigned>(snapshot.rm_control),
         static_cast<unsigned>(snapshot.rm_offset),
         static_cast<unsigned>(snapshot.rm_format));
  printf("boot: pi4kms pipeline phase=%s csc=0x%08x "
         "coeff=0x%08x/0x%08x/0x%08x/0x%08x/0x%08x/0x%08x "
         "channel=0x%08x\r\n",
         phase,
         static_cast<unsigned>(snapshot.csc_control),
         static_cast<unsigned>(snapshot.csc_12_11),
         static_cast<unsigned>(snapshot.csc_14_13),
         static_cast<unsigned>(snapshot.csc_22_21),
         static_cast<unsigned>(snapshot.csc_24_23),
         static_cast<unsigned>(snapshot.csc_32_31),
         static_cast<unsigned>(snapshot.csc_34_33),
         static_cast<unsigned>(snapshot.csc_channel_control));
}

bool RangesOverlap(uint32_t first_a, uint32_t count_a,
                   uint32_t first_b, uint32_t count_b) {
  return first_a < first_b + count_b && first_b < first_a + count_a;
}

bool ArmDlistRegionSafe(uint32_t slot, uint32_t word_count) {
  if (word_count == 0U || slot < 32U ||
      slot > kHvs5DlistWordCapacity - word_count) {
    return false;
  }
  if (g_firmware_dlist_saved &&
      RangesOverlap(slot, word_count,
                    g_firmware_dlist_pointer,
                    g_firmware_dlist_word_count)) {
    return false;
  }
  for (uint32_t channel = 0U; channel < kHvsChannelCount; ++channel) {
    const uint32_t pending = ReadRegister(
        kHvsBase, kHvsPendingDlist0 + channel * sizeof(uint32_t));
    const uint32_t active = ReadRegister(
        kHvsBase, kHvsActiveDlist0 + channel * sizeof(uint32_t));
    const bool selected_channel =
        channel == static_cast<uint32_t>(g_probe_info.selected_channel);


    const bool pending_owned =
        (g_recovery_lifecycle.FirmwareDisplayClaimed() ||
         g_requested_composite) &&
        selected_channel &&
        (pending == kTakeoverDlistSlots[0] ||
         pending == kTakeoverDlistSlots[1]);
    const bool active_owned =
        (g_recovery_lifecycle.FirmwareDisplayClaimed() ||
         g_requested_composite) &&
        selected_channel &&
        (active == kTakeoverDlistSlots[0] ||
         active == kTakeoverDlistSlots[1]);
    if ((!pending_owned && pending >= slot && pending < slot + word_count) ||
        (!active_owned && active >= slot && active < slot + word_count)) {
      return false;
    }
  }
  return true;
}

bool TakeoverSlotSafe(uint32_t slot) {
  return ArmDlistRegionSafe(slot, kHvs5MaximumArmDlistWords);
}

bool WaitForActiveDlist(uint32_t expected, bool wait_for_vblank) {
  if (!wait_for_vblank) {
    return true;
  }
  const uintptr active_register =
      kHvsBase + kHvsActiveDlist0 +
      static_cast<uint32_t>(g_probe_info.selected_channel) *
          sizeof(uint32_t);
  const uint64_t start = CTimer::GetClockTicks64();
  do {
    if (read32(active_register) == expected) {
      return true;
    }
  } while (CTimer::GetClockTicks64() - start < kDlistSwitchTimeoutUs);
  return read32(active_register) == expected;
}

void WritePendingDlist(uint32_t pointer) {
  const uintptr pending_register =
      kHvsBase + kHvsPendingDlist0 +
      static_cast<uint32_t>(g_probe_info.selected_channel) *
          sizeof(uint32_t);
  DataSyncBarrier();
  write32(pending_register, pointer);
  DataSyncBarrier();
}

void RestartHdmiPhy(const ModeRegisterPlan *native_plan) {
  // Match vc5_hdmi_phy_init(), including the reset-state transitions.  Merely
  // restoring the final register values does not retrigger lane or PLL start.
  write32(g_port->phy + kHdmiPhyResetControl,
          kHdmiPhyLaneResetMask);
  write32(g_port->phy + kHdmiPhyPowerdownControl,
          kHdmiPhyPowerdownLaneMask);
  write32(g_port->phy + kHdmiPhyPowerdownControl,
          kHdmiPhyPowerdownRandomGenerator);
  write32(g_port->phy + kHdmiPhyResetControl, 0U);

  // VC4 DRM updates these registers with read/modify/write operations.  The
  // firmware can leave undocumented calibration and RM bits set, so preserve
  // them while replacing only fields that are part of the native mode plan.
  const uint32_t rm_control = native_plan != nullptr ?
      (g_firmware_pipeline.rm_control | native_plan->rm_control) :
      g_firmware_pipeline.rm_control;
  const uint32_t phy_pll_calibration1 = native_plan != nullptr ?
      ((g_firmware_pipeline.phy_pll_calibration1 & 0xf0000000U) |
       native_plan->phy_pll_calibration1) :
      g_firmware_pipeline.phy_pll_calibration1;
  const uint32_t phy_pll_calibration2 = native_plan != nullptr ?
      ((g_firmware_pipeline.phy_pll_calibration2 & 0xf0000000U) |
       native_plan->phy_pll_calibration2) :
      g_firmware_pipeline.phy_pll_calibration2;
  const uint32_t rm_offset = native_plan != nullptr ?
      native_plan->rm_offset : g_firmware_pipeline.rm_offset;
  const uint32_t phy_clock_divider = native_plan != nullptr ?
      native_plan->phy_clock_divider : g_firmware_pipeline.phy_clock_divider;
  const uint32_t phy_pll_calibration4 = native_plan != nullptr ?
      native_plan->phy_pll_calibration4 :
      g_firmware_pipeline.phy_pll_calibration4;
  const uint32_t phy_pll_control0 = native_plan != nullptr ?
      native_plan->phy_pll_control0 : g_firmware_pipeline.phy_pll_control0;
  const uint32_t phy_pll_control1 = native_plan != nullptr ?
      native_plan->phy_pll_control1 : g_firmware_pipeline.phy_pll_control1;
  const uint32_t rm_format = native_plan != nullptr ?
      ((g_firmware_pipeline.rm_format & ~(3U << 24U)) |
       native_plan->rm_format) :
      g_firmware_pipeline.rm_format;
  const uint32_t phy_pll_config = native_plan != nullptr ?
      native_plan->phy_pll_config : g_firmware_pipeline.phy_pll_config;
  const uint32_t phy_tmds_clock_word_select = native_plan != nullptr ?
      native_plan->phy_tmds_clock_word_select :
      g_firmware_pipeline.phy_tmds_clock_word_select;
  const uint32_t phy_control3 = native_plan != nullptr ?
      native_plan->phy_control3 : g_firmware_pipeline.phy_control3;
  const uint32_t phy_control0 = native_plan != nullptr ?
      native_plan->phy_control0 : g_firmware_pipeline.phy_control0;
  const uint32_t phy_control1 = native_plan != nullptr ?
      native_plan->phy_control1 : g_firmware_pipeline.phy_control1;
  const uint32_t phy_control2 = native_plan != nullptr ?
      native_plan->phy_control2 : g_firmware_pipeline.phy_control2;
  const uint32_t phy_channel_swap = native_plan != nullptr ?
      native_plan->phy_channel_swap : g_firmware_pipeline.phy_channel_swap;
  const uint32_t phy_reset_control = native_plan != nullptr ?
      native_plan->phy_reset_control : g_firmware_pipeline.phy_reset_control;
  const uint32_t phy_powerdown_control = native_plan != nullptr ?
      native_plan->phy_powerdown_control :
      g_firmware_pipeline.phy_powerdown_control;

  write32(g_port->rm + kHdmiRmControl, rm_control);
  write32(g_port->phy + kHdmiPhyPllCalibration1,
          phy_pll_calibration1);
  write32(g_port->phy + kHdmiPhyPllCalibration2,
          phy_pll_calibration2);
  write32(g_port->rm + kHdmiRmOffset, rm_offset);
  write32(g_port->phy + kHdmiPhyClockDivider,
          phy_clock_divider);
  write32(g_port->phy + kHdmiPhyPllCalibration4,
          phy_pll_calibration4);
  write32(g_port->phy + kHdmiPhyPllControl0,
          phy_pll_control0);
  write32(g_port->phy + kHdmiPhyPllControl1,
          phy_pll_control1);
  write32(g_port->rm + kHdmiRmFormat, rm_format);
  write32(g_port->phy + kHdmiPhyPllConfig,
          phy_pll_config);
  write32(g_port->phy + kHdmiPhyTmdsClockWordSelect,
          phy_tmds_clock_word_select);
  write32(g_port->phy + kHdmiPhyControl3,
          phy_control3);
  write32(g_port->phy + kHdmiPhyControl0,
          phy_control0);
  write32(g_port->phy + kHdmiPhyControl1,
          phy_control1);
  write32(g_port->phy + kHdmiPhyControl2,
          phy_control2);
  write32(g_port->phy + kHdmiPhyChannelSwap,
          phy_channel_swap);

  // The low-to-high edge is part of the documented VC5 sequence.  Preserve
  // any non-PLL bits captured from the firmware-owned working mode.
  write32(g_port->phy + kHdmiPhyResetControl,
          phy_reset_control & ~kHdmiPhyPllResetMask);
  write32(g_port->phy + kHdmiPhyResetControl,
          phy_reset_control | kHdmiPhyPllResetMask);
  write32(g_port->phy + kHdmiPhyPowerdownControl,
          phy_powerdown_control);
  DataSyncBarrier();
}

void ResetHdmiCoreForTakeover() {
  // Match vc5_hdmi_reset(), which runs when the Linux HDMI component binds
  // after NOTIFY_DISPLAY_DONE.  Restoring the final DVP values alone misses
  // the reset edge that starts a freshly handed-over HDMI0 core.
  const uint32_t software_init_before = ReadRegister(
      kHdmi0DvpGlobalBase, kHdmiDvpSoftwareInit);
  const uint32_t software_init_restored =
      g_firmware_pipeline.dvp_software_init &
      ~g_port->software_init_bit;
  write32(kHdmi0DvpGlobalBase + kHdmiDvpSoftwareInit,
          software_init_before | g_port->software_init_bit);
  DataSyncBarrier();
  CTimer::SimpleusDelay(1U);
  write32(kHdmi0DvpGlobalBase + kHdmiDvpSoftwareInit,
          software_init_restored);
  write32(kHdmi0DvpGlobalBase + kHdmiDvpMiscConfig,
          g_firmware_pipeline.dvp_misc_config);

  write32(ARM_HD_BASE + kHdmiHdDvpControl, 0U);
  const uint32_t clock_stop_before = ReadRegister(
      g_port->dvp, kHdmiDvpClockStop);
  write32(g_port->dvp + kHdmiDvpClockStop,
          clock_stop_before | kHdmiDvpClockStopPixel);
  DataSyncBarrier();

  printf("boot: pi4kms hdmi core reset software_init=0x%08x->0x%08x "
         "clock_stop=0x%08x->0x%08x hd_dvp=0x%08x\r\n",
         static_cast<unsigned>(software_init_before),
         static_cast<unsigned>(ReadRegister(
             kHdmi0DvpGlobalBase, kHdmiDvpSoftwareInit)),
         static_cast<unsigned>(clock_stop_before),
         static_cast<unsigned>(ReadRegister(
             g_port->dvp, kHdmiDvpClockStop)),
         static_cast<unsigned>(ReadRegister(
             ARM_HD_BASE, kHdmiHdDvpControl)));
}

bool RecenterHdmiFifo() {
  // vc4_hdmi_recenter_fifo() performs the edge twice, one millisecond apart,
  // after the video path has been enabled, and waits for RECENTER_DONE.
  const uint32_t fifo = ReadRegister(
      g_port->core, kHdmiFifoControl) & kHdmiFifoValidWriteMask;
  write32(g_port->core + kHdmiFifoControl,
          fifo & ~kHdmiFifoRecenter);
  write32(g_port->core + kHdmiFifoControl,
          fifo | kHdmiFifoRecenter);
  CTimer::SimpleusDelay(1000U);
  write32(g_port->core + kHdmiFifoControl,
          fifo & ~kHdmiFifoRecenter);
  write32(g_port->core + kHdmiFifoControl,
          fifo | kHdmiFifoRecenter);
  DataSyncBarrier();

  const uint64_t start = CTimer::GetClockTicks64();
  do {
    if ((ReadRegister(g_port->core, kHdmiFifoControl) &
         kHdmiFifoRecenterDone) != 0U) {
      return true;
    }
  } while (CTimer::GetClockTicks64() - start < 1000U);
  return (ReadRegister(g_port->core, kHdmiFifoControl) &
          kHdmiFifoRecenterDone) != 0U;
}

bool WaitForHdmiActive(bool active) {
  const uint64_t start = CTimer::GetClockTicks64();
  do {
    const bool current =
        (ReadRegister(g_port->core, kHdmiSchedulerControl) &
         kHdmiSchedulerHdmiActive) != 0U;
    if (current == active) {
      return true;
    }
  } while (CTimer::GetClockTicks64() - start < 1000U);
  return ((ReadRegister(g_port->core, kHdmiSchedulerControl) &
           kHdmiSchedulerHdmiActive) != 0U) == active;
}

bool RestartDisplayPipeline(uint32_t control, uint32_t background,
                            const char *owner,
                            const ModeRegisterPlan *native_plan) {
  if (!g_firmware_pipeline.valid) {
    return false;
  }
  if (native_plan != nullptr && !ProgramNativeModeClocks(*native_plan)) {
    return false;
  }
  const uint32_t channel =
      static_cast<uint32_t>(g_probe_info.selected_channel);
  const uintptr control_register =
      kHvsBase + kHvsChannelControl0 + channel * kHvsChannelStride;
  const uintptr background_register =
      kHvsBase + kHvsChannelBackground0 + channel * kHvsChannelStride;
  const uintptr pixel_valve_base = SelectedPixelValveBase();
  const uint32_t before_control = read32(control_register);
  const uint32_t before_status = ReadRegister(
      kHvsBase, kHvsChannelStatus0 + channel * kHvsChannelStride);
  const uint32_t before_pv_control = ReadRegister(
      pixel_valve_base, kPixelValveControl);
  const uint32_t before_pv_vertical = ReadRegister(
      pixel_valve_base, kPixelValveVerticalControl);
  const uint32_t pixel_valve_control = native_plan != nullptr ?
      native_plan->pixel_valve_control :
      g_firmware_pipeline.pixel_valve_control;
  const uint32_t pixel_valve_vertical_control = native_plan != nullptr ?
      native_plan->pixel_valve_vertical_control :
      g_firmware_pipeline.pixel_valve_vertical_control;
  const uint32_t pixel_valve_horizontal_a = native_plan != nullptr ?
      native_plan->pixel_valve_horizontal_a :
      g_firmware_pipeline.pixel_valve_horizontal_a;
  const uint32_t pixel_valve_horizontal_b = native_plan != nullptr ?
      native_plan->pixel_valve_horizontal_b :
      g_firmware_pipeline.pixel_valve_horizontal_b;
  const uint32_t pixel_valve_vertical_a = native_plan != nullptr ?
      native_plan->pixel_valve_vertical_a :
      g_firmware_pipeline.pixel_valve_vertical_a;
  const uint32_t pixel_valve_vertical_b = native_plan != nullptr ?
      native_plan->pixel_valve_vertical_b :
      g_firmware_pipeline.pixel_valve_vertical_b;
  const uint32_t hdmi_horizontal_a = native_plan != nullptr ?
      native_plan->hdmi_horizontal_a :
      g_firmware_pipeline.hdmi_horizontal_a;
  const uint32_t hdmi_horizontal_b = native_plan != nullptr ?
      native_plan->hdmi_horizontal_b :
      g_firmware_pipeline.hdmi_horizontal_b;
  const uint32_t hdmi_vertical_a = native_plan != nullptr ?
      native_plan->hdmi_vertical_a : g_firmware_pipeline.hdmi_vertical_a0;
  const uint32_t hdmi_vertical_b_even = native_plan != nullptr ?
      native_plan->hdmi_vertical_b_even :
      g_firmware_pipeline.hdmi_vertical_b0;
  const uint32_t hdmi_vertical_b_odd = native_plan != nullptr ?
      native_plan->hdmi_vertical_b_odd :
      g_firmware_pipeline.hdmi_vertical_b1;

  // DISPLAY_DONE parks the whole VC5 pipeline.  Rebuild the captured,
  // already-working firmware mode in the same broad order as VC4 DRM:
  // clocks/PHY and HDMI timing first, then HVS, PixelValve, and HD video.
  write32(pixel_valve_base + kPixelValveVerticalControl,
          pixel_valve_vertical_control & ~kPixelValveVideoEnable);
  write32(pixel_valve_base + kPixelValveControl,
          pixel_valve_control & ~kPixelValveEnable);
  write32(pixel_valve_base + kPixelValveControl,
          (pixel_valve_control & ~kPixelValveEnable) |
              kPixelValveFifoClear);
  DataSyncBarrier();

  write32(kHvsBase + kHvsControl, g_firmware_pipeline.hvs_control);

  ResetHdmiCoreForTakeover();

  RestartHdmiPhy(native_plan);

  // Both vc5_hdmi_set_timings() and the working Pi5 KMS path release every
  // DVP clock-stop bit after the core reset.  The firmware's live value is
  // not a valid substitute for this post-reset transition.
  write32(g_port->dvp + kHdmiDvpClockStop, 0U);
  write32(g_port->dvp + kHdmiDvpInterfaceConfig,
          g_firmware_pipeline.dvp_interface_config);
  write32(g_port->dvp + kHdmiDvpInterfaceXbar,
          g_firmware_pipeline.dvp_interface_xbar);
  write32(g_port->csc + kHdmiCsc12_11,
          g_firmware_pipeline.csc_12_11);
  write32(g_port->csc + kHdmiCsc14_13,
          g_firmware_pipeline.csc_14_13);
  write32(g_port->csc + kHdmiCsc22_21,
          g_firmware_pipeline.csc_22_21);
  write32(g_port->csc + kHdmiCsc24_23,
          g_firmware_pipeline.csc_24_23);
  write32(g_port->csc + kHdmiCsc32_31,
          g_firmware_pipeline.csc_32_31);
  write32(g_port->csc + kHdmiCsc34_33,
          g_firmware_pipeline.csc_34_33);
  write32(g_port->csc + kHdmiCscChannelControl,
          g_firmware_pipeline.csc_channel_control);
  write32(g_port->csc + kHdmiCscControl,
          g_firmware_pipeline.csc_control);
  // DISPLAY_DONE leaves MODE_HDMI and HDMI_ACTIVE asserted even though the
  // encoder has been parked.  Force the same inactive-to-active transition
  // that a fresh VC4 DRM modeset gets from reset state; rewriting an already
  // asserted MODE_HDMI bit does not restart the scheduler.
  write32(g_port->core + kHdmiRamPacketConfig,
          g_firmware_pipeline.hdmi_ram_packet_config &
              ~kHdmiRamPacketEnable);
  write32(g_port->core + kHdmiSchedulerControl,
          (g_firmware_pipeline.hdmi_scheduler_control |
           kHdmiSchedulerManualFormat |
           kHdmiSchedulerIgnoreVsyncPredicts) &
              ~kHdmiSchedulerModeHdmi);
  DataSyncBarrier();
  const bool hdmi_deactivated = WaitForHdmiActive(false);
  write32(g_port->core + kHdmiHorizontalA,
          hdmi_horizontal_a);
  write32(g_port->core + kHdmiHorizontalB,
          hdmi_horizontal_b);
  write32(g_port->core + kHdmiVerticalA0,
          hdmi_vertical_a);
  write32(g_port->core + kHdmiVerticalB0,
          hdmi_vertical_b_even);
  write32(g_port->core + kHdmiVerticalA1,
          hdmi_vertical_a);
  write32(g_port->core + kHdmiVerticalB1,
          hdmi_vertical_b_odd);
  write32(g_port->core + kHdmiMiscControl,
          g_firmware_pipeline.hdmi_misc_control);
  write32(g_port->core + kHdmiDeepColorConfig1,
          g_firmware_pipeline.hdmi_deep_color_config1);
  const uint32_t gcp_word1 =
      (g_firmware_pipeline.hdmi_gcp_word1 &
       ~kHdmiGcpSubpacketByte0Mask) |
      kHdmiGcpClearAvMute;
  write32(g_port->core + kHdmiGcpWord1, gcp_word1);
  write32(g_port->core + kHdmiGcpConfig,
          g_firmware_pipeline.hdmi_gcp_config | kHdmiGcpEnable);

  write32(pixel_valve_base + kPixelValveHorizontalA,
          pixel_valve_horizontal_a);
  write32(pixel_valve_base + kPixelValveHorizontalB,
          pixel_valve_horizontal_b);
  write32(pixel_valve_base + kPixelValveVerticalA,
          pixel_valve_vertical_a);
  write32(pixel_valve_base + kPixelValveVerticalB,
          pixel_valve_vertical_b);
  write32(pixel_valve_base + kPixelValveMuxConfig,
          g_firmware_pipeline.pixel_valve_mux_config);

  // Match vc4_hvs_init_channel() for HVS5.  The channel then waits for
  // PixelValve VSTART and latches the already-installed pending dlist.
  write32(control_register, 0U);
  write32(control_register, kHvs5ChannelReset);
  write32(control_register, 0U);
  write32(control_register, control);
  write32(background_register, background);

  write32(pixel_valve_base + kPixelValveControl,
          pixel_valve_control | kPixelValveEnable);
  write32(pixel_valve_base + kPixelValveVerticalControl,
          pixel_valve_vertical_control);

  // Match vc4_hdmi_encoder_post_crtc_enable(): restart the HD video state
  // machine with its one-shot clear/reset bits, then remove output blanking.
  uint32_t hd_video = ReadRegister(ARM_HD_BASE, g_port->hd_video_control);
  hd_video &= ~(kHdmiHdVideoVsyncLow | kHdmiHdVideoHsyncLow);
  if (native_plan != nullptr) {
    if (!native_plan->mode.v_sync_positive) {
      hd_video |= kHdmiHdVideoVsyncLow;
    }
    if (!native_plan->mode.h_sync_positive) {
      hd_video |= kHdmiHdVideoHsyncLow;
    }
  } else {
    hd_video |= g_firmware_pipeline.hd_video_control &
        (kHdmiHdVideoVsyncLow | kHdmiHdVideoHsyncLow);
  }
  hd_video |= kHdmiHdVideoEnable | kHdmiHdVideoUnderflowEnable |
      kHdmiHdVideoFrameCounterReset | kHdmiHdVideoClearRgb |
      kHdmiHdVideoBlankInsertEnable;
  write32(ARM_HD_BASE + g_port->hd_video_control, hd_video);
  write32(ARM_HD_BASE + g_port->hd_video_control,
          ReadRegister(ARM_HD_BASE, g_port->hd_video_control) &
              ~kHdmiHdVideoBlankPixel);

  const uint32_t packet_ram_hash_before = HashPacketRam(nullptr);
  if (g_firmware_packet_ram_saved) {
    // Packet RAM must be globally powered while it is written.  Keep every
    // individual packet slot disabled until the complete firmware image has
    // been restored, then re-enable the captured slot mask below.
    write32(g_port->core + kHdmiRamPacketConfig,
            kHdmiRamPacketEnable);
    for (uint32_t i = 0U; i < kHdmiPacketRamWords; ++i) {
      write32(g_port->packet_ram + i * sizeof(uint32_t),
              g_firmware_packet_ram[i]);
    }
    DataSyncBarrier();
  }
  const uint32_t packet_ram_hash_after = HashPacketRam(nullptr);
  const uint32_t packet_ram_hash_expected =
      HashPacketRam(g_firmware_packet_ram);
  const bool packet_ram_restored = g_firmware_packet_ram_saved &&
      packet_ram_hash_after == packet_ram_hash_expected;

  write32(g_port->core + kHdmiSchedulerControl,
          ReadRegister(g_port->core, kHdmiSchedulerControl) |
              kHdmiSchedulerModeHdmi);
  DataSyncBarrier();
  const bool hdmi_activated_early = WaitForHdmiActive(true);
  write32(g_port->core + kHdmiRamPacketConfig,
          g_firmware_pipeline.hdmi_ram_packet_config);
  DataSyncBarrier();

  const bool fifo_recentered = RecenterHdmiFifo();
  const uint32_t frame_count_start = ReadRegister(
      ARM_HD_BASE, g_port->hd_frame_count);
  CTimer::SimpleusDelay(25000U);
  const uint32_t frame_count_end = ReadRegister(
      ARM_HD_BASE, g_port->hd_frame_count);
  const bool hdmi_activated = hdmi_activated_early || WaitForHdmiActive(true);

  const uint32_t after_control = read32(control_register);
  const uint32_t after_status = ReadRegister(
      kHvsBase, kHvsChannelStatus0 + channel * kHvsChannelStride);
  const uint32_t after_phy_reset = ReadRegister(
      g_port->phy, kHdmiPhyResetControl);
  const uint32_t after_phy_powerdown = ReadRegister(
      g_port->phy, kHdmiPhyPowerdownControl);
  const uint32_t after_hd_video = ReadRegister(
      ARM_HD_BASE, g_port->hd_video_control);
  printf("boot: pi4kms pipeline restart owner=%s control=0x%08x->0x%08x "
         "status=0x%08x->0x%08x background=0x%08x pv=0x%08x "
         "phy=0x%08x/0x%08x hd_vid=0x%08x frame=0x%08x->0x%08x "
         "fifo=0x%08x recenter=%s sched=0x%08x transition=%s/%s\r\n",
         owner, static_cast<unsigned>(before_control),
         static_cast<unsigned>(after_control),
         static_cast<unsigned>(before_status),
         static_cast<unsigned>(after_status),
         static_cast<unsigned>(read32(background_register)),
         static_cast<unsigned>(ReadRegister(
             pixel_valve_base, kPixelValveControl)),
         static_cast<unsigned>(after_phy_reset),
         static_cast<unsigned>(after_phy_powerdown),
         static_cast<unsigned>(after_hd_video),
         static_cast<unsigned>(frame_count_start),
         static_cast<unsigned>(frame_count_end),
         static_cast<unsigned>(ReadRegister(
             g_port->core, kHdmiFifoControl)),
         fifo_recentered ? "done" : "timeout",
         static_cast<unsigned>(ReadRegister(
             g_port->core, kHdmiSchedulerControl)),
         hdmi_deactivated ? "inactive" : "inactive-timeout",
         hdmi_activated ? "active" : "active-timeout");
  printf("boot: pi4kms pixelvalve video owner=%s vertical_control="
         "0x%08x->0x%08x control=0x%08x->0x%08x\r\n",
         owner, static_cast<unsigned>(before_pv_vertical),
         static_cast<unsigned>(ReadRegister(
             pixel_valve_base, kPixelValveVerticalControl)),
         static_cast<unsigned>(before_pv_control),
         static_cast<unsigned>(ReadRegister(
             pixel_valve_base, kPixelValveControl)));
  printf("boot: pi4kms hdmi unmute owner=%s gcp=0x%08x/0x%08x\r\n",
         owner,
         static_cast<unsigned>(ReadRegister(
             g_port->core, kHdmiGcpConfig)),
         static_cast<unsigned>(ReadRegister(
             g_port->core, kHdmiGcpWord1)));
  printf("boot: pi4kms auxiliary restore owner=%s pv_mux=0x%08x "
         "packet=0x%08x/0x%08x packet_ram=0x%08x->0x%08x/0x%08x "
         "phy_status=0x%08x/0x%08x\r\n",
         owner,
         static_cast<unsigned>(ReadRegister(
             pixel_valve_base, kPixelValveMuxConfig)),
         static_cast<unsigned>(ReadRegister(
             g_port->core, kHdmiRamPacketConfig)),
         static_cast<unsigned>(ReadRegister(
             g_port->core, kHdmiRamPacketStatus)),
         static_cast<unsigned>(packet_ram_hash_before),
         static_cast<unsigned>(packet_ram_hash_after),
         static_cast<unsigned>(packet_ram_hash_expected),
         static_cast<unsigned>(ReadRegister(
             g_port->phy, kHdmiPhyStatus0)),
         static_cast<unsigned>(ReadRegister(
             g_port->phy, kHdmiPhyStatus1)));
  DisplayPipelineSnapshot restarted_pipeline = {};
  if (CaptureDisplayPipeline(&restarted_pipeline)) {
    LogDisplayPipeline("post-restart", restarted_pipeline);
  }
  return after_control == control &&
         (after_control & kHvs5ChannelEnable) != 0U &&
         hdmi_deactivated && hdmi_activated &&
         packet_ram_restored &&
         after_phy_reset == (native_plan != nullptr ?
             native_plan->phy_reset_control :
             g_firmware_pipeline.phy_reset_control) &&
         after_phy_powerdown == (native_plan != nullptr ?
             native_plan->phy_powerdown_control :
             g_firmware_pipeline.phy_powerdown_control) &&
         (ReadRegister(g_port->core, kHdmiSchedulerControl) &
          kHdmiSchedulerHdmiActive) != 0U &&
         (after_hd_video & kHdmiHdVideoEnable) ==
             (g_firmware_pipeline.hd_video_control &
              kHdmiHdVideoEnable);
}

void LogFirmwareClockRate(const char *phase, const char *name,
                          uint32_t clock_id) {
  uint32_t rate_hz = 0U;
  const bool valid = GetFirmwareClockRate(clock_id, &rate_hz);
  printf("boot: pi4kms clock phase=%s name=%s id=%u valid=%u rate_hz=%u\r\n",
         phase, name, static_cast<unsigned>(clock_id), valid ? 1U : 0U,
         static_cast<unsigned>(rate_hz));
}

void LogDisplayOwnershipState(const char *phase) {
  const uintptr pixel_valve_base = SelectedPixelValveBase();
  const uint32_t channel =
      static_cast<uint32_t>(g_probe_info.selected_channel);
  printf("boot: pi4kms ownership-state phase=%s hvs_mux=0x%08x "
         "hvs_control=0x%08x channel_control=0x%08x "
         "channel_status=0x%08x pending=%u active=%u\r\n",
         phase,
         static_cast<unsigned>(ReadRegister(kHvsBase, g_port->hvs_mux)),
         static_cast<unsigned>(ReadRegister(kHvsBase, kHvsControl)),
         static_cast<unsigned>(ReadRegister(
             kHvsBase, kHvsChannelControl0 +
             channel * kHvsChannelStride)),
         static_cast<unsigned>(ReadRegister(
             kHvsBase, kHvsChannelStatus0 +
             channel * kHvsChannelStride)),
         static_cast<unsigned>(ReadRegister(
             kHvsBase, kHvsPendingDlist0 + channel * sizeof(uint32_t))),
         static_cast<unsigned>(ReadRegister(
             kHvsBase, kHvsActiveDlist0 + channel * sizeof(uint32_t))));
  printf("boot: pi4kms ownership-state phase=%s pv_control=0x%08x "
         "pv_vertical=0x%08x pv_int=0x%08x pv_status=0x%08x "
         "dvp_clock_stop=0x%08x hd_vid=0x%08x hd_frame=0x%08x "
         "hdmi_sched=0x%08x phy_reset=0x%08x phy_powerdown=0x%08x\r\n",
         phase,
         static_cast<unsigned>(ReadRegister(
             pixel_valve_base, kPixelValveControl)),
         static_cast<unsigned>(ReadRegister(
             pixel_valve_base, kPixelValveVerticalControl)),
         static_cast<unsigned>(ReadRegister(
             pixel_valve_base, kPixelValveInterruptStatus)),
         static_cast<unsigned>(ReadRegister(
             pixel_valve_base, kPixelValveStatus)),
         static_cast<unsigned>(ReadRegister(
             g_port->dvp, kHdmiDvpClockStop)),
         static_cast<unsigned>(ReadRegister(
             ARM_HD_BASE, g_port->hd_video_control)),
         static_cast<unsigned>(ReadRegister(
             ARM_HD_BASE, g_port->hd_frame_count)),
         static_cast<unsigned>(ReadRegister(
             g_port->core, kHdmiSchedulerControl)),
         static_cast<unsigned>(ReadRegister(
             g_port->phy, kHdmiPhyResetControl)),
         static_cast<unsigned>(ReadRegister(
             g_port->phy, kHdmiPhyPowerdownControl)));
  DisplayPipelineSnapshot pipeline = {};
  if (CaptureDisplayPipeline(&pipeline)) {
    LogDisplayPipeline(phase, pipeline);
  }
  LogFirmwareClockRate(phase, "pixel", kFirmwareClockPixel);
  LogFirmwareClockRate(phase, "m2mc", kFirmwareClockM2mc);
  LogFirmwareClockRate(phase, "pixel-bvb", kFirmwareClockPixelBvb);
  LogFirmwareClockRate(phase, "display", kFirmwareClockDisplay);
}

bool NotifyFirmwareDisplayDone() {
  TPropertyTagSimple tag = {};
  tag.Tag.nTagId = kFirmwareNotifyDisplayDone;
  tag.Tag.nValueBufSize = sizeof tag.nValue;
  tag.Tag.nValueLength = 0U;
  LogDisplayOwnershipState("before-notify");
  CBcmPropertyTags tags;
  if (!tags.GetTags(&tag, sizeof tag)) {
    printf("boot: pi4kms firmware handover status=fail "
           "tag=0x%08x\r\n",
           static_cast<unsigned>(kFirmwareNotifyDisplayDone));
    return false;
  }
  g_recovery_lifecycle.MarkFirmwareDisplayClaimed();
  LogDisplayOwnershipState("after-notify");
  printf("boot: pi4kms firmware handover status=pass tag=0x%08x "
         "response=0x%08x ownership=arm\r\n",
         static_cast<unsigned>(kFirmwareNotifyDisplayDone),
         static_cast<unsigned>(tag.Tag.nValueLength));
  return true;
}

uint32_t SelectedChannelDlist(uint32_t register_offset) {
  const uint32_t channel =
      static_cast<uint32_t>(g_probe_info.selected_channel);
  return ReadRegister(kHvsBase,
                      register_offset + channel * sizeof(uint32_t));
}

bool CaptureActiveFirmwareDlist() {
  const uint32_t pending = SelectedChannelDlist(kHvsPendingDlist0);
  const uint32_t active = SelectedChannelDlist(kHvsActiveDlist0);
  if (active != pending || active >= kHvs5DlistWordCapacity) {
    printf("boot: pi4kms takeover status=deferred phase=capture "
           "pending=%u active=%u\r\n",
           static_cast<unsigned>(pending),
           static_cast<unsigned>(active));
    return false;
  }

  uint32_t words[kProbeDlistWordCapacity] = {};
  uint32_t captured = 0U;
  for (; captured < kProbeDlistWordCapacity &&
         active + captured < kHvs5DlistWordCapacity; ++captured) {
    words[captured] = ReadRegister(
        kHvsBase, kHvs5DlistStart +
        (active + captured) * sizeof(uint32_t));
  }
  Hvs5DlistInfo info = {};
  if (!InspectHvs5Dlist(words, captured, &info) ||
      !info.end_found || info.used_words < kHvs5Rgb565UnityDlistWords) {
    printf("boot: pi4kms takeover status=fail phase=capture "
           "dlist=%u captured=%u used=%u end=%u\r\n",
           static_cast<unsigned>(active),
           static_cast<unsigned>(captured),
           static_cast<unsigned>(info.used_words),
           info.end_found ? 1U : 0U);
    return false;
  }

  memcpy(g_firmware_dlist_words, words,
         info.used_words * sizeof(uint32_t));
  g_firmware_dlist_pointer = active;
  g_firmware_dlist_word_count = info.used_words;
  const uint32_t channel =
      static_cast<uint32_t>(g_probe_info.selected_channel);
  g_firmware_channel_control = ReadRegister(
      kHvsBase, kHvsChannelControl0 + channel * kHvsChannelStride);
  g_firmware_channel_background = ReadRegister(
      kHvsBase, kHvsChannelBackground0 + channel * kHvsChannelStride);
  g_firmware_pixel_valve_control = ReadRegister(
      SelectedPixelValveBase(), kPixelValveControl);
  g_firmware_pixel_valve_vertical_control = ReadRegister(
      SelectedPixelValveBase(), kPixelValveVerticalControl);


  if (!g_requested_composite) {
    if (!CaptureDisplayPipeline(&g_firmware_pipeline)) {
      printf("boot: pi4kms takeover status=fail phase=pipeline-capture\r\n");
      return false;
    }
    if (NativeModeChangesGeometry() &&
        !g_firmware_clock_rates_saved && !CaptureFirmwareClockRates()) {
      printf("boot: pi4kms takeover status=fail phase=clock-capture\r\n");
      return false;
    }
    CaptureFirmwarePacketRam();
  }
  g_firmware_dlist_saved = true;
  printf("boot: pi4kms takeover firmware-list dlist=%u words=%u "
         "hash=0x%08x control=0x%08x background=0x%08x "
         "pv_control=0x%08x pv_vertical=0x%08x status=saved\r\n",
         static_cast<unsigned>(active),
         static_cast<unsigned>(info.used_words),
         static_cast<unsigned>(info.hash),
         static_cast<unsigned>(g_firmware_channel_control),
         static_cast<unsigned>(g_firmware_channel_background),
         static_cast<unsigned>(g_firmware_pixel_valve_control),
         static_cast<unsigned>(g_firmware_pixel_valve_vertical_control));
  return true;
}

enum DlistVerifyMode {
  kVerifyCustom,
  kVerifyFirmwareControls
};

bool IsFirmwareControlOrEndWord(const uint32_t *words,
                                uint32_t word_count,
                                uint32_t index) {
  if (words == nullptr || word_count == 0U) {
    return false;
  }
  if (index == word_count - 1U) {
    return true;
  }
  uint32_t offset = 0U;
  while (offset < word_count - 1U) {
    if (index == offset) {
      return true;
    }
    const uint32_t plane_words = (words[offset] >> 24U) & 0x3fU;
    if (plane_words == 0U || plane_words >= word_count - offset) {
      return false;
    }
    offset += plane_words;
  }
  return false;
}

bool WriteAndVerifyDlist(uint32_t slot, const uint32_t *words,
                         uint32_t word_count, DlistVerifyMode verify_mode) {
  if (words == nullptr || word_count == 0U ||
      slot > kHvs5DlistWordCapacity - word_count) {
    return false;
  }
  for (uint32_t i = 0U; i < word_count; ++i) {
    write32(kHvsBase + kHvs5DlistStart +
                (slot + i) * sizeof(uint32_t), words[i]);
  }
  DataSyncBarrier();
  for (uint32_t i = 0U; i < word_count; ++i) {
    if ((verify_mode == kVerifyCustom &&
         words[i] == 0xc0c0c0c0U) ||
        (verify_mode == kVerifyFirmwareControls &&
         !IsFirmwareControlOrEndWord(words, word_count, i))) {
      continue;
    }
    const uintptr word_register = kHvsBase + kHvs5DlistStart +
        (slot + i) * sizeof(uint32_t);
    uint32_t actual = 0U;
    uint32_t read_attempt = 0U;
    for (; read_attempt < kDlistReadAttempts; ++read_attempt) {
      actual = read32(word_register);
      if (actual == words[i]) {
        break;
      }
      if (read_attempt + 1U < kDlistReadAttempts) {
        CTimer::SimpleusDelay(1U);
      }
    }
    if (actual != words[i]) {
      const uint32_t channel =
          static_cast<uint32_t>(g_probe_info.selected_channel);
      printf("boot: pi4kms dlist verify status=fail slot=%u word=%u "
             "expected=0x%08x actual=0x%08x reads=%u "
             "pending=%u active=%u\r\n",
             static_cast<unsigned>(slot), static_cast<unsigned>(i),
             static_cast<unsigned>(words[i]),
             static_cast<unsigned>(actual),
             static_cast<unsigned>(kDlistReadAttempts),
             static_cast<unsigned>(ReadRegister(
                 kHvsBase, kHvsPendingDlist0 +
                 channel * sizeof(uint32_t))),
             static_cast<unsigned>(ReadRegister(
                 kHvsBase, kHvsActiveDlist0 +
                 channel * sizeof(uint32_t))));
      return false;
    }
  }
  return true;
}

typedef void (*BuildFilterKernelFn)(
    uint32_t words[kHvs5FilterKernelWords]);

bool UploadHvs5FilterKernel(uint32_t slot, const char *name,
                            BuildFilterKernelFn build) {
  if (!ArmDlistRegionSafe(slot, kHvs5FilterKernelWords)) {
    printf("boot: pi4kms takeover status=fail phase=filter-kernel "
           "filter=%s slot=%u\r\n", name, static_cast<unsigned>(slot));
    return false;
  }
  uint32_t kernel[kHvs5FilterKernelWords] = {};
  build(kernel);
  if (!WriteAndVerifyDlist(slot, kernel, kHvs5FilterKernelWords,
                           kVerifyCustom)) {
    printf("boot: pi4kms takeover status=fail phase=filter-kernel-write "
           "filter=%s slot=%u\r\n", name, static_cast<unsigned>(slot));
    return false;
  }
  return true;
}

bool ConfigureResolvedNativeMode(const Mode &mode,
                                 const char *source,
                                 unsigned hdmi_group,
                                 unsigned hdmi_mode) {
  g_native_mode_plan = {};
  g_native_mode_configured = false;
  g_native_mode_active = false;
  g_native_mode_failed = false;

  ModeRegisterPlan plan = {};
  if (!BuildModeRegisterPlan(mode, &plan,
                             g_port == &kHdmiPorts[1] ? 1U : 0U)) {
    printf("boot: pi4kms native mode status=deferred reason=invalid "
           "size=%ux%u\r\n",
           static_cast<unsigned>(mode.width),
           static_cast<unsigned>(mode.height));
    return false;
  }
  if (!g_probe_usable) {
    printf("boot: pi4kms native mode status=deferred "
           "reason=probe-unusable requested=%ux%u\r\n",
           static_cast<unsigned>(mode.width),
           static_cast<unsigned>(mode.height));
    return false;
  }

  g_native_mode_plan = plan;
  g_native_mode_configured = true;
  printf("boot: pi4kms native mode status=armed source=%s "
         "group=%u mode=%u size=%ux%u pclk=%u totals=%ux%u "
         "clocks=%u/%u geometry=%s recovery=firmware-static "
         "phy=0x%08x/0x%08x/0x%08x rm=0x%08x port=%u swap=0x%04x "
         "pv_control=0x%08x\r\n",
         source, hdmi_group, hdmi_mode,
         static_cast<unsigned>(mode.width),
         static_cast<unsigned>(mode.height),
         static_cast<unsigned>(mode.pixel_clock),
         static_cast<unsigned>(plan.horizontal_total),
         static_cast<unsigned>(plan.vertical_total),
         static_cast<unsigned>(plan.hsm_clock),
         static_cast<unsigned>(plan.pixel_bvb_clock),
         mode.width == g_probe_info.width &&
                 mode.height == g_probe_info.height ? "same" : "switch",
         static_cast<unsigned>(plan.phy_control0),
         static_cast<unsigned>(plan.phy_control1),
         static_cast<unsigned>(plan.phy_control2),
         static_cast<unsigned>(plan.rm_offset),
         g_port == &kHdmiPorts[1] ? 1U : 0U,
         static_cast<unsigned>(plan.phy_channel_swap),
         static_cast<unsigned>(plan.pixel_valve_control));
  return true;
}

}  // namespace

bool ProbeFirmwareScanout() {
  ProbeSnapshot snapshot = {};
  snapshot.hvs_control = ReadRegister(kHvsBase, kHvsControl);
  snapshot.hvs_status = ReadRegister(kHvsBase, kHvsStatus);
  snapshot.hvs_identity = ReadRegister(kHvsBase, kHvsIdentity);
  snapshot.hvs_output2_mux = ReadRegister(kHvsBase, kHvsOutput2Mux);
  snapshot.hvs_output5_mux = ReadRegister(kHvsBase, kHvsOutput5Mux);
  snapshot.hvs_output4_mux = ReadRegister(kHvsBase, kHvsOutput4Mux);
  for (uint32_t channel = 0U; channel < kHvsChannelCount; ++channel) {
    const uint32_t channel_offset = channel * kHvsChannelStride;
    HvsChannelSnapshot &current = snapshot.channels[channel];
    current.control = ReadRegister(
        kHvsBase, kHvsChannelControl0 + channel_offset);
    current.background = ReadRegister(
        kHvsBase, kHvsChannelBackground0 + channel_offset);
    current.status = ReadRegister(
        kHvsBase, kHvsChannelStatus0 + channel_offset);
    current.pending_dlist = ReadRegister(
        kHvsBase, kHvsPendingDlist0 + channel * sizeof(uint32_t));
    current.active_dlist = ReadRegister(
        kHvsBase, kHvsActiveDlist0 + channel * sizeof(uint32_t));
    current.fifo_base = ReadRegister(
        kHvsBase, kHvsChannelFifoBase0 + channel_offset);
  }
  if (!g_requested_composite) {
    CapturePixelValve(kPixelValve2Base, &snapshot.pixel_valve2);
    CapturePixelValve(kPixelValve4Base, &snapshot.pixel_valve4);
  }



  if (g_requested_composite) {
    if ((snapshot.hvs_control & kHvsControlEnable) != 0U) {
      CapturePixelValve(kPixelValve3Base, &snapshot.pixel_valve3);
    }
    snapshot.prefer_composite = true;
  }


  snapshot.prefer_hdmi1 = g_requested_port == 1U;

  const int32_t selected_channel = ResolveFirmwareHdmiChannel(
      snapshot, nullptr);
  if (selected_channel >= 0) {
    const uint32_t active_dlist =
        snapshot.channels[selected_channel].active_dlist;
    if (active_dlist < kHvs5DlistWordCapacity) {
      for (uint32_t i = 0U; i < kProbeDlistWordCapacity &&
                            active_dlist + i < kHvs5DlistWordCapacity; ++i) {
        const uint32_t word = ReadRegister(
            kHvsBase, kHvs5DlistStart + (active_dlist + i) * sizeof word);
        snapshot.dlist_words[snapshot.dlist_word_count++] = word;
      }
    }
  }

  ProbeInfo info = {};
  const bool usable = AnalyzeProbeSnapshot(snapshot, &info);
  g_probe_snapshot = snapshot;
  g_probe_info = info;
  g_probe_usable = usable;



  g_port = &kHdmiPorts[info.selected_pixel_valve == 4 ? 1 : 0];
  printf("boot: pi4kms probe hdmi port requested=%u selected=%u "
         "pv2_on=%u pv4_on=%u%s\r\n",
         g_requested_port, g_port == &kHdmiPorts[1] ? 1U : 0U,
         info.hdmi0_enabled ? 1U : 0U, info.hdmi1_enabled ? 1U : 0U,
         g_requested_port == 1U && g_port != &kHdmiPorts[1] ?
             " hdmi1=off-at-boot fallback=hdmi0" : "");
  printf("boot: pi4kms probe hvs control=0x%08x status=0x%08x "
         "identity=0x%08x output2_mux=0x%08x output4_mux=0x%08x "
         "output5_mux=0x%08x enabled=%u\r\n",
         static_cast<unsigned>(snapshot.hvs_control),
         static_cast<unsigned>(snapshot.hvs_status),
         static_cast<unsigned>(snapshot.hvs_identity),
         static_cast<unsigned>(snapshot.hvs_output2_mux),
         static_cast<unsigned>(snapshot.hvs_output4_mux),
         static_cast<unsigned>(snapshot.hvs_output5_mux),
         info.hvs_enabled ? 1U : 0U);
  printf("boot: pi4kms probe hdmi pv2_control=0x%08x "
         "pv4_control=0x%08x selected_pv=%d channel=%d route_valid=%u\r\n",
         static_cast<unsigned>(snapshot.pixel_valve2.control),
         static_cast<unsigned>(snapshot.pixel_valve4.control),
         static_cast<int>(info.selected_pixel_valve),
         static_cast<int>(info.selected_channel), info.route_valid ? 1U : 0U);
  printf("boot: pi4kms probe channel enabled=%u size=%ux%u mode=%s "
         "line=%u pending_dlist=%u active_dlist=%u pointer_valid=%u\r\n",
         info.channel_enabled ? 1U : 0U,
         static_cast<unsigned>(info.width),
         static_cast<unsigned>(info.height), ModeName(info.mode),
         static_cast<unsigned>(info.line),
         static_cast<unsigned>(info.pending_dlist),
         static_cast<unsigned>(info.active_dlist),
         info.dlist_pointer_valid ? 1U : 0U);
  printf("boot: pi4kms probe dlist captured=%u used=%u planes=%u "
         "end=%u hash=0x%08x "
         "first=0x%08x status=%s takeover=off dispmanx-active=1\r\n",
         static_cast<unsigned>(snapshot.dlist_word_count),
         static_cast<unsigned>(info.dlist_used_words),
         static_cast<unsigned>(info.dlist_plane_count),
         info.dlist_end_found ? 1U : 0U,
         static_cast<unsigned>(info.dlist_hash),
         snapshot.dlist_word_count != 0U ?
             static_cast<unsigned>(snapshot.dlist_words[0]) : 0U,
         usable ? "firmware-scanout-readable" : "unsupported-state");
  if (usable && !g_requested_composite) {
    DisplayPipelineSnapshot pipeline = {};
    if (CaptureDisplayPipeline(&pipeline)) {
      LogDisplayPipeline("probe-read-only", pipeline);
      LogFirmwareClockRate(
          "probe-read-only", "pixel", kFirmwareClockPixel);
      LogFirmwareClockRate(
          "probe-read-only", "m2mc", kFirmwareClockM2mc);
      LogFirmwareClockRate(
          "probe-read-only", "pixel-bvb", kFirmwareClockPixelBvb);
      LogFirmwareClockRate(
          "probe-read-only", "display", kFirmwareClockDisplay);
    }
  }
  return usable;
}

bool ConfigureNativeMode(unsigned hdmi_group, unsigned hdmi_mode,
                         const char *hdmi_timings,
                         const char *named_mode) {
  g_native_mode_plan = {};
  g_native_mode_configured = false;
  g_native_mode_active = false;
  g_native_mode_failed = false;

  Mode mode = {};
  if (!ResolveBmxMode(hdmi_group, hdmi_mode, hdmi_timings,
                      named_mode, &mode)) {
    printf("boot: pi4kms native mode status=deferred reason=unresolved "
           "group=%u mode=%u\r\n",
           hdmi_group, hdmi_mode);
    return false;
  }
  return ConfigureResolvedNativeMode(
      mode,
      named_mode != nullptr && *named_mode != '\0' ? "named" :
          hdmi_group == 2U && hdmi_mode == 87U ? "custom" : "cea",
      hdmi_group, hdmi_mode);
}

bool ConfigureNativeMode(const Mode &mode) {
  return ConfigureResolvedNativeMode(mode, "resolved", 0U, 0U);
}

void ConfigureTakeover(bool requested) {
  g_takeover_requested = requested;
  g_takeover_active = false;
  g_takeover_failed = false;
  g_native_mode_active = false;
  g_native_mode_failed = false;
  g_present_sequence = 0U;
  g_recovery_lifecycle.Reset();
  g_submitted_slot = 0U;
  ClearDlistTemplates();
  ClearFirmwareRecoverySnapshot();
  if (requested && NativeModeChangesGeometry() &&
      (!CaptureDisplayPipeline(&g_firmware_pipeline) ||
       !CaptureFirmwareClockRates())) {
    g_takeover_failed = true;
    printf("boot: pi4kms takeover status=disabled "
           "reason=firmware-clock-recovery-unavailable\r\n");
  }
  printf("boot: pi4kms takeover requested=%u probe_usable=%u "
         "mode=%s-display-done-double-dlist slots=%u,%u "
         "writes=deferred\r\n",
         requested ? 1U : 0U, g_probe_usable ? 1U : 0U,
         g_native_mode_configured ? "native" : "firmware",
         static_cast<unsigned>(kTakeoverDlistSlots[0]),
         static_cast<unsigned>(kTakeoverDlistSlots[1]));
}

bool GetPlannedDisplaySize(uint32_t *width, uint32_t *height) {
  if (width == nullptr || height == nullptr || !g_takeover_requested ||
      !g_probe_usable || g_takeover_failed) {
    return false;
  }
  if (g_native_mode_configured && !g_native_mode_failed) {
    *width = g_native_mode_plan.mode.width;
    *height = g_native_mode_plan.mode.height;
  } else {
    *width = g_probe_info.width;
    *height = g_probe_info.height;


    if (g_requested_composite && g_composite_progressivo &&
        g_composite_righe != 0U) {
      *height = g_composite_righe;
    }
  }
  return *width != 0U && *height != 0U;
}

bool TakeoverReady() {
  return g_takeover_requested && g_probe_usable && !g_takeover_failed &&
         g_probe_info.route_valid && g_probe_info.selected_channel >= 0 &&
         SelectedPixelValveBase() != 0U &&
         TakeoverSlotSafe(kTakeoverDlistSlots[0]) &&
         TakeoverSlotSafe(kTakeoverDlistSlots[1]);
}

bool TakeoverActive() {
  return g_takeover_active;
}

bool FirmwareDisplayClaimed() {
  return g_recovery_lifecycle.FirmwareDisplayClaimed();
}

bool NativeScanoutCommitted() {
  return g_recovery_lifecycle.NativeScanoutCommitted();
}

#if BMX_V3D_RENDER_TEST_KERNEL
bool ReconfigureCommittedNativeMode(const Mode &mode) {
  if (!g_recovery_lifecycle.NativeScanoutCommitted() ||
      !g_takeover_active || g_takeover_failed ||
      !g_firmware_pipeline.valid || !g_firmware_packet_ram_saved) {
    printf("boot: pi4kms native mode switch status=fail "
           "reason=runtime-state\r\n");
    return false;
  }

  ModeRegisterPlan plan = {};
  if (!BuildModeRegisterPlan(mode, &plan,
                             g_port == &kHdmiPorts[1] ? 1U : 0U)) {
    printf("boot: pi4kms native mode switch status=fail "
           "reason=invalid size=%ux%u\r\n",
           static_cast<unsigned>(mode.width),
           static_cast<unsigned>(mode.height));
    return false;
  }
  if (!SynchronizePreviousPresent(true)) {
    printf("boot: pi4kms native mode switch status=fail "
           "reason=present-fence\r\n");
    return false;
  }

  const uint32_t channel =
      static_cast<uint32_t>(g_probe_info.selected_channel);
  const uint32_t background = ReadRegister(
      kHvsBase, kHvsChannelBackground0 + channel * kHvsChannelStride) &
      ~(kHvs5BackgroundBackToBack | kHvsBackgroundInterlace |
        kHvsBackgroundGamma);
  if (!RestartDisplayPipeline(plan.hvs_channel_control, background,
                              "arm-native-mode-switch", &plan) ||
      !WaitForActiveDlist(kTakeoverDlistSlots[g_submitted_slot], true)) {
    g_takeover_failed = true;
    printf("boot: pi4kms native mode switch status=fail "
           "reason=pipeline-restart size=%ux%u\r\n",
           static_cast<unsigned>(mode.width),
           static_cast<unsigned>(mode.height));
    return false;
  }

  g_native_mode_plan = plan;
  g_native_mode_configured = true;
  g_native_mode_active = true;
  g_native_mode_failed = false;
  printf("boot: pi4kms native mode switch status=pass size=%ux%u "
         "pclk=%u totals=%ux%u ownership=arm\r\n",
         static_cast<unsigned>(mode.width),
         static_cast<unsigned>(mode.height),
         static_cast<unsigned>(mode.pixel_clock),
         static_cast<unsigned>(plan.horizontal_total),
         static_cast<unsigned>(plan.vertical_total));
  return true;
}
#endif



uint32_t CanaleLeggi(uint32_t base) {
  return ReadRegister(kHvsBase, base +
      static_cast<uint32_t>(g_probe_info.selected_channel) * kHvsChannelStride);
}

void CanaleScrivi(uint32_t base, uint32_t value) {
  DataSyncBarrier();
  write32(kHvsBase + base +
          static_cast<uint32_t>(g_probe_info.selected_channel) *
              kHvsChannelStride, value);
  DataSyncBarrier();
}



void CompositeScriviCanale(int variante, uint32_t control,
                           uint32_t background) {
  if (variante == 2) {
    CanaleScrivi(kHvsChannelControl0, 0U);
    CanaleScrivi(kHvsChannelControl0, kHvs5ChannelReset);
    CanaleScrivi(kHvsChannelControl0, 0U);
  }
  CanaleScrivi(kHvsChannelControl0, control);
  CanaleScrivi(kHvsChannelBackground0, background);
}


void CompositeRimettiCanale(int variante) {
  if (variante == 2) {
    CanaleScrivi(kHvsChannelControl0, 0U);
    CanaleScrivi(kHvsChannelControl0, kHvs5ChannelReset);
    CanaleScrivi(kHvsChannelControl0, 0U);
  }
  CanaleScrivi(kHvsChannelControl0, g_composite_fw_control);
  CanaleScrivi(kHvsChannelBackground0, g_composite_fw_background);
}












static bool AttendiListaComposito(uint32_t expected) {
  const uint64_t start = CTimer::GetClockTicks64();
  do {


    if (SelectedChannelDlist(kHvsPendingDlist0) != expected) {
      return false;
    }
    if (SelectedChannelDlist(kHvsActiveDlist0) == expected) {
      return true;
    }
  } while (CTimer::GetClockTicks64() - start < kDlistSwitchTimeoutUs);
  return SelectedChannelDlist(kHvsActiveDlist0) == expected;
}

static void CompositeSincronizza(uint32_t previous_slot) {
  const bool nostra = AttendiListaComposito(previous_slot);
  if (!nostra) {
    if (++g_composite_reasserts <= 8U ||
        (g_composite_reasserts & 1023U) == 0U) {
      printf("boot: pi4kms composito lista ripresa dal firmware volte=%u "
             "slot=%u pending=%u active=%u\r\n",
             static_cast<unsigned>(g_composite_reasserts),
             static_cast<unsigned>(previous_slot),
             static_cast<unsigned>(SelectedChannelDlist(kHvsPendingDlist0)),
             static_cast<unsigned>(SelectedChannelDlist(kHvsActiveDlist0)));
    }
    WritePendingDlist(previous_slot);
  }
  g_composite_storia = (g_composite_storia << 1U) | (nostra ? 0U : 1U);
  if (!g_composite_arreso &&
      static_cast<unsigned>(__builtin_popcountll(g_composite_storia)) >=
          kCompositeRiprese) {
    g_composite_arreso = true;
    printf("boot: pi4kms composito resa: il firmware si e' ripreso la "
           "lista %u volte negli ultimi 64 fotogrammi (%u in tutto)\r\n",
           kCompositeRiprese, static_cast<unsigned>(g_composite_reasserts));
  }
}

bool SynchronizePreviousPresent(bool wait_for_vblank) {
  if (!wait_for_vblank ||
      !g_recovery_lifecycle.NativeScanoutCommitted()) {
    return true;
  }



  if (g_requested_composite && g_composite_progressivo) {
    const uint32_t c = CanaleLeggi(kHvsChannelControl0);
    const uint32_t b = CanaleLeggi(kHvsChannelBackground0);
    if (c != g_composite_control || b != g_composite_background) {
      if (++g_composite_canale_rimesso <= 8U ||
          (g_composite_canale_rimesso & 1023U) == 0U) {
        printf("boot: pi4kms composito canale ripreso dal firmware volte=%u "
               "control=0x%08x background=0x%08x\r\n",
               static_cast<unsigned>(g_composite_canale_rimesso),
               static_cast<unsigned>(c), static_cast<unsigned>(b));
      }
      CanaleScrivi(kHvsChannelControl0, g_composite_control);
      CanaleScrivi(kHvsChannelBackground0, g_composite_background);
    }
  }

  const uint32_t previous_slot =
      kTakeoverDlistSlots[g_submitted_slot];
  if (g_requested_composite) {


    CompositeSincronizza(previous_slot);
  } else if (!WaitForActiveDlist(previous_slot, true)) {
    printf("boot: pi4kms present status=fail phase=previous-active "
           "slot=%u pending=%u active=%u\r\n",
           static_cast<unsigned>(previous_slot),
           static_cast<unsigned>(SelectedChannelDlist(kHvsPendingDlist0)),
           static_cast<unsigned>(SelectedChannelDlist(kHvsActiveDlist0)));
    g_takeover_failed = true;
    return false;
  }

  static bool scheduling_logged = false;
  if (!scheduling_logged) {
    scheduling_logged = true;
    printf("boot: pi4kms present scheduling=wait-previous-before-render "
           "submit=without-post-wait\r\n");
  }
  return true;
}

bool PresentPlanes(const Plane *planes, uint32_t plane_count,
                   uint32_t display_width, uint32_t display_height,
                   bool wait_for_vblank) {
  uint32_t planned_width = 0U;
  uint32_t planned_height = 0U;
  if (!TakeoverReady() || planes == nullptr || plane_count == 0U ||
      plane_count > kHvs5MaximumPlanes ||
      !GetPlannedDisplaySize(&planned_width, &planned_height) ||
      display_width != planned_width || display_height != planned_height) {
    return false;
  }

  const uint32_t slot_index =
      g_takeover_active ? (g_submitted_slot ^ 1U) : 0U;
  DlistTemplate *dlist_template = &g_dlist_templates[slot_index];
  const bool cache_hit = MatchesDlistTemplate(
      *dlist_template, planes, plane_count, display_width, display_height);
  if (cache_hit) {
    PatchDlistAddresses(dlist_template, planes);
    if (!g_dlist_cache_hit_logged[slot_index]) {
      g_dlist_cache_hit_logged[slot_index] = true;
      printf("boot: pi4kms dlist template slot=%u status=hit words=%u "
             "planes=%u\r\n", static_cast<unsigned>(slot_index),
             static_cast<unsigned>(dlist_template->word_count),
             static_cast<unsigned>(plane_count));
    }
  } else {
    Hvs5Plane hvs_planes[kHvs5MaximumPlanes] = {};
    for (uint32_t i = 0U; i < plane_count; ++i) {
      if (planes[i].format != kPlaneFormatRgb565 &&
          planes[i].format != kPlaneFormatArgb8888) {
        return false;
      }
      hvs_planes[i].framebuffer_bus_address =
          planes[i].framebuffer_bus_address;
      hvs_planes[i].pitch = planes[i].pitch;
      hvs_planes[i].width = planes[i].width;
      hvs_planes[i].height = planes[i].height;
      hvs_planes[i].format = planes[i].format == kPlaneFormatArgb8888
          ? kHvs5PixelFormatArgb8888 : kHvs5PixelFormatRgb565;
      if (planes[i].filter != kScaleFilterNearest &&
          planes[i].filter != kScaleFilterMitchell) {
        return false;
      }
      hvs_planes[i].filter = planes[i].filter == kScaleFilterMitchell
          ? kHvs5ScaleFilterMitchell : kHvs5ScaleFilterNearest;
      hvs_planes[i].destination_x = planes[i].destination_x;
      hvs_planes[i].destination_y = planes[i].destination_y;
      hvs_planes[i].destination_width = planes[i].destination_width;
      hvs_planes[i].destination_height = planes[i].destination_height;
    }
    dlist_template->valid = false;
    if (!BuildHvs5Dlist(
            hvs_planes, plane_count, display_width, display_height,
            dlist_template->words, kHvs5MaximumArmDlistWords,
            &dlist_template->word_count,
            &dlist_template->required_kernels)) {
      return false;
    }
    uint32_t offset = 0U;
    for (uint32_t i = 0U; i < plane_count; ++i) {
      const bool unity = planes[i].width == planes[i].destination_width &&
                         planes[i].height == planes[i].destination_height;
      dlist_template->framebuffer_words[i] = offset + (unity ? 5U : 6U);
      const uint32_t plane_words =
          (dlist_template->words[offset] >> 24U) & 0x3fU;
      if (plane_words == 0U ||
          dlist_template->framebuffer_words[i] >= offset + plane_words) {
        return false;
      }
      offset += plane_words;
      dlist_template->planes[i] = planes[i];
    }
    dlist_template->plane_count = plane_count;
    dlist_template->display_width = display_width;
    dlist_template->display_height = display_height;
    dlist_template->valid = true;
  }
  const uint32_t *words = dlist_template->words;
  const uint32_t word_count = dlist_template->word_count;
  const Hvs5FilterKernelUsage &required_kernels =
      dlist_template->required_kernels;
  if (!g_recovery_lifecycle.NativeScanoutCommitted() &&
      !g_firmware_dlist_saved && !CaptureActiveFirmwareDlist()) {
    return false;
  }


  if (!g_requested_composite &&
      !g_recovery_lifecycle.FirmwareDisplayClaimed()) {
    if (!NotifyFirmwareDisplayDone()) {
      g_takeover_failed = true;
      return false;
    }
    // The firmware parks the selected HVS channel at EOF as part of the
    // ownership transfer.  There is no subsequent firmware frame boundary to
    // wait for; the ARM can install its first list immediately.
  }

  const bool verify_first_native_present =
      !g_recovery_lifecycle.NativeScanoutCommitted();
  if (!SynchronizePreviousPresent(wait_for_vblank)) {
    return false;
  }

  const uint32_t slot = kTakeoverDlistSlots[slot_index];
  if (!ArmDlistRegionSafe(slot, word_count)) {
    printf("boot: pi4kms takeover status=fail phase=dlist slot=%u\r\n",
           static_cast<unsigned>(slot));
    g_takeover_failed = true;
    return false;
  }
  if (required_kernels.mitchell &&
      !UploadHvs5FilterKernel(kHvs5MitchellKernelSlot, "mitchell",
                              BuildHvs5MitchellKernel)) {
    g_takeover_failed = true;
    return false;
  }
  if (required_kernels.nearest &&
      !UploadHvs5FilterKernel(kHvs5NearestKernelSlot, "nearest",
                              BuildHvs5NearestKernel)) {
    g_takeover_failed = true;
    return false;
  }
  bool dlist_written = false;
  uint32_t write_attempt = 0U;
  for (; write_attempt < kDlistWriteAttempts; ++write_attempt) {
    if (WriteAndVerifyDlist(
            slot, words, word_count, kVerifyCustom)) {
      dlist_written = true;
      break;
    }
    if (write_attempt + 1U < kDlistWriteAttempts) {
      CTimer::SimpleusDelay(10U);
    }
  }
  if (!dlist_written) {
    printf("boot: pi4kms takeover status=fail phase=dlist slot=%u "
           "attempts=%u\r\n",
           static_cast<unsigned>(slot),
           static_cast<unsigned>(kDlistWriteAttempts));
    g_takeover_failed = true;
    return false;
  }
  if (write_attempt != 0U) {
    printf("boot: pi4kms dlist retry status=recovered slot=%u "
           "attempt=%u\r\n",
           static_cast<unsigned>(slot),
           static_cast<unsigned>(write_attempt + 1U));
  }



  if (!g_takeover_active && g_requested_composite &&
      g_composite_progressivo) {
    CompositeScriviCanale(g_composite_variante, g_composite_control,
                          g_composite_background);
    printf("boot: pi4kms composito canale progressivo variante=%d "
           "control=0x%08x background=0x%08x righe=%u\r\n",
           g_composite_variante, static_cast<unsigned>(g_composite_control),
           static_cast<unsigned>(g_composite_background),
           static_cast<unsigned>(g_composite_righe));
  }
  WritePendingDlist(slot);


  if (!g_takeover_active && g_requested_composite) {
    printf("boot: pi4kms composito presa lista=%u parole=%u piani=%u "
           "canale=%ux%u (firmware acceso, solo la lista)\r\n",
           static_cast<unsigned>(slot), static_cast<unsigned>(word_count),
           static_cast<unsigned>(plane_count),
           static_cast<unsigned>(display_width),
           static_cast<unsigned>(display_height));
  }
  if (!g_takeover_active && !g_requested_composite) {
    const ModeRegisterPlan *native_plan =
        g_native_mode_configured && !g_native_mode_failed ?
            &g_native_mode_plan : nullptr;
    const uint32_t arm_control = native_plan != nullptr ?
        native_plan->hvs_channel_control :
        kHvs5ChannelEnable |
            ((display_width & kHvs5ChannelDimensionMask) <<
             kHvs5ChannelWidthShift) |
            (display_height & kHvs5ChannelDimensionMask);
    const uint32_t arm_background = g_firmware_channel_background &
        ~(kHvs5BackgroundBackToBack | kHvsBackgroundInterlace |
          kHvsBackgroundGamma);
    bool restarted = RestartDisplayPipeline(
        arm_control, arm_background,
        native_plan != nullptr ? "arm-native-mode" : "arm-firmware-mode",
        native_plan);
    if (!restarted && native_plan != nullptr) {
      g_native_mode_failed = true;
      const bool geometry_changed =
          native_plan->mode.width != g_probe_info.width ||
          native_plan->mode.height != g_probe_info.height;
      printf("boot: pi4kms native mode status=fail fallback=%s\r\n",
             geometry_changed ? "firmware-static-pending" :
                                "firmware-replay");
      if (geometry_changed) {
        // The submitted ARM list and its output layout belong to the new
        // geometry.  It must never be replayed with the old firmware timing.
        // The caller restores the captured firmware list, registers, and
        // clocks through RestoreFirmwareScanout().
        g_takeover_failed = true;
        return false;
      }
      restarted = (!NativeModeChangesGeometry() ||
                   RestoreFirmwareClockRates()) &&
          RestartDisplayPipeline(
              arm_control, arm_background,
              "arm-firmware-fallback", nullptr);
    }
    if (!restarted) {
      printf("boot: pi4kms takeover status=fail phase=pipeline-restart\r\n");
      g_takeover_failed = true;
      return false;
    }
    g_native_mode_active = native_plan != nullptr && !g_native_mode_failed;
  }
  // The ownership transition is committed only after the first ARM list is
  // observed in the active HVS slot.  Steady-state presentation instead
  // fences the previous submission before rendering, then queues this list
  // without a second wait, matching the Pi5 pipeline.
  if (verify_first_native_present && !WaitForActiveDlist(slot, true) &&
      !(g_requested_composite && WaitForActiveDlist(slot, true))) {
    printf("boot: pi4kms takeover status=fail phase=activate "
           "slot=%u pending=%u active=%u\r\n",
           static_cast<unsigned>(slot),
           static_cast<unsigned>(SelectedChannelDlist(kHvsPendingDlist0)),
           static_cast<unsigned>(SelectedChannelDlist(kHvsActiveDlist0)));



    if (g_requested_composite && g_composite_progressivo) {
      WritePendingDlist(g_firmware_dlist_pointer);
      const bool lista = WaitForActiveDlist(g_firmware_dlist_pointer, true);
      CompositeRimettiCanale(g_composite_variante);
      WritePendingDlist(g_composite_fw_pending);
      printf("boot: pi4kms composito canale del firmware rimesso lista=%u\r\n",
             lista ? 1U : 0U);
    }
    g_takeover_failed = true;
    return false;
  }

  g_takeover_active = true;
  g_submitted_slot = slot_index;
  ++g_present_sequence;



  if (g_requested_composite &&
      !g_recovery_lifecycle.FirmwareDisplayClaimed()) {
    g_recovery_lifecycle.MarkFirmwareDisplayClaimed();
  }
  if (g_recovery_lifecycle.CommitNativePresent()) {
    const uint32_t firmware_dlist = g_firmware_dlist_pointer;
    const uint32_t firmware_words = g_firmware_dlist_word_count;
#if BMX_V3D_RENDER_TEST_KERNEL
    RetireFirmwareRecoverySnapshot();
#else
    ClearFirmwareRecoverySnapshot();
#endif
    printf("boot: pi4kms native commit sequence=%u slot=%u "
           "firmware_dlist=%u words=%u recovery=retired "
           "late_failure=hold-last-frame\r\n",
           static_cast<unsigned>(g_present_sequence),
           static_cast<unsigned>(slot),
           static_cast<unsigned>(firmware_dlist),
           static_cast<unsigned>(firmware_words));
  }
  if (g_present_sequence <= 3U) {
    printf("boot: pi4kms present sequence=%u slot=%u dlist=%u words=%u "
           "planes=%u fb=0x%08x pitch=%u size=%ux%u dst=%u,%u/%ux%u "
           "display=%ux%u ownership=arm switch=pending-dlist\r\n",
           static_cast<unsigned>(g_present_sequence),
           static_cast<unsigned>(slot_index),
           static_cast<unsigned>(slot),
           static_cast<unsigned>(word_count),
           static_cast<unsigned>(plane_count),
           static_cast<unsigned>(planes[0].framebuffer_bus_address),
           static_cast<unsigned>(planes[0].pitch),
           static_cast<unsigned>(planes[0].width),
           static_cast<unsigned>(planes[0].height),
           static_cast<unsigned>(planes[0].destination_x),
           static_cast<unsigned>(planes[0].destination_y),
           static_cast<unsigned>(planes[0].destination_width),
           static_cast<unsigned>(planes[0].destination_height),
           static_cast<unsigned>(display_width),
           static_cast<unsigned>(display_height));
  }
  return true;
}

bool RestoreFirmwareScanout(bool wait_for_vblank) {
  if (g_recovery_lifecycle.NativeScanoutCommitted()) {
    static bool retired_restore_logged = false;
    if (!retired_restore_logged) {
      retired_restore_logged = true;
      printf("boot: pi4kms restore status=unavailable "
             "reason=native-committed action=hold-last-frame\r\n");
    }
    return false;
  }
  if (!g_takeover_active &&
      !g_recovery_lifecycle.FirmwareDisplayClaimed()) {
    return true;
  }
  if (!g_probe_usable || !g_firmware_dlist_saved ||
      g_firmware_dlist_word_count == 0U) {
    return false;
  }
  const bool clocks_restored =
      (!NativeModeChangesGeometry() ||
       (!g_native_mode_active && !g_native_mode_failed)) ||
      RestoreFirmwareClockRates();
  const bool list_restored = clocks_restored && WriteAndVerifyDlist(
      g_firmware_dlist_pointer, g_firmware_dlist_words,
      g_firmware_dlist_word_count, kVerifyFirmwareControls);
  if (list_restored) {
    WritePendingDlist(g_firmware_dlist_pointer);
  }


  if (list_restored && g_requested_composite && g_composite_progressivo) {
    (void)WaitForActiveDlist(g_firmware_dlist_pointer, true);
    CompositeRimettiCanale(g_composite_variante);
    WritePendingDlist(g_composite_fw_pending);
  }
  const bool channel_restored = list_restored &&
      (!g_recovery_lifecycle.FirmwareDisplayClaimed() ||
       g_requested_composite ||
       RestartDisplayPipeline(
          g_firmware_channel_control, g_firmware_channel_background,
          "firmware-static", nullptr));
  const bool restored = channel_restored && WaitForActiveDlist(
      g_firmware_dlist_pointer, wait_for_vblank);
  g_takeover_active = false;
  g_native_mode_active = false;
  printf("boot: pi4kms restore firmware_dlist=%u status=%s "
         "firmware_display=%s clocks=%s\r\n",
         static_cast<unsigned>(g_firmware_dlist_pointer),
         restored ? "pass" : "fail",
         g_recovery_lifecycle.FirmwareDisplayClaimed() ? "stopped" : "active",
         clocks_restored ? "restored" : "fail");
  if (!restored) {
    g_takeover_failed = true;
  }
  return restored;
}


void RequestHdmiPort(unsigned port) {
  g_requested_port = port == 1U ? 1U : 0U;
}

unsigned ActiveHdmiPort() {
  return g_port == &kHdmiPorts[1] ? 1U : 0U;
}






void RequestComposite(bool composite) {
  g_requested_composite = composite;
  g_composite_reasserts = 0U;
  g_composite_progressivo = false;
  g_composite_variante = -1;
  g_composite_righe = 0U;
  g_composite_canale_rimesso = 0U;
  g_composite_storia = 0U;
  g_composite_arreso = false;
}




bool CompositeArreso() {
  return g_requested_composite && g_composite_arreso;
}




void CompositeLascia() {
  g_takeover_failed = true;
  g_takeover_active = false;
  if (g_composite_progressivo) {
    CompositeRimettiCanale(g_composite_variante);
    WritePendingDlist(g_composite_fw_pending);
    g_composite_progressivo = false;
  }
  printf("boot: pi4kms composito scalatore lasciato al firmware "
         "(riprese %u)\r\n", static_cast<unsigned>(g_composite_reasserts));
}

void LogCompositeState(const char *phase) {
  const uint32_t hvs_control = ReadRegister(kHvsBase, kHvsControl);
  printf("boot: pi4kms composito phase=%s hvs control=0x%08x status=0x%08x "
         "identity=0x%08x mux2=0x%08x mux4=0x%08x mux5=0x%08x\r\n",
         phase, static_cast<unsigned>(hvs_control),
         static_cast<unsigned>(ReadRegister(kHvsBase, kHvsStatus)),
         static_cast<unsigned>(ReadRegister(kHvsBase, kHvsIdentity)),
         static_cast<unsigned>(ReadRegister(kHvsBase, kHvsOutput2Mux)),
         static_cast<unsigned>(ReadRegister(kHvsBase, kHvsOutput4Mux)),
         static_cast<unsigned>(ReadRegister(kHvsBase, kHvsOutput5Mux)));
  if ((hvs_control & kHvsControlEnable) == 0U) {
    printf("boot: pi4kms composito phase=%s hvs spento: niente PixelValve 3 "
           "ne' VEC\r\n", phase);
    return;
  }
  for (uint32_t channel = 0U; channel < kHvsChannelCount; ++channel) {
    const uint32_t control = ReadRegister(
        kHvsBase, kHvsChannelControl0 + channel * kHvsChannelStride);
    printf("boot: pi4kms composito phase=%s canale %u control=0x%08x "
           "(%ux%u) background=0x%08x status=0x%08x pending=%u active=%u "
           "fifo=0x%08x\r\n",
           phase, static_cast<unsigned>(channel),
           static_cast<unsigned>(control),
           static_cast<unsigned>((control >> kHvs5ChannelWidthShift) &
                                 kHvs5ChannelDimensionMask),
           static_cast<unsigned>(control & kHvs5ChannelDimensionMask),
           static_cast<unsigned>(ReadRegister(
               kHvsBase, kHvsChannelBackground0 +
               channel * kHvsChannelStride)),
           static_cast<unsigned>(ReadRegister(
               kHvsBase, kHvsChannelStatus0 + channel * kHvsChannelStride)),
           static_cast<unsigned>(ReadRegister(
               kHvsBase, kHvsPendingDlist0 + channel * sizeof(uint32_t))),
           static_cast<unsigned>(ReadRegister(
               kHvsBase, kHvsActiveDlist0 + channel * sizeof(uint32_t))),
           static_cast<unsigned>(ReadRegister(
               kHvsBase, kHvsChannelFifoBase0 +
               channel * kHvsChannelStride)));
  }
  printf("boot: pi4kms composito phase=%s pv3 control=0x%08x "
         "vcontrol=0x%08x h=0x%08x/0x%08x v=0x%08x/0x%08x int=0x%08x "
         "status=0x%08x mux=0x%08x\r\n",
         phase,
         static_cast<unsigned>(ReadRegister(
             kPixelValve3Base, kPixelValveControl)),
         static_cast<unsigned>(ReadRegister(
             kPixelValve3Base, kPixelValveVerticalControl)),
         static_cast<unsigned>(ReadRegister(
             kPixelValve3Base, kPixelValveHorizontalA)),
         static_cast<unsigned>(ReadRegister(
             kPixelValve3Base, kPixelValveHorizontalB)),
         static_cast<unsigned>(ReadRegister(
             kPixelValve3Base, kPixelValveVerticalA)),
         static_cast<unsigned>(ReadRegister(
             kPixelValve3Base, kPixelValveVerticalB)),
         static_cast<unsigned>(ReadRegister(
             kPixelValve3Base, kPixelValveInterruptStatus)),
         static_cast<unsigned>(ReadRegister(
             kPixelValve3Base, kPixelValveStatus)),
         static_cast<unsigned>(ReadRegister(
             kPixelValve3Base, kPixelValveMuxConfig)));

  printf("boot: pi4kms composito phase=%s vec revid=0x%08x config0=0x%08x "
         "config1=0x%08x config2=0x%08x config3=0x%08x cfg=0x%08x "
         "schph=0x%08x freq=0x%08x/0x%08x clmp=0x%08x/0x%08x\r\n",
         phase,
         static_cast<unsigned>(ReadRegister(kVecBase, 0x100U)),
         static_cast<unsigned>(ReadRegister(kVecBase, 0x104U)),
         static_cast<unsigned>(ReadRegister(kVecBase, 0x188U)),
         static_cast<unsigned>(ReadRegister(kVecBase, 0x18cU)),
         static_cast<unsigned>(ReadRegister(kVecBase, 0x1a0U)),
         static_cast<unsigned>(ReadRegister(kVecBase, 0x208U)),
         static_cast<unsigned>(ReadRegister(kVecBase, 0x108U)),
         static_cast<unsigned>(ReadRegister(kVecBase, 0x180U)),
         static_cast<unsigned>(ReadRegister(kVecBase, 0x184U)),
         static_cast<unsigned>(ReadRegister(kVecBase, 0x144U)),
         static_cast<unsigned>(ReadRegister(kVecBase, 0x148U)));
  printf("boot: pi4kms composito phase=%s vec dac_config=0x%08x "
         "dac_misc=0x%08x dac_test=0x%08x status0=0x%08x mask0=0x%08x "
         "int=0x%08x/0x%08x wse=0x%08x/0x%08x/0x%08x\r\n",
         phase,
         static_cast<unsigned>(ReadRegister(kVecBase, 0x210U)),
         static_cast<unsigned>(ReadRegister(kVecBase, 0x214U)),
         static_cast<unsigned>(ReadRegister(kVecBase, 0x20cU)),
         static_cast<unsigned>(ReadRegister(kVecBase, 0x200U)),
         static_cast<unsigned>(ReadRegister(kVecBase, 0x204U)),
         static_cast<unsigned>(ReadRegister(kVecBase, 0x190U)),
         static_cast<unsigned>(ReadRegister(kVecBase, 0x194U)),
         static_cast<unsigned>(ReadRegister(kVecBase, 0x0c4U)),
         static_cast<unsigned>(ReadRegister(kVecBase, 0x0c8U)),
         static_cast<unsigned>(ReadRegister(kVecBase, 0x0d0U)));
  const uint32_t active = ReadRegister(
      kHvsBase, kHvsActiveDlist0 + 1U * sizeof(uint32_t));
  for (uint32_t base = 0U;
       base < 32U && active + base < kHvs5DlistWordCapacity; base += 8U) {
    uint32_t w[8] = {};
    for (uint32_t i = 0U; i < 8U; ++i) {
      if (active + base + i < kHvs5DlistWordCapacity) {
        w[i] = ReadRegister(kHvsBase, kHvs5DlistStart +
                            (active + base + i) * sizeof(uint32_t));
      }
    }
    printf("boot: pi4kms composito phase=%s lista %u +%02u: %08x %08x %08x "
           "%08x %08x %08x %08x %08x\r\n",
           phase, static_cast<unsigned>(active), static_cast<unsigned>(base),
           static_cast<unsigned>(w[0]), static_cast<unsigned>(w[1]),
           static_cast<unsigned>(w[2]), static_cast<unsigned>(w[3]),
           static_cast<unsigned>(w[4]), static_cast<unsigned>(w[5]),
           static_cast<unsigned>(w[6]), static_cast<unsigned>(w[7]));
  }
}






void CompositeCampioni(const char *quando) {
  uint32_t modi[4] = {};
  uint32_t riga_min = 0xffffffffU;
  uint32_t riga_max = 0U;
  uint32_t quadri = 0U;
  uint32_t cambi_lista = 0U;
  uint32_t n = 0U;
  uint32_t p_min = 0xffffffffU, p_max = 0U, a_min = 0xffffffffU, a_max = 0U;
  uint32_t fr_prima = (CanaleLeggi(kHvsChannelStatus0) >> 14U) & 0x3fU;
  uint32_t a_ultimo = SelectedChannelDlist(kHvsActiveDlist0);
  const uint64_t start = CTimer::GetClockTicks64();
  while (CTimer::GetClockTicks64() - start < 100000U) {
    const uint32_t st = CanaleLeggi(kHvsChannelStatus0);
    const uint32_t p = SelectedChannelDlist(kHvsPendingDlist0);
    const uint32_t a = SelectedChannelDlist(kHvsActiveDlist0);
    const uint32_t riga = st & 0xfffU;
    const uint32_t fr = (st >> 14U) & 0x3fU;
    ++modi[st >> 30U];
    riga_min = riga < riga_min ? riga : riga_min;
    riga_max = riga > riga_max ? riga : riga_max;
    if (fr != fr_prima) {
      ++quadri;
      fr_prima = fr;
    }
    if (a != a_ultimo) {
      ++cambi_lista;
      a_ultimo = a;
    }
    p_min = p < p_min ? p : p_min;
    p_max = p > p_max ? p : p_max;
    a_min = a < a_min ? a : a_min;
    a_max = a > a_max ? a : a_max;
    ++n;
    CTimer::SimpleusDelay(200U);
  }
  printf("boot: pi4kms composito campioni %s n=%u modi=%u/%u/%u/%u "
         "riga=%u..%u quadri=%u cambi_lista=%u pending=%u..%u "
         "active=%u..%u\r\n", quando, static_cast<unsigned>(n),
         static_cast<unsigned>(modi[0]), static_cast<unsigned>(modi[1]),
         static_cast<unsigned>(modi[2]), static_cast<unsigned>(modi[3]),
         static_cast<unsigned>(riga_min), static_cast<unsigned>(riga_max),
         static_cast<unsigned>(quadri), static_cast<unsigned>(cambi_lista),
         static_cast<unsigned>(p_min), static_cast<unsigned>(p_max),
         static_cast<unsigned>(a_min), static_cast<unsigned>(a_max));
}


void CompositeListe(const char *quando) {
  const uint32_t liste[2] = {SelectedChannelDlist(kHvsPendingDlist0),
                             SelectedChannelDlist(kHvsActiveDlist0)};
  for (uint32_t k = 0U; k < 2U; ++k) {
    for (uint32_t base = 0U; base < 16U; base += 8U) {
      uint32_t w[8] = {};
      for (uint32_t i = 0U; i < 8U; ++i) {
        if (liste[k] + base + i < kHvs5DlistWordCapacity) {
          w[i] = ReadRegister(kHvsBase, kHvs5DlistStart +
                              (liste[k] + base + i) * sizeof(uint32_t));
        }
      }
      printf("boot: pi4kms composito %s lista %s %u +%02u: %08x %08x %08x "
             "%08x %08x %08x %08x %08x\r\n", quando,
             k == 0U ? "in attesa" : "in uso", static_cast<unsigned>(liste[k]),
             static_cast<unsigned>(base),
             static_cast<unsigned>(w[0]), static_cast<unsigned>(w[1]),
             static_cast<unsigned>(w[2]), static_cast<unsigned>(w[3]),
             static_cast<unsigned>(w[4]), static_cast<unsigned>(w[5]),
             static_cast<unsigned>(w[6]), static_cast<unsigned>(w[7]));
    }
  }
}






bool CompositeProvaProgressivo(uint32_t slot) {
  const uint32_t righe =
      ReadRegister(kPixelValve3Base, kPixelValveVerticalB) & 0xffffU;
  const uint32_t fw_control = CanaleLeggi(kHvsChannelControl0);
  const uint32_t fw_background = CanaleLeggi(kHvsChannelBackground0);
  const uint32_t larghezza =
      (fw_control >> kHvs5ChannelWidthShift) & kHvs5ChannelDimensionMask;
  const uint32_t altezza = fw_control & kHvs5ChannelDimensionMask;
  if (righe == 0U || righe * 2U != altezza ||
      (fw_control & kHvs5ChannelEnable) == 0U) {
    printf("boot: pi4kms composito progressivo status=skip righe=%u "
           "control=0x%08x\r\n", static_cast<unsigned>(righe),
           static_cast<unsigned>(fw_control));
    return false;
  }
  g_composite_fw_control = fw_control;
  g_composite_fw_background = fw_background;
  for (int v = 0; v < 3; ++v) {
    uint32_t control = (fw_control & ~kHvs5ChannelDimensionMask) | righe;
    uint32_t background = fw_background & ~kHvsBackgroundInterlace;
    if (v >= 1) {
      background &= ~kHvs5BackgroundBackToBack;
    }
    if (v == 2) {
      background &= ~kHvsBackgroundGamma;
      control = kHvs5ChannelEnable | (larghezza << kHvs5ChannelWidthShift) |
                righe;
    }
    CompositeScriviCanale(v, control, background);
    WritePendingDlist(slot);
    const bool presa =
        WaitForActiveDlist(slot, true) || WaitForActiveDlist(slot, true);
    printf("boot: pi4kms composito progressivo variante=%d "
           "control=0x%08x->0x%08x background=0x%08x->0x%08x presa=%u "
           "stato=0x%08x pending=%u active=%u\r\n", v,
           static_cast<unsigned>(fw_control), static_cast<unsigned>(control),
           static_cast<unsigned>(fw_background),
           static_cast<unsigned>(background), presa ? 1U : 0U,
           static_cast<unsigned>(CanaleLeggi(kHvsChannelStatus0)),
           static_cast<unsigned>(SelectedChannelDlist(kHvsPendingDlist0)),
           static_cast<unsigned>(SelectedChannelDlist(kHvsActiveDlist0)));
    if (presa) {
      g_composite_progressivo = true;
      g_composite_variante = v;
      g_composite_righe = righe;
      g_composite_control = control;
      g_composite_background = background;
      CompositeCampioni("progressivo");
      return true;
    }
    CompositeRimettiCanale(v);
    WritePendingDlist(g_composite_fw_pending);
  }
  return false;
}

bool CompositeHoldTest(unsigned duration_ms) {
  if (!g_requested_composite || !g_probe_usable ||
      g_probe_info.selected_channel != 1 || g_takeover_active) {
    printf("boot: pi4kms composito prova status=skip usable=%u canale=%d\r\n",
           g_probe_usable ? 1U : 0U,
           static_cast<int>(g_probe_info.selected_channel));
    return false;
  }









  const uint32_t background = g_probe_snapshot.channels[1].background;
  const uint32_t pv3_vcontrol = ReadRegister(kPixelValve3Base, 0x04U);
  const uint32_t vec_config2 = ReadRegister(kVecBase, 0x18cU);
  const bool pv3_semiquadri = (pv3_vcontrol & (1U << 4U)) != 0U;
  const bool vec_progressivo = (vec_config2 & (1U << 15U)) != 0U;
  if ((background & kHvsBackgroundInterlace) != 0U &&
      (pv3_semiquadri || !vec_progressivo)) {
    printf("boot: pi4kms composito prova status=fail phase=interlacciato "
           "background=0x%08x pv3_vcontrol=0x%08x vec_config2=0x%08x\r\n",
           static_cast<unsigned>(background),
           static_cast<unsigned>(pv3_vcontrol),
           static_cast<unsigned>(vec_config2));
    return false;
  }
  if ((background & kHvsBackgroundInterlace) != 0U) {
    printf("boot: pi4kms composito semiquadri dell'HVS col progressivo: "
           "canale a quadro intero, pv3_vcontrol=0x%08x vec_config2=0x%08x"
           "\r\n", static_cast<unsigned>(pv3_vcontrol),
           static_cast<unsigned>(vec_config2));
  }



  CompositeCampioni("prima");
  CompositeListe("prima");
  const uint32_t firmware = SelectedChannelDlist(kHvsActiveDlist0);
  const uint32_t pending = SelectedChannelDlist(kHvsPendingDlist0);
  if (firmware >= kHvs5DlistWordCapacity) {
    printf("boot: pi4kms composito prova status=fail phase=lista "
           "pending=%u active=%u\r\n",
           static_cast<unsigned>(pending), static_cast<unsigned>(firmware));
    return false;
  }
  if (firmware != pending) {
    printf("boot: pi4kms composito lista in attesa diversa da quella in uso: "
           "pending=%u active=%u, si prova lo stesso\r\n",
           static_cast<unsigned>(pending), static_cast<unsigned>(firmware));
  }
  g_composite_fw_pending = pending;
  uint32_t words[kProbeDlistWordCapacity] = {};
  uint32_t captured = 0U;
  for (; captured < kProbeDlistWordCapacity &&
         firmware + captured < kHvs5DlistWordCapacity; ++captured) {
    words[captured] = ReadRegister(
        kHvsBase, kHvs5DlistStart + (firmware + captured) * sizeof(uint32_t));
  }
  Hvs5DlistInfo info = {};
  const uint32_t slot = kTakeoverDlistSlots[1];
  if (!InspectHvs5Dlist(words, captured, &info) || !info.end_found ||
      RangesOverlap(slot, info.used_words, firmware, info.used_words) ||
      !ArmDlistRegionSafe(slot, info.used_words) ||
      !WriteAndVerifyDlist(slot, words, info.used_words,
                           kVerifyFirmwareControls)) {
    printf("boot: pi4kms composito prova status=fail phase=copia lista=%u "
           "parole=%u fine=%u posto=%u\r\n",
           static_cast<unsigned>(firmware),
           static_cast<unsigned>(info.used_words),
           info.end_found ? 1U : 0U, static_cast<unsigned>(slot));
    return false;
  }


  WritePendingDlist(slot);
  bool taken = WaitForActiveDlist(slot, true) || WaitForActiveDlist(slot, true);



  if (!taken) {
    WritePendingDlist(pending);
    printf("boot: pi4kms composito la lista non si prende coi semiquadri: "
           "posto=%u pending=%u active=%u stato=0x%08x\r\n",
           static_cast<unsigned>(slot),
           static_cast<unsigned>(SelectedChannelDlist(kHvsPendingDlist0)),
           static_cast<unsigned>(SelectedChannelDlist(kHvsActiveDlist0)),
           static_cast<unsigned>(CanaleLeggi(kHvsChannelStatus0)));
    if ((background & kHvsBackgroundInterlace) != 0U) {
      taken = CompositeProvaProgressivo(slot);
    }
    if (!taken) {
      printf("boot: pi4kms composito prova status=fail phase=presa\r\n");
      return false;
    }
  }
  uint32_t canale_toccato = 0U;
  uint32_t checks = 0U;
  uint32_t lost = 0U;
  uint32_t first_pending = 0U;
  uint32_t first_active = 0U;
  const uint64_t start = CTimer::GetClockTicks64();
  while (taken && CTimer::GetClockTicks64() - start <
                      static_cast<uint64_t>(duration_ms) * 1000U) {
    const uint32_t p = SelectedChannelDlist(kHvsPendingDlist0);
    const uint32_t a = SelectedChannelDlist(kHvsActiveDlist0);
    ++checks;
    if (p != slot || a != slot) {
      if (lost++ == 0U) {
        first_pending = p;
        first_active = a;
      }
    }

    if (g_composite_progressivo &&
        (CanaleLeggi(kHvsChannelControl0) != g_composite_control ||
         CanaleLeggi(kHvsChannelBackground0) != g_composite_background)) {
      ++canale_toccato;
    }
    CTimer::SimpleusDelay(2000U);
  }
  uint32_t changed = 0U;
  for (uint32_t i = 0U; i < info.used_words; ++i) {
    if (IsFirmwareControlOrEndWord(words, info.used_words, i) &&
        ReadRegister(kHvsBase, kHvs5DlistStart +
                     (slot + i) * sizeof(uint32_t)) != words[i]) {
      ++changed;
    }
  }


  WritePendingDlist(firmware);
  const bool restored =
      WaitForActiveDlist(firmware, true) || WaitForActiveDlist(firmware, true);
  if (g_composite_progressivo) {
    CompositeRimettiCanale(g_composite_variante);
    WritePendingDlist(g_composite_fw_pending);
    printf("boot: pi4kms composito progressivo variante=%d tenuta "
           "canale_toccato=%u, ritorno al canale del firmware\r\n",
           g_composite_variante, static_cast<unsigned>(canale_toccato));
  }
  printf("boot: pi4kms composito prova lista=%u parole=%u piani=%u posto=%u "
         "presa=%u controlli=%u persi=%u primo=%u/%u toccate=%u ritorno=%u "
         "ms=%u\r\n",
         static_cast<unsigned>(firmware),
         static_cast<unsigned>(info.used_words),
         static_cast<unsigned>(info.plane_count),
         static_cast<unsigned>(slot), taken ? 1U : 0U,
         static_cast<unsigned>(checks), static_cast<unsigned>(lost),
         static_cast<unsigned>(first_pending),
         static_cast<unsigned>(first_active),
         static_cast<unsigned>(changed), restored ? 1U : 0U, duration_ms);
  const bool tenuta = taken && restored && checks != 0U && lost == 0U &&
                      changed == 0U && canale_toccato == 0U;
  if (!tenuta) {
    g_composite_progressivo = false;
  }
  return tenuta;
}

void Shutdown() {
  if (!g_recovery_lifecycle.NativeScanoutCommitted()) {
    (void)RestoreFirmwareScanout(true);
  }
  g_takeover_requested = false;
  g_takeover_active = false;
  g_takeover_failed = false;
  g_native_mode_configured = false;
  g_native_mode_active = false;
  g_native_mode_failed = false;
  g_native_mode_plan = {};
  g_recovery_lifecycle.Reset();
  g_submitted_slot = 0U;
  ClearDlistTemplates();
  ClearFirmwareRecoverySnapshot();
  g_probe_usable = false;
  g_probe_snapshot = {};
  g_probe_info = {};
}

}  // namespace pi4kms
