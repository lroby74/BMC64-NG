#include "pi5_kms.h"
#include "kms/kms_mode.h"
#include "scaling_order.h"

#include <circle/bcm2835.h>
#include <circle/bcmpropertytags.h>
#include <circle/memory.h>
#include <circle/memio.h>
#include <circle/new.h>
#include <circle/synchronize64.h>
#include <circle/timer.h>

#include <stdio.h>
#include <string.h>

#ifndef ALIGN_UP
#define ALIGN_UP(x, y) (((x) + (y)-1) & ~((y)-1))
#endif

namespace pi5kms {

namespace {

constexpr uintptr kPv0Base = ARM_IO_BASE + 0x410000;
constexpr uintptr kHvsBase = ARM_IO_BASE + 0x580000;
constexpr uintptr kHdmi0DvpGlobalBase = ARM_IO_BASE + 0x700000;
constexpr uintptr kHdmi0HdBase = ARM_IO_BASE + 0x720000;
constexpr uintptr kHdmi0Base = ARM_IO_BASE + 0x701400;
constexpr uintptr kHdmi0DvpBase = ARM_IO_BASE + 0x701000;
constexpr uintptr kHdmi0PhyBase = ARM_IO_BASE + 0x701D00;
constexpr uintptr kHdmi0RmBase = ARM_IO_BASE + 0x702000;
constexpr uintptr kHdmi0PacketRamBase = ARM_IO_BASE + 0x703800;
constexpr uintptr kHdmi0CscBase = ARM_IO_BASE + 0x700100;

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
constexpr u32 kPvPipeInitCtrl = 0x094;
constexpr u32 kPvControlEnable = 1U << 0;
constexpr u32 kPvControlFifoClear = 1U << 1;
constexpr u32 kPvControlWaitHstart = 1U << 12;
constexpr u32 kPvControlTriggerUnderflow = 1U << 13;
constexpr u32 kPvControlClearAtStart = 1U << 14;
constexpr unsigned kPvControlFifoLevelShift = 15;
constexpr unsigned kPvD0ControlFifoLevelHighShift = 24;
constexpr u32 kPvC0FifoLevel = 64U - 3U * 6U;
constexpr u32 kPvD0FifoLevel = 512U - 3U * 6U;
constexpr u32 kPvVControlVideoEnable = 1U << 0;
constexpr u32 kPvVControlContinuous = 1U << 1;
constexpr u32 kPvVControlOddTiming = 1U << 29;
constexpr u32 kPvMuxRgbNoSwap = 8U << 2;
constexpr u32 kPvPipeInit = (1U << 8) | (1U << 4) | (1U << 0);

constexpr u32 kHvsVersion = 0x000;
constexpr u32 kHvsVersionD0 = 0x54;
constexpr u32 kHvsControl = 0x020;
constexpr u32 kHvsControlEnable = 1U << 31;
constexpr u32 kHvs6cDisp0Ctrl0 = 0x030;
constexpr u32 kHvs6cDisp0Ctrl1 = 0x034;
constexpr u32 kHvs6cDisp0Bgnd0 = 0x038;
constexpr u32 kHvs6cDisp0Lptrs = 0x03C;
constexpr u32 kHvs6cDisp0Status = 0x044;
constexpr u32 kHvs6cDisp0Dl = 0x048;
constexpr u32 kHvs6cDisp0Run = 0x04C;
constexpr u32 kHvs6dDisp0Ctrl0 = 0x100;
constexpr u32 kHvs6dDisp0Ctrl1 = 0x104;
constexpr u32 kHvs6dDisp0Bgnd0 = 0x108;
constexpr u32 kHvs6dDisp0Lptrs = 0x110;
constexpr u32 kHvs6dDisp0Cob = 0x114;
constexpr u32 kHvs6dDisp0Status = 0x118;
constexpr u32 kHvs6dDisp0Dl = 0x11C;
constexpr u32 kHvs6dDisp0Run = 0x120;
constexpr u32 kHvsDlistStart = 0x4000;
constexpr u32 kHvsFilterMitchellSlot = 16;
constexpr u32 kHvsFilterNearestSlot = 32;
constexpr u32 kHvsDlistSlot0 = 64;
constexpr u32 kHvsDlistSlot1 = 224;
constexpr u32 kHvsStatusModeShift = 13;
constexpr u32 kHvsStatusModeMask = 3U << kHvsStatusModeShift;
constexpr u32 kHvsStatusModeEof = 3;
constexpr u32 kHvsDispCtrl0Enable = 1U << 31;
constexpr u32 kHvsDispCtrl0Reset = 1U << 30;
constexpr u32 kHvsDispCtrl1Interlace = 1U << 0;
constexpr u32 kHvsDispCtrl1BgEnable = 1U << 8;
constexpr u32 kHvsCtl0End = 1U << 31;
constexpr u32 kHvsCtl0Valid = 1U << 30;
constexpr u32 kHvsCtl0AddrModeLinear = 0;
constexpr u32 kHvsCtl0Unity = 1U << 15;
constexpr u32 kHvsPtr0UpmBaseShift = 16;
constexpr u32 kHvsPtr0UpmHandleShift = 10;
constexpr u32 kHvsPtr0UpmBufferLines2 = 0;
constexpr u32 kHvsPtr0UpmBufferLinesShift = 8;
constexpr u32 kHvsUpmWordSize = 256;
constexpr u32 kHvsUpmSlotBytes = 64 * 1024;
constexpr u32 kHvsLbmSlotWords = 256;
constexpr u32 kHvsPixelFormatRgb565 = 4;
constexpr u32 kHvsPixelFormatRgba8888 = 7;
constexpr u32 kHvsPixelOrderXrgb = 2;
constexpr u32 kHvsPixelOrderArgb = 2;
constexpr u32 kHvsAlphaMaskNone = 0;
constexpr u32 kHvsAlphaMaskFixed = 3;
constexpr u32 kHvsOpaqueAlpha = 0xFFFU;
constexpr u32 kHvsSclHPpfVPpf = 0;
constexpr u32 kHvsSclHTpzVPpf = 1;
constexpr u32 kHvsSclHPpfVTpz = 2;
constexpr u32 kHvsSclHTpzVTpz = 3;
constexpr u32 kHvsSclHPpfVNone = 4;
constexpr u32 kHvsSclHNoneVPpf = 5;
constexpr u32 kHvsSclHNoneVTpz = 6;
constexpr u32 kHvsSclHTpzVNone = 7;
constexpr u32 kHvsPpfNoInterp = 1U << 31;
constexpr u32 kHvsPpfAgc = 1U << 30;
constexpr unsigned kHvsPpfPhaseBits = 6;
constexpr unsigned kHvsFilterKernelWords = 11;
constexpr unsigned kHvsScanoutDlistWords = 160;
constexpr unsigned kHvsScanoutTemplatePlanes = 8;

struct ScanoutDlistTemplate {
  bool valid;
  unsigned plane_count;
  u32 display_width;
  u32 display_height;
  Plane planes[kHvsScanoutTemplatePlanes];
  u32 words[kHvsScanoutDlistWords];
  unsigned word_count;
  unsigned ptr0_words[kHvsScanoutTemplatePlanes];
  unsigned address_words[kHvsScanoutTemplatePlanes];
};

bool g_hvs_filter_kernels_uploaded = false;
unsigned g_hvs_submitted_dlist = 0;
bool g_hvs_has_active_dlist = false;
bool g_hvs_vblank_timeout_logged = false;
ScanoutDlistTemplate g_scanout_dlist_templates[2] = {};
bool g_scanout_dlist_cache_hit_logged[2] = {};
PresentTiming g_last_present_timing = {};
Mode g_active_mode = {};
bool g_active_mode_valid = false;
bool g_waiting_for_initial_sink = false;
u64 g_initial_sink_detected_us = 0;

constexpr u64 kInitialSinkSettleUs = 250000;

constexpr u32 HvsField(u32 value, unsigned shift) {
  return value << shift;
}

constexpr u32 HvsPpfFilterWord(int c0, int c1, int c2) {
  return ((u32)(c0 & 0x1FF) << 0) |
         ((u32)(c1 & 0x1FF) << 9) |
         ((u32)(c2 & 0x1FF) << 18);
}

constexpr u32 kHvsMitchellKernel[6] = {
  HvsPpfFilterWord(0, -2, -6),
  HvsPpfFilterWord(-8, -10, -8),
  HvsPpfFilterWord(-3, 2, 18),
  HvsPpfFilterWord(50, 82, 119),
  HvsPpfFilterWord(155, 187, 213),
  HvsPpfFilterWord(227, 227, 0),
};

constexpr u32 kHvsNearestKernel[6] = {
  HvsPpfFilterWord(0, 0, 0),
  HvsPpfFilterWord(0, 0, 0),
  HvsPpfFilterWord(1, 1, 1),
  HvsPpfFilterWord(1, 255, 255),
  HvsPpfFilterWord(255, 255, 255),
  HvsPpfFilterWord(255, 255, 0),
};

constexpr u32 kHdmiFifoCtl = 0x07C;
constexpr u32 kHdmiRamPacketConfig = 0x0C4;
constexpr u32 kHdmiRamPacketStatus = 0x0CC;
constexpr u32 kHdmiSchedulerControl = 0x0E8;
constexpr u32 kHdmiHorza = 0x0EC;
constexpr u32 kHdmiHorzb = 0x0F0;
constexpr u32 kHdmiVerta0 = 0x0F4;
constexpr u32 kHdmiVertb0 = 0x0F8;
constexpr u32 kHdmiVerta1 = 0x100;
constexpr u32 kHdmiVertb1 = 0x104;
constexpr u32 kHdmiMiscControl = 0x114;
constexpr u32 kHdmiDeepColorConfig1 = 0x18C;
constexpr u32 kHdmiGcpConfig = 0x194;
constexpr u32 kHdmiGcpWord1 = 0x198;
constexpr u32 kHdmiHotplug = 0x1C8;
constexpr u32 kHdmiClockStop = 0x0BC;
constexpr u32 kHdmiVecInterfaceConfig = 0x0F0;
constexpr u32 kHdmiVecInterfaceXbar = 0x0F4;
constexpr u32 kHdmiDvpSoftwareInit = 0x004;
constexpr u32 kHdmiDvpMiscConfig = 0x008;
constexpr u32 kHdmiDvpSoftwareInitHdmi0 = 1U << 1;
constexpr u32 kHdmiDvpAudioClockDisableHdmi0 = 1U << 3;
constexpr u32 kHdmiDvpControl = 0x000;
constexpr u32 kHdmiClockStopPixel = 1U << 1;
constexpr u32 kHdmiSchedulerManualFormat = 1U << 15;
constexpr u32 kHdmiSchedulerIgnoreVsyncPredicts = 1U << 5;
constexpr u32 kHdmiSchedulerHdmiActive = 1U << 1;
constexpr u32 kHdmiSchedulerModeHdmi = 1U << 0;
constexpr u32 kHdmiRamPacketEnable = 1U << 16;
constexpr u32 kHdmiAviPacketEnable = 1U << 2;
constexpr u32 kHdmiFifoRecenterDone = 1U << 14;
constexpr u32 kHdmiFifoRecenter = 1U << 6;
constexpr u32 kHdmiFifoMasterSlaveN = 1U << 0;
constexpr u32 kHdmiFifoValidWriteMask = 0x0000EFFF;

constexpr u32 kHdmiHdVideoControl = 0x044;
constexpr u32 kHdmiHdFrameCount = 0x060;
constexpr u32 kHdmiHdVideoEnable = 1U << 31;
constexpr u32 kHdmiHdVideoUnderflowEnable = 1U << 30;
constexpr u32 kHdmiHdVideoFrameCounterReset = 1U << 29;
constexpr u32 kHdmiHdVideoVsyncLow = 1U << 28;
constexpr u32 kHdmiHdVideoHsyncLow = 1U << 27;
constexpr u32 kHdmiHdVideoClearRgb = 1U << 23;
constexpr u32 kHdmiHdVideoBlankPixel = 1U << 18;
constexpr u32 kHdmiHdVideoBlankInsertEnable = 1U << 16;
constexpr u32 kHdmiDeepColorInitPackPhaseMask = 7U << 8;
constexpr u32 kHdmiDeepColorInitPackPhase8Bpc = 2U << 8;
constexpr u32 kHdmiDeepColorDepthMask = 0xFU;
constexpr u32 kHdmiGcpEnable = 1U << 31;
constexpr u32 kHdmiGcpSubpacketBytes01Mask = 0xFFFFU;
constexpr u32 kHdmiGcpClearAvMute = 1U << 4;
constexpr u32 kHdmiHotplugConnected = 1U << 0;
constexpr u32 kHdmiPacketStride = 0x24;
constexpr u32 kHdmiAviPacketId = 2;

constexpr u32 kHdmiCscControl = 0x000;
constexpr u32 kHdmiCsc12_11 = 0x004;
constexpr u32 kHdmiCsc14_13 = 0x008;
constexpr u32 kHdmiCsc22_21 = 0x00C;
constexpr u32 kHdmiCsc24_23 = 0x010;
constexpr u32 kHdmiCsc32_31 = 0x014;
constexpr u32 kHdmiCsc34_33 = 0x018;
constexpr u32 kHdmiCscChannelControl = 0x02C;
constexpr u32 kHdmiCscEnable = 1U << 2;
constexpr u32 kHdmiCscCustomMode = 3U;
constexpr u32 kHdmiRgbInterfaceXbar = 0x00354021;

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
constexpr u32 kPhyPllPostKdivBypass = 1U << 4;
constexpr u32 kPhyPllResetPllResetb = 1U << 0;

constexpr u32 kRmOffset = 0x018;

constexpr unsigned long long kVc6VcoMin = 8000000000ULL;
constexpr unsigned long long kVc6VcoMax = 12000000000ULL;
constexpr unsigned long long kOscillatorFrequency = 54000000ULL;













struct HdmiPortRegisters {
  uintptr core;
  uintptr dvp;
  uintptr phy;
  uintptr rm;
  uintptr packet_ram;
  uintptr csc;
  uintptr pv;
  u32 hd_video_control;
  u32 hd_frame_count;
  u32 software_init_bit;
  u32 audio_clock_disable_bit;
  unsigned hvs_channel;
};

const HdmiPortRegisters kHdmiPorts[2] = {
  {kHdmi0Base, kHdmi0DvpBase, kHdmi0PhyBase, kHdmi0RmBase,
   kHdmi0PacketRamBase, kHdmi0CscBase, kPv0Base,
   kHdmiHdVideoControl, kHdmiHdFrameCount,
   kHdmiDvpSoftwareInitHdmi0, kHdmiDvpAudioClockDisableHdmi0, 0U},
  {ARM_IO_BASE + 0x706400, ARM_IO_BASE + 0x706000, ARM_IO_BASE + 0x706D00,
   ARM_IO_BASE + 0x707000, ARM_IO_BASE + 0x708800, ARM_IO_BASE + 0x700180,
   ARM_IO_BASE + 0x411000, 0x048U, 0x064U, 1U << 2, 1U << 4, 1U},
};



const HdmiPortRegisters *g_port = &kHdmiPorts[0];
unsigned g_requested_hdmi_port = 0U;
bool g_porta_scelta = false;


u32 ReadReg(uintptr base, u32 offset) {
  return read32(base + offset);
}

void WriteReg(uintptr base, u32 offset, u32 value) {
  write32(base + offset, value);
}

bool HdmiSinkConnected() {
  return (ReadReg(g_port->core, kHdmiHotplug) &
          kHdmiHotplugConnected) != 0;
}

bool WaitForRegisterBit(uintptr base, u32 offset, u32 mask,
                        bool set, unsigned timeout_us) {
  const u64 start = CTimer::GetClockTicks64();
  do {
    if (((ReadReg(base, offset) & mask) != 0) == set) {
      return true;
    }
  } while (CTimer::GetClockTicks64() - start < timeout_us);
  return ((ReadReg(base, offset) & mask) != 0) == set;
}

u32 QuiesceHdmiOutputForModeSet() {
  // Follow the BCM2712 post-CRTC disable ordering before touching its PHY:
  // stop data-island packets, blank the stream, then allow one millisecond
  // for the blank to reach the link.  Do not clear HD_VID_CTL.ENABLE here;
  // upstream vc4 documents that operation as lockup-prone on BCM2712.
  const u32 packet_config = ReadReg(g_port->core, kHdmiRamPacketConfig);
  WriteReg(g_port->core, kHdmiRamPacketConfig, 0);

  const u32 hd_video = ReadReg(kHdmi0HdBase, g_port->hd_video_control);
  WriteReg(kHdmi0HdBase, g_port->hd_video_control,
           hd_video | kHdmiHdVideoClearRgb | kHdmiHdVideoBlankPixel);
  DataSyncBarrier();
  CTimer::SimpleusDelay(1000);

  printf("boot: pi5kms hdmi quiesce packets=0x%08x->0x%08x "
         "hd_video=0x%08x->0x%08x enable-preserved=yes\r\n",
         packet_config,
         ReadReg(g_port->core, kHdmiRamPacketConfig),
         hd_video,
         ReadReg(kHdmi0HdBase, g_port->hd_video_control));
  return packet_config;
}

void ResetHdmiCoreForModeSet() {
  // BCM2712 exposes HDMI0 reset 1 and its 108 MHz audio clock gate through
  // the shared DVP block.  Firmware does not necessarily initialize either
  // one when the board boots without a sink, so the native modeset must not
  // inherit their state.
  const u32 misc_before = ReadReg(kHdmi0DvpGlobalBase,
                                  kHdmiDvpMiscConfig);
  WriteReg(kHdmi0DvpGlobalBase, kHdmiDvpMiscConfig,
           misc_before & ~g_port->audio_clock_disable_bit);

  const u32 reset_before = ReadReg(kHdmi0DvpGlobalBase,
                                   kHdmiDvpSoftwareInit);
  WriteReg(kHdmi0DvpGlobalBase, kHdmiDvpSoftwareInit,
           reset_before | g_port->software_init_bit);
  DataSyncBarrier();
  CTimer::SimpleusDelay(1);
  WriteReg(kHdmi0DvpGlobalBase, kHdmiDvpSoftwareInit,
           reset_before & ~g_port->software_init_bit);

  WriteReg(kHdmi0HdBase, kHdmiDvpControl, 0);
  const u32 clock_stop_before = ReadReg(g_port->dvp, kHdmiClockStop);
  WriteReg(g_port->dvp, kHdmiClockStop,
           clock_stop_before | kHdmiClockStopPixel);
  DataSyncBarrier();

  printf("boot: pi5kms hdmi core reset software_init=0x%08x->0x%08x "
         "misc=0x%08x->0x%08x clock_stop=0x%08x->0x%08x\r\n",
         reset_before,
         ReadReg(kHdmi0DvpGlobalBase, kHdmiDvpSoftwareInit),
         misc_before,
         ReadReg(kHdmi0DvpGlobalBase, kHdmiDvpMiscConfig),
         clock_stop_before,
         ReadReg(g_port->dvp, kHdmiClockStop));
}

bool RecenterHdmiFifo() {
  // Match vc4_hdmi_recenter_fifo(): the second edge one millisecond after the
  // first is significant on a controller starting from reset state.
  const u32 fifo = ReadReg(g_port->core, kHdmiFifoCtl) &
                   kHdmiFifoValidWriteMask;
  WriteReg(g_port->core, kHdmiFifoCtl, fifo & ~kHdmiFifoRecenter);
  WriteReg(g_port->core, kHdmiFifoCtl, fifo | kHdmiFifoRecenter);
  CTimer::SimpleusDelay(1000);
  WriteReg(g_port->core, kHdmiFifoCtl, fifo & ~kHdmiFifoRecenter);
  WriteReg(g_port->core, kHdmiFifoCtl, fifo | kHdmiFifoRecenter);
  DataSyncBarrier();
  return WaitForRegisterBit(g_port->core, kHdmiFifoCtl,
                            kHdmiFifoRecenterDone, true, 1000);
}

u8 CeaVicForMode(const Mode &mode) {
  if (mode.width == 1280 && mode.height == 720 &&
      mode.pixel_clock == 74250000) {
    return mode.h_front_porch == 440 ? 19 :
           mode.h_front_porch == 110 ? 4 : 0;
  }
  if (mode.width == 1920 && mode.height == 1080 &&
      mode.pixel_clock == 148500000) {
    return mode.h_front_porch == 528 ? 31 :
           mode.h_front_porch == 88 ? 16 : 0;
  }
  return 0;
}

void ConfigureHdmiRgbOutput(const Mode &mode) {
  // vc5_hdmi_csc_setup() is required on BCM2712 even for RGB output.  A
  // connected firmware boot happens to leave this DVP crossbar and CSC
  // programmed, while a headless boot does not.  Program the native RGB
  // route explicitly so SetMode never depends on that inherited state.
  const bool limited_range = CeaVicForMode(mode) != 0;
  const u16 scale = limited_range ? 0x1B80 : 0x2000;
  const u16 offset = limited_range ? 0x0400 : 0x0000;

  WriteReg(g_port->dvp, kHdmiVecInterfaceConfig, 0);
  WriteReg(g_port->dvp, kHdmiVecInterfaceXbar,
           kHdmiRgbInterfaceXbar);

  WriteReg(g_port->csc, kHdmiCsc12_11, scale);
  WriteReg(g_port->csc, kHdmiCsc14_13,
           static_cast<u32>(offset) << 16);
  WriteReg(g_port->csc, kHdmiCsc22_21,
           static_cast<u32>(scale) << 16);
  WriteReg(g_port->csc, kHdmiCsc24_23,
           static_cast<u32>(offset) << 16);
  WriteReg(g_port->csc, kHdmiCsc32_31, 0);
  WriteReg(g_port->csc, kHdmiCsc34_33,
           (static_cast<u32>(offset) << 16) | scale);
  WriteReg(g_port->csc, kHdmiCscChannelControl, 0);
  WriteReg(g_port->csc, kHdmiCscControl,
           kHdmiCscEnable | kHdmiCscCustomMode);
  DataSyncBarrier();

  printf("boot: pi5kms rgb route range=%s interface=0x%08x "
         "xbar=0x%08x csc=0x%08x coeff=0x%08x,0x%08x,0x%08x\r\n",
         limited_range ? "limited" : "full",
         ReadReg(g_port->dvp, kHdmiVecInterfaceConfig),
         ReadReg(g_port->dvp, kHdmiVecInterfaceXbar),
         ReadReg(g_port->csc, kHdmiCscControl),
         ReadReg(g_port->csc, kHdmiCsc12_11),
         ReadReg(g_port->csc, kHdmiCsc22_21),
         ReadReg(g_port->csc, kHdmiCsc34_33));
}

bool WriteAviInfoFrame(const Mode &mode) {
  u8 packet[kHdmiPacketStride] = {};
  packet[0] = 0x82;  // AVI InfoFrame type.
  packet[1] = 0x02;  // Version 2.
  packet[2] = 13;    // Payload bytes.

  const u8 vic = CeaVicForMode(mode);
  if (vic != 0) {
    packet[5] = 2U << 4;  // 16:9 picture aspect ratio.
    packet[7] = vic;
  }

  unsigned checksum = 0;
  for (unsigned i = 0; i < 3; ++i) {
    checksum += packet[i];
  }
  for (unsigned i = 4; i < 17; ++i) {
    checksum += packet[i];
  }
  packet[3] = static_cast<u8>(0U - checksum);

  WriteReg(g_port->core, kHdmiRamPacketConfig,
           ReadReg(g_port->core, kHdmiRamPacketConfig) &
               ~kHdmiAviPacketEnable);
  WaitForRegisterBit(g_port->core, kHdmiRamPacketStatus,
                     kHdmiAviPacketEnable, false, 100);

  u32 packet_offset = kHdmiAviPacketId * kHdmiPacketStride;
  const u32 packet_end = packet_offset + kHdmiPacketStride;
  for (unsigned i = 0; i < 17; i += 7) {
    WriteReg(g_port->packet_ram, packet_offset,
             static_cast<u32>(packet[i]) |
                 (static_cast<u32>(packet[i + 1]) << 8) |
                 (static_cast<u32>(packet[i + 2]) << 16));
    packet_offset += sizeof(u32);
    WriteReg(g_port->packet_ram, packet_offset,
             static_cast<u32>(packet[i + 3]) |
                 (static_cast<u32>(packet[i + 4]) << 8) |
                 (static_cast<u32>(packet[i + 5]) << 16) |
                 (static_cast<u32>(packet[i + 6]) << 24));
    packet_offset += sizeof(u32);
  }
  while (packet_offset < packet_end) {
    WriteReg(g_port->packet_ram, packet_offset, 0);
    packet_offset += sizeof(u32);
  }
  DataSyncBarrier();

  WriteReg(g_port->core, kHdmiRamPacketConfig,
           ReadReg(g_port->core, kHdmiRamPacketConfig) |
               kHdmiAviPacketEnable);
  const bool active = WaitForRegisterBit(
      g_port->core, kHdmiRamPacketStatus,
      kHdmiAviPacketEnable, true, 100);
  printf("boot: pi5kms avi infoframe vic=%u checksum=0x%02x active=%s\r\n",
         static_cast<unsigned>(vic), static_cast<unsigned>(packet[3]),
         active ? "yes" : "timeout");
  return active;
}

bool EnableHdmiOutput(const Mode &mode) {
  u32 hd_video = kHdmiHdVideoEnable |
                 kHdmiHdVideoUnderflowEnable |
                 kHdmiHdVideoFrameCounterReset |
                 kHdmiHdVideoClearRgb |
                 kHdmiHdVideoBlankInsertEnable;
  if (!mode.v_sync_positive) {
    hd_video |= kHdmiHdVideoVsyncLow;
  }
  if (!mode.h_sync_positive) {
    hd_video |= kHdmiHdVideoHsyncLow;
  }
  WriteReg(kHdmi0HdBase, g_port->hd_video_control, hd_video);
  WriteReg(kHdmi0HdBase, g_port->hd_video_control,
           ReadReg(kHdmi0HdBase, g_port->hd_video_control) &
               ~kHdmiHdVideoBlankPixel);

  WriteReg(g_port->core, kHdmiSchedulerControl,
           ReadReg(g_port->core, kHdmiSchedulerControl) |
               kHdmiSchedulerModeHdmi);
  DataSyncBarrier();
  const bool hdmi_active = WaitForRegisterBit(
      g_port->core, kHdmiSchedulerControl,
      kHdmiSchedulerHdmiActive, true, 1000);

  // Circle's HDMI audio device uses this bit as the hardware-ready contract.
  // Make it deterministic even when firmware skipped display initialization.
  WriteReg(g_port->core, kHdmiRamPacketConfig,
           ReadReg(g_port->core, kHdmiRamPacketConfig) |
               kHdmiRamPacketEnable);
  DataSyncBarrier();
  const bool avi_active = WriteAviInfoFrame(mode);

  const bool fifo_recentered = RecenterHdmiFifo();
  const u32 frame_start = ReadReg(kHdmi0HdBase, g_port->hd_frame_count);
  CTimer::SimpleusDelay(25000);
  const u32 frame_end = ReadReg(kHdmi0HdBase, g_port->hd_frame_count);

  printf("boot: pi5kms hdmi enable active=%s avi=%s recenter=%s "
         "scheduler=0x%08x hd_video=0x%08x packets=0x%08x "
         "frame=0x%08x->0x%08x\r\n",
         hdmi_active ? "yes" : "timeout",
         avi_active ? "yes" : "timeout",
         fifo_recentered ? "done" : "timeout",
         ReadReg(g_port->core, kHdmiSchedulerControl),
         ReadReg(kHdmi0HdBase, g_port->hd_video_control),
         ReadReg(g_port->core, kHdmiRamPacketConfig),
         frame_start, frame_end);


  const bool frame_ok =
      g_port->hvs_channel == 0U || frame_end != frame_start;
  if (!frame_ok) {
    printf("boot: pi5kms hdmi 1: il contatore dei quadri e' fermo\r\n");
  }
  return hdmi_active && avi_active && fifo_recentered && frame_ok;
}

bool SetClockRate(unsigned clock_id, u32 rate) {
  CBcmPropertyTags tags;
  TPropertyTagSetClockRate tag;
  tag.nClockId = clock_id;
  tag.nRate = rate;
  tag.nSkipSettingTurbo = 0;
  return tags.GetTag(PROPTAG_SET_CLOCK_RATE, &tag, sizeof tag, 12);
}





bool SetClockRateAlmeno(unsigned clock_id, u32 rate) {
  CBcmPropertyTags tags;
  TPropertyTagClockRate tag;
  tag.nClockId = clock_id;
  if (tags.GetTag(PROPTAG_GET_CLOCK_RATE, &tag, sizeof tag, 4) &&
      tag.nRate >= rate) {
    printf("boot: pi5kms clock %u resta a %u (ne servono %u)\r\n",
           clock_id, tag.nRate, rate);
    return true;
  }
  return SetClockRate(clock_id, rate);
}

u32 MakeHdmiHorza(const Mode &mode) {
  const u32 sync_flags = (mode.h_sync_positive ? (1U << 15) : 0) |
                         (mode.v_sync_positive ? (1U << 14) : 0);
  return ((u32)mode.h_front_porch << 16) | sync_flags | mode.width;
}

u32 MakeHdmiHorzb(const Mode &mode) {
  return ((u32)mode.h_back_porch << 16) | mode.h_sync;
}

u32 MakeHdmiVerta(const Mode &mode) {
  return ((u32)mode.v_sync << 24) | ((u32)mode.v_front_porch << 16) |
         mode.height;
}

u32 MakeHdmiVertb(const Mode &mode) {
  return mode.v_back_porch;
}

u32 MakePvHorza(const Mode &mode) {
  return ((u32)(mode.h_back_porch / 2) << 16) | (mode.h_sync / 2);
}

u32 MakePvHorzb(const Mode &mode) {
  return ((u32)(mode.h_front_porch / 2) << 16) | (mode.width / 2);
}

u32 MakePvVerta(const Mode &mode) {
  return ((u32)mode.v_back_porch << 16) | mode.v_sync;
}

u32 MakePvVertb(const Mode &mode) {
  return ((u32)mode.v_front_porch << 16) | mode.height;
}

bool IsBcm2712D0() {
  return (ReadReg(kHvsBase, kHvsVersion) & 0xFFU) == kHvsVersionD0;
}

u32 PvConfiguredControl() {
  if (IsBcm2712D0()) {
    // Pi500/D0 exposes three FIFO-level high bits at 26:24.  The working
    // firmware reference programs a 512-byte FIFO threshold of 494 bytes.
    return ((kPvD0FifoLevel >> 6) << kPvD0ControlFifoLevelHighShift) |
           ((kPvD0FifoLevel & 0x3FU) << kPvControlFifoLevelShift) |
           kPvControlClearAtStart;
  }
  return (kPvC0FifoLevel << kPvControlFifoLevelShift) |
         kPvControlClearAtStart |
         kPvControlTriggerUnderflow |
         kPvControlWaitHstart |
         kPvControlFifoClear;
}

u32 PvConfiguredVControl() {
  // The D0 firmware reference runs progressive HDMI continuously without
  // ODD_TIMING.  C0 follows the upstream one-pixel-per-clock sequence.
  return kPvVControlContinuous |
         (IsBcm2712D0() ? 0 : kPvVControlOddTiming);
}

void ConfigurePixelValve(const Mode &mode) {
  // Match vc4_crtc_config_pv() for bcm2712-pixelvalve0: progressive HDMI0,
  // RGB24, one pixel per clock and encoder clock-select 0.  C0 uses the
  // upstream 64-byte FIFO layout; D0 uses its 512-byte FIFO layout.  Every
  // control word is written from known state because firmware leaves a
  // materially different (and non-running) PV setup after a headless boot.
  WriteReg(g_port->pv, kPvHorza, MakePvHorza(mode));
  WriteReg(g_port->pv, kPvHorzb, MakePvHorzb(mode));
  WriteReg(g_port->pv, kPvVerta, MakePvVerta(mode));
  WriteReg(g_port->pv, kPvVertb, MakePvVertb(mode));
  WriteReg(g_port->pv, 0x008, 0);  // PV_VSYNCD_EVEN.
  WriteReg(g_port->pv, kPvVControl, PvConfiguredVControl());
  WriteReg(g_port->pv, kPvMuxCfg, kPvMuxRgbNoSwap);
  WriteReg(g_port->pv, kPvPipeInitCtrl, kPvPipeInit);
  WriteReg(g_port->pv, kPvControl, PvConfiguredControl());
  DataSyncBarrier();

  printf("boot: pi5kms pixelvalve configured ctl=0x%08x vctl=0x%08x "
         "mux=0x%08x pipe=0x%08x\r\n",
         ReadReg(g_port->pv, kPvControl),
         ReadReg(g_port->pv, kPvVControl),
         ReadReg(g_port->pv, kPvMuxCfg),
         ReadReg(g_port->pv, kPvPipeInitCtrl));
}




u32 HvsCanaleScarto(u32 version) {
  return g_port->hvs_channel * (version == kHvsVersionD0 ? 0x40U : 0x20U);
}

u32 HvsDisp0Ctrl0Offset() {
  const u32 version = ReadReg(kHvsBase, kHvsVersion) & 0xFF;
  return (version == kHvsVersionD0 ? kHvs6dDisp0Ctrl0 : kHvs6cDisp0Ctrl0) +
         HvsCanaleScarto(version);
}

u32 HvsDisp0Ctrl1Offset() {
  const u32 version = ReadReg(kHvsBase, kHvsVersion) & 0xFF;
  return (version == kHvsVersionD0 ? kHvs6dDisp0Ctrl1 : kHvs6cDisp0Ctrl1) +
         HvsCanaleScarto(version);
}

u32 HvsDisp0LptrsOffset() {
  const u32 version = ReadReg(kHvsBase, kHvsVersion) & 0xFF;
  return (version == kHvsVersionD0 ? kHvs6dDisp0Lptrs : kHvs6cDisp0Lptrs) +
         HvsCanaleScarto(version);
}

u32 HvsDisp0StatusOffset() {
  const u32 version = ReadReg(kHvsBase, kHvsVersion) & 0xFF;
  return (version == kHvsVersionD0 ? kHvs6dDisp0Status : kHvs6cDisp0Status) +
         HvsCanaleScarto(version);
}

u32 HvsDisp0Bgnd0Offset() {
  const u32 version = ReadReg(kHvsBase, kHvsVersion) & 0xFF;
  return (version == kHvsVersionD0 ? kHvs6dDisp0Bgnd0 : kHvs6cDisp0Bgnd0) +
         HvsCanaleScarto(version);
}

u32 MakeHvsDispCtrl0(const Mode &mode) {
  return kHvsDispCtrl0Enable |
         (((u32)mode.width - 1U) << 16) |
         ((u32)mode.height - 1U);
}

void ProgramHvsDisplay0(const Mode &mode) {
  const u32 ctrl0 = HvsDisp0Ctrl0Offset();
  const u32 ctrl1 = HvsDisp0Ctrl1Offset();
  const u32 ctrl1_before = ReadReg(kHvsBase, ctrl1);

  u32 hvs_control = ReadReg(kHvsBase, kHvsControl) | kHvsControlEnable;
  if (g_port->hvs_channel == 1U) {


    hvs_control &= ~(3U << 12);
  }
  WriteReg(kHvsBase, kHvsControl, hvs_control);
  WriteReg(kHvsBase, ctrl0, kHvsDispCtrl0Reset);
  WriteReg(kHvsBase, ctrl1, ctrl1_before & ~kHvsDispCtrl1Interlace);
  WriteReg(kHvsBase, ctrl0, MakeHvsDispCtrl0(mode));
}

bool IsValidRect(const Rect &rect) {
  return rect.width != 0 && rect.height != 0 &&
         rect.x >= 0 && rect.y >= 0 &&
         rect.width <= 0x1FFFU && rect.height <= 0x1FFFU &&
         (u32)rect.x <= 0x1FFFU && (u32)rect.y <= 0x1FFFU;
}

bool IsUnityPlane(const Plane &plane) {
  return plane.source.width == plane.destination.width &&
         plane.source.height == plane.destination.height;
}

bool IsFullscreenOpaquePlane(const Plane &plane,
                             u32 display_width,
                             u32 display_height) {
  return plane.format == kPixelFormatRgb565 &&
         plane.destination.x == 0 &&
         plane.destination.y == 0 &&
         plane.destination.width == display_width &&
         plane.destination.height == display_height;
}

bool NeedsBackgroundFill(const Plane *planes, unsigned plane_count,
                         u32 display_width, u32 display_height) {
  if (plane_count == 0 ||
      !IsFullscreenOpaquePlane(planes[0], display_width, display_height)) {
    return true;
  }

  for (unsigned i = 0; i < plane_count; ++i) {
    if (planes[i].format == kPixelFormatArgb8888) {
      return true;
    }
  }

  return false;
}

HvsScaling GetHvsScaling(u32 src, u32 dst) {
  if (src == dst) {
    return kHvsScalingNone;
  }

  if (3U * dst >= 2U * src) {
    return kHvsScalingPpf;
  }
  return kHvsScalingTpz;
}

u32 MakeHvsSclField(HvsScaling x_scaling, HvsScaling y_scaling) {
  if (x_scaling == kHvsScalingPpf && y_scaling == kHvsScalingPpf) {
    return kHvsSclHPpfVPpf;
  }
  if (x_scaling == kHvsScalingTpz && y_scaling == kHvsScalingPpf) {
    return kHvsSclHTpzVPpf;
  }
  if (x_scaling == kHvsScalingPpf && y_scaling == kHvsScalingTpz) {
    return kHvsSclHPpfVTpz;
  }
  if (x_scaling == kHvsScalingTpz && y_scaling == kHvsScalingTpz) {
    return kHvsSclHTpzVTpz;
  }
  if (x_scaling == kHvsScalingPpf && y_scaling == kHvsScalingNone) {
    return kHvsSclHPpfVNone;
  }
  if (x_scaling == kHvsScalingNone && y_scaling == kHvsScalingPpf) {
    return kHvsSclHNoneVPpf;
  }
  if (x_scaling == kHvsScalingNone && y_scaling == kHvsScalingTpz) {
    return kHvsSclHNoneVTpz;
  }
  if (x_scaling == kHvsScalingTpz && y_scaling == kHvsScalingNone) {
    return kHvsSclHTpzVNone;
  }
  return 0;
}

u32 MakeHvsPlaneCtl0(unsigned next_words, bool unity,
                     PixelFormat format, u32 scl_field) {
  u32 hvs_format;
  u32 pixel_order;
  u32 alpha_mask;

  switch (format) {
    case kPixelFormatRgb565:
      hvs_format = kHvsPixelFormatRgb565;
      pixel_order = kHvsPixelOrderXrgb;
      alpha_mask = kHvsAlphaMaskFixed;
      break;
    case kPixelFormatArgb8888:
      hvs_format = kHvsPixelFormatRgba8888;
      pixel_order = kHvsPixelOrderArgb;
      alpha_mask = kHvsAlphaMaskNone;
      break;
    default:
      return 0;
  }

  return kHvsCtl0Valid |
         HvsField(next_words & 0x3FU, 24) |
         HvsField(kHvsCtl0AddrModeLinear, 20) |
         HvsField(alpha_mask, 18) |
         (unity ? kHvsCtl0Unity : 0) |
         HvsField(pixel_order, 13) |
         HvsField(scl_field, 8) |
         HvsField(scl_field, 5) |
         hvs_format;
}

u32 MakeHvsPos0(const Rect &rect) {
  return ((u32)rect.y << 16) | (u32)rect.x;
}

u32 MakeHvsPos2(u32 width, u32 height) {
  return ((height - 1U) << 16) | (width - 1U);
}

u32 MakeHvsPos1(const Rect &rect) {
  return ((rect.height - 1U) << 16) | (rect.width - 1U);
}

u32 MakeHvsPtr0(u32 framebuffer_bus_address, unsigned upm_slot) {
  const u32 ptr_upper =
      (u32)(((u64)framebuffer_bus_address >> 32) & 0xFFU);
  const u32 upm_base =
      (upm_slot * kHvsUpmSlotBytes) / kHvsUpmWordSize;
  const u32 upm_handle = upm_slot & 0x1FU;

  return ptr_upper |
         HvsField(upm_base, kHvsPtr0UpmBaseShift) |
         HvsField(upm_handle, kHvsPtr0UpmHandleShift) |
         HvsField(kHvsPtr0UpmBufferLines2, kHvsPtr0UpmBufferLinesShift);
}

u32 MakeHvsLbmBase(unsigned lbm_slot) {
  return lbm_slot * kHvsLbmSlotWords;
}

u32 MakeHvsPpfWord(u32 src, u32 dst, u32 xy, bool nearest) {
  const u32 src_fixed = src << 16;
  u32 scale = src_fixed / dst;
  int offset = (int)((xy & 0xFFFFU) >> (16 - kHvsPpfPhaseBits));
  offset += -(1 << (kHvsPpfPhaseBits - 1));

  scale &= ~1U;
  int offset2 = (int)(src_fixed - dst * scale);
  offset2 >>= 16 - kHvsPpfPhaseBits;
  int phase = offset + (offset2 >> 1);
  if (phase >= (1 << kHvsPpfPhaseBits)) {
    phase = (1 << kHvsPpfPhaseBits) - 1;
  }
  phase &= (1U << (kHvsPpfPhaseBits + 1U)) - 1U;

  return (nearest ? kHvsPpfNoInterp : 0) |
         kHvsPpfAgc |
         HvsField(scale & 0x1FFFFU, 8) |
         (u32)phase;
}

u32 MakeHvsTpzWord0(u32 src, u32 dst) {
  u32 scale;
  if ((dst << 16) < (src << 16)) {
    scale = (src << 16) / dst;
  } else {
    scale = (1U << 16) + 1U;
  }
  return HvsField(scale & 0x1FFFFFU, 8);
}

u32 MakeHvsTpzWord1(u32 src, u32 dst) {
  u32 scale;
  u32 recip;
  if ((dst << 16) < (src << 16)) {
    scale = (src << 16) / dst;
    recip = ~0U / scale;
  } else {
    recip = (1U << 16) - 1U;
  }
  return recip & 0xFFFFU;
}

void WriteDlistWord(unsigned index, u32 value);

void UploadHvsFilterKernel(unsigned slot, const u32 kernel[6]) {
  for (unsigned i = 0; i < kHvsFilterKernelWords; ++i) {
    const unsigned src = i < 6 ? i : (kHvsFilterKernelWords - i - 1);
    WriteDlistWord(slot + i, kernel[src]);
  }
}

void EnsureHvsFilterKernelsUploaded() {
  if (g_hvs_filter_kernels_uploaded) {
    return;
  }

  UploadHvsFilterKernel(kHvsFilterMitchellSlot, kHvsMitchellKernel);
  UploadHvsFilterKernel(kHvsFilterNearestSlot, kHvsNearestKernel);
  g_hvs_filter_kernels_uploaded = true;
}

void WriteDlistWord(unsigned index, u32 value) {
  WriteReg(kHvsBase, kHvsDlistStart + index * sizeof(u32), value);
}

void ProgramHvsCob0() {
  const u32 version = ReadReg(kHvsBase, kHvsVersion) & 0xFF;
  if (version != kHvsVersionD0) {
    return;
  }

  constexpr u32 line_width = 3840;
  constexpr u32 num_lines = 4;
  if (g_port->hvs_channel == 1U) {


    const u32 top1 = line_width + line_width * num_lines;
    WriteReg(kHvsBase, kHvs6dDisp0Cob + HvsCanaleScarto(version),
             (top1 << 16) | (line_width + 16));
    return;
  }
  u32 top = line_width + line_width * num_lines;
  u32 base = top + 16;
  top += line_width * num_lines;

  WriteReg(kHvsBase, kHvs6dDisp0Cob, (top << 16) | base);
}

bool AppendPlaneDlist(const Plane &plane,
                      unsigned upm_slot,
                      u32 *dlist, unsigned capacity, unsigned *count,
                      unsigned *ptr0_word, unsigned *address_word) {
  if (dlist == nullptr || count == nullptr || *count >= capacity) {
    return false;
  }
  if (capacity - *count < 32) {
    return false;
  }

  const bool unity = IsUnityPlane(plane);
  const HvsScaling x_scaling =
      GetHvsScaling(plane.source.width, plane.destination.width);
  const HvsScaling y_scaling =
      GetHvsScaling(plane.source.height, plane.destination.height);
  const u32 scl_field = unity ? 0 : MakeHvsSclField(x_scaling, y_scaling);
  const bool nearest = plane.filter == kScaleFilterNearest;

  const u32 bytes_per_pixel = plane.depth / 8;
  const u32 source_offset =
      (u32)plane.source.y * plane.pitch +
      (u32)plane.source.x * bytes_per_pixel;
  const u32 framebuffer_bus_address =
      plane.framebuffer_bus_address + source_offset;

  const unsigned start = *count;
  dlist[(*count)++] = 0;
  dlist[(*count)++] = MakeHvsPos0(plane.destination);
  dlist[(*count)++] = kHvsOpaqueAlpha << 4;
  if (!unity) {
    dlist[(*count)++] = MakeHvsPos1(plane.destination);
  }
  dlist[(*count)++] = MakeHvsPos2(plane.source.width, plane.source.height);
  dlist[(*count)++] = 0xC0C0C0C0U;
  *ptr0_word = *count;
  dlist[(*count)++] = MakeHvsPtr0(framebuffer_bus_address, upm_slot);
  *address_word = *count;
  dlist[(*count)++] = framebuffer_bus_address;
  dlist[(*count)++] = plane.pitch & 0x1FFFFU;

  if (!unity && y_scaling != kHvsScalingNone) {
    dlist[(*count)++] = MakeHvsLbmBase(upm_slot);
  }

  if (!unity) {
    HvsScalingParameter parameter_order[4];
    const unsigned parameter_count = BuildHvsScalingParameterOrder(
        x_scaling, y_scaling, parameter_order);
    for (unsigned i = 0; i < parameter_count; ++i) {
      switch (parameter_order[i]) {
        case kHvsHorizontalPpf:
          dlist[(*count)++] = MakeHvsPpfWord(plane.source.width,
                                             plane.destination.width,
                                             (u32)plane.source.x << 16,
                                             nearest);
          break;
        case kHvsVerticalPpf:
          dlist[(*count)++] = MakeHvsPpfWord(plane.source.height,
                                             plane.destination.height,
                                             (u32)plane.source.y << 16,
                                             nearest);
          dlist[(*count)++] = 0xC0C0C0C0U;
          break;
        case kHvsHorizontalTpz:
          dlist[(*count)++] = MakeHvsTpzWord0(plane.source.width,
                                              plane.destination.width);
          dlist[(*count)++] = MakeHvsTpzWord1(plane.source.width,
                                              plane.destination.width);
          break;
        case kHvsVerticalTpz:
          dlist[(*count)++] = MakeHvsTpzWord0(plane.source.height,
                                              plane.destination.height);
          dlist[(*count)++] = MakeHvsTpzWord1(plane.source.height,
                                              plane.destination.height);
          dlist[(*count)++] = 0xC0C0C0C0U;
          break;
      }
    }

    if (x_scaling == kHvsScalingPpf || y_scaling == kHvsScalingPpf) {
      const u32 kernel_slot = nearest ? kHvsFilterNearestSlot
                                      : kHvsFilterMitchellSlot;
      dlist[(*count)++] = kernel_slot;
      dlist[(*count)++] = kernel_slot;
      dlist[(*count)++] = kernel_slot;
      dlist[(*count)++] = kernel_slot;
    }
  }

  if (*count >= capacity) {
    return false;
  }
  // NEXT skips over this per-plane END. The composed CRTC list still needs
  // a final END after the last plane, matching the Linux VC6 HVS path.
  dlist[(*count)++] = kHvsCtl0End;

  const unsigned plane_words = *count - start;
  if (plane_words > 0x3FU) {
    return false;
  }

  dlist[start] = MakeHvsPlaneCtl0(plane_words, unity, plane.format, scl_field);
  if (dlist[start] == 0) {
    return false;
  }

  return true;
}

bool ValidatePlane(const Plane &plane, u32 display_width,
                   u32 display_height) {
  if (plane.framebuffer_bus_address == 0 || plane.pitch == 0 ||
      plane.width == 0 || plane.height == 0 ||
      display_width == 0 || display_height == 0) {
    return false;
  }

  if ((plane.format == kPixelFormatRgb565 && plane.depth != 16) ||
      (plane.format == kPixelFormatArgb8888 && plane.depth != 32) ||
      (plane.format != kPixelFormatRgb565 &&
       plane.format != kPixelFormatArgb8888)) {
    printf("boot: pi5kms unsupported plane format depth %u format %u\r\n",
           plane.depth, (unsigned)plane.format);
    return false;
  }
  if (plane.filter != kScaleFilterNearest &&
      plane.filter != kScaleFilterMitchell) {
    printf("boot: pi5kms unsupported scale filter %u\r\n",
           (unsigned)plane.filter);
    return false;
  }

  if (plane.pitch > 0x1FFFFU || plane.width > 0x1FFFU ||
      plane.height > 0x1FFFU || display_width > 0x1FFFU ||
      display_height > 0x1FFFU) {
    printf("boot: pi5kms plane geometry out of range\r\n");
    return false;
  }

  if (!IsValidRect(plane.source) || !IsValidRect(plane.destination)) {
    printf("boot: pi5kms invalid plane rect\r\n");
    return false;
  }

  if ((u32)plane.source.x + plane.source.width > plane.width ||
      (u32)plane.source.y + plane.source.height > plane.height ||
      (u32)plane.destination.x + plane.destination.width > display_width ||
      (u32)plane.destination.y + plane.destination.height > display_height) {
    printf("boot: pi5kms plane rect outside source or display\r\n");
    return false;
  }

  return true;
}

void LogScanoutPlanes(const Plane *planes, unsigned plane_count,
                      u32 dlist_slot) {
  for (unsigned i = 0; i < plane_count; ++i) {
    printf("boot: pi5kms scanout plane %u fb 0x%08x src %d,%d %ux%u "
           "dst %d,%d %ux%u pitch %u depth %u dlist %u\r\n",
           i,
           planes[i].framebuffer_bus_address,
           planes[i].source.x, planes[i].source.y,
           planes[i].source.width, planes[i].source.height,
           planes[i].destination.x, planes[i].destination.y,
           planes[i].destination.width, planes[i].destination.height,
           planes[i].pitch, planes[i].depth, dlist_slot);
  }
}

bool BuildScanoutDlist(const Plane *planes, unsigned plane_count,
                       u32 display_width, u32 display_height,
                       u32 *dlist, unsigned dlist_capacity,
                       unsigned *dlist_count,
                       unsigned *ptr0_words,
                       unsigned *address_words) {
  if (planes == nullptr || plane_count == 0 ||
      dlist == nullptr || dlist_count == nullptr ||
      ptr0_words == nullptr || address_words == nullptr) {
    return false;
  }

  for (unsigned i = 0; i < plane_count; ++i) {
    if (!ValidatePlane(planes[i], display_width, display_height)) {
      return false;
    }
  }

  EnsureHvsFilterKernelsUploaded();

  unsigned count = 0;
  for (unsigned i = 0; i < plane_count; ++i) {
    if (!AppendPlaneDlist(planes[i], i, dlist, dlist_capacity, &count,
                          &ptr0_words[i], &address_words[i])) {
      printf("boot: pi5kms display list build failed at plane %u\r\n", i);
      return false;
    }
  }

  if (count >= dlist_capacity) {
    printf("boot: pi5kms display list overflow %u\r\n", count);
    return false;
  }
  dlist[count++] = kHvsCtl0End;
  *dlist_count = count;
  return true;
}

bool SameRect(const Rect &left, const Rect &right) {
  return left.x == right.x && left.y == right.y &&
         left.width == right.width && left.height == right.height;
}

bool SamePlaneLayout(const Plane &left, const Plane &right) {
  return left.pitch == right.pitch && left.width == right.width &&
         left.height == right.height && left.depth == right.depth &&
         left.format == right.format && left.filter == right.filter &&
         SameRect(left.source, right.source) &&
         SameRect(left.destination, right.destination);
}

bool MatchesScanoutTemplate(const ScanoutDlistTemplate &dlist_template,
                            const Plane *planes, unsigned plane_count,
                            u32 display_width, u32 display_height) {
  if (!dlist_template.valid || dlist_template.plane_count != plane_count ||
      dlist_template.display_width != display_width ||
      dlist_template.display_height != display_height) {
    return false;
  }
  for (unsigned i = 0; i < plane_count; ++i) {
    if (planes[i].framebuffer_bus_address == 0 ||
        !SamePlaneLayout(dlist_template.planes[i], planes[i])) {
      return false;
    }
  }
  return true;
}

void RememberScanoutLayout(ScanoutDlistTemplate *dlist_template,
                           const Plane *planes, unsigned plane_count,
                           u32 display_width, u32 display_height) {
  dlist_template->valid = true;
  dlist_template->plane_count = plane_count;
  dlist_template->display_width = display_width;
  dlist_template->display_height = display_height;
  for (unsigned i = 0; i < plane_count; ++i) {
    dlist_template->planes[i] = planes[i];
  }
}

void PatchScanoutAddresses(ScanoutDlistTemplate *dlist_template,
                           const Plane *planes) {
  for (unsigned i = 0; i < dlist_template->plane_count; ++i) {
    const u32 bytes_per_pixel = planes[i].depth / 8;
    const u32 source_offset =
        (u32)planes[i].source.y * planes[i].pitch +
        (u32)planes[i].source.x * bytes_per_pixel;
    const u32 address = planes[i].framebuffer_bus_address + source_offset;
    dlist_template->words[dlist_template->ptr0_words[i]] =
        MakeHvsPtr0(address, i);
    dlist_template->words[dlist_template->address_words[i]] = address;
  }
}

u32 HvsDlistSlot(unsigned dlist_index) {
  return dlist_index == 0 ? kHvsDlistSlot0 : kHvsDlistSlot1;
}

bool ProgramScanout(const Plane *planes, unsigned plane_count,
                    u32 display_width, u32 display_height,
                    bool wait_for_vblank, bool log_planes,
                    bool program_channel) {
#ifdef BMC64_DEBUG_PROFILE
  u32 timing_start_us = 0;
  u32 wait_start_us = 0;
  u32 wait_done_us = 0;
  bool wait_requested = false;
  if (!program_channel) {
    timing_start_us = CTimer::GetClockTicks();
    ++g_last_present_timing.sequence;
    g_last_present_timing.valid = false;
    g_last_present_timing.wait_requested = false;
    g_last_present_timing.wait_us = 0;
    g_last_present_timing.total_us = 0;
  }
#endif
  if (planes == nullptr || plane_count == 0) {
    return false;
  }

  const unsigned dlist_index =
      (program_channel || !g_hvs_has_active_dlist)
          ? 0
          : (g_hvs_submitted_dlist ^ 1U);
  const u32 dlist_slot = HvsDlistSlot(dlist_index);
  const u32 ctrl0 = HvsDisp0Ctrl0Offset();
  const u32 ctrl1 = HvsDisp0Ctrl1Offset();
  const u32 bgnd0 = HvsDisp0Bgnd0Offset();
  const u32 lptrs = HvsDisp0LptrsOffset();
  const bool background_fill =
      NeedsBackgroundFill(planes, plane_count, display_width, display_height);

  u32 uncached_dlist[kHvsScanoutDlistWords];
  unsigned uncached_ptr0_words[kHvsScanoutDlistWords];
  unsigned uncached_address_words[kHvsScanoutDlistWords];
  const u32 *dlist = uncached_dlist;
  unsigned count = 0;
  bool cache_hit = false;
  if (plane_count <= kHvsScanoutTemplatePlanes) {
    ScanoutDlistTemplate *dlist_template =
        &g_scanout_dlist_templates[dlist_index];
    cache_hit = MatchesScanoutTemplate(
        *dlist_template, planes, plane_count, display_width, display_height);
    if (!cache_hit) {
      if (!BuildScanoutDlist(
              planes, plane_count, display_width, display_height,
              dlist_template->words, kHvsScanoutDlistWords,
              &dlist_template->word_count, dlist_template->ptr0_words,
              dlist_template->address_words)) {
        dlist_template->valid = false;
        return false;
      }
      RememberScanoutLayout(dlist_template, planes, plane_count,
                            display_width, display_height);
    } else {
      PatchScanoutAddresses(dlist_template, planes);
      if (!g_scanout_dlist_cache_hit_logged[dlist_index]) {
        g_scanout_dlist_cache_hit_logged[dlist_index] = true;
        printf("boot: pi5kms dlist template slot=%u status=hit words=%u "
               "planes=%u\r\n", dlist_index, dlist_template->word_count,
               plane_count);
      }
    }
    dlist = dlist_template->words;
    count = dlist_template->word_count;
  } else if (!BuildScanoutDlist(
                 planes, plane_count, display_width, display_height,
                 uncached_dlist, kHvsScanoutDlistWords, &count,
                 uncached_ptr0_words, uncached_address_words)) {
    return false;
  }

  if (log_planes) {
    LogScanoutPlanes(planes, plane_count, dlist_slot);
  }

  if (program_channel) {
    WriteReg(kHvsBase, kHvsControl,
             ReadReg(kHvsBase, kHvsControl) | kHvsControlEnable);
  }

  if (!program_channel && wait_for_vblank && g_hvs_has_active_dlist) {
#ifdef BMC64_DEBUG_PROFILE
    wait_requested = true;
    wait_start_us = CTimer::GetClockTicks();
#endif
    if (!WaitForVBlank()) {
      if (!g_hvs_vblank_timeout_logged) {
        printf("boot: pi5kms vblank wait timed out; presenting anyway\r\n");
        g_hvs_vblank_timeout_logged = true;
      }
    }
#ifdef BMC64_DEBUG_PROFILE
    wait_done_us = CTimer::GetClockTicks();
#endif
  }

  for (unsigned i = 0; i < count; ++i) {
    WriteDlistWord(dlist_slot + i, dlist[i]);
  }

  if (program_channel) {
    ProgramHvsCob0();
    WriteReg(kHvsBase, bgnd0, 0);
  }

  if (background_fill) {
    WriteReg(kHvsBase, ctrl1,
             ReadReg(kHvsBase, ctrl1) | kHvsDispCtrl1BgEnable);
  } else {
    WriteReg(kHvsBase, ctrl1,
             ReadReg(kHvsBase, ctrl1) & ~kHvsDispCtrl1BgEnable);
  }

  WriteReg(kHvsBase, lptrs, dlist_slot);
  if (program_channel) {
    WriteReg(kHvsBase, ctrl0,
             kHvsDispCtrl0Enable |
             ((display_width - 1U) << 16) |
             (display_height - 1U));
  }

  g_hvs_submitted_dlist = dlist_index;
  g_hvs_has_active_dlist = true;
#ifdef BMC64_DEBUG_PROFILE
  if (!program_channel) {
    const u32 timing_done_us = CTimer::GetClockTicks();
    g_last_present_timing.valid = true;
    g_last_present_timing.wait_requested = wait_requested;
    g_last_present_timing.wait_us =
        wait_requested ? wait_done_us - wait_start_us : 0;
    g_last_present_timing.total_us = timing_done_us - timing_start_us;
  }
#endif
  return true;
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

void InitVc6Phy(u32 tmds_rate) {
  u32 vco_div = 0;
  const unsigned long long vco_freq = GetVc6VcoFreq(tmds_rate, &vco_div);
  const u32 rm_offset = 0x80000000U | GetRmOffset(vco_freq);

  if (tmds_rate > 222000000) {
    printf("boot: pi5kms warning tmds %u above validated lane table\r\n",
           tmds_rate);
  }

  WriteReg(g_port->phy, kPhyResetCtl, 0);
  WriteReg(g_port->phy, kPhyPowerupCtl, 0);
  WriteReg(g_port->phy, kPhyPllPostKdiv, kPhyPllPostKdivBypass);

  WriteReg(g_port->phy, kPhyPllMisc0, 0x810C6000);
  WriteReg(g_port->phy, kPhyPllMisc1, 0x00B8C451);
  WriteReg(g_port->phy, kPhyPllMisc2, 0x46402E31);
  WriteReg(g_port->phy, kPhyPllMisc3, 0x00B8C005);
  WriteReg(g_port->phy, kPhyPllMisc4, 0x42410261);
  WriteReg(g_port->phy, kPhyPllMisc5, 0xCC021001);
  WriteReg(g_port->phy, kPhyPllMisc6, 0xC8301C80);
  WriteReg(g_port->phy, kPhyPllMisc7, 0xB0804444);
  WriteReg(g_port->phy, kPhyPllMisc8, 0xF80F8000);

  WriteReg(g_port->phy, kPhyPllRefclk, 0x00002036);
  WriteReg(g_port->phy, kPhyResetCtl, 0x0000007F);
  WriteReg(g_port->rm, kRmOffset, rm_offset);
  WriteReg(g_port->phy, kPhyPllVcoclkDiv, 0x00000400 | vco_div);
  WriteReg(g_port->phy, kPhyPllCfg, 0);
  WriteReg(g_port->phy, kPhyPllPostKdiv, 0x00000009);

  WriteReg(g_port->phy, kPhyCtl0, 0x80828700);
  WriteReg(g_port->phy, kPhyCtl1, 0x80828700);
  WriteReg(g_port->phy, kPhyCtl2, 0x80828700);
  WriteReg(g_port->phy, kPhyCtlCk, 0x80828700);
  WriteReg(g_port->phy, kPhyTmdsClkWordSel, 0);

  WriteReg(g_port->phy, kPhyPowerupCtl, 0x000001CF);
  WriteReg(g_port->phy, kPhyPllPowerupCtl, 0x00000001);
  WriteReg(g_port->phy, kPhyPllResetCtl,
           ReadReg(g_port->phy, kPhyPllResetCtl) & ~kPhyPllResetPllResetb);
  WriteReg(g_port->phy, kPhyPllResetCtl,
           ReadReg(g_port->phy, kPhyPllResetCtl) | kPhyPllResetPllResetb);
}

void ServiceInitialSinkAttach() {
  if (!g_waiting_for_initial_sink || !g_active_mode_valid) {
    return;
  }

  if (!HdmiSinkConnected()) {
    g_initial_sink_detected_us = 0;
    return;
  }

  const u64 now_us = CTimer::GetClockTicks64();
  if (g_initial_sink_detected_us == 0) {
    g_initial_sink_detected_us = now_us;
    printf("boot: pi5kms initial sink detected hotplug=0x%08x "
           "state=settling settle_us=%llu\r\n",
           ReadReg(g_port->core, kHdmiHotplug),
           static_cast<unsigned long long>(kInitialSinkSettleUs));
    return;
  }
  if (now_us - g_initial_sink_detected_us < kInitialSinkSettleUs) {
    return;
  }

  // This is a one-shot startup completion, not a runtime link monitor.  A
  // stable first HPD completes the live headless mode using the normal
  // BCM2712 disable/reconfigure/enable lifecycle and then retires the check.
  g_waiting_for_initial_sink = false;
  g_initial_sink_detected_us = 0;
  const Mode mode = g_active_mode;
  printf("boot: pi5kms initial sink stable hotplug=0x%08x "
         "action=live-modeset\r\n",
         ReadReg(g_port->core, kHdmiHotplug));

  const bool ready = SetMode(mode);
  printf("boot: pi5kms initial sink completion status=%s "
         "monitor=retired\r\n",
         ready ? "ready" : "failed");
}









void FermaCanale(unsigned porta) {
  const uintptr pv = kHdmiPorts[porta].pv;
  const u32 ctl_prima = ReadReg(pv, kPvControl);
  const u32 vctl_prima = ReadReg(pv, kPvVControl);
  if ((ctl_prima & kPvControlEnable) != 0) {
    WriteReg(pv, kPvVControl, vctl_prima & ~kPvVControlVideoEnable);
    DataSyncBarrier();
    CTimer::SimpleusDelay(20000);
    WriteReg(pv, kPvControl, ctl_prima & ~kPvControlEnable);
    WriteReg(pv, kPvControl,
             (ctl_prima & ~kPvControlEnable) | kPvControlFifoClear);
  }
  const u32 version = ReadReg(kHvsBase, kHvsVersion) & 0xFFU;
  const u32 ctrl0 =
      (version == kHvsVersionD0 ? kHvs6dDisp0Ctrl0 : kHvs6cDisp0Ctrl0) +
      kHdmiPorts[porta].hvs_channel *
          (version == kHvsVersionD0 ? 0x40U : 0x20U);
  const u32 hvs_prima = ReadReg(kHvsBase, ctrl0);
  if ((hvs_prima & kHvsDispCtrl0Enable) != 0) {
    WriteReg(kHvsBase, ctrl0, hvs_prima | kHvsDispCtrl0Reset);
    WriteReg(kHvsBase, ctrl0,
             ReadReg(kHvsBase, ctrl0) & ~kHvsDispCtrl0Enable);
  }
  DataSyncBarrier();
  printf("boot: pi5kms fermato il canale %u "
         "pv ctl=0x%08x->0x%08x vctl=0x%08x->0x%08x "
         "hvs=0x%08x->0x%08x\r\n",
         porta, ctl_prima, ReadReg(pv, kPvControl), vctl_prima,
         ReadReg(pv, kPvVControl), hvs_prima, ReadReg(kHvsBase, ctrl0));
}







void ScegliPorta() {
  if (g_porta_scelta) {
    return;
  }
  g_porta_scelta = true;
  g_port = &kHdmiPorts[0];
  if (g_requested_hdmi_port != 1U) {
    return;
  }
  const u32 version = ReadReg(kHvsBase, kHvsVersion) & 0xFFU;
  const bool hvs_noto = version == 0x53U || version == kHvsVersionD0;
  const u32 pv1_ctl = ReadReg(kHdmiPorts[1].pv, kPvControl);
  const u32 pv1_vctl = ReadReg(kHdmiPorts[1].pv, kPvVControl);
  const bool pv1_accesa = (pv1_ctl & kPvControlEnable) != 0 &&
                          (pv1_vctl & kPvVControlVideoEnable) != 0;
  const u32 hotplug =
      (hvs_noto && pv1_accesa) ? ReadReg(kHdmiPorts[1].core, kHdmiHotplug)
                               : 0U;
  const bool presa = hvs_noto && pv1_accesa &&
                     (hotplug & kHdmiHotplugConnected) != 0;
  printf("boot: pi5kms porta chiesta 1: hvs 0x%02x pv1 ctl=0x%08x "
         "vctl=0x%08x hotplug=0x%08x -> %s\r\n",
         version, pv1_ctl, pv1_vctl, hotplug,
         presa ? "presa la HDMI 1" : "resto sulla HDMI 0");
  if (presa) {
    g_port = &kHdmiPorts[1];
    FermaCanale(0U);
  }
}

}  // namespace

bool ResolveBmcMode(unsigned hdmi_group, unsigned hdmi_mode,
                    const char *hdmi_timings, const char *named_mode,
                    Mode *mode) {
  return bmxkms::ResolveBmcMode(
      hdmi_group, hdmi_mode, hdmi_timings, named_mode,
      bmxkms::kTimingParsePermissive, mode);
}

bool SetMode(const Mode &mode) {
  if ((mode.width & 1) || (mode.h_front_porch & 1) || (mode.h_sync & 1) ||
      (mode.h_back_porch & 1)) {
    printf("boot: pi5kms unsupported odd horizontal timing\r\n");
    return false;
  }

  ScegliPorta();
  printf("boot: pi5kms set %ux%u pclk %u\r\n",
         mode.width, mode.height, mode.pixel_clock);
  const bool live_modeset = g_active_mode_valid;
  const u32 packet_config_before =
      live_modeset ? QuiesceHdmiOutputForModeSet() : 0;
  g_active_mode_valid = false;
  g_waiting_for_initial_sink = false;
  g_initial_sink_detected_us = 0;

  const u32 pv_control_before = ReadReg(g_port->pv, kPvControl);
  const u32 pv_vcontrol_before = ReadReg(g_port->pv, kPvVControl);
  WriteReg(g_port->pv, kPvControl, pv_control_before & ~kPvControlEnable);
  WriteReg(g_port->pv, kPvControl,
           (pv_control_before & ~kPvControlEnable) | kPvControlFifoClear);


  bool (*const clock_condiviso)(unsigned, u32) =
      g_port->hvs_channel == 1U ? SetClockRateAlmeno : SetClockRate;
  if (!SetClockRate(kClockPixel, mode.pixel_clock) ||
      !clock_condiviso(kClockM2mc, GetHsmClock(mode.pixel_clock)) ||
      !clock_condiviso(kClockPixelBvb, GetBvbClock(mode.pixel_clock)) ||
      !clock_condiviso(kClockDisp, GetHsmClock(mode.pixel_clock))) {
    printf("boot: pi5kms clock setup failed\r\n");
    WriteReg(g_port->pv, kPvControl, pv_control_before);
    WriteReg(g_port->pv, kPvVControl, pv_vcontrol_before);
    return false;
  }

  if (!live_modeset) {
    ResetHdmiCoreForModeSet();
  } else {
    printf("boot: pi5kms hdmi core reset skipped for live modeset\r\n");
  }
  InitVc6Phy(mode.pixel_clock);

  WriteReg(g_port->dvp, kHdmiClockStop, 0);
  WriteReg(g_port->core, kHdmiSchedulerControl,
           ReadReg(g_port->core, kHdmiSchedulerControl) |
               kHdmiSchedulerManualFormat |
               kHdmiSchedulerIgnoreVsyncPredicts);
  WriteReg(g_port->core, kHdmiHorza, MakeHdmiHorza(mode));
  WriteReg(g_port->core, kHdmiHorzb, MakeHdmiHorzb(mode));
  WriteReg(g_port->core, kHdmiVerta0, MakeHdmiVerta(mode));
  WriteReg(g_port->core, kHdmiVertb0, MakeHdmiVertb(mode));
  WriteReg(g_port->core, kHdmiVerta1, MakeHdmiVerta(mode));
  WriteReg(g_port->core, kHdmiVertb1, MakeHdmiVertb(mode));
  WriteReg(g_port->core, kHdmiMiscControl,
           ReadReg(g_port->core, kHdmiMiscControl) & ~0xFU);

  ConfigureHdmiRgbOutput(mode);

  u32 deep_color = ReadReg(g_port->core, kHdmiDeepColorConfig1);
  deep_color &= ~(kHdmiDeepColorInitPackPhaseMask |
                  kHdmiDeepColorDepthMask);
  deep_color |= kHdmiDeepColorInitPackPhase8Bpc;
  WriteReg(g_port->core, kHdmiDeepColorConfig1, deep_color);

  u32 gcp_word1 = ReadReg(g_port->core, kHdmiGcpWord1);
  gcp_word1 &= ~kHdmiGcpSubpacketBytes01Mask;
  gcp_word1 |= kHdmiGcpClearAvMute;
  WriteReg(g_port->core, kHdmiGcpWord1, gcp_word1);
  WriteReg(g_port->core, kHdmiGcpConfig,
           ReadReg(g_port->core, kHdmiGcpConfig) | kHdmiGcpEnable);

  ConfigurePixelValve(mode);

  const u32 fifo = ReadReg(g_port->core, kHdmiFifoCtl) &
                   kHdmiFifoValidWriteMask;
  WriteReg(g_port->core, kHdmiFifoCtl,
           fifo | kHdmiFifoMasterSlaveN);

  ProgramHvsDisplay0(mode);

  WriteReg(g_port->pv, kPvControl,
           PvConfiguredControl() | kPvControlEnable);
  WriteReg(g_port->pv, kPvVControl,
           PvConfiguredVControl() | kPvVControlVideoEnable);

  if (live_modeset) {
    // Preserve packet producers which remain active across this one-shot
    // transition (notably HDMI audio).  AVI is rewritten below for the mode.
    WriteReg(g_port->core, kHdmiRamPacketConfig,
             packet_config_before & ~kHdmiAviPacketEnable);
  }

  bool output_ready = EnableHdmiOutput(mode);


  if (output_ready && g_port->hvs_channel == 1U &&
      !WaitForNextVBlank(100000)) {
    printf("boot: pi5kms hdmi 1: il canale 1 non da' il ritorno di "
           "quadro\r\n");
    output_ready = false;
  }




  if (!output_ready && !live_modeset && g_port->hvs_channel == 1U) {
    printf("boot: pi5kms hdmi 1 non partita: ripiego sulla HDMI 0\r\n");
    FermaCanale(1U);
    g_port = &kHdmiPorts[0];
    return SetMode(mode);
  }
  if (output_ready) {
    g_active_mode = mode;
    g_active_mode_valid = true;
    g_waiting_for_initial_sink = !HdmiSinkConnected();
  }
  printf("boot: pi5kms set complete output=%s\r\n",
         output_ready ? "ready" : "not-ready");
  printf("boot: pi5kms startup sink hotplug=0x%08x state=%s\r\n",
         ReadReg(g_port->core, kHdmiHotplug),
         g_waiting_for_initial_sink ? "waiting-initial-attach" : "complete");
  return output_ready;
}

bool CreateFramebuffer(u32 width, u32 height, u32 depth, Framebuffer *fb) {
  if (fb == nullptr || width == 0 || height == 0 ||
      (depth != 16 && depth != 32)) {
    return false;
  }

  const u32 bytes_per_pixel = depth / 8;
  const u32 pitch = ALIGN_UP(width * bytes_per_pixel, 64);
  const u32 size = pitch * height;
  uint8_t *pixels = new (HEAP_DMA30) uint8_t[size];
  if (pixels == nullptr) {
    return false;
  }

  fb->allocation = pixels;
  fb->pixels = pixels;
  fb->width = width;
  fb->height = height;
  fb->pitch = pitch;
  fb->depth = depth;
  fb->size = size;

  ClearFramebuffer(*fb);

  printf("boot: pi5kms framebuffer 0x%08x %ux%u depth %u pitch %u size %u\r\n",
         (u32)(uintptr)fb->pixels, fb->width, fb->height, fb->depth,
         fb->pitch, fb->size);

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
  if (fb.pixels == nullptr || fb.size == 0) {
    return;
  }

  memset(fb.pixels, 0, fb.size);
  FlushFramebuffer(fb);
}

void FlushFramebuffer(const Framebuffer &fb) {
  if (fb.pixels == nullptr || fb.size == 0) {
    return;
  }

  CleanDataCacheRange((u64)(uintptr)fb.pixels, fb.size);
}

void FlushFramebufferRows(const Framebuffer &fb, u32 first_row,
                          u32 row_count) {
  if (fb.pixels == nullptr || fb.pitch == 0 || first_row >= fb.height ||
      row_count == 0) {
    return;
  }

  if (row_count > fb.height - first_row) {
    row_count = fb.height - first_row;
  }
  CleanDataCacheRange(
      (u64)(uintptr)(fb.pixels + first_row * fb.pitch),
      row_count * fb.pitch);
}

u32 HvsDisplay0Mode() {
  return (ReadReg(kHvsBase, HvsDisp0StatusOffset()) &
          kHvsStatusModeMask) >> kHvsStatusModeShift;
}

bool WaitForVBlank(unsigned timeout_us) {
  const unsigned start = CTimer::GetClockTicks();

  while ((unsigned)(CTimer::GetClockTicks() - start) < timeout_us) {
    if (HvsDisplay0Mode() == kHvsStatusModeEof) {
      return true;
    }
    CTimer::SimpleusDelay(10);
  }

  return false;
}

bool WaitForNextVBlank(unsigned timeout_us) {
  const unsigned start = CTimer::GetClockTicks();
  bool left_end_of_frame = HvsDisplay0Mode() != kHvsStatusModeEof;

  while ((unsigned)(CTimer::GetClockTicks() - start) < timeout_us) {
    const u32 mode = HvsDisplay0Mode();
    if (mode != kHvsStatusModeEof) {
      left_end_of_frame = true;
    } else if (left_end_of_frame) {
      return true;
    }
    CTimer::SimpleusDelay(10);
  }

  return false;
}

bool ConfigureScanout(const Framebuffer &fb) {
  if (fb.depth != 16) {
    printf("boot: pi5kms framebuffer scanout supports RGB565 depth 16 only\r\n");
    return false;
  }
  return ConfigureScanout((u32)(uintptr)fb.pixels, fb.pitch, fb.width,
                          fb.height, fb.depth);
}

bool ConfigureScanout(u32 framebuffer_bus_address, u32 pitch, u32 width,
                      u32 height, u32 depth) {
  if (depth != 16) {
    printf("boot: pi5kms scanout supports RGB565 depth 16 only\r\n");
    return false;
  }

  Plane plane = {
    framebuffer_bus_address,
    pitch,
    width,
    height,
    depth,
    kPixelFormatRgb565,
    kScaleFilterNearest,
    {0, 0, width, height},
    {0, 0, width, height}
  };

  return ConfigureScanout(plane, width, height);
}

bool ConfigureScanout(const Plane &plane, u32 display_width,
                      u32 display_height) {
  return ConfigureScanout(&plane, 1, display_width, display_height);
}

bool ConfigureScanout(const Plane *planes, unsigned plane_count,
                      u32 display_width, u32 display_height) {
  return ProgramScanout(planes, plane_count, display_width, display_height,
                        false, true, true);
}

bool PresentScanout(const Plane *planes, unsigned plane_count,
                    u32 display_width, u32 display_height,
                    bool wait_for_vblank) {
  ServiceInitialSinkAttach();
  return ProgramScanout(planes, plane_count, display_width, display_height,
                        wait_for_vblank, false, false);
}

bool GetLastPresentTiming(PresentTiming *timing) {
  if (timing == nullptr) {
    return false;
  }
  *timing = g_last_present_timing;
  return timing->valid;
}









void SondaHdmi1() {
  printf("boot: pi5kms sonda-hdmi1 hvs version=0x%08x control=0x%08x\r\n",
         ReadReg(kHvsBase, kHvsVersion), ReadReg(kHvsBase, kHvsControl));
  static const u32 kBlocchi[4] = {0x030U, 0x050U, 0x100U, 0x140U};
  for (unsigned i = 0; i < 4U; ++i) {
    const u32 b = kBlocchi[i];
    printf("boot: pi5kms sonda-hdmi1 hvs@0x%03x %08x %08x %08x %08x %08x "
           "%08x %08x %08x %08x\r\n", b,
           ReadReg(kHvsBase, b + 0x00U), ReadReg(kHvsBase, b + 0x04U),
           ReadReg(kHvsBase, b + 0x08U), ReadReg(kHvsBase, b + 0x0CU),
           ReadReg(kHvsBase, b + 0x10U), ReadReg(kHvsBase, b + 0x14U),
           ReadReg(kHvsBase, b + 0x18U), ReadReg(kHvsBase, b + 0x1CU),
           ReadReg(kHvsBase, b + 0x20U));
  }
  for (unsigned pv = 0; pv < 2U; ++pv) {
    const uintptr b = kPv0Base + pv * 0x1000U;
    printf("boot: pi5kms sonda-hdmi1 pv%u control=0x%08x vcontrol=0x%08x "
           "horz=0x%08x/0x%08x vert=0x%08x/0x%08x mux=0x%08x "
           "pipe=0x%08x\r\n", pv,
           ReadReg(b, kPvControl), ReadReg(b, kPvVControl),
           ReadReg(b, kPvHorza), ReadReg(b, kPvHorzb),
           ReadReg(b, kPvVerta), ReadReg(b, kPvVertb),
           ReadReg(b, kPvMuxCfg), ReadReg(b, kPvPipeInitCtrl));
  }
}






void RequestHdmiPort(unsigned port) {
  g_requested_hdmi_port = port == 1U ? 1U : 0U;
}

unsigned ActiveHdmiPort() {
  return g_port == &kHdmiPorts[1] ? 1U : 0U;
}

bool HdmiPortSupported(unsigned port) {
  return port <= 1U;
}

}  // namespace pi5kms
