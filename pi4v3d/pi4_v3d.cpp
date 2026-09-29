#include "pi4v3d/pi4_v3d.h"

#if RASPPI != 4
#error pi4_v3d.cpp is only valid for Raspberry Pi 4 builds
#endif

#include <circle/bcm2835.h>
#include <circle/bcmpropertytags.h>
#include <circle/memio.h>
#include <circle/new.h>
#include <circle/synchronize.h>
#include <circle/timer.h>
#include <circle/types.h>
#include <stdio.h>
#include <string.h>

#include "pi4v3d/pi4_v3d_identity.h"
#include "pi4v3d/pi4_v3d_effect_policy.h"
#include "pi4v3d/pi4_v3d_mmu.h"
#include "pi4v3d/pi4_v3d_power.h"
#include "pi4v3d/pi4_v3d_render.h"
#include "pi4v3d/pi4_v3d42_texture_state.h"
#include "v3dcrt/edge_glow_filter.h"

namespace pi4v3d {

namespace {

const uintptr kV3dHubBase = ARM_IO_BASE + 0x00c00000U;
const uintptr kV3dCoreBase = ARM_IO_BASE + 0x00c04000U;
const uintptr kRpividAsbBase = ARM_IO_BASE + 0x00c11000U;
const uintptr kPmGrafx = ARM_PM_BASE + 0x10cU;

const u32 kHubIdent0 = 0x0008U;
const u32 kHubIdent1 = 0x000cU;
const u32 kHubIdent2 = 0x0010U;
const u32 kHubIdent3 = 0x0014U;
const u32 kHubIntStatus = 0x0050U;
const u32 kHubIntClear = 0x0058U;
const u32 kHubMmuControl = 0x1000U;
const u32 kHubMmuCtl = 0x1200U;
const u32 kHubMmuPtPaBase = 0x1204U;
const u32 kHubMmuVioId = 0x122cU;
const u32 kHubMmuIllegalAddr = 0x1230U;
const u32 kHubMmuVioAddr = 0x1234U;
const u32 kHubMmuDebugInfo = 0x1238U;
const u32 kHubTfuCs = 0x0400U;
const u32 kHubTfuIcfg = 0x0408U;
const u32 kHubTfuIia = 0x040cU;
const u32 kHubTfuIca = 0x0410U;
const u32 kHubTfuIis = 0x0414U;
const u32 kHubTfuIua = 0x0418U;
const u32 kHubTfuIoa = 0x041cU;
const u32 kHubTfuIos = 0x0420U;
const u32 kHubTfuCoef0 = 0x0424U;
const u32 kHubTfuCoef1 = 0x0428U;
const u32 kHubTfuCoef2 = 0x042cU;
const u32 kHubTfuCoef3 = 0x0430U;
const u32 kCoreIdent0 = 0x0000U;
const u32 kCoreIdent1 = 0x0004U;
const u32 kCoreIdent2 = 0x0008U;
const u32 kCoreSliceCacheControl = 0x0024U;
const u32 kCoreL2TCacheControl = 0x0030U;
const u32 kCoreL2TFlushStart = 0x0034U;
const u32 kCoreL2TFlushEnd = 0x0038U;
const u32 kCoreIntStatus = 0x0050U;
const u32 kCoreIntClear = 0x0058U;
const u32 kCoreCt0Cs = 0x0100U;
const u32 kCoreCt1Cs = 0x0104U;
const u32 kCoreCt0Ea = 0x0108U;
const u32 kCoreCt1Ea = 0x010cU;
const u32 kCoreCt0Ca = 0x0110U;
const u32 kCoreCt1Ca = 0x0114U;
const u32 kCoreCt0Pc = 0x0128U;
const u32 kCoreCt1Pc = 0x012cU;
const u32 kCoreCt0Qts = 0x015cU;
const u32 kCoreCt0Qba = 0x0160U;
const u32 kCoreCt1Qba = 0x0164U;
const u32 kCoreCt0Qea = 0x0168U;
const u32 kCoreCt1Qea = 0x016cU;
const u32 kCoreCt0Qma = 0x0170U;
const u32 kCoreCt0Qms = 0x0174U;

const u32 kHubIntMmuWriteViolation = 1U << 5;
const u32 kHubIntMmuPtInvalid = 1U << 4;
const u32 kHubIntMmuCapExceeded = 1U << 3;
const u32 kHubIntTfuComplete = 1U << 1;
const u32 kHubIntTfuFault = 1U << 0;
const u32 kHubIntMmuFaultMask = kHubIntMmuWriteViolation |
                                kHubIntMmuPtInvalid |
                                kHubIntMmuCapExceeded;
const u32 kMmuControlFlushing = 1U << 2;
const u32 kMmuControlFlush = 1U << 1;
const u32 kMmuControlEnable = 1U << 0;
const u32 kMmuCtlCapExceededAbort = 1U << 26;
const u32 kMmuCtlCapExceededInt = 1U << 25;
const u32 kMmuCtlPtInvalidAbort = 1U << 19;
const u32 kMmuCtlPtInvalidInt = 1U << 18;
const u32 kMmuCtlPtInvalidEnable = 1U << 16;
const u32 kMmuCtlWriteViolationAbort = 1U << 11;
const u32 kMmuCtlWriteViolationInt = 1U << 10;
const u32 kMmuCtlTlbClearing = 1U << 7;
const u32 kMmuCtlTlbClear = 1U << 2;
const u32 kMmuCtlEnable = 1U << 0;
const u32 kMmuIllegalAddrEnable = 1U << 31;
const u32 kTfuCsConversionCountMask = 0x00ff0000U;
const u32 kTfuCsBusy = 1U << 0;
const u32 kL2TCacheTmuWriteCombinerFlush = 1U << 8;
const u32 kL2TCacheFlushModeClean = 2U << 1;
const u32 kL2TCacheFlush = 1U << 0;
const u32 kCoreIntFrDone = 1U << 0;
const u32 kCoreIntFlDone = 1U << 1;
const u32 kCoreIntRenderError =
    (1U << 2) | (1U << 3) | (1U << 4) | (1U << 5);
const u32 kCoreCt0QtsEnable = 1U << 1;

const u32 kAsbV3dSlaveCtrl = 0x0008U;
const u32 kAsbV3dMasterCtrl = 0x000cU;
const u32 kAsbBridgeId = 0x0020U;
const u32 kAsbReqStop = 1U << 0;
const u32 kAsbAck = 1U << 1;
const u32 kPmV3dResetNot = 1U << 6;
const u32 kPmPassword = 0x5a000000U;
const u32 kProtectedValueMask = 0x00ffffffU;
const u32 kClockV3d = 5U;
const u32 kClockStateOn = 1U << 0;
const u32 kClockStateDoesNotExist = 1U << 1;
const u32 kPropGetClockState = 0x00030001U;
const u32 kPropSetClockState = 0x00038001U;
const unsigned kAsbTimeoutUs = 100U;
const unsigned kClockTimeoutUs = 10000U;
const unsigned kMmuTimeoutUs = 100U;
const unsigned kTfuTimeoutUs = 100000U;
const unsigned kRenderTimeoutUs = 200000U;
const u32 kFirstV3dAddress = kMmuPageSize;
const u32 kMmuScratchBytes = kMmuPageSize;
const u32 kTestWidth = 4U;
const u32 kTestHeight = 4U;
const u32 kTestReadbackBytes = kTestWidth * kTestHeight * sizeof(u32);
const u32 kRenderJobSlotCount = 2U;
const u32 kRenderJobExecutionCount = 3U;
const u32 kEdgeGlowRegionKernelSize = 3U;
const u32 kEdgeGlowEdgeInsetX1000 = 50U;
const u32 kEdgeGlowEdgeNormalOffsetX1000 = 25U;
const float kEdgeGlowEdgeInset =
    static_cast<float>(kEdgeGlowEdgeInsetX1000) / 1000.0f;
const float kEdgeGlowEdgeNormalOffset =
    static_cast<float>(kEdgeGlowEdgeNormalOffsetX1000) / 1000.0f;
const u64 kEdgeGlowTemporalTimeConstantUs = 120000ULL;
const u64 kEdgeGlowTemporalResetGapUs = 500000ULL;

struct TPropertyTagClockState {
  TPropertyTag Tag;
  u32 nClockId;
  u32 nState;
} PACKED;

bool g_requested = false;
bool g_initialized = false;
bool g_hardware_visible = false;
bool g_mmu_ready = false;
bool g_m1_attempted = false;
bool g_m1_passed = false;
bool g_m2_attempted = false;
bool g_m2_passed = false;
bool g_m3_attempted = false;
bool g_m3_passed = false;
bool g_m4_attempted = false;
bool g_m4_passed = false;
bool g_runtime_armed = false;
bool g_runtime_failed = false;
u32 g_m2_readback_hash = 0U;
const char *g_shader_preset = "off";
const char *g_boot_test = "off";

struct Buffer {
  uint8_t *allocation;
  uint8_t *cpu;
  u32 allocation_size;
  u32 size;
  u32 dma_address;
  u32 v3d_address;
  MmuMapping mapping;
};

uint8_t *g_page_table_allocation = nullptr;
uint32_t *g_page_table = nullptr;
u32 g_page_table_allocation_size = 0U;
u32 g_page_table_dma_address = 0U;
uint8_t *g_mmu_scratch_allocation = nullptr;
uint8_t *g_mmu_scratch = nullptr;
u32 g_mmu_scratch_allocation_size = 0U;
u32 g_mmu_scratch_dma_address = 0U;
u32 g_next_v3d_address = kFirstV3dAddress;
Buffer g_test_source = {};
Buffer g_test_target = {};
Buffer g_render_target_alt = {};
Buffer g_render_controls[kRenderJobSlotCount] = {};
Buffer g_render_tile_scratch = {};
RenderJob g_render_jobs[kRenderJobSlotCount] = {};
Buffer g_frame_source = {};
Buffer g_frame_intermediate = {};
Buffer g_frame_output_intermediate = {};
Buffer g_frame_targets[kRenderJobSlotCount] = {};
Buffer g_frame_controls[kRenderJobSlotCount] = {};
Buffer g_frame_source_pass_controls[kRenderJobSlotCount] = {};
Buffer g_frame_output_pass_controls[kRenderJobSlotCount] = {};
Buffer g_frame_self_test_control = {};
Buffer g_frame_tile_scratch = {};
RenderJob g_frame_jobs[kRenderJobSlotCount] = {};
RenderJob g_frame_source_pass_jobs[kRenderJobSlotCount] = {};
RenderJob g_frame_output_pass_jobs[kRenderJobSlotCount] = {};
RenderJob g_frame_self_test_job = {};
u32 g_frame_width = 0U;
u32 g_frame_height = 0U;
u32 g_frame_target_width = 0U;
u32 g_frame_target_height = 0U;
u32 g_frame_source_bytes = 0U;
u32 g_frame_intermediate_bytes = 0U;
u32 g_frame_output_intermediate_bytes = 0U;
u32 g_frame_target_bytes = 0U;
u32 g_frame_tile_state_bytes = 0U;
u32 g_frame_sequence = 0U;
RenderedFrame g_last_rendered_frame = {};
v3d42::Rgba8TextureLayout g_frame_source_layout = {};
v3d42::Rgba8TextureLayout g_frame_intermediate_layout = {};
v3d42::Rgba8TextureLayout g_frame_output_intermediate_layout = {};
bool g_frame_self_test_passed = false;
bool g_frame_effect_valid = false;
bool g_frame_multipass_active = false;
bool g_frame_source_pass_active = false;
bool g_frame_output_pass_active = false;
bool g_frame_output_pass_edge_glow = false;
enum FrameOutputPassMode {
  kFrameOutputPassOff = 0,
  kFrameOutputPassBloom,
  kFrameOutputPassBloomSource
};
FrameOutputPassMode g_frame_output_pass_mode = kFrameOutputPassOff;
bool g_frame_effect_output_log_pending = false;
bool g_frame_last_effect_change = false;
bool g_frame_geometry_enabled = false;
float g_frame_curvature_x = 0.0f;
float g_frame_curvature_y = 0.0f;
float g_frame_skew_x = 0.0f;
float g_frame_skew_y = 0.0f;
float g_frame_trapezoid = 0.0f;
float g_frame_rotation_degrees = 0.0f;
float g_frame_overscan_scale = 1.0f;
float g_frame_scanline_weight = 0.0f;
float g_frame_scanline_gap_brightness = 1.0f;
bool g_frame_scanline_multisample = false;
bool g_frame_edge_blur_enabled = false;
float g_frame_edge_blur_strength = 0.0f;
float g_frame_edge_blur_radius = 0.2f;
bool g_frame_phosphor_mask_enabled = false;
u32 g_frame_phosphor_mask_pattern = 1U;
float g_frame_phosphor_mask_brightness = 1.0f;
bool g_frame_vignette_enabled = false;
float g_frame_vignette_strength = 0.0f;
float g_frame_vignette_scale = 1.0f;
float g_frame_vignette_softness = 0.02f;
bool g_frame_uneven_illumination_enabled = false;
float g_frame_uneven_illumination_strength = 0.0f;
float g_frame_uneven_illumination_scale = 0.02f;
bool g_frame_glass_reflection_enabled = false;
float g_frame_glass_reflection_angle = 0.0f;
float g_frame_glass_reflection_width = 0.02f;
float g_frame_glass_reflection_position = 0.0f;
bool g_frame_rounded_screen_mask_enabled = false;
float g_frame_rounded_corner_radius = 0.0f;
float g_frame_rounded_border_softness = 0.0f;
bool g_frame_edge_glow_enabled = false;
float g_frame_edge_glow_strength = 0.0f;
float g_frame_edge_glow_width = 0.01f;
bool g_frame_output_response_enabled = false;
bool g_frame_output_response_fast = false;
u32 g_frame_output_level_mapping = 1U;
float g_frame_input_gamma = 1.0f;
float g_frame_output_gamma = 1.0f;
float g_frame_output_saturation = 1.0f;
float g_frame_black_level = 0.0f;
float g_frame_white_clip = 1.0f;
bool g_frame_convergence_enabled = false;
float g_frame_red_offset_x = 0.0f;
float g_frame_red_offset_y = 0.0f;
float g_frame_blue_offset_x = 0.0f;
float g_frame_blue_offset_y = 0.0f;
float g_frame_convergence_radial_strength = 0.0f;
bool g_frame_horizontal_filtering_enabled = false;
float g_frame_horizontal_sigma_x = 0.0f;
bool g_frame_bloom_enabled = false;
float g_frame_bloom_factor = 0.0f;
bool g_frame_horizontal_jitter_enabled = false;
float g_frame_horizontal_jitter_strength = 0.0f;
float g_frame_horizontal_jitter_frequency = 0.01f;
float g_frame_horizontal_jitter_speed = 0.0f;
bool g_frame_composite_artifacts_enabled = false;
float g_frame_composite_chroma_blur = 0.0f;
float g_frame_composite_luma_sharpen = 0.0f;
float g_frame_composite_color_bleed = 0.0f;
bool g_frame_noise_enabled = false;
float g_frame_luminance_noise = 0.0f;
float g_frame_chroma_noise = 0.0f;
float g_frame_noise_speed = 0.0f;
bool g_frame_temporal_package_active = false;
u16 g_frame_output_response_lut[65536] = {};
u16 g_frame_output_response_palette[256] = {};
u16 g_frame_output_response_palette_source[256] = {};
bool g_frame_output_response_lut_valid = false;
bool g_frame_output_response_palette_valid = false;
u32 g_frame_output_response_lut_build_us = 0U;
u32 g_frame_source_upload_us = 0U;
bool g_frame_source_linear_filter = false;
v3dcrt::EdgeGlowTemporalFilter g_edge_glow_temporal_filter = {};

bool IsProductionFramePreset() {
  return ResolveFramePresetPolicy(g_shader_preset).production;
}

bool IsRuntimeFramePreset() {
  return ResolveFramePresetPolicy(g_shader_preset).runtime;
}

u32 ReadRegister(uintptr base, u32 offset) {
  return read32(base + offset);
}

void WriteRegister(uintptr base, u32 offset, u32 value) {
  DataSyncBarrier();
  write32(base + offset, value);
  DataSyncBarrier();
}

bool ReadClockRate(u32 tag_id, u32 *rate) {
  if (rate == nullptr) {
    return false;
  }

  CBcmPropertyTags tags;
  TPropertyTagClockRate tag;
  memset(&tag, 0, sizeof tag);
  tag.nClockId = kClockV3d;
  if (!tags.GetTag(tag_id, &tag, sizeof tag, 4)) {
    return false;
  }
  *rate = tag.nRate;
  return true;
}

bool ReadClockState(u32 *state) {
  if (state == nullptr) {
    return false;
  }

  CBcmPropertyTags tags;
  TPropertyTagClockState tag;
  memset(&tag, 0, sizeof tag);
  tag.nClockId = kClockV3d;
  if (!tags.GetTag(kPropGetClockState, &tag, sizeof tag, 4) ||
      tag.nClockId != kClockV3d) {
    return false;
  }
  *state = tag.nState;
  return true;
}

bool SetClockState(bool enable, u32 *response_state) {
  CBcmPropertyTags tags;
  TPropertyTagClockState tag;
  memset(&tag, 0, sizeof tag);
  tag.nClockId = kClockV3d;
  tag.nState = enable ? kClockStateOn : 0U;
  if (!tags.GetTag(kPropSetClockState, &tag, sizeof tag, 8) ||
      tag.nClockId != kClockV3d) {
    return false;
  }
  if (response_state != nullptr) {
    *response_state = tag.nState;
  }
  return (tag.nState & kClockStateDoesNotExist) == 0U &&
         ((tag.nState & kClockStateOn) != 0U) == enable;
}

PowerSnapshot CapturePowerSnapshot(bool measured_clock_valid,
                                   u32 measured_clock) {
  PowerSnapshot snapshot = {};
  snapshot.pm_grafx = read32(kPmGrafx);
  snapshot.asb_bridge_id = ReadRegister(kRpividAsbBase, kAsbBridgeId);
  snapshot.asb_slave = ReadRegister(kRpividAsbBase, kAsbV3dSlaveCtrl);
  snapshot.asb_master = ReadRegister(kRpividAsbBase, kAsbV3dMasterCtrl);
  u32 clock_state = 0U;
  snapshot.clock_state_valid = ReadClockState(&clock_state);
  snapshot.clock_state = clock_state;
  snapshot.measured_clock_valid = measured_clock_valid;
  snapshot.measured_clock_hz = measured_clock;
  return snapshot;
}

void WriteProtectedRegister(uintptr address, u32 value) {
  DataSyncBarrier();
  write32(address, kPmPassword | (value & kProtectedValueMask));
  DataSyncBarrier();
}

bool WaitForRegisterBits(uintptr address, u32 mask, u32 expected,
                         unsigned timeout_us, u32 *last_value) {
  const unsigned start = CTimer::GetClockTicks();
  for (;;) {
    const u32 value = read32(address);
    if (last_value != nullptr) {
      *last_value = value;
    }
    if ((value & mask) == expected) {
      return true;
    }
    if ((unsigned)(CTimer::GetClockTicks() - start) >= timeout_us) {
      return false;
    }
    CTimer::SimpleusDelay(1);
  }
}

bool SetBridgeActive(u32 offset, bool enable, u32 *last_value) {
  const uintptr address = kRpividAsbBase + offset;
  const u32 before = read32(address);
  const u32 requested = enable ? before & ~kAsbReqStop
                               : before | kAsbReqStop;
  WriteProtectedRegister(address, requested);
  const u32 mask = kAsbReqStop | kAsbAck;
  const u32 expected = enable ? 0U : mask;
  return WaitForRegisterBits(address, mask, expected, kAsbTimeoutUs,
                             last_value);
}

bool WaitForClockRunning(u32 *measured_clock) {
  const unsigned start = CTimer::GetClockTicks();
  for (;;) {
    u32 rate = 0U;
    if (ReadClockRate(PROPTAG_GET_CLOCK_RATE_MEASURED, &rate) && rate != 0U) {
      if (measured_clock != nullptr) {
        *measured_clock = rate;
      }
      return true;
    }
    if ((unsigned)(CTimer::GetClockTicks() - start) >= kClockTimeoutUs) {
      return false;
    }
    CTimer::SimpleusDelay(100);
  }
}

void RestorePowerSnapshot(const PowerSnapshot &initial) {
  u32 slave = 0U;
  u32 master = 0U;
  const bool slave_stopped =
      SetBridgeActive(kAsbV3dSlaveCtrl, false, &slave);
  const bool master_stopped =
      SetBridgeActive(kAsbV3dMasterCtrl, false, &master);

  u32 clock_off_state = 0U;
  const bool clock_stopped = SetClockState(false, &clock_off_state);

  const u32 pm_before = read32(kPmGrafx);
  WriteProtectedRegister(kPmGrafx, pm_before & ~kPmV3dResetNot);
  const u32 pm_after = read32(kPmGrafx);
  const bool reset_asserted = (pm_after & kPmV3dResetNot) == 0U;

  const bool initial_clock_on =
      initial.clock_state_valid &&
      (initial.clock_state & kClockStateDoesNotExist) == 0U &&
      (initial.clock_state & kClockStateOn) != 0U;
  u32 restored_clock_state = 0U;
  const bool clock_restored =
      SetClockState(initial_clock_on, &restored_clock_state);

  printf("boot: pi4v3d power rollback slave_ok=%u slave=0x%08x "
         "master_ok=%u master=0x%08x clock_off_ok=%u "
         "reset_ok=%u pm_grafx=0x%08x clock_restore_ok=%u "
         "clock_state=0x%08x\r\n",
         slave_stopped ? 1U : 0U, slave, master_stopped ? 1U : 0U,
         master, clock_stopped ? 1U : 0U, reset_asserted ? 1U : 0U,
         pm_after, clock_restored ? 1U : 0U, restored_clock_state);
}

bool RunPowerOnSequence(const PowerSnapshot &initial) {
  u32 state = 0U;
  if (!SetClockState(true, &state)) {
    printf("boot: pi4v3d power failed step=clock-prime "
           "state=0x%08x\r\n", state);
    RestorePowerSnapshot(initial);
    return false;
  }
  CTimer::SimpleusDelay(1);

  if (!SetClockState(false, &state)) {
    printf("boot: pi4v3d power failed step=clock-stop "
           "state=0x%08x\r\n", state);
    RestorePowerSnapshot(initial);
    return false;
  }

  const u32 pm_before = read32(kPmGrafx);
  WriteProtectedRegister(kPmGrafx, pm_before | kPmV3dResetNot);
  const u32 pm_after = read32(kPmGrafx);
  if ((pm_after & kPmV3dResetNot) == 0U) {
    printf("boot: pi4v3d power failed step=reset-deassert "
           "pm_grafx=0x%08x\r\n", pm_after);
    RestorePowerSnapshot(initial);
    return false;
  }
  printf("boot: pi4v3d power step=reset-deassert "
         "pm_before=0x%08x pm_after=0x%08x\r\n", pm_before, pm_after);

  if (!SetClockState(true, &state)) {
    printf("boot: pi4v3d power failed step=clock-start "
           "state=0x%08x\r\n", state);
    RestorePowerSnapshot(initial);
    return false;
  }
  u32 measured_clock = 0U;
  if (!WaitForClockRunning(&measured_clock)) {
    printf("boot: pi4v3d power failed step=clock-running-timeout\r\n");
    RestorePowerSnapshot(initial);
    return false;
  }
  printf("boot: pi4v3d power step=clock-start state=0x%08x "
         "measured_hz=%u\r\n", state, measured_clock);

  u32 master = 0U;
  if (!SetBridgeActive(kAsbV3dMasterCtrl, true, &master)) {
    printf("boot: pi4v3d power failed step=asb-master "
           "control=0x%08x\r\n", master);
    RestorePowerSnapshot(initial);
    return false;
  }
  printf("boot: pi4v3d power step=asb-master control=0x%08x\r\n",
         master);

  u32 slave = 0U;
  if (!SetBridgeActive(kAsbV3dSlaveCtrl, true, &slave)) {
    printf("boot: pi4v3d power failed step=asb-slave "
           "control=0x%08x\r\n", slave);
    RestorePowerSnapshot(initial);
    return false;
  }
  printf("boot: pi4v3d power step=asb-slave control=0x%08x\r\n",
         slave);
  return true;
}

uintptr AlignUp(uintptr value, u32 alignment) {
  return (value + alignment - 1U) & ~(static_cast<uintptr>(alignment) - 1U);
}

bool AllocateAlignedLow(u32 size, uint8_t **allocation, uint8_t **cpu,
                        u32 *allocation_size) {
  if (size == 0U || allocation == nullptr || cpu == nullptr ||
      allocation_size == nullptr || size > UINT32_MAX - kMmuPageSize + 1U) {
    return false;
  }

  const u32 bytes = size + kMmuPageSize - 1U;
  uint8_t *raw = new (HEAP_DMA30) uint8_t[bytes];
  if (raw == nullptr) {
    return false;
  }

  uint8_t *aligned = reinterpret_cast<uint8_t *>(
      AlignUp(reinterpret_cast<uintptr>(raw), kMmuPageSize));
  memset(aligned, 0, size);
  *allocation = raw;
  *cpu = aligned;
  *allocation_size = bytes;
  return true;
}

bool LowDmaAddress(const void *pointer, u32 *address) {
  if (pointer == nullptr || address == nullptr) {
    return false;
  }
  const uintptr value = reinterpret_cast<uintptr>(pointer);
  if (value > UINT32_MAX) {
    return false;
  }
  *address = static_cast<u32>(value);
  return true;
}

bool WaitForMmuFlush() {
  WriteRegister(kV3dHubBase, kHubMmuControl,
                kMmuControlFlush | kMmuControlEnable);
  if (!WaitForRegisterBits(kV3dHubBase + kHubMmuControl,
                           kMmuControlFlushing, 0U, kMmuTimeoutUs,
                           nullptr)) {
    printf("boot: pi4v3d mmu failed step=mmuc-flush-timeout "
           "mmuc=0x%08x\r\n",
           ReadRegister(kV3dHubBase, kHubMmuControl));
    return false;
  }

  WriteRegister(kV3dHubBase, kHubMmuCtl,
                ReadRegister(kV3dHubBase, kHubMmuCtl) |
                    kMmuCtlTlbClear);
  if (!WaitForRegisterBits(kV3dHubBase + kHubMmuCtl,
                           kMmuCtlTlbClearing, 0U, kMmuTimeoutUs,
                           nullptr)) {
    printf("boot: pi4v3d mmu failed step=tlb-clear-timeout "
           "ctl=0x%08x\r\n",
           ReadRegister(kV3dHubBase, kHubMmuCtl));
    return false;
  }
  return true;
}

void FreeBuffer(Buffer *buffer) {
  if (buffer == nullptr) {
    return;
  }
  if (buffer->mapping.page_count != 0U && g_page_table != nullptr) {
    const u32 first_page = buffer->mapping.first_page;
    const u32 page_count = buffer->mapping.page_count;
    if (ClearMmuMapping(g_page_table, kMmuPageTableEntries,
                        &buffer->mapping)) {
      CleanAndInvalidateDataCacheRange(
          reinterpret_cast<uintptr>(&g_page_table[first_page]),
          page_count * sizeof(u32));
      if (g_mmu_ready) {
        (void)WaitForMmuFlush();
      }
    }
  }
  delete[] buffer->allocation;
  memset(buffer, 0, sizeof *buffer);
}

void CleanBufferForV3d(const Buffer &buffer) {
  if (buffer.cpu != nullptr && buffer.size != 0U) {
    CleanAndInvalidateDataCacheRange(
        reinterpret_cast<uintptr>(buffer.cpu), buffer.size);
  }
}

void CleanBufferRangeForV3d(const Buffer &buffer, u32 offset, u32 size) {
  if (buffer.cpu != nullptr && size != 0U && offset <= buffer.size &&
      size <= buffer.size - offset) {
    CleanAndInvalidateDataCacheRange(
        reinterpret_cast<uintptr>(buffer.cpu + offset), size);
  }
}

bool CleanRenderJobDynamicUniforms(const Buffer &control,
                                   const RenderJob &job) {
  u32 offset = 0U;
  u32 size = 0U;
  if (!RenderJobDynamicUniformRange(job, &offset, &size)) {
    return false;
  }
  CleanBufferRangeForV3d(control, offset, size);
  return true;
}

void InvalidateBufferFromV3d(const Buffer &buffer) {
  if (buffer.cpu != nullptr && buffer.size != 0U) {
    CleanAndInvalidateDataCacheRange(
        reinterpret_cast<uintptr>(buffer.cpu), buffer.size);
  }
}

void FreeMmu() {
  g_mmu_ready = false;
  g_next_v3d_address = kFirstV3dAddress;
  WriteRegister(kV3dHubBase, kHubMmuControl, 0U);
  WriteRegister(kV3dHubBase, kHubMmuCtl, 0U);
  WriteRegister(kV3dHubBase, kHubMmuIllegalAddr, 0U);
  WriteRegister(kV3dHubBase, kHubMmuPtPaBase, 0U);
  delete[] g_page_table_allocation;
  delete[] g_mmu_scratch_allocation;
  g_page_table_allocation = nullptr;
  g_page_table = nullptr;
  g_page_table_allocation_size = 0U;
  g_page_table_dma_address = 0U;
  g_mmu_scratch_allocation = nullptr;
  g_mmu_scratch = nullptr;
  g_mmu_scratch_allocation_size = 0U;
  g_mmu_scratch_dma_address = 0U;
}

bool InitializeMmu() {
  if (g_mmu_ready) {
    return true;
  }

  uint8_t *page_table_cpu = nullptr;
  if (!AllocateAlignedLow(kMmuPageTableBytes, &g_page_table_allocation,
                          &page_table_cpu,
                          &g_page_table_allocation_size) ||
      !AllocateAlignedLow(kMmuScratchBytes, &g_mmu_scratch_allocation,
                          &g_mmu_scratch,
                          &g_mmu_scratch_allocation_size)) {
    printf("boot: pi4v3d mmu failed step=allocation\r\n");
    delete[] g_page_table_allocation;
    delete[] g_mmu_scratch_allocation;
    g_page_table_allocation = nullptr;
    g_mmu_scratch_allocation = nullptr;
    return false;
  }

  g_page_table = reinterpret_cast<uint32_t *>(page_table_cpu);
  // Unlike the legacy VideoCore blocks addressed through BUS_ADDRESS(), the
  // Pi 4 V3D MMU consumes ARM physical addresses for its page table and PTEs.
  // All allocations come from HEAP_DMA30, so the truncated addresses remain
  // within the V3D 4.2 DMA aperture.
  if (!LowDmaAddress(g_page_table, &g_page_table_dma_address) ||
      !LowDmaAddress(g_mmu_scratch, &g_mmu_scratch_dma_address)) {
    printf("boot: pi4v3d mmu failed step=dma30-address\r\n");
    FreeMmu();
    return false;
  }
  if ((g_page_table_dma_address & (kMmuPageSize - 1U)) != 0U ||
      (g_mmu_scratch_dma_address & (kMmuPageSize - 1U)) != 0U) {
    printf("boot: pi4v3d mmu failed step=dma-alignment "
           "pt_dma=0x%08x scratch_dma=0x%08x\r\n",
           g_page_table_dma_address, g_mmu_scratch_dma_address);
    FreeMmu();
    return false;
  }

  CleanAndInvalidateDataCacheRange(
      reinterpret_cast<uintptr>(g_page_table), kMmuPageTableBytes);
  CleanAndInvalidateDataCacheRange(
      reinterpret_cast<uintptr>(g_mmu_scratch), kMmuScratchBytes);

  WriteRegister(kV3dCoreBase, kCoreL2TFlushStart, 0U);
  WriteRegister(kV3dCoreBase, kCoreL2TFlushEnd, ~0U);
  WriteRegister(kV3dCoreBase, kCoreIntClear, ~0U);
  WriteRegister(kV3dHubBase, kHubIntClear, ~0U);

  const u32 requested_ctl =
      kMmuCtlEnable | kMmuCtlPtInvalidEnable | kMmuCtlPtInvalidAbort |
      kMmuCtlPtInvalidInt | kMmuCtlWriteViolationAbort |
      kMmuCtlWriteViolationInt | kMmuCtlCapExceededAbort |
      kMmuCtlCapExceededInt;
  WriteRegister(kV3dHubBase, kHubMmuPtPaBase,
                g_page_table_dma_address >> kMmuPageShift);
  WriteRegister(kV3dHubBase, kHubMmuIllegalAddr,
                (g_mmu_scratch_dma_address >> kMmuPageShift) |
                    kMmuIllegalAddrEnable);
  WriteRegister(kV3dHubBase, kHubMmuCtl, requested_ctl);
  WriteRegister(kV3dHubBase, kHubMmuControl, kMmuControlEnable);

  const u32 pt_reg = ReadRegister(kV3dHubBase, kHubMmuPtPaBase);
  const u32 ctl = ReadRegister(kV3dHubBase, kHubMmuCtl);
  const u32 mmuc = ReadRegister(kV3dHubBase, kHubMmuControl);
  const bool programmed =
      pt_reg == (g_page_table_dma_address >> kMmuPageShift) &&
      (ctl & requested_ctl) == requested_ctl &&
      (mmuc & kMmuControlEnable) != 0U;
  g_mmu_ready = programmed && WaitForMmuFlush();
  printf("boot: pi4v3d mmu pt_cpu=%p pt_dma=0x%08x "
         "scratch_cpu=%p scratch_dma=0x%08x pt_bytes=%u "
         "pt_reg=0x%08x ctl=0x%08x mmuc=0x%08x ready=%u\r\n",
         g_page_table, g_page_table_dma_address,
         g_mmu_scratch, g_mmu_scratch_dma_address,
         static_cast<unsigned>(kMmuPageTableBytes), pt_reg, ctl,
         ReadRegister(kV3dHubBase, kHubMmuControl),
         g_mmu_ready ? 1U : 0U);
  if (!g_mmu_ready) {
    FreeMmu();
  }
  return g_mmu_ready;
}

bool AllocateAndMapBuffer(const char *name, u32 requested_size,
                          Buffer *buffer) {
  if (name == nullptr || buffer == nullptr || !g_mmu_ready ||
      g_page_table == nullptr || requested_size == 0U ||
      requested_size > UINT32_MAX - (kMmuPageSize - 1U)) {
    return false;
  }

  const u32 mapped_size =
      (requested_size + kMmuPageSize - 1U) & ~(kMmuPageSize - 1U);
  if (g_next_v3d_address > UINT32_MAX - mapped_size ||
      !AllocateAlignedLow(mapped_size, &buffer->allocation,
                          &buffer->cpu, &buffer->allocation_size)) {
    return false;
  }
  buffer->size = mapped_size;
  if (!LowDmaAddress(buffer->cpu, &buffer->dma_address)) {
    FreeBuffer(buffer);
    return false;
  }
  buffer->v3d_address = g_next_v3d_address;
  if (!InsertContiguousPtes(g_page_table, kMmuPageTableEntries,
                            buffer->v3d_address, buffer->dma_address,
                            buffer->size, &buffer->mapping)) {
    FreeBuffer(buffer);
    return false;
  }

  g_next_v3d_address += mapped_size;
  CleanAndInvalidateDataCacheRange(
      reinterpret_cast<uintptr>(&g_page_table[buffer->mapping.first_page]),
      buffer->mapping.page_count * sizeof(u32));
  if (!WaitForMmuFlush()) {
    FreeBuffer(buffer);
    return false;
  }

  printf("boot: pi4v3d buffer %s cpu=%p dma=0x%08x "
         "v3d_va=0x%08x size=%u pages=%u pte=0x%08x\r\n",
         name, buffer->cpu, buffer->dma_address,
         buffer->v3d_address, buffer->size,
         static_cast<unsigned>(buffer->mapping.page_count),
         static_cast<unsigned>(buffer->mapping.first_pte));
  return true;
}

bool InitializeTestBuffers() {
  if (!AllocateAndMapBuffer("source", kMmuPageSize, &g_test_source) ||
      !AllocateAndMapBuffer("target", kMmuPageSize, &g_test_target)) {
    printf("boot: pi4v3d buffers failed step=allocate-or-map\r\n");
    FreeBuffer(&g_test_source);
    FreeBuffer(&g_test_target);
    return false;
  }
  return true;
}

bool InitializeRenderBuffers() {
  if (g_render_target_alt.cpu != nullptr &&
      g_render_controls[0].cpu != nullptr &&
      g_render_controls[1].cpu != nullptr &&
      g_render_tile_scratch.cpu != nullptr) {
    return true;
  }
  if (!AllocateAndMapBuffer("render-target-1", kMmuPageSize,
                            &g_render_target_alt) ||
      !AllocateAndMapBuffer("render-control-0", kRenderControlBytes,
                            &g_render_controls[0]) ||
      !AllocateAndMapBuffer("render-control-1", kRenderControlBytes,
                            &g_render_controls[1]) ||
      !AllocateAndMapBuffer("render-tile", kRenderTileScratchBytes,
                            &g_render_tile_scratch)) {
    printf("boot: pi4v3d render buffers failed step=allocate-or-map\r\n");
    FreeBuffer(&g_render_target_alt);
    for (u32 slot = 0U; slot < kRenderJobSlotCount; ++slot) {
      FreeBuffer(&g_render_controls[slot]);
    }
    FreeBuffer(&g_render_tile_scratch);
    return false;
  }
  return true;
}

bool CleanV3dCaches() {
  WriteRegister(kV3dCoreBase, kCoreL2TCacheControl,
                kL2TCacheTmuWriteCombinerFlush);
  if (!WaitForRegisterBits(kV3dCoreBase + kCoreL2TCacheControl,
                           kL2TCacheTmuWriteCombinerFlush, 0U,
                           kMmuTimeoutUs, nullptr)) {
    printf("boot: pi4v3d cache failed step=tmuwcf "
           "l2t=0x%08x\r\n",
           ReadRegister(kV3dCoreBase, kCoreL2TCacheControl));
    return false;
  }

  WriteRegister(kV3dCoreBase, kCoreL2TFlushStart, 0U);
  WriteRegister(kV3dCoreBase, kCoreL2TFlushEnd, ~0U);
  WriteRegister(kV3dCoreBase, kCoreL2TCacheControl,
                kL2TCacheFlushModeClean | kL2TCacheFlush);
  if (!WaitForRegisterBits(kV3dCoreBase + kCoreL2TCacheControl,
                           kL2TCacheFlush, 0U, kMmuTimeoutUs, nullptr)) {
    printf("boot: pi4v3d cache failed step=l2t-clean "
           "l2t=0x%08x\r\n",
           ReadRegister(kV3dCoreBase, kCoreL2TCacheControl));
    return false;
  }
  return true;
}

bool InvalidateV3dCaches() {
  WriteRegister(kV3dCoreBase, kCoreL2TFlushStart, 0U);
  WriteRegister(kV3dCoreBase, kCoreL2TFlushEnd, ~0U);
  WriteRegister(kV3dCoreBase, kCoreL2TCacheControl, kL2TCacheFlush);
  if (!WaitForRegisterBits(kV3dCoreBase + kCoreL2TCacheControl,
                           kL2TCacheFlush, 0U, kMmuTimeoutUs, nullptr)) {
    printf("boot: pi4v3d cache failed step=l2t-invalidate "
           "l2t=0x%08x\r\n",
           ReadRegister(kV3dCoreBase, kCoreL2TCacheControl));
    return false;
  }
  // V3D 4.x keeps shader instructions, uniforms and texture data in
  // per-slice read-only caches.  Control buffers are rebuilt in place when
  // an effect changes, so invalidating L2T alone can leave the slices using
  // a stale shader against a new uniform stream.  Writing all ones clears
  // every cache on every slice, matching Mesa's V3D submit sequence.
  WriteRegister(kV3dCoreBase, kCoreSliceCacheControl, ~0U);
  return true;
}

u32 CurrentMmuFaults() {
  return ReadRegister(kV3dHubBase, kHubIntStatus) & kHubIntMmuFaultMask;
}

void LogControlListStatus(const char *phase, bool binner,
                          u32 start, u32 end) {
  const u32 hub_int = ReadRegister(kV3dHubBase, kHubIntStatus);
  const u32 core_int = ReadRegister(kV3dCoreBase, kCoreIntStatus);
  const u32 cs = ReadRegister(kV3dCoreBase,
                              binner ? kCoreCt0Cs : kCoreCt1Cs);
  const u32 ca = ReadRegister(kV3dCoreBase,
                              binner ? kCoreCt0Ca : kCoreCt1Ca);
  const u32 ea = ReadRegister(kV3dCoreBase,
                              binner ? kCoreCt0Ea : kCoreCt1Ea);
  const u32 pc = ReadRegister(kV3dCoreBase,
                              binner ? kCoreCt0Pc : kCoreCt1Pc);
  printf("boot: pi4v3d render %s phase=%s cl=0x%08x..0x%08x "
         "hub_int=0x%08x core_int=0x%08x mmu_faults=0x%08x "
         "mmu_ctl=0x%08x vio_id=0x%08x vio_addr=0x%08x "
         "cs=0x%08x ca=0x%08x ea=0x%08x pc=0x%08x\r\n",
         binner ? "binner" : "renderer", phase, start, end, hub_int,
         core_int, hub_int & kHubIntMmuFaultMask,
         ReadRegister(kV3dHubBase, kHubMmuCtl),
         ReadRegister(kV3dHubBase, kHubMmuVioId),
         ReadRegister(kV3dHubBase, kHubMmuVioAddr), cs, ca, ea, pc);
}

bool WaitForControlList(bool binner, u32 start, u32 end,
                        u32 *elapsed_us, bool verbose) {
  const u32 done = binner ? kCoreIntFlDone : kCoreIntFrDone;
  const unsigned start_ticks = CTimer::GetClockTicks();
  for (;;) {
    const u32 core_int = ReadRegister(kV3dCoreBase, kCoreIntStatus);
    const u32 mmu_faults = CurrentMmuFaults();
    const unsigned elapsed = CTimer::GetClockTicks() - start_ticks;
    if (mmu_faults != 0U || (core_int & kCoreIntRenderError) != 0U) {
      if (elapsed_us != nullptr) {
        *elapsed_us = elapsed;
      }
      LogControlListStatus("fault", binner, start, end);
      return false;
    }
    if ((core_int & done) != 0U) {
      if (elapsed_us != nullptr) {
        *elapsed_us = elapsed;
      }
      if (verbose) {
        LogControlListStatus("done", binner, start, end);
      }
      return true;
    }
    if (elapsed >= kRenderTimeoutUs) {
      if (elapsed_us != nullptr) {
        *elapsed_us = elapsed;
      }
      LogControlListStatus("timeout", binner, start, end);
      return false;
    }
    CTimer::SimpleusDelay(10U);
  }
}

bool SubmitBcl(const RenderCommandLists &lists, const Buffer &tile_scratch,
               u32 tile_state_offset, u32 *elapsed_us, bool verbose) {
  WriteRegister(kV3dCoreBase, kCoreIntClear, ~0U);
  WriteRegister(kV3dHubBase, kHubIntClear, ~0U);
  if (!InvalidateV3dCaches()) {
    return false;
  }
  WriteRegister(kV3dCoreBase, kCoreCt0Qma,
                tile_scratch.v3d_address);
  WriteRegister(kV3dCoreBase, kCoreCt0Qms,
                kRenderTileAllocationBytes);
  WriteRegister(kV3dCoreBase, kCoreCt0Qts,
                kCoreCt0QtsEnable |
                    (tile_scratch.v3d_address + tile_state_offset));
  WriteRegister(kV3dCoreBase, kCoreCt0Qba, lists.bcl_start);
  if (verbose) {
    LogControlListStatus("submit", true, lists.bcl_start, lists.bcl_end);
  }
  WriteRegister(kV3dCoreBase, kCoreCt0Qea, lists.bcl_end);
  const bool ok = WaitForControlList(true, lists.bcl_start, lists.bcl_end,
                                     elapsed_us, verbose);
  WriteRegister(kV3dCoreBase, kCoreIntClear, ~0U);
  WriteRegister(kV3dHubBase, kHubIntClear, ~0U);
  return ok;
}

bool SubmitRcl(const RenderCommandLists &lists, u32 *elapsed_us,
               bool verbose) {
  WriteRegister(kV3dCoreBase, kCoreIntClear, ~0U);
  WriteRegister(kV3dHubBase, kHubIntClear, ~0U);
  if (!InvalidateV3dCaches()) {
    return false;
  }
  WriteRegister(kV3dCoreBase, kCoreCt1Qba, lists.rcl_start);
  if (verbose) {
    LogControlListStatus("submit", false, lists.rcl_start, lists.rcl_end);
  }
  WriteRegister(kV3dCoreBase, kCoreCt1Qea, lists.rcl_end);
  const bool ok = WaitForControlList(false, lists.rcl_start, lists.rcl_end,
                                     elapsed_us, verbose);
  WriteRegister(kV3dCoreBase, kCoreIntClear, ~0U);
  WriteRegister(kV3dHubBase, kHubIntClear, ~0U);
  return ok;
}

bool SubmitTfuCopy(const TfuCopyRegisters &tfu, u32 *elapsed_us,
                   u32 *final_hub_int, u32 *final_tfu_cs) {
  WriteRegister(kV3dHubBase, kHubIntClear, ~0U);
  const u32 initial_tfu_cs = ReadRegister(kV3dHubBase, kHubTfuCs);
  const u32 initial_count = initial_tfu_cs & kTfuCsConversionCountMask;

  WriteRegister(kV3dHubBase, kHubTfuIia, tfu.iia);
  WriteRegister(kV3dHubBase, kHubTfuIis, tfu.iis);
  WriteRegister(kV3dHubBase, kHubTfuIca, tfu.ica);
  WriteRegister(kV3dHubBase, kHubTfuIua, tfu.iua);
  WriteRegister(kV3dHubBase, kHubTfuIoa, tfu.ioa);
  WriteRegister(kV3dHubBase, kHubTfuIos, tfu.ios);
  WriteRegister(kV3dHubBase, kHubTfuCoef0, 0U);
  WriteRegister(kV3dHubBase, kHubTfuCoef1, 0U);
  WriteRegister(kV3dHubBase, kHubTfuCoef2, 0U);
  WriteRegister(kV3dHubBase, kHubTfuCoef3, 0U);

  const unsigned start = CTimer::GetClockTicks();
  WriteRegister(kV3dHubBase, kHubTfuIcfg, tfu.icfg);
  bool complete = false;
  u32 hub_int = 0U;
  u32 tfu_cs = 0U;
  for (;;) {
    hub_int = ReadRegister(kV3dHubBase, kHubIntStatus);
    tfu_cs = ReadRegister(kV3dHubBase, kHubTfuCs);
    const unsigned elapsed = CTimer::GetClockTicks() - start;
    if ((hub_int & (kHubIntTfuFault | kHubIntMmuFaultMask)) != 0U) {
      break;
    }
    if ((hub_int & kHubIntTfuComplete) != 0U ||
        (tfu_cs & kTfuCsConversionCountMask) != initial_count) {
      complete = (tfu_cs & kTfuCsBusy) == 0U;
      break;
    }
    if (elapsed >= kTfuTimeoutUs) {
      break;
    }
    CTimer::SimpleusDelay(1U);
  }

  if (elapsed_us != nullptr) {
    *elapsed_us = CTimer::GetClockTicks() - start;
  }
  if (final_hub_int != nullptr) {
    *final_hub_int = hub_int;
  }
  if (final_tfu_cs != nullptr) {
    *final_tfu_cs = tfu_cs;
  }
  DataSyncBarrier();
  return complete && (hub_int & (kHubIntTfuFault | kHubIntMmuFaultMask)) == 0U;
}

bool RunM1OffscreenTest() {
  if (g_m1_attempted) {
    return g_m1_passed;
  }
  g_m1_attempted = true;

  if (!InitializeMmu() || !InitializeTestBuffers()) {
    printf("boot: pi4v3d test=mmu status=fail phase=setup\r\n");
    FreeBuffer(&g_test_source);
    FreeBuffer(&g_test_target);
    FreeMmu();
    return false;
  }

  u32 *source = reinterpret_cast<u32 *>(g_test_source.cpu);
  u32 *target = reinterpret_cast<u32 *>(g_test_target.cpu);
  for (u32 i = 0; i < g_test_target.size / sizeof(u32); ++i) {
    target[i] = 0xdeadc0deU;
  }
  for (u32 i = 0; i < kTestWidth * kTestHeight; ++i) {
    source[i] = 0x3f000000U + i * 0x00080000U;
  }
  CleanAndInvalidateDataCacheRange(reinterpret_cast<uintptr>(g_test_source.cpu),
                                   g_test_source.size);
  CleanAndInvalidateDataCacheRange(reinterpret_cast<uintptr>(g_test_target.cpu),
                                   g_test_target.size);

  TfuCopyRegisters tfu = {};
  if (!BuildTfuR32Copy(g_test_source.v3d_address,
                       g_test_target.v3d_address,
                       kTestWidth, kTestHeight, &tfu)) {
    printf("boot: pi4v3d test=mmu status=fail phase=tfu-build\r\n");
    return false;
  }

  printf("boot: pi4v3d tfu submit icfg=0x%08x iia=0x%08x "
         "iis=0x%08x ioa=0x%08x ios=0x%08x\r\n",
         static_cast<unsigned>(tfu.icfg),
         static_cast<unsigned>(tfu.iia),
         static_cast<unsigned>(tfu.iis),
         static_cast<unsigned>(tfu.ioa),
         static_cast<unsigned>(tfu.ios));
  u32 elapsed_us = 0U;
  u32 hub_int = 0U;
  u32 tfu_cs = 0U;
  const bool submitted = SubmitTfuCopy(tfu, &elapsed_us, &hub_int, &tfu_cs);
  const u32 mmu_ctl = ReadRegister(kV3dHubBase, kHubMmuCtl);
  const u32 vio_id = ReadRegister(kV3dHubBase, kHubMmuVioId);
  const u32 vio_addr = ReadRegister(kV3dHubBase, kHubMmuVioAddr);
  printf("boot: pi4v3d tfu complete=%u elapsed_us=%u hub_int=0x%08x "
         "tfu_cs=0x%08x mmu_ctl=0x%08x vio_id=0x%08x "
         "vio_addr=0x%08x\r\n",
         submitted ? 1U : 0U, elapsed_us, hub_int, tfu_cs, mmu_ctl,
         vio_id, vio_addr);
  if (!submitted) {
    printf("boot: pi4v3d test=mmu status=fail phase=tfu-submit\r\n");
    return false;
  }

  const bool cache_clean = CleanV3dCaches();
  printf("boot: pi4v3d cache post-tfu clean=%u l2t=0x%08x\r\n",
         cache_clean ? 1U : 0U,
         ReadRegister(kV3dCoreBase, kCoreL2TCacheControl));

  CleanAndInvalidateDataCacheRange(reinterpret_cast<uintptr>(g_test_target.cpu),
                                   g_test_target.size);
  u32 mismatch = kTestWidth * kTestHeight;
  for (u32 i = 0; i < kTestWidth * kTestHeight; ++i) {
    if (target[i] != source[i]) {
      mismatch = i;
      break;
    }
  }
  const u32 source_hash = Fnv1a32(source, kTestReadbackBytes);
  const u32 target_hash = Fnv1a32(target, kTestReadbackBytes);
  u32 first_changed = g_test_target.size / sizeof(u32);
  for (u32 i = 0; i < g_test_target.size / sizeof(u32); ++i) {
    if (target[i] != 0xdeadc0deU) {
      first_changed = i;
      break;
    }
  }
  const u32 whole_target_hash = Fnv1a32(target, g_test_target.size);
  const bool readback_ok = mismatch == kTestWidth * kTestHeight &&
                           source_hash == target_hash && cache_clean;
  printf("boot: pi4v3d readback ok=%u source_hash=0x%08x "
         "target_hash=0x%08x first=0x%08x middle=0x%08x "
         "last=0x%08x mismatch=%u first_changed=%u "
         "whole_hash=0x%08x\r\n",
         readback_ok ? 1U : 0U, source_hash, target_hash, target[0],
         target[(kTestWidth * kTestHeight) / 2U],
         target[kTestWidth * kTestHeight - 1U], mismatch, first_changed,
         whole_target_hash);

  g_m1_passed = readback_ok &&
                (hub_int & (kHubIntTfuFault | kHubIntMmuFaultMask)) == 0U;
  printf("boot: pi4v3d test=mmu status=%s "
         "mmu=ready buffers=ready offscreen-readback=%s\r\n",
         g_m1_passed ? "pass" : "fail",
         readback_ok ? "pass" : "fail");
  return g_m1_passed;
}

Buffer *RenderTargetForSlot(u32 slot) {
  if (slot == 0U) {
    return &g_test_target;
  }
  return slot == 1U ? &g_render_target_alt : nullptr;
}

bool PrepareFragmentReplayJobs() {
  FillScanlineProbeSource(g_test_source.cpu, g_test_source.size);
  memset(g_render_tile_scratch.cpu, 0, g_render_tile_scratch.size);

  for (u32 slot = 0U; slot < kRenderJobSlotCount; ++slot) {
    Buffer *target = RenderTargetForSlot(slot);
    Buffer &control = g_render_controls[slot];
    if (target == nullptr || target->size < kScanlineProbeTargetBytes) {
      return false;
    }
    RenderAddresses addresses = {};
    addresses.control = control.v3d_address;
    addresses.source_texture = g_test_source.v3d_address;
    addresses.target = target->v3d_address;
    addresses.tile_allocation = g_render_tile_scratch.v3d_address;
    addresses.tile_state = g_render_tile_scratch.v3d_address +
                           kRenderTileStateOffset;
    if (!PrepareScanlineProbeRenderJob(
            addresses, control.cpu, control.size, slot,
            &g_render_jobs[slot])) {
      return false;
    }

    const RenderCommandLists &lists = g_render_jobs[slot].lists;
    printf("boot: pi4v3d render job prepared slot=%u "
           "control=0x%08x target=0x%08x "
           "bcl=0x%08x..0x%08x rcl=0x%08x..0x%08x "
           "generic=0x%08x..0x%08x control_hash=0x%08x\r\n",
           static_cast<unsigned>(slot),
           static_cast<unsigned>(addresses.control),
           static_cast<unsigned>(addresses.target),
           static_cast<unsigned>(lists.bcl_start),
           static_cast<unsigned>(lists.bcl_end),
           static_cast<unsigned>(lists.rcl_start),
           static_cast<unsigned>(lists.rcl_end),
           static_cast<unsigned>(lists.generic_start),
           static_cast<unsigned>(lists.generic_end),
           static_cast<unsigned>(g_render_jobs[slot].control_hash));
  }

  printf("boot: pi4v3d render resources slots=%u target=%ux%u source=4x4 "
         "tile_alloc=0x%08x tile_alloc_bytes=%u "
         "tsda=0x%08x tsda_bytes=%u\r\n",
         static_cast<unsigned>(kRenderJobSlotCount),
         static_cast<unsigned>(kScanlineProbeWidth),
         static_cast<unsigned>(kScanlineProbeHeight),
         static_cast<unsigned>(g_render_tile_scratch.v3d_address),
         static_cast<unsigned>(kRenderTileAllocationBytes),
         static_cast<unsigned>(g_render_tile_scratch.v3d_address +
                               kRenderTileStateOffset),
         static_cast<unsigned>(kRenderTileStateBytes));

  CleanBufferForV3d(g_test_source);
  CleanBufferForV3d(g_render_tile_scratch);
  for (u32 slot = 0U; slot < kRenderJobSlotCount; ++slot) {
    CleanBufferForV3d(g_render_controls[slot]);
  }
  DataSyncBarrier();
  return true;
}

bool ExecuteFragmentReplayJob(u32 sequence, u32 slot, u32 *readback_hash) {
  if (slot >= kRenderJobSlotCount || readback_hash == nullptr) {
    return false;
  }
  Buffer *target = RenderTargetForSlot(slot);
  Buffer &control = g_render_controls[slot];
  RenderJob &job = g_render_jobs[slot];
  if (target == nullptr ||
      !RenderJobControlIntact(job, control.cpu, control.size)) {
    printf("boot: pi4v3d render job sequence=%u slot=%u "
           "status=fail phase=control-integrity\r\n",
           static_cast<unsigned>(sequence), static_cast<unsigned>(slot));
    return false;
  }

  FillScanlineProbeTarget(target->cpu, target->size);
  CleanBufferForV3d(*target);
  DataSyncBarrier();
  if (!StartRenderJob(&job)) {
    printf("boot: pi4v3d render job sequence=%u slot=%u "
           "status=fail phase=lifecycle-start state=%u\r\n",
           static_cast<unsigned>(sequence), static_cast<unsigned>(slot),
           static_cast<unsigned>(job.state));
    return false;
  }

  u32 binner_us = 0U;
  const bool binner_ok = SubmitBcl(
      job.lists, g_render_tile_scratch, kRenderTileStateOffset,
      &binner_us, true);
  u32 renderer_us = 0U;
  const bool renderer_ok =
      binner_ok && SubmitRcl(job.lists, &renderer_us, true);
  const bool cache_clean = renderer_ok && CleanV3dCaches();
  if (renderer_ok) {
    InvalidateBufferFromV3d(*target);
    DataSyncBarrier();
  }

  RenderReadback readback = {};
  const bool readback_ok =
      renderer_ok &&
      AnalyzeScanlineProbeTarget(target->cpu, target->size, &readback);
  const bool control_intact =
      RenderJobControlIntact(job, control.cpu, control.size);
  const bool succeeded = binner_ok && renderer_ok && cache_clean &&
                         readback_ok && control_intact;
  const bool lifecycle_finished = FinishRenderJob(&job, succeeded);
  *readback_hash = readback.hash;

  printf("boot: pi4v3d render job sequence=%u slot=%u submit=%u "
         "status=%s binner=%u renderer=%u cache_clean=%u "
         "control_intact=%u binner_us=%u renderer_us=%u "
         "hash=0x%08x changed=%u/%u nonzero=%u/%u unique=%u "
         "guard_mismatches=%u\r\n",
         static_cast<unsigned>(sequence), static_cast<unsigned>(slot),
         static_cast<unsigned>(job.submission_count),
         succeeded && lifecycle_finished ? "pass" : "fail",
         binner_ok ? 1U : 0U, renderer_ok ? 1U : 0U,
         cache_clean ? 1U : 0U, control_intact ? 1U : 0U,
         static_cast<unsigned>(binner_us),
         static_cast<unsigned>(renderer_us),
         static_cast<unsigned>(readback.hash),
         static_cast<unsigned>(readback.changed_pixels),
         static_cast<unsigned>(kScanlineProbeWidth * kScanlineProbeHeight),
         static_cast<unsigned>(readback.nonzero_pixels),
         static_cast<unsigned>(kScanlineProbeWidth * kScanlineProbeHeight),
         static_cast<unsigned>(readback.unique_colors),
         static_cast<unsigned>(readback.guard_mismatches));
  return succeeded && lifecycle_finished;
}

bool InitializeFragmentReplayResources(const char *test_name) {
  if (!InitializeRenderBuffers() ||
      g_test_source.size < kScanlineProbeSourceBytes ||
      g_test_target.size < kScanlineProbeTargetBytes ||
      g_render_target_alt.size < kScanlineProbeTargetBytes ||
      !PrepareFragmentReplayJobs()) {
    printf("boot: pi4v3d test=%s status=fail "
           "phase=render-resource-setup\r\n", test_name);
    return false;
  }
  return true;
}

bool RunM2FragmentReplayTest() {
  if (g_m2_attempted) {
    return g_m2_passed;
  }
  g_m2_attempted = true;

  if (!RunM1OffscreenTest()) {
    printf("boot: pi4v3d test=fragment_replay status=fail "
           "phase=m1-safety-ladder\r\n");
    return false;
  }
  if (!InitializeFragmentReplayResources("fragment_replay")) {
    return false;
  }

  g_m2_passed = ExecuteFragmentReplayJob(0U, 0U, &g_m2_readback_hash);
  printf("boot: pi4v3d test=fragment_replay status=%s "
         "m1=pass binner=%s renderer=%s offscreen-readback=%s\r\n",
         g_m2_passed ? "pass" : "fail",
         g_m2_passed ? "pass" : "fail",
         g_m2_passed ? "pass" : "fail",
         g_m2_passed ? "pass" : "fail");
  return g_m2_passed;
}

bool RunM3FragmentLifecycleTest() {
  if (g_m3_attempted) {
    return g_m3_passed;
  }
  g_m3_attempted = true;

  if (!RunM2FragmentReplayTest()) {
    printf("boot: pi4v3d test=fragment_lifecycle status=fail "
           "phase=m2-safety-ladder\r\n");
    return false;
  }

  u32 slot1_hash = 0U;
  u32 slot0_reuse_hash = 0U;
  const bool slot1_ok =
      ExecuteFragmentReplayJob(1U, 1U, &slot1_hash);
  const bool slot0_reuse_ok =
      slot1_ok && ExecuteFragmentReplayJob(2U, 0U, &slot0_reuse_hash);
  const bool stable_readback =
      slot0_reuse_ok && g_m2_readback_hash != 0U &&
      slot1_hash == g_m2_readback_hash &&
      slot0_reuse_hash == g_m2_readback_hash;
  const bool lifecycle_ok =
      g_render_jobs[0].state == kRenderJobCompleted &&
      g_render_jobs[1].state == kRenderJobCompleted &&
      g_render_jobs[0].submission_count == 2U &&
      g_render_jobs[1].submission_count == 1U;
  g_m3_passed = slot1_ok && slot0_reuse_ok && stable_readback &&
                lifecycle_ok;
  printf("boot: pi4v3d test=fragment_lifecycle status=%s "
         "m2=pass jobs=%u slots=%u slot0_submits=%u slot1_submits=%u "
         "stable_readback=%u resources_reused=%u hash=0x%08x\r\n",
         g_m3_passed ? "pass" : "fail",
         static_cast<unsigned>(kRenderJobExecutionCount),
         static_cast<unsigned>(kRenderJobSlotCount),
         static_cast<unsigned>(g_render_jobs[0].submission_count),
         static_cast<unsigned>(g_render_jobs[1].submission_count),
         stable_readback ? 1U : 0U, lifecycle_ok ? 1U : 0U,
         static_cast<unsigned>(g_m2_readback_hash));
  return g_m3_passed;
}

bool RunM4FragmentScanoutTest(const ScanoutTarget &target) {
  if (g_m4_attempted) {
    return g_m4_passed;
  }
  g_m4_attempted = true;

  if (target.pixels == nullptr || target.width < kBootScanoutWidth ||
      target.height < kBootScanoutHeight ||
      target.pitch < kBootScanoutPitch) {
    printf("boot: pi4v3d test=fragment_scanout status=fail "
           "phase=scanout-target width=%u height=%u pitch=%u\r\n",
           static_cast<unsigned>(target.width),
           static_cast<unsigned>(target.height),
           static_cast<unsigned>(target.pitch));
    return false;
  }
  if (!RunM3FragmentLifecycleTest()) {
    printf("boot: pi4v3d test=fragment_scanout status=fail "
           "phase=m3-safety-ladder\r\n");
    return false;
  }

  uint32_t scanout_hash = 0U;
  const bool copied = CopyScanlineProbeToScanout(
      g_test_target.cpu, g_test_target.size, target.pixels,
      target.width, target.height, target.pitch, &scanout_hash);
  g_m4_passed = copied && scanout_hash == g_m2_readback_hash;
  printf("boot: pi4v3d test=fragment_scanout stage=handoff-ready "
         "ready=%u m3=pass copy=%u format=rgb565 size=%ux%u pitch=%u "
         "hash=0x%08x stable_readback=%u\r\n",
         g_m4_passed ? 1U : 0U, copied ? 1U : 0U,
         static_cast<unsigned>(kBootScanoutWidth),
         static_cast<unsigned>(kBootScanoutHeight),
         static_cast<unsigned>(target.pitch),
         static_cast<unsigned>(scanout_hash),
         scanout_hash == g_m2_readback_hash ? 1U : 0U);
  return g_m4_passed;
}

void ResetFrameResources(bool release_storage) {
  if (release_storage) {
    FreeBuffer(&g_frame_source);
    FreeBuffer(&g_frame_intermediate);
    FreeBuffer(&g_frame_output_intermediate);
  }
  for (u32 slot = 0U; slot < kRenderJobSlotCount; ++slot) {
    if (release_storage) {
      FreeBuffer(&g_frame_targets[slot]);
      FreeBuffer(&g_frame_controls[slot]);
      FreeBuffer(&g_frame_source_pass_controls[slot]);
      FreeBuffer(&g_frame_output_pass_controls[slot]);
    }
    ResetRenderJob(&g_frame_jobs[slot]);
    ResetRenderJob(&g_frame_source_pass_jobs[slot]);
    ResetRenderJob(&g_frame_output_pass_jobs[slot]);
  }
  if (release_storage) {
    FreeBuffer(&g_frame_self_test_control);
  }
  ResetRenderJob(&g_frame_self_test_job);
  if (release_storage) {
    FreeBuffer(&g_frame_tile_scratch);
  }
  g_frame_width = 0U;
  g_frame_height = 0U;
  g_frame_target_width = 0U;
  g_frame_target_height = 0U;
  g_frame_source_bytes = 0U;
  g_frame_intermediate_bytes = 0U;
  g_frame_output_intermediate_bytes = 0U;
  g_frame_target_bytes = 0U;
  g_frame_tile_state_bytes = 0U;
  g_frame_sequence = 0U;
  g_last_rendered_frame = {};
  memset(&g_frame_source_layout, 0, sizeof g_frame_source_layout);
  memset(&g_frame_intermediate_layout, 0,
         sizeof g_frame_intermediate_layout);
  memset(&g_frame_output_intermediate_layout, 0,
         sizeof g_frame_output_intermediate_layout);
  g_frame_self_test_passed = false;
  g_frame_effect_valid = false;
  g_frame_multipass_active = false;
  g_frame_source_pass_active = false;
  g_frame_output_pass_active = false;
  g_frame_output_pass_edge_glow = false;
  g_frame_output_pass_mode = kFrameOutputPassOff;
  g_frame_effect_output_log_pending = false;
  g_frame_last_effect_change = false;
  g_frame_geometry_enabled = false;
  g_frame_curvature_x = 0.0f;
  g_frame_curvature_y = 0.0f;
  g_frame_skew_x = 0.0f;
  g_frame_skew_y = 0.0f;
  g_frame_trapezoid = 0.0f;
  g_frame_rotation_degrees = 0.0f;
  g_frame_overscan_scale = 1.0f;
  g_frame_scanline_weight = 0.0f;
  g_frame_scanline_gap_brightness = 1.0f;
  g_frame_scanline_multisample = false;
  g_frame_edge_blur_enabled = false;
  g_frame_edge_blur_strength = 0.0f;
  g_frame_edge_blur_radius = 0.2f;
  g_frame_phosphor_mask_enabled = false;
  g_frame_phosphor_mask_pattern = 1U;
  g_frame_phosphor_mask_brightness = 1.0f;
  g_frame_vignette_enabled = false;
  g_frame_vignette_strength = 0.0f;
  g_frame_vignette_scale = 1.0f;
  g_frame_vignette_softness = 0.02f;
  g_frame_uneven_illumination_enabled = false;
  g_frame_uneven_illumination_strength = 0.0f;
  g_frame_uneven_illumination_scale = 0.02f;
  g_frame_glass_reflection_enabled = false;
  g_frame_glass_reflection_angle = 0.0f;
  g_frame_glass_reflection_width = 0.02f;
  g_frame_glass_reflection_position = 0.0f;
  g_frame_rounded_screen_mask_enabled = false;
  g_frame_rounded_corner_radius = 0.0f;
  g_frame_rounded_border_softness = 0.0f;
  g_frame_edge_glow_enabled = false;
  g_frame_edge_glow_strength = 0.0f;
  g_frame_edge_glow_width = 0.01f;
  g_frame_output_response_enabled = false;
  g_frame_output_response_fast = false;
  g_frame_output_level_mapping = 1U;
  g_frame_input_gamma = 1.0f;
  g_frame_output_gamma = 1.0f;
  g_frame_output_saturation = 1.0f;
  g_frame_black_level = 0.0f;
  g_frame_white_clip = 1.0f;
  g_frame_convergence_enabled = false;
  g_frame_red_offset_x = 0.0f;
  g_frame_red_offset_y = 0.0f;
  g_frame_blue_offset_x = 0.0f;
  g_frame_blue_offset_y = 0.0f;
  g_frame_convergence_radial_strength = 0.0f;
  g_frame_horizontal_filtering_enabled = false;
  g_frame_horizontal_sigma_x = 0.0f;
  g_frame_bloom_enabled = false;
  g_frame_bloom_factor = 0.0f;
  g_frame_horizontal_jitter_enabled = false;
  g_frame_horizontal_jitter_strength = 0.0f;
  g_frame_horizontal_jitter_frequency = 0.01f;
  g_frame_horizontal_jitter_speed = 0.0f;
  g_frame_composite_artifacts_enabled = false;
  g_frame_composite_chroma_blur = 0.0f;
  g_frame_composite_luma_sharpen = 0.0f;
  g_frame_composite_color_bleed = 0.0f;
  g_frame_noise_enabled = false;
  g_frame_luminance_noise = 0.0f;
  g_frame_chroma_noise = 0.0f;
  g_frame_noise_speed = 0.0f;
  g_frame_temporal_package_active = false;
  g_frame_output_response_lut_valid = false;
  g_frame_output_response_palette_valid = false;
  g_frame_output_response_lut_build_us = 0U;
  g_frame_source_upload_us = 0U;
  g_frame_source_linear_filter = false;
  v3dcrt::ResetEdgeGlowTemporalFilter(&g_edge_glow_temporal_filter);
}

bool FrameByteSizes(u32 source_width, u32 source_height,
                    u32 target_width, u32 target_height,
                    v3d42::Rgba8TextureLayout *source_layout,
                    u32 *target_bytes) {
  if (source_layout == nullptr || target_bytes == nullptr ||
      source_width == 0U || source_height == 0U ||
      target_width == 0U || target_height == 0U ||
      target_width > UINT32_MAX / sizeof(u16) ||
      !v3d42::ComputeRgba8TextureLayout(
          source_width, source_height, source_layout)) {
    return false;
  }
  const u32 target_row = target_width * sizeof(u16);
  if (target_height > UINT32_MAX / target_row) {
    return false;
  }
  *target_bytes = target_row * target_height;
  return true;
}

bool EnsureMappedBufferCapacity(const char *name, u32 requested_size,
                                Buffer *buffer) {
  if (buffer == nullptr || requested_size == 0U ||
      requested_size > UINT32_MAX - (kMmuPageSize - 1U)) {
    return false;
  }
  const u32 mapped_size =
      (requested_size + kMmuPageSize - 1U) & ~(kMmuPageSize - 1U);
  if (buffer->cpu != nullptr && buffer->size >= mapped_size) {
    return true;
  }

  Buffer replacement = {};
  if (!AllocateAndMapBuffer(name, requested_size, &replacement)) {
    return false;
  }
  FreeBuffer(buffer);
  *buffer = replacement;
  return true;
}

bool PrepareFrameRenderJobs(u32 source_width, u32 source_height,
                            u32 target_width, u32 target_height,
                            const RenderGeometryParams &effect_geometry,
                            const RenderScanlineParams &scanlines,
                            bool scanline_multisample,
                            const RenderEdgeBlurParams &edge_blur,
                            const RenderPhosphorMaskParams &phosphor_mask,
                            const RenderVignetteParams &vignette,
                            const RenderUnevenIlluminationParams &
                                uneven_illumination,
                            const RenderGlassReflectionParams &
                                glass_reflection,
                            const RenderRoundedScreenMaskParams &
                                rounded_screen_mask,
                            const RenderEdgeGlowParams &edge_glow,
                            const RenderOutputResponseParams &output_response,
                            const RenderConvergenceParams &convergence,
                            const RenderHorizontalFilteringParams &
                                horizontal_filtering,
                            const RenderBloomParams &bloom,
                            const RenderHorizontalJitterParams &
                                horizontal_jitter,
                            const RenderCompositeArtifactsParams &
                                composite_artifacts,
                            const RenderNoiseParams &noise,
                            bool linear_filter) {
  const bool source_multipass = convergence.enabled ||
      horizontal_filtering.enabled || horizontal_jitter.enabled ||
      composite_artifacts.enabled || noise.enabled;
  RenderGeometry output_geometry = {
    source_width,
    source_height,
    true,
    linear_filter,
    target_width,
    target_height,
    target_width * sizeof(u16),
    scanlines.weight,
    scanlines.gap_brightness,
    edge_blur.enabled,
    edge_blur.strength,
    edge_blur.radius,
    phosphor_mask.enabled,
    phosphor_mask.pattern,
    phosphor_mask.brightness,
    vignette.enabled,
    vignette.strength,
    vignette.scale,
    vignette.softness,
    uneven_illumination.enabled,
    uneven_illumination.strength,
    uneven_illumination.scale,
    glass_reflection.enabled,
    glass_reflection.angle,
    glass_reflection.width,
    glass_reflection.position,
    rounded_screen_mask.enabled,
    rounded_screen_mask.corner_radius,
    rounded_screen_mask.border_softness,
    edge_glow.enabled,
    edge_glow.strength,
    edge_glow.width,
    output_response.enabled,
    output_response.fast,
    output_response.level_mapping,
    output_response.input_gamma,
    output_response.output_gamma,
    output_response.saturation,
    output_response.black_level,
    output_response.white_clip,
    effect_geometry.enabled,
    effect_geometry.curvature_x,
    effect_geometry.curvature_y,
    effect_geometry.skew_x,
    effect_geometry.skew_y,
    effect_geometry.trapezoid,
    effect_geometry.rotation_degrees,
    effect_geometry.overscan_scale,
    {
      convergence.enabled,
      convergence.red_offset_x,
      convergence.red_offset_y,
      convergence.blue_offset_x,
      convergence.blue_offset_y,
      convergence.radial_strength,
      horizontal_filtering.enabled,
      horizontal_filtering.sigma_x,
      bloom.enabled,
      bloom.factor,
      horizontal_jitter.enabled,
      horizontal_jitter.strength,
      horizontal_jitter.frequency,
      horizontal_jitter.speed,
      composite_artifacts.enabled,
      composite_artifacts.chroma_blur,
      composite_artifacts.luma_sharpen,
      composite_artifacts.color_bleed,
      noise.enabled,
      noise.luminance,
      noise.chroma,
      noise.speed,
      0.0f,
      scanline_multisample
    }
  };
  RenderGeometry source_geometry = {};
  RenderPassConfig source_pass = {};
  if (source_multipass) {
    v3d42::Rgba8RenderTargetStoreConfig intermediate_store = {};
    if (!v3d42::GetRgba8RenderTargetStoreConfig(
            g_frame_intermediate_layout, &intermediate_store)) {
      return false;
    }

    source_geometry = output_geometry;
    source_geometry.source_linear_filter =
        convergence.enabled || horizontal_jitter.enabled;
    source_geometry.target_width = source_width;
    source_geometry.target_height = source_height;
    source_geometry.target_stride =
        g_frame_intermediate_layout.padded_row_bytes;
    source_geometry.scanline_weight = 0.0f;
    source_geometry.scanline_gap_brightness = 1.0f;
    source_geometry.edge_blur_enabled = false;
    source_geometry.phosphor_mask_enabled = false;
    source_geometry.vignette_enabled = false;
    source_geometry.uneven_illumination_enabled = false;
    source_geometry.glass_reflection_enabled = false;
    source_geometry.rounded_screen_mask_enabled = false;
    source_geometry.edge_glow_enabled = false;
    source_geometry.output_response_enabled = false;
    source_geometry.geometry_enabled = false;
    source_geometry.standalone.bloom_enabled = false;
    source_pass.kind = kRenderPassSource;
    source_pass.target_format = kRenderTargetRgba8Tiled;
    source_pass.target_memory_format =
        intermediate_store.memory_format;
    source_pass.target_height_in_ub_or_stride =
        intermediate_store.height_in_ub_or_stride;
    source_pass.package_class = kRenderPackageAutomatic;

    RenderStandaloneEffects output_effects = {};
    output_effects.bloom_enabled = bloom.enabled;
    output_effects.bloom_factor = bloom.factor;
    output_effects.scanline_multisample = scanline_multisample;
    output_geometry.standalone = output_effects;
  }

  const bool late_effects_enabled = uneven_illumination.enabled ||
      glass_reflection.enabled || rounded_screen_mask.enabled ||
      edge_glow.enabled;
  const bool post_effects_enabled =
      vignette.enabled || late_effects_enabled;
  // Geometry and source-row scanlines must precede Bloom because they change
  // where source light lands. Edge Blur and Phosphor can instead consume the
  // source-resolution Bloom result in the final output pass. That keeps their
  // two-effect paths within the Pi4 frame budget without dropping a frame.
  const bool bloom_pre_effects_enabled =
      effect_geometry.enabled || scanlines.weight > 0.0f;
  const bool bloom_post_effects_enabled =
      edge_blur.enabled || phosphor_mask.enabled || post_effects_enabled;
  const bool bloom_after_early_split = bloom.enabled &&
      bloom_pre_effects_enabled;
  const bool bloom_before_post_split = bloom.enabled &&
      !bloom_pre_effects_enabled && bloom_post_effects_enabled;
  const bool output_multipass =
      bloom_after_early_split || bloom_before_post_split;
  const FrameOutputPassMode output_pass_mode = bloom_after_early_split ?
      kFrameOutputPassBloom :
      (bloom_before_post_split ? kFrameOutputPassBloomSource :
       kFrameOutputPassOff);

  RenderGeometry output_prepass_geometry = output_geometry;
  RenderGeometry final_geometry = output_geometry;
  RenderPassConfig output_prepass = {};
  RenderPassConfig final_pass = {
    kRenderPassAutomatic,
    kRenderTargetRgb565Raster,
    0U,
    0U,
    kRenderPackageAutomatic
  };
  if (output_multipass) {
    v3d42::Rgba8RenderTargetStoreConfig output_intermediate_store = {};
    const v3d42::Rgba8TextureLayout &output_pass_layout =
        output_pass_mode == kFrameOutputPassBloomSource ?
            g_frame_intermediate_layout :
            g_frame_output_intermediate_layout;
    if (!v3d42::GetRgba8RenderTargetStoreConfig(
            output_pass_layout,
            &output_intermediate_store)) {
      return false;
    }
    output_prepass.target_format = kRenderTargetRgba8Tiled;
    output_prepass.kind = kRenderPassOutput;
    output_prepass.target_memory_format =
        output_intermediate_store.memory_format;
    output_prepass.target_height_in_ub_or_stride =
        output_intermediate_store.height_in_ub_or_stride;
    output_prepass.package_class = kRenderPackageAutomatic;
    output_prepass_geometry.target_stride =
        output_pass_layout.padded_row_bytes;

    final_geometry.source_width = target_width;
    final_geometry.source_height = target_height;
    final_geometry.source_uses_hardware_tiling = true;
    final_geometry.source_linear_filter = false;
    final_geometry.scanline_weight = 0.0f;
    final_geometry.scanline_gap_brightness = 1.0f;
    final_geometry.edge_blur_enabled = false;
    final_geometry.phosphor_mask_enabled = false;
    final_geometry.output_response_enabled = false;
    final_geometry.geometry_enabled = false;
    final_geometry.standalone = {};
    final_pass.kind = kRenderPassOutput;

    if (output_pass_mode == kFrameOutputPassBloom) {
      output_prepass_geometry.standalone.bloom_enabled = false;
      final_geometry.vignette_enabled = false;
      final_geometry.uneven_illumination_enabled = false;
      final_geometry.glass_reflection_enabled = false;
      final_geometry.rounded_screen_mask_enabled = false;
      final_geometry.edge_glow_enabled = false;
      final_geometry.standalone.bloom_enabled = true;
      final_geometry.standalone.bloom_factor = bloom.factor;
      final_pass.package_class = kRenderPackagePi4Bloom;
    } else {
      output_prepass_geometry.target_width = source_width;
      output_prepass_geometry.target_height = source_height;
      output_prepass_geometry.scanline_weight = 0.0f;
      output_prepass_geometry.scanline_gap_brightness = 1.0f;
      output_prepass_geometry.edge_blur_enabled = false;
      output_prepass_geometry.phosphor_mask_enabled = false;
      output_prepass_geometry.vignette_enabled = false;
      output_prepass_geometry.uneven_illumination_enabled = false;
      output_prepass_geometry.glass_reflection_enabled = false;
      output_prepass_geometry.rounded_screen_mask_enabled = false;
      output_prepass_geometry.edge_glow_enabled = false;
      output_prepass_geometry.output_response_enabled = false;
      output_prepass_geometry.geometry_enabled = false;
      output_prepass_geometry.standalone = {};
      output_prepass_geometry.standalone.bloom_enabled = true;
      output_prepass_geometry.standalone.bloom_factor = bloom.factor;
      output_prepass.package_class = kRenderPackagePi4Bloom;

      final_geometry = output_geometry;
      final_geometry.standalone.bloom_enabled = false;
      final_pass.package_class = kRenderPackageAutomatic;
    }
  }

  for (u32 slot = 0U; slot < kRenderJobSlotCount; ++slot) {
    if (source_multipass) {
      RenderAddresses source_addresses = {};
      source_addresses.control =
          g_frame_source_pass_controls[slot].v3d_address;
      source_addresses.source_texture = g_frame_source.v3d_address;
      source_addresses.target = g_frame_intermediate.v3d_address;
      source_addresses.tile_allocation =
          g_frame_tile_scratch.v3d_address;
      source_addresses.tile_state =
          g_frame_tile_scratch.v3d_address + kRenderTileStateOffset;
      if (!PrepareFullscreenRenderPassJob(
              source_addresses, source_geometry, source_pass,
              g_frame_source_pass_controls[slot].cpu,
              g_frame_source_pass_controls[slot].size, slot,
              &g_frame_source_pass_jobs[slot])) {
        return false;
      }
      CleanBufferForV3d(g_frame_source_pass_controls[slot]);
    } else {
      ResetRenderJob(&g_frame_source_pass_jobs[slot]);
    }

    if (output_multipass) {
      RenderAddresses output_pass_addresses = {};
      output_pass_addresses.control =
          g_frame_output_pass_controls[slot].v3d_address;
      output_pass_addresses.source_texture = source_multipass ?
          g_frame_intermediate.v3d_address : g_frame_source.v3d_address;
      output_pass_addresses.target =
          g_frame_output_intermediate.v3d_address;
      output_pass_addresses.tile_allocation =
          g_frame_tile_scratch.v3d_address;
      output_pass_addresses.tile_state =
          g_frame_tile_scratch.v3d_address + kRenderTileStateOffset;
      if (!PrepareFullscreenRenderPassJob(
              output_pass_addresses, output_prepass_geometry,
              output_prepass, g_frame_output_pass_controls[slot].cpu,
              g_frame_output_pass_controls[slot].size, slot,
              &g_frame_output_pass_jobs[slot])) {
        return false;
      }
      CleanBufferForV3d(g_frame_output_pass_controls[slot]);
    } else {
      ResetRenderJob(&g_frame_output_pass_jobs[slot]);
    }

    RenderAddresses addresses = {};
    addresses.control = g_frame_controls[slot].v3d_address;
    addresses.source_texture = output_multipass ?
        g_frame_output_intermediate.v3d_address :
        (source_multipass ?
            g_frame_intermediate.v3d_address : g_frame_source.v3d_address);
    addresses.target = g_frame_targets[slot].v3d_address;
    addresses.tile_allocation = g_frame_tile_scratch.v3d_address;
    addresses.tile_state = g_frame_tile_scratch.v3d_address +
                           kRenderTileStateOffset;
    if (!PrepareFullscreenRenderPassJob(
            addresses, final_geometry, final_pass,
            g_frame_controls[slot].cpu, g_frame_controls[slot].size,
            slot, &g_frame_jobs[slot])) {
      return false;
    }
    CleanBufferForV3d(g_frame_controls[slot]);
  }
  g_frame_source_pass_active = source_multipass;
  g_frame_output_pass_active = output_multipass;
  g_frame_output_pass_edge_glow =
      output_pass_mode == kFrameOutputPassBloom && edge_glow.enabled;
  g_frame_output_pass_mode = output_pass_mode;
  g_frame_multipass_active = source_multipass || output_multipass;
  DataSyncBarrier();
  return true;
}

bool PrepareFrameSelfTestJob(u32 source_width, u32 source_height,
                             u32 target_width, u32 target_height) {
  const RenderGeometry geometry = {
    source_width,
    source_height,
    true,
    false,
    target_width,
    target_height,
    target_width * sizeof(u16),
    0.0f,
    1.0f,
    false,
    0.0f,
    0.2f,
    false,
    1U,
    1.0f,
    false,
    0.0f,
    1.0f,
    0.02f,
    false,
    0.0f,
    0.02f,
    false,
    0.0f,
    0.02f,
    0.0f,
    false,
    0.0f,
    0.0f,
    false,
    0.0f,
    0.01f,
    false,
    false,
    1U,
    1.0f,
    1.0f,
    1.0f,
    0.0f,
    1.0f
  };
  RenderAddresses addresses = {};
  addresses.control = g_frame_self_test_control.v3d_address;
  addresses.source_texture = g_frame_source.v3d_address;
  addresses.target = g_frame_targets[0].v3d_address;
  addresses.tile_allocation = g_frame_tile_scratch.v3d_address;
  addresses.tile_state = g_frame_tile_scratch.v3d_address +
                         kRenderTileStateOffset;
  if (!PrepareFullscreenRenderJob(
          addresses, geometry, g_frame_self_test_control.cpu,
          g_frame_self_test_control.size, 0U, &g_frame_self_test_job)) {
    return false;
  }
  CleanBufferForV3d(g_frame_self_test_control);
  DataSyncBarrier();
  return true;
}

bool InitializeFrameResources(u32 source_width, u32 source_height,
                              u32 target_width, u32 target_height) {
  if (!g_mmu_ready || !g_runtime_armed || g_runtime_failed) {
    return false;
  }
  if (g_frame_source.cpu != nullptr && g_frame_width == source_width &&
      g_frame_height == source_height &&
      g_frame_target_width == target_width &&
      g_frame_target_height == target_height && g_frame_self_test_passed) {
    return true;
  }

  // Circle cannot return allocations larger than its biggest heap bucket to
  // a free list.  Keep the largest V3D allocation seen for each role and only
  // rebuild geometry-dependent state.  This also makes repeated machine/mode
  // matrices bounded instead of consuming the DMA30 heap on every resize.
  ResetFrameResources(false);
  v3d42::Rgba8TextureLayout source_layout = {};
  v3d42::Rgba8TextureLayout output_layout = {};
  u32 target_bytes = 0U;
  const u32 tile_state_bytes =
      RenderTileStateBytes(target_width, target_height);
  if (!FrameByteSizes(source_width, source_height,
                      target_width, target_height,
                      &source_layout, &target_bytes) ||
      !v3d42::ComputeRgba8TextureLayout(
          target_width, target_height, &output_layout) ||
      tile_state_bytes == 0U ||
      tile_state_bytes > UINT32_MAX - kRenderTileStateOffset) {
    printf("boot: pi4v3d frame resources status=fail phase=geometry "
           "source=%ux%u target=%ux%u tiles=%u\r\n",
           static_cast<unsigned>(source_width),
           static_cast<unsigned>(source_height),
           static_cast<unsigned>(target_width),
           static_cast<unsigned>(target_height),
           static_cast<unsigned>(
               RenderTileCount(target_width, target_height)));
    return false;
  }

  if (!EnsureMappedBufferCapacity("frame-source", source_layout.size_bytes,
                                  &g_frame_source) ||
      !EnsureMappedBufferCapacity("frame-intermediate",
                                  source_layout.size_bytes,
                                  &g_frame_intermediate) ||
      !EnsureMappedBufferCapacity("frame-output-intermediate",
                                  output_layout.size_bytes,
                                  &g_frame_output_intermediate) ||
      !EnsureMappedBufferCapacity("frame-target-0", target_bytes,
                                  &g_frame_targets[0]) ||
      !EnsureMappedBufferCapacity("frame-target-1", target_bytes,
                                  &g_frame_targets[1]) ||
      !EnsureMappedBufferCapacity("frame-control-0", kRenderControlBytes,
                                  &g_frame_controls[0]) ||
      !EnsureMappedBufferCapacity("frame-control-1", kRenderControlBytes,
                                  &g_frame_controls[1]) ||
      !EnsureMappedBufferCapacity("frame-source-control-0",
                                  kRenderControlBytes,
                                  &g_frame_source_pass_controls[0]) ||
      !EnsureMappedBufferCapacity("frame-source-control-1",
                                  kRenderControlBytes,
                                  &g_frame_source_pass_controls[1]) ||
      !EnsureMappedBufferCapacity("frame-output-control-0",
                                  kRenderControlBytes,
                                  &g_frame_output_pass_controls[0]) ||
      !EnsureMappedBufferCapacity("frame-output-control-1",
                                  kRenderControlBytes,
                                  &g_frame_output_pass_controls[1]) ||
      !EnsureMappedBufferCapacity("frame-selftest-control",
                                  kRenderControlBytes,
                                  &g_frame_self_test_control) ||
      !EnsureMappedBufferCapacity("frame-tile",
                                  kRenderTileStateOffset + tile_state_bytes,
                                  &g_frame_tile_scratch)) {
    printf("boot: pi4v3d frame resources status=fail phase=allocation "
           "source=%ux%u target=%ux%u layout=%s source_bytes=%u\r\n",
           static_cast<unsigned>(source_width),
           static_cast<unsigned>(source_height),
           static_cast<unsigned>(target_width),
           static_cast<unsigned>(target_height),
           v3d42::Rgba8TextureTilingName(source_layout.tiling),
           static_cast<unsigned>(source_layout.size_bytes));
    ResetFrameResources(false);
    return false;
  }

  memset(g_frame_tile_scratch.cpu, 0, g_frame_tile_scratch.size);
  if (!PrepareFrameSelfTestJob(source_width, source_height,
                               target_width, target_height)) {
    printf("boot: pi4v3d frame resources status=fail "
           "phase=selftest-control-build source=%ux%u target=%ux%u\r\n",
           static_cast<unsigned>(source_width),
           static_cast<unsigned>(source_height),
           static_cast<unsigned>(target_width),
           static_cast<unsigned>(target_height));
    ResetFrameResources(false);
    return false;
  }
  CleanBufferForV3d(g_frame_tile_scratch);
  DataSyncBarrier();

  g_frame_width = source_width;
  g_frame_height = source_height;
  g_frame_target_width = target_width;
  g_frame_target_height = target_height;
  g_frame_source_bytes = source_layout.size_bytes;
  g_frame_intermediate_bytes = source_layout.size_bytes;
  g_frame_output_intermediate_bytes = output_layout.size_bytes;
  g_frame_target_bytes = target_bytes;
  g_frame_tile_state_bytes = tile_state_bytes;
  g_frame_source_layout = source_layout;
  g_frame_intermediate_layout = source_layout;
  g_frame_output_intermediate_layout = output_layout;
  g_frame_effect_valid = false;
  g_frame_multipass_active = false;
  g_frame_source_pass_active = false;
  g_frame_output_pass_active = false;
  g_frame_output_pass_edge_glow = false;
  g_frame_output_pass_mode = kFrameOutputPassOff;
  g_frame_effect_output_log_pending = false;
  g_frame_last_effect_change = false;
  g_frame_scanline_weight = 0.0f;
  g_frame_scanline_gap_brightness = 1.0f;
  g_frame_scanline_multisample = false;
  g_frame_edge_blur_enabled = false;
  g_frame_edge_blur_strength = 0.0f;
  g_frame_edge_blur_radius = 0.2f;
  g_frame_phosphor_mask_enabled = false;
  g_frame_phosphor_mask_pattern = 1U;
  g_frame_phosphor_mask_brightness = 1.0f;
  g_frame_vignette_enabled = false;
  g_frame_vignette_strength = 0.0f;
  g_frame_vignette_scale = 1.0f;
  g_frame_vignette_softness = 0.02f;
  g_frame_uneven_illumination_enabled = false;
  g_frame_uneven_illumination_strength = 0.0f;
  g_frame_uneven_illumination_scale = 0.02f;
  g_frame_glass_reflection_enabled = false;
  g_frame_glass_reflection_angle = 0.0f;
  g_frame_glass_reflection_width = 0.02f;
  g_frame_glass_reflection_position = 0.0f;
  g_frame_rounded_screen_mask_enabled = false;
  g_frame_rounded_corner_radius = 0.0f;
  g_frame_rounded_border_softness = 0.0f;
  g_frame_edge_glow_enabled = false;
  g_frame_edge_glow_strength = 0.0f;
  g_frame_edge_glow_width = 0.01f;
  g_frame_output_response_enabled = false;
  g_frame_output_response_fast = false;
  g_frame_output_level_mapping = 1U;
  g_frame_input_gamma = 1.0f;
  g_frame_output_gamma = 1.0f;
  g_frame_output_saturation = 1.0f;
  g_frame_black_level = 0.0f;
  g_frame_white_clip = 1.0f;
  g_frame_convergence_enabled = false;
  g_frame_red_offset_x = 0.0f;
  g_frame_red_offset_y = 0.0f;
  g_frame_blue_offset_x = 0.0f;
  g_frame_blue_offset_y = 0.0f;
  g_frame_convergence_radial_strength = 0.0f;
  g_frame_horizontal_filtering_enabled = false;
  g_frame_horizontal_sigma_x = 0.0f;
  g_frame_bloom_enabled = false;
  g_frame_bloom_factor = 0.0f;
  g_frame_horizontal_jitter_enabled = false;
  g_frame_horizontal_jitter_strength = 0.0f;
  g_frame_horizontal_jitter_frequency = 0.01f;
  g_frame_horizontal_jitter_speed = 0.0f;
  g_frame_composite_artifacts_enabled = false;
  g_frame_composite_chroma_blur = 0.0f;
  g_frame_composite_luma_sharpen = 0.0f;
  g_frame_composite_color_bleed = 0.0f;
  g_frame_noise_enabled = false;
  g_frame_luminance_noise = 0.0f;
  g_frame_chroma_noise = 0.0f;
  g_frame_noise_speed = 0.0f;
  g_frame_temporal_package_active = false;
  g_frame_output_response_lut_valid = false;
  g_frame_output_response_palette_valid = false;
  g_frame_output_response_lut_build_us = 0U;
  g_frame_source_upload_us = 0U;
  g_frame_source_linear_filter = false;
  v3dcrt::ResetEdgeGlowTemporalFilter(&g_edge_glow_temporal_filter);
  printf("boot: pi4v3d frame resources status=ready slots=%u "
         "source=%ux%u target=%ux%u source_bytes=%u "
         "intermediate_bytes=%u target_bytes=%u "
         "tiles=%u "
         "tile_alloc_bytes=%u tile_state_bytes=%u source_layout=%s "
         "padded=%ux%u array_stride_64=%u ub_pad=%u\r\n",
         static_cast<unsigned>(kRenderJobSlotCount),
         static_cast<unsigned>(source_width),
         static_cast<unsigned>(source_height),
         static_cast<unsigned>(target_width),
         static_cast<unsigned>(target_height),
         static_cast<unsigned>(source_layout.size_bytes),
         static_cast<unsigned>(source_layout.size_bytes),
         static_cast<unsigned>(target_bytes),
         static_cast<unsigned>(
             RenderTileCount(target_width, target_height)),
         static_cast<unsigned>(kRenderTileAllocationBytes),
         static_cast<unsigned>(tile_state_bytes),
         v3d42::Rgba8TextureTilingName(source_layout.tiling),
         static_cast<unsigned>(source_layout.padded_width),
         static_cast<unsigned>(source_layout.padded_height),
         static_cast<unsigned>(source_layout.array_stride_64_bytes),
         static_cast<unsigned>(source_layout.level_0_ub_pad));
  return true;
}

bool ApplyFrameParams(const FrameParams &params) {
  const FramePresetPolicy preset_policy =
      ResolveFramePresetPolicy(g_shader_preset);
  const bool force_scanlines = preset_policy.force_scanlines;
  const bool scanlines_enabled =
      preset_policy.menu_effects &&
      (force_scanlines || params.enable_scanlines);
  float menu_weight = params.scanline_weight;
  float gap_brightness = params.scanline_gap_brightness;
  if (force_scanlines && menu_weight <= 0.0f) {
    menu_weight = 3.0f;
  }
  if (force_scanlines && gap_brightness >= 1.0f) {
    gap_brightness = 0.5f;
  }
  const RenderScanlineParams scanlines = ResolveRenderScanlineParams(
      scanlines_enabled, menu_weight, gap_brightness);
  const bool scanline_multisample =
      scanlines_enabled && params.enable_scanline_multisample;
  const bool scaled_output =
      g_frame_width != g_frame_target_width ||
      g_frame_height != g_frame_target_height;
  const RenderGeometryParams effect_geometry =
      ResolveRenderGeometryParams(
          scaled_output && preset_policy.menu_effects &&
              params.enable_geometry,
          params.curvature_x, params.curvature_y,
          params.skew_x, params.skew_y, params.trapezoid,
          params.rotation_degrees, params.overscan_scale);
  const RenderEdgeBlurParams edge_blur = ResolveRenderEdgeBlurParams(
      scaled_output && preset_policy.menu_effects && params.enable_edge_blur,
      params.edge_blur_strength, params.edge_blur_radius);
  const RenderPhosphorMaskParams phosphor_mask =
      ResolveRenderPhosphorMaskParams(
          scaled_output && preset_policy.menu_effects &&
              params.enable_mask,
          params.phosphor_mask_pattern, params.mask_brightness);
  const RenderVignetteParams vignette = ResolveRenderVignetteParams(
      scaled_output && preset_policy.menu_effects && params.enable_vignette,
      params.vignette_strength, params.vignette_scale,
      params.vignette_softness);
  const RenderUnevenIlluminationParams uneven_illumination =
      ResolveRenderUnevenIlluminationParams(
          scaled_output && preset_policy.menu_effects &&
              params.enable_uneven_illumination,
          params.uneven_illumination_strength,
          params.uneven_illumination_scale);
  const RenderGlassReflectionParams glass_reflection =
      ResolveRenderGlassReflectionParams(
          scaled_output && preset_policy.menu_effects &&
              params.enable_glass_reflection,
          params.glass_reflection_angle,
          params.glass_reflection_width,
          params.glass_reflection_position);
  const RenderRoundedScreenMaskParams rounded_screen_mask =
      ResolveRenderRoundedScreenMaskParams(
          scaled_output && preset_policy.menu_effects &&
              params.enable_rounded_screen_mask,
          params.rounded_corner_radius,
          params.rounded_border_softness);
  const RenderEdgeGlowParams edge_glow = ResolveRenderEdgeGlowParams(
      scaled_output && preset_policy.menu_effects && params.enable_edge_glow,
      params.edge_glow_strength, params.edge_glow_width);
  const RenderOutputResponseParams output_response =
      ResolveRenderOutputResponseParams(
          scaled_output && preset_policy.menu_effects &&
              params.enable_output_response,
          params.fast_output_response, params.output_level_mapping,
          params.input_gamma, params.output_gamma,
          params.output_saturation, params.black_level, params.white_clip);
  const bool full_crt_effects =
      scaled_output && preset_policy.menu_effects;
  const RenderConvergenceParams convergence = ResolveRenderConvergenceParams(
      full_crt_effects && params.enable_convergence,
      params.red_offset_x, params.red_offset_y,
      params.blue_offset_x, params.blue_offset_y,
      params.convergence_radial_strength);
  const RenderHorizontalFilteringParams horizontal_filtering =
      ResolveRenderHorizontalFilteringParams(
          full_crt_effects && params.enable_horizontal_filtering,
          params.horizontal_sigma_x);
  const RenderBloomParams bloom = ResolveRenderBloomParams(
      full_crt_effects && params.enable_bloom, params.bloom_factor);
  const RenderHorizontalJitterParams horizontal_jitter =
      ResolveRenderHorizontalJitterParams(
          full_crt_effects && params.enable_horizontal_jitter,
          params.horizontal_jitter_strength,
          params.horizontal_jitter_frequency,
          params.horizontal_jitter_speed);
  const RenderCompositeArtifactsParams composite_artifacts =
      ResolveRenderCompositeArtifactsParams(
          full_crt_effects && params.enable_composite_artifacts,
          params.composite_chroma_blur, params.composite_luma_sharpen,
          params.composite_color_bleed);
  const RenderNoiseParams noise = ResolveRenderNoiseParams(
      full_crt_effects && params.enable_noise,
      params.luminance_noise, params.chroma_noise, params.noise_speed);
  const RenderOutputResponseParams gpu_output_response =
      ResolveRenderOutputResponseParams(
          false, false, 1U, 1.0f, 1.0f, 1.0f, 0.0f, 1.0f);
  const bool linear_filter =
      ResolveRenderSourceLinearFilter(params.enable_interpolation);
  const bool output_response_changed =
      !g_frame_effect_valid ||
      g_frame_output_response_enabled != output_response.enabled ||
      g_frame_output_response_fast != output_response.fast ||
      g_frame_output_level_mapping != output_response.level_mapping ||
      g_frame_input_gamma != output_response.input_gamma ||
      g_frame_output_gamma != output_response.output_gamma ||
      g_frame_output_saturation != output_response.saturation ||
      g_frame_black_level != output_response.black_level ||
      g_frame_white_clip != output_response.white_clip;
  if (g_frame_effect_valid &&
      g_frame_geometry_enabled == effect_geometry.enabled &&
      g_frame_curvature_x == effect_geometry.curvature_x &&
      g_frame_curvature_y == effect_geometry.curvature_y &&
      g_frame_skew_x == effect_geometry.skew_x &&
      g_frame_skew_y == effect_geometry.skew_y &&
      g_frame_trapezoid == effect_geometry.trapezoid &&
      g_frame_rotation_degrees == effect_geometry.rotation_degrees &&
      g_frame_overscan_scale == effect_geometry.overscan_scale &&
      g_frame_scanline_weight == scanlines.weight &&
      g_frame_scanline_gap_brightness == scanlines.gap_brightness &&
      g_frame_scanline_multisample == scanline_multisample &&
      g_frame_edge_blur_enabled == edge_blur.enabled &&
      g_frame_edge_blur_strength == edge_blur.strength &&
      g_frame_edge_blur_radius == edge_blur.radius &&
      g_frame_phosphor_mask_enabled == phosphor_mask.enabled &&
      g_frame_phosphor_mask_pattern == phosphor_mask.pattern &&
      g_frame_phosphor_mask_brightness == phosphor_mask.brightness &&
      g_frame_vignette_enabled == vignette.enabled &&
      g_frame_vignette_strength == vignette.strength &&
      g_frame_vignette_scale == vignette.scale &&
      g_frame_vignette_softness == vignette.softness &&
      g_frame_uneven_illumination_enabled == uneven_illumination.enabled &&
      g_frame_uneven_illumination_strength == uneven_illumination.strength &&
      g_frame_uneven_illumination_scale == uneven_illumination.scale &&
      g_frame_glass_reflection_enabled == glass_reflection.enabled &&
      g_frame_glass_reflection_angle == glass_reflection.angle &&
      g_frame_glass_reflection_width == glass_reflection.width &&
      g_frame_glass_reflection_position == glass_reflection.position &&
      g_frame_rounded_screen_mask_enabled == rounded_screen_mask.enabled &&
      g_frame_rounded_corner_radius == rounded_screen_mask.corner_radius &&
      g_frame_rounded_border_softness == rounded_screen_mask.border_softness &&
      g_frame_edge_glow_enabled == edge_glow.enabled &&
      g_frame_edge_glow_strength == edge_glow.strength &&
      g_frame_edge_glow_width == edge_glow.width &&
      g_frame_output_response_enabled == output_response.enabled &&
      g_frame_output_response_fast == output_response.fast &&
      g_frame_output_level_mapping == output_response.level_mapping &&
      g_frame_input_gamma == output_response.input_gamma &&
      g_frame_output_gamma == output_response.output_gamma &&
      g_frame_output_saturation == output_response.saturation &&
      g_frame_black_level == output_response.black_level &&
      g_frame_white_clip == output_response.white_clip &&
      g_frame_convergence_enabled == convergence.enabled &&
      g_frame_red_offset_x == convergence.red_offset_x &&
      g_frame_red_offset_y == convergence.red_offset_y &&
      g_frame_blue_offset_x == convergence.blue_offset_x &&
      g_frame_blue_offset_y == convergence.blue_offset_y &&
      g_frame_convergence_radial_strength == convergence.radial_strength &&
      g_frame_horizontal_filtering_enabled == horizontal_filtering.enabled &&
      g_frame_horizontal_sigma_x == horizontal_filtering.sigma_x &&
      g_frame_bloom_enabled == bloom.enabled &&
      g_frame_bloom_factor == bloom.factor &&
      g_frame_horizontal_jitter_enabled == horizontal_jitter.enabled &&
      g_frame_horizontal_jitter_strength == horizontal_jitter.strength &&
      g_frame_horizontal_jitter_frequency == horizontal_jitter.frequency &&
      g_frame_horizontal_jitter_speed == horizontal_jitter.speed &&
      g_frame_composite_artifacts_enabled == composite_artifacts.enabled &&
      g_frame_composite_chroma_blur == composite_artifacts.chroma_blur &&
      g_frame_composite_luma_sharpen == composite_artifacts.luma_sharpen &&
      g_frame_composite_color_bleed == composite_artifacts.color_bleed &&
      g_frame_noise_enabled == noise.enabled &&
      g_frame_luminance_noise == noise.luminance &&
      g_frame_chroma_noise == noise.chroma &&
      g_frame_noise_speed == noise.speed &&
      g_frame_source_linear_filter == linear_filter) {
    return true;
  }
  if (!PrepareFrameRenderJobs(g_frame_width, g_frame_height,
                              g_frame_target_width,
                              g_frame_target_height, effect_geometry,
                              scanlines,
                              scanline_multisample,
                              edge_blur,
                              phosphor_mask,
                              vignette,
                              uneven_illumination,
                              glass_reflection,
                              rounded_screen_mask,
                              edge_glow,
                              gpu_output_response,
                              convergence,
                              horizontal_filtering,
                              bloom,
                              horizontal_jitter,
                              composite_artifacts,
                              noise,
                              linear_filter)) {
    return false;
  }

  g_frame_effect_valid = true;
  g_frame_geometry_enabled = effect_geometry.enabled;
  g_frame_curvature_x = effect_geometry.curvature_x;
  g_frame_curvature_y = effect_geometry.curvature_y;
  g_frame_skew_x = effect_geometry.skew_x;
  g_frame_skew_y = effect_geometry.skew_y;
  g_frame_trapezoid = effect_geometry.trapezoid;
  g_frame_rotation_degrees = effect_geometry.rotation_degrees;
  g_frame_overscan_scale = effect_geometry.overscan_scale;
  g_frame_scanline_weight = scanlines.weight;
  g_frame_scanline_gap_brightness = scanlines.gap_brightness;
  g_frame_scanline_multisample = scanline_multisample;
  g_frame_edge_blur_enabled = edge_blur.enabled;
  g_frame_edge_blur_strength = edge_blur.strength;
  g_frame_edge_blur_radius = edge_blur.radius;
  g_frame_phosphor_mask_enabled = phosphor_mask.enabled;
  g_frame_phosphor_mask_pattern = phosphor_mask.pattern;
  g_frame_phosphor_mask_brightness = phosphor_mask.brightness;
  g_frame_vignette_enabled = vignette.enabled;
  g_frame_vignette_strength = vignette.strength;
  g_frame_vignette_scale = vignette.scale;
  g_frame_vignette_softness = vignette.softness;
  g_frame_uneven_illumination_enabled = uneven_illumination.enabled;
  g_frame_uneven_illumination_strength = uneven_illumination.strength;
  g_frame_uneven_illumination_scale = uneven_illumination.scale;
  g_frame_glass_reflection_enabled = glass_reflection.enabled;
  g_frame_glass_reflection_angle = glass_reflection.angle;
  g_frame_glass_reflection_width = glass_reflection.width;
  g_frame_glass_reflection_position = glass_reflection.position;
  g_frame_rounded_screen_mask_enabled = rounded_screen_mask.enabled;
  g_frame_rounded_corner_radius = rounded_screen_mask.corner_radius;
  g_frame_rounded_border_softness = rounded_screen_mask.border_softness;
  if (g_frame_edge_glow_enabled != edge_glow.enabled) {
    v3dcrt::ResetEdgeGlowTemporalFilter(&g_edge_glow_temporal_filter);
  }
  g_frame_edge_glow_enabled = edge_glow.enabled;
  g_frame_edge_glow_strength = edge_glow.strength;
  g_frame_edge_glow_width = edge_glow.width;
  if (output_response_changed) {
    g_frame_output_response_lut_valid = false;
    g_frame_output_response_palette_valid = false;
  }
  g_frame_output_response_enabled = output_response.enabled;
  g_frame_output_response_fast = output_response.fast;
  g_frame_output_level_mapping = output_response.level_mapping;
  g_frame_input_gamma = output_response.input_gamma;
  g_frame_output_gamma = output_response.output_gamma;
  g_frame_output_saturation = output_response.saturation;
  g_frame_black_level = output_response.black_level;
  g_frame_white_clip = output_response.white_clip;
  g_frame_convergence_enabled = convergence.enabled;
  g_frame_red_offset_x = convergence.red_offset_x;
  g_frame_red_offset_y = convergence.red_offset_y;
  g_frame_blue_offset_x = convergence.blue_offset_x;
  g_frame_blue_offset_y = convergence.blue_offset_y;
  g_frame_convergence_radial_strength = convergence.radial_strength;
  g_frame_horizontal_filtering_enabled = horizontal_filtering.enabled;
  g_frame_horizontal_sigma_x = horizontal_filtering.sigma_x;
  g_frame_bloom_enabled = bloom.enabled;
  g_frame_bloom_factor = bloom.factor;
  g_frame_horizontal_jitter_enabled = horizontal_jitter.enabled;
  g_frame_horizontal_jitter_strength = horizontal_jitter.strength;
  g_frame_horizontal_jitter_frequency = horizontal_jitter.frequency;
  g_frame_horizontal_jitter_speed = horizontal_jitter.speed;
  g_frame_composite_artifacts_enabled = composite_artifacts.enabled;
  g_frame_composite_chroma_blur = composite_artifacts.chroma_blur;
  g_frame_composite_luma_sharpen = composite_artifacts.luma_sharpen;
  g_frame_composite_color_bleed = composite_artifacts.color_bleed;
  g_frame_noise_enabled = noise.enabled;
  g_frame_luminance_noise = noise.luminance;
  g_frame_chroma_noise = noise.chroma;
  g_frame_noise_speed = noise.speed;
  g_frame_temporal_package_active =
      ((noise.enabled && noise.speed > 0.0f) ||
       (horizontal_jitter.enabled && horizontal_jitter.speed > 0.0f)) &&
      g_frame_source_pass_active;
  g_frame_output_response_lut_build_us = 0U;
  g_frame_source_linear_filter = linear_filter;
  g_frame_effect_output_log_pending = true;
  const bool standalone_uneven =
      uneven_illumination.enabled && scaled_output &&
      !effect_geometry.enabled &&
      !edge_blur.enabled && !phosphor_mask.enabled && !vignette.enabled;
  const bool standalone_glass =
      glass_reflection.enabled && scaled_output &&
      !effect_geometry.enabled &&
      !edge_blur.enabled && !phosphor_mask.enabled && !vignette.enabled &&
      !uneven_illumination.enabled;
  const bool standalone_edge_glow =
      edge_glow.enabled && scaled_output &&
      !effect_geometry.enabled &&
      !edge_blur.enabled && !phosphor_mask.enabled && !vignette.enabled &&
      !uneven_illumination.enabled && !glass_reflection.enabled &&
      !rounded_screen_mask.enabled;
  const unsigned post_effect_count =
      (vignette.enabled ? 1U : 0U) +
      (uneven_illumination.enabled ? 1U : 0U) +
      (glass_reflection.enabled ? 1U : 0U) +
      (rounded_screen_mask.enabled ? 1U : 0U) +
      (edge_glow.enabled ? 1U : 0U);
  const bool late_effects_enabled =
      uneven_illumination.enabled || glass_reflection.enabled ||
      rounded_screen_mask.enabled || edge_glow.enabled;
  const bool post_effects_enabled =
      vignette.enabled || late_effects_enabled;
  const unsigned early_effect_count =
      (effect_geometry.enabled ? 1U : 0U) +
      (edge_blur.enabled ? 1U : 0U) +
      (phosphor_mask.enabled ? 1U : 0U);
  const bool compact_early_post_effects =
      early_effect_count == 1U && post_effect_count == 1U &&
      scanlines.weight <= 0.0f &&
      ((edge_blur.enabled && post_effects_enabled) ||
       (effect_geometry.enabled && late_effects_enabled) ||
       (phosphor_mask.enabled &&
        (uneven_illumination.enabled || glass_reflection.enabled ||
         edge_glow.enabled)));
  const bool compact_post_effects =
      scaled_output && !bloom.enabled &&
      (compact_early_post_effects ||
       (post_effect_count >= 2U && !effect_geometry.enabled &&
        !edge_blur.enabled && !phosphor_mask.enabled));
  const bool compact_standalone_post_effect =
      scaled_output && !bloom.enabled && early_effect_count == 0U &&
      post_effect_count == 1U &&
      (vignette.enabled || rounded_screen_mask.enabled);
  const bool compact_specialized_post_effect =
      scaled_output && !bloom.enabled && scanlines.weight <= 0.0f &&
      early_effect_count == 1U && post_effect_count == 1U &&
      (effect_geometry.enabled || edge_blur.enabled ||
       phosphor_mask.enabled) &&
      (uneven_illumination.enabled || glass_reflection.enabled ||
       edge_glow.enabled);
  const char *source_package = !g_frame_source_pass_active ? "off" :
      (noise.enabled || horizontal_jitter.enabled) ?
          "crt_source_noise" :
      composite_artifacts.enabled ? "crt_source_composite" :
      convergence.enabled ? "crt_source_convergence" :
      "crt_source_filter";
  const char *automatic_package =
      gpu_output_response.enabled && scaled_output ?
          (gpu_output_response.fast &&
                  gpu_output_response.level_mapping == 1U ?
              "crt_output_response_fast_cubic" :
              "crt_output_response") :
      compact_specialized_post_effect ?
          (edge_glow.enabled ? "crt_output_edge_glow_pi4" :
           glass_reflection.enabled ?
              "crt_output_glass_reflection_pi4" :
              "crt_output_uneven_illumination_pi4") :
      (compact_post_effects || compact_standalone_post_effect) ?
          (edge_blur.enabled ? "crt_output_edge_blur_post_pi4" :
           early_effect_count != 0U ? "crt_output_early_post_pi4" :
              "crt_output_late_effects_pi4") :
      edge_glow.enabled && scaled_output ?
          (standalone_edge_glow ? "crt_output_edge_glow_pi4" :
              "crt_output_edge_glow") :
      rounded_screen_mask.enabled && scaled_output ?
          "crt_output_rounded_screen_mask" :
      glass_reflection.enabled && scaled_output ?
          (standalone_glass ? "crt_output_glass_reflection_pi4" :
              "crt_output_glass_reflection") :
      uneven_illumination.enabled && scaled_output ?
          (standalone_uneven ? "crt_output_uneven_illumination_pi4" :
              "crt_output_uneven_illumination") :
      vignette.enabled && scaled_output ?
          "crt_output_vignette" :
      phosphor_mask.enabled && scaled_output ?
          "crt_output_phosphor_mask" :
      edge_blur.enabled && scaled_output ?
          "crt_output_edge_blur" :
      bloom.enabled && scaled_output ?
          "crt_output_bloom_pi4" :
      effect_geometry.enabled && scanlines.weight <= 0.0f && scaled_output ?
          "crt_output_geometry" :
      (!effect_geometry.enabled && scaled_output ?
          "crt_output_scanlines_pi4" : "crt_output_scanlines");
  const char *package =
      g_frame_output_pass_mode == kFrameOutputPassBloom ?
          "crt_output_bloom_pi4" : automatic_package;
  const char *output_prepass_package =
      g_frame_output_pass_mode == kFrameOutputPassBloom ?
          "non-bloom-output" :
      g_frame_output_pass_mode == kFrameOutputPassBloomSource ?
          "crt_output_bloom_pi4" : "off";
  printf("boot: pi4v3d frame effect preset=%s geometry=%u "
         "curvature_x10000=%u,%u skew_x10000=%d,%d "
         "trapezoid_x10000=%d rotation_x100=%d overscan_x100=%u "
         "scanlines=%u multisample=%u "
         "weight_x100=%u gap_x100=%u edge_blur=%u "
         "edge_strength_x100=%u edge_radius_x100=%u phosphor_mask=%u "
         "mask_pattern=%u mask_brightness_x100=%u vignette=%u "
         "vignette_strength_x100=%u vignette_scale_x100=%u "
         "vignette_softness_x100=%u uneven_illumination=%u "
         "uneven_strength_x100=%u uneven_scale_x100=%u "
         "glass_reflection=%u glass_angle_x100=%d glass_width_x100=%u "
         "glass_position_x100=%u rounded_screen_mask=%u "
         "rounded_radius_x100=%u rounded_softness_x100=%u "
         "edge_glow=%u edge_glow_strength_x100=%u "
         "edge_glow_width_x100=%u "
         "convergence=%u red_offset_x100=%d,%d blue_offset_x100=%d,%d "
         "convergence_radial_x100=%u horizontal_filtering=%u "
         "sigma_x100=%u bloom=%u bloom_factor_x100=%u "
         "horizontal_jitter=%u jitter_strength_x100=%u "
         "jitter_frequency_x100=%u jitter_speed_x100=%u "
         "composite=%u chroma_blur_x100=%u luma_sharpen_x100=%u "
         "color_bleed_x100=%u noise=%u luminance_noise_x100=%u "
         "chroma_noise_x100=%u noise_speed_x100=%u "
         "output_response=%u response_fast=%u level_mapping=%u "
         "input_gamma_x100=%u output_gamma_x100=%u saturation_x100=%u "
         "black_level_x100=%u white_clip_x100=%u "
         "response_path=%s multipass=%u source_multipass=%u "
         "output_multipass=%u source_package=%s "
         "output_prepass_package=%s "
         "output_package=%s package=%s sampler=%s "
         "controls_rebuilt=1\r\n",
         g_shader_preset,
         effect_geometry.enabled ? 1U : 0U,
         static_cast<unsigned>(effect_geometry.curvature_x * 10000.0f + 0.5f),
         static_cast<unsigned>(effect_geometry.curvature_y * 10000.0f + 0.5f),
         static_cast<int>(effect_geometry.skew_x * 10000.0f),
         static_cast<int>(effect_geometry.skew_y * 10000.0f),
         static_cast<int>(effect_geometry.trapezoid * 10000.0f),
         static_cast<int>(effect_geometry.rotation_degrees * 100.0f),
         static_cast<unsigned>(effect_geometry.overscan_scale * 100.0f + 0.5f),
         scanlines_enabled ? 1U : 0U,
         scanline_multisample ? 1U : 0U,
         static_cast<unsigned>(scanlines.weight * 100.0f + 0.5f),
         static_cast<unsigned>(scanlines.gap_brightness * 100.0f + 0.5f),
         edge_blur.enabled ? 1U : 0U,
         static_cast<unsigned>(edge_blur.strength * 100.0f + 0.5f),
         static_cast<unsigned>(edge_blur.radius * 100.0f + 0.5f),
         phosphor_mask.enabled ? 1U : 0U,
         static_cast<unsigned>(phosphor_mask.pattern),
         static_cast<unsigned>(phosphor_mask.brightness * 100.0f + 0.5f),
         vignette.enabled ? 1U : 0U,
         static_cast<unsigned>(vignette.strength * 100.0f + 0.5f),
         static_cast<unsigned>(vignette.scale * 100.0f + 0.5f),
         static_cast<unsigned>(vignette.softness * 100.0f + 0.5f),
         uneven_illumination.enabled ? 1U : 0U,
         static_cast<unsigned>(
             uneven_illumination.strength * 100.0f + 0.5f),
         static_cast<unsigned>(uneven_illumination.scale * 100.0f + 0.5f),
         glass_reflection.enabled ? 1U : 0U,
         static_cast<int>(glass_reflection.angle * 100.0f),
         static_cast<unsigned>(glass_reflection.width * 100.0f + 0.5f),
         static_cast<unsigned>(glass_reflection.position * 100.0f + 0.5f),
         rounded_screen_mask.enabled ? 1U : 0U,
         static_cast<unsigned>(
             rounded_screen_mask.corner_radius * 100.0f + 0.5f),
         static_cast<unsigned>(
             rounded_screen_mask.border_softness * 100.0f + 0.5f),
         edge_glow.enabled ? 1U : 0U,
         static_cast<unsigned>(edge_glow.strength * 100.0f + 0.5f),
         static_cast<unsigned>(edge_glow.width * 100.0f + 0.5f),
         convergence.enabled ? 1U : 0U,
         static_cast<int>(convergence.red_offset_x * 100.0f),
         static_cast<int>(convergence.red_offset_y * 100.0f),
         static_cast<int>(convergence.blue_offset_x * 100.0f),
         static_cast<int>(convergence.blue_offset_y * 100.0f),
         static_cast<unsigned>(convergence.radial_strength * 100.0f + 0.5f),
         horizontal_filtering.enabled ? 1U : 0U,
         static_cast<unsigned>(horizontal_filtering.sigma_x * 100.0f + 0.5f),
         bloom.enabled ? 1U : 0U,
         static_cast<unsigned>(bloom.factor * 100.0f + 0.5f),
         horizontal_jitter.enabled ? 1U : 0U,
         static_cast<unsigned>(horizontal_jitter.strength * 100.0f + 0.5f),
         static_cast<unsigned>(horizontal_jitter.frequency * 100.0f + 0.5f),
         static_cast<unsigned>(horizontal_jitter.speed * 100.0f + 0.5f),
         composite_artifacts.enabled ? 1U : 0U,
         static_cast<unsigned>(composite_artifacts.chroma_blur * 100.0f + 0.5f),
         static_cast<unsigned>(composite_artifacts.luma_sharpen * 100.0f + 0.5f),
         static_cast<unsigned>(composite_artifacts.color_bleed * 100.0f + 0.5f),
         noise.enabled ? 1U : 0U,
         static_cast<unsigned>(noise.luminance * 100.0f + 0.5f),
         static_cast<unsigned>(noise.chroma * 100.0f + 0.5f),
         static_cast<unsigned>(noise.speed * 100.0f + 0.5f),
         output_response.enabled ? 1U : 0U,
         output_response.fast ? 1U : 0U,
         static_cast<unsigned>(output_response.level_mapping),
         static_cast<unsigned>(output_response.input_gamma * 100.0f + 0.5f),
         static_cast<unsigned>(output_response.output_gamma * 100.0f + 0.5f),
         static_cast<unsigned>(output_response.saturation * 100.0f + 0.5f),
         static_cast<unsigned>(output_response.black_level * 100.0f + 0.5f),
         static_cast<unsigned>(output_response.white_clip * 100.0f + 0.5f),
         output_response.enabled ? "source-rgb565" : "off",
         g_frame_multipass_active ? 1U : 0U,
         g_frame_source_pass_active ? 1U : 0U,
         g_frame_output_pass_active ? 1U : 0U,
         source_package,
         output_prepass_package,
         package,
         package,
         linear_filter ? "linear" : "nearest");
  return true;
}

u32 Rgba8WordFromRgb565(u16 rgb565) {
  u32 red = (rgb565 >> 11U) & 0x1fU;
  u32 green = (rgb565 >> 5U) & 0x3fU;
  u32 blue = rgb565 & 0x1fU;
  red = (red << 3U) | (red >> 2U);
  green = (green << 2U) | (green >> 4U);
  blue = (blue << 3U) | (blue >> 2U);
  return 0xff000000U | (blue << 16U) | (green << 8U) | red;
}

bool SampleFrameTexture(const Buffer &texture,
                        const v3d42::Rgba8TextureLayout &layout,
                        float u, float v, bool linear_filter,
                        v3dcrt::EdgeGlowFieldColor *sample) {
  if (sample == nullptr) {
    return false;
  }
  v3d42::RgbFloat color = {};
  if (!v3d42::SampleRgba8Texture(
          texture.cpu, texture.size, layout, u, v,
          linear_filter, true, &color)) {
    return false;
  }
  sample->red = color.red;
  sample->green = color.green;
  sample->blue = color.blue;
  return true;
}

float EdgeGlowSmoothstep(float value) {
  float normalized = (value - 0.20f) / 0.65f;
  if (normalized < 0.0f) {
    normalized = 0.0f;
  } else if (normalized > 1.0f) {
    normalized = 1.0f;
  }
  return normalized * normalized * (3.0f - 2.0f * normalized);
}

bool BuildEdgeGlowFrameColors(const Buffer &sample_texture,
                              const v3d42::Rgba8TextureLayout &sample_layout,
                              const char *sample_source,
                              RenderEdgeGlowFrameColors *colors,
                              bool log_samples) {
  if (colors == nullptr || sample_source == nullptr) {
    return false;
  }
  memset(colors, 0, sizeof *colors);
  if (!g_frame_edge_glow_enabled) {
    v3dcrt::ResetEdgeGlowTemporalFilter(&g_edge_glow_temporal_filter);
    return true;
  }

  const float tangent_coordinates[kEdgeGlowRegionKernelSize] = {
    0.25f, 0.50f, 0.75f,
  };
  const float normal_coordinates[kEdgeGlowRegionKernelSize] = {
    kEdgeGlowEdgeInset - kEdgeGlowEdgeNormalOffset,
    kEdgeGlowEdgeInset,
    kEdgeGlowEdgeInset + kEdgeGlowEdgeNormalOffset,
  };
  const u32 region_weights[kEdgeGlowRegionKernelSize] = {1U, 2U, 1U};
  const float region_weight_scale = 1.0f / 16.0f;
  v3dcrt::EdgeGlowFieldColor current[v3dcrt::kEdgeGlowFieldSampleCount] = {};
  for (u32 edge = 0U; edge < v3dcrt::kEdgeGlowFieldSampleCount; ++edge) {
    for (u32 y = 0U; y < kEdgeGlowRegionKernelSize; ++y) {
      for (u32 x = 0U; x < kEdgeGlowRegionKernelSize; ++x) {
        float sample_u = 0.0f;
        float sample_v = 0.0f;
        switch (edge) {
        case v3dcrt::kEdgeGlowFieldTop:
          sample_u = tangent_coordinates[x];
          sample_v = normal_coordinates[y];
          break;
        case v3dcrt::kEdgeGlowFieldBottom:
          sample_u = tangent_coordinates[x];
          sample_v = 1.0f - normal_coordinates[y];
          break;
        case v3dcrt::kEdgeGlowFieldLeft:
          sample_u = normal_coordinates[x];
          sample_v = tangent_coordinates[y];
          break;
        case v3dcrt::kEdgeGlowFieldRight:
          sample_u = 1.0f - normal_coordinates[x];
          sample_v = tangent_coordinates[y];
          break;
        default:
          return false;
        }
        v3dcrt::EdgeGlowFieldColor color = {};
        if (!SampleFrameTexture(
                sample_texture, sample_layout, sample_u, sample_v,
                g_frame_source_linear_filter, &color)) {
          v3dcrt::ResetEdgeGlowTemporalFilter(
              &g_edge_glow_temporal_filter);
          return false;
        }
        const float luma = color.red * 0.2126f + color.green * 0.7152f +
                           color.blue * 0.0722f;
        const float bright_weight =
            0.55f + EdgeGlowSmoothstep(luma) * 0.45f;
        const float kernel_weight =
            static_cast<float>(region_weights[x] * region_weights[y]);
        current[edge].red += color.red * bright_weight * kernel_weight;
        current[edge].green += color.green * bright_weight * kernel_weight;
        current[edge].blue += color.blue * bright_weight * kernel_weight;
      }
    }
    current[edge].red *= region_weight_scale;
    current[edge].green *= region_weight_scale;
    current[edge].blue *= region_weight_scale;
  }

  v3dcrt::EdgeGlowFieldColor filtered[v3dcrt::kEdgeGlowFieldSampleCount] = {};
  if (!v3dcrt::UpdateEdgeGlowTemporalFilter(
          current, CTimer::GetClockTicks64(),
          kEdgeGlowTemporalTimeConstantUs, kEdgeGlowTemporalResetGapUs,
          &g_edge_glow_temporal_filter, filtered)) {
    v3dcrt::ResetEdgeGlowTemporalFilter(&g_edge_glow_temporal_filter);
    return false;
  }
  for (u32 edge = 0U; edge < v3dcrt::kEdgeGlowFieldSampleCount; ++edge) {
    colors->color[edge][0] = filtered[edge].red;
    colors->color[edge][1] = filtered[edge].green;
    colors->color[edge][2] = filtered[edge].blue;
  }
  if (log_samples) {
    printf("boot: pi4v3d edge glow field source=frame-uniform "
           "sample_source=%s "
           "model=edge-local filter=%s regions=4 samples=36 "
           "inset_x1000=%u normal_radius_x1000=%u temporal_tau_ms=%u "
           "top_x1000=%d,%d,%d left_x1000=%d,%d,%d\r\n",
           sample_source,
           g_frame_source_linear_filter ? "linear" : "nearest",
           static_cast<unsigned>(kEdgeGlowEdgeInsetX1000),
           static_cast<unsigned>(kEdgeGlowEdgeNormalOffsetX1000),
           static_cast<unsigned>(kEdgeGlowTemporalTimeConstantUs / 1000ULL),
           static_cast<int>(colors->color[v3dcrt::kEdgeGlowFieldTop][0] *
                            1000.0f + 0.5f),
           static_cast<int>(colors->color[v3dcrt::kEdgeGlowFieldTop][1] *
                            1000.0f + 0.5f),
           static_cast<int>(colors->color[v3dcrt::kEdgeGlowFieldTop][2] *
                            1000.0f + 0.5f),
           static_cast<int>(colors->color[v3dcrt::kEdgeGlowFieldLeft][0] *
                            1000.0f + 0.5f),
           static_cast<int>(colors->color[v3dcrt::kEdgeGlowFieldLeft][1] *
                            1000.0f + 0.5f),
           static_cast<int>(colors->color[v3dcrt::kEdgeGlowFieldLeft][2] *
                            1000.0f + 0.5f));
  }
  return true;
}

bool UpdateFrameEdgeGlowUniforms(bool sample_source_pass) {
  if (!g_frame_edge_glow_enabled) {
    return true;
  }
  const u32 slot = g_frame_sequence & 1U;
  const Buffer &sample_texture = sample_source_pass ?
      g_frame_intermediate : g_frame_source;
  const v3d42::Rgba8TextureLayout &sample_layout = sample_source_pass ?
      g_frame_intermediate_layout : g_frame_source_layout;
  Buffer &control = g_frame_output_pass_edge_glow ?
      g_frame_output_pass_controls[slot] : g_frame_controls[slot];
  RenderJob &job = g_frame_output_pass_edge_glow ?
      g_frame_output_pass_jobs[slot] : g_frame_jobs[slot];
  RenderEdgeGlowFrameColors colors = {};
  if (slot >= kRenderJobSlotCount ||
      !BuildEdgeGlowFrameColors(
          sample_texture, sample_layout,
          sample_source_pass ? "source-pass" : "uploaded-frame",
          &colors, g_frame_effect_output_log_pending) ||
      !PatchRenderJobEdgeGlowFrameColors(
          colors, control.cpu, control.size, &job) ||
      !CleanRenderJobDynamicUniforms(control, job)) {
    return false;
  }
  DataSyncBarrier();
  return true;
}

bool UpdateFrameTemporalUniform() {
  if (!g_frame_temporal_package_active) {
    return true;
  }
  const u32 slot = g_frame_sequence & 1U;
  if (slot >= kRenderJobSlotCount ||
      !PatchRenderJobTemporalFrame(
          static_cast<float>(g_frame_sequence & 0x3ffU),
          g_frame_source_pass_controls[slot].cpu,
          g_frame_source_pass_controls[slot].size,
          &g_frame_source_pass_jobs[slot]) ||
      !CleanRenderJobDynamicUniforms(
          g_frame_source_pass_controls[slot],
          g_frame_source_pass_jobs[slot])) {
    return false;
  }
  DataSyncBarrier();
  return true;
}

bool UploadFrameSource(const FrameSource &source) {
  if (source.pixels == nullptr || g_frame_source.cpu == nullptr ||
      source.source_width != g_frame_width ||
      source.source_height != g_frame_height ||
      source.source_x > source.width ||
      source.source_width > source.width - source.source_x ||
      source.source_y > source.height ||
      source.source_height > source.height - source.source_y ||
      source.left_edge_padding >= source.source_width) {
    return false;
  }
  const u32 input_bytes =
      source.format == kFrameSourceRgb565 ? sizeof(u16) : sizeof(u8);
  if (source.width > UINT32_MAX / input_bytes ||
      source.pitch < source.width * input_bytes ||
      (source.format == kFrameSourceIndexed8 &&
       source.palette_rgb565 == nullptr)) {
    return false;
  }

  g_frame_output_response_lut_build_us = 0U;
  const u16 *active_palette = source.palette_rgb565;
  if (g_frame_output_response_enabled) {
    const RenderOutputResponseParams output_response = {
      true,
      g_frame_output_response_fast,
      g_frame_output_level_mapping,
      g_frame_input_gamma,
      g_frame_output_gamma,
      g_frame_output_saturation,
      g_frame_black_level,
      g_frame_white_clip
    };
    if (source.format == kFrameSourceIndexed8) {
      if (!g_frame_output_response_palette_valid ||
          memcmp(g_frame_output_response_palette_source,
                 source.palette_rgb565,
                 sizeof g_frame_output_response_palette_source) != 0) {
        const unsigned palette_start = CTimer::GetClockTicks();
        for (u32 index = 0U; index < 256U; ++index) {
          g_frame_output_response_palette[index] =
              ResolveRenderOutputResponseRgb565(
                  source.palette_rgb565[index], output_response);
        }
        memcpy(g_frame_output_response_palette_source,
               source.palette_rgb565,
               sizeof g_frame_output_response_palette_source);
        g_frame_output_response_palette_valid = true;
        g_frame_output_response_lut_build_us =
            CTimer::GetClockTicks() - palette_start;
      }
      active_palette = g_frame_output_response_palette;
    } else if (!g_frame_output_response_lut_valid) {
      const unsigned lut_start = CTimer::GetClockTicks();
      if (!BuildRenderOutputResponseRgb565Lut(
              output_response, g_frame_output_response_lut,
              sizeof g_frame_output_response_lut /
                  sizeof g_frame_output_response_lut[0])) {
        return false;
      }
      g_frame_output_response_lut_valid = true;
      g_frame_output_response_lut_build_us =
          CTimer::GetClockTicks() - lut_start;
    }
  }

  struct UploadContext {
    const FrameSource *source;
    const u16 *palette;
    const u16 *output_response_lut;
    u32 input_bytes;
  } context = {
    &source,
    active_palette,
    g_frame_output_response_enabled ? g_frame_output_response_lut : nullptr,
    input_bytes
  };
  const auto region_source = [](void *opaque, u32 origin_x, u32 origin_y,
                                u32 width, u32 height, u32 *rgba8,
                                u32 stride_words) -> bool {
    UploadContext *upload = static_cast<UploadContext *>(opaque);
    if (upload == nullptr || rgba8 == nullptr || stride_words < width) {
      return false;
    }
    for (u32 y = 0U; y < height; ++y) {
      const u8 *row = upload->source->pixels +
          (upload->source->source_y + origin_y + y) *
              upload->source->pitch +
          upload->source->source_x * upload->input_bytes;
      for (u32 x = 0U; x < width; ++x) {
        u32 source_x = 0U;
        if (!ResolveLeftEdgePaddedSourceCoordinate(
                origin_x + x, upload->source->source_width,
                upload->source->left_edge_padding, &source_x)) {
          return false;
        }
        u16 pixel = 0U;
        if (upload->source->format == kFrameSourceRgb565) {
          memcpy(&pixel, row + source_x * sizeof(u16), sizeof pixel);
          if (upload->output_response_lut != nullptr) {
            pixel = upload->output_response_lut[pixel];
          }
        } else {
          pixel = upload->palette[row[source_x]];
        }
        rgba8[y * stride_words + x] = Rgba8WordFromRgb565(pixel);
      }
    }
    return true;
  };

  const unsigned upload_start = CTimer::GetClockTicks();
  if (!v3d42::UploadRgba8TexturePhysical(
          g_frame_source.cpu, g_frame_source_bytes, g_frame_source_layout,
          region_source, &context)) {
    return false;
  }
  CleanBufferForV3d(g_frame_source);
  g_frame_source_upload_us = CTimer::GetClockTicks() - upload_start;
  return true;
}

bool SubmitPreparedFrameRender(Buffer &target_buffer, Buffer &control,
                               RenderJob &job, u32 target_bytes,
                               bool cpu_visible_target,
                               u32 *binner_us,
                               u32 *renderer_us) {
  if (binner_us == nullptr || renderer_us == nullptr ||
      target_buffer.cpu == nullptr || target_bytes == 0U ||
      target_bytes > target_buffer.size) {
    return false;
  }
  if (!RenderJobControlIntact(job, control.cpu, control.size)) {
    return false;
  }

  if (cpu_visible_target) {
    memset(target_buffer.cpu, 0, target_bytes);
    CleanBufferForV3d(target_buffer);
  }
  DataSyncBarrier();
  if (!StartRenderJob(&job)) {
    return false;
  }

  const bool binner_ok = SubmitBcl(
      job.lists, g_frame_tile_scratch, kRenderTileStateOffset,
      binner_us, false);
  const bool renderer_ok =
      binner_ok && SubmitRcl(job.lists, renderer_us, false);
  const bool cache_clean = renderer_ok && CleanV3dCaches();
  if (cache_clean && cpu_visible_target) {
    InvalidateBufferFromV3d(target_buffer);
    DataSyncBarrier();
  }
  const bool control_intact =
      RenderJobControlIntact(job, control.cpu, control.size);
  const bool succeeded = binner_ok && renderer_ok && cache_clean &&
                         control_intact;
  const bool lifecycle_finished = FinishRenderJob(&job, succeeded);
  return succeeded && lifecycle_finished;
}

struct FrameRenderTimings {
  u32 source_binner_us;
  u32 source_renderer_us;
  u32 prepass_binner_us;
  u32 prepass_renderer_us;
  u32 output_binner_us;
  u32 output_renderer_us;
};

bool SubmitFrameRenderSlot(u32 slot, FrameRenderTimings *timings) {
  if (slot >= kRenderJobSlotCount || timings == nullptr) {
    return false;
  }
  memset(timings, 0, sizeof *timings);
  if (g_frame_source_pass_active) {
    if (!SubmitPreparedFrameRender(
            g_frame_intermediate, g_frame_source_pass_controls[slot],
            g_frame_source_pass_jobs[slot], g_frame_intermediate_bytes,
            false,
            &timings->source_binner_us, &timings->source_renderer_us)) {
      return false;
    }
    if (g_frame_edge_glow_enabled) {
      InvalidateBufferFromV3d(g_frame_intermediate);
      DataSyncBarrier();
      if (!UpdateFrameEdgeGlowUniforms(true)) {
        return false;
      }
    }
  }
  if (g_frame_output_pass_active &&
      !SubmitPreparedFrameRender(
          g_frame_output_intermediate,
          g_frame_output_pass_controls[slot],
          g_frame_output_pass_jobs[slot],
          g_frame_output_intermediate_bytes, false,
          &timings->prepass_binner_us,
          &timings->prepass_renderer_us)) {
    return false;
  }
  return SubmitPreparedFrameRender(
      g_frame_targets[slot], g_frame_controls[slot], g_frame_jobs[slot],
      g_frame_target_bytes, true, &timings->output_binner_us,
      &timings->output_renderer_us);
}

u16 FrameSelfTestPixel(u32 x, u32 y) {
  const u32 red = (x * 3U + y * 5U + (x >> 4U)) & 0x1fU;
  const u32 green = (x * 7U + y * 11U + (y >> 3U)) & 0x3fU;
  const u32 blue = (x * 13U + y * 17U + (x ^ y)) & 0x1fU;
  return static_cast<u16>((red << 11U) | (green << 5U) | blue);
}

u16 SwapRgb565RedBlue(u16 pixel) {
  return static_cast<u16>(((pixel & 0x001fU) << 11U) |
                          (pixel & 0x07e0U) |
                          ((pixel & 0xf800U) >> 11U));
}

u32 Fnv1a32AppendU16(u32 hash, u16 value) {
  hash ^= value & 0xffU;
  hash *= 16777619U;
  hash ^= value >> 8U;
  hash *= 16777619U;
  return hash;
}

bool RunFrameSelfTest() {
  if (g_frame_self_test_passed) {
    return true;
  }
  if (g_frame_source.cpu == nullptr || g_frame_targets[0].cpu == nullptr ||
      g_frame_self_test_control.cpu == nullptr) {
    return false;
  }

  const auto region_source = [](void *, u32 origin_x, u32 origin_y,
                                u32 width, u32 height, u32 *rgba8,
                                u32 stride_words) -> bool {
    if (rgba8 == nullptr || stride_words < width) {
      return false;
    }
    for (u32 y = 0U; y < height; ++y) {
      for (u32 x = 0U; x < width; ++x) {
        rgba8[y * stride_words + x] = Rgba8WordFromRgb565(
            FrameSelfTestPixel(origin_x + x, origin_y + y));
      }
    }
    return true;
  };
  if (!v3d42::UploadRgba8TexturePhysical(
          g_frame_source.cpu, g_frame_source_bytes, g_frame_source_layout,
          region_source, nullptr)) {
    return false;
  }
  CleanBufferForV3d(g_frame_source);
  DataSyncBarrier();

  u32 binner_us = 0U;
  u32 renderer_us = 0U;
  if (!SubmitPreparedFrameRender(
          g_frame_targets[0], g_frame_self_test_control,
          g_frame_self_test_job, g_frame_target_bytes, true,
          &binner_us, &renderer_us)) {
    printf("boot: pi4v3d frame selftest status=fail phase=submit "
           "size=%ux%u\r\n",
           static_cast<unsigned>(g_frame_width),
           static_cast<unsigned>(g_frame_height));
    return false;
  }

  const u16 *gpu = reinterpret_cast<const u16 *>(g_frame_targets[0].cpu);
  const u32 pixel_count = g_frame_target_width * g_frame_target_height;
  u32 direct_matches = 0U;
  u32 boundary_matches = 0U;
  u32 vertical_matches = 0U;
  u32 horizontal_matches = 0U;
  u32 rotate_matches = 0U;
  u32 rb_swap_matches = 0U;
  u32 gpu_hash = 2166136261U;
  u32 expected_hash = 2166136261U;
  u32 first_x = UINT32_MAX;
  u32 first_y = UINT32_MAX;
  u16 first_expected = 0U;
  u16 first_actual = 0U;
  u32 first_boundary_x = UINT32_MAX;
  u32 first_boundary_y = UINT32_MAX;
  u32 pixel_index = 0U;
  for (u32 y = 0U; y < g_frame_target_height; ++y) {
    ScaledSourceCoordinate source_y = {};
    if (!ResolveScaledSourceCoordinate(
            y, g_frame_height, g_frame_target_height, &source_y)) {
      return false;
    }
    for (u32 x = 0U; x < g_frame_target_width; ++x) {
      ScaledSourceCoordinate source_x = {};
      if (!ResolveScaledSourceCoordinate(
              x, g_frame_width, g_frame_target_width, &source_x)) {
        return false;
      }
      const u16 actual = gpu[pixel_index++];
      const u16 expected =
          FrameSelfTestPixel(source_x.direct, source_y.direct);
      const bool direct_match = actual == expected;
      const bool boundary_match =
          !direct_match &&
          ((source_x.exact_boundary &&
            actual == FrameSelfTestPixel(
                          source_x.alternate, source_y.direct)) ||
           (source_y.exact_boundary &&
            actual == FrameSelfTestPixel(
                          source_x.direct, source_y.alternate)) ||
           (source_x.exact_boundary && source_y.exact_boundary &&
            actual == FrameSelfTestPixel(
                          source_x.alternate, source_y.alternate)));
      direct_matches += direct_match ? 1U : 0U;
      boundary_matches += boundary_match ? 1U : 0U;
      vertical_matches +=
          actual == FrameSelfTestPixel(
              source_x.direct,
              g_frame_height - 1U - source_y.direct) ? 1U : 0U;
      horizontal_matches +=
          actual == FrameSelfTestPixel(
              g_frame_width - 1U - source_x.direct,
              source_y.direct) ? 1U : 0U;
      rotate_matches +=
          actual == FrameSelfTestPixel(
              g_frame_width - 1U - source_x.direct,
              g_frame_height - 1U - source_y.direct) ? 1U : 0U;
      rb_swap_matches +=
          actual == SwapRgb565RedBlue(expected) ? 1U : 0U;
      gpu_hash = Fnv1a32AppendU16(gpu_hash, actual);
      expected_hash = Fnv1a32AppendU16(expected_hash, expected);
      if (boundary_match && first_boundary_x == UINT32_MAX) {
        first_boundary_x = x;
        first_boundary_y = y;
      }
      if (!direct_match && !boundary_match && first_x == UINT32_MAX) {
        first_x = x;
        first_y = y;
        first_expected = expected;
        first_actual = actual;
      }
    }
  }

  g_frame_self_test_passed =
      direct_matches + boundary_matches == pixel_count;
  printf("boot: pi4v3d frame selftest status=%s source=%ux%u "
         "target=%ux%u pixels=%u "
         "direct=%u boundary=%u vflip=%u hflip=%u rot180=%u rb_swap=%u "
         "expected_hash=0x%08x gpu_hash=0x%08x "
         "first_boundary=%u,%u first_mismatch=%u,%u/0x%04x->0x%04x "
         "binner_us=%u renderer_us=%u visible_upload=%u\r\n",
         g_frame_self_test_passed ? "pass" : "fail",
         static_cast<unsigned>(g_frame_width),
         static_cast<unsigned>(g_frame_height),
         static_cast<unsigned>(g_frame_target_width),
         static_cast<unsigned>(g_frame_target_height),
         static_cast<unsigned>(pixel_count),
         static_cast<unsigned>(direct_matches),
         static_cast<unsigned>(boundary_matches),
         static_cast<unsigned>(vertical_matches),
         static_cast<unsigned>(horizontal_matches),
         static_cast<unsigned>(rotate_matches),
         static_cast<unsigned>(rb_swap_matches),
         static_cast<unsigned>(expected_hash),
         static_cast<unsigned>(gpu_hash),
         static_cast<unsigned>(first_boundary_x),
         static_cast<unsigned>(first_boundary_y),
         static_cast<unsigned>(first_x),
         static_cast<unsigned>(first_y),
         static_cast<unsigned>(first_expected),
         static_cast<unsigned>(first_actual),
         static_cast<unsigned>(binner_us),
         static_cast<unsigned>(renderer_us),
         g_frame_self_test_passed ? 1U : 0U);
  return g_frame_self_test_passed;
}

bool ExecuteFrameRender(const FrameTarget &target) {
  const u32 sequence = g_frame_sequence;
  const u32 slot = sequence & 1U;
  if (target.width != g_frame_target_width ||
      target.height != g_frame_target_height ||
      (target.copy_to_target &&
       (target.pixels == nullptr ||
        target.pitch < target.width * sizeof(u16)))) {
    return false;
  }

  FrameRenderTimings timings = {};
  const u64 render_job_start_us = CTimer::GetClockTicks64();
  if (!SubmitFrameRenderSlot(slot, &timings)) {
    return false;
  }
  const u64 render_job_done_us = CTimer::GetClockTicks64();
  const u32 binner_us =
      timings.source_binner_us + timings.prepass_binner_us +
      timings.output_binner_us;
  const u32 renderer_us =
      timings.source_renderer_us + timings.prepass_renderer_us +
      timings.output_renderer_us;
  const Buffer &target_buffer = g_frame_targets[slot];

  const u32 tight_pitch = g_frame_target_width * sizeof(u16);
  g_last_rendered_frame.valid = true;
  g_last_rendered_frame.pixels = target_buffer.cpu;
  // BCM2711 HVS5 is an AXI master and consumes the DMA/physical address.
  // BUS_ADDRESS() adds the legacy VideoCore 0xc0000000 alias, which is valid
  // for older VC blocks but makes HVS5 scan out unrelated memory.
  g_last_rendered_frame.framebuffer_bus_address =
      target_buffer.dma_address;
  g_last_rendered_frame.width = g_frame_target_width;
  g_last_rendered_frame.height = g_frame_target_height;
  g_last_rendered_frame.pitch = tight_pitch;
  g_last_rendered_frame.sequence = sequence;
  g_last_rendered_frame.slot = slot;
  u32 readback_copy_us = 0U;
  if (target.copy_to_target) {
    const u64 readback_copy_start_us = CTimer::GetClockTicks64();
    for (u32 y = 0U; y < g_frame_target_height; ++y) {
      memcpy(target.pixels + y * target.pitch,
             target_buffer.cpu + y * tight_pitch, tight_pitch);
    }
    readback_copy_us = static_cast<u32>(
        CTimer::GetClockTicks64() - readback_copy_start_us);
  }
  const bool effect_change = g_frame_effect_output_log_pending;
  g_frame_last_effect_change = effect_change;
  // Full-frame hashing and formatted logging are intentionally limited to
  // startup and effect changes.  On Pi4 they execute on the VICE core; doing
  // this periodically would stall PCM production without adding new state.
  const bool log_frame = effect_change || sequence < 3U;
  u32 hash = 0U;
  const u64 hash_start_us = CTimer::GetClockTicks64();
  if (log_frame) {
    hash = 2166136261U;
    for (u32 y = 0U; y < g_frame_target_height; ++y) {
      const uint8_t *row = target_buffer.cpu + y * tight_pitch;
      for (u32 x = 0U; x < tight_pitch; ++x) {
        hash ^= row[x];
        hash *= 16777619U;
      }
    }
  }
  const u64 hash_done_us = CTimer::GetClockTicks64();
  ++g_frame_sequence;
  g_frame_effect_output_log_pending = false;
  if (log_frame) {
    printf("boot: pi4v3d frame sequence=%u slot=%u status=pass "
           "source=%ux%u target=%ux%u multipass=%u "
           "source_binner_us=%u source_renderer_us=%u "
           "output_prepass_binner_us=%u output_prepass_renderer_us=%u "
           "output_binner_us=%u output_renderer_us=%u "
           "binner_us=%u renderer_us=%u "
           "render_job_us=%u readback_copy_us=%u hash_us=%u "
           "source_upload_us=%u response_cache_us=%u "
           "hash=0x%08x effect_change=%u\r\n",
           static_cast<unsigned>(sequence), static_cast<unsigned>(slot),
           static_cast<unsigned>(g_frame_width),
           static_cast<unsigned>(g_frame_height),
           static_cast<unsigned>(g_frame_target_width),
           static_cast<unsigned>(g_frame_target_height),
           g_frame_multipass_active ? 1U : 0U,
           static_cast<unsigned>(timings.source_binner_us),
           static_cast<unsigned>(timings.source_renderer_us),
           static_cast<unsigned>(timings.prepass_binner_us),
           static_cast<unsigned>(timings.prepass_renderer_us),
           static_cast<unsigned>(timings.output_binner_us),
           static_cast<unsigned>(timings.output_renderer_us),
           static_cast<unsigned>(binner_us),
           static_cast<unsigned>(renderer_us),
           static_cast<unsigned>(render_job_done_us - render_job_start_us),
           static_cast<unsigned>(readback_copy_us),
           static_cast<unsigned>(hash_done_us - hash_start_us),
           static_cast<unsigned>(g_frame_source_upload_us),
           static_cast<unsigned>(g_frame_output_response_lut_build_us),
           static_cast<unsigned>(hash), effect_change ? 1U : 0U);
  }
  return true;
}

}  // namespace

void Configure(bool requested, const char *shader_preset,
               const char *boot_test) {
  g_requested = requested;
  g_shader_preset = shader_preset != nullptr ? shader_preset : "off";
  g_boot_test = boot_test != nullptr ? boot_test : "off";
}

bool Initialize() {
  if (!g_requested) {
    return false;
  }
  if (g_initialized) {
    return false;
  }
  g_initialized = true;

  u32 configured_clock = 0U;
  u32 measured_clock = 0U;
  const bool configured_clock_valid =
      ReadClockRate(PROPTAG_GET_CLOCK_RATE, &configured_clock);
  const bool measured_clock_valid =
      ReadClockRate(PROPTAG_GET_CLOCK_RATE_MEASURED, &measured_clock);

  PowerSnapshot power =
      CapturePowerSnapshot(measured_clock_valid, measured_clock);
  PowerInfo power_info = AnalyzePowerSnapshot(power);
  const bool power_write_requested =
      IsProductionFramePreset() ||
      strcmp(g_boot_test, "mmu") == 0 ||
      strcmp(g_boot_test, "fragment_replay") == 0 ||
      strcmp(g_boot_test, "fragment_lifecycle") == 0 ||
      strcmp(g_boot_test, "fragment_scanout") == 0 ||
      strcmp(g_boot_test, "fragment_fullscreen") == 0;

  printf("boot: pi4v3d m0%s preset=%s test=%s\r\n",
         power_write_requested ? "+power-sequence" : " read-only",
         g_shader_preset, g_boot_test);
  printf("boot: pi4v3d clock configured_valid=%u configured_hz=%u "
         "measured_valid=%u measured_hz=%u state_valid=%u state=0x%08x\r\n",
         configured_clock_valid ? 1U : 0U, configured_clock,
         measured_clock_valid ? 1U : 0U, measured_clock,
         power.clock_state_valid ? 1U : 0U,
         static_cast<unsigned>(power.clock_state));
  printf("boot: pi4v3d control pm_grafx=0x%08x bridge_id=0x%08x "
         "asb_slave=0x%08x asb_master=0x%08x readable=%u "
         "bridge_id_valid=%u domain_enabled=%u reset_released=%u "
         "bridges_active=%u bridges_stopped=%u\r\n",
         static_cast<unsigned>(power.pm_grafx),
         static_cast<unsigned>(power.asb_bridge_id),
         static_cast<unsigned>(power.asb_slave),
         static_cast<unsigned>(power.asb_master),
         power_info.controls_readable ? 1U : 0U,
         power_info.bridge_id_valid ? 1U : 0U,
         power_info.domain_enabled ? 1U : 0U,
         power_info.reset_released ? 1U : 0U,
         power_info.slave_active && power_info.master_active ? 1U : 0U,
         power_info.slave_stopped && power_info.master_stopped ? 1U : 0U);

  if (!power_info.safe_to_probe && power_write_requested) {
    printf("boot: pi4v3d power requested=1 eligible=%u\r\n",
           power_info.power_on_eligible ? 1U : 0U);
    const PowerSnapshot initial_power = power;
    if (power_info.power_on_eligible && RunPowerOnSequence(initial_power)) {
      measured_clock = 0U;
      const bool post_measured_clock_valid =
          ReadClockRate(PROPTAG_GET_CLOCK_RATE_MEASURED, &measured_clock);
      power = CapturePowerSnapshot(post_measured_clock_valid, measured_clock);
      power_info = AnalyzePowerSnapshot(power);
      printf("boot: pi4v3d power post pm_grafx=0x%08x "
             "asb_slave=0x%08x asb_master=0x%08x state=0x%08x "
             "measured_hz=%u safe_to_probe=%u\r\n",
             static_cast<unsigned>(power.pm_grafx),
             static_cast<unsigned>(power.asb_slave),
             static_cast<unsigned>(power.asb_master),
             static_cast<unsigned>(power.clock_state), measured_clock,
             power_info.safe_to_probe ? 1U : 0U);
      if (!power_info.safe_to_probe) {
        printf("boot: pi4v3d power failed step=post-state-verification\r\n");
        RestorePowerSnapshot(initial_power);
        measured_clock = 0U;
        const bool restored_measured_clock_valid =
            ReadClockRate(PROPTAG_GET_CLOCK_RATE_MEASURED, &measured_clock);
        power = CapturePowerSnapshot(restored_measured_clock_valid,
                                     measured_clock);
        power_info = AnalyzePowerSnapshot(power);
      }
    } else if (!power_info.power_on_eligible) {
      printf("boot: pi4v3d power denied reason=precondition-mismatch\r\n");
    }
  }

  if (!power_info.safe_to_probe) {
    printf("boot: pi4v3d identity skipped clock_running=%u "
           "reset_released=%u bridges_active=%u control_readable=%u "
           "bridge_id_valid=%u\r\n",
           power_info.clock_running ? 1U : 0U,
           power_info.reset_released ? 1U : 0U,
           power_info.slave_active && power_info.master_active ? 1U : 0U,
           power_info.controls_readable ? 1U : 0U,
           power_info.bridge_id_valid ? 1U : 0U);
    printf("boot: pi4v3d status=power-sequence-required "
           "dispmanx-fallback-active\r\n");
    return false;
  }

  IdentityRegisters registers = {};
  registers.hub_ident0 = ReadRegister(kV3dHubBase, kHubIdent0);
  registers.hub_ident1 = ReadRegister(kV3dHubBase, kHubIdent1);
  registers.hub_ident2 = ReadRegister(kV3dHubBase, kHubIdent2);
  registers.hub_ident3 = ReadRegister(kV3dHubBase, kHubIdent3);
  registers.core_ident0 = ReadRegister(kV3dCoreBase, kCoreIdent0);
  registers.core_ident1 = ReadRegister(kV3dCoreBase, kCoreIdent1);
  registers.core_ident2 = ReadRegister(kV3dCoreBase, kCoreIdent2);
  registers.mmu_debug = ReadRegister(kV3dHubBase, kHubMmuDebugInfo);

  const IdentityInfo identity = DecodeIdentity(registers);
  g_hardware_visible = identity.supported;
  printf("boot: pi4v3d identity hub=%08x/%08x/%08x/%08x "
         "core=%08x/%08x/%08x mmu_debug=%08x\r\n",
         static_cast<unsigned>(registers.hub_ident0),
         static_cast<unsigned>(registers.hub_ident1),
         static_cast<unsigned>(registers.hub_ident2),
         static_cast<unsigned>(registers.hub_ident3),
         static_cast<unsigned>(registers.core_ident0),
         static_cast<unsigned>(registers.core_ident1),
         static_cast<unsigned>(registers.core_ident2),
         static_cast<unsigned>(registers.mmu_debug));
  printf("boot: pi4v3d decoded accessible=%u supported=%u reference=%u "
         "version=%u cores=%u iprev=%u mmu=%u core_version=%u "
         "pa_bits=%u va_bits=%u mmu_version=%u\r\n",
         identity.accessible ? 1U : 0U, identity.supported ? 1U : 0U,
         identity.reference_profile ? 1U : 0U,
         static_cast<unsigned>(identity.version),
         static_cast<unsigned>(identity.cores),
         static_cast<unsigned>(identity.ip_revision),
         identity.has_mmu ? 1U : 0U,
         static_cast<unsigned>(identity.core_version),
         static_cast<unsigned>(identity.physical_address_width),
         static_cast<unsigned>(identity.virtual_address_width),
         static_cast<unsigned>(identity.mmu_version));
  printf("boot: pi4v3d status=%s dispmanx-fallback-active\r\n",
         identity.supported ? "hardware-visible-render-backend-pending" :
                              "unsupported-identity");
  if (identity.supported && IsProductionFramePreset()) {
    const bool ladder_ok = RunM3FragmentLifecycleTest();
    g_runtime_armed = ladder_ok;
    printf("boot: pi4v3d runtime preset=%s status=%s m3=%s "
           "frame-selftest=pending dispmanx-fallback=ready\r\n",
           g_shader_preset, g_runtime_armed ? "armed" : "fail",
           ladder_ok ? "pass" : "fail");
    return g_runtime_armed;
  }
  return false;
}

bool Requested() {
  return g_requested;
}

bool IsAvailable() {
  return g_hardware_visible && g_mmu_ready && g_runtime_armed &&
         !g_runtime_failed;
}

bool GetLastRenderedFrame(RenderedFrame *frame) {
  if (frame == nullptr || !g_last_rendered_frame.valid) {
    return false;
  }
  *frame = g_last_rendered_frame;
  return true;
}

bool RenderFrame(const FrameSource &source, const FrameTarget &target,
                 const FrameParams &params) {
  if (!IsAvailable() || !IsRuntimeFramePreset()) {
    return false;
  }
  if (!InitializeFrameResources(source.source_width, source.source_height,
                                target.width, target.height) ||
      !RunFrameSelfTest() || !ApplyFrameParams(params) ||
      !UploadFrameSource(source) ||
      (!g_frame_source_pass_active &&
       !UpdateFrameEdgeGlowUniforms(false)) ||
      !UpdateFrameTemporalUniform() ||
      !ExecuteFrameRender(target)) {
    printf("boot: pi4v3d frame status=fail sequence=%u; "
           "disabling-runtime\r\n",
           static_cast<unsigned>(g_frame_sequence));
    g_runtime_failed = true;
    return false;
  }
  return true;
}

uint32_t LastFrameSequence() {
  return g_frame_sequence == 0U ? 0U : g_frame_sequence - 1U;
}

bool LastFrameChangedEffect() {
  return g_frame_last_effect_change;
}

bool RunBootTest(const ScanoutTarget &target) {
  if (g_requested && strcmp(g_boot_test, "off") != 0) {
    if (strcmp(g_boot_test, "mmu") == 0 && g_hardware_visible) {
      printf("boot: pi4v3d test=mmu power-and-identity-pass "
             "starting-m1\r\n");
      return RunM1OffscreenTest();
    } else if (strcmp(g_boot_test, "fragment_replay") == 0 &&
               g_hardware_visible) {
      printf("boot: pi4v3d test=fragment_replay power-and-identity-pass "
             "starting-m1-plus-m2\r\n");
      return RunM2FragmentReplayTest();
    } else if (strcmp(g_boot_test, "fragment_lifecycle") == 0 &&
               g_hardware_visible) {
      printf("boot: pi4v3d test=fragment_lifecycle "
             "power-and-identity-pass starting-m1-through-m3\r\n");
      return RunM3FragmentLifecycleTest();
    } else if (strcmp(g_boot_test, "fragment_scanout") == 0 &&
               g_hardware_visible) {
      printf("boot: pi4v3d test=fragment_scanout "
             "power-and-identity-pass starting-m1-through-m4\r\n");
      return RunM4FragmentScanoutTest(target);
    } else if (strcmp(g_boot_test, "fragment_fullscreen") == 0 &&
               g_hardware_visible) {
      printf("boot: pi4v3d test=fragment_fullscreen "
             "power-and-identity-pass starting-m1-through-m3-plus-m5\r\n");
      const bool ladder_ok = RunM3FragmentLifecycleTest();
      const bool preset_ok = strcmp(g_shader_preset, "frame_copy") == 0;
      g_runtime_armed = ladder_ok && preset_ok;
      printf("boot: pi4v3d test=fragment_fullscreen status=%s "
             "m3=%s preset=%s runtime_armed=%u "
             "dispmanx-fallback=ready\r\n",
             g_runtime_armed ? "armed" : "fail",
             ladder_ok ? "pass" : "fail", g_shader_preset,
             g_runtime_armed ? 1U : 0U);
      return g_runtime_armed;
    } else {
      printf("boot: pi4v3d test=%s pending-render-backend\r\n",
             g_boot_test);
    }
  }
  return false;
}

void Shutdown() {
  ResetFrameResources(true);
  FreeBuffer(&g_test_source);
  FreeBuffer(&g_test_target);
  FreeBuffer(&g_render_target_alt);
  for (u32 slot = 0U; slot < kRenderJobSlotCount; ++slot) {
    FreeBuffer(&g_render_controls[slot]);
    ResetRenderJob(&g_render_jobs[slot]);
  }
  FreeBuffer(&g_render_tile_scratch);
  if (g_page_table != nullptr || g_mmu_ready) {
    FreeMmu();
  }
  g_requested = false;
  g_initialized = false;
  g_hardware_visible = false;
  g_m1_attempted = false;
  g_m1_passed = false;
  g_m2_attempted = false;
  g_m2_passed = false;
  g_m3_attempted = false;
  g_m3_passed = false;
  g_m4_attempted = false;
  g_m4_passed = false;
  g_runtime_armed = false;
  g_runtime_failed = false;
  g_m2_readback_hash = 0U;
  g_shader_preset = "off";
  g_boot_test = "off";
}

}  // namespace pi4v3d
