#include "pi5v3d/pi5_v3d.h"
#include "pi5v3d/pi5_v3d71_texture_state.h"
#include "pi5v3d/pi5_v3d_shader.h"
#include "pi5v3d/shaders/pi5_shader_package_adapter.h"
#include "pi5v3d/shaders/shader_artifact_materializer.h"
#include "v3dcrt/bloom_pipeline.h"
#include "v3dcrt/edge_glow_filter.h"
#include "v3dcrt/output_response.h"

#include <circle/bcm2835.h>
#include <circle/bcmpropertytags.h>
#include <circle/new.h>
#include <circle/memio.h>
#include <circle/synchronize.h>
#include <circle/timer.h>

#include <math.h>
#include <stdio.h>
#include <string.h>

namespace pi5v3d {

namespace {

bool g_requested = false;
bool g_kms_active = false;
bool g_initialized = false;
bool g_available = false;
bool g_hardware_visible = false;
bool g_unavailable_logged = false;
bool g_buffers_ready = false;
bool g_buffers_logged = false;
bool g_frame_path_logged = false;
bool g_frame_unsupported_logged = false;
bool g_runtime_qpu_failed = false;
bool g_runtime_qpu_failure_logged = false;
bool g_runtime_fragment_failed = false;
bool g_runtime_fragment_failure_logged = false;
bool g_direct_scanout_failed = false;
bool g_direct_scanout_logged = false;
bool g_direct_scanout_failure_logged = false;
bool g_qpu_warmup_done = false;
bool g_qpu_warmup_attempted = false;
bool g_qpu_effect_probe_done = false;
u32 g_qpu_effect_probe_attempts = 0;
bool g_fragment_probe_done = false;
bool g_fragment_package_ready = false;
bool g_fragment_bloom_packages_ready = false;
bool g_fragment_fast_cubic_package_ready = false;
bool g_fragment_fast_cubic_selected = false;
bool g_fragment_fast_cubic_runtime_failed = false;
bool g_runtime_bloom_failed = false;
bool g_runtime_bloom_failure_logged = false;
bool g_fragment_bloom_path_log_valid = false;
bool g_fragment_bloom_path_active = false;
u32 g_fragment_probe_attempts = 0;
u32 g_fragment_temporal_frame = 0;
bool g_fragment_probe_wait_for_vblank = true;
bool g_fragment_frame_state_log_valid = false;
u32 g_fragment_frame_log_width = 0;
u32 g_fragment_frame_log_height = 0;
bool g_fragment_frame_log_linear = false;
u32 g_fragment_frame_log_gap_bits = 0;
u32 g_fragment_frame_log_weight_bits = 0;
u32 g_fragment_frame_log_mask_signature = 0;
u32 g_fragment_frame_log_response_signature = 0;
u32 g_fragment_frame_log_effect_signature = 0;
bool g_fragment_frame_log_bloom_enabled = false;
u32 g_fragment_frame_log_bloom_factor_bits = 0;
bool g_fragment_source_effect_log_valid = false;
bool g_fragment_source_filter_log_enabled = false;
u32 g_fragment_source_filter_log_sigma_bits = 0;
u32 g_fragment_source_convergence_log_signature = 0;
u32 g_fragment_source_composite_log_signature = 0;
u32 g_fragment_source_jitter_log_signature = 0;
u32 g_fragment_source_noise_log_signature = 0;
bool g_fragment_source_sampler_log_linear = false;
bool g_source_palette_log_valid = false;
u32 g_source_palette_log_generation = 0;
u32 g_source_palette_log_signature = 0;
struct FragmentSourceStagingCache {
  u32 palette[256];
  u16 palette_rgb565[256];
  bool palette_valid;
  u32 palette_generation;
  u32 palette_signature;
  v3dcrt::OutputResponseParams palette_output_response;
  u16 *output_response_lut;
  bool output_response_lut_valid;
  v3dcrt::OutputResponseParams output_response_lut_params;
};
FragmentSourceStagingCache g_fragment_source_staging_cache = {};
bool Rgba8TextureLayoutsEqual(
    const v3d71::Rgba8TextureLayout &a,
    const v3d71::Rgba8TextureLayout &b);
bool g_qpu_scanline_param_log_valid = false;
int g_qpu_scanline_log_weight_x100 = 0;
int g_qpu_scanline_log_gap_x100 = 0;
u32 g_qpu_scanline_log_scale_x16 = 0;
u32 g_qpu_scanline_log_half_mask = 0;
u32 g_qpu_scanline_log_quarter_mask = 0;
u32 g_qpu_scanline_log_eighth_mask = 0;
u32 g_qpu_scanline_log_sixteenth_mask = 0;
bool g_qpu_frame_program_log_valid = false;
int g_qpu_frame_program_log_program = -1;
bool g_qpu_geometry_unsupported_logged = false;
bool g_output_resolution_fallback_logged = false;
ShaderPreset g_shader_preset = kShaderOff;
BootTestMode g_boot_test_mode = kBootTestOff;
FragmentPackageMode g_fragment_package_mode = kFragmentPackageDefault;
RenderResolution g_render_resolution = kRenderResolutionSource;
struct BloomPassTimings {
  u32 source_us;
  u32 base_us;
  u32 horizontal_us;
  u32 vertical_us;
  u32 composite_us;
};

struct FragmentPassTimings {
  u32 stage_us;
  u32 source_prepare_us;
  u32 source_render_us;
  u32 edge_sample_us;
  u32 output_prepare_us;
  u32 output_render_us;
};

enum BloomPassTimingIndex {
  kBloomPassTimingSource = 0,
  kBloomPassTimingBase,
  kBloomPassTimingHorizontal,
  kBloomPassTimingVertical,
  kBloomPassTimingComposite,
  kBloomPassTimingCount
};

struct RenderStats {
  bool active;
  bool complete;
  ShaderPreset shader;
  const char *path;
  const char *work_label;
  bool direct_scanout;
  u64 window_start_us;
  u32 frames;
  u32 total_frames;
  u32 stage_min_us;
  u32 stage_max_us;
  u64 stage_total_us;
  u32 work_min_us;
  u32 work_max_us;
  u64 work_total_us;
  u32 scanout_min_us;
  u32 scanout_max_us;
  u64 scanout_total_us;
  u32 total_min_us;
  u32 total_max_us;
  u64 total_total_us;
};

enum FragmentPassTimingIndex {
  kFragmentPassTimingStage = 0,
  kFragmentPassTimingSourcePrepare,
  kFragmentPassTimingSourceRender,
  kFragmentPassTimingEdgeSample,
  kFragmentPassTimingOutputPrepare,
  kFragmentPassTimingOutputRender,
  kFragmentPassTimingCount
};

struct FragmentPassStats {
  bool active;
  bool complete;
  const char *path;
  u32 signature;
  u32 frames;
  u32 min_us[kFragmentPassTimingCount];
  u32 max_us[kFragmentPassTimingCount];
  u64 total_us[kFragmentPassTimingCount];
};

struct BloomPassStats {
  bool active;
  bool complete;
  u32 signature;
  u32 frames;
  u32 min_us[kBloomPassTimingCount];
  u32 max_us[kBloomPassTimingCount];
  u64 total_us[kBloomPassTimingCount];
};

enum RenderFallbackReason {
  kRenderFallbackRuntimeQpuDisabled = 0,
  kRenderFallbackStageSource,
  kRenderFallbackSourceStageBlit,
  kRenderFallbackQpuGeometry,
  kRenderFallbackQpuCsdBuild,
  kRenderFallbackQpuSubmit,
  kRenderFallbackQpuVerify,
  kRenderFallbackQpuCpuScanout,
  kRenderFallbackFragmentProbePending,
  kRenderFallbackFragmentProbeDone,
  kRenderFallbackFragmentProbeSubmit,
  kRenderFallbackRuntimeFragmentDisabled,
  kRenderFallbackFragmentFrameState,
  kRenderFallbackFragmentFrameSubmit,
  kRenderFallbackBloomPrepare,
  kRenderFallbackBloomFrameState,
  kRenderFallbackBloomFrameSubmit,
  kRenderFallbackCount
};

struct RenderFallbackStats {
  u32 total;
  u32 count[kRenderFallbackCount];
  bool logged[kRenderFallbackCount];
};

struct FragmentReplayTileScratchLayout {
  u32 width;
  u32 height;
  u32 tiles_x;
  u32 tiles_y;
  u32 tile_alloc_bytes;
  u32 tsda_offset;
  u32 tsda_bytes;
  u32 total_bytes;
};

RenderStats g_render_stats;
FragmentPassStats g_fragment_pass_stats;
BloomPassStats g_bloom_pass_stats;
bool g_render_stats_scanout_mode_valid = false;
bool g_render_stats_direct_scanout = false;
RenderFallbackStats g_render_fallback_stats;
Buffer g_source_scratch;
Buffer g_fragment_source_scratch;
Buffer g_target_scratch;
Buffer g_target_scratch_alt;
Buffer g_fragment_intermediate_scratch;
v3d71::Rgba8TextureLayout g_fragment_intermediate_layout;
v3d71::Rgba8TextureLayout g_fragment_source_layout;
Buffer g_fragment_bloom_base_scratch;
Buffer g_fragment_bloom_horizontal_scratch;
Buffer g_fragment_bloom_vertical_scratch;
v3d71::Rgba8TextureLayout g_fragment_bloom_base_layout;
v3d71::Rgba8TextureLayout g_fragment_bloom_horizontal_layout;
v3d71::Rgba8TextureLayout g_fragment_bloom_vertical_layout;
u32 g_qpu_target_buffer_index = 0;
u32 g_qpu_target_select_log_count = 0;
Buffer *g_last_completed_target = nullptr;
u32 g_last_completed_width = 0;
u32 g_last_completed_height = 0;
uint8_t *g_page_table_allocation = nullptr;
u32 *g_page_table = nullptr;
uint8_t *g_mmu_scratch_allocation = nullptr;
uint8_t *g_mmu_scratch = nullptr;
u64 g_page_table_bus_address = 0;
u64 g_mmu_scratch_bus_address = 0;
u32 g_page_table_allocation_size = 0;
u32 g_mmu_scratch_allocation_size = 0;
u32 g_next_v3d_address = 0;
bool g_mmu_ready = false;
bool g_mmu_logged = false;

// Register ranges from Raspberry Pi Linux rpi-6.12.y:
// arch/arm64/boot/dts/broadcom/bcm2712-ds.dtsi, node brcm,2712-v3d.
constexpr uintptr kV3dHubBase = 0x1002000000UL;
constexpr uintptr kV3dCore0Base = 0x1002008000UL;
constexpr uintptr kV3dSmsBase = 0x1002030800UL;

constexpr u32 kV3dHubIdent0 = 0x00008;
constexpr u32 kV3dHubIdent1 = 0x0000C;
constexpr u32 kV3dHubIdent2 = 0x00010;
constexpr u32 kV3dHubIdent3 = 0x00014;
constexpr u32 kV3dHubIntStatus = 0x00050;
constexpr u32 kV3dHubIntClear = 0x00058;
constexpr u32 kV3dMmuControl = 0x01000;
constexpr u32 kV3dMmuCtl = 0x01200;
constexpr u32 kV3dMmuPtPaBase = 0x01204;
constexpr u32 kV3dMmuVioId = 0x0122C;
constexpr u32 kV3dMmuIllegalAddr = 0x01230;
constexpr u32 kV3dMmuVioAddr = 0x01234;
constexpr u32 kV3dMmuDebugInfo = 0x01238;

constexpr u32 kV3dCoreIdent0 = 0x00000;
constexpr u32 kV3dCoreIdent1 = 0x00004;
constexpr u32 kV3dCoreIdent2 = 0x00008;
constexpr u32 kV3dCoreL2TCacheCtl = 0x00030;
constexpr u32 kV3dCoreL2TFlushStart = 0x00034;
constexpr u32 kV3dCoreL2TFlushEnd = 0x00038;
constexpr u32 kV3dCoreIntStatus = 0x00050;
constexpr u32 kV3dCoreIntClear = 0x00058;
constexpr u32 kV3dCoreSliceCacheCtl = 0x00024;
constexpr u32 kV3dCleCt0Cs = 0x00100;
constexpr u32 kV3dCleCt1Cs = 0x00104;
constexpr u32 kV3dCleCt0Ea = 0x00108;
constexpr u32 kV3dCleCt1Ea = 0x0010C;
constexpr u32 kV3dCleCt0Ca = 0x00110;
constexpr u32 kV3dCleCt1Ca = 0x00114;
constexpr u32 kV3dCleCt0Pc = 0x00128;
constexpr u32 kV3dCleCt1Pc = 0x0012C;
constexpr u32 kV3dCleCt0Qts = 0x0015C;
constexpr u32 kV3dCleCt0Qba = 0x00160;
constexpr u32 kV3dCleCt1Qba = 0x00164;
constexpr u32 kV3dCleCt0Qea = 0x00168;
constexpr u32 kV3dCleCt1Qea = 0x0016C;
constexpr u32 kV3dCleCt0Qma = 0x00170;
constexpr u32 kV3dCleCt0Qms = 0x00174;
constexpr u32 kV3dCsdStatus = 0x00900;
constexpr u32 kV3dCsdQueuedCfg0 = 0x00930;
constexpr u32 kV3dCsdQueuedCfg1 = 0x00934;
constexpr u32 kV3dCsdQueuedCfg2 = 0x00938;
constexpr u32 kV3dCsdQueuedCfg3 = 0x0093C;
constexpr u32 kV3dCsdQueuedCfg4 = 0x00940;
constexpr u32 kV3dCsdQueuedCfg5 = 0x00944;
constexpr u32 kV3dCsdQueuedCfg6 = 0x00948;
constexpr u32 kV3dCsdQueuedCfg7 = 0x0094C;
constexpr u32 kV3dCsdCurrentCfg0 = 0x00958;
constexpr u32 kV3dCsdCurrentCfg1 = 0x0095C;
constexpr u32 kV3dCsdCurrentCfg2 = 0x00960;
constexpr u32 kV3dCsdCurrentCfg3 = 0x00964;
constexpr u32 kV3dCsdCurrentCfg4 = 0x00968;
constexpr u32 kV3dCsdCurrentCfg5 = 0x0096C;
constexpr u32 kV3dCsdCurrentCfg6 = 0x00970;
constexpr u32 kV3dCsdCurrentCfg7 = 0x00974;

constexpr u32 kV3dSmsTeeCs = 0x00400;
constexpr u32 kV3dSmsStateMask = 0x0000000F;
constexpr u32 kV3dSmsPowerOffState = 0x0000000D;

constexpr u32 kV3dHubIdent1NCoresMask = 0x00000F00;
constexpr u32 kV3dHubIdent1NCoresShift = 8;
constexpr u32 kV3dHubIdent1RevMask = 0x000000F0;
constexpr u32 kV3dHubIdent1RevShift = 4;
constexpr u32 kV3dHubIdent1TverMask = 0x0000000F;
constexpr u32 kV3dHubIdent1TverShift = 0;
constexpr u32 kV3dHubIdent2WithMmu = 1U << 8;
constexpr u32 kV3dHubIntMmuWriteViolation = 1U << 5;
constexpr u32 kV3dHubIntMmuPtInvalid = 1U << 4;
constexpr u32 kV3dHubIntMmuCapExceeded = 1U << 3;
constexpr u32 kV3dHubIdent3IprevMask = 0x0000FF00;
constexpr u32 kV3dHubIdent3IprevShift = 8;
constexpr u32 kV3dMmuControlFlushing = 1U << 2;
constexpr u32 kV3dMmuControlFlush = 1U << 1;
constexpr u32 kV3dMmuControlEnable = 1U << 0;
constexpr u32 kV3dMmuCtlCapExceededAbort = 1U << 26;
constexpr u32 kV3dMmuCtlCapExceededInt = 1U << 25;
constexpr u32 kV3dMmuCtlPtInvalidEnable = 1U << 16;
constexpr u32 kV3dMmuCtlPtInvalidAbort = 1U << 19;
constexpr u32 kV3dMmuCtlPtInvalidInt = 1U << 18;
constexpr u32 kV3dMmuCtlWriteViolationAbort = 1U << 11;
constexpr u32 kV3dMmuCtlWriteViolationInt = 1U << 10;
constexpr u32 kV3dMmuCtlTlbClearing = 1U << 7;
constexpr u32 kV3dMmuCtlTlbClear = 1U << 2;
constexpr u32 kV3dMmuCtlEnable = 1U << 0;
constexpr u32 kV3dMmuIllegalAddrEnable = 1U << 31;
constexpr u32 kV3dMmuDebugInfoPaWidthMask = 0x00000F00;
constexpr u32 kV3dMmuDebugInfoPaWidthShift = 8;
constexpr u32 kV3dMmuDebugInfoVaWidthMask = 0x000000F0;
constexpr u32 kV3dMmuDebugInfoVaWidthShift = 4;
constexpr u32 kV3dMmuDebugInfoVersionMask = 0x0000000F;
constexpr u32 kV3dMmuDebugInfoVersionShift = 0;
constexpr u32 kV3dCoreIdent0VerMask = 0xFF000000;
constexpr u32 kV3dCoreIdent0VerShift = 24;
constexpr u32 kV3dL2TCacheFlush = 1U << 0;
constexpr u32 kV3dL2TCacheFlushModeClean = 2U << 1;
constexpr u32 kV3dL2TCacheTmuWriteCombinerFlush = 1U << 8;
constexpr u32 kV3dSliceCacheInvalidateAll = 0x0F0F0F0FU;
constexpr u32 kV3dIntFrDone = 1U << 0;
constexpr u32 kV3dIntFlDone = 1U << 1;
constexpr u32 kV3dIntCsdDone = 1U << 6;
constexpr u32 kV3dIntOutOfMemory = 1U << 2;
constexpr u32 kV3dIntQpuMask = 0x0FFF0000U;
constexpr u32 kV3dIntRenderError =
    kV3dIntOutOfMemory | (1U << 3) | (1U << 4) | (1U << 5);
constexpr u32 kV3dCleCt0QtsEnable = 1U << 1;

constexpr unsigned kExpectedV3dVersion = 71;
constexpr unsigned kClockV3d = 5;
constexpr u32 kV3dMmuPageShift = 12;
constexpr u32 kV3dMmuPageSize = 1U << kV3dMmuPageShift;
constexpr u32 kV3dPageTableBytes = 4 * 1024 * 1024;
constexpr u32 kV3dPageTableEntries = kV3dPageTableBytes / sizeof(u32);
constexpr u32 kV3dFirstAddress = kV3dMmuPageSize;
constexpr u32 kV3dPteValid = 1U << 28;
constexpr u32 kV3dPteWriteable = 1U << 29;
constexpr u32 kBufferAlignment = 4096;
constexpr u32 kMmuScratchBytes = kV3dMmuPageSize;
constexpr u32 kScratchSourceBytes = 512 * 512 * 2;
constexpr u32 kSoftenedSourceYOffset = 256;
constexpr u32 kScratchTargetWidth = 384;
constexpr u32 kScratchTargetHeight = 240;
constexpr u32 kScratchTargetDepth = 16;
constexpr u32 kOutputTargetMaxWidth = 1920;
constexpr u32 kOutputTargetMaxHeight = 1080;
constexpr u32 kScratchControlBytes = 64 * 1024;
constexpr u32 kRenderStatsLogIntervalFrames = 60;
constexpr u32 kQpuEffectProbeMaxAttempts = 120;
constexpr u32 kFragmentProbeMaxAttempts = 180;
constexpr u32 kSourcePatternTile = 32;
constexpr u32 kSolidRclOffset = 0;
constexpr u32 kSolidGenericListOffset = 8 * 1024;
constexpr u32 kSolidTileListOffset = 16 * 1024;
constexpr u32 kSolidTileWidth = 64;
constexpr u32 kSolidTileHeight = 64;
constexpr u32 kSolidClearColorLowBits = 0xFFFFFFFFU;
constexpr u32 kQpuTestCodeOffset = 32 * 1024;
constexpr u32 kQpuTestUniformOffset = 33 * 1024;
constexpr u32 kQpuTestOutputOffset = 0;
constexpr u32 kQpuTestUniformWords = 2;
constexpr u32 kQpuTestWorkgroupSize = 16;
constexpr u32 kQpuTestWorkgroupsPerSupergroup = 1;
constexpr u32 kQpuTestNumBatches = 1;
constexpr u32 kQpuTestMagic = 0x51375055U;  // "Q7PU"
constexpr u32 kQpuFillUniformWords = 3;
constexpr u32 kQpuFillWordsPerGroup = 16;
constexpr u32 kQpuFillBytesPerGroup = kQpuFillWordsPerGroup * sizeof(u32);
constexpr u32 kQpuFillGroups =
    (kScratchTargetWidth * kScratchTargetHeight *
     (kScratchTargetDepth / 8)) / kQpuFillBytesPerGroup;
constexpr u32 kQpuFillColorRgb565 = 0x07E0U;
constexpr u32 kQpuFillColorWord =
    (kQpuFillColorRgb565 << 16) | kQpuFillColorRgb565;
constexpr u32 kQpuFrameCopyUniformWords = 6;
constexpr u32 kQpuFrameCopyBytesPerPixel = kScratchTargetDepth / 8;
constexpr u32 kQpuFrameScanlineUniformWords = 10;
constexpr u16 kQpuFrameScanlineHalfMask = 0x7BEFU;
constexpr u16 kQpuFrameScanlineQuarterMask = 0x39E7U;
constexpr u16 kQpuFrameScanlineEighthMask = 0x18C3U;
constexpr u16 kQpuFrameScanlineSixteenthMask = 0x0841U;
constexpr u32 kQpuFrameScanlineScaleDenominator = 16;
constexpr int kQpuFrameScanlineMaxWeightX100 = 1500;
constexpr u32 kFragmentArtifactCodeSliceBytes = 8 * 1024;
constexpr u32 kFragmentArtifactDataSliceBytes = 4 * 1024;
constexpr u32 kFragmentArtifactCodeOffset = 36 * 1024;
constexpr u32 kFragmentArtifactClOffset = 44 * 1024;
constexpr u32 kFragmentArtifactSamplerOffset = 48 * 1024;
constexpr u32 kFragmentArtifactTextureOffset = 52 * 1024;
constexpr u32 kFragmentArtifactAttributeOffset = 56 * 1024;
constexpr u32 kFragmentArtifactResolvedPatchCapacity = 32;
constexpr u32 kFragmentArtifactAttributeRecordOffset = 0x60;
constexpr u32 kFragmentArtifactSecondAttributeRecordOffset = 0x70;
constexpr u32 kFragmentPrimaryTextureStateOffset = 0x00;
constexpr u32 kFragmentBloomTextureStateOffset = 0x20;
constexpr u32 kFragmentPrimarySamplerStateOffset = 0x60;
constexpr u32 kFragmentBloomSamplerStateOffset = 0x80;
constexpr u32 kFragmentReplayBclOffset = 0;
constexpr u32 kFragmentReplayRclOffset = 4 * 1024;
constexpr u32 kFragmentReplayGenericTileListOffset = 0x80;
constexpr u32 kFragmentReplayWidth = 16;
constexpr u32 kFragmentReplayHeight = 16;
constexpr u32 kFragmentFullscreenWidth = kScratchTargetWidth;
constexpr u32 kFragmentFullscreenHeight = kScratchTargetHeight;
constexpr u32 kFragmentTileAllocBytesPerTile = 64;
constexpr u32 kFragmentTileAllocGuardBytes = 8 * 1024;
constexpr u32 kFragmentTileAllocExtraBytes = 512 * 1024;
constexpr u32 kFragmentTileStateBytesPerTile = 256;
constexpr u32 kFragmentTileStateMinimumBytes = 64 * 1024;
constexpr u32 kFragmentSourceTextureWords = 16;
constexpr u32 kFragmentSourceTextureBytes =
    kFragmentSourceTextureWords * sizeof(u32);

struct FragmentReplayGeometry {
  u32 width;
  u32 height;
  const char *log_name;
};

enum OutputResolutionPath {
  kOutputResolutionPathDisabled = 0,
  kOutputResolutionPathPassthrough,
  kOutputResolutionPathSplitGeometry
};

struct FragmentReplayRenderTarget {
  Buffer *buffer;
  u32 bytes_per_pixel;
  u32 memory_format;
  u32 output_image_format;
  u32 height_in_ub_or_stride;
  u32 image_height;
  bool rb_swap;
  const char *format_name;
};

enum FragmentProbeFrameResult {
  kFragmentProbeFrameWaiting = 0,
  kFragmentProbeFramePresented,
  kFragmentProbeFrameFailed,
  kFragmentProbeFrameDone
};

struct FragmentArtifactSlice {
  const char *name;
  const char *kind;
  u32 offset;
  u32 size;
};

enum FragmentArtifactBindingIndex {
  kFragmentArtifactCodeBinding = 0,
  kFragmentArtifactClBinding,
  kFragmentArtifactSamplerBinding,
  kFragmentArtifactTextureBinding,
  kFragmentArtifactAttributeBinding,
  kFragmentArtifactBindingCount
};

struct FragmentReplayPreparedState {
  bool ready;
  const shader_artifacts::ShaderArtifact *artifact;
  FragmentReplayGeometry geometry;
  shader_artifacts::ShaderArtifactBufferBinding
      bindings[kFragmentArtifactBindingCount];
  u32 bcl_start;
  u32 bcl_end;
  u32 generic_start;
  u32 generic_end;
  u32 rcl_start;
  u32 rcl_end;
};

struct FragmentReplayContext {
  const char *name;
  const shader_artifacts::PreparedFragmentShaderPackage *package;
  Buffer *control_scratch;
  Buffer *tile_scratch;
  FragmentReplayTileScratchLayout *tile_layout;
  FragmentReplayPreparedState *prepared;
};

constexpr u32 kEdgeGlowFrameSampleCount =
    v3dcrt::kEdgeGlowFieldSampleCount;
constexpr u32 kEdgeGlowFrameSampleChannels = 3U;
constexpr u32 kEdgeGlowRegionKernelSize = 3U;
constexpr u32 kEdgeGlowEdgeInsetX1000 = 50U;
constexpr u32 kEdgeGlowEdgeNormalOffsetX1000 = 25U;
constexpr float kEdgeGlowEdgeInset =
    static_cast<float>(kEdgeGlowEdgeInsetX1000) / 1000.0f;
constexpr float kEdgeGlowEdgeNormalOffset =
    static_cast<float>(kEdgeGlowEdgeNormalOffsetX1000) / 1000.0f;
constexpr u64 kEdgeGlowTemporalTimeConstantUs = 120000ULL;
constexpr u64 kEdgeGlowTemporalResetGapUs = 500000ULL;

struct EdgeGlowFrameSamples {
  v3dcrt::EdgeGlowFieldColor color[kEdgeGlowFrameSampleCount];
};

v3dcrt::EdgeGlowTemporalFilter g_edge_glow_temporal_filter = {};

const char *const
    kEdgeGlowFrameSampleSemantics[kEdgeGlowFrameSampleCount]
                                 [kEdgeGlowFrameSampleChannels] = {
  {
    "edge_glow_top_r",
    "edge_glow_top_g",
    "edge_glow_top_b",
  },
  {
    "edge_glow_bottom_r",
    "edge_glow_bottom_g",
    "edge_glow_bottom_b",
  },
  {
    "edge_glow_left_r",
    "edge_glow_left_g",
    "edge_glow_left_b",
  },
  {
    "edge_glow_right_r",
    "edge_glow_right_g",
    "edge_glow_right_b",
  },
};

const FragmentArtifactSlice
    kFragmentArtifactSlices[kFragmentArtifactBindingCount] = {
  {"resource_0x2f2d000", "resource", kFragmentArtifactCodeOffset,
   kFragmentArtifactCodeSliceBytes},
  {"CL_0x3071000", "CL", kFragmentArtifactClOffset,
   kFragmentArtifactDataSliceBytes},
  {"sampler_0x3269000", "sampler", kFragmentArtifactSamplerOffset,
   kFragmentArtifactDataSliceBytes},
  {"resource_0x1355000", "resource", kFragmentArtifactTextureOffset,
   kFragmentArtifactDataSliceBytes},
  {"resource_0x119d000", "resource", kFragmentArtifactAttributeOffset,
   kFragmentArtifactDataSliceBytes}
};

enum FragmentPass {
  kFragmentPassOutput = 0,
  kFragmentPassSource,
  kFragmentPassBloomBase,
  kFragmentPassBloomHorizontal,
  kFragmentPassBloomVertical,
  kFragmentPassBloomComposite,
  kFragmentPassCount
};

struct FragmentPassResources {
  Buffer control_scratch;
  Buffer tile_scratch;
  FragmentReplayTileScratchLayout tile_layout;
  FragmentReplayPreparedState prepared;
  shader_artifacts::PreparedFragmentShaderPackage package;
};

FragmentPassResources g_fragment_pass_resources[kFragmentPassCount] = {};
FragmentPassResources &g_fragment_output =
    g_fragment_pass_resources[kFragmentPassOutput];
FragmentPassResources &g_fragment_source =
    g_fragment_pass_resources[kFragmentPassSource];
FragmentPassResources &g_fragment_bloom_base =
    g_fragment_pass_resources[kFragmentPassBloomBase];
FragmentPassResources &g_fragment_bloom_horizontal =
    g_fragment_pass_resources[kFragmentPassBloomHorizontal];
FragmentPassResources &g_fragment_bloom_vertical =
    g_fragment_pass_resources[kFragmentPassBloomVertical];
FragmentPassResources &g_fragment_bloom_composite =
    g_fragment_pass_resources[kFragmentPassBloomComposite];
const char *const kFragmentPassNames[kFragmentPassCount] = {
  "output", "source", "bloom-base", "bloom-horizontal",
  "bloom-vertical", "bloom-composite",
};
shader_artifacts::PreparedFragmentShaderPackage
    g_fragment_output_fast_cubic_package;

enum QpuFrameProgram {
  kQpuFrameProgramCopy = 0,
  kQpuFrameProgramScanlines
};

struct QpuFrameJob {
  u32 width;
  u32 height;
  u32 row_bytes;
  u32 groups_per_row;
  u32 source_row_skip;
  u32 target_row_skip;
  u32 scanline_scale_x16;
  u32 scanline_half_mask;
  u32 scanline_quarter_mask;
  u32 scanline_eighth_mask;
  u32 scanline_sixteenth_mask;
  int scanline_weight_x100;
  int scanline_gap_x100;
};

void RecordRenderStats(const char *path, const char *work_label,
                       u64 frame_start_us, u64 frame_done_us,
                       u32 stage_us, u32 work_us, u32 scanout_us);
void RecordFragmentPassStats(const char *path, u32 signature,
                             const FragmentPassTimings &timings);
void RecordBloomPassStats(u32 signature,
                          const BloomPassTimings &timings);
void RecordRenderFallback(RenderFallbackReason reason);

int ParamX100(float value);
const char *QpuFrameStatsPath(QpuFrameProgram program,
                              const char *scanout_mode);
bool BlitRgb565BufferNearestToScanout(const Buffer &buffer,
                                      u32 source_width,
                                      u32 source_height,
                                      const OutputTarget &target);

const u64 kQpuMagicStoreCode[] = {
  0x39803186bb03f000ULL,  // ldunifrf rf0: output V3D VA
  0x39807186bb03f000ULL,  // ldunifrf rf1: magic value
  0x3800318bf903f043ULL,  // tmud = rf1
  0x3800318cf903f003ULL,  // tmua = rf0, issuing the store
  0x38203186bb03f000ULL,
  0x38203186bb03f000ULL,
  0x38003186bb03f000ULL,
  0x38003186bb03f000ULL,
  0x38203186bb03f000ULL,
  0x38003186bb03f000ULL,
  0x38003186bb03f000ULL,
  0x38003186bb03f000ULL
};

const u64 kQpuCopyRgb565RowsCode[] = {
  0x39853186bb03f000ULL,  // ldunifrf rf20: rows
  0x39857186bb03f000ULL,  // ldunifrf rf21: 16-word groups per row
  0x3980b186bb03f000ULL,  // ldunifrf rf2: source V3D VA
  0x3980f186bb03f000ULL,  // ldunifrf rf3: target V3D VA
  0x39813186bb03f000ULL,  // ldunifrf rf4: source row skip
  0x39817186bb03f000ULL,  // ldunifrf rf5: target row skip
  0x3800218abb03f002ULL,
  0x39e0218a7c03f282ULL,
  0x380021823803f08aULL,
  0x380021833803f0caULL,
  0x39c0218df903f103ULL,
  0x39e0218d7c03f344ULL,
  0x38002196f903f543ULL,
  0x0420108cf908d083ULL,
  0x38003186bb03f000ULL,
  0x38003186bb03f000ULL,
  0x3882b186bb03f000ULL,
  0x3800318bf903f283ULL,
  0x39e063163c0c3581ULL,
  0x02ffffb5ff009000ULL,
  0x040010c6bb0cd00fULL,
  0x38003186bb03f000ULL,
  0x38003186bb03f000ULL,
  0x380021823803f084ULL,
  0x380021833803f0c5ULL,
  0x39e061943c03f501ULL,
  0x02ffff75ff009000ULL,
  0x38003186bb03f000ULL,
  0x38003186bb03f000ULL,
  0x38003186bb03f000ULL,
  0x38203186bb03f000ULL,
  0x38203186bb03f000ULL,
  0x38003186bb03f000ULL,
  0x38003186bb03f000ULL,
  0x38203186bb03f000ULL,
  0x38003186bb03f000ULL,
  0x38003186bb03f000ULL,
  0x38003186bb03f000ULL
};

const u64 kQpuScanlineRgb565RowsCode[] = {
  0x39853186bb03f000ULL,  // ldunifrf rf20: rows
  0x39857186bb03f000ULL,  // ldunifrf rf21: 16-word groups per row
  0x3980b186bb03f000ULL,  // ldunifrf rf2: source V3D VA
  0x3980f186bb03f000ULL,  // ldunifrf rf3: target V3D VA
  0x39813186bb03f000ULL,  // ldunifrf rf4: source row skip
  0x39817186bb03f000ULL,  // ldunifrf rf5: target row skip
  0x3981b186bb03f000ULL,  // ldunifrf rf6: duplicated RGB565 1/2 mask
  0x3981f186bb03f000ULL,  // ldunifrf rf7: duplicated RGB565 1/4 mask
  0x39823186bb03f000ULL,  // ldunifrf rf8: duplicated RGB565 1/8 mask
  0x39827186bb03f000ULL,  // ldunifrf rf9: duplicated RGB565 1/16 mask
  0x3800218abb03f002ULL,
  0x39e0218a7c03f282ULL,
  0x380021823803f08aULL,
  0x380021833803f0caULL,
  0x39c0218df903f103ULL,
  0x39e0218d7c03f344ULL,
  0x39c02197f903f003ULL,
  0x39e07186b503f5c1ULL,
  0x0200007f00009000ULL,
  0x38003186bb03f000ULL,
  0x38003186bb03f000ULL,
  0x38003186bb03f000ULL,
  0x38002196f903f543ULL,
  0x0420108cf908d083ULL,
  0x38003186bb03f000ULL,
  0x38003186bb03f000ULL,
  0x3882b186bb03f000ULL,
  0x3800318bf903f283ULL,
  0x39e063163c0c3581ULL,
  0x02ffffb5ff009000ULL,
  0x040010c6bb0cd00fULL,
  0x38003186bb03f000ULL,
  0x38003186bb03f000ULL,
  0x020000b000009000ULL,
  0x38003186bb03f000ULL,
  0x38003186bb03f000ULL,
  0x38003186bb03f000ULL,
  0x38002196f903f543ULL,
  0x0420108cf908d083ULL,
  0x38003186bb03f000ULL,
  0x38003186bb03f000ULL,
  0x3882b186bb03f000ULL,
  0x39e0218b7d03f281ULL,
  0x3800218bb503f2c6ULL,
  0x39e0218c7d03f282ULL,
  0x3800218cb503f307ULL,
  0x3800218b3803f2ccULL,
  0x39e0218c7d03f283ULL,
  0x3800218cb503f308ULL,
  0x3800218b3803f2ccULL,
  0x39e0218c7d03f284ULL,
  0x3800218cb503f309ULL,
  0x3800218a3803f2ccULL,
  0x3800318bf903f283ULL,
  0x39e063163c0c3581ULL,
  0x02ffff5dff009000ULL,
  0x040010c6bb0cd00fULL,
  0x38003186bb03f000ULL,
  0x38003186bb03f000ULL,
  0x39e02197b703f5c1ULL,
  0x380021823803f084ULL,
  0x380021833803f0c5ULL,
  0x39e061943c03f501ULL,
  0x02fffe75ff009000ULL,
  0x38003186bb03f000ULL,
  0x38003186bb03f000ULL,
  0x38003186bb03f000ULL,
  0x38203186bb03f000ULL,
  0x38203186bb03f000ULL,
  0x38003186bb03f000ULL,
  0x38003186bb03f000ULL,
  0x38203186bb03f000ULL,
  0x38003186bb03f000ULL,
  0x38003186bb03f000ULL,
  0x38003186bb03f000ULL
};

const u64 kQpuFillRgb565WordsCode[] = {
  0x3982f186bb03f000ULL,  // ldunifrf rf11: 16-word group count
  0x39833186bb03f000ULL,  // ldunifrf rf12: output V3D VA
  0x39837186bb03f000ULL,  // ldunifrf rf13: duplicated RGB565 word
  0x3800218abb03f002ULL,
  0x39e0218a7c03f282ULL,
  0x3800218c3803f30aULL,
  0x39c0218af903f103ULL,
  0x39e0218a7c03f284ULL,
  0x39e062cb3c3432c1ULL,
  0x02ffffddff009000ULL,
  0x3800318cf903f303ULL,
  0x38003186bb03f000ULL,
  0x04001306bb30a00fULL,
  0x38203186bb03f000ULL,
  0x38203186bb03f000ULL,
  0x38003186bb03f000ULL,
  0x38003186bb03f000ULL,
  0x38203186bb03f000ULL,
  0x38003186bb03f000ULL,
  0x38003186bb03f000ULL,
  0x38003186bb03f000ULL
};

enum V3d71Packet {
  kV3d71Flush = 4,
  kV3d71StartTileBinning = 6,
  kV3d71EndOfRendering = 13,
  kV3d71ReturnFromSubList = 18,
  kV3d71FlushVcdCache = 19,
  kV3d71StartAddressOfGenericTileList = 20,
  kV3d71BranchToImplicitTileList = 21,
  kV3d71SupertileCoordinates = 23,
  kV3d71ClearRenderTargets = 25,
  kV3d71EndOfLoads = 26,
  kV3d71EndOfTileMarker = 27,
  kV3d71StoreTileBufferGeneral = 29,
  kV3d71VertexArrayPrims = 36,
  kV3d71SetInstanceId = 54,
  kV3d71PrimListFormat = 56,
  kV3d71GlShaderState = 64,
  kV3d71VcmCacheSize = 71,
  kV3d71TransformFeedbackSpecs = 74,
  kV3d71BlendConstantColor = 86,
  kV3d71ColorWriteMasks = 87,
  kV3d71ZeroAllCentroidFlags = 88,
  kV3d71SampleState = 91,
  kV3d71OcclusionQueryCounter = 92,
  kV3d71CfgBits = 96,
  kV3d71ZeroAllFlatShadeFlags = 97,
  kV3d71ZeroAllNoperspectiveFlags = 99,
  kV3d71PointSize = 104,
  kV3d71LineWidth = 105,
  kV3d71ClipWindow = 107,
  kV3d71ViewportOffset = 108,
  kV3d71ClipperZMinMax = 109,
  kV3d71ClipperXyScaling = 110,
  kV3d71ClipperZScaleOffset = 111,
  kV3d71NumberOfLayers = 119,
  kV3d71TileBinningModeCfg = 120,
  kV3d71TileRenderingModeCfg = 121,
  kV3d71MulticoreRenderingSupertileCfg = 122,
  kV3d71MulticoreRenderingTileListSetBase = 123,
  kV3d71TileCoordinates = 124,
  kV3d71TileCoordinatesImplicit = 125,
  kV3d71TileListInitialBlockSize = 126
};

enum V3d71StoreBuffer {
  kV3d71StoreRenderTarget0 = 0,
  kV3d71StoreNone = 8
};

enum V3d71Format {
  kV3d71MemoryFormatRaster = 0,
  kV3d71DecimateSample0 = 0,
  kV3d71DitherNone = 0,
  kV3d71InternalBpp32 = 0,
  kV3d71RenderTargetTypeClamp8 = 8,
  kV3d71OutputImageFormatBgr565 = 7,
  kV3d71OutputImageFormatRgba8 = 27
};

struct CommandListWriter {
  uint8_t *base;
  u32 capacity;
  u32 offset;
  bool ok;
};

const char *BufferUsageName(BufferUsage usage);
bool CopyRgb565BufferToScanout(const Buffer &buffer,
                               const OutputTarget &target,
                               const char *label);
void ResetRenderStats();
void ResetFragmentPassStats();
void ResetBloomPassStats();
void SelectRenderStatsScanoutMode(bool direct_scanout);
void CleanBufferRangeForV3D(const Buffer &buffer, u32 offset, u32 size);

void CleanFragmentReplayDynamicState(
    const FragmentReplayContext &context) {
  if (context.control_scratch == nullptr) {
    return;
  }
  const Buffer &control = *context.control_scratch;
  CleanBufferRangeForV3D(
      control, kFragmentArtifactCodeOffset +
                   kFragmentPrimarySamplerStateOffset,
      kFragmentBloomSamplerStateOffset + v3d71::kSamplerStateBytes -
          kFragmentPrimarySamplerStateOffset);
  CleanBufferRangeForV3D(
      control, kFragmentArtifactClOffset,
      kFragmentArtifactDataSliceBytes);
  CleanBufferRangeForV3D(
      control, kFragmentArtifactSamplerOffset +
                   kFragmentPrimaryTextureStateOffset,
      kFragmentBloomTextureStateOffset + v3d71::kTextureShaderStateBytes -
          kFragmentPrimaryTextureStateOffset);
}
bool EnsureFragmentRenderTargets(const FragmentReplayGeometry &geometry);
bool EnsureFragmentIntermediateTarget(
    const FragmentReplayGeometry &geometry);
bool EnsureFragmentReplayControlForContext(
    const FragmentReplayContext &context);
bool EnsureRgba8FragmentTarget(
    Buffer *buffer,
    v3d71::Rgba8TextureLayout *layout,
    const FragmentReplayGeometry &geometry,
    const char *label,
    bool *changed);
bool EnsureFragmentSourceBuffer(
    const v3d71::Rgba8TextureLayout &required);
OutputResolutionPath SelectedOutputResolutionPath();

FragmentReplayContext FragmentReplayContextForPass(FragmentPass pass) {
  FragmentPassResources &resources = g_fragment_pass_resources[pass];
  const bool fast_cubic =
      (pass == kFragmentPassOutput || pass == kFragmentPassBloomBase) &&
      g_fragment_fast_cubic_selected &&
      g_fragment_fast_cubic_package_ready;
  const FragmentReplayContext context = {
    kFragmentPassNames[pass],
    fast_cubic ? &g_fragment_output_fast_cubic_package :
                 &resources.package,
    &resources.control_scratch,
    &resources.tile_scratch,
    &resources.tile_layout,
    &resources.prepared
  };
  return context;
}

u32 Field(u32 value, u32 mask, unsigned shift) {
  return (value & mask) >> shift;
}

u32 AlignUp32(u32 value, u32 alignment) {
  return (value + alignment - 1U) & ~(alignment - 1U);
}

uintptr AlignUpPtr(uintptr value, u32 alignment) {
  const uintptr mask = (uintptr)alignment - 1U;
  return (value + mask) & ~mask;
}

bool IsPowerOfTwo(u32 value) {
  return value != 0 && (value & (value - 1U)) == 0;
}

bool CheckedAdd32(u32 a, u32 b, u32 *result) {
  if (result == nullptr || a > 0xFFFFFFFFU - b) {
    return false;
  }
  *result = a + b;
  return true;
}

bool CheckedMul32(u32 a, u32 b, u32 *result) {
  if (result == nullptr || (a != 0 && b > 0xFFFFFFFFU / a)) {
    return false;
  }
  *result = a * b;
  return true;
}

bool CheckedAlignUp32(u32 value, u32 alignment, u32 *result) {
  if (!IsPowerOfTwo(alignment) || value > 0xFFFFFFFFU - alignment + 1U) {
    return false;
  }
  *result = AlignUp32(value, alignment);
  return true;
}

bool BuildRgb565FragmentRenderTarget(
    Buffer &buffer,
    FragmentReplayRenderTarget *target) {
  if (target == nullptr || buffer.cpu == nullptr ||
      buffer.v3d_address == 0 || buffer.depth != 16 ||
      buffer.pitch == 0 || buffer.size == 0) {
    return false;
  }
  const FragmentReplayRenderTarget result = {
    &buffer,
    2,
    kV3d71MemoryFormatRaster,
    kV3d71OutputImageFormatBgr565,
    buffer.pitch,
    0,
    true,
    "rgb565-raster"
  };
  *target = result;
  return true;
}

bool BuildRgba8FragmentRenderTarget(
    Buffer &buffer,
    const v3d71::Rgba8TextureLayout &layout,
    FragmentReplayRenderTarget *target) {
  v3d71::Rgba8RenderTargetStoreConfig store = {};
  if (target == nullptr || buffer.cpu == nullptr ||
      buffer.v3d_address == 0 || buffer.depth != 32 ||
      buffer.width < layout.width || buffer.height < layout.height ||
      buffer.pitch != layout.padded_row_bytes ||
      buffer.size < layout.size_bytes ||
      !v3d71::GetRgba8RenderTargetStoreConfig(layout, &store)) {
    return false;
  }
  const FragmentReplayRenderTarget result = {
    &buffer,
    4,
    store.memory_format,
    kV3d71OutputImageFormatRgba8,
    store.height_in_ub_or_stride,
    0,
    false,
    "rgba8-v3d-tiled"
  };
  *target = result;
  return true;
}

bool FragmentRenderTargetSupportsGeometry(
    const FragmentReplayRenderTarget &target,
    const FragmentReplayGeometry &geometry) {
  if (target.buffer == nullptr || target.buffer->cpu == nullptr ||
      target.buffer->v3d_address == 0 || target.bytes_per_pixel == 0 ||
      geometry.width == 0 || geometry.height == 0 ||
      target.buffer->width < geometry.width ||
      target.buffer->height < geometry.height) {
    return false;
  }
  u32 visible_row_bytes = 0;
  u32 visible_bytes = 0;
  return CheckedMul32(geometry.width, target.bytes_per_pixel,
                      &visible_row_bytes) &&
         target.buffer->pitch >= visible_row_bytes &&
         CheckedMul32(target.buffer->pitch, geometry.height,
                      &visible_bytes) &&
         target.buffer->size >= visible_bytes;
}

u32 Min32(u32 a, u32 b) {
  return a < b ? a : b;
}

u16 Rgb565(u32 red, u32 green, u32 blue) {
  return (u16)(((red >> 3) << 11) |
               ((green >> 2) << 5) |
               (blue >> 3));
}

const char *SafeString(const char *value) {
  return value != nullptr ? value : "(null)";
}

const char *RenderResolutionName(RenderResolution resolution) {
  return resolution == kRenderResolutionOutput ? "output" : "source";
}

const char *ArtifactStageName(shader_artifacts::ShaderArtifactStage stage) {
  switch (stage) {
    case shader_artifacts::kArtifactStageFragment:
      return "fragment";
    case shader_artifacts::kArtifactStageVertex:
      return "vertex";
    case shader_artifacts::kArtifactStageCoordinate:
      return "coordinate";
    case shader_artifacts::kArtifactStageNone:
    default:
      return "none";
  }
}

const char *ArtifactPatchKindName(
    shader_artifacts::ShaderArtifactPatchKind kind) {
  switch (kind) {
    case shader_artifacts::kArtifactPatchGlShaderStateRecord:
      return "gl_shader_state_record";
    case shader_artifacts::kArtifactPatchShaderCode:
      return "shader_code";
    case shader_artifacts::kArtifactPatchShaderUniform:
      return "shader_uniform";
    case shader_artifacts::kArtifactPatchUniformWordAddressCandidate:
      return "uniform_word_address";
    case shader_artifacts::kArtifactPatchSamplerWordAddressCandidate:
      return "sampler_word_address";
    case shader_artifacts::kArtifactPatchShaderAttribute:
      return "shader_attribute";
    default:
      return "unknown";
  }
}

const char *ArtifactAddressEncodingName(
    shader_artifacts::ShaderArtifactAddressEncoding encoding) {
  switch (encoding) {
    case shader_artifacts::kArtifactAddressDirect:
      return "direct";
    case shader_artifacts::kArtifactAddressWordShiftedRight4:
      return "word_shifted_right_4";
    default:
      return "unknown";
  }
}

u32 ReadLe32(const uint8_t *src) {
  return ((u32)src[0]) |
         ((u32)src[1] << 8) |
         ((u32)src[2] << 16) |
         ((u32)src[3] << 24);
}

void WriteLe32(uint8_t *dst, u32 value) {
  dst[0] = (uint8_t)value;
  dst[1] = (uint8_t)(value >> 8);
  dst[2] = (uint8_t)(value >> 16);
  dst[3] = (uint8_t)(value >> 24);
}

u32 ReadReg(uintptr base, u32 offset) {
  return read32(base + offset);
}

void WriteReg(uintptr base, u32 offset, u32 value) {
  write32(base + offset, value);
}

u64 CpuToAxiBusAddress(const void *ptr) {
  return (u64)(uintptr)ptr;
}

bool WaitForRegClear(uintptr base, u32 offset, u32 mask,
                     unsigned timeout_us, const char *label) {
  const unsigned start = CTimer::GetClockTicks();

  while ((unsigned)(CTimer::GetClockTicks() - start) < timeout_us) {
    if ((ReadReg(base, offset) & mask) == 0) {
      return true;
    }
    CTimer::SimpleusDelay(10);
  }

  printf("boot: pi5v3d timeout waiting for %s clear reg=0x%04x "
         "mask=0x%08x value=0x%08x\r\n",
         label, offset, mask, ReadReg(base, offset));
  return false;
}

void ClPut8(CommandListWriter *cl, u32 value) {
  if (cl == nullptr || !cl->ok || cl->offset >= cl->capacity) {
    if (cl != nullptr) {
      cl->ok = false;
    }
    return;
  }

  cl->base[cl->offset++] = (uint8_t)value;
}

void ClPut16(CommandListWriter *cl, u32 value) {
  ClPut8(cl, value);
  ClPut8(cl, value >> 8);
}

void ClPut32(CommandListWriter *cl, u32 value) {
  ClPut8(cl, value);
  ClPut8(cl, value >> 8);
  ClPut8(cl, value >> 16);
  ClPut8(cl, value >> 24);
}

u32 FloatBits(float value) {
  u32 bits = 0;
  memcpy(&bits, &value, sizeof bits);
  return bits;
}

void ClPutFloat(CommandListWriter *cl, float value) {
  const u32 bits = FloatBits(value);
  ClPut32(cl, bits);
}

u32 ClGpuAddress(const Buffer &buffer, const CommandListWriter &cl) {
  return buffer.v3d_address + cl.offset;
}

u32 SolidTilesX() {
  return (kScratchTargetWidth + kSolidTileWidth - 1U) / kSolidTileWidth;
}

u32 SolidTilesY() {
  return (kScratchTargetHeight + kSolidTileHeight - 1U) / kSolidTileHeight;
}

u32 TilesForPixels(u32 pixels) {
  return (pixels + kSolidTileWidth - 1U) / kSolidTileWidth;
}

bool ComputeFragmentReplayTileScratchLayout(
    const FragmentReplayGeometry &geometry,
    FragmentReplayTileScratchLayout *layout) {
  if (layout == nullptr || geometry.width == 0 || geometry.height == 0 ||
      geometry.width > kOutputTargetMaxWidth ||
      geometry.height > kOutputTargetMaxHeight) {
    return false;
  }

  const u32 tiles_x = TilesForPixels(geometry.width);
  const u32 tiles_y = TilesForPixels(geometry.height);
  u32 tile_count = 0;
  u32 initial_tile_alloc = 0;
  u32 aligned_tile_alloc = 0;
  u32 tile_alloc_bytes = 0;
  u32 tsda_bytes = 0;
  u32 total_bytes = 0;
  if (!CheckedMul32(tiles_x, tiles_y, &tile_count) ||
      !CheckedMul32(tile_count, kFragmentTileAllocBytesPerTile,
                    &initial_tile_alloc) ||
      !CheckedAlignUp32(initial_tile_alloc, kV3dMmuPageSize,
                        &aligned_tile_alloc) ||
      !CheckedAdd32(aligned_tile_alloc, kFragmentTileAllocGuardBytes,
                    &tile_alloc_bytes) ||
      !CheckedAdd32(tile_alloc_bytes, kFragmentTileAllocExtraBytes,
                    &tile_alloc_bytes) ||
      !CheckedMul32(tile_count, kFragmentTileStateBytesPerTile,
                    &tsda_bytes)) {
    return false;
  }
  if (tsda_bytes < kFragmentTileStateMinimumBytes) {
    tsda_bytes = kFragmentTileStateMinimumBytes;
  }
  if (!CheckedAdd32(tile_alloc_bytes, tsda_bytes, &total_bytes)) {
    return false;
  }

  *layout = {
    geometry.width,
    geometry.height,
    tiles_x,
    tiles_y,
    tile_alloc_bytes,
    tile_alloc_bytes,
    tsda_bytes,
    total_bytes
  };
  return true;
}

float FragmentReplayClipperScaleX(u32 width) {
  return (float)width * 32.0f;
}

float FragmentReplayClipperScaleY(u32 height) {
  return -((float)height * 32.0f);
}

float FragmentReplayViewportOffsetX(u32 width) {
  return (float)width * 0.5f;
}

float FragmentReplayViewportOffsetY(u32 height) {
  return (float)height * 0.5f;
}

u32 RtStride128Bits() {
  return (kSolidTileWidth * 1U) / 2U;
}

void EmitTileRenderingModeCommon(CommandListWriter *cl) {
  ClPut8(cl, kV3d71TileRenderingModeCfg);
  ClPut8(cl, 0);  // sub-id 0, one render target.
  ClPut16(cl, kScratchTargetWidth);
  ClPut16(cl, kScratchTargetHeight);
  ClPut8(cl, 1U << 6);  // Early-Z disabled; no depth/stencil attachment.
  ClPut8(cl, (3U << 7) | (3U << 4));  // 64x64 tiles.
  ClPut8(cl, 1U);  // high bit of log2 tile height.
}

void EmitTileRenderingModeCommonForSize(CommandListWriter *cl,
                                        u32 width, u32 height) {
  ClPut8(cl, kV3d71TileRenderingModeCfg);
  ClPut8(cl, 0);  // sub-id 0, one render target.
  ClPut16(cl, width);
  ClPut16(cl, height);
  ClPut8(cl, 1U << 6);  // Early-Z disabled; no depth/stencil attachment.
  ClPut8(cl, (3U << 7) | (3U << 4));  // 64x64 tiles.
  ClPut8(cl, 1U);  // high bit of log2 tile height.
}

void EmitRenderTargetPart1(CommandListWriter *cl) {
  const u32 stride = RtStride128Bits();
  const u32 stride_minus_one = stride - 1U;

  ClPut8(cl, kV3d71TileRenderingModeCfg);
  ClPut8(cl, 2);  // sub-id 2, render target 0, base address 0.
  ClPut8(cl, 0);
  ClPut8(cl, (stride_minus_one << 2) & 0xFFU);
  ClPut8(cl, (kV3d71RenderTargetTypeClamp8 << 3) |
             (kV3d71InternalBpp32 << 1) |
             ((stride_minus_one >> 6) & 1U));
  ClPut32(cl, kSolidClearColorLowBits);
}

void EmitRenderTargetPart1ForSize(CommandListWriter *cl, u32 clear_color) {
  const u32 stride = RtStride128Bits();
  const u32 stride_minus_one = stride - 1U;

  ClPut8(cl, kV3d71TileRenderingModeCfg);
  ClPut8(cl, 2);  // sub-id 2, render target 0, base address 0.
  ClPut8(cl, 0);
  ClPut8(cl, (stride_minus_one << 2) & 0xFFU);
  ClPut8(cl, (kV3d71RenderTargetTypeClamp8 << 3) |
             (kV3d71InternalBpp32 << 1) |
             ((stride_minus_one >> 6) & 1U));
  ClPut32(cl, clear_color);
}

void EmitZsClearValues(CommandListWriter *cl) {
  ClPut8(cl, kV3d71TileRenderingModeCfg);
  ClPut8(cl, 1);  // sub-id 1.
  ClPut8(cl, 0);  // stencil clear value.
  ClPut32(cl, 0x3F800000U);  // z clear value 1.0f.
  ClPut16(cl, 0);
}

void EmitMulticoreSupertileCfg(CommandListWriter *cl) {
  const u32 tiles_x = SolidTilesX();
  const u32 tiles_y = SolidTilesY();

  ClPut8(cl, kV3d71MulticoreRenderingSupertileCfg);
  ClPut8(cl, 0);  // supertile width: 1 tile.
  ClPut8(cl, 0);  // supertile height: 1 tile.
  ClPut8(cl, tiles_x);
  ClPut8(cl, tiles_y);
  ClPut8(cl, tiles_x & 0xFFU);
  ClPut8(cl, ((tiles_y << 4) & 0xFFU) | ((tiles_x >> 8) & 0x0FU));
  ClPut8(cl, (tiles_y >> 4) & 0xFFU);
  ClPut8(cl, 0);  // one tile-list set, raster order off, multicore off.
}

void EmitMulticoreSupertileCfgForSize(CommandListWriter *cl,
                                      u32 width, u32 height) {
  const u32 tiles_x = TilesForPixels(width);
  const u32 tiles_y = TilesForPixels(height);

  ClPut8(cl, kV3d71MulticoreRenderingSupertileCfg);
  ClPut8(cl, 0);  // supertile width: 1 tile.
  ClPut8(cl, 0);  // supertile height: 1 tile.
  ClPut8(cl, tiles_x);
  ClPut8(cl, tiles_y);
  ClPut8(cl, tiles_x & 0xFFU);
  ClPut8(cl, ((tiles_y << 4) & 0xFFU) | ((tiles_x >> 8) & 0x0FU));
  ClPut8(cl, (tiles_y >> 4) & 0xFFU);
  ClPut8(cl, 0);  // one tile-list set, raster order off, multicore off.
}

void EmitTileListInitialBlockSize(CommandListWriter *cl) {
  ClPut8(cl, kV3d71TileListInitialBlockSize);
  ClPut8(cl, (1U << 2) | 1U);  // auto chaining, 128-byte first blocks.
}

void EmitTileListInitialBlockSize64(CommandListWriter *cl) {
  ClPut8(cl, kV3d71TileListInitialBlockSize);
  ClPut8(cl, 1U << 2);  // auto chaining, 64-byte first blocks.
}

void EmitTileListSetBase(CommandListWriter *cl, u32 address) {
  ClPut8(cl, kV3d71MulticoreRenderingTileListSetBase);
  ClPut32(cl, address);
}

void EmitTileCoordinates(CommandListWriter *cl, u32 tile_x, u32 tile_y) {
  ClPut8(cl, kV3d71TileCoordinates);
  ClPut8(cl, tile_x & 0xFFU);
  ClPut8(cl, ((tile_y << 4) & 0xFFU) | ((tile_x >> 8) & 0x0FU));
  ClPut8(cl, (tile_y >> 4) & 0xFFU);
}

void EmitSupertileCoordinates(CommandListWriter *cl,
                              u32 supertile_x, u32 supertile_y) {
  ClPut8(cl, kV3d71SupertileCoordinates);
  ClPut8(cl, supertile_x);
  ClPut8(cl, supertile_y);
}

void EmitStartAddressOfGenericTileList(CommandListWriter *cl,
                                       u32 start, u32 end) {
  ClPut8(cl, kV3d71StartAddressOfGenericTileList);
  ClPut32(cl, start);
  ClPut32(cl, end);
}

void EmitNumberOfLayers(CommandListWriter *cl, u32 layers) {
  ClPut8(cl, kV3d71NumberOfLayers);
  ClPut8(cl, layers > 0 ? layers - 1U : 0);
}

void EmitTileBinningModeCfg(CommandListWriter *cl, u32 width, u32 height) {
  ClPut8(cl, kV3d71TileBinningModeCfg);
  ClPut8(cl, 0);
  ClPut8(cl, (3U << 3) | 3U);  // 64x64 tiles.
  ClPut16(cl, 0);
  ClPut16(cl, width > 0 ? width - 1U : 0);
  ClPut16(cl, height > 0 ? height - 1U : 0);
}

void EmitOcclusionQueryCounter(CommandListWriter *cl, u32 address) {
  ClPut8(cl, kV3d71OcclusionQueryCounter);
  ClPut32(cl, address);
}

void EmitClipWindow(CommandListWriter *cl, u32 left, u32 bottom,
                    u32 width, u32 height) {
  ClPut8(cl, kV3d71ClipWindow);
  ClPut16(cl, left);
  ClPut16(cl, bottom);
  ClPut16(cl, width);
  ClPut16(cl, height);
}

void EmitFragmentReplayCfgBits(CommandListWriter *cl) {
  ClPut8(cl, kV3d71CfgBits);
  ClPut8(cl, (1U << 2) | (1U << 1) | 1U);
  ClPut8(cl, 7U << 4);  // Depth test ALWAYS, no depth updates.
  ClPut8(cl, 1U << 6);  // Z clipping MIN_ONE_TO_ONE.
}

void EmitFloatPacket(CommandListWriter *cl, u32 opcode, float value) {
  ClPut8(cl, opcode);
  ClPutFloat(cl, value);
}

void EmitClipperXyScaling(CommandListWriter *cl, float half_width,
                          float half_height) {
  ClPut8(cl, kV3d71ClipperXyScaling);
  ClPutFloat(cl, half_width);
  ClPutFloat(cl, half_height);
}

void EmitClipperZScaleOffset(CommandListWriter *cl, float scale,
                             float offset) {
  ClPut8(cl, kV3d71ClipperZScaleOffset);
  ClPutFloat(cl, scale);
  ClPutFloat(cl, offset);
}

void EmitClipperZMinMax(CommandListWriter *cl, float minimum, float maximum) {
  ClPut8(cl, kV3d71ClipperZMinMax);
  ClPutFloat(cl, minimum);
  ClPutFloat(cl, maximum);
}

void EmitViewportOffset(CommandListWriter *cl, float fine_x, int coarse_x,
                        float fine_y, int coarse_y) {
  const u32 fixed_x = (u32)(fine_x * 256.0f);
  const u32 fixed_y = (u32)(fine_y * 256.0f);
  ClPut8(cl, kV3d71ViewportOffset);
  ClPut8(cl, fixed_x);
  ClPut8(cl, fixed_x >> 8);
  ClPut8(cl, ((u32)coarse_x << 6) | (fixed_x >> 16));
  ClPut8(cl, ((u32)coarse_x >> 2) & 0xFFU);
  ClPut8(cl, fixed_y);
  ClPut8(cl, fixed_y >> 8);
  ClPut8(cl, ((u32)coarse_y << 6) | (fixed_y >> 16));
  ClPut8(cl, ((u32)coarse_y >> 2) & 0xFFU);
}

void EmitColorWriteMasks(CommandListWriter *cl, u32 mask) {
  ClPut8(cl, kV3d71ColorWriteMasks);
  ClPut32(cl, mask);
}

void EmitBlendConstantColor(CommandListWriter *cl) {
  ClPut8(cl, kV3d71BlendConstantColor);
  ClPut16(cl, 0);
  ClPut16(cl, 0);
  ClPut16(cl, 0);
  ClPut16(cl, 0);
}

void EmitTransformFeedbackSpecs(CommandListWriter *cl) {
  ClPut8(cl, kV3d71TransformFeedbackSpecs);
  ClPut8(cl, 0);
}

void EmitSampleState(CommandListWriter *cl) {
  ClPut8(cl, kV3d71SampleState);
  ClPut8(cl, 15);
  ClPut8(cl, 0);
  ClPut16(cl, 0x3F80U);  // f18.7 encoding of coverage 1.0.
}

void EmitVcmCacheSize(CommandListWriter *cl, u32 binning_batches,
                      u32 rendering_batches) {
  ClPut8(cl, kV3d71VcmCacheSize);
  ClPut8(cl, ((rendering_batches & 0x0FU) << 4) |
             (binning_batches & 0x0FU));
}

void EmitGlShaderState(CommandListWriter *cl, u32 record_address,
                       u32 attribute_arrays) {
  ClPut8(cl, kV3d71GlShaderState);
  ClPut32(cl, record_address | (attribute_arrays & 0x1FU));
}

void EmitVertexArrayPrims(CommandListWriter *cl, u32 mode, u32 length,
                          u32 first_vertex) {
  ClPut8(cl, kV3d71VertexArrayPrims);
  ClPut8(cl, mode);
  ClPut32(cl, length);
  ClPut32(cl, first_vertex);
}

void EmitPrimListFormat(CommandListWriter *cl) {
  ClPut8(cl, kV3d71PrimListFormat);
  ClPut8(cl, 2);  // List triangles.
}

void EmitSetInstanceId(CommandListWriter *cl, u32 instance_id) {
  ClPut8(cl, kV3d71SetInstanceId);
  ClPut32(cl, instance_id);
}

void EmitBranchToImplicitTileList(CommandListWriter *cl, u32 tile_list_set) {
  ClPut8(cl, kV3d71BranchToImplicitTileList);
  ClPut8(cl, tile_list_set);
}

void EmitStoreTileBufferGeneralConfigured(
    CommandListWriter *cl, u32 buffer_to_store, u32 address,
    u32 memory_format, u32 output_image_format,
    u32 height_in_ub_or_stride, u32 image_height,
    bool rb_swap, bool clear_buffer) {
  const u32 rb_swap_bits = rb_swap ? (1U << 4) : 0;

  ClPut8(cl, kV3d71StoreTileBufferGeneral);
  ClPut8(cl, (memory_format << 4) | (buffer_to_store & 0x0FU));
  ClPut8(cl, ((output_image_format << 4) & 0xFFU) |
             (kV3d71DecimateSample0 << 2) |
             kV3d71DitherNone);
  ClPut8(cl, rb_swap_bits | (clear_buffer ? (1U << 2) : 0U) |
             ((output_image_format >> 4) & 0x03U));
  ClPut8(cl, (height_in_ub_or_stride << 4) & 0xFFU);
  ClPut8(cl, (height_in_ub_or_stride >> 4) & 0xFFU);
  ClPut8(cl, (height_in_ub_or_stride >> 12) & 0xFFU);
  ClPut16(cl, image_height);
  ClPut32(cl, address);
}

void EmitStoreTileBufferGeneral(CommandListWriter *cl, u32 buffer_to_store,
                                u32 address, u32 stride,
                                bool clear_buffer) {
  EmitStoreTileBufferGeneralConfigured(
      cl, buffer_to_store, address, kV3d71MemoryFormatRaster,
      kV3d71OutputImageFormatBgr565, stride, 0,
      buffer_to_store == kV3d71StoreRenderTarget0, clear_buffer);
}

void EmitInitialTileClear(CommandListWriter *cl) {
  for (u32 i = 0; i < 2; ++i) {
    EmitTileCoordinates(cl, 0, 0);
    ClPut8(cl, kV3d71EndOfLoads);
    EmitStoreTileBufferGeneral(cl, kV3d71StoreNone, 0, 0, false);
    ClPut8(cl, kV3d71ClearRenderTargets);
    ClPut8(cl, kV3d71EndOfTileMarker);
  }
  ClPut8(cl, kV3d71FlushVcdCache);
}

bool BuildSolidGenericTileList(u32 *start, u32 *end) {
  if (start == nullptr || end == nullptr ||
      g_fragment_output.control_scratch.cpu == nullptr ||
      g_fragment_output.control_scratch.v3d_address == 0 ||
      kSolidGenericListOffset >= g_fragment_output.control_scratch.size) {
    return false;
  }

  CommandListWriter cl = {
    g_fragment_output.control_scratch.cpu + kSolidGenericListOffset,
    g_fragment_output.control_scratch.size - kSolidGenericListOffset,
    0,
    true
  };

  *start = g_fragment_output.control_scratch.v3d_address + kSolidGenericListOffset;
  ClPut8(&cl, kV3d71TileCoordinatesImplicit);
  ClPut8(&cl, kV3d71EndOfLoads);
  EmitStoreTileBufferGeneral(&cl, kV3d71StoreRenderTarget0,
                             g_target_scratch.v3d_address,
                             g_target_scratch.pitch, false);
  ClPut8(&cl, kV3d71ClearRenderTargets);
  ClPut8(&cl, kV3d71EndOfTileMarker);
  ClPut8(&cl, kV3d71ReturnFromSubList);
  *end = g_fragment_output.control_scratch.v3d_address + kSolidGenericListOffset + cl.offset;

  return cl.ok;
}

bool BuildSolidRcl(u32 *start, u32 *end, u32 *generic_start,
                   u32 *generic_end) {
  if (start == nullptr || end == nullptr || generic_start == nullptr ||
      generic_end == nullptr || g_fragment_output.control_scratch.cpu == nullptr ||
      g_fragment_output.control_scratch.v3d_address == 0 ||
      g_target_scratch.v3d_address == 0 ||
      kSolidRclOffset >= g_fragment_output.control_scratch.size ||
      kSolidGenericListOffset >= g_fragment_output.control_scratch.size ||
      kSolidTileListOffset >= g_fragment_output.control_scratch.size) {
    return false;
  }

  memset(g_fragment_output.control_scratch.cpu, 0, g_fragment_output.control_scratch.size);
  if (!BuildSolidGenericTileList(generic_start, generic_end)) {
    return false;
  }

  CommandListWriter rcl = {
    g_fragment_output.control_scratch.cpu + kSolidRclOffset,
    kSolidGenericListOffset - kSolidRclOffset,
    0,
    true
  };

  *start = ClGpuAddress(g_fragment_output.control_scratch, rcl);
  EmitTileRenderingModeCommon(&rcl);
  EmitRenderTargetPart1(&rcl);
  EmitZsClearValues(&rcl);
  EmitTileListInitialBlockSize(&rcl);
  EmitTileListSetBase(&rcl, g_fragment_output.control_scratch.v3d_address +
                            kSolidTileListOffset);
  EmitMulticoreSupertileCfg(&rcl);
  EmitInitialTileClear(&rcl);
  EmitStartAddressOfGenericTileList(&rcl, *generic_start, *generic_end);

  const u32 tiles_x = SolidTilesX();
  const u32 tiles_y = SolidTilesY();
  for (u32 y = 0; y < tiles_y; ++y) {
    for (u32 x = 0; x < tiles_x; ++x) {
      EmitSupertileCoordinates(&rcl, x, y);
    }
  }
  ClPut8(&rcl, kV3d71EndOfRendering);
  *end = ClGpuAddress(g_fragment_output.control_scratch, rcl);

  if (!rcl.ok) {
    return false;
  }

  CleanBufferForV3D(g_fragment_output.control_scratch);
  return true;
}

bool GetClockRate(u32 tag_id, u32 clock_id, u32 *rate) {
  if (rate == nullptr) {
    return false;
  }

  CBcmPropertyTags tags;
  TPropertyTagClockRate tag;
  memset(&tag, 0, sizeof tag);
  tag.nClockId = clock_id;
  if (!tags.GetTag(tag_id, &tag, sizeof tag, 4) || tag.nRate == 0) {
    return false;
  }

  *rate = tag.nRate;
  return true;
}

bool GetV3dClockRate(u32 *rate) {
  return GetClockRate(PROPTAG_GET_CLOCK_RATE_MEASURED, kClockV3d, rate) ||
         GetClockRate(PROPTAG_GET_CLOCK_RATE, kClockV3d, rate);
}

void PrintAddress(const char *name, uintptr address) {
  printf("%s=0x%08x%08x ", name, (u32)(address >> 32), (u32)address);
}

void PrintAddress64(const char *name, u64 address) {
  printf("%s=0x%08x%08x ", name, (u32)(address >> 32), (u32)address);
}

bool AllocateAlignedDma(u32 size, u32 alignment,
                        uint8_t **allocation, uint8_t **cpu,
                        u32 *allocation_size) {
  if (allocation == nullptr || cpu == nullptr ||
      allocation_size == nullptr || size == 0 || alignment == 0 ||
      !IsPowerOfTwo(alignment)) {
    return false;
  }

  const u32 alloc_size = size + alignment - 1U;
  uint8_t *raw = new (HEAP_DMA30) uint8_t[alloc_size];
  if (raw == nullptr) {
    return false;
  }

  uint8_t *aligned = (uint8_t *)AlignUpPtr((uintptr)raw, alignment);
  memset(aligned, 0, size);

  *allocation = raw;
  *cpu = aligned;
  *allocation_size = alloc_size;
  return true;
}

bool FlushV3dMmu() {
  if (g_page_table != nullptr) {
    CleanDataCacheRange((u64)(uintptr)g_page_table, kV3dPageTableBytes);
  }

  WriteReg(kV3dHubBase, kV3dMmuControl,
           kV3dMmuControlFlush | kV3dMmuControlEnable);
  if (!WaitForRegClear(kV3dHubBase, kV3dMmuControl,
                       kV3dMmuControlFlushing, 100,
                       "MMUC flush")) {
    return false;
  }

  WriteReg(kV3dHubBase, kV3dMmuCtl,
           ReadReg(kV3dHubBase, kV3dMmuCtl) | kV3dMmuCtlTlbClear);
  return WaitForRegClear(kV3dHubBase, kV3dMmuCtl,
                         kV3dMmuCtlTlbClearing, 100,
                         "MMU TLB clear");
}

void InitializeCoreState() {
  WriteReg(kV3dCore0Base, kV3dCoreL2TFlushStart, 0);
  WriteReg(kV3dCore0Base, kV3dCoreL2TFlushEnd, ~0U);
  WriteReg(kV3dCore0Base, kV3dCoreIntClear, 0xFFFFFFFFU);
  WriteReg(kV3dHubBase, kV3dHubIntClear, 0xFFFFFFFFU);
}

void InvalidateV3dCaches() {
  WriteReg(kV3dCore0Base, kV3dCoreL2TCacheCtl, kV3dL2TCacheFlush);
  WriteReg(kV3dCore0Base, kV3dCoreSliceCacheCtl,
           kV3dSliceCacheInvalidateAll);
}

u32 CurrentMmuFaults() {
  return ReadReg(kV3dHubBase, kV3dHubIntStatus) &
         (kV3dHubIntMmuWriteViolation |
          kV3dHubIntMmuPtInvalid |
          kV3dHubIntMmuCapExceeded);
}

u32 ScaledMmuVioAddr(u32 raw_vio_addr) {
  return raw_vio_addr << 4;
}

void LogRenderStatus(const char *phase, u32 rcl_start, u32 rcl_end) {
  const u32 hub_int = ReadReg(kV3dHubBase, kV3dHubIntStatus);
  const u32 core_int = ReadReg(kV3dCore0Base, kV3dCoreIntStatus);
  const u32 mmu_ctl = ReadReg(kV3dHubBase, kV3dMmuCtl);
  const u32 vio_id = ReadReg(kV3dHubBase, kV3dMmuVioId);
  const u32 vio_addr = ReadReg(kV3dHubBase, kV3dMmuVioAddr);
  const u32 ct1cs = ReadReg(kV3dCore0Base, kV3dCleCt1Cs);
  const u32 ct1ca = ReadReg(kV3dCore0Base, kV3dCleCt1Ca);
  const u32 ct1ea = ReadReg(kV3dCore0Base, kV3dCleCt1Ea);
  const u32 ct1pc = ReadReg(kV3dCore0Base, kV3dCleCt1Pc);
  const u32 mmu_faults = hub_int & (kV3dHubIntMmuWriteViolation |
                                    kV3dHubIntMmuPtInvalid |
                                    kV3dHubIntMmuCapExceeded);

  printf("boot: pi5v3d solid %s rcl=0x%08x..0x%08x "
         "hub_int=0x%08x core_int=0x%08x mmu_faults=0x%08x "
         "mmu_ctl=0x%08x vio_id=0x%08x vio_addr=0x%08x "
         "vio_addr_x16=0x%08x "
         "ct1cs=0x%08x ct1ca=0x%08x ct1ea=0x%08x ct1pc=0x%08x\r\n",
         phase, rcl_start, rcl_end, hub_int, core_int, mmu_faults,
         mmu_ctl, vio_id, vio_addr, ScaledMmuVioAddr(vio_addr),
         ct1cs, ct1ca, ct1ea, ct1pc);
}

bool WaitForRenderDone(u32 rcl_start, u32 rcl_end, unsigned timeout_us,
                       bool log_success) {
  const unsigned start_ticks = CTimer::GetClockTicks();

  while ((unsigned)(CTimer::GetClockTicks() - start_ticks) < timeout_us) {
    const u32 core_int = ReadReg(kV3dCore0Base, kV3dCoreIntStatus);
    const u32 mmu_faults = CurrentMmuFaults();

    if (mmu_faults != 0 || (core_int & kV3dIntRenderError) != 0) {
      LogRenderStatus("fault", rcl_start, rcl_end);
      return false;
    }

    if ((core_int & kV3dIntFrDone) != 0) {
      if (log_success) {
        LogRenderStatus("done", rcl_start, rcl_end);
      }
      return true;
    }

    CTimer::SimpleusDelay(10);
  }

  LogRenderStatus("timeout", rcl_start, rcl_end);
  return false;
}

bool SubmitSolidRcl(u32 rcl_start, u32 rcl_end, bool log_success) {
  if (rcl_start == 0 || rcl_end <= rcl_start) {
    return false;
  }

  WriteReg(kV3dCore0Base, kV3dCoreIntClear, 0xFFFFFFFFU);
  WriteReg(kV3dHubBase, kV3dHubIntClear, 0xFFFFFFFFU);
  InvalidateV3dCaches();
  DataSyncBarrier();

  if (log_success) {
    LogRenderStatus("submit", rcl_start, rcl_end);
  }
  WriteReg(kV3dCore0Base, kV3dCleCt1Qba, rcl_start);
  DataSyncBarrier();
  WriteReg(kV3dCore0Base, kV3dCleCt1Qea, rcl_end);

  const bool ok = WaitForRenderDone(rcl_start, rcl_end, 200000, log_success);
  WriteReg(kV3dCore0Base, kV3dCoreIntClear, 0xFFFFFFFFU);
  WriteReg(kV3dHubBase, kV3dHubIntClear, 0xFFFFFFFFU);
  return ok;
}

u32 QpuMagicStoreCodeBytes() {
  return (u32)(sizeof kQpuMagicStoreCode);
}

u32 QpuMagicStoreCodeWords() {
  return (u32)(sizeof kQpuMagicStoreCode / sizeof kQpuMagicStoreCode[0]);
}

u32 QpuFillRgb565WordsCodeBytes() {
  return (u32)(sizeof kQpuFillRgb565WordsCode);
}

u32 QpuFillRgb565WordsCodeWords() {
  return (u32)(sizeof kQpuFillRgb565WordsCode /
               sizeof kQpuFillRgb565WordsCode[0]);
}

u32 QpuCopyRgb565RowsCodeBytes() {
  return (u32)(sizeof kQpuCopyRgb565RowsCode);
}

u32 QpuCopyRgb565RowsCodeWords() {
  return (u32)(sizeof kQpuCopyRgb565RowsCode /
               sizeof kQpuCopyRgb565RowsCode[0]);
}

u32 QpuScanlineRgb565RowsCodeBytes() {
  return (u32)(sizeof kQpuScanlineRgb565RowsCode);
}

u32 QpuScanlineRgb565RowsCodeWords() {
  return (u32)(sizeof kQpuScanlineRgb565RowsCode /
               sizeof kQpuScanlineRgb565RowsCode[0]);
}

const u64 *QpuFrameProgramCode(QpuFrameProgram program) {
  return program == kQpuFrameProgramScanlines ?
      kQpuScanlineRgb565RowsCode : kQpuCopyRgb565RowsCode;
}

u32 QpuFrameProgramCodeBytes(QpuFrameProgram program) {
  return program == kQpuFrameProgramScanlines ?
      QpuScanlineRgb565RowsCodeBytes() : QpuCopyRgb565RowsCodeBytes();
}

u32 QpuFrameProgramCodeWords(QpuFrameProgram program) {
  return program == kQpuFrameProgramScanlines ?
      QpuScanlineRgb565RowsCodeWords() : QpuCopyRgb565RowsCodeWords();
}

u32 QpuFrameProgramUniformWords(QpuFrameProgram program) {
  return program == kQpuFrameProgramScanlines ?
      kQpuFrameScanlineUniformWords : kQpuFrameCopyUniformWords;
}

const char *QpuFrameProgramLogName(QpuFrameProgram program) {
  return program == kQpuFrameProgramScanlines ?
      "qpu-scanlines" : "qpu-copy";
}

bool RuntimeQpuBufferContainsFrame(const Buffer &buffer,
                                   u32 staged_height,
                                   u32 row_bytes,
                                   const char *reason_name,
                                   const char **reason) {
  const u64 required_size =
      (u64)(staged_height - 1U) * buffer.pitch + row_bytes;
  if (required_size > buffer.size) {
    if (reason != nullptr) {
      *reason = reason_name;
    }
    return false;
  }
  return true;
}

bool RuntimeQpuTargetSupportsFrame(const Buffer &target_buffer,
                                   u32 staged_width,
                                   u32 staged_height,
                                   u32 row_bytes,
                                   const char **reason) {
  if (target_buffer.cpu == nullptr || target_buffer.v3d_address == 0 ||
      target_buffer.depth != 16) {
    if (reason != nullptr) {
      *reason = "target-buffer";
    }
    return false;
  }
  if (staged_width > target_buffer.width ||
      staged_height > target_buffer.height) {
    if (reason != nullptr) {
      *reason = "target-size";
    }
    return false;
  }
  if (target_buffer.pitch < row_bytes) {
    if (reason != nullptr) {
      *reason = "target-pitch";
    }
    return false;
  }
  if (!RuntimeQpuBufferContainsFrame(target_buffer, staged_height, row_bytes,
                                     "target-bytes", reason)) {
    return false;
  }
  return true;
}

bool RuntimeQpuFrameGeometrySupported(QpuFrameProgram program,
                                      u32 staged_width,
                                      u32 staged_height,
                                      const char **reason) {
  if (program != kQpuFrameProgramCopy &&
      program != kQpuFrameProgramScanlines) {
    if (reason != nullptr) {
      *reason = "program";
    }
    return false;
  }
  if (staged_width == 0 || staged_height == 0) {
    if (reason != nullptr) {
      *reason = "empty";
    }
    return false;
  }
  if (g_source_scratch.cpu == nullptr ||
      g_source_scratch.v3d_address == 0 ||
      g_source_scratch.depth != 16) {
    if (reason != nullptr) {
      *reason = "source-buffer";
    }
    return false;
  }
  if (staged_width > g_source_scratch.width ||
      staged_height > g_source_scratch.height) {
    if (reason != nullptr) {
      *reason = "source-size";
    }
    return false;
  }
  if (staged_width > 0xFFFFFFFFU / kQpuFrameCopyBytesPerPixel) {
    if (reason != nullptr) {
      *reason = "row-overflow";
    }
    return false;
  }

  const u32 row_bytes = staged_width * kQpuFrameCopyBytesPerPixel;
  if (row_bytes == 0 || (row_bytes % kQpuFillBytesPerGroup) != 0) {
    if (reason != nullptr) {
      *reason = "row-alignment";
    }
    return false;
  }
  if (g_source_scratch.pitch < row_bytes) {
    if (reason != nullptr) {
      *reason = "source-pitch";
    }
    return false;
  }
  if (!RuntimeQpuBufferContainsFrame(g_source_scratch, staged_height,
                                     row_bytes, "source-bytes", reason)) {
    return false;
  }
  return RuntimeQpuTargetSupportsFrame(g_target_scratch, staged_width,
                                       staged_height, row_bytes, reason) &&
         RuntimeQpuTargetSupportsFrame(g_target_scratch_alt, staged_width,
                                       staged_height, row_bytes, reason);
}

void LogRuntimeQpuGeometryUnsupportedIfNeeded(QpuFrameProgram program,
                                              u32 staged_width,
                                              u32 staged_height,
                                              const char *reason) {
  if (g_qpu_geometry_unsupported_logged) {
    return;
  }

  const u32 row_bytes =
      staged_width <= 0xFFFFFFFFU / kQpuFrameCopyBytesPerPixel ?
          staged_width * kQpuFrameCopyBytesPerPixel : 0U;
  printf("boot: pi5v3d runtime qpu geometry unsupported "
         "reason=%s program=%s staged=%ux%u row_bytes=%u "
         "source=%ux%u pitch=%u target0=%ux%u pitch=%u "
         "target1=%ux%u pitch=%u align_bytes=%u\r\n",
         reason != nullptr ? reason : "unknown",
         QpuFrameProgramLogName(program),
         staged_width,
         staged_height,
         row_bytes,
         g_source_scratch.width,
         g_source_scratch.height,
         g_source_scratch.pitch,
         g_target_scratch.width,
         g_target_scratch.height,
         g_target_scratch.pitch,
         g_target_scratch_alt.width,
         g_target_scratch_alt.height,
         g_target_scratch_alt.pitch,
         kQpuFillBytesPerGroup);
  g_qpu_geometry_unsupported_logged = true;
}

const char *HvsScaleFilterName(pi5kms::ScaleFilter filter) {
  return filter == pi5kms::kScaleFilterMitchell ? "mitchell" : "nearest";
}

Buffer &CurrentQpuTargetBuffer() {
  return g_qpu_target_buffer_index == 0 ? g_target_scratch
                                        : g_target_scratch_alt;
}

Buffer &SelectNextQpuTargetBuffer() {
  g_qpu_target_buffer_index ^= 1U;
  return CurrentQpuTargetBuffer();
}

u32 QpuTargetBufferIndex(const Buffer &buffer) {
  return &buffer == &g_target_scratch_alt ? 1U : 0U;
}

void ClearCompletedRenderTarget() {
  g_last_completed_target = nullptr;
  g_last_completed_width = 0;
  g_last_completed_height = 0;
}

void RecordCompletedRenderTarget(const OutputTarget &target) {
  Buffer &buffer = CurrentQpuTargetBuffer();
  u32 width = target.destination_rect.width;
  u32 height = target.destination_rect.height;
  if (target.rendered_plane != nullptr &&
      target.rendered_plane->framebuffer_bus_address != 0) {
    width = target.rendered_plane->source.width;
    height = target.rendered_plane->source.height;
  }
  if (buffer.cpu == nullptr || buffer.depth != 16 || width == 0 ||
      height == 0 || width > buffer.width || height > buffer.height ||
      buffer.pitch < width * 2U) {
    ClearCompletedRenderTarget();
    return;
  }
  g_last_completed_target = &buffer;
  g_last_completed_width = width;
  g_last_completed_height = height;
}

u32 DuplicateRgb565Word(u16 rgb) {
  return ((u32)rgb << 16) | rgb;
}

int ClampedScanlineGapX100(const RenderParams &params) {
  int gap_x100 = ParamX100(params.scanline_gap_brightness);
  if (gap_x100 < 0) {
    gap_x100 = 0;
  } else if (gap_x100 > 100) {
    gap_x100 = 100;
  }
  return gap_x100;
}

int ClampedScanlineWeightX100(const RenderParams &params) {
  int weight_x100 = ParamX100(params.scanline_weight);
  if (weight_x100 < 0) {
    weight_x100 = 0;
  } else if (weight_x100 > kQpuFrameScanlineMaxWeightX100) {
    weight_x100 = kQpuFrameScanlineMaxWeightX100;
  }
  return weight_x100;
}

u32 ScanlineBrightnessX16(const RenderParams &params) {
  const int gap_x100 = ClampedScanlineGapX100(params);
  const int weight_x100 = ClampedScanlineWeightX100(params);

  // The temporary QPU scanline program can only darken alternating rows, while
  // the GLSL parameter controls scanline width. Approximate it as a blend
  // strength toward the requested gap brightness: weight 0 leaves rows
  // unchanged, weight 15 applies the full gap brightness.
  const int darkening_x100 =
      ((100 - gap_x100) * weight_x100 +
       kQpuFrameScanlineMaxWeightX100 / 2) /
      kQpuFrameScanlineMaxWeightX100;
  const int brightness_x100 = 100 - darkening_x100;
  u32 scale =
      (u32)((brightness_x100 * (int)kQpuFrameScanlineScaleDenominator + 50) /
            100);
  if (scale > kQpuFrameScanlineScaleDenominator) {
    scale = kQpuFrameScanlineScaleDenominator;
  }
  return scale;
}

void ResetQpuScanlineParamLog() {
  g_qpu_scanline_param_log_valid = false;
  g_qpu_scanline_log_weight_x100 = 0;
  g_qpu_scanline_log_gap_x100 = 0;
  g_qpu_scanline_log_scale_x16 = 0;
  g_qpu_scanline_log_half_mask = 0;
  g_qpu_scanline_log_quarter_mask = 0;
  g_qpu_scanline_log_eighth_mask = 0;
  g_qpu_scanline_log_sixteenth_mask = 0;
}

void LogQpuFrameProgramIfChanged(QpuFrameProgram program,
                                 const QpuFrameJob &job) {
  if (g_qpu_frame_program_log_valid &&
      g_qpu_frame_program_log_program == (int)program) {
    return;
  }

  g_qpu_frame_program_log_valid = true;
  g_qpu_frame_program_log_program = (int)program;

  if (program == kQpuFrameProgramScanlines) {
    printf("boot: pi5v3d frame qpu program=%s copy=%ux%u "
           "groups_per_row=%u scanline_x16=%u "
           "dark_masks=0x%08x,0x%08x,0x%08x,0x%08x\r\n",
           QpuFrameProgramLogName(program),
           job.width,
           job.height,
           job.groups_per_row,
           job.scanline_scale_x16,
           job.scanline_half_mask,
           job.scanline_quarter_mask,
           job.scanline_eighth_mask,
           job.scanline_sixteenth_mask);
    return;
  }

  printf("boot: pi5v3d frame qpu program=%s copy=%ux%u "
         "groups_per_row=%u\r\n",
         QpuFrameProgramLogName(program),
         job.width,
         job.height,
         job.groups_per_row);
}

void LogQpuScanlineParamsIfChanged(QpuFrameProgram program,
                                   const QpuFrameJob &job) {
  if (program != kQpuFrameProgramScanlines) {
    return;
  }

  if (g_qpu_scanline_param_log_valid &&
      g_qpu_scanline_log_scale_x16 == job.scanline_scale_x16 &&
      g_qpu_scanline_log_half_mask == job.scanline_half_mask &&
      g_qpu_scanline_log_quarter_mask == job.scanline_quarter_mask &&
      g_qpu_scanline_log_eighth_mask == job.scanline_eighth_mask &&
      g_qpu_scanline_log_sixteenth_mask == job.scanline_sixteenth_mask) {
    if (g_qpu_scanline_log_weight_x100 != job.scanline_weight_x100 ||
        g_qpu_scanline_log_gap_x100 != job.scanline_gap_x100) {
      printf("boot: pi5v3d frame qpu-scanlines params quantized-unchanged "
             "scanline_weight_x100=%d gap_x100=%d "
             "previous_weight_x100=%d previous_gap_x100=%d "
             "scanline_x16=%u "
             "dark_masks=0x%08x,0x%08x,0x%08x,0x%08x\r\n",
             job.scanline_weight_x100,
             job.scanline_gap_x100,
             g_qpu_scanline_log_weight_x100,
             g_qpu_scanline_log_gap_x100,
             job.scanline_scale_x16,
             job.scanline_half_mask,
             job.scanline_quarter_mask,
             job.scanline_eighth_mask,
             job.scanline_sixteenth_mask);
      g_qpu_scanline_log_weight_x100 = job.scanline_weight_x100;
      g_qpu_scanline_log_gap_x100 = job.scanline_gap_x100;
    }
    return;
  }

  g_qpu_scanline_param_log_valid = true;
  g_qpu_scanline_log_weight_x100 = job.scanline_weight_x100;
  g_qpu_scanline_log_gap_x100 = job.scanline_gap_x100;
  g_qpu_scanline_log_scale_x16 = job.scanline_scale_x16;
  g_qpu_scanline_log_half_mask = job.scanline_half_mask;
  g_qpu_scanline_log_quarter_mask = job.scanline_quarter_mask;
  g_qpu_scanline_log_eighth_mask = job.scanline_eighth_mask;
  g_qpu_scanline_log_sixteenth_mask = job.scanline_sixteenth_mask;

  printf("boot: pi5v3d frame qpu-scanlines params "
         "scanline_weight_x100=%d gap_x100=%d scanline_x16=%u "
         "dark_masks=0x%08x,0x%08x,0x%08x,0x%08x\r\n",
         job.scanline_weight_x100,
         job.scanline_gap_x100,
         job.scanline_scale_x16,
         job.scanline_half_mask,
         job.scanline_quarter_mask,
         job.scanline_eighth_mask,
         job.scanline_sixteenth_mask);
}

u32 ScanlineTermMask(u32 scale_x16, u32 bit, u16 mask) {
  return (scale_x16 & bit) != 0 ? DuplicateRgb565Word(mask) : 0U;
}

bool ScanlineControlsDisableEffect(const RenderParams &params) {
  return ClampedScanlineWeightX100(params) <= 0 ||
         ClampedScanlineGapX100(params) >= 100;
}

QpuFrameProgram SelectedQpuFrameProgram(const RenderParams &params) {
  if (g_shader_preset == kShaderScanlines && params.enable_scanlines &&
      !ScanlineControlsDisableEffect(params)) {
    return kQpuFrameProgramScanlines;
  }
  return kQpuFrameProgramCopy;
}

bool BuildQpuFrameJob(QpuFrameProgram program,
                      u32 staged_width,
                      u32 staged_height,
                      const Buffer &target_buffer,
                      const RenderParams &params,
                      QpuFrameJob *job) {
  if (job == nullptr || g_source_scratch.cpu == nullptr ||
      target_buffer.cpu == nullptr || staged_width == 0 ||
      staged_height == 0 || staged_width > target_buffer.width ||
      staged_height > target_buffer.height) {
    return false;
  }

  const u32 row_bytes = staged_width * kQpuFrameCopyBytesPerPixel;
  if (row_bytes == 0 ||
      (row_bytes % kQpuFillBytesPerGroup) != 0 ||
      g_source_scratch.pitch < row_bytes ||
      target_buffer.pitch < row_bytes) {
    return false;
  }

  QpuFrameJob built = {
    staged_width,
    staged_height,
    row_bytes,
    row_bytes / kQpuFillBytesPerGroup,
    g_source_scratch.pitch - row_bytes,
    target_buffer.pitch - row_bytes,
    0,
    0,
    0,
    0,
    0,
    ClampedScanlineWeightX100(params),
    ClampedScanlineGapX100(params)
  };
  if (program == kQpuFrameProgramScanlines) {
    built.scanline_scale_x16 = ScanlineBrightnessX16(params);
    if (built.scanline_scale_x16 >= kQpuFrameScanlineScaleDenominator) {
      built.scanline_scale_x16 = kQpuFrameScanlineScaleDenominator - 1U;
    }
    built.scanline_half_mask =
        ScanlineTermMask(built.scanline_scale_x16, 8,
                         kQpuFrameScanlineHalfMask);
    built.scanline_quarter_mask =
        ScanlineTermMask(built.scanline_scale_x16, 4,
                         kQpuFrameScanlineQuarterMask);
    built.scanline_eighth_mask =
        ScanlineTermMask(built.scanline_scale_x16, 2,
                         kQpuFrameScanlineEighthMask);
    built.scanline_sixteenth_mask =
        ScanlineTermMask(built.scanline_scale_x16, 1,
                         kQpuFrameScanlineSixteenthMask);
  }

  *job = built;
  return true;
}

void LogCsdStatus(const char *phase, const u32 cfg[7]) {
  const u32 hub_int = ReadReg(kV3dHubBase, kV3dHubIntStatus);
  const u32 core_int = ReadReg(kV3dCore0Base, kV3dCoreIntStatus);
  const u32 status = ReadReg(kV3dCore0Base, kV3dCsdStatus);
  const u32 current0 = ReadReg(kV3dCore0Base, kV3dCsdCurrentCfg0);
  const u32 current1 = ReadReg(kV3dCore0Base, kV3dCsdCurrentCfg1);
  const u32 current2 = ReadReg(kV3dCore0Base, kV3dCsdCurrentCfg2);
  const u32 current3 = ReadReg(kV3dCore0Base, kV3dCsdCurrentCfg3);
  const u32 current4 = ReadReg(kV3dCore0Base, kV3dCsdCurrentCfg4);
  const u32 current5 = ReadReg(kV3dCore0Base, kV3dCsdCurrentCfg5);
  const u32 current6 = ReadReg(kV3dCore0Base, kV3dCsdCurrentCfg6);
  const u32 current7 = ReadReg(kV3dCore0Base, kV3dCsdCurrentCfg7);
  const u32 vio_id = ReadReg(kV3dHubBase, kV3dMmuVioId);
  const u32 vio_addr = ReadReg(kV3dHubBase, kV3dMmuVioAddr);
  const u32 mmu_faults = hub_int & (kV3dHubIntMmuWriteViolation |
                                    kV3dHubIntMmuPtInvalid |
                                    kV3dHubIntMmuCapExceeded);
  printf("boot: pi5v3d qpu %s cfg=%08x,%08x,%08x,%08x,%08x,%08x,%08x "
         "status=0x%08x hub_int=0x%08x core_int=0x%08x "
         "mmu_faults=0x%08x qpu_int=0x%08x vio_id=0x%08x vio_addr=0x%08x "
         "current=%08x,%08x,%08x,%08x,%08x,%08x,%08x,%08x\r\n",
         phase,
         cfg[0], cfg[1], cfg[2], cfg[3], cfg[4], cfg[5], cfg[6],
         status, hub_int, core_int, mmu_faults, core_int & kV3dIntQpuMask,
         vio_id, vio_addr,
         current0, current1, current2, current3,
         current4, current5, current6, current7);
}

bool CleanV3dCachesAfterTmuWrite() {
  WriteReg(kV3dCore0Base, kV3dCoreL2TCacheCtl,
           kV3dL2TCacheTmuWriteCombinerFlush);
  if (!WaitForRegClear(kV3dCore0Base, kV3dCoreL2TCacheCtl,
                       kV3dL2TCacheTmuWriteCombinerFlush, 100,
                       "TMU write combiner flush")) {
    return false;
  }

  WriteReg(kV3dCore0Base, kV3dCoreL2TCacheCtl,
           kV3dL2TCacheFlush | kV3dL2TCacheFlushModeClean);
  return WaitForRegClear(kV3dCore0Base, kV3dCoreL2TCacheCtl,
                         kV3dL2TCacheFlush, 100,
                         "L2T clean");
}

bool WaitForQpuDone(const u32 cfg[7], unsigned timeout_us, bool log_success) {
  const unsigned start_ticks = CTimer::GetClockTicks();

  while ((unsigned)(CTimer::GetClockTicks() - start_ticks) < timeout_us) {
    const u32 core_int = ReadReg(kV3dCore0Base, kV3dCoreIntStatus);
    const u32 mmu_faults = CurrentMmuFaults();

    if (mmu_faults != 0 || (core_int & kV3dIntQpuMask) != 0) {
      LogCsdStatus("fault", cfg);
      return false;
    }

    if ((core_int & kV3dIntCsdDone) != 0) {
      if (log_success) {
        LogCsdStatus("done", cfg);
      }
      return true;
    }

    CTimer::SimpleusDelay(10);
  }

  LogCsdStatus("timeout", cfg);
  return false;
}

bool BuildQpuMagicStoreCsd(u32 cfg[7]) {
  if (cfg == nullptr || g_fragment_output.control_scratch.cpu == nullptr ||
      g_fragment_output.control_scratch.v3d_address == 0 ||
      g_target_scratch.cpu == nullptr ||
      g_target_scratch.v3d_address == 0 ||
      kQpuTestCodeOffset + QpuMagicStoreCodeBytes() >
          g_fragment_output.control_scratch.size ||
      kQpuTestUniformOffset + kQpuTestUniformWords * sizeof(u32) >
          g_fragment_output.control_scratch.size ||
      (kQpuTestCodeOffset & 7U) != 0 ||
      (kQpuTestUniformOffset & 3U) != 0) {
    return false;
  }

  memset(g_target_scratch.cpu, 0, g_target_scratch.size);
  uint64_t *code =
      (uint64_t *)(g_fragment_output.control_scratch.cpu + kQpuTestCodeOffset);
  for (u32 i = 0; i < QpuMagicStoreCodeWords(); ++i) {
    code[i] = kQpuMagicStoreCode[i];
  }

  u32 *uniforms =
      (u32 *)(g_fragment_output.control_scratch.cpu + kQpuTestUniformOffset);
  uniforms[0] = g_target_scratch.v3d_address + kQpuTestOutputOffset;
  uniforms[1] = kQpuTestMagic;

  memset(cfg, 0, sizeof(u32) * 7);
  cfg[0] = 1U << 16;
  cfg[1] = 1U << 16;
  cfg[2] = 1U << 16;
  cfg[3] = ((kQpuTestNumBatches - 1U) << 12) |
           (kQpuTestWorkgroupsPerSupergroup << 8) |
           (kQpuTestWorkgroupSize & 0xFFU);
  cfg[4] = kQpuTestNumBatches;
  cfg[5] = g_fragment_output.control_scratch.v3d_address + kQpuTestCodeOffset;
  cfg[6] = g_fragment_output.control_scratch.v3d_address + kQpuTestUniformOffset;

  CleanBufferForV3D(g_fragment_output.control_scratch);
  CleanBufferForV3D(g_target_scratch);
  return true;
}

bool BuildQpuFillCsd(u32 cfg[7]) {
  if (cfg == nullptr || g_fragment_output.control_scratch.cpu == nullptr ||
      g_fragment_output.control_scratch.v3d_address == 0 ||
      g_target_scratch.cpu == nullptr ||
      g_target_scratch.v3d_address == 0 ||
      kQpuTestCodeOffset + QpuFillRgb565WordsCodeBytes() >
          g_fragment_output.control_scratch.size ||
      kQpuTestUniformOffset + kQpuFillUniformWords * sizeof(u32) >
          g_fragment_output.control_scratch.size ||
      (kQpuTestCodeOffset & 7U) != 0 ||
      (kQpuTestUniformOffset & 3U) != 0 ||
      g_target_scratch.size == 0 ||
      (g_target_scratch.size % kQpuFillBytesPerGroup) != 0) {
    return false;
  }

  const u32 groups = g_target_scratch.size / kQpuFillBytesPerGroup;
  memset(g_target_scratch.cpu, 0, g_target_scratch.size);
  uint64_t *code =
      (uint64_t *)(g_fragment_output.control_scratch.cpu + kQpuTestCodeOffset);
  for (u32 i = 0; i < QpuFillRgb565WordsCodeWords(); ++i) {
    code[i] = kQpuFillRgb565WordsCode[i];
  }

  u32 *uniforms =
      (u32 *)(g_fragment_output.control_scratch.cpu + kQpuTestUniformOffset);
  uniforms[0] = groups;
  uniforms[1] = g_target_scratch.v3d_address + kQpuTestOutputOffset;
  uniforms[2] = kQpuFillColorWord;

  memset(cfg, 0, sizeof(u32) * 7);
  cfg[0] = 1U << 16;
  cfg[1] = 1U << 16;
  cfg[2] = 1U << 16;
  cfg[3] = ((kQpuTestNumBatches - 1U) << 12) |
           (kQpuTestWorkgroupsPerSupergroup << 8) |
           (kQpuTestWorkgroupSize & 0xFFU);
  cfg[4] = kQpuTestNumBatches;
  cfg[5] = g_fragment_output.control_scratch.v3d_address + kQpuTestCodeOffset;
  cfg[6] = g_fragment_output.control_scratch.v3d_address + kQpuTestUniformOffset;

  CleanBufferForV3D(g_fragment_output.control_scratch);
  CleanBufferForV3D(g_target_scratch);
  return true;
}

bool BuildQpuFrameCsd(QpuFrameProgram program,
                      const QpuFrameJob &job,
                      const Buffer &target_buffer,
                      u32 cfg[7]) {
  const u32 code_bytes = QpuFrameProgramCodeBytes(program);
  const u32 code_words = QpuFrameProgramCodeWords(program);
  const u32 uniform_words = QpuFrameProgramUniformWords(program);
  if (cfg == nullptr || g_fragment_output.control_scratch.cpu == nullptr ||
      g_fragment_output.control_scratch.v3d_address == 0 ||
      g_source_scratch.cpu == nullptr ||
      g_source_scratch.v3d_address == 0 ||
      target_buffer.cpu == nullptr ||
      target_buffer.v3d_address == 0 ||
      kQpuTestCodeOffset + code_bytes > g_fragment_output.control_scratch.size ||
      kQpuTestUniformOffset + uniform_words * sizeof(u32) >
          g_fragment_output.control_scratch.size ||
      (kQpuTestCodeOffset & 7U) != 0 ||
      (kQpuTestUniformOffset & 3U) != 0 ||
      job.width == 0 ||
      job.height == 0 ||
      job.row_bytes == 0 ||
      job.groups_per_row == 0 ||
      job.width > target_buffer.width ||
      job.height > target_buffer.height ||
      g_source_scratch.pitch < job.row_bytes ||
      target_buffer.pitch < job.row_bytes ||
      (job.row_bytes % kQpuFillBytesPerGroup) != 0) {
    return false;
  }

  uint64_t *code =
      (uint64_t *)(g_fragment_output.control_scratch.cpu + kQpuTestCodeOffset);
  const u64 *program_code = QpuFrameProgramCode(program);
  for (u32 i = 0; i < code_words; ++i) {
    code[i] = program_code[i];
  }

  u32 *uniforms =
      (u32 *)(g_fragment_output.control_scratch.cpu + kQpuTestUniformOffset);
  uniforms[0] = job.height;
  uniforms[1] = job.groups_per_row;
  uniforms[2] = g_source_scratch.v3d_address;
  uniforms[3] = target_buffer.v3d_address;
  uniforms[4] = job.source_row_skip;
  uniforms[5] = job.target_row_skip;
  if (program == kQpuFrameProgramScanlines) {
    uniforms[6] = job.scanline_half_mask;
    uniforms[7] = job.scanline_quarter_mask;
    uniforms[8] = job.scanline_eighth_mask;
    uniforms[9] = job.scanline_sixteenth_mask;
  }

  memset(cfg, 0, sizeof(u32) * 7);
  cfg[0] = 1U << 16;
  cfg[1] = 1U << 16;
  cfg[2] = 1U << 16;
  cfg[3] = ((kQpuTestNumBatches - 1U) << 12) |
           (kQpuTestWorkgroupsPerSupergroup << 8) |
           (kQpuTestWorkgroupSize & 0xFFU);
  cfg[4] = kQpuTestNumBatches;
  cfg[5] = g_fragment_output.control_scratch.v3d_address + kQpuTestCodeOffset;
  cfg[6] = g_fragment_output.control_scratch.v3d_address + kQpuTestUniformOffset;

  CleanBufferForV3D(g_fragment_output.control_scratch);
  CleanBufferForV3D(g_source_scratch);
  CleanBufferForV3D(target_buffer);
  return true;
}

bool SubmitQpuCsd(const u32 cfg[7], bool log_status = true) {
  if (cfg == nullptr || cfg[0] == 0 || cfg[5] == 0) {
    return false;
  }

  WriteReg(kV3dCore0Base, kV3dCoreIntClear, 0xFFFFFFFFU);
  WriteReg(kV3dHubBase, kV3dHubIntClear, 0xFFFFFFFFU);
  InvalidateV3dCaches();
  DataSyncBarrier();

  if (log_status) {
    LogCsdStatus("submit", cfg);
  }
  WriteReg(kV3dCore0Base, kV3dCsdQueuedCfg1, cfg[1]);
  WriteReg(kV3dCore0Base, kV3dCsdQueuedCfg2, cfg[2]);
  WriteReg(kV3dCore0Base, kV3dCsdQueuedCfg3, cfg[3]);
  WriteReg(kV3dCore0Base, kV3dCsdQueuedCfg4, cfg[4]);
  WriteReg(kV3dCore0Base, kV3dCsdQueuedCfg5, cfg[5]);
  WriteReg(kV3dCore0Base, kV3dCsdQueuedCfg6, cfg[6]);
  WriteReg(kV3dCore0Base, kV3dCsdQueuedCfg7, 0);
  DataSyncBarrier();
  WriteReg(kV3dCore0Base, kV3dCsdQueuedCfg0, cfg[0]);

  const bool done = WaitForQpuDone(cfg, 200000, log_status);
  bool clean = false;
  if (done) {
    clean = CleanV3dCachesAfterTmuWrite();
  }
  WriteReg(kV3dCore0Base, kV3dCoreIntClear, 0xFFFFFFFFU);
  WriteReg(kV3dHubBase, kV3dHubIntClear, 0xFFFFFFFFU);
  return done && clean;
}

bool ReadQpuMagicStoreTarget(u32 *first_word) {
  if (first_word == nullptr ||
      g_target_scratch.cpu == nullptr ||
      g_target_scratch.size < sizeof(u32)) {
    return false;
  }

  InvalidateBufferFromV3D(g_target_scratch);
  *first_word = *(const u32 *)g_target_scratch.cpu;
  return true;
}

bool VerifyQpuMagicStoreTarget() {
  if (g_target_scratch.cpu == nullptr ||
      g_target_scratch.size < sizeof(u32)) {
    return false;
  }

  u32 first_word = 0;
  if (!ReadQpuMagicStoreTarget(&first_word)) {
    return false;
  }
  const bool ok = first_word == kQpuTestMagic;
  printf("boot: pi5v3d qpu magic first=0x%08x expected=0x%08x "
         "match=%s code_words=%u uniforms_va=0x%08x output_va=0x%08x\r\n",
         first_word, kQpuTestMagic, ok ? "yes" : "no",
         QpuMagicStoreCodeWords(),
         g_fragment_output.control_scratch.v3d_address + kQpuTestUniformOffset,
         g_target_scratch.v3d_address + kQpuTestOutputOffset);
  return ok;
}

bool VerifyQpuFillTarget() {
  if (g_target_scratch.cpu == nullptr ||
      g_target_scratch.size < sizeof(u32) ||
      (g_target_scratch.size % sizeof(u32)) != 0) {
    return false;
  }

  InvalidateBufferFromV3D(g_target_scratch);
  const u32 *words = (const u32 *)g_target_scratch.cpu;
  const u32 word_count = g_target_scratch.size / sizeof(u32);
  const u32 first = words[0];
  const u32 middle = words[word_count / 2U];
  const u32 last = words[word_count - 1U];
  const bool ok = first == kQpuFillColorWord &&
                  middle == kQpuFillColorWord &&
                  last == kQpuFillColorWord;
  printf("boot: pi5v3d qpu fill samples=0x%08x,0x%08x,0x%08x "
         "expected=0x%08x match=%s words=%u groups=%u "
         "uniforms_va=0x%08x output_va=0x%08x\r\n",
         first, middle, last, kQpuFillColorWord, ok ? "yes" : "no",
         word_count, g_target_scratch.size / kQpuFillBytesPerGroup,
         g_fragment_output.control_scratch.v3d_address + kQpuTestUniformOffset,
         g_target_scratch.v3d_address + kQpuTestOutputOffset);
  return ok;
}

u16 SourceScratchPixel(u32 x, u32 y) {
  return *((const u16 *)(g_source_scratch.cpu + y * g_source_scratch.pitch) + x);
}

u16 TargetScratchPixel(const Buffer &target_buffer, u32 x, u32 y) {
  return *((const u16 *)(target_buffer.cpu + y * target_buffer.pitch) + x);
}

u32 DarkenRgb565Term(u16 rgb, u32 shift, u32 duplicated_mask) {
  return ((u32)rgb >> shift) & (duplicated_mask & 0xFFFFU);
}

u16 DarkenRgb565WithScanlineTerms(u16 rgb, const QpuFrameJob &job) {
  return (u16)(
      DarkenRgb565Term(rgb, 1, job.scanline_half_mask) +
      DarkenRgb565Term(rgb, 2, job.scanline_quarter_mask) +
      DarkenRgb565Term(rgb, 3, job.scanline_eighth_mask) +
      DarkenRgb565Term(rgb, 4, job.scanline_sixteenth_mask));
}

u16 ExpectedQpuFramePixel(QpuFrameProgram program,
                          const QpuFrameJob &job,
                          u16 rgb,
                          u32 y) {
  if (program == kQpuFrameProgramScanlines && (y & 1U) != 0) {
    return DarkenRgb565WithScanlineTerms(rgb, job);
  }
  return rgb;
}

struct QpuFrameEffectProbe {
  bool found;
  u32 x;
  u32 y;
  u16 source;
  u16 expected;
  u16 target;
};

QpuFrameEffectProbe FindQpuFrameEffectProbe(QpuFrameProgram program,
                                            const QpuFrameJob &job,
                                            const Buffer &target_buffer) {
  QpuFrameEffectProbe probe = {false, 0, 0, 0, 0, 0};
  if (program != kQpuFrameProgramScanlines ||
      g_source_scratch.cpu == nullptr ||
      target_buffer.cpu == nullptr ||
      job.width == 0 ||
      job.height < 2 ||
      g_source_scratch.pitch < job.row_bytes ||
      target_buffer.pitch < job.row_bytes) {
    return probe;
  }

  for (u32 y = 1; y < job.height; y += 2) {
    for (u32 x = 0; x < job.width; ++x) {
      const u16 source = SourceScratchPixel(x, y);
      const u16 expected = ExpectedQpuFramePixel(program, job, source, y);
      if (source != expected) {
        probe.found = true;
        probe.x = x;
        probe.y = y;
        probe.source = source;
        probe.expected = expected;
        probe.target = TargetScratchPixel(target_buffer, x, y);
        return probe;
      }
    }
  }

  return probe;
}

bool VerifyQpuFrameTarget(QpuFrameProgram program,
                          const QpuFrameJob &job,
                          const Buffer &target_buffer,
                          bool log_status) {
  if (g_source_scratch.cpu == nullptr ||
      target_buffer.cpu == nullptr ||
      job.width == 0 ||
      job.height == 0 ||
      job.row_bytes == 0 ||
      g_source_scratch.pitch < job.row_bytes ||
      target_buffer.pitch < job.row_bytes) {
    return false;
  }

  InvalidateBufferFromV3D(target_buffer);
  const u16 src_first = SourceScratchPixel(0, 0);
  const u16 src_mid =
      SourceScratchPixel(job.width / 2U,
                         job.height / 2U);
  const u16 src_last =
      SourceScratchPixel(job.width - 1U,
                         job.height - 1U);
  const u16 dst_first = TargetScratchPixel(target_buffer, 0, 0);
  const u16 dst_mid =
      TargetScratchPixel(target_buffer, job.width / 2U,
                         job.height / 2U);
  const u16 dst_last =
      TargetScratchPixel(target_buffer, job.width - 1U,
                         job.height - 1U);
  const u16 exp_first = ExpectedQpuFramePixel(program, job, src_first, 0);
  const u16 exp_mid =
      ExpectedQpuFramePixel(program, job, src_mid, job.height / 2U);
  const u16 exp_last =
      ExpectedQpuFramePixel(program, job, src_last, job.height - 1U);
  const QpuFrameEffectProbe probe =
      FindQpuFrameEffectProbe(program, job, target_buffer);
  const bool probe_ok = !probe.found || probe.expected == probe.target;
  const bool ok = exp_first == dst_first &&
                  exp_mid == dst_mid &&
                  exp_last == dst_last &&
                  probe_ok;
  if (log_status || !ok) {
    printf("boot: pi5v3d frame %s samples src=0x%04x,0x%04x,0x%04x "
           "expected=0x%04x,0x%04x,0x%04x dst=0x%04x,0x%04x,0x%04x "
           "match=%s copy=%ux%u groups_per_row=%u scanline_x16=%u "
           "dark_masks=0x%08x,0x%08x,0x%08x,0x%08x "
           "effect_probe=%s x=%u y=%u src=0x%04x expected=0x%04x "
           "dst=0x%04x target_index=%u\r\n",
           QpuFrameProgramLogName(program),
           src_first, src_mid, src_last,
           exp_first, exp_mid, exp_last,
           dst_first, dst_mid, dst_last, ok ? "yes" : "no",
           job.width, job.height,
           job.groups_per_row,
           program == kQpuFrameProgramScanlines ? job.scanline_scale_x16 : 0U,
           program == kQpuFrameProgramScanlines ? job.scanline_half_mask : 0U,
           program == kQpuFrameProgramScanlines ?
               job.scanline_quarter_mask : 0U,
           program == kQpuFrameProgramScanlines ?
               job.scanline_eighth_mask : 0U,
           program == kQpuFrameProgramScanlines ?
               job.scanline_sixteenth_mask : 0U,
           probe.found ? (probe_ok ? "match" : "mismatch") : "none",
           probe.x,
           probe.y,
           probe.source,
           probe.expected,
           probe.target,
           QpuTargetBufferIndex(target_buffer));
  }
  return ok;
}

bool ProbeQpuFrameEffectUntilFound(QpuFrameProgram program,
                                   const QpuFrameJob &job,
                                   const Buffer &target_buffer) {
  if (program != kQpuFrameProgramScanlines || g_qpu_effect_probe_done) {
    return true;
  }

  if (g_qpu_effect_probe_attempts >= kQpuEffectProbeMaxAttempts) {
    return true;
  }

  InvalidateBufferFromV3D(target_buffer);
  ++g_qpu_effect_probe_attempts;

  const QpuFrameEffectProbe probe =
      FindQpuFrameEffectProbe(program, job, target_buffer);
  if (probe.found) {
    const bool ok = probe.expected == probe.target;
    printf("boot: pi5v3d frame %s effect_probe=%s attempt=%u "
           "x=%u y=%u src=0x%04x expected=0x%04x dst=0x%04x "
           "scanline_x16=%u dark_masks=0x%08x,0x%08x,0x%08x,0x%08x\r\n",
           QpuFrameProgramLogName(program),
           ok ? "match" : "mismatch",
           g_qpu_effect_probe_attempts,
           probe.x,
           probe.y,
           probe.source,
           probe.expected,
           probe.target,
           job.scanline_scale_x16,
           job.scanline_half_mask,
           job.scanline_quarter_mask,
           job.scanline_eighth_mask,
           job.scanline_sixteenth_mask);
    g_qpu_effect_probe_done = true;
    return ok;
  }

  if (g_qpu_effect_probe_attempts >= kQpuEffectProbeMaxAttempts) {
    printf("boot: pi5v3d frame %s effect_probe=none attempts=%u "
           "scanline_x16=%u\r\n",
           QpuFrameProgramLogName(program),
           g_qpu_effect_probe_attempts,
           job.scanline_scale_x16);
    g_qpu_effect_probe_done = true;
  }

  return true;
}

void ResetFragmentReplayPreparedState(FragmentReplayPreparedState *state) {
  if (state != nullptr) {
    memset(state, 0, sizeof *state);
  }
}

void ResetFragmentReplayPreparedState() {
  ResetFragmentReplayPreparedState(&g_fragment_output.prepared);
}

void ResetFragmentReplayContexts() {
  for (u32 pass = 0; pass < kFragmentPassCount; ++pass) {
    ResetFragmentReplayPreparedState(
        &g_fragment_pass_resources[pass].prepared);
  }
}

void ResetFragmentPassPackages() {
  for (u32 pass = 0; pass < kFragmentPassCount; ++pass) {
    shader_artifacts::ResetPreparedFragmentShaderPackage(
        &g_fragment_pass_resources[pass].package);
  }
  shader_artifacts::ResetPreparedFragmentShaderPackage(
      &g_fragment_output_fast_cubic_package);
}

void SelectFragmentOutputPackage(const RenderParams &params) {
  const u32 level_mapping = params.output_level_mapping <= 2U ?
      params.output_level_mapping : 1U;
  const bool requested =
      SelectedOutputResolutionPath() == kOutputResolutionPathSplitGeometry &&
      params.enable_output_response &&
      params.fast_output_response &&
      level_mapping == 1U;
  const bool selected =
      requested && g_fragment_fast_cubic_package_ready &&
      !g_fragment_fast_cubic_runtime_failed;
  if (selected == g_fragment_fast_cubic_selected) {
    return;
  }

  g_fragment_fast_cubic_selected = selected;
  ResetFragmentReplayPreparedState(&g_fragment_output.prepared);
  ResetFragmentReplayPreparedState(&g_fragment_bloom_base.prepared);
  g_fragment_frame_state_log_valid = false;
  g_fragment_bloom_path_log_valid = false;
  g_frame_path_logged = false;
  ResetFragmentPassStats();
  ResetBloomPassStats();
  ResetRenderStats();
  printf("boot: pi5v3d output package select=%s requested=%s "
         "specialized=%s\r\n",
         selected ? "fast-cubic" : "generic",
         requested ? "fast-cubic" : "generic",
         g_fragment_fast_cubic_package_ready ? "ready" : "unavailable");
}

void DisableFastCubicRuntime(const char *phase) {
  if (g_fragment_fast_cubic_runtime_failed) {
    return;
  }
  g_fragment_fast_cubic_runtime_failed = true;
  g_fragment_fast_cubic_selected = false;
  ResetFragmentReplayPreparedState(&g_fragment_output.prepared);
  ResetFragmentReplayPreparedState(&g_fragment_bloom_base.prepared);
  g_fragment_frame_state_log_valid = false;
  g_fragment_bloom_path_log_valid = false;
  g_frame_path_logged = false;
  ResetFragmentPassStats();
  ResetBloomPassStats();
  ResetRenderStats();
  printf("boot: pi5v3d output package fast-cubic disabled phase=%s "
         "fallback=generic\r\n",
         SafeString(phase));
}

bool BuildFragmentArtifactBindingsForControl(
    shader_artifacts::ShaderArtifactBufferBinding *bindings,
    u32 binding_count,
    const Buffer &control_scratch) {
  if (bindings == nullptr ||
      binding_count < kFragmentArtifactBindingCount ||
      control_scratch.cpu == nullptr ||
      control_scratch.v3d_address == 0 ||
      control_scratch.size == 0) {
    return false;
  }

  for (u32 i = 0; i < kFragmentArtifactBindingCount; ++i) {
    const FragmentArtifactSlice &slice = kFragmentArtifactSlices[i];
    if (slice.offset > control_scratch.size ||
        slice.size > control_scratch.size - slice.offset) {
      printf("boot: pi5v3d fragment artifact binding %s overflows "
             "control scratch offset=%u size=%u scratch=%u\r\n",
             SafeString(slice.name), slice.offset, slice.size,
             control_scratch.size);
      return false;
    }

    memset(control_scratch.cpu + slice.offset, 0, slice.size);
    bindings[i].name = slice.name;
    bindings[i].kind = slice.kind;
    bindings[i].cpu = control_scratch.cpu + slice.offset;
    bindings[i].size = slice.size;
    bindings[i].v3d_address = control_scratch.v3d_address + slice.offset;
  }

  return true;
}

bool BuildFragmentArtifactBindings(
    shader_artifacts::ShaderArtifactBufferBinding *bindings,
    u32 binding_count) {
  return BuildFragmentArtifactBindingsForControl(
      bindings, binding_count, g_fragment_output.control_scratch);
}

void LogFragmentArtifactBindingsForControl(
    const shader_artifacts::ShaderArtifactBufferBinding *bindings,
    u32 binding_count,
    const Buffer &control_scratch) {
  for (u32 i = 0; i < binding_count; ++i) {
    printf("boot: pi5v3d fragment artifact binding[%u] name=%s kind=%s "
           "va=0x%08x offset=%u size=%u cpu=0x%08x\r\n",
           i,
           SafeString(bindings[i].name),
           SafeString(bindings[i].kind),
           bindings[i].v3d_address,
           bindings[i].v3d_address - control_scratch.v3d_address,
           bindings[i].size,
           (u32)(uintptr)bindings[i].cpu);
  }
}

void LogFragmentArtifactBindings(
    const shader_artifacts::ShaderArtifactBufferBinding *bindings,
    u32 binding_count) {
  LogFragmentArtifactBindingsForControl(
      bindings, binding_count, g_fragment_output.control_scratch);
}

bool ReadArtifactFragmentUniformSemantic(
    const shader_artifacts::PreparedFragmentShaderPackage *prepared,
    const shader_artifacts::ShaderArtifact &artifact,
    const shader_artifacts::ShaderArtifactBufferBinding *bindings,
    u32 binding_count,
    const char *semantic,
    u32 *uniform_index,
    u32 *value);

void LogFragmentArtifactPatchedWords(
    const shader_artifacts::PreparedFragmentShaderPackage *prepared,
    const shader_artifacts::ShaderArtifact &artifact,
    const shader_artifacts::ShaderArtifactBufferBinding *bindings) {
  const uint8_t *cl = bindings[kFragmentArtifactClBinding].cpu;
  const uint8_t *sampler = bindings[kFragmentArtifactSamplerBinding].cpu;
  const uint8_t *texture = bindings[kFragmentArtifactTextureBinding].cpu;
  const uint8_t *attribute = bindings[kFragmentArtifactAttributeBinding].cpu;
  u32 y_scale_index = 0;
  u32 y_scale_value = 0;
  u32 weight_index = 0;
  u32 weight_value = 0;
  const bool have_y_scale = ReadArtifactFragmentUniformSemantic(
      prepared, artifact, bindings, kFragmentArtifactBindingCount,
      "fragcoord_y_scale", &y_scale_index, &y_scale_value);
  const bool have_weight = ReadArtifactFragmentUniformSemantic(
      prepared, artifact, bindings, kFragmentArtifactBindingCount,
      "scanline_weight", &weight_index, &weight_value);
  printf("boot: pi5v3d fragment artifact patched_words "
         "fragment_uniform[%u]=0x%08x fragment_uniform[%u]=0x%08x "
         "sampler[0]=0x%08x sampler[4]=0x%08x "
         "attr_record[0]=0x%08x attr_record[4]=0x%08x "
         "attribute[0]=0x%08x texture[0]=0x%08x\r\n",
         have_y_scale ? y_scale_index : 0xffffffffU,
         have_y_scale ? y_scale_value : 0U,
         have_weight ? weight_index : 0xffffffffU,
         have_weight ? weight_value : 0U,
         ReadLe32(sampler + 0x00),
         ReadLe32(sampler + 0x10),
         ReadLe32(cl + kFragmentArtifactAttributeRecordOffset),
         ReadLe32(cl + kFragmentArtifactSecondAttributeRecordOffset),
         ReadLe32(attribute + 0x00),
         ReadLe32(texture + 0x00));
}

void LogFragmentArtifactResolvedPatches(
    const shader_artifacts::ShaderArtifactResolvedPatch *resolved,
    u32 resolved_count) {
  for (u32 i = 0; i < resolved_count; ++i) {
    printf("boot: pi5v3d fragment artifact patch[%u] kind=%s "
           "stage=%s index=0x%08x word=0x%08x target=%s/%s+0x%04x "
           "encoding=%s value=0x%08x applied=%s\r\n",
           i,
           ArtifactPatchKindName(resolved[i].kind),
           ArtifactStageName(resolved[i].stage),
           resolved[i].index,
           resolved[i].word_index,
           SafeString(resolved[i].buffer_kind),
           SafeString(resolved[i].buffer_name),
           resolved[i].offset,
           ArtifactAddressEncodingName(resolved[i].encoding),
           resolved[i].value,
           resolved[i].applied ? "yes" : "no");
  }
}

const shader_artifacts::ShaderArtifactBufferBinding *FindArtifactBinding(
    const shader_artifacts::ShaderArtifactBufferBinding *bindings,
    u32 binding_count,
    const char *name,
    const char *kind) {
  if (bindings == nullptr || name == nullptr || kind == nullptr) {
    return nullptr;
  }
  for (u32 i = 0; i < binding_count; ++i) {
    if (bindings[i].name != nullptr && bindings[i].kind != nullptr &&
        strcmp(bindings[i].name, name) == 0 &&
        strcmp(bindings[i].kind, kind) == 0) {
      return &bindings[i];
    }
  }
  return nullptr;
}

bool FindArtifactStageCodeAddress(
    const shader_artifacts::ShaderArtifact &artifact,
    shader_artifacts::ShaderArtifactStage stage,
    const shader_artifacts::ShaderArtifactBufferBinding *bindings,
    u32 binding_count,
    u32 *address) {
  if (address == nullptr) {
    return false;
  }
  for (u32 i = 0; i < artifact.stage_count; ++i) {
    const shader_artifacts::ShaderArtifactStageCode &code =
        artifact.stages[i];
    if (code.stage != stage) {
      continue;
    }
    const shader_artifacts::ShaderArtifactBufferBinding *binding =
        FindArtifactBinding(bindings, binding_count, code.buffer_name,
                            "resource");
    if (binding == nullptr || code.code_offset > binding->size) {
      return false;
    }
    *address = binding->v3d_address + code.code_offset;
    return true;
  }
  return false;
}

bool FindArtifactUniformAddress(
    const shader_artifacts::ShaderArtifact &artifact,
    shader_artifacts::ShaderArtifactStage stage,
    const shader_artifacts::ShaderArtifactBufferBinding *bindings,
    u32 binding_count,
    u32 *address) {
  if (address == nullptr) {
    return false;
  }
  for (u32 i = 0; i < artifact.uniform_count; ++i) {
    const shader_artifacts::ShaderArtifactUniformBlock &uniform =
        artifact.uniforms[i];
    if (uniform.stage != stage) {
      continue;
    }
    const shader_artifacts::ShaderArtifactBufferBinding *binding =
        FindArtifactBinding(bindings, binding_count, uniform.buffer_name,
                            "CL");
    if (binding == nullptr || uniform.offset > binding->size) {
      return false;
    }
    *address = binding->v3d_address + uniform.offset;
    return true;
  }
  return false;
}

bool FindArtifactUniformStorage(
    const shader_artifacts::ShaderArtifact &artifact,
    shader_artifacts::ShaderArtifactStage stage,
    const shader_artifacts::ShaderArtifactBufferBinding *bindings,
    u32 binding_count,
    uint8_t **cpu,
    u32 *word_count) {
  if (cpu == nullptr || word_count == nullptr) {
    return false;
  }
  for (u32 i = 0; i < artifact.uniform_count; ++i) {
    const shader_artifacts::ShaderArtifactUniformBlock &uniform =
        artifact.uniforms[i];
    if (uniform.stage != stage) {
      continue;
    }
    const shader_artifacts::ShaderArtifactBufferBinding *binding =
        FindArtifactBinding(bindings, binding_count, uniform.buffer_name,
                            "CL");
    const u32 byte_count = uniform.word_count * sizeof(u32);
    if (binding == nullptr || binding->cpu == nullptr ||
        uniform.offset > binding->size ||
        byte_count > binding->size - uniform.offset) {
      return false;
    }
    *cpu = binding->cpu + uniform.offset;
    *word_count = uniform.word_count;
    return true;
  }
  return false;
}

bool ReadArtifactFragmentUniformSemantic(
    const shader_artifacts::PreparedFragmentShaderPackage *prepared,
    const shader_artifacts::ShaderArtifact &artifact,
    const shader_artifacts::ShaderArtifactBufferBinding *bindings,
    u32 binding_count,
    const char *semantic,
    u32 *uniform_index,
    u32 *value) {
  uint8_t *uniforms = nullptr;
  u32 word_count = 0;
  u32 index = 0;
  if (value == nullptr ||
      !shader_artifacts::FindPreparedFragmentUniformIndex(
          prepared, semantic, &index) ||
      !FindArtifactUniformStorage(
          artifact, shader_artifacts::kArtifactStageFragment,
          bindings, binding_count, &uniforms, &word_count) ||
      index >= word_count) {
    return false;
  }
  if (uniform_index != nullptr) {
    *uniform_index = index;
  }
  *value = ReadLe32(uniforms + index * sizeof(u32));
  return true;
}

bool BuildFragmentReplayShaderStateRecord(
    const shader_artifacts::ShaderArtifact &artifact,
    const shader_artifacts::ShaderArtifactBufferBinding *bindings,
    u32 binding_count) {
  using namespace shader_artifacts;

  const ShaderArtifactBufferBinding *cl_binding =
      FindArtifactBinding(bindings, binding_count, "CL_0x3071000", "CL");
  if (cl_binding == nullptr || cl_binding->cpu == nullptr ||
      0x40 > cl_binding->size ||
      32 > cl_binding->size - 0x40 ||
      kFragmentArtifactAttributeRecordOffset > cl_binding->size) {
    return false;
  }

  u32 fs_code = 0;
  u32 fs_uniforms = 0;
  u32 vs_code = 0;
  u32 vs_uniforms = 0;
  u32 cs_code = 0;
  u32 cs_uniforms = 0;
  if (!FindArtifactStageCodeAddress(artifact, kArtifactStageFragment,
                                    bindings, binding_count, &fs_code) ||
      !FindArtifactUniformAddress(artifact, kArtifactStageFragment,
                                  bindings, binding_count, &fs_uniforms) ||
      !FindArtifactStageCodeAddress(artifact, kArtifactStageVertex,
                                    bindings, binding_count, &vs_code) ||
      !FindArtifactUniformAddress(artifact, kArtifactStageVertex,
                                  bindings, binding_count, &vs_uniforms) ||
      !FindArtifactStageCodeAddress(artifact, kArtifactStageCoordinate,
                                    bindings, binding_count, &cs_code) ||
      !FindArtifactUniformAddress(artifact, kArtifactStageCoordinate,
                                  bindings, binding_count, &cs_uniforms)) {
    return false;
  }

  uint8_t *record = cl_binding->cpu + 0x40;
  memset(record, 0, 32);
  // BCM2712 revision 10 uses GL_SHADER_STATE_RECORD_DRAW_INDEX.
  record[0] = 1U << 1;  // Enable clipping.
  record[1] = 1U << 7;  // Fragment shader uses real pixel-centre W.
  record[2] = 1U << 5;  // Disable implicit point/line varyings.
  record[3] = 2;  // Number of varyings in fragment shader.
  record[4] = 1;  // Coordinate shader output VPM segment size.
  record[5] = 0;  // Coordinate shader input VPM segment size, min 1.
  record[6] = 1;  // Vertex shader output VPM segment size.
  record[7] = 0;  // Vertex shader input VPM segment size, min 1.
  WriteLe32(record + 8, fs_code | 1U);
  WriteLe32(record + 12, fs_uniforms);
  WriteLe32(record + 16, vs_code | 3U);
  WriteLe32(record + 20, vs_uniforms);
  WriteLe32(record + 24, cs_code | 3U);
  WriteLe32(record + 28, cs_uniforms);

  printf("boot: pi5v3d fragment replay shader_record "
         "flags=%02x,%02x,%02x varyings=%u "
         "record=0x%08x fs_code=0x%08x fs_uniforms=0x%08x "
         "vs_code=0x%08x vs_uniforms=0x%08x "
         "cs_code=0x%08x cs_uniforms=0x%08x attr=0x%08x\r\n",
         record[0], record[1], record[2], record[3],
         cl_binding->v3d_address + 0x40,
         fs_code, fs_uniforms, vs_code, vs_uniforms,
         cs_code, cs_uniforms,
         cl_binding->v3d_address + kFragmentArtifactAttributeRecordOffset);
  return true;
}

bool PatchFragmentReplayGeometry(
    const shader_artifacts::PreparedFragmentShaderPackage *prepared,
    const shader_artifacts::ShaderArtifact &artifact,
    const shader_artifacts::ShaderArtifactBufferBinding *bindings,
    u32 binding_count,
    const FragmentReplayGeometry &geometry) {
  uint8_t *fragment_uniforms = nullptr;
  uint8_t *vertex_uniforms = nullptr;
  uint8_t *coordinate_uniforms = nullptr;
  u32 fragment_word_count = 0;
  u32 vertex_word_count = 0;
  u32 coordinate_word_count = 0;
  u32 y_scale_index = 0;
  u32 y_bias_index = 0;
  if (geometry.width == 0 || geometry.height == 0 ||
      !FindArtifactUniformStorage(
          artifact, shader_artifacts::kArtifactStageFragment,
          bindings, binding_count, &fragment_uniforms,
          &fragment_word_count) ||
      !FindArtifactUniformStorage(
          artifact, shader_artifacts::kArtifactStageVertex,
          bindings, binding_count, &vertex_uniforms,
          &vertex_word_count) ||
      !FindArtifactUniformStorage(
          artifact, shader_artifacts::kArtifactStageCoordinate,
          bindings, binding_count, &coordinate_uniforms,
          &coordinate_word_count) ||
      vertex_word_count < 4U || coordinate_word_count < 4U) {
    return false;
  }

  const bool have_y_scale =
      shader_artifacts::FindPreparedFragmentUniformIndex(
          prepared, "fragcoord_y_scale", &y_scale_index);
  const bool have_y_bias =
      shader_artifacts::FindPreparedFragmentUniformIndex(
          prepared, "fragcoord_y_bias", &y_bias_index);
  if ((have_y_scale && y_scale_index >= fragment_word_count) ||
      (have_y_bias && y_bias_index >= fragment_word_count)) {
    return false;
  }

  const float clipper_x = FragmentReplayClipperScaleX(geometry.width);
  const float clipper_y = FragmentReplayClipperScaleY(geometry.height);
  const float viewport_x = FragmentReplayViewportOffsetX(geometry.width);
  const float viewport_y = FragmentReplayViewportOffsetY(geometry.height);

  if (have_y_scale) {
    WriteLe32(fragment_uniforms + y_scale_index * sizeof(u32),
              FloatBits(-1.0f));
  }
  if (have_y_bias) {
    WriteLe32(fragment_uniforms + y_bias_index * sizeof(u32),
              FloatBits((float)geometry.height));
  }
  WriteLe32(vertex_uniforms + 0U * sizeof(u32), FloatBits(clipper_x));
  WriteLe32(vertex_uniforms + 1U * sizeof(u32), FloatBits(clipper_y));
  WriteLe32(vertex_uniforms + 2U * sizeof(u32), FloatBits(0.5f));
  WriteLe32(vertex_uniforms + 3U * sizeof(u32), FloatBits(1.0f));
  WriteLe32(coordinate_uniforms + 0U * sizeof(u32), 0U);
  WriteLe32(coordinate_uniforms + 1U * sizeof(u32), FloatBits(clipper_x));
  WriteLe32(coordinate_uniforms + 2U * sizeof(u32), FloatBits(clipper_y));
  WriteLe32(coordinate_uniforms + 3U * sizeof(u32), FloatBits(1.0f));

  printf("boot: pi5v3d fragment replay geometry mode=%s size=%ux%u "
         "clipper_bits=(0x%08x,0x%08x) "
         "viewport_bits=(0x%08x,0x%08x)\r\n",
         SafeString(geometry.log_name),
         geometry.width, geometry.height,
         FloatBits(clipper_x), FloatBits(clipper_y),
         FloatBits(viewport_x), FloatBits(viewport_y));
  return true;
}

u32 FragmentSourceTextureWord(u32 x, u32 y) {
  const u32 red = 32U + x * 48U;
  const u32 green = 48U + y * 44U;
  const u32 blue = ((x ^ y) & 1U) != 0 ? 216U : 72U;
  return 0xFF000000U | (blue << 16) | (green << 8) | red;
}

void ResetFragmentSourceStagingCache() {
  delete[] g_fragment_source_staging_cache.output_response_lut;
  memset(&g_fragment_source_staging_cache, 0,
         sizeof g_fragment_source_staging_cache);
}

bool EnsureFragmentSourcePalette(
    const TextureSource &source,
    const v3dcrt::OutputResponseParams &output_response) {
  FragmentSourceStagingCache *cache = &g_fragment_source_staging_cache;
  if (cache->palette_valid &&
      cache->palette_generation == source.palette_generation &&
      cache->palette_signature == source.palette_signature &&
      v3dcrt::OutputResponseParamsEqual(
          cache->palette_output_response, output_response)) {
    return true;
  }

  for (u32 index = 0U; index < 256U; ++index) {
    const u16 pixel = v3dcrt::ResolveOutputResponseRgb565(
        source.pal565[index], output_response);
    cache->palette_rgb565[index] = pixel;
    cache->palette[index] = v3d71::Rgba8TextureWordFromRgb565(pixel);
  }
  cache->palette_generation = source.palette_generation;
  cache->palette_signature = source.palette_signature;
  cache->palette_output_response = output_response;
  cache->palette_valid = true;
  return true;
}

bool EnsureFragmentSourceOutputResponseLut(
    const v3dcrt::OutputResponseParams &output_response) {
  FragmentSourceStagingCache *cache = &g_fragment_source_staging_cache;
  if (!output_response.enabled) {
    return true;
  }
  if (cache->output_response_lut_valid &&
      v3dcrt::OutputResponseParamsEqual(
          cache->output_response_lut_params, output_response)) {
    return true;
  }
  if (cache->output_response_lut == nullptr) {
    cache->output_response_lut = new (HEAP_ANY) u16[65536U];
    if (cache->output_response_lut == nullptr) {
      return false;
    }
  }
  if (!v3dcrt::BuildOutputResponseRgb565Lut(
          output_response, cache->output_response_lut, 65536U)) {
    return false;
  }
  cache->output_response_lut_params = output_response;
  cache->output_response_lut_valid = true;
  return true;
}

bool WriteFragmentSourceTextureWords(const u32 *words, const char *label,
                                     bool log_success) {
  if (g_source_scratch.cpu == nullptr ||
      g_source_scratch.v3d_address == 0 ||
      g_source_scratch.size < kFragmentSourceTextureBytes ||
      words == nullptr) {
    return false;
  }

  for (u32 i = 0; i < kFragmentSourceTextureWords; ++i) {
    WriteLe32(g_source_scratch.cpu + i * sizeof(u32), words[i]);
  }
  CleanBufferForV3D(g_source_scratch);

  if (log_success) {
    printf("boot: pi5v3d fragment source texture staged "
           "label=%s source_va=0x%08x bytes=%u words="
           "0x%08x,0x%08x,0x%08x,0x%08x\r\n",
           SafeString(label),
           g_source_scratch.v3d_address,
           kFragmentSourceTextureBytes,
           ReadLe32(g_source_scratch.cpu + 0x00),
           ReadLe32(g_source_scratch.cpu + 0x0C),
           ReadLe32(g_source_scratch.cpu + 0x30),
           ReadLe32(g_source_scratch.cpu + 0x3C));
  }
  return true;
}

bool FillFragmentSourceTexture() {
  u32 words[kFragmentSourceTextureWords];
  for (u32 y = 0; y < 4; ++y) {
    for (u32 x = 0; x < 4; ++x) {
      words[y * 4U + x] = FragmentSourceTextureWord(x, y);
    }
  }
  return WriteFragmentSourceTextureWords(words, "boot-pattern", true);
}

bool FragmentSourceTextureWordsHaveNonblack(const u32 *words) {
  if (words == nullptr) {
    return false;
  }

  for (u32 i = 0; i < kFragmentSourceTextureWords; ++i) {
    if ((words[i] & 0x00FFFFFFU) != 0) {
      return true;
    }
  }
  return false;
}

u32 FragmentSourceTextureSignature(const u32 *words) {
  if (words == nullptr) {
    return 0;
  }

  u32 signature = 2166136261U;
  for (u32 i = 0; i < kFragmentSourceTextureWords; ++i) {
    signature ^= words[i];
    signature *= 16777619U;
  }
  return signature;
}

bool FillFragmentLiveProbeTexture(u32 staged_width, u32 staged_height,
                                  bool *has_nonblack,
                                  u32 *sample_signature,
                                  bool log_success) {
  if (has_nonblack != nullptr) {
    *has_nonblack = false;
  }
  if (sample_signature != nullptr) {
    *sample_signature = 0;
  }

  if (g_source_scratch.cpu == nullptr ||
      g_source_scratch.v3d_address == 0 ||
      g_source_scratch.size < kFragmentSourceTextureBytes ||
      staged_width == 0 || staged_height == 0 ||
      g_source_scratch.width < staged_width ||
      g_source_scratch.height < staged_height ||
      g_source_scratch.pitch < staged_width * 2U) {
    printf("boot: pi5v3d fragment live probe source invalid "
           "staged=%ux%u scratch=%ux%u pitch=%u size=%u\r\n",
           staged_width,
           staged_height,
           g_source_scratch.width,
           g_source_scratch.height,
           g_source_scratch.pitch,
           g_source_scratch.size);
    return false;
  }

  u32 words[kFragmentSourceTextureWords];
  for (u32 y = 0; y < 4; ++y) {
    const u32 sy = staged_height <= 1 ?
        0 : (y * (staged_height - 1U) + 1U) / 3U;
    const u16 *row =
        (const u16 *)(g_source_scratch.cpu + sy * g_source_scratch.pitch);
    for (u32 x = 0; x < 4; ++x) {
      const u32 sx = staged_width <= 1 ?
          0 : (x * (staged_width - 1U) + 1U) / 3U;
      words[y * 4U + x] =
          v3d71::Rgba8TextureWordFromRgb565(row[sx]);
    }
  }

  const bool nonblack = FragmentSourceTextureWordsHaveNonblack(words);
  const u32 signature = FragmentSourceTextureSignature(words);
  if (has_nonblack != nullptr) {
    *has_nonblack = nonblack;
  }
  if (sample_signature != nullptr) {
    *sample_signature = signature;
  }
  if (!nonblack) {
    return true;
  }

  return WriteFragmentSourceTextureWords(words, "live-probe", log_success);
}

bool PatchFragmentReplayTextureAtOffset(
    const shader_artifacts::ShaderArtifactBufferBinding *bindings,
    u32 binding_count,
    const Buffer &texture_buffer,
    const v3d71::Rgba8TextureLayout &layout,
    u32 state_offset,
    bool flip_y,
    const char *label,
    bool log_success) {
  const shader_artifacts::ShaderArtifactBufferBinding *texture_state =
      FindArtifactBinding(bindings, binding_count, "sampler_0x3269000",
                          "sampler");
  if (texture_state == nullptr || texture_state->cpu == nullptr ||
      state_offset > texture_state->size ||
      v3d71::kTextureShaderStateBytes >
          texture_state->size - state_offset ||
      texture_buffer.v3d_address == 0 ||
      layout.size_bytes > texture_buffer.size) {
    return false;
  }

  const u32 texture_address = texture_buffer.v3d_address;
  if (!v3d71::PackRgba8TextureShaderState(
          texture_state->cpu + state_offset,
          texture_state->size - state_offset,
          texture_address, layout, flip_y)) {
    return false;
  }
  if (log_success) {
    printf("boot: pi5v3d fragment texture descriptor patched label=%s "
           "state=0x%08x words=0x%08x,0x%08x,0x%08x,0x%08x,"
           "0x%08x,0x%08x texture_va=0x%08x size=%ux%u "
           "layout=%s padded=%ux%u bytes=%u ub_pad=%u flip_y=%u\r\n",
           SafeString(label), texture_state->v3d_address + state_offset,
           ReadLe32(texture_state->cpu + state_offset + 0x00),
           ReadLe32(texture_state->cpu + state_offset + 0x04),
           ReadLe32(texture_state->cpu + state_offset + 0x08),
           ReadLe32(texture_state->cpu + state_offset + 0x0C),
           ReadLe32(texture_state->cpu + state_offset + 0x10),
           ReadLe32(texture_state->cpu + state_offset + 0x14),
           texture_address,
           layout.width,
           layout.height,
           v3d71::Rgba8TextureTilingName(layout.tiling),
           layout.padded_width,
           layout.padded_height,
           layout.size_bytes,
           layout.level_0_ub_pad,
           flip_y ? 1U : 0U);
  }
  return true;
}

bool PatchFragmentReplayTexture(
    const shader_artifacts::ShaderArtifactBufferBinding *bindings,
    u32 binding_count,
    const Buffer &texture_buffer,
    const v3d71::Rgba8TextureLayout &layout,
    bool flip_y,
    const char *label,
    bool log_success) {
  return PatchFragmentReplayTextureAtOffset(
      bindings, binding_count, texture_buffer, layout,
      kFragmentPrimaryTextureStateOffset, flip_y, label, log_success);
}

bool PatchFragmentReplayTextureToSource(
    const shader_artifacts::ShaderArtifactBufferBinding *bindings,
    u32 binding_count,
    u32 width,
    u32 height,
    bool flip_y,
    bool log_success) {
  v3d71::Rgba8TextureLayout layout = {};
  return v3d71::ComputeRgba8TextureLayout(width, height, &layout) &&
         PatchFragmentReplayTexture(
             bindings, binding_count, g_source_scratch, layout, flip_y,
             "emulator-source", log_success);
}

bool PatchFragmentReplaySamplerStateAtOffset(
    const shader_artifacts::ShaderArtifactBufferBinding *bindings,
    u32 binding_count,
    u32 sampler_offset,
    bool linear_filter) {
  const shader_artifacts::ShaderArtifactBufferBinding *code =
      FindArtifactBinding(bindings, binding_count, "resource_0x2f2d000",
                          "resource");
  if (code == nullptr || code->cpu == nullptr ||
      sampler_offset > code->size ||
      v3d71::kSamplerStateBytes > code->size - sampler_offset) {
    return false;
  }
  return v3d71::PackSamplerState(code->cpu + sampler_offset,
                                 code->size - sampler_offset,
                                 linear_filter);
}

bool PatchFragmentReplaySamplerState(
    const shader_artifacts::ShaderArtifactBufferBinding *bindings,
    u32 binding_count,
    bool linear_filter) {
  return PatchFragmentReplaySamplerStateAtOffset(
      bindings, binding_count, kFragmentPrimarySamplerStateOffset,
      linear_filter);
}

bool PatchFragmentReplayTextureUniformSemantic(
    const shader_artifacts::PreparedFragmentShaderPackage *prepared,
    const shader_artifacts::ShaderArtifact &artifact,
    const shader_artifacts::ShaderArtifactBufferBinding *bindings,
    u32 binding_count,
    const char *semantic,
    u32 texture_state_offset,
    u32 sampler_state_offset) {
  uint8_t *fragment_uniforms = nullptr;
  u32 fragment_word_count = 0;
  const shader_artifacts::ShaderArtifactBufferBinding *texture_state =
      FindArtifactBinding(bindings, binding_count, "sampler_0x3269000",
                          "sampler");
  const shader_artifacts::ShaderArtifactBufferBinding *code =
      FindArtifactBinding(bindings, binding_count, "resource_0x2f2d000",
                          "resource");
  if (prepared == nullptr || semantic == nullptr ||
      texture_state == nullptr || code == nullptr ||
      texture_state_offset > texture_state->size ||
      v3d71::kTextureShaderStateBytes >
          texture_state->size - texture_state_offset ||
      sampler_state_offset > code->size ||
      v3d71::kSamplerStateBytes > code->size - sampler_state_offset ||
      !FindArtifactUniformStorage(
          artifact, shader_artifacts::kArtifactStageFragment,
          bindings, binding_count, &fragment_uniforms,
          &fragment_word_count)) {
    return false;
  }

  return shader_artifacts::
      PatchPreparedFragmentTextureUniformsForSemantic(
          prepared, fragment_uniforms, fragment_word_count, semantic,
          texture_state->v3d_address + texture_state_offset,
          code->v3d_address + sampler_state_offset);
}

void DisableFragmentEffects(RenderParams *params, bool preserve_geometry) {
  if (params == nullptr) {
    return;
  }
  if (!preserve_geometry) {
    params->enable_geometry = false;
  }
  params->enable_convergence = false;
  params->enable_horizontal_filtering = false;
  params->enable_edge_blur = false;
  params->enable_scanlines = false;
  params->enable_scanline_multisample = false;
  params->enable_mask = false;
  params->enable_bloom = false;
  params->enable_vignette = false;
  params->enable_uneven_illumination = false;
  params->enable_horizontal_jitter = false;
  params->enable_composite_artifacts = false;
  params->enable_glass_reflection = false;
  params->enable_rounded_screen_mask = false;
  params->enable_edge_glow = false;
  params->enable_noise = false;
  params->enable_output_response = false;
}

RenderParams ResolveFragmentEffectScope(const RenderParams &params) {
  RenderParams effective = params;
  if (g_shader_preset != kShaderCrt) {
    DisableFragmentEffects(&effective, false);
  } else if (SelectedOutputResolutionPath() ==
             kOutputResolutionPathSplitGeometry) {
    const bool enable_scanlines = effective.enable_scanlines;
    const bool enable_scanline_multisample =
        effective.enable_scanline_multisample;
    const bool enable_edge_blur = effective.enable_edge_blur;
    const bool enable_mask = effective.enable_mask;
    const bool enable_vignette = effective.enable_vignette;
    const bool enable_uneven_illumination =
        effective.enable_uneven_illumination;
    const bool enable_glass_reflection = effective.enable_glass_reflection;
    const bool enable_rounded_screen_mask =
        effective.enable_rounded_screen_mask;
    const bool enable_edge_glow = effective.enable_edge_glow;
    const bool enable_output_response = effective.enable_output_response;
    const bool enable_bloom = effective.enable_bloom;
    DisableFragmentEffects(&effective, true);
    effective.enable_edge_blur = enable_edge_blur;
    effective.enable_mask = enable_mask;
    effective.enable_vignette = enable_vignette;
    effective.enable_uneven_illumination = enable_uneven_illumination;
    effective.enable_glass_reflection = enable_glass_reflection;
    effective.enable_rounded_screen_mask = enable_rounded_screen_mask;
    effective.enable_edge_glow = enable_edge_glow;
    effective.enable_output_response = enable_output_response;
    effective.enable_bloom = enable_bloom;
    effective.enable_scanlines = enable_scanlines;
    effective.enable_scanline_multisample =
        enable_scanline_multisample;
  }
  return effective;
}

RenderParams ResolveFragmentSourceEffectScope(const RenderParams &params) {
  RenderParams effective = params;
  DisableFragmentEffects(&effective, false);
  if (g_shader_preset == kShaderCrt &&
      SelectedOutputResolutionPath() ==
          kOutputResolutionPathSplitGeometry) {
    effective.enable_horizontal_filtering =
        params.enable_horizontal_filtering;
    effective.enable_convergence = params.enable_convergence;
    effective.enable_composite_artifacts =
        params.enable_composite_artifacts;
    effective.enable_horizontal_jitter = params.enable_horizontal_jitter;
    effective.enable_noise = params.enable_noise;
  }
  return effective;
}

bool PatchFragmentReplayEffectUniforms(
    const shader_artifacts::PreparedFragmentShaderPackage *prepared,
    const shader_artifacts::ShaderArtifact &artifact,
    const shader_artifacts::ShaderArtifactBufferBinding *bindings,
    u32 binding_count,
    u32 source_width,
    u32 source_height,
    u32 output_height,
    const RenderParams &params,
    shader_artifacts::CrtFragmentUniformValues *values) {
  uint8_t *fragment_uniforms = nullptr;
  u32 fragment_word_count = 0;
  shader_artifacts::CrtFragmentUniformValues resolved = {};
  if (values == nullptr ||
      !FindArtifactUniformStorage(
          artifact, shader_artifacts::kArtifactStageFragment,
          bindings, binding_count, &fragment_uniforms,
          &fragment_word_count) ||
      !shader_artifacts::PatchPreparedFragmentTextureUniforms(
          prepared, fragment_uniforms, fragment_word_count) ||
      !shader_artifacts::BuildCrtFragmentUniformValues(
          source_width, source_height, output_height, params, &resolved)) {
    return false;
  }

  const bool full_crt_package =
      shader_artifacts::GetPreparedFragmentShaderPackageKind(prepared) ==
      shader_artifacts::kFragmentShaderPackageCrtSinglePass;
  const bool precomputed_fast_cubic_package =
      shader_artifacts::GetPreparedFragmentShaderPackageKind(prepared) ==
      shader_artifacts::kFragmentShaderPackageCrtOutputResponseFastCubic;
  for (u32 i = 0; i < resolved.patch_count; ++i) {
    const shader_artifacts::CrtFragmentUniformValues::Patch &patch =
        resolved.patches[i];
    if (!shader_artifacts::PatchPreparedFragmentUniform(
            prepared, fragment_uniforms, fragment_word_count,
            patch.semantic, FloatBits(patch.value)) && full_crt_package) {
      return false;
    }
  }
  if (precomputed_fast_cubic_package) {
    for (u32 i = 0; i < v3dcrt::kPrecomputedEffectUniformPatchCount; ++i) {
      const v3dcrt::EffectUniformValues::Patch &patch =
          resolved.derived_patches[i];
      if (!shader_artifacts::PatchPreparedFragmentUniform(
              prepared, fragment_uniforms, fragment_word_count,
              patch.semantic, FloatBits(patch.value))) {
        return false;
      }
    }
  }
  *values = resolved;
  return true;
}

bool FragmentScanlinesActive(
    const shader_artifacts::CrtFragmentUniformValues &values) {
  return values.scanline_weight > 0.0f &&
         values.scanline_gap_brightness < 1.0f;
}

bool FragmentPhosphorMaskEnabled(
    const shader_artifacts::CrtFragmentUniformValues &values) {
  return values.phosphor_mask_enable > 0.5f;
}

bool FragmentPhosphorMaskActive(
    const shader_artifacts::CrtFragmentUniformValues &values) {
  return FragmentPhosphorMaskEnabled(values) &&
         values.phosphor_mask_brightness < 1.0f;
}

float FragmentPatchValue(
    const shader_artifacts::CrtFragmentUniformValues &values,
    const char *semantic) {
  for (u32 i = 0; i < values.patch_count; ++i) {
    if (strcmp(values.patches[i].semantic, semantic) == 0) {
      return values.patches[i].value;
    }
  }
  return 0.0f;
}

const char *FragmentEffectName(
    const shader_artifacts::CrtFragmentUniformValues &values) {
  const bool scanlines = FragmentScanlinesActive(values);
  const bool mask = FragmentPhosphorMaskActive(values);
  struct EffectGroup {
    const char *semantic;
    const char *name;
  };
  const EffectGroup groups[] = {
    {"geometry_enable", "geometry"},
    {"convergence_enable", "convergence"},
    {"horizontal_filter_enable", "horizontal-filter"},
    {"edge_blur_enable", "edge-blur"},
    {"vignette_enable", "vignette"},
    {"uneven_illumination_enable", "uneven-illumination"},
    {"horizontal_jitter_enable", "horizontal-jitter"},
    {"composite_artifacts_enable", "composite-artifacts"},
    {"glass_reflection_enable", "glass-reflection"},
    {"rounded_screen_mask_enable", "rounded-screen-mask"},
    {"edge_glow_enable", "edge-glow"},
    {"noise_enable", "noise"},
    {"output_response_enable", "output-response"},
  };
  u32 active_count = (scanlines ? 1U : 0U) + (mask ? 1U : 0U);
  const char *single_name = scanlines ? "scanlines" :
      (mask ? "phosphor-mask" : nullptr);
  for (u32 i = 0; i < sizeof groups / sizeof groups[0]; ++i) {
    if (FragmentPatchValue(values, groups[i].semantic) > 0.5f) {
      ++active_count;
      single_name = groups[i].name;
    }
  }
  if (active_count == 1U) {
    return single_name;
  }
  if (active_count > 1U) {
    return "crt-combined";
  }
  return "pass-through";
}

u32 FragmentPhosphorMaskSignature(
    const shader_artifacts::CrtFragmentUniformValues &values) {
  const float mask_values[] = {
    values.phosphor_mask_enable,
    values.phosphor_mask_pattern,
    values.phosphor_mask_brightness,
  };
  u32 signature = 2166136261U;
  for (u32 i = 0; i < sizeof mask_values / sizeof mask_values[0]; ++i) {
    signature ^= FloatBits(mask_values[i]);
    signature *= 16777619U;
  }
  return signature;
}

bool FragmentOutputResponseActive(
    const shader_artifacts::CrtFragmentUniformValues &values) {
  return values.output_response_enable > 0.5f;
}

bool IsOutputResponsePackage(
    const shader_artifacts::PreparedFragmentShaderPackage *package) {
  const shader_artifacts::FragmentShaderPackageKind kind =
      shader_artifacts::GetPreparedFragmentShaderPackageKind(package);
  return kind == shader_artifacts::kFragmentShaderPackageCrtOutputResponse ||
         kind == shader_artifacts::
             kFragmentShaderPackageCrtOutputResponseFastCubic;
}

u32 FragmentOutputResponseSignature(
    const shader_artifacts::CrtFragmentUniformValues &values) {
  const float response_values[] = {
    values.output_response_enable,
    values.output_response_fast,
    values.input_gamma,
    values.inverse_output_gamma,
    values.saturation,
    values.black_level,
    values.white_clip,
    values.level_mapping,
  };
  u32 signature = 2166136261U;
  for (u32 i = 0;
       i < sizeof response_values / sizeof response_values[0]; ++i) {
    signature ^= FloatBits(response_values[i]);
    signature *= 16777619U;
  }
  return signature;
}

u32 FragmentEffectSignature(
    const shader_artifacts::CrtFragmentUniformValues &values) {
  u32 signature = 2166136261U;
  for (u32 i = 0; i < values.patch_count; ++i) {
    if (strcmp(values.patches[i].semantic, "temporal_frame") == 0) {
      continue;
    }
    signature ^= FloatBits(values.patches[i].value);
    signature *= 16777619U;
  }
  return signature;
}

u32 MixFragmentTimingSignature(u32 signature, u32 value) {
  signature ^= value;
  signature *= 16777619U;
  return signature;
}

u32 FragmentTimingSignature(
    const shader_artifacts::CrtFragmentUniformValues &source_values,
    const shader_artifacts::CrtFragmentUniformValues &output_values,
    bool source_linear,
    bool output_linear,
    bool bloom_enabled,
    float bloom_factor,
    bool rounded_enabled,
    float rounded_radius,
    float rounded_softness) {
  u32 signature = FragmentEffectSignature(source_values);
  signature = MixFragmentTimingSignature(
      signature, FragmentEffectSignature(output_values));
  signature = MixFragmentTimingSignature(
      signature, source_linear ? 1U : 0U);
  signature = MixFragmentTimingSignature(
      signature, output_linear ? 1U : 0U);
  signature = MixFragmentTimingSignature(
      signature, bloom_enabled ? 1U : 0U);
  signature = MixFragmentTimingSignature(signature, FloatBits(bloom_factor));
  signature = MixFragmentTimingSignature(
      signature, rounded_enabled ? 1U : 0U);
  signature = MixFragmentTimingSignature(
      signature, FloatBits(rounded_radius));
  return MixFragmentTimingSignature(
      signature, FloatBits(rounded_softness));
}

void LogFragmentSourceEffectState(
    const shader_artifacts::CrtFragmentUniformValues &values,
    bool linear_sampler) {
  const bool filter_enabled =
      FragmentPatchValue(values, "horizontal_filter_enable") > 0.5f;
  const float sigma = FragmentPatchValue(values, "horizontal_sigma_x");
  const float convergence_values[] = {
    FragmentPatchValue(values, "convergence_enable"),
    FragmentPatchValue(values, "red_offset_x"),
    FragmentPatchValue(values, "red_offset_y"),
    FragmentPatchValue(values, "blue_offset_x"),
    FragmentPatchValue(values, "blue_offset_y"),
    FragmentPatchValue(values, "convergence_radial_strength"),
  };
  u32 convergence_signature = 2166136261U;
  for (u32 i = 0;
       i < sizeof convergence_values / sizeof convergence_values[0]; ++i) {
    convergence_signature ^= FloatBits(convergence_values[i]);
    convergence_signature *= 16777619U;
  }
  const float composite_values[] = {
    FragmentPatchValue(values, "composite_artifacts_enable"),
    FragmentPatchValue(values, "composite_chroma_blur"),
    FragmentPatchValue(values, "composite_luma_sharpen"),
    FragmentPatchValue(values, "composite_color_bleed"),
  };
  u32 composite_signature = 2166136261U;
  for (u32 i = 0;
       i < sizeof composite_values / sizeof composite_values[0]; ++i) {
    composite_signature ^= FloatBits(composite_values[i]);
    composite_signature *= 16777619U;
  }
  const float jitter_values[] = {
    FragmentPatchValue(values, "horizontal_jitter_enable"),
    FragmentPatchValue(values, "horizontal_jitter_strength"),
    FragmentPatchValue(values, "horizontal_jitter_frequency"),
    FragmentPatchValue(values, "horizontal_jitter_speed"),
  };
  u32 jitter_signature = 2166136261U;
  for (u32 i = 0;
       i < sizeof jitter_values / sizeof jitter_values[0]; ++i) {
    jitter_signature ^= FloatBits(jitter_values[i]);
    jitter_signature *= 16777619U;
  }
  const float noise_values[] = {
    FragmentPatchValue(values, "noise_enable"),
    FragmentPatchValue(values, "luminance_noise"),
    FragmentPatchValue(values, "chroma_noise"),
    FragmentPatchValue(values, "noise_speed"),
  };
  u32 noise_signature = 2166136261U;
  for (u32 i = 0;
       i < sizeof noise_values / sizeof noise_values[0]; ++i) {
    noise_signature ^= FloatBits(noise_values[i]);
    noise_signature *= 16777619U;
  }
  const u32 sigma_bits = FloatBits(sigma);
  if (g_fragment_source_effect_log_valid &&
      g_fragment_source_filter_log_enabled == filter_enabled &&
      g_fragment_source_filter_log_sigma_bits == sigma_bits &&
      g_fragment_source_convergence_log_signature ==
          convergence_signature &&
      g_fragment_source_composite_log_signature == composite_signature &&
      g_fragment_source_jitter_log_signature == jitter_signature &&
      g_fragment_source_noise_log_signature == noise_signature &&
      g_fragment_source_sampler_log_linear == linear_sampler) {
    return;
  }

  printf("boot: pi5v3d fragment source-domain state effect=%s "
         "horizontal_filter=%s sigma_x100=%d convergence=%s "
         "red_x100=%d red_y100=%d blue_x100=%d blue_y100=%d "
         "radial_x100=%d composite=%s chroma_blur_x100=%d "
         "luma_sharpen_x100=%d color_bleed_x100=%d "
         "jitter=%s jitter_strength_x100=%d jitter_frequency_x100=%d "
         "jitter_speed_x100=%d "
         "noise=%s luminance_x1000=%d chroma_x1000=%d noise_speed_x100=%d "
         "sampler=%s\r\n",
         FragmentEffectName(values), filter_enabled ? "on" : "off",
         ParamX100(sigma), convergence_values[0] > 0.5f ? "on" : "off",
         ParamX100(convergence_values[1]),
         ParamX100(convergence_values[2]),
         ParamX100(convergence_values[3]),
         ParamX100(convergence_values[4]),
         ParamX100(convergence_values[5]),
         composite_values[0] > 0.5f ? "on" : "off",
         ParamX100(composite_values[1]),
         ParamX100(composite_values[2]),
         ParamX100(composite_values[3]),
         jitter_values[0] > 0.5f ? "on" : "off",
         ParamX100(jitter_values[1]),
         ParamX100(jitter_values[2]),
         ParamX100(jitter_values[3]),
         noise_values[0] > 0.5f ? "on" : "off",
         ParamX100(noise_values[1] * 10.0f),
         ParamX100(noise_values[2] * 10.0f),
         ParamX100(noise_values[3]),
         linear_sampler ? "linear" : "nearest");
  g_fragment_source_effect_log_valid = true;
  g_fragment_source_filter_log_enabled = filter_enabled;
  g_fragment_source_filter_log_sigma_bits = sigma_bits;
  g_fragment_source_convergence_log_signature = convergence_signature;
  g_fragment_source_composite_log_signature = composite_signature;
  g_fragment_source_jitter_log_signature = jitter_signature;
  g_fragment_source_noise_log_signature = noise_signature;
  g_fragment_source_sampler_log_linear = linear_sampler;
}

bool EnsureFragmentReplayTileScratchForContext(
    const FragmentReplayContext &context,
    const FragmentReplayGeometry &geometry) {
  if (context.tile_scratch == nullptr || context.tile_layout == nullptr) {
    return false;
  }
  Buffer &tile_scratch = *context.tile_scratch;
  FragmentReplayTileScratchLayout &tile_layout = *context.tile_layout;
  FragmentReplayTileScratchLayout required = {};
  if (!ComputeFragmentReplayTileScratchLayout(geometry, &required)) {
    printf("boot: pi5v3d fragment replay tile layout unsupported "
           "mode=%s target=%ux%u\r\n",
           SafeString(geometry.log_name), geometry.width, geometry.height);
    return false;
  }

  if (tile_scratch.cpu != nullptr &&
      tile_scratch.v3d_address != 0 &&
      tile_scratch.size >= required.total_bytes &&
      tile_layout.width == geometry.width &&
      tile_layout.height == geometry.height) {
    return true;
  }

  if (tile_scratch.cpu == nullptr ||
      tile_scratch.v3d_address == 0 ||
      tile_scratch.size < required.total_bytes) {
    Buffer replacement = {};
    if (!AllocateBuffer(kBufferUsageControl, required.total_bytes,
                        kBufferAlignment, &replacement)) {
      printf("boot: pi5v3d fragment replay tile scratch allocation failed "
             "mode=%s target=%ux%u bytes=%u\r\n",
             SafeString(geometry.log_name), geometry.width, geometry.height,
             required.total_bytes);
      return false;
    }
    FreeBuffer(&tile_scratch);
    tile_scratch = replacement;
  }

  tile_layout = required;
  memset(tile_scratch.cpu, 0, tile_scratch.size);
  CleanBufferForV3D(tile_scratch);
  printf("boot: pi5v3d fragment replay tile scratch context=%s target=%ux%u "
         "tiles=%ux%u va=0x%08x tile_alloc=0x%08x bytes=%u "
         "tsda=0x%08x bytes=%u total=%u cpu=0x%08x\r\n",
         SafeString(context.name), geometry.width, geometry.height,
         required.tiles_x, required.tiles_y,
         tile_scratch.v3d_address,
         tile_scratch.v3d_address,
         required.tile_alloc_bytes,
         tile_scratch.v3d_address + required.tsda_offset,
         required.tsda_bytes,
         required.total_bytes,
         (u32)(uintptr)tile_scratch.cpu);
  return true;
}

bool EnsureFragmentReplayTileScratch(
    const FragmentReplayGeometry &geometry) {
  return EnsureFragmentReplayTileScratchForContext(
      FragmentReplayContextForPass(kFragmentPassOutput), geometry);
}

bool BuildFragmentReplayBclForContext(
    const FragmentReplayContext &context,
    const shader_artifacts::ShaderArtifactBufferBinding *bindings,
    const FragmentReplayGeometry &geometry,
    u32 *start, u32 *end) {
  if (context.control_scratch == nullptr) {
    return false;
  }
  const Buffer &control_scratch = *context.control_scratch;
  if (bindings == nullptr || start == nullptr || end == nullptr ||
      geometry.width == 0 || geometry.height == 0 ||
      control_scratch.cpu == nullptr ||
      control_scratch.v3d_address == 0 ||
      kFragmentReplayRclOffset <= kFragmentReplayBclOffset ||
      kFragmentReplayRclOffset > control_scratch.size) {
    return false;
  }

  memset(control_scratch.cpu + kFragmentReplayBclOffset, 0,
         kFragmentReplayRclOffset - kFragmentReplayBclOffset);
  CommandListWriter cl = {
    control_scratch.cpu + kFragmentReplayBclOffset,
    kFragmentReplayRclOffset - kFragmentReplayBclOffset,
    0,
    true
  };

  *start = control_scratch.v3d_address + kFragmentReplayBclOffset;
  EmitNumberOfLayers(&cl, 1);
  EmitTileBinningModeCfg(&cl, geometry.width, geometry.height);
  ClPut8(&cl, kV3d71FlushVcdCache);
  EmitOcclusionQueryCounter(&cl, 0);
  ClPut8(&cl, kV3d71StartTileBinning);
  EmitClipWindow(&cl, 0, 0, geometry.width, geometry.height);
  EmitFragmentReplayCfgBits(&cl);
  EmitFloatPacket(&cl, kV3d71PointSize, 1.0f);
  EmitFloatPacket(&cl, kV3d71LineWidth, 1.0f);
  EmitClipperXyScaling(&cl,
                       FragmentReplayClipperScaleX(geometry.width),
                       FragmentReplayClipperScaleY(geometry.height));
  EmitClipperZScaleOffset(&cl, 0.5f, 0.5f);
  EmitClipperZMinMax(&cl, 0.0f, 1.0f);
  EmitViewportOffset(&cl,
                     FragmentReplayViewportOffsetX(geometry.width), 0,
                     FragmentReplayViewportOffsetY(geometry.height), 0);
  EmitColorWriteMasks(&cl, 0);
  EmitBlendConstantColor(&cl);
  ClPut8(&cl, kV3d71ZeroAllFlatShadeFlags);
  ClPut8(&cl, kV3d71ZeroAllNoperspectiveFlags);
  ClPut8(&cl, kV3d71ZeroAllCentroidFlags);
  EmitTransformFeedbackSpecs(&cl);
  EmitOcclusionQueryCounter(&cl, 0);
  EmitSampleState(&cl);
  EmitVcmCacheSize(&cl, 4, 4);
  EmitGlShaderState(
      &cl,
      bindings[kFragmentArtifactClBinding].v3d_address + 0x40,
      2);
  EmitVertexArrayPrims(&cl, 4, 3, 0);  // TRIANGLES.
  ClPut8(&cl, kV3d71Flush);

  if (!cl.ok || cl.offset != 0x7CU) {
    printf("boot: pi5v3d fragment replay BCL build failed "
           "ok=%s bytes=%u expected=124\r\n",
           cl.ok ? "yes" : "no", cl.offset);
    return false;
  }

  *end = control_scratch.v3d_address + kFragmentReplayBclOffset +
         cl.offset;
  return true;
}

bool BuildFragmentReplayBcl(
    const shader_artifacts::ShaderArtifactBufferBinding *bindings,
    const FragmentReplayGeometry &geometry,
    u32 *start, u32 *end) {
  return BuildFragmentReplayBclForContext(
      FragmentReplayContextForPass(kFragmentPassOutput), bindings, geometry, start, end);
}

bool BuildFragmentReplayGenericTileList(
    const shader_artifacts::ShaderArtifactBufferBinding *bindings,
    const FragmentReplayRenderTarget &target,
    u32 *start, u32 *end) {
  if (bindings == nullptr || start == nullptr || end == nullptr ||
      bindings[kFragmentArtifactClBinding].cpu == nullptr ||
      bindings[kFragmentArtifactClBinding].v3d_address == 0 ||
      target.buffer == nullptr || target.buffer->v3d_address == 0 ||
      kFragmentReplayGenericTileListOffset >=
          bindings[kFragmentArtifactClBinding].size) {
    return false;
  }

  CommandListWriter cl = {
    bindings[kFragmentArtifactClBinding].cpu +
        kFragmentReplayGenericTileListOffset,
    bindings[kFragmentArtifactClBinding].size -
        kFragmentReplayGenericTileListOffset,
    0,
    true
  };

  *start = bindings[kFragmentArtifactClBinding].v3d_address +
           kFragmentReplayGenericTileListOffset;
  ClPut8(&cl, kV3d71TileCoordinatesImplicit);
  ClPut8(&cl, kV3d71EndOfLoads);
  EmitPrimListFormat(&cl);
  EmitSetInstanceId(&cl, 0);
  EmitBranchToImplicitTileList(&cl, 0);
  EmitStoreTileBufferGeneralConfigured(
      &cl, kV3d71StoreRenderTarget0, target.buffer->v3d_address,
      target.memory_format, target.output_image_format,
      target.height_in_ub_or_stride, target.image_height,
      target.rb_swap, false);
  ClPut8(&cl, kV3d71ClearRenderTargets);
  ClPut8(&cl, kV3d71EndOfTileMarker);
  ClPut8(&cl, kV3d71ReturnFromSubList);

  if (!cl.ok || cl.offset != 0x1BU) {
    printf("boot: pi5v3d fragment replay generic tile list build failed "
           "ok=%s bytes=%u expected=27\r\n",
           cl.ok ? "yes" : "no", cl.offset);
    return false;
  }

  *end = bindings[kFragmentArtifactClBinding].v3d_address +
         kFragmentReplayGenericTileListOffset + cl.offset;
  return true;
}

bool BuildFragmentReplayGenericTileList(
    const shader_artifacts::ShaderArtifactBufferBinding *bindings,
    Buffer &target_buffer,
    u32 *start, u32 *end) {
  FragmentReplayRenderTarget target = {};
  return BuildRgb565FragmentRenderTarget(target_buffer, &target) &&
         BuildFragmentReplayGenericTileList(bindings, target, start, end);
}

bool BuildFragmentReplayRclForContext(
    const FragmentReplayContext &context,
    u32 generic_start, u32 generic_end,
    const FragmentReplayGeometry &geometry,
    u32 *start, u32 *end) {
  if (context.control_scratch == nullptr ||
      context.tile_scratch == nullptr) {
    return false;
  }
  const Buffer &control_scratch = *context.control_scratch;
  const Buffer &tile_scratch = *context.tile_scratch;
  if (start == nullptr || end == nullptr ||
      geometry.width == 0 || geometry.height == 0 ||
      generic_start == 0 ||
      generic_end <= generic_start || control_scratch.cpu == nullptr ||
      control_scratch.v3d_address == 0 ||
      tile_scratch.v3d_address == 0 ||
      kFragmentReplayRclOffset >= control_scratch.size ||
      kFragmentArtifactCodeOffset <= kFragmentReplayRclOffset) {
    return false;
  }

  memset(control_scratch.cpu + kFragmentReplayRclOffset, 0,
         kFragmentArtifactCodeOffset - kFragmentReplayRclOffset);
  CommandListWriter rcl = {
    control_scratch.cpu + kFragmentReplayRclOffset,
    kFragmentArtifactCodeOffset - kFragmentReplayRclOffset,
    0,
    true
  };

  *start = control_scratch.v3d_address + kFragmentReplayRclOffset;
  EmitTileRenderingModeCommonForSize(&rcl, geometry.width, geometry.height);
  EmitRenderTargetPart1ForSize(&rcl, 0);
  EmitZsClearValues(&rcl);
  EmitTileListInitialBlockSize64(&rcl);
  EmitTileListSetBase(&rcl, tile_scratch.v3d_address);
  EmitMulticoreSupertileCfgForSize(&rcl, geometry.width, geometry.height);
  EmitInitialTileClear(&rcl);
  EmitStartAddressOfGenericTileList(&rcl, generic_start, generic_end);
  const u32 tiles_x = TilesForPixels(geometry.width);
  const u32 tiles_y = TilesForPixels(geometry.height);
  for (u32 y = 0; y < tiles_y; ++y) {
    for (u32 x = 0; x < tiles_x; ++x) {
      EmitSupertileCoordinates(&rcl, x, y);
    }
  }
  ClPut8(&rcl, kV3d71EndOfRendering);

  if (!rcl.ok) {
    printf("boot: pi5v3d fragment replay RCL build failed "
           "mode=%s bytes=%u tiles=%ux%u\r\n",
           SafeString(geometry.log_name), rcl.offset, tiles_x, tiles_y);
    return false;
  }

  *end = control_scratch.v3d_address + kFragmentReplayRclOffset +
         rcl.offset;
  return true;
}

bool BuildFragmentReplayRcl(u32 generic_start, u32 generic_end,
                            const FragmentReplayGeometry &geometry,
                            u32 *start, u32 *end) {
  return BuildFragmentReplayRclForContext(
      FragmentReplayContextForPass(kFragmentPassOutput), generic_start, generic_end,
      geometry, start, end);
}

void LogBinnerStatus(const char *phase, u32 bcl_start, u32 bcl_end) {
  const u32 hub_int = ReadReg(kV3dHubBase, kV3dHubIntStatus);
  const u32 core_int = ReadReg(kV3dCore0Base, kV3dCoreIntStatus);
  const u32 mmu_ctl = ReadReg(kV3dHubBase, kV3dMmuCtl);
  const u32 vio_id = ReadReg(kV3dHubBase, kV3dMmuVioId);
  const u32 vio_addr = ReadReg(kV3dHubBase, kV3dMmuVioAddr);
  const u32 ct0cs = ReadReg(kV3dCore0Base, kV3dCleCt0Cs);
  const u32 ct0ca = ReadReg(kV3dCore0Base, kV3dCleCt0Ca);
  const u32 ct0ea = ReadReg(kV3dCore0Base, kV3dCleCt0Ea);
  const u32 ct0pc = ReadReg(kV3dCore0Base, kV3dCleCt0Pc);
  const u32 ct0qma = ReadReg(kV3dCore0Base, kV3dCleCt0Qma);
  const u32 ct0qms = ReadReg(kV3dCore0Base, kV3dCleCt0Qms);
  const u32 ct0qts = ReadReg(kV3dCore0Base, kV3dCleCt0Qts);
  const u32 mmu_faults = hub_int & (kV3dHubIntMmuWriteViolation |
                                    kV3dHubIntMmuPtInvalid |
                                    kV3dHubIntMmuCapExceeded);

  printf("boot: pi5v3d fragment replay binner %s "
         "bcl=0x%08x..0x%08x hub_int=0x%08x core_int=0x%08x "
         "mmu_faults=0x%08x mmu_ctl=0x%08x vio_id=0x%08x "
         "vio_addr=0x%08x vio_addr_x16=0x%08x "
         "ct0cs=0x%08x ct0ca=0x%08x "
         "ct0ea=0x%08x ct0pc=0x%08x qma=0x%08x qms=0x%08x "
         "qts=0x%08x\r\n",
         phase, bcl_start, bcl_end, hub_int, core_int, mmu_faults,
         mmu_ctl, vio_id, vio_addr, ScaledMmuVioAddr(vio_addr),
         ct0cs, ct0ca, ct0ea, ct0pc, ct0qma, ct0qms, ct0qts);
}

bool WaitForBinnerDone(u32 bcl_start, u32 bcl_end, unsigned timeout_us,
                       bool log_success) {
  const unsigned start_ticks = CTimer::GetClockTicks();

  while ((unsigned)(CTimer::GetClockTicks() - start_ticks) < timeout_us) {
    const u32 core_int = ReadReg(kV3dCore0Base, kV3dCoreIntStatus);
    const u32 mmu_faults = CurrentMmuFaults();

    if (mmu_faults != 0 || (core_int & kV3dIntRenderError) != 0) {
      LogBinnerStatus("fault", bcl_start, bcl_end);
      return false;
    }

    if ((core_int & kV3dIntFlDone) != 0) {
      if (log_success) {
        LogBinnerStatus("done", bcl_start, bcl_end);
      }
      return true;
    }

    CTimer::SimpleusDelay(10);
  }

  LogBinnerStatus("timeout", bcl_start, bcl_end);
  return false;
}

bool SubmitFragmentReplayBclForContext(
    const FragmentReplayContext &context,
    u32 bcl_start, u32 bcl_end, bool log_success) {
  if (context.tile_scratch == nullptr || context.tile_layout == nullptr) {
    return false;
  }
  const Buffer &tile_scratch = *context.tile_scratch;
  const FragmentReplayTileScratchLayout &tile_layout =
      *context.tile_layout;
  if (bcl_start == 0 || bcl_end <= bcl_start ||
      tile_scratch.v3d_address == 0 ||
      tile_layout.tile_alloc_bytes == 0 ||
      tile_layout.tsda_bytes == 0 ||
      tile_layout.total_bytes > tile_scratch.size) {
    return false;
  }

  WriteReg(kV3dCore0Base, kV3dCoreIntClear, 0xFFFFFFFFU);
  WriteReg(kV3dHubBase, kV3dHubIntClear, 0xFFFFFFFFU);
  InvalidateV3dCaches();
  DataSyncBarrier();

  const u32 tile_alloc = tile_scratch.v3d_address;
  const u32 tsda = tile_scratch.v3d_address + tile_layout.tsda_offset;
  WriteReg(kV3dCore0Base, kV3dCleCt0Qma, tile_alloc);
  WriteReg(kV3dCore0Base, kV3dCleCt0Qms,
           tile_layout.tile_alloc_bytes);
  WriteReg(kV3dCore0Base, kV3dCleCt0Qts, kV3dCleCt0QtsEnable | tsda);
  WriteReg(kV3dCore0Base, kV3dCleCt0Qba, bcl_start);
  DataSyncBarrier();
  if (log_success) {
    LogBinnerStatus("submit", bcl_start, bcl_end);
  }
  WriteReg(kV3dCore0Base, kV3dCleCt0Qea, bcl_end);

  const bool ok = WaitForBinnerDone(bcl_start, bcl_end, 200000, log_success);
  WriteReg(kV3dCore0Base, kV3dCoreIntClear, 0xFFFFFFFFU);
  WriteReg(kV3dHubBase, kV3dHubIntClear, 0xFFFFFFFFU);
  return ok;
}

bool SubmitFragmentReplayBcl(u32 bcl_start, u32 bcl_end, bool log_success) {
  return SubmitFragmentReplayBclForContext(
      FragmentReplayContextForPass(kFragmentPassOutput), bcl_start, bcl_end, log_success);
}

void LogFragmentReplayTargetSamples(const Buffer &target_buffer,
                                    const FragmentReplayGeometry &geometry) {
  if (target_buffer.cpu == nullptr ||
      geometry.width == 0 || geometry.height == 0 ||
      target_buffer.pitch < geometry.width * 2U ||
      target_buffer.width < geometry.width ||
      target_buffer.height < geometry.height) {
    return;
  }

  const u16 *first = (const u16 *)target_buffer.cpu;
  const u16 *mid = (const u16 *)(target_buffer.cpu +
      (geometry.height / 2U) * target_buffer.pitch) +
      (geometry.width / 2U);
  const u16 *last = (const u16 *)(target_buffer.cpu +
      (geometry.height - 1U) * target_buffer.pitch) +
      (geometry.width - 1U);
  u32 nonzero = 0;
  for (u32 y = 0; y < geometry.height; ++y) {
    const u16 *row = (const u16 *)(target_buffer.cpu +
        y * target_buffer.pitch);
    for (u32 x = 0; x < geometry.width; ++x) {
      if (row[x] != 0) {
        ++nonzero;
      }
    }
  }

  printf("boot: pi5v3d fragment replay target samples "
         "mode=%s first=0x%04x mid=0x%04x last=0x%04x "
         "target_index=%u nonzero=%u/%u\r\n",
         SafeString(geometry.log_name),
         *first, *mid, *last, QpuTargetBufferIndex(target_buffer), nonzero,
         geometry.width * geometry.height);
}

bool BuildFragmentReplayTargetPlane(
    const OutputTarget &target,
    const Buffer &target_buffer,
    const FragmentReplayGeometry &geometry,
    pi5kms::Plane *plane) {
  if (plane == nullptr || target_buffer.hvs_bus_address == 0 ||
      target_buffer.depth != 16 ||
      geometry.width == 0 || geometry.height == 0 ||
      target_buffer.width < geometry.width ||
      target_buffer.height < geometry.height ||
      target_buffer.pitch < geometry.width * 2U ||
      target.display_width == 0 || target.display_height == 0 ||
      target.destination_rect.width == 0 ||
      target.destination_rect.height == 0) {
    return false;
  }

  *plane = {
    target_buffer.hvs_bus_address,
    target_buffer.pitch,
    target_buffer.width,
    target_buffer.height,
    target_buffer.depth,
    pi5kms::kPixelFormatRgb565,
    pi5kms::kScaleFilterNearest,
    {0, 0, geometry.width, geometry.height},
    {(s32)target.destination_rect.x,
     (s32)target.destination_rect.y,
     target.destination_rect.width,
     target.destination_rect.height}
  };
  return true;
}

bool PresentFragmentReplayTargetDirect(const OutputTarget &target,
                                       const Buffer &target_buffer,
                                       const FragmentReplayGeometry &geometry,
                                       bool log_success) {
  if (!target.allow_direct_scanout || g_direct_scanout_failed) {
    return false;
  }

  pi5kms::Plane plane;
  if (!BuildFragmentReplayTargetPlane(target, target_buffer, geometry,
                                      &plane)) {
    g_direct_scanout_failed = true;
    if (!g_direct_scanout_failure_logged) {
      printf("boot: pi5v3d fragment scanout plane build failed "
             "mode=%s; using CPU scanout copy\r\n",
             SafeString(geometry.log_name));
      g_direct_scanout_failure_logged = true;
    }
    return false;
  }

  if (!pi5kms::PresentScanout(&plane, 1, target.display_width,
                              target.display_height,
                              target.wait_for_vblank)) {
    g_direct_scanout_failed = true;
    if (!g_direct_scanout_failure_logged) {
      printf("boot: pi5v3d fragment scanout direct HVS present failed "
             "mode=%s; using CPU scanout copy\r\n",
             SafeString(geometry.log_name));
      g_direct_scanout_failure_logged = true;
    }
    return false;
  }

  if (target.presented != nullptr) {
    *target.presented = true;
  }
  if (log_success) {
    printf("boot: pi5v3d fragment scanout direct mode=%s "
           "hvs=0x%08x pitch=%u "
           "src=0,0 %ux%u dst=%u,%u %ux%u display=%ux%u filter=%s "
           "wait_vblank=%u\r\n",
           SafeString(geometry.log_name),
           target_buffer.hvs_bus_address,
           target_buffer.pitch,
           geometry.width,
           geometry.height,
           target.destination_rect.x, target.destination_rect.y,
           target.destination_rect.width, target.destination_rect.height,
           target.display_width, target.display_height,
           HvsScaleFilterName(pi5kms::kScaleFilterNearest),
           target.wait_for_vblank ? 1U : 0U);
  }
  return true;
}

bool PresentFragmentReplayTargetFallback(const OutputTarget &target,
                                         const Buffer &target_buffer,
                                         const FragmentReplayGeometry &geometry,
                                         bool log_success) {
  if (!BlitRgb565BufferNearestToScanout(target_buffer,
                                        geometry.width,
                                        geometry.height,
                                        target)) {
    return false;
  }

  if (log_success) {
    printf("boot: pi5v3d fragment scanout copied to KMS framebuffer "
           "mode=%s target_index=%u src=0,0 %ux%u "
           "dst=%u,%u %ux%u scanout=%ux%u\r\n",
           SafeString(geometry.log_name),
           QpuTargetBufferIndex(target_buffer),
           geometry.width,
           geometry.height,
           target.destination_rect.x, target.destination_rect.y,
           target.destination_rect.width, target.destination_rect.height,
           target.scanout != nullptr ? target.scanout->width : 0,
           target.scanout != nullptr ? target.scanout->height : 0);
  }
  return true;
}

bool PrepareFragmentReplayRuntimeStateForContext(
    const FragmentReplayContext &context,
    const FragmentReplayGeometry &geometry,
    const FragmentReplayRenderTarget &render_target) {
  using namespace shader_artifacts;

  if (context.package == nullptr || context.control_scratch == nullptr ||
      context.tile_scratch == nullptr ||
      context.tile_layout == nullptr || context.prepared == nullptr ||
      render_target.buffer == nullptr) {
    return false;
  }
  Buffer &control_scratch = *context.control_scratch;
  Buffer &tile_scratch = *context.tile_scratch;
  FragmentReplayTileScratchLayout &tile_layout = *context.tile_layout;
  FragmentReplayPreparedState *state = context.prepared;
  const ShaderArtifact *prepared_artifact =
      GetPreparedFragmentShaderArtifact(context.package);
  if (state->ready && state->artifact == prepared_artifact &&
      state->geometry.width == geometry.width &&
      state->geometry.height == geometry.height) {
    return true;
  }

  if (state->ready &&
      (state->geometry.width != geometry.width ||
       state->geometry.height != geometry.height)) {
    g_fragment_frame_state_log_valid = false;
    g_frame_path_logged = false;
    ResetRenderStats();
  }
  ResetFragmentReplayPreparedState(state);
  if (!FragmentRenderTargetSupportsGeometry(render_target, geometry)) {
    const Buffer &target_buffer = *render_target.buffer;
    printf("boot: pi5v3d fragment replay prepare target too small "
           "context=%s mode=%s format=%s target=%ux%u pitch=%u "
           "requested=%ux%u\r\n",
           SafeString(context.name), SafeString(geometry.log_name),
           SafeString(render_target.format_name),
           target_buffer.width, target_buffer.height, target_buffer.pitch,
           geometry.width, geometry.height);
    return false;
  }
  if (!EnsureFragmentReplayTileScratchForContext(context, geometry)) {
    return false;
  }

  state->geometry = geometry;
  ShaderArtifactBufferBinding *bindings = state->bindings;
  if (!BuildFragmentArtifactBindingsForControl(
          bindings, kFragmentArtifactBindingCount, control_scratch)) {
    printf("boot: pi5v3d fragment replay prepare binding setup failed\r\n");
    ResetFragmentReplayPreparedState(state);
    return false;
  }

  ShaderArtifactResolvedPatch
      resolved[kFragmentArtifactResolvedPatchCapacity];
  ShaderArtifactMaterializeResult result;
  const char *reason = nullptr;
  const ShaderArtifact *artifact_ptr = prepared_artifact;
  if (artifact_ptr == nullptr) {
    printf("boot: pi5v3d fragment replay prepare package unavailable\r\n");
    ResetFragmentReplayPreparedState(state);
    return false;
  }
  const ShaderArtifact &artifact = *artifact_ptr;
  state->artifact = artifact_ptr;
  const bool materialized = MaterializeShaderArtifact(
      artifact,
      bindings,
      kFragmentArtifactBindingCount,
      resolved,
      kFragmentArtifactResolvedPatchCapacity,
      &result,
      &reason);
  if (!materialized) {
    printf("boot: pi5v3d fragment replay prepare materialize failed "
           "artifact=%s status=%s reason=%s resolved=%u applied=%u "
           "data_blocks=%u address_patches=%u\r\n",
           SafeString(artifact.name),
           ShaderArtifactMaterializeStatusName(result.status),
           SafeString(reason),
           result.resolved_patch_points,
           result.applied_patch_words,
           result.data_blocks_copied,
           result.applied_address_word_patches);
    ResetFragmentReplayPreparedState(state);
    return false;
  }
  if (!BuildFragmentReplayShaderStateRecord(artifact, bindings,
                                            kFragmentArtifactBindingCount)) {
    printf("boot: pi5v3d fragment replay prepare shader state record "
           "build failed\r\n");
    ResetFragmentReplayPreparedState(state);
    return false;
  }
  if (!PatchFragmentReplayGeometry(context.package, artifact, bindings,
                                   kFragmentArtifactBindingCount, geometry)) {
    printf("boot: pi5v3d fragment replay prepare geometry patch failed "
           "mode=%s\r\n",
           SafeString(geometry.log_name));
    ResetFragmentReplayPreparedState(state);
    return false;
  }
  if (!PatchFragmentReplayTextureToSource(
          bindings, kFragmentArtifactBindingCount, 4U, 4U, false, true)) {
    printf("boot: pi5v3d fragment replay prepare source texture patch "
           "failed\r\n");
    ResetFragmentReplayPreparedState(state);
    return false;
  }

  memset(render_target.buffer->cpu, 0, render_target.buffer->size);
  memset(tile_scratch.cpu, 0, tile_scratch.size);

  if (!BuildFragmentReplayBclForContext(
          context, bindings, geometry,
          &state->bcl_start, &state->bcl_end) ||
      !BuildFragmentReplayGenericTileList(bindings, render_target,
                                          &state->generic_start,
                                          &state->generic_end) ||
      !BuildFragmentReplayRclForContext(
          context, state->generic_start, state->generic_end, geometry,
          &state->rcl_start, &state->rcl_end)) {
    printf("boot: pi5v3d fragment replay prepare CL build failed\r\n");
    ResetFragmentReplayPreparedState(state);
    return false;
  }

  CleanBufferForV3D(control_scratch);
  CleanBufferForV3D(tile_scratch);
  CleanBufferForV3D(*render_target.buffer);

  const u32 tiles_x = TilesForPixels(geometry.width);
  const u32 tiles_y = TilesForPixels(geometry.height);
  printf("boot: pi5v3d fragment replay prepared context=%s mode=%s "
         "artifact=%s format=%s "
         "bcl=0x%08x..0x%08x bytes=%u rcl=0x%08x..0x%08x bytes=%u "
         "generic=0x%08x..0x%08x tile_alloc=0x%08x size=%u "
         "tsda=0x%08x shader_record=0x%08x target=0x%08x pitch=%u "
         "target_size=%ux%u tiles=%ux%u\r\n",
         SafeString(context.name), SafeString(geometry.log_name),
         SafeString(artifact.name), SafeString(render_target.format_name),
         state->bcl_start, state->bcl_end,
         state->bcl_end - state->bcl_start,
         state->rcl_start, state->rcl_end,
         state->rcl_end - state->rcl_start,
         state->generic_start, state->generic_end,
         tile_scratch.v3d_address,
         tile_layout.tile_alloc_bytes,
         tile_scratch.v3d_address + tile_layout.tsda_offset,
         bindings[kFragmentArtifactClBinding].v3d_address + 0x40,
         render_target.buffer->v3d_address,
         render_target.buffer->pitch,
         geometry.width,
         geometry.height,
         tiles_x, tiles_y);
  LogFragmentArtifactPatchedWords(context.package, artifact, bindings);
  state->ready = true;
  return true;
}

bool PrepareFragmentReplayRuntimeState(
    const FragmentReplayGeometry &geometry) {
  FragmentReplayRenderTarget render_target = {};
  return BuildRgb565FragmentRenderTarget(g_target_scratch, &render_target) &&
         PrepareFragmentReplayRuntimeStateForContext(
             FragmentReplayContextForPass(kFragmentPassOutput), geometry, render_target);
}

constexpr bool SelectsCrtOutputSplit(
    ShaderPreset shader_preset,
    FragmentPackageMode fragment_package_mode,
    RenderResolution render_resolution,
    BootTestMode boot_test_mode) {
  return render_resolution == kRenderResolutionOutput &&
         boot_test_mode == kBootTestOff &&
         shader_preset == kShaderCrt &&
         (fragment_package_mode == kFragmentPackageDefault ||
          fragment_package_mode == kFragmentPackageCoreDiagnostic);
}

static_assert(
    SelectsCrtOutputSplit(kShaderCrt, kFragmentPackageDefault,
                          kRenderResolutionOutput, kBootTestOff),
    "the production CRT output split must not require a package override");
static_assert(
    SelectsCrtOutputSplit(kShaderCrt, kFragmentPackageCoreDiagnostic,
                          kRenderResolutionOutput, kBootTestOff),
    "the legacy core package override must remain compatible");
static_assert(
    !SelectsCrtOutputSplit(kShaderCrt, kFragmentPackageNoiseDiagnostic,
                           kRenderResolutionOutput, kBootTestOff),
    "explicit diagnostic packages must retain their source-resolution path");

OutputResolutionPath SelectedOutputResolutionPath() {
  if (g_render_resolution != kRenderResolutionOutput ||
      g_boot_test_mode != kBootTestOff) {
    return kOutputResolutionPathDisabled;
  }
  if (g_shader_preset == kShaderSharp) {
    return kOutputResolutionPathPassthrough;
  }
  if (SelectsCrtOutputSplit(g_shader_preset, g_fragment_package_mode,
                            g_render_resolution, g_boot_test_mode)) {
    return kOutputResolutionPathSplitGeometry;
  }
  return kOutputResolutionPathDisabled;
}

const char *OutputResolutionPathName(OutputResolutionPath path) {
  switch (path) {
    case kOutputResolutionPathPassthrough:
      return "passthrough";
    case kOutputResolutionPathSplitGeometry:
      return "source-output-split";
    case kOutputResolutionPathDisabled:
    default:
      return "disabled";
  }
}

void LogOutputResolutionFallback(const OutputTarget &target,
                                 u32 source_width,
                                 u32 source_height,
                                 const char *reason) {
  if (g_output_resolution_fallback_logged) {
    return;
  }
  printf("boot: pi5v3d output-resolution probe fallback reason=%s "
         "requested=%ux%u source=%ux%u\r\n",
         SafeString(reason), target.destination_rect.width,
         target.destination_rect.height,
         source_width, source_height);
  g_output_resolution_fallback_logged = true;
}

bool PrepareSplitFragmentReplayState(
    const FragmentReplayGeometry &output_geometry,
    u32 source_width,
    u32 source_height) {
  const FragmentReplayGeometry source_geometry = {
    source_width,
    source_height,
    "fragment_frame_source_pass"
  };
  if (!EnsureFragmentRenderTargets(output_geometry) ||
      !EnsureFragmentIntermediateTarget(source_geometry)) {
    return false;
  }

  FragmentReplayRenderTarget source_target = {};
  FragmentReplayRenderTarget output_target = {};
  return BuildRgba8FragmentRenderTarget(
             g_fragment_intermediate_scratch,
             g_fragment_intermediate_layout, &source_target) &&
         BuildRgb565FragmentRenderTarget(g_target_scratch, &output_target) &&
         PrepareFragmentReplayRuntimeStateForContext(
             FragmentReplayContextForPass(kFragmentPassSource), source_geometry, source_target) &&
         PrepareFragmentReplayRuntimeStateForContext(
             FragmentReplayContextForPass(kFragmentPassOutput), output_geometry, output_target);
}

bool EnsureBloomTargets(const v3dcrt::BloomPassPlan &plan) {
  const FragmentReplayGeometry base_geometry = {
    plan.output_width,
    plan.output_height,
    "fragment_bloom_base"
  };
  const FragmentReplayGeometry horizontal_geometry = {
    plan.horizontal_width,
    plan.horizontal_height,
    "fragment_bloom_horizontal"
  };
  const FragmentReplayGeometry vertical_geometry = {
    plan.blur_width,
    plan.blur_height,
    "fragment_bloom_vertical"
  };

  for (u32 pass = kFragmentPassBloomBase;
       pass <= kFragmentPassBloomComposite; ++pass) {
    if (!EnsureFragmentReplayControlForContext(
            FragmentReplayContextForPass(
                static_cast<FragmentPass>(pass)))) {
      return false;
    }
  }

  bool base_changed = false;
  bool horizontal_changed = false;
  bool vertical_changed = false;
  if (!EnsureRgba8FragmentTarget(
          &g_fragment_bloom_base_scratch,
          &g_fragment_bloom_base_layout,
          base_geometry, "bloom-base", &base_changed) ||
      !EnsureRgba8FragmentTarget(
          &g_fragment_bloom_horizontal_scratch,
          &g_fragment_bloom_horizontal_layout,
          horizontal_geometry, "bloom-horizontal", &horizontal_changed) ||
      !EnsureRgba8FragmentTarget(
          &g_fragment_bloom_vertical_scratch,
          &g_fragment_bloom_vertical_layout,
          vertical_geometry, "bloom-vertical", &vertical_changed)) {
    return false;
  }

  if (base_changed || horizontal_changed || vertical_changed) {
    for (u32 pass = kFragmentPassBloomBase;
         pass <= kFragmentPassBloomComposite; ++pass) {
      ResetFragmentReplayPreparedState(
          &g_fragment_pass_resources[pass].prepared);
    }
    g_frame_path_logged = false;
  }
  return true;
}

bool PrepareBloomReplayState(const v3dcrt::BloomPassPlan &plan) {
  if (!plan.enabled || !g_fragment_bloom_packages_ready ||
      g_runtime_bloom_failed || !EnsureBloomTargets(plan)) {
    return false;
  }

  const FragmentReplayGeometry base_geometry = {
    plan.output_width,
    plan.output_height,
    "fragment_bloom_base"
  };
  const FragmentReplayGeometry horizontal_geometry = {
    plan.horizontal_width,
    plan.horizontal_height,
    "fragment_bloom_horizontal"
  };
  const FragmentReplayGeometry vertical_geometry = {
    plan.blur_width,
    plan.blur_height,
    "fragment_bloom_vertical"
  };
  const FragmentReplayGeometry composite_geometry = {
    plan.output_width,
    plan.output_height,
    "fragment_bloom_composite"
  };
  FragmentReplayRenderTarget base_target = {};
  FragmentReplayRenderTarget horizontal_target = {};
  FragmentReplayRenderTarget vertical_target = {};
  FragmentReplayRenderTarget composite_target = {};
  return BuildRgba8FragmentRenderTarget(
             g_fragment_bloom_base_scratch,
             g_fragment_bloom_base_layout, &base_target) &&
         BuildRgba8FragmentRenderTarget(
             g_fragment_bloom_horizontal_scratch,
             g_fragment_bloom_horizontal_layout, &horizontal_target) &&
         BuildRgba8FragmentRenderTarget(
             g_fragment_bloom_vertical_scratch,
             g_fragment_bloom_vertical_layout, &vertical_target) &&
         BuildRgb565FragmentRenderTarget(
             g_target_scratch, &composite_target) &&
         PrepareFragmentReplayRuntimeStateForContext(
             FragmentReplayContextForPass(kFragmentPassBloomBase), base_geometry, base_target) &&
         PrepareFragmentReplayRuntimeStateForContext(
             FragmentReplayContextForPass(kFragmentPassBloomHorizontal), horizontal_geometry,
             horizontal_target) &&
         PrepareFragmentReplayRuntimeStateForContext(
             FragmentReplayContextForPass(kFragmentPassBloomVertical), vertical_geometry,
             vertical_target) &&
         PrepareFragmentReplayRuntimeStateForContext(
             FragmentReplayContextForPass(kFragmentPassBloomComposite), composite_geometry,
             composite_target);
}

bool PrepareContinuousFragmentGeometry(
    const OutputTarget &target,
    u32 source_width,
    u32 source_height,
    FragmentReplayGeometry *geometry,
    bool *output_resolution_active) {
  if (source_width == 0 || source_height == 0 || geometry == nullptr ||
      output_resolution_active == nullptr) {
    return false;
  }

  *output_resolution_active = false;
  const OutputResolutionPath output_path = SelectedOutputResolutionPath();
  if (output_path != kOutputResolutionPathDisabled) {
    const FragmentReplayGeometry output_geometry = {
      target.destination_rect.width,
      target.destination_rect.height,
      "fragment_frame_output"
    };
    if (output_geometry.width == 0 || output_geometry.height == 0 ||
        output_geometry.width > kOutputTargetMaxWidth ||
        output_geometry.height > kOutputTargetMaxHeight) {
      LogOutputResolutionFallback(
          target, source_width, source_height, "unsupported-size");
    } else if ((output_path == kOutputResolutionPathSplitGeometry &&
                PrepareSplitFragmentReplayState(
                    output_geometry, source_width, source_height)) ||
               (output_path != kOutputResolutionPathSplitGeometry &&
                EnsureFragmentRenderTargets(output_geometry) &&
                PrepareFragmentReplayRuntimeState(output_geometry))) {
      *geometry = output_geometry;
      *output_resolution_active = true;
      return true;
    } else {
      LogOutputResolutionFallback(
          target, source_width, source_height, "prepare-failed");
    }
  } else if (g_render_resolution == kRenderResolutionOutput) {
    LogOutputResolutionFallback(
        target, source_width, source_height, "unsupported-preset-package");
  }

  const FragmentReplayGeometry source_geometry = {
    source_width,
    source_height,
    "fragment_frame_source"
  };
  if (!EnsureFragmentRenderTargets(source_geometry) ||
      !PrepareFragmentReplayRuntimeState(source_geometry)) {
    return false;
  }
  *geometry = source_geometry;
  return true;
}

bool UpdatePreparedFragmentFrameStateForContext(
    const FragmentReplayContext &context,
    const Buffer &texture_buffer,
    const v3d71::Rgba8TextureLayout &texture_layout,
    const RenderParams &params,
    const EdgeGlowFrameSamples *edge_glow_samples,
    const FragmentReplayRenderTarget &render_target,
    bool track_state_log,
    const char *pass_name,
    shader_artifacts::CrtFragmentUniformValues *effect_values) {
  if (context.prepared == nullptr || context.control_scratch == nullptr) {
    return false;
  }
  FragmentReplayPreparedState *state = context.prepared;
  const u32 source_width = texture_layout.width;
  const u32 source_height = texture_layout.height;
  if (!state->ready || source_width == 0 || source_height == 0 ||
      state->artifact == nullptr || effect_values == nullptr ||
      !FragmentRenderTargetSupportsGeometry(
          render_target, state->geometry)) {
    return false;
  }

  shader_artifacts::CrtFragmentUniformValues resolved_effect = {};
  if (context.package == nullptr ||
      !PatchFragmentReplayTexture(
          state->bindings, kFragmentArtifactBindingCount,
          texture_buffer, texture_layout, true, pass_name, false) ||
      !PatchFragmentReplaySamplerState(
          state->bindings, kFragmentArtifactBindingCount,
          params.enable_interpolation) ||
      !PatchFragmentReplayEffectUniforms(
          context.package, *state->artifact, state->bindings,
          kFragmentArtifactBindingCount,
          source_width, source_height, state->geometry.height,
          params, &resolved_effect)) {
    return false;
  }

  const u32 gap_bits = FloatBits(resolved_effect.scanline_gap_brightness);
  const u32 weight_bits = FloatBits(resolved_effect.scanline_weight);
  const u32 response_signature =
      FragmentOutputResponseSignature(resolved_effect);
  const u32 mask_signature = FragmentPhosphorMaskSignature(resolved_effect);
  const u32 effect_signature = FragmentEffectSignature(resolved_effect);
  const u32 bloom_factor_bits = FloatBits(params.bloom_factor);
  const bool log_state = track_state_log &&
      (!g_fragment_frame_state_log_valid ||
      g_fragment_frame_log_width != source_width ||
      g_fragment_frame_log_height != source_height ||
      g_fragment_frame_log_linear != params.enable_interpolation ||
      g_fragment_frame_log_gap_bits != gap_bits ||
      g_fragment_frame_log_weight_bits != weight_bits ||
      g_fragment_frame_log_mask_signature != mask_signature ||
      g_fragment_frame_log_response_signature != response_signature ||
      g_fragment_frame_log_effect_signature != effect_signature ||
      g_fragment_frame_log_bloom_enabled != params.enable_bloom ||
      g_fragment_frame_log_bloom_factor_bits != bloom_factor_bits);
  if (log_state && !PatchFragmentReplayTexture(
          state->bindings, kFragmentArtifactBindingCount,
          texture_buffer, texture_layout, true, pass_name, true)) {
    return false;
  }

  u32 generic_start = 0;
  u32 generic_end = 0;
  if (!BuildFragmentReplayGenericTileList(
          state->bindings, render_target, &generic_start, &generic_end) ||
      generic_start != state->generic_start ||
      generic_end != state->generic_end) {
    return false;
  }

  const shader_artifacts::FragmentShaderPackageKind package_kind =
      shader_artifacts::GetPreparedFragmentShaderPackageKind(context.package);
  if (package_kind ==
          shader_artifacts::kFragmentShaderPackageCrtOutputEdgeGlow ||
      IsOutputResponsePackage(context.package)) {
    uint8_t *fragment_uniforms = nullptr;
    u32 fragment_word_count = 0;
    if (edge_glow_samples == nullptr ||
        !FindArtifactUniformStorage(
            *state->artifact, shader_artifacts::kArtifactStageFragment,
            state->bindings, kFragmentArtifactBindingCount,
            &fragment_uniforms, &fragment_word_count)) {
      return false;
    }
    for (u32 i = 0; i < v3dcrt::kFrameEffectUniformPatchCount; ++i) {
      const v3dcrt::EffectUniformValues::Patch &patch =
          resolved_effect.derived_patches[
              v3dcrt::kFrameEffectUniformPatchOffset + i];
      if (!shader_artifacts::PatchPreparedFragmentUniform(
              context.package, fragment_uniforms, fragment_word_count,
              patch.semantic, FloatBits(patch.value))) {
        return false;
      }
    }
    for (u32 sample = 0; sample < kEdgeGlowFrameSampleCount; ++sample) {
      const float channels[kEdgeGlowFrameSampleChannels] = {
        edge_glow_samples->color[sample].red,
        edge_glow_samples->color[sample].green,
        edge_glow_samples->color[sample].blue,
      };
      for (u32 channel = 0;
           channel < kEdgeGlowFrameSampleChannels; ++channel) {
        if (!shader_artifacts::PatchPreparedFragmentUniform(
                context.package, fragment_uniforms, fragment_word_count,
                kEdgeGlowFrameSampleSemantics[sample][channel],
                FloatBits(channels[channel]))) {
          return false;
        }
      }
    }
  }

  CleanFragmentReplayDynamicState(context);
  if (log_state) {
    const shader_artifacts::ShaderArtifactBufferBinding *code =
        &state->bindings[kFragmentArtifactCodeBinding];
    printf("boot: pi5v3d fragment frame state context=%s pass=%s "
           "source=%ux%u "
           "filter=%s output_scope=%s effect=%s "
           "geometry=%s curvature_x100=%d,%d skew_x100=%d,%d "
           "trapezoid_x100=%d rotation_deg_x100=%d overscan_x100=%d "
           "horizontal_filter=%s sigma_x100=%d "
           "weight_x100=%d gap_x100=%d "
           "convergence=%s red_x100=%d,%d blue_x100=%d,%d radial_x100=%d "
           "edge_blur=%s edge_strength_x100=%d edge_radius_x100=%d "
           "edge_glow=%s glow_strength_x100=%d glow_width_x100=%d "
           "glass=%s glass_angle_x100=%d glass_width_x100=%d "
           "glass_position_x100=%d rounded=%s corner_x100=%d "
           "border_x100=%d "
           "vignette=%s vignette_strength_x100=%d vignette_scale_x100=%d "
           "vignette_softness_x100=%d "
           "uneven=%s uneven_strength_x100=%d uneven_scale_x100=%d "
           "jitter=%s jitter_strength_x100=%d jitter_frequency_x100=%d "
           "jitter_speed_x100=%d "
           "composite=%s chroma_blur_x100=%d luma_sharpen_x100=%d "
           "color_bleed_x100=%d "
           "noise=%s luminance_x1000=%d chroma_x1000=%d noise_speed_x100=%d "
           "mask=%s mask_pattern=%u mask_brightness_x100=%d "
           "response=%s input_gamma_x100=%d output_gamma_x100=%d "
           "saturation_x100=%d black_x100=%d white_x100=%d "
           "level_mapping=%u bloom=%s bloom_factor_x100=%d "
           "sampler_words=0x%08x,0x%08x "
           "effect_uniforms=0x%08x,0x%08x effect_signature=0x%08x "
           "target_index=%u "
           "target_va=0x%08x\r\n",
           SafeString(context.name), SafeString(pass_name), source_width,
           source_height,
           params.enable_interpolation ? "linear" : "nearest",
           OutputResolutionPathName(SelectedOutputResolutionPath()),
           FragmentEffectName(resolved_effect),
           FragmentPatchValue(resolved_effect, "geometry_enable") > 0.5f ?
               "on" : "off",
           ParamX100(FragmentPatchValue(resolved_effect, "curvature_x")),
           ParamX100(FragmentPatchValue(resolved_effect, "curvature_y")),
           ParamX100(FragmentPatchValue(resolved_effect, "skew_x")),
           ParamX100(FragmentPatchValue(resolved_effect, "skew_y")),
           ParamX100(FragmentPatchValue(resolved_effect, "trapezoid")),
           ParamX100(FragmentPatchValue(
               resolved_effect, "rotation_radians") * 57.2957795f),
           ParamX100(FragmentPatchValue(resolved_effect, "overscan_scale")),
           params.enable_horizontal_filtering ? "on" : "off",
           ParamX100(params.horizontal_sigma_x),
           ParamX100(resolved_effect.scanline_weight),
           ParamX100(resolved_effect.scanline_gap_brightness),
           params.enable_convergence ? "on" : "off",
           ParamX100(params.red_offset_x),
           ParamX100(params.red_offset_y),
           ParamX100(params.blue_offset_x),
           ParamX100(params.blue_offset_y),
           ParamX100(params.convergence_radial_strength),
           params.enable_edge_blur ? "on" : "off",
           ParamX100(params.edge_blur_strength),
           ParamX100(params.edge_blur_radius),
           params.enable_edge_glow ? "on" : "off",
           ParamX100(params.edge_glow_strength),
           ParamX100(params.edge_glow_width),
           params.enable_glass_reflection ? "on" : "off",
           ParamX100(params.glass_reflection_angle),
           ParamX100(params.glass_reflection_width),
           ParamX100(params.glass_reflection_position),
           params.enable_rounded_screen_mask ? "on" : "off",
           ParamX100(params.rounded_corner_radius),
           ParamX100(params.rounded_border_softness),
           params.enable_vignette ? "on" : "off",
           ParamX100(params.vignette_strength),
           ParamX100(params.vignette_scale),
           ParamX100(params.vignette_softness),
           params.enable_uneven_illumination ? "on" : "off",
           ParamX100(params.uneven_illumination_strength),
           ParamX100(params.uneven_illumination_scale),
           params.enable_horizontal_jitter ? "on" : "off",
           ParamX100(params.horizontal_jitter_strength),
           ParamX100(params.horizontal_jitter_frequency),
           ParamX100(params.horizontal_jitter_speed),
           params.enable_composite_artifacts ? "on" : "off",
           ParamX100(params.composite_chroma_blur),
           ParamX100(params.composite_luma_sharpen),
           ParamX100(params.composite_color_bleed),
           params.enable_noise ? "on" : "off",
           ParamX100(params.luminance_noise * 10.0f),
           ParamX100(params.chroma_noise * 10.0f),
           ParamX100(params.noise_speed),
           FragmentPhosphorMaskEnabled(resolved_effect) ? "on" : "off",
           (u32)resolved_effect.phosphor_mask_pattern,
           ParamX100(resolved_effect.phosphor_mask_brightness),
           FragmentOutputResponseActive(resolved_effect) ?
               (resolved_effect.output_response_fast > 0.5f ?
                    "fast" : "accurate") : "off",
           ParamX100(resolved_effect.input_gamma),
           ParamX100(1.0f / resolved_effect.inverse_output_gamma),
           ParamX100(resolved_effect.saturation),
           ParamX100(resolved_effect.black_level),
           ParamX100(resolved_effect.white_clip),
           (u32)resolved_effect.level_mapping,
           params.enable_bloom ? "on" : "off",
           ParamX100(params.bloom_factor),
           ReadLe32(code->cpu + 0x60),
           ReadLe32(code->cpu + 0x64),
           weight_bits,
           gap_bits,
           effect_signature,
           QpuTargetBufferIndex(*render_target.buffer),
           render_target.buffer->v3d_address);
    if (IsOutputResponsePackage(context.package)) {
      const bool fast_cubic =
          package_kind == shader_artifacts::
              kFragmentShaderPackageCrtOutputResponseFastCubic;
      static const char *const common_verified_semantics[] = {
        "edge_glow_enable",
        "edge_glow_strength",
        "edge_glow_width",
        "output_response_enable",
        "saturation",
      };
      for (u32 i = 0;
           i < sizeof common_verified_semantics /
                   sizeof common_verified_semantics[0];
           ++i) {
        u32 index = 0;
        u32 bits = 0;
        if (!ReadArtifactFragmentUniformSemantic(
                context.package, *state->artifact, state->bindings,
                kFragmentArtifactBindingCount, common_verified_semantics[i],
                &index, &bits)) {
          printf("boot: pi5v3d output uniforms context=%s status=missing "
                 "semantic=%s\r\n",
                 SafeString(context.name), common_verified_semantics[i]);
          break;
        }
        const u32 expected = FloatBits(FragmentPatchValue(
            resolved_effect, common_verified_semantics[i]));
        if (bits != expected) {
          printf("boot: pi5v3d output uniforms context=%s status=mismatch "
                 "semantic=%s index=%u bits=0x%08x expected=0x%08x\r\n",
                 SafeString(context.name), common_verified_semantics[i], index,
                 bits, expected);
          break;
        }
      }
      if (fast_cubic) {
        for (u32 i = 0;
             i < v3dcrt::kPrecomputedEffectUniformPatchCount; ++i) {
          const v3dcrt::EffectUniformValues::Patch &patch =
              resolved_effect.derived_patches[i];
          u32 index = 0;
          u32 bits = 0;
          if (!ReadArtifactFragmentUniformSemantic(
                  context.package, *state->artifact, state->bindings,
                  kFragmentArtifactBindingCount,
                  patch.semantic, &index, &bits)) {
            printf("boot: pi5v3d output uniforms context=%s status=missing "
                   "semantic=%s\r\n",
                   SafeString(context.name), patch.semantic);
            break;
          }
          const u32 expected = FloatBits(patch.value);
          if (bits != expected) {
            printf("boot: pi5v3d output uniforms context=%s status=mismatch "
                   "semantic=%s index=%u bits=0x%08x expected=0x%08x\r\n",
                   SafeString(context.name), patch.semantic,
                   index, bits, expected);
            break;
          }
        }
      } else {
        static const char *const generic_verified_semantics[] = {
          "black_level",
          "white_clip",
          "output_response_fast",
          "input_gamma",
          "inverse_output_gamma",
          "level_mapping",
        };
        for (u32 i = 0;
             i < sizeof generic_verified_semantics /
                     sizeof generic_verified_semantics[0];
             ++i) {
          u32 index = 0;
          u32 bits = 0;
          if (!ReadArtifactFragmentUniformSemantic(
                  context.package, *state->artifact, state->bindings,
                  kFragmentArtifactBindingCount,
                  generic_verified_semantics[i], &index, &bits)) {
            printf("boot: pi5v3d output uniforms context=%s status=missing "
                   "semantic=%s\r\n",
                   SafeString(context.name), generic_verified_semantics[i]);
            break;
          }
          const u32 expected = FloatBits(FragmentPatchValue(
              resolved_effect, generic_verified_semantics[i]));
          if (bits != expected) {
            printf("boot: pi5v3d output uniforms context=%s status=mismatch "
                   "semantic=%s index=%u bits=0x%08x expected=0x%08x\r\n",
                   SafeString(context.name), generic_verified_semantics[i],
                   index, bits, expected);
            break;
          }
        }
      }
    }
    g_fragment_frame_state_log_valid = true;
    g_fragment_frame_log_width = source_width;
    g_fragment_frame_log_height = source_height;
    g_fragment_frame_log_linear = params.enable_interpolation;
    g_fragment_frame_log_gap_bits = gap_bits;
    g_fragment_frame_log_weight_bits = weight_bits;
    g_fragment_frame_log_mask_signature = mask_signature;
    g_fragment_frame_log_response_signature = response_signature;
    g_fragment_frame_log_effect_signature = effect_signature;
    g_fragment_frame_log_bloom_enabled = params.enable_bloom;
    g_fragment_frame_log_bloom_factor_bits = bloom_factor_bits;
  }
  *effect_values = resolved_effect;
  return true;
}

bool PatchPreparedBloomScalarUniform(
    const FragmentReplayContext &context,
    FragmentReplayPreparedState *state,
    const char *semantic,
    float value) {
  uint8_t *fragment_uniforms = nullptr;
  u32 fragment_word_count = 0;
  return context.package != nullptr && state != nullptr &&
      state->artifact != nullptr &&
      FindArtifactUniformStorage(
          *state->artifact, shader_artifacts::kArtifactStageFragment,
          state->bindings, kFragmentArtifactBindingCount,
          &fragment_uniforms, &fragment_word_count) &&
      shader_artifacts::PatchPreparedFragmentUniform(
          context.package, fragment_uniforms, fragment_word_count,
          semantic, FloatBits(value));
}

bool FinalizePreparedBloomFrameState(
    const FragmentReplayContext &context,
    const FragmentReplayRenderTarget &render_target) {
  if (context.prepared == nullptr || context.control_scratch == nullptr) {
    return false;
  }
  FragmentReplayPreparedState *state = context.prepared;
  u32 generic_start = 0;
  u32 generic_end = 0;
  if (!BuildFragmentReplayGenericTileList(
          state->bindings, render_target, &generic_start, &generic_end) ||
      generic_start != state->generic_start ||
      generic_end != state->generic_end) {
    return false;
  }
  CleanFragmentReplayDynamicState(context);
  return true;
}

bool UpdatePreparedBloomBlurFrameState(
    const FragmentReplayContext &context,
    const Buffer &texture_buffer,
    const v3d71::Rgba8TextureLayout &texture_layout,
    const FragmentReplayRenderTarget &render_target,
    const char *texel_semantic,
    float texel_size,
    const char *pass_name,
    bool log_state) {
  if (context.prepared == nullptr || context.package == nullptr ||
      context.control_scratch == nullptr ||
      texture_layout.width == 0 || texture_layout.height == 0) {
    return false;
  }
  FragmentReplayPreparedState *state = context.prepared;
  if (!state->ready || state->artifact == nullptr ||
      !FragmentRenderTargetSupportsGeometry(render_target, state->geometry) ||
      !PatchFragmentReplayTextureAtOffset(
          state->bindings, kFragmentArtifactBindingCount,
          texture_buffer, texture_layout,
          kFragmentPrimaryTextureStateOffset, true, pass_name, log_state) ||
      !PatchFragmentReplaySamplerStateAtOffset(
          state->bindings, kFragmentArtifactBindingCount,
          kFragmentPrimarySamplerStateOffset, true) ||
      !PatchFragmentReplayTextureUniformSemantic(
          context.package, *state->artifact, state->bindings,
          kFragmentArtifactBindingCount, "source_texture",
          kFragmentPrimaryTextureStateOffset,
          kFragmentPrimarySamplerStateOffset) ||
      !PatchPreparedBloomScalarUniform(
          context, state, texel_semantic, texel_size) ||
      !FinalizePreparedBloomFrameState(context, render_target)) {
    return false;
  }
  if (log_state) {
    printf("boot: pi5v3d bloom frame state pass=%s source=%ux%u "
           "target=%ux%u filter=linear %s_x1000000=%u "
           "target_va=0x%08x\r\n",
           SafeString(pass_name), texture_layout.width,
           texture_layout.height, state->geometry.width,
           state->geometry.height, SafeString(texel_semantic),
           (u32)(texel_size * 1000000.0f + 0.5f),
           render_target.buffer != nullptr ?
               render_target.buffer->v3d_address : 0U);
  }
  return true;
}

float ClampBloomUniform(float value, float minimum, float maximum) {
  if (!(value >= minimum)) {
    return minimum;
  }
  return value > maximum ? maximum : value;
}

bool UpdatePreparedBloomCompositeFrameState(
    const Buffer &base_texture,
    const v3d71::Rgba8TextureLayout &base_layout,
    const Buffer &bloom_texture,
    const v3d71::Rgba8TextureLayout &bloom_layout,
    const FragmentReplayRenderTarget &render_target,
    const v3dcrt::BloomPassPlan &plan,
    const RenderParams &params,
    bool log_state) {
  const FragmentReplayContext context = FragmentReplayContextForPass(kFragmentPassBloomComposite);
  if (context.prepared == nullptr || context.package == nullptr ||
      context.control_scratch == nullptr ||
      base_layout.width == 0 || base_layout.height == 0 ||
      bloom_layout.width == 0 || bloom_layout.height == 0) {
    return false;
  }
  FragmentReplayPreparedState *state = context.prepared;
  const float rounded_enable =
      params.enable_rounded_screen_mask ? 1.0f : 0.0f;
  const float rounded_radius =
      ClampBloomUniform(params.rounded_corner_radius, 0.0f, 0.2f);
  const float rounded_softness =
      ClampBloomUniform(params.rounded_border_softness, 0.0f, 0.08f);
  if (!state->ready || state->artifact == nullptr ||
      !FragmentRenderTargetSupportsGeometry(render_target, state->geometry) ||
      !PatchFragmentReplayTextureAtOffset(
          state->bindings, kFragmentArtifactBindingCount,
          base_texture, base_layout, kFragmentPrimaryTextureStateOffset,
          true, "bloom-composite-base", log_state) ||
      !PatchFragmentReplayTextureAtOffset(
          state->bindings, kFragmentArtifactBindingCount,
          bloom_texture, bloom_layout, kFragmentBloomTextureStateOffset,
          true, "bloom-composite-blur", log_state) ||
      !PatchFragmentReplaySamplerStateAtOffset(
          state->bindings, kFragmentArtifactBindingCount,
          kFragmentPrimarySamplerStateOffset, false) ||
      !PatchFragmentReplaySamplerStateAtOffset(
          state->bindings, kFragmentArtifactBindingCount,
          kFragmentBloomSamplerStateOffset, true) ||
      !PatchFragmentReplayTextureUniformSemantic(
          context.package, *state->artifact, state->bindings,
          kFragmentArtifactBindingCount, "source_texture",
          kFragmentPrimaryTextureStateOffset,
          kFragmentPrimarySamplerStateOffset) ||
      !PatchFragmentReplayTextureUniformSemantic(
          context.package, *state->artifact, state->bindings,
          kFragmentArtifactBindingCount, "bloom_texture",
          kFragmentBloomTextureStateOffset,
          kFragmentBloomSamplerStateOffset) ||
      !PatchPreparedBloomScalarUniform(
          context, state, "bloom_factor", plan.factor) ||
      !PatchPreparedBloomScalarUniform(
          context, state, "rounded_screen_mask_enable", rounded_enable) ||
      !PatchPreparedBloomScalarUniform(
          context, state, "rounded_corner_radius", rounded_radius) ||
      !PatchPreparedBloomScalarUniform(
          context, state, "rounded_border_softness", rounded_softness) ||
      !FinalizePreparedBloomFrameState(context, render_target)) {
    return false;
  }
  if (log_state) {
    printf("boot: pi5v3d bloom frame state pass=composite "
           "base=%ux%u bloom=%ux%u target=%ux%u factor_x100=%d "
           "base_filter=nearest bloom_filter=linear rounded=%s "
           "radius_x100=%d softness_x100=%d target_va=0x%08x\r\n",
           base_layout.width, base_layout.height,
           bloom_layout.width, bloom_layout.height,
           state->geometry.width, state->geometry.height,
           ParamX100(plan.factor), rounded_enable > 0.5f ? "on" : "off",
           ParamX100(rounded_radius), ParamX100(rounded_softness),
           render_target.buffer != nullptr ?
               render_target.buffer->v3d_address : 0U);
  }
  return true;
}

bool UpdatePreparedFragmentFrameState(
    u32 source_width,
    u32 source_height,
    const RenderParams &params,
    Buffer &target_buffer,
    shader_artifacts::CrtFragmentUniformValues *effect_values) {
  v3d71::Rgba8TextureLayout texture_layout = {};
  FragmentReplayRenderTarget render_target = {};
  return v3d71::ComputeRgba8TextureLayout(
             source_width, source_height, &texture_layout) &&
         BuildRgb565FragmentRenderTarget(target_buffer, &render_target) &&
         UpdatePreparedFragmentFrameStateForContext(
             FragmentReplayContextForPass(kFragmentPassOutput), g_fragment_source_scratch,
             texture_layout, params, nullptr, render_target,
             true, "single-pass",
             effect_values);
}

bool SubmitPreparedFragmentReplayPassForContext(
    const FragmentReplayContext &context,
    const OutputTarget &target,
    const Buffer &target_buffer,
    bool present_scanout,
    bool return_direct_presented,
    bool invalidate_target_for_cpu,
    bool log_target_samples,
    bool log_timings,
    u64 *submit_done_us) {
  if (context.prepared == nullptr || context.tile_layout == nullptr) {
    return false;
  }
  FragmentReplayPreparedState *state = context.prepared;
  const FragmentReplayTileScratchLayout &tile_layout =
      *context.tile_layout;
  if (submit_done_us != nullptr) {
    *submit_done_us = 0;
  }
  if (!state->ready || target_buffer.cpu == nullptr ||
      target_buffer.v3d_address == 0 ||
      tile_layout.width != state->geometry.width ||
      tile_layout.height != state->geometry.height) {
    printf("boot: pi5v3d fragment replay prepared state missing "
           "context=%s\r\n", SafeString(context.name));
    return false;
  }

  const u64 start_us = CTimer::GetClockTicks64();
  if (!SubmitFragmentReplayBclForContext(
          context, state->bcl_start, state->bcl_end, false)) {
    const u64 failed_us = CTimer::GetClockTicks64();
    printf("boot: pi5v3d fragment replay timings mode=%s status=binner-failed "
           "binner:%u render:0 invalidate:0 sample:0 "
           "present_direct:0 present_fallback:0 total:%u\r\n",
           SafeString(state->geometry.log_name),
           (u32)(failed_us - start_us),
           (u32)(failed_us - start_us));
    return false;
  }
  const u64 binner_done_us = CTimer::GetClockTicks64();

  if (!SubmitSolidRcl(state->rcl_start, state->rcl_end, false)) {
    const u64 failed_us = CTimer::GetClockTicks64();
    printf("boot: pi5v3d fragment replay timings mode=%s status=render-failed "
           "binner:%u render:%u invalidate:0 sample:0 "
           "present_direct:0 present_fallback:0 total:%u\r\n",
           SafeString(state->geometry.log_name),
           (u32)(binner_done_us - start_us),
           (u32)(failed_us - binner_done_us),
           (u32)(failed_us - start_us));
    return false;
  }
  const u64 render_done_us = CTimer::GetClockTicks64();

  bool target_invalidated = false;
  u64 invalidate_done_us = render_done_us;
  if (invalidate_target_for_cpu || log_target_samples) {
    InvalidateBufferFromV3D(target_buffer);
    invalidate_done_us = CTimer::GetClockTicks64();
    target_invalidated = true;
  }
  u64 sample_done_us = invalidate_done_us;
  if (log_target_samples) {
    LogFragmentReplayTargetSamples(target_buffer, state->geometry);
    sample_done_us = CTimer::GetClockTicks64();
  }

  if (!present_scanout) {
    if (submit_done_us != nullptr) {
      *submit_done_us = sample_done_us;
    }
    if (log_timings) {
      printf("boot: pi5v3d fragment replay timings context=%s mode=%s "
             "status=offscreen binner:%u render:%u invalidate:%u "
             "sample:%u total:%u\r\n",
             SafeString(context.name), SafeString(state->geometry.log_name),
             (u32)(binner_done_us - start_us),
             (u32)(render_done_us - binner_done_us),
             (u32)(invalidate_done_us - render_done_us),
             (u32)(sample_done_us - invalidate_done_us),
             (u32)(sample_done_us - start_us));
    }
    return true;
  }

  if (target.rendered_plane != nullptr) {
    BuildFragmentReplayTargetPlane(target, target_buffer, state->geometry,
                                   target.rendered_plane);
  }
  if (!target.allow_direct_scanout && target.rendered_plane != nullptr &&
      target.rendered_plane->framebuffer_bus_address != 0) {
    const u64 deferred_done_us = CTimer::GetClockTicks64();
    if (submit_done_us != nullptr) {
      *submit_done_us = deferred_done_us;
    }
    if (log_timings) {
      printf("boot: pi5v3d fragment replay timings mode=%s "
             "status=deferred wait_vblank=%u binner:%u render:%u "
             "invalidate:%u sample:%u present_direct:0 "
             "present_fallback:0 total:%u\r\n",
             SafeString(state->geometry.log_name),
             target.wait_for_vblank ? 1U : 0U,
             (u32)(binner_done_us - start_us),
             (u32)(render_done_us - binner_done_us),
             (u32)(invalidate_done_us - render_done_us),
             (u32)(sample_done_us - invalidate_done_us),
             (u32)(deferred_done_us - start_us));
    }
    return true;
  }

  const bool direct_ok =
      PresentFragmentReplayTargetDirect(target, target_buffer,
                                        state->geometry, log_timings);
  const u64 direct_done_us = CTimer::GetClockTicks64();
  if (direct_ok) {
    if (submit_done_us != nullptr) {
      *submit_done_us = direct_done_us;
    }
    if (log_timings) {
      printf("boot: pi5v3d fragment replay timings mode=%s status=direct "
             "wait_vblank=%u binner:%u render:%u invalidate:%u sample:%u "
             "present_direct:%u present_fallback:0 total:%u\r\n",
             SafeString(state->geometry.log_name),
             target.wait_for_vblank ? 1U : 0U,
             (u32)(binner_done_us - start_us),
             (u32)(render_done_us - binner_done_us),
             (u32)(invalidate_done_us - render_done_us),
             (u32)(sample_done_us - invalidate_done_us),
             (u32)(direct_done_us - sample_done_us),
             (u32)(direct_done_us - start_us));
    }
    if (log_target_samples) {
      printf("boot: pi5v3d fragment scanout submitted and presented; %s\r\n",
             return_direct_presented ?
                 "runtime fragment probe complete" :
                 "normal emulator boot continues");
    }
    return return_direct_presented;
  }

  if (!target_invalidated) {
    InvalidateBufferFromV3D(target_buffer);
  }
  const bool fallback_ok =
      PresentFragmentReplayTargetFallback(target, target_buffer,
                                          state->geometry, log_timings);
  const u64 fallback_done_us = CTimer::GetClockTicks64();
  if (fallback_ok) {
    if (submit_done_us != nullptr) {
      *submit_done_us = fallback_done_us;
    }
    if (log_timings) {
      printf("boot: pi5v3d fragment replay timings mode=%s status=fallback "
             "binner:%u render:%u invalidate:%u sample:%u "
             "present_direct:%u present_fallback:%u total:%u\r\n",
             SafeString(state->geometry.log_name),
             (u32)(binner_done_us - start_us),
             (u32)(render_done_us - binner_done_us),
             (u32)(invalidate_done_us - render_done_us),
             (u32)(sample_done_us - invalidate_done_us),
             (u32)(direct_done_us - sample_done_us),
             (u32)(fallback_done_us - direct_done_us),
             (u32)(fallback_done_us - start_us));
    }
    if (log_target_samples) {
      printf("boot: pi5v3d fragment scanout submitted and staged for "
             "KMS present; %s\r\n",
             return_direct_presented ?
                 "runtime fragment probe complete" :
                 "normal emulator boot continues");
    }
    return true;
  }
  if (submit_done_us != nullptr) {
    *submit_done_us = fallback_done_us;
  }
  printf("boot: pi5v3d fragment replay timings mode=%s status=present-failed "
         "binner:%u render:%u invalidate:%u sample:%u "
         "present_direct:%u present_fallback:%u total:%u\r\n",
         SafeString(state->geometry.log_name),
         (u32)(binner_done_us - start_us),
         (u32)(render_done_us - binner_done_us),
         (u32)(invalidate_done_us - render_done_us),
         (u32)(sample_done_us - invalidate_done_us),
         (u32)(direct_done_us - sample_done_us),
         (u32)(fallback_done_us - direct_done_us),
         (u32)(fallback_done_us - start_us));
  printf("boot: pi5v3d fragment scanout present failed; "
         "normal emulator boot continues\r\n");
  return false;
}

bool SubmitPreparedFragmentReplayPass(const OutputTarget &target,
                                      const Buffer &target_buffer,
                                      bool return_direct_presented,
                                      bool log_target_samples,
                                      bool log_timings,
                                      u64 *submit_done_us) {
  return SubmitPreparedFragmentReplayPassForContext(
      FragmentReplayContextForPass(kFragmentPassOutput), target, target_buffer, true,
      return_direct_presented, true, log_target_samples,
      log_timings, submit_done_us);
}

u32 SwapRgba8RedBlue(u32 value) {
  return (value & 0xFF00FF00U) |
         ((value & 0x000000FFU) << 16) |
         ((value & 0x00FF0000U) >> 16);
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

bool BuildEdgeGlowFrameSamples(
    const Buffer &texture_buffer,
    const v3d71::Rgba8TextureLayout &texture_layout,
    bool enabled,
    bool linear_filter,
    bool log_samples,
    EdgeGlowFrameSamples *samples) {
  if (samples == nullptr) {
    return false;
  }
  memset(samples, 0, sizeof *samples);
  if (!enabled) {
    v3dcrt::ResetEdgeGlowTemporalFilter(&g_edge_glow_temporal_filter);
    return true;
  }
  if (texture_buffer.cpu == nullptr ||
      texture_buffer.size < texture_layout.size_bytes) {
    v3dcrt::ResetEdgeGlowTemporalFilter(&g_edge_glow_temporal_filter);
    return false;
  }

  const float tangent_coordinates[kEdgeGlowRegionKernelSize] = {
    0.25f,
    0.50f,
    0.75f,
  };
  const float normal_coordinates[kEdgeGlowRegionKernelSize] = {
    kEdgeGlowEdgeInset - kEdgeGlowEdgeNormalOffset,
    kEdgeGlowEdgeInset,
    kEdgeGlowEdgeInset + kEdgeGlowEdgeNormalOffset,
  };
  const u32 region_weights[kEdgeGlowRegionKernelSize] = {1U, 2U, 1U};
  constexpr float kRegionWeightScale = 1.0f / 16.0f;
  EdgeGlowFrameSamples current = {};
  for (u32 i = 0; i < kEdgeGlowFrameSampleCount; ++i) {
    for (u32 y = 0; y < kEdgeGlowRegionKernelSize; ++y) {
      for (u32 x = 0; x < kEdgeGlowRegionKernelSize; ++x) {
        float sample_u = 0.0f;
        float sample_v = 0.0f;
        switch (i) {
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
        v3d71::RgbFloat color = {};
        if (!v3d71::SampleRgba8Texture(
                texture_buffer.cpu, texture_buffer.size, texture_layout,
                sample_u, sample_v,
                linear_filter, true, &color)) {
          v3dcrt::ResetEdgeGlowTemporalFilter(
              &g_edge_glow_temporal_filter);
          return false;
        }
        const float luma =
            color.red * 0.2126f + color.green * 0.7152f +
            color.blue * 0.0722f;
        const float bright_weight =
            0.55f + EdgeGlowSmoothstep(luma) * 0.45f;
        const float kernel_weight = static_cast<float>(
            region_weights[x] * region_weights[y]);
        current.color[i].red +=
            color.red * bright_weight * kernel_weight;
        current.color[i].green +=
            color.green * bright_weight * kernel_weight;
        current.color[i].blue +=
            color.blue * bright_weight * kernel_weight;
      }
    }
    current.color[i].red *= kRegionWeightScale;
    current.color[i].green *= kRegionWeightScale;
    current.color[i].blue *= kRegionWeightScale;
  }

  if (!v3dcrt::UpdateEdgeGlowTemporalFilter(
          current.color, CTimer::GetClockTicks64(),
          kEdgeGlowTemporalTimeConstantUs, kEdgeGlowTemporalResetGapUs,
          &g_edge_glow_temporal_filter, samples->color)) {
    v3dcrt::ResetEdgeGlowTemporalFilter(&g_edge_glow_temporal_filter);
    return false;
  }

  if (log_samples) {
    printf("boot: pi5v3d edge glow field source=frame-uniform "
           "model=edge-local filter=%s regions=4 samples=36 "
           "inset_x1000=%u normal_radius_x1000=%u temporal_tau_ms=%u "
           "top_x1000=%d,%d,%d left_x1000=%d,%d,%d\r\n",
           linear_filter ? "linear" : "nearest",
           kEdgeGlowEdgeInsetX1000,
           kEdgeGlowEdgeNormalOffsetX1000,
           static_cast<u32>(kEdgeGlowTemporalTimeConstantUs / 1000ULL),
           (int)(samples->color[v3dcrt::kEdgeGlowFieldTop].red *
                 1000.0f + 0.5f),
           (int)(samples->color[v3dcrt::kEdgeGlowFieldTop].green *
                 1000.0f + 0.5f),
           (int)(samples->color[v3dcrt::kEdgeGlowFieldTop].blue *
                 1000.0f + 0.5f),
           (int)(samples->color[v3dcrt::kEdgeGlowFieldLeft].red *
                 1000.0f + 0.5f),
           (int)(samples->color[v3dcrt::kEdgeGlowFieldLeft].green *
                 1000.0f + 0.5f),
           (int)(samples->color[v3dcrt::kEdgeGlowFieldLeft].blue *
                 1000.0f + 0.5f));
  }
  return true;
}

void LogFragmentIntermediateHandoffSamples(
    const v3d71::Rgba8TextureLayout &source_layout) {
  if (source_layout.width != g_fragment_intermediate_layout.width ||
      source_layout.height != g_fragment_intermediate_layout.height ||
      source_layout.width == 0 || source_layout.height == 0) {
    printf("boot: pi5v3d fragment multipass handoff samples "
           "status=incompatible source=%ux%u intermediate=%ux%u\r\n",
           source_layout.width, source_layout.height,
           g_fragment_intermediate_layout.width,
           g_fragment_intermediate_layout.height);
    return;
  }

  const u32 x[] = {
    0, source_layout.width - 1U, 0, source_layout.width - 1U,
    source_layout.width / 2U
  };
  const u32 y[] = {
    0, 0, source_layout.height - 1U, source_layout.height - 1U,
    source_layout.height / 2U
  };
  u32 direct_matches = 0;
  u32 vertical_matches = 0;
  u32 rb_matches = 0;
  u32 vertical_rb_matches = 0;
  u32 first_source = 0;
  u32 first_intermediate = 0;
  u32 last_source = 0;
  u32 last_intermediate = 0;
  for (u32 i = 0; i < sizeof x / sizeof x[0]; ++i) {
    u32 source_offset = 0;
    u32 vertical_offset = 0;
    u32 intermediate_offset = 0;
    if (!v3d71::Rgba8TexturePixelOffset(
            source_layout, x[i], y[i], &source_offset) ||
        !v3d71::Rgba8TexturePixelOffset(
            source_layout, x[i], source_layout.height - 1U - y[i],
            &vertical_offset) ||
        !v3d71::Rgba8TexturePixelOffset(
            g_fragment_intermediate_layout, x[i], y[i],
            &intermediate_offset)) {
      printf("boot: pi5v3d fragment multipass handoff samples "
             "status=offset-failed sample=%u\r\n", i);
      return;
    }
    const u32 source =
        ReadLe32(g_fragment_source_scratch.cpu + source_offset);
    const u32 vertical =
        ReadLe32(g_fragment_source_scratch.cpu + vertical_offset);
    const u32 intermediate = ReadLe32(
        g_fragment_intermediate_scratch.cpu + intermediate_offset);
    direct_matches += intermediate == source ? 1U : 0U;
    vertical_matches += intermediate == vertical ? 1U : 0U;
    rb_matches += intermediate == SwapRgba8RedBlue(source) ? 1U : 0U;
    vertical_rb_matches +=
        intermediate == SwapRgba8RedBlue(vertical) ? 1U : 0U;
    if (i == 0) {
      first_source = source;
      first_intermediate = intermediate;
    }
    if (i + 1U == sizeof x / sizeof x[0]) {
      last_source = source;
      last_intermediate = intermediate;
    }
  }

  printf("boot: pi5v3d fragment multipass handoff samples status=ok "
         "count=%u direct=%u vertical=%u rb_swap=%u vertical_rb_swap=%u "
         "first=0x%08x/0x%08x last=0x%08x/0x%08x\r\n",
         (u32)(sizeof x / sizeof x[0]), direct_matches, vertical_matches,
         rb_matches, vertical_rb_matches,
         first_source, first_intermediate, last_source, last_intermediate);
}

void DisableRuntimeBloom(RenderFallbackReason reason,
                         const char *stage) {
  RecordRenderFallback(reason);
  g_runtime_bloom_failed = true;
  if (!g_runtime_bloom_failure_logged) {
    printf("boot: pi5v3d bloom disabled after runtime failure stage=%s; "
           "using validated two-pass output path\r\n",
           SafeString(stage));
    g_runtime_bloom_failure_logged = true;
  }
}

bool RenderBloomOutputPasses(
    const TextureSource &source,
    const OutputTarget &target,
    const FragmentReplayGeometry &output_geometry,
    Buffer &target_buffer,
    const v3dcrt::BloomPassPlan &plan,
    u64 frame_start_us,
    u64 staged_done_us,
    u64 source_prepared_us,
    u64 source_done_us,
    u64 edge_samples_done_us,
    const RenderParams &source_params,
    const shader_artifacts::CrtFragmentUniformValues &source_effect_values,
    const RenderParams &output_params,
    const EdgeGlowFrameSamples &edge_glow_samples,
    bool record_timing_sample,
    bool log_path) {
  if (!PrepareBloomReplayState(plan)) {
    DisableRuntimeBloom(kRenderFallbackBloomPrepare, "prepare");
    return false;
  }

  FragmentReplayRenderTarget base_target = {};
  FragmentReplayRenderTarget horizontal_target = {};
  FragmentReplayRenderTarget vertical_target = {};
  FragmentReplayRenderTarget composite_target = {};
  RenderParams base_params = output_params;
  // Apply the rounded screen boundary only after the blurred light has been
  // recombined, otherwise the soft boundary is multiplied twice.
  base_params.enable_rounded_screen_mask = false;
  shader_artifacts::CrtFragmentUniformValues base_effect_values = {};
  if (!BuildRgba8FragmentRenderTarget(
          g_fragment_bloom_base_scratch,
          g_fragment_bloom_base_layout, &base_target) ||
      !BuildRgba8FragmentRenderTarget(
          g_fragment_bloom_horizontal_scratch,
          g_fragment_bloom_horizontal_layout, &horizontal_target) ||
      !BuildRgba8FragmentRenderTarget(
          g_fragment_bloom_vertical_scratch,
          g_fragment_bloom_vertical_layout, &vertical_target) ||
      !BuildRgb565FragmentRenderTarget(
          target_buffer, &composite_target) ||
      !UpdatePreparedFragmentFrameStateForContext(
          FragmentReplayContextForPass(kFragmentPassBloomBase),
          g_fragment_intermediate_scratch,
          g_fragment_intermediate_layout, base_params,
          &edge_glow_samples, base_target,
          true, "bloom-base", &base_effect_values) ||
      !UpdatePreparedBloomBlurFrameState(
          FragmentReplayContextForPass(kFragmentPassBloomHorizontal),
          g_fragment_bloom_base_scratch,
          g_fragment_bloom_base_layout, horizontal_target,
          "source_texel_x",
          1.0f / (float)g_fragment_bloom_base_layout.width,
          "bloom-horizontal", log_path) ||
      !UpdatePreparedBloomBlurFrameState(
          FragmentReplayContextForPass(kFragmentPassBloomVertical),
          g_fragment_bloom_horizontal_scratch,
          g_fragment_bloom_horizontal_layout, vertical_target,
          "source_texel_y",
          1.0f / (float)g_fragment_bloom_horizontal_layout.height,
          "bloom-vertical", log_path) ||
      !UpdatePreparedBloomCompositeFrameState(
          g_fragment_bloom_base_scratch,
          g_fragment_bloom_base_layout,
          g_fragment_bloom_vertical_scratch,
          g_fragment_bloom_vertical_layout,
          composite_target, plan, output_params, log_path)) {
    DisableRuntimeBloom(kRenderFallbackBloomFrameState, "frame-state");
    return false;
  }
  const u64 output_prepared_us = CTimer::GetClockTicks64();

  u64 base_done_us = 0;
  u64 horizontal_done_us = 0;
  u64 vertical_done_us = 0;
  u64 composite_done_us = 0;
  if (!SubmitPreparedFragmentReplayPassForContext(
          FragmentReplayContextForPass(kFragmentPassBloomBase), target,
          g_fragment_bloom_base_scratch, false, false, false, false,
          log_path, &base_done_us)) {
    DisableRuntimeBloom(kRenderFallbackBloomFrameSubmit, "base-submit");
    return false;
  }
  DataSyncBarrier();
  if (!SubmitPreparedFragmentReplayPassForContext(
          FragmentReplayContextForPass(kFragmentPassBloomHorizontal), target,
          g_fragment_bloom_horizontal_scratch, false, false, false, false,
          log_path, &horizontal_done_us)) {
    DisableRuntimeBloom(kRenderFallbackBloomFrameSubmit,
                        "horizontal-submit");
    return false;
  }
  DataSyncBarrier();
  if (!SubmitPreparedFragmentReplayPassForContext(
          FragmentReplayContextForPass(kFragmentPassBloomVertical), target,
          g_fragment_bloom_vertical_scratch, false, false, false, false,
          log_path, &vertical_done_us)) {
    DisableRuntimeBloom(kRenderFallbackBloomFrameSubmit, "vertical-submit");
    return false;
  }
  DataSyncBarrier();
  if (!SubmitPreparedFragmentReplayPassForContext(
          FragmentReplayContextForPass(kFragmentPassBloomComposite), target, target_buffer,
          true, true, false, log_path, log_path, &composite_done_us)) {
    DisableRuntimeBloom(kRenderFallbackBloomFrameSubmit,
                        "composite-submit");
    return false;
  }

  const u64 done_us = composite_done_us != 0 ?
      composite_done_us : CTimer::GetClockTicks64();
  if (base_done_us == 0) {
    base_done_us = output_prepared_us;
  }
  if (horizontal_done_us == 0) {
    horizontal_done_us = base_done_us;
  }
  if (vertical_done_us == 0) {
    vertical_done_us = horizontal_done_us;
  }
  const bool deferred = !target.allow_direct_scanout &&
      target.rendered_plane != nullptr &&
      target.rendered_plane->framebuffer_bus_address != 0;
  const bool direct = target.presented != nullptr && *target.presented;
  const char *scanout_mode = deferred ? "deferred-hvs" :
      (direct ? "direct-hvs" : "nearest-cpu");
  if (log_path) {
    printf("boot: pi5v3d frame fragment bloom multipass shader=%s "
           "source_format=%u source=%ux%u rect=%u,%u %ux%u "
           "source_pass=%ux%u filter=%s effect=%s "
           "base_pass=%ux%u effect=%s horizontal_pass=%ux%u "
           "vertical_pass=%ux%u "
           "downsample=%u factor_x100=%d composite=%ux%u "
           "rounded=%s target_index=%u target_va=0x%08x "
           "dst=%u,%u %ux%u display=%ux%u scanout=%s "
           "timing_us=stage:%u source_prepare:%u source_render:%u "
           "edge_sample:%u output_prepare:%u base:%u horizontal:%u "
           "vertical:%u composite_present:%u total:%u\r\n",
           ShaderPresetName(g_shader_preset), source.pixelmode,
           source.width, source.height,
           source.source_rect.x, source.source_rect.y,
           source.source_rect.width, source.source_rect.height,
           g_fragment_intermediate_layout.width,
           g_fragment_intermediate_layout.height,
           source_params.enable_interpolation ? "linear" : "nearest",
           FragmentEffectName(source_effect_values),
           output_geometry.width, output_geometry.height,
           FragmentEffectName(base_effect_values),
           plan.horizontal_width, plan.horizontal_height,
           plan.blur_width, plan.blur_height,
           v3dcrt::kBloomDownsampleFactor, ParamX100(plan.factor),
           output_geometry.width, output_geometry.height,
           output_params.enable_rounded_screen_mask ? "on" : "off",
           QpuTargetBufferIndex(target_buffer), target_buffer.v3d_address,
           target.destination_rect.x, target.destination_rect.y,
           target.destination_rect.width, target.destination_rect.height,
           target.display_width, target.display_height, scanout_mode,
           (u32)(staged_done_us - frame_start_us),
           (u32)(source_prepared_us - staged_done_us),
           (u32)(source_done_us - source_prepared_us),
           (u32)(edge_samples_done_us - source_done_us),
           (u32)(output_prepared_us - edge_samples_done_us),
           (u32)(base_done_us - output_prepared_us),
           (u32)(horizontal_done_us - base_done_us),
           (u32)(vertical_done_us - horizontal_done_us),
           (u32)(done_us - vertical_done_us),
           (u32)(done_us - frame_start_us));
    g_frame_path_logged = true;
  }
  g_fragment_bloom_path_log_valid = true;
  g_fragment_bloom_path_active = true;
  const u32 timing_signature = FragmentTimingSignature(
      source_effect_values, base_effect_values,
      source_params.enable_interpolation,
      output_params.enable_interpolation,
      true, plan.factor,
      output_params.enable_rounded_screen_mask,
      output_params.rounded_corner_radius,
      output_params.rounded_border_softness);
  const FragmentPassTimings fragment_pass_timings = {
    (u32)(staged_done_us - frame_start_us),
    (u32)(source_prepared_us - staged_done_us),
    (u32)(source_done_us - source_prepared_us),
    (u32)(edge_samples_done_us - source_done_us),
    (u32)(output_prepared_us - edge_samples_done_us),
    (u32)(done_us - output_prepared_us)
  };
  const BloomPassTimings bloom_pass_timings = {
    (u32)(source_done_us - source_prepared_us),
    (u32)(base_done_us - output_prepared_us),
    (u32)(horizontal_done_us - base_done_us),
    (u32)(vertical_done_us - horizontal_done_us),
    (u32)(done_us - vertical_done_us)
  };
  if (record_timing_sample) {
    RecordFragmentPassStats(
        "fragment-frame-5pass-bloom", timing_signature,
        fragment_pass_timings);
    RecordBloomPassStats(timing_signature, bloom_pass_timings);
  }
  RecordRenderStats("fragment-frame-5pass-bloom", "fragment-5pass",
                    frame_start_us, done_us,
                    (u32)(staged_done_us - frame_start_us),
                    (u32)(done_us - staged_done_us), 0);
  return true;
}

bool RenderSplitFragmentFrameToScanout(
    const TextureSource &source,
    const OutputTarget &target,
    const FragmentReplayGeometry &output_geometry,
    u32 staged_width,
    u32 staged_height,
    u64 frame_start_us,
    u64 staged_done_us,
    const RenderParams &params) {
  v3d71::Rgba8TextureLayout staged_layout = {};
  FragmentReplayRenderTarget source_target = {};
  if (!v3d71::ComputeRgba8TextureLayout(
          staged_width, staged_height, &staged_layout) ||
      !BuildRgba8FragmentRenderTarget(
          g_fragment_intermediate_scratch,
          g_fragment_intermediate_layout, &source_target)) {
    RecordRenderFallback(kRenderFallbackFragmentFrameState);
    return false;
  }

  const RenderParams output_params = ResolveFragmentEffectScope(params);
  v3dcrt::BloomPassPlan bloom_plan = {};
  if (!v3dcrt::BuildBloomPassPlan(
          output_geometry.width, output_geometry.height,
          output_params.enable_bloom, output_params.bloom_factor,
          &bloom_plan)) {
    RecordRenderFallback(kRenderFallbackFragmentFrameState);
    return false;
  }
  const bool bloom_requested = bloom_plan.enabled &&
      !g_runtime_bloom_failed;
  const bool bloom_path_changed = !g_fragment_bloom_path_log_valid ||
      g_fragment_bloom_path_active != bloom_requested;
  const bool first_frame_log = !g_frame_path_logged || bloom_path_changed;
  RenderParams source_params = ResolveFragmentSourceEffectScope(params);
  // Convergence and jitter use fractional source-texel offsets. Keep the
  // effect-free source path nearest, but interpolate active displacement.
  source_params.enable_interpolation = source_params.enable_convergence ||
      source_params.enable_horizontal_jitter;
  shader_artifacts::CrtFragmentUniformValues source_effect_values = {};
  if (!UpdatePreparedFragmentFrameStateForContext(
          FragmentReplayContextForPass(kFragmentPassSource), g_fragment_source_scratch,
          staged_layout,
          source_params, nullptr, source_target, false, "source-pass",
          &source_effect_values)) {
    RecordRenderFallback(kRenderFallbackFragmentFrameState);
    return false;
  }
  LogFragmentSourceEffectState(
      source_effect_values, source_params.enable_interpolation);
  const u64 source_prepared_us = CTimer::GetClockTicks64();

  u64 source_done_us = 0;
  if (!SubmitPreparedFragmentReplayPassForContext(
          FragmentReplayContextForPass(kFragmentPassSource), target,
          g_fragment_intermediate_scratch, false, false, false, false,
          first_frame_log, &source_done_us)) {
    RecordRenderFallback(kRenderFallbackFragmentFrameSubmit);
    return false;
  }
  if (source_done_us == 0) {
    source_done_us = CTimer::GetClockTicks64();
  }
  DataSyncBarrier();
  if (first_frame_log || output_params.enable_edge_glow) {
    // The next V3D pass is coherent through the V3D cache barrier. Only
    // invalidate the CPU cache when diagnostics or Edge Glow read pixels.
    InvalidateBufferFromV3D(g_fragment_intermediate_scratch);
  }
  if (first_frame_log) {
    LogFragmentIntermediateHandoffSamples(staged_layout);
  }

  EdgeGlowFrameSamples edge_glow_samples = {};
  if (!BuildEdgeGlowFrameSamples(
          g_fragment_intermediate_scratch,
          g_fragment_intermediate_layout,
          output_params.enable_edge_glow,
          output_params.enable_interpolation,
          first_frame_log,
          &edge_glow_samples)) {
    RecordRenderFallback(kRenderFallbackFragmentFrameState);
    return false;
  }
  const u64 edge_samples_done_us = CTimer::GetClockTicks64();

  Buffer &target_buffer = SelectNextQpuTargetBuffer();
  const bool target_select_logged = g_qpu_target_select_log_count < 4;
  if (target_select_logged) {
    printf("boot: pi5v3d fragment target select index=%u "
           "v3d_va=0x%08x hvs=0x%08x pitch=%u\r\n",
           QpuTargetBufferIndex(target_buffer),
           target_buffer.v3d_address,
           target_buffer.hvs_bus_address,
           target_buffer.pitch);
    ++g_qpu_target_select_log_count;
  }

  if (bloom_requested) {
    if (RenderBloomOutputPasses(
            source, target, output_geometry, target_buffer, bloom_plan,
            frame_start_us, staged_done_us, source_prepared_us,
            source_done_us, edge_samples_done_us,
            source_params, source_effect_values, output_params,
            edge_glow_samples,
            !target_select_logged,
            first_frame_log)) {
      return true;
    }
    if (g_fragment_fast_cubic_selected) {
      DisableFastCubicRuntime("bloom-output");
    }
  }

  FragmentReplayRenderTarget output_target = {};
  shader_artifacts::CrtFragmentUniformValues output_effect_values = {};
  if (!BuildRgb565FragmentRenderTarget(target_buffer, &output_target)) {
    RecordRenderFallback(kRenderFallbackFragmentFrameState);
    return false;
  }
  FragmentReplayContext output_context = FragmentReplayContextForPass(kFragmentPassOutput);
  bool output_state_ready =
      PrepareFragmentReplayRuntimeStateForContext(
          output_context, output_geometry, output_target) &&
      UpdatePreparedFragmentFrameStateForContext(
          output_context, g_fragment_intermediate_scratch,
          g_fragment_intermediate_layout, output_params,
          &edge_glow_samples, output_target,
          true, "output-pass", &output_effect_values);
  if (!output_state_ready && g_fragment_fast_cubic_selected) {
    DisableFastCubicRuntime("output-frame-state");
    output_context = FragmentReplayContextForPass(kFragmentPassOutput);
    output_state_ready =
        PrepareFragmentReplayRuntimeStateForContext(
            output_context, output_geometry, output_target) &&
        UpdatePreparedFragmentFrameStateForContext(
            output_context, g_fragment_intermediate_scratch,
            g_fragment_intermediate_layout, output_params,
            &edge_glow_samples, output_target,
            true, "output-pass", &output_effect_values);
  }
  if (!output_state_ready) {
    RecordRenderFallback(kRenderFallbackFragmentFrameState);
    return false;
  }
  const u64 output_prepared_us = CTimer::GetClockTicks64();

  u64 submit_done_us = 0;
  if (!SubmitPreparedFragmentReplayPassForContext(
          output_context, target, target_buffer, true,
          true, false, first_frame_log, first_frame_log, &submit_done_us)) {
    if (g_fragment_fast_cubic_selected) {
      DisableFastCubicRuntime("output-submit");
    }
    RecordRenderFallback(kRenderFallbackFragmentFrameSubmit);
    return false;
  }
  const u64 done_us = submit_done_us != 0 ?
      submit_done_us : CTimer::GetClockTicks64();

  const bool deferred = !target.allow_direct_scanout &&
      target.rendered_plane != nullptr &&
      target.rendered_plane->framebuffer_bus_address != 0;
  const bool direct = target.presented != nullptr && *target.presented;
  const char *scanout_mode = deferred ? "deferred-hvs" :
      (direct ? "direct-hvs" : "nearest-cpu");
  if (first_frame_log) {
    printf("boot: pi5v3d frame fragment multipass shader=%s "
           "source_format=%u source=%ux%u rect=%u,%u %ux%u "
           "source_pass=%ux%u filter=%s effect=%s "
           "intermediate=rgba8-v3d-tiled layout=%s padded=%ux%u "
           "bytes=%u va=0x%08x output_pass=%ux%u filter=%s effect=%s "
           "resolution=output output_scope=%s target_index=%u "
           "target_va=0x%08x hvs=0x%08x pitch=%u "
           "dst=%u,%u %ux%u display=%ux%u scanout=%s "
           "timing_us=stage:%u source_prepare:%u source_render:%u "
           "edge_sample:%u output_prepare:%u output_render:%u total:%u\r\n",
           ShaderPresetName(g_shader_preset), source.pixelmode,
           source.width, source.height,
           source.source_rect.x, source.source_rect.y,
           source.source_rect.width, source.source_rect.height,
           g_fragment_intermediate_layout.width,
           g_fragment_intermediate_layout.height,
           source_params.enable_interpolation ? "linear" : "nearest",
           FragmentEffectName(source_effect_values),
           v3d71::Rgba8TextureTilingName(
               g_fragment_intermediate_layout.tiling),
           g_fragment_intermediate_layout.padded_width,
           g_fragment_intermediate_layout.padded_height,
           g_fragment_intermediate_layout.size_bytes,
           g_fragment_intermediate_scratch.v3d_address,
           output_geometry.width, output_geometry.height,
           output_params.enable_interpolation ? "linear" : "nearest",
           FragmentEffectName(output_effect_values),
           OutputResolutionPathName(SelectedOutputResolutionPath()),
           QpuTargetBufferIndex(target_buffer),
           target_buffer.v3d_address, target_buffer.hvs_bus_address,
           target_buffer.pitch,
           target.destination_rect.x, target.destination_rect.y,
           target.destination_rect.width, target.destination_rect.height,
           target.display_width, target.display_height, scanout_mode,
           (u32)(staged_done_us - frame_start_us),
           (u32)(source_prepared_us - staged_done_us),
           (u32)(source_done_us - source_prepared_us),
           (u32)(edge_samples_done_us - source_done_us),
           (u32)(output_prepared_us - edge_samples_done_us),
           (u32)(done_us - output_prepared_us),
           (u32)(done_us - frame_start_us));
    g_frame_path_logged = true;
  }
  g_fragment_bloom_path_log_valid = true;
  g_fragment_bloom_path_active = false;
  const u32 timing_signature = FragmentTimingSignature(
      source_effect_values, output_effect_values,
      source_params.enable_interpolation,
      output_params.enable_interpolation,
      false, 0.0f,
      output_params.enable_rounded_screen_mask,
      output_params.rounded_corner_radius,
      output_params.rounded_border_softness);
  const FragmentPassTimings fragment_pass_timings = {
    (u32)(staged_done_us - frame_start_us),
    (u32)(source_prepared_us - staged_done_us),
    (u32)(source_done_us - source_prepared_us),
    (u32)(edge_samples_done_us - source_done_us),
    (u32)(output_prepared_us - edge_samples_done_us),
    (u32)(done_us - output_prepared_us)
  };
  if (!target_select_logged) {
    RecordFragmentPassStats(
        "fragment-frame-2pass", timing_signature, fragment_pass_timings);
  }
  RecordRenderStats("fragment-frame-2pass", "fragment-2pass",
                    frame_start_us, done_us,
                    (u32)(staged_done_us - frame_start_us),
                    (u32)(done_us - staged_done_us), 0);
  return true;
}

bool RenderFragmentFrameToScanout(const TextureSource &source,
                                  const OutputTarget &target,
                                  u32 staged_width,
                                  u32 staged_height,
                                  u64 frame_start_us,
                                  u64 staged_done_us,
                                  const RenderParams &params) {
  SelectFragmentOutputPackage(params);
  FragmentReplayGeometry render_geometry = {};
  bool output_resolution_active = false;
  bool geometry_ready = PrepareContinuousFragmentGeometry(
      target, staged_width, staged_height,
      &render_geometry, &output_resolution_active);
  if (!geometry_ready && g_fragment_fast_cubic_selected) {
    DisableFastCubicRuntime("geometry-prepare");
    geometry_ready = PrepareContinuousFragmentGeometry(
        target, staged_width, staged_height,
        &render_geometry, &output_resolution_active);
  }
  if (!geometry_ready) {
    RecordRenderFallback(kRenderFallbackFragmentFrameState);
    return false;
  }
  if (output_resolution_active &&
      SelectedOutputResolutionPath() ==
          kOutputResolutionPathSplitGeometry) {
    return RenderSplitFragmentFrameToScanout(
        source, target, render_geometry, staged_width, staged_height,
        frame_start_us, staged_done_us, params);
  }
  const RenderParams fragment_params = ResolveFragmentEffectScope(params);
  const bool first_frame_log = !g_frame_path_logged;
  Buffer &target_buffer = SelectNextQpuTargetBuffer();
  if (g_qpu_target_select_log_count < 4) {
    printf("boot: pi5v3d fragment target select index=%u "
           "v3d_va=0x%08x hvs=0x%08x pitch=%u\r\n",
           QpuTargetBufferIndex(target_buffer),
           target_buffer.v3d_address,
           target_buffer.hvs_bus_address,
           target_buffer.pitch);
    ++g_qpu_target_select_log_count;
  }

  shader_artifacts::CrtFragmentUniformValues effect_values = {};
  if (!UpdatePreparedFragmentFrameState(
          staged_width, staged_height, fragment_params, target_buffer,
          &effect_values)) {
    RecordRenderFallback(kRenderFallbackFragmentFrameState);
    return false;
  }

  u64 submit_done_us = 0;
  if (!SubmitPreparedFragmentReplayPass(
          target, target_buffer, true, first_frame_log, first_frame_log,
          &submit_done_us)) {
    RecordRenderFallback(kRenderFallbackFragmentFrameSubmit);
    return false;
  }
  const u64 done_us = submit_done_us != 0 ?
      submit_done_us : CTimer::GetClockTicks64();

  const bool deferred = !target.allow_direct_scanout &&
      target.rendered_plane != nullptr &&
      target.rendered_plane->framebuffer_bus_address != 0;
  const bool direct = target.presented != nullptr && *target.presented;
  const char *scanout_mode = deferred ? "deferred-hvs" :
      (direct ? "direct-hvs" : "nearest-cpu");
  if (first_frame_log) {
    printf("boot: pi5v3d frame fragment shader=%s source_format=%u "
           "source=%ux%u rect=%u,%u %ux%u texture=rgba8-v3d-tiled "
           "filter=%s effect=%s weight_x100=%d gap_x100=%d "
           "mask=%s mask_pattern=%u mask_brightness_x100=%d response=%s "
           "level_mapping=%u "
           "target_index=%u render=%ux%u resolution=%s output_scope=%s "
           "target_va=0x%08x hvs=0x%08x pitch=%u "
           "dst=%u,%u %ux%u display=%ux%u scanout=%s "
           "timing_us=stage:%u fragment_and_present:%u total:%u\r\n",
           ShaderPresetName(g_shader_preset),
           source.pixelmode,
           source.width,
           source.height,
           source.source_rect.x,
           source.source_rect.y,
           source.source_rect.width,
           source.source_rect.height,
           fragment_params.enable_interpolation ? "linear" : "nearest",
           FragmentEffectName(effect_values),
           ParamX100(effect_values.scanline_weight),
           ParamX100(effect_values.scanline_gap_brightness),
           FragmentPhosphorMaskEnabled(effect_values) ? "on" : "off",
           (u32)effect_values.phosphor_mask_pattern,
           ParamX100(effect_values.phosphor_mask_brightness),
           FragmentOutputResponseActive(effect_values) ?
               (effect_values.output_response_fast > 0.5f ?
                    "fast" : "accurate") : "off",
           (u32)effect_values.level_mapping,
           QpuTargetBufferIndex(target_buffer),
           render_geometry.width,
           render_geometry.height,
           output_resolution_active ? "output" : "source",
           output_resolution_active ?
               OutputResolutionPathName(SelectedOutputResolutionPath()) :
               "source",
           target_buffer.v3d_address,
           target_buffer.hvs_bus_address,
           target_buffer.pitch,
           target.destination_rect.x,
           target.destination_rect.y,
           target.destination_rect.width,
           target.destination_rect.height,
           target.display_width,
           target.display_height,
           scanout_mode,
           (u32)(staged_done_us - frame_start_us),
           (u32)(done_us - staged_done_us),
           (u32)(done_us - frame_start_us));
    g_frame_path_logged = true;
  }
  RecordRenderStats("fragment-frame", "fragment",
                    frame_start_us, done_us,
                    (u32)(staged_done_us - frame_start_us),
                    (u32)(done_us - staged_done_us),
                    0);
  return true;
}

bool RunFragmentReplayPass(const OutputTarget &target,
                           const FragmentReplayGeometry &geometry,
                           bool present_scanout,
                           bool use_source_texture,
                           bool fill_source_texture,
                           bool return_direct_presented) {
  using namespace shader_artifacts;

  if (g_target_scratch.cpu == nullptr || g_target_scratch.size == 0 ||
      geometry.width == 0 || geometry.height == 0 ||
      g_target_scratch.width < geometry.width ||
      g_target_scratch.height < geometry.height ||
      g_target_scratch.pitch < geometry.width * 2U) {
    printf("boot: pi5v3d fragment replay target too small "
           "mode=%s target=%ux%u pitch=%u requested=%ux%u\r\n",
           SafeString(geometry.log_name),
           g_target_scratch.width, g_target_scratch.height,
           g_target_scratch.pitch,
           geometry.width, geometry.height);
    return false;
  }
  if (!EnsureFragmentReplayTileScratch(geometry)) {
    return false;
  }

  ShaderArtifactBufferBinding bindings[kFragmentArtifactBindingCount];
  memset(bindings, 0, sizeof bindings);
  if (!BuildFragmentArtifactBindings(bindings,
                                     kFragmentArtifactBindingCount)) {
    printf("boot: pi5v3d fragment replay binding setup failed\r\n");
    return false;
  }

  ShaderArtifactResolvedPatch
      resolved[kFragmentArtifactResolvedPatchCapacity];
  ShaderArtifactMaterializeResult result;
  const char *reason = nullptr;
  const ShaderArtifact *artifact_ptr =
      GetPreparedFragmentShaderArtifact(&g_fragment_output.package);
  if (artifact_ptr == nullptr) {
    printf("boot: pi5v3d fragment replay package unavailable\r\n");
    return false;
  }
  const ShaderArtifact &artifact = *artifact_ptr;
  const bool materialized = MaterializeShaderArtifact(
      artifact,
      bindings,
      kFragmentArtifactBindingCount,
      resolved,
      kFragmentArtifactResolvedPatchCapacity,
      &result,
      &reason);
  if (!materialized) {
    printf("boot: pi5v3d fragment replay materialize failed "
           "artifact=%s status=%s reason=%s resolved=%u applied=%u "
           "data_blocks=%u address_patches=%u\r\n",
           SafeString(artifact.name),
           ShaderArtifactMaterializeStatusName(result.status),
           SafeString(reason),
           result.resolved_patch_points,
           result.applied_patch_words,
           result.data_blocks_copied,
           result.applied_address_word_patches);
    return false;
  }
  if (!BuildFragmentReplayShaderStateRecord(artifact, bindings,
                                            kFragmentArtifactBindingCount)) {
    printf("boot: pi5v3d fragment replay shader state record build failed\r\n");
    return false;
  }
  if (!PatchFragmentReplayGeometry(
          &g_fragment_output.package, artifact, bindings,
                                   kFragmentArtifactBindingCount, geometry)) {
    printf("boot: pi5v3d fragment replay geometry patch failed mode=%s\r\n",
           SafeString(geometry.log_name));
    return false;
  }
  if (use_source_texture) {
    if (fill_source_texture && !FillFragmentSourceTexture()) {
      printf("boot: pi5v3d fragment source texture fill failed\r\n");
      return false;
    }
    if (!PatchFragmentReplayTextureToSource(
            bindings, kFragmentArtifactBindingCount,
            4U, 4U, false, true)) {
      printf("boot: pi5v3d fragment source texture patch failed\r\n");
      return false;
    }
  }

  memset(g_target_scratch.cpu, 0, g_target_scratch.size);
  memset(g_fragment_output.tile_scratch.cpu, 0,
         g_fragment_output.tile_scratch.size);

  u32 bcl_start = 0;
  u32 bcl_end = 0;
  u32 generic_start = 0;
  u32 generic_end = 0;
  u32 rcl_start = 0;
  u32 rcl_end = 0;
  if (!BuildFragmentReplayBcl(bindings, geometry, &bcl_start, &bcl_end) ||
      !BuildFragmentReplayGenericTileList(bindings, g_target_scratch,
                                          &generic_start, &generic_end) ||
      !BuildFragmentReplayRcl(generic_start, generic_end, geometry,
                              &rcl_start, &rcl_end)) {
    printf("boot: pi5v3d fragment replay CL build failed\r\n");
    return false;
  }

  CleanBufferForV3D(g_fragment_output.control_scratch);
  CleanBufferForV3D(g_fragment_output.tile_scratch);
  CleanBufferForV3D(g_target_scratch);

  const u32 tiles_x = TilesForPixels(geometry.width);
  const u32 tiles_y = TilesForPixels(geometry.height);
  printf("boot: pi5v3d fragment replay CL ready mode=%s artifact=%s "
         "bcl=0x%08x..0x%08x bytes=%u rcl=0x%08x..0x%08x bytes=%u "
         "generic=0x%08x..0x%08x tile_alloc=0x%08x size=%u "
         "tsda=0x%08x shader_record=0x%08x target=0x%08x pitch=%u "
         "target_size=%ux%u tiles=%ux%u\r\n",
         SafeString(geometry.log_name),
         SafeString(artifact.name),
         bcl_start, bcl_end, bcl_end - bcl_start,
         rcl_start, rcl_end, rcl_end - rcl_start,
         generic_start, generic_end,
         g_fragment_output.tile_scratch.v3d_address,
         g_fragment_output.tile_layout.tile_alloc_bytes,
         g_fragment_output.tile_scratch.v3d_address +
             g_fragment_output.tile_layout.tsda_offset,
         bindings[kFragmentArtifactClBinding].v3d_address + 0x40,
         g_target_scratch.v3d_address,
         g_target_scratch.pitch,
         geometry.width,
         geometry.height,
         tiles_x, tiles_y);
  LogFragmentArtifactPatchedWords(
      &g_fragment_output.package, artifact, bindings);

  if (!SubmitFragmentReplayBcl(bcl_start, bcl_end, true)) {
    return false;
  }
  if (!SubmitSolidRcl(rcl_start, rcl_end, true)) {
    return false;
  }

  InvalidateBufferFromV3D(g_target_scratch);
  LogFragmentReplayTargetSamples(g_target_scratch, geometry);
  if (present_scanout) {
    if (PresentFragmentReplayTargetDirect(target, g_target_scratch,
                                          geometry, true)) {
      printf("boot: pi5v3d fragment scanout submitted and presented; %s\r\n",
             return_direct_presented ?
                 "runtime fragment probe complete" :
                 "normal emulator boot continues");
      return return_direct_presented;
    }
    if (PresentFragmentReplayTargetFallback(target, g_target_scratch,
                                            geometry, true)) {
      printf("boot: pi5v3d fragment scanout submitted and staged for "
             "KMS present; %s\r\n",
             return_direct_presented ?
                 "runtime fragment probe complete" :
                 "normal emulator boot continues");
      return true;
    }
    printf("boot: pi5v3d fragment scanout present failed; "
           "normal emulator boot continues\r\n");
    return false;
  }
  printf("boot: pi5v3d fragment replay submitted; "
         "normal emulator boot continues\r\n");
  return false;
}

bool RunFragmentReplayBootTest(const OutputTarget &target,
                               const FragmentReplayGeometry &geometry,
                               bool present_scanout,
                               bool use_source_texture) {
  return RunFragmentReplayPass(target, geometry, present_scanout,
                               use_source_texture,
                               use_source_texture,
                               false);
}

FragmentProbeFrameResult RenderFragmentProbeFrameToScanout(
    const OutputTarget &target,
    u32 staged_width,
    u32 staged_height,
    u64 frame_start_us,
    u64 staged_done_us) {
  bool has_nonblack = false;
  u32 sample_signature = 0;
  ++g_fragment_probe_attempts;
  if (!FillFragmentLiveProbeTexture(staged_width, staged_height,
                                    &has_nonblack, &sample_signature,
                                    false)) {
    g_fragment_probe_done = true;
    return kFragmentProbeFrameFailed;
  }
  if (!has_nonblack) {
    if (g_fragment_probe_attempts == 1 ||
        g_fragment_probe_attempts >= kFragmentProbeMaxAttempts) {
      printf("boot: pi5v3d fragment live probe waiting source "
             "attempt=%u/%u staged=%ux%u signature=0x%08x\r\n",
             g_fragment_probe_attempts,
             kFragmentProbeMaxAttempts,
             staged_width,
             staged_height,
             sample_signature);
    }
    if (g_fragment_probe_attempts >= kFragmentProbeMaxAttempts) {
      printf("boot: pi5v3d fragment live probe skipped after black source "
             "attempts=%u staged=%ux%u signature=0x%08x\r\n",
             g_fragment_probe_attempts,
             staged_width,
             staged_height,
             sample_signature);
      g_fragment_probe_done = true;
      return kFragmentProbeFrameDone;
    }
    return kFragmentProbeFrameWaiting;
  }

  g_fragment_probe_done = true;
  u64 submit_done_us = 0;
  OutputTarget probe_target = target;
  probe_target.wait_for_vblank =
      target.wait_for_vblank && g_fragment_probe_wait_for_vblank;
  const bool presented =
      SubmitPreparedFragmentReplayPass(probe_target, g_target_scratch,
                                       true, false, true,
                                       &submit_done_us);
  const u64 done_us = submit_done_us != 0 ?
      submit_done_us : CTimer::GetClockTicks64();
  if (!presented) {
    return kFragmentProbeFrameFailed;
  }

  if (!g_frame_path_logged) {
    printf("boot: pi5v3d frame fragment probe shader=%s "
           "source_staged=%ux%u source_texture=4x4-live attempts=%u "
           "source_sig=0x%08x "
           "target_va=0x%08x hvs=0x%08x pitch=%u dst=%u,%u %ux%u "
           "display=%ux%u wait_vblank=%u "
           "timing_us=stage:%u fragment:%u total:%u\r\n",
           ShaderPresetName(g_shader_preset),
           staged_width,
           staged_height,
           g_fragment_probe_attempts,
           sample_signature,
           g_target_scratch.v3d_address,
           g_target_scratch.hvs_bus_address,
           g_target_scratch.pitch,
           target.destination_rect.x,
           target.destination_rect.y,
           target.destination_rect.width,
           target.destination_rect.height,
           target.display_width,
           target.display_height,
           probe_target.wait_for_vblank ? 1U : 0U,
           (u32)(staged_done_us - frame_start_us),
           (u32)(done_us - staged_done_us),
           (u32)(done_us - frame_start_us));
    g_frame_path_logged = true;
  }
  RecordRenderStats("fragment-probe", "fragment",
                    frame_start_us, done_us,
                    (u32)(staged_done_us - frame_start_us),
                    (u32)(done_us - staged_done_us),
                    0);
  return kFragmentProbeFramePresented;
}

bool RunFragmentArtifactBootTest() {
  using namespace shader_artifacts;

  ShaderArtifactBufferBinding bindings[kFragmentArtifactBindingCount];
  memset(bindings, 0, sizeof bindings);
  if (!BuildFragmentArtifactBindings(bindings,
                                     kFragmentArtifactBindingCount)) {
    printf("boot: pi5v3d fragment artifact binding setup failed\r\n");
    return false;
  }

  ShaderArtifactResolvedPatch
      resolved[kFragmentArtifactResolvedPatchCapacity];
  ShaderArtifactMaterializeResult result;
  const char *reason = nullptr;
  const ShaderArtifact *artifact_ptr =
      GetPreparedFragmentShaderArtifact(&g_fragment_output.package);
  if (artifact_ptr == nullptr) {
    printf("boot: pi5v3d fragment artifact package unavailable\r\n");
    return false;
  }
  const ShaderArtifact &artifact = *artifact_ptr;
  const bool ok = MaterializeShaderArtifact(
      artifact,
      bindings,
      kFragmentArtifactBindingCount,
      resolved,
      kFragmentArtifactResolvedPatchCapacity,
      &result,
      &reason);
  if (!ok) {
    printf("boot: pi5v3d fragment artifact materialize failed "
           "artifact=%s status=%s reason=%s resolved=%u applied=%u "
           "data_blocks=%u address_patches=%u\r\n",
           SafeString(artifact.name),
           ShaderArtifactMaterializeStatusName(result.status),
           SafeString(reason),
           result.resolved_patch_points,
           result.applied_patch_words,
           result.data_blocks_copied,
           result.applied_address_word_patches);
    return false;
  }

  CleanBufferForV3D(g_fragment_output.control_scratch);
  printf("boot: pi5v3d fragment artifact materialize ok artifact=%s "
         "schema=%s stages=%u uniforms=%u samplers=%u data_blocks=%u "
         "patches=%u/%u applied=%u address_patches=%u "
         "required_resource=%u required_cl=%u "
         "required_sampler=%u comparison=%s\r\n",
         SafeString(artifact.name),
         SafeString(artifact.schema),
         result.stages_copied,
         result.uniforms_copied,
         result.samplers_copied,
         result.data_blocks_copied,
         result.resolved_patch_points,
         artifact.patch_point_count,
         result.applied_patch_words,
         result.applied_address_word_patches,
         result.required_resource_bytes,
         result.required_cl_bytes,
         result.required_sampler_bytes,
         artifact.comparison_compatible ? "yes" : "no");
  LogFragmentArtifactBindings(bindings, kFragmentArtifactBindingCount);
  LogFragmentArtifactPatchedWords(
      &g_fragment_output.package, artifact, bindings);
  LogFragmentArtifactResolvedPatches(resolved,
                                     result.resolved_patch_points);
  printf("boot: pi5v3d fragment artifact materialize-only; "
         "draw submit is intentionally disabled until GL_SHADER/"
         "primitive packet replay is implemented\r\n");
  return false;
}

bool RunQpuBootTest(const OutputTarget &target) {
  u32 cfg[7];
  if (!BuildQpuMagicStoreCsd(cfg)) {
    printf("boot: pi5v3d qpu CSD build failed\r\n");
    return false;
  }

  printf("boot: pi5v3d qpu CSD code=0x%08x words=%u "
         "uniforms=0x%08x output=0x%08x magic=0x%08x "
         "wg_size=%u wgs_per_sg=%u batches=%u\r\n",
         g_fragment_output.control_scratch.v3d_address + kQpuTestCodeOffset,
         QpuMagicStoreCodeWords(),
         cfg[6],
         g_target_scratch.v3d_address + kQpuTestOutputOffset,
         kQpuTestMagic,
         kQpuTestWorkgroupSize,
         kQpuTestWorkgroupsPerSupergroup,
         kQpuTestNumBatches);

  if (!SubmitQpuCsd(cfg)) {
    return false;
  }

  if (!VerifyQpuMagicStoreTarget()) {
    return false;
  }

  return CopyRgb565BufferToScanout(g_target_scratch, target, "qpu");
}

bool RunQpuFillBootTest(const OutputTarget &target) {
  u32 cfg[7];
  if (!BuildQpuFillCsd(cfg)) {
    printf("boot: pi5v3d qpu fill CSD build failed\r\n");
    return false;
  }

  printf("boot: pi5v3d qpu fill CSD code=0x%08x words=%u "
         "uniforms=0x%08x output=0x%08x color=0x%08x "
         "groups=%u expected_groups=%u wg_size=%u wgs_per_sg=%u "
         "batches=%u\r\n",
         g_fragment_output.control_scratch.v3d_address + kQpuTestCodeOffset,
         QpuFillRgb565WordsCodeWords(),
         cfg[6],
         g_target_scratch.v3d_address + kQpuTestOutputOffset,
         kQpuFillColorWord,
         g_target_scratch.size / kQpuFillBytesPerGroup,
         kQpuFillGroups,
         kQpuTestWorkgroupSize,
         kQpuTestWorkgroupsPerSupergroup,
         kQpuTestNumBatches);

  if (!SubmitQpuCsd(cfg)) {
    return false;
  }

  if (!VerifyQpuFillTarget()) {
    return false;
  }

  return CopyRgb565BufferToScanout(g_target_scratch, target, "qpu-fill");
}

bool VerifySolidTarget() {
  if (g_target_scratch.cpu == nullptr ||
      g_target_scratch.width == 0 || g_target_scratch.height == 0 ||
      g_target_scratch.pitch < g_target_scratch.width * 2U) {
    return false;
  }

  const u16 *first = (const u16 *)g_target_scratch.cpu;
  const u16 *mid = (const u16 *)(g_target_scratch.cpu +
      (g_target_scratch.height / 2U) * g_target_scratch.pitch) +
      (g_target_scratch.width / 2U);
  const u16 *last = (const u16 *)(g_target_scratch.cpu +
      (g_target_scratch.height - 1U) * g_target_scratch.pitch) +
      (g_target_scratch.width - 1U);
  const bool ok = *first != 0 && *mid != 0 && *last != 0;

  printf("boot: pi5v3d solid pixels first=0x%04x mid=0x%04x "
         "last=0x%04x expected_nonzero=%s\r\n",
         *first, *mid, *last, ok ? "yes" : "no");
  return ok;
}

bool CopyRgb565BufferToScanout(const Buffer &buffer,
                               const OutputTarget &target,
                               const char *label) {
  if (target.scanout == nullptr || target.scanout->pixels == nullptr ||
      target.scanout->depth != 16 || buffer.cpu == nullptr ||
      buffer.depth != 16 || buffer.pitch < buffer.width * 2U) {
    return false;
  }

  const u32 copy_width = Min32(buffer.width, target.scanout->width);
  const u32 copy_height = Min32(buffer.height, target.scanout->height);
  if (copy_width == 0 || copy_height == 0 || target.scanout->size == 0) {
    return false;
  }

  const u32 dst_x = (target.scanout->width - copy_width) / 2U;
  const u32 dst_y = (target.scanout->height - copy_height) / 2U;
  const u32 row_bytes = copy_width * 2U;

  memset(target.scanout->pixels, 0, target.scanout->size);
  for (u32 y = 0; y < copy_height; ++y) {
    memcpy(target.scanout->pixels + (dst_y + y) * target.scanout->pitch +
           dst_x * 2U,
           buffer.cpu + y * buffer.pitch,
           row_bytes);
  }

  pi5kms::FlushFramebuffer(*target.scanout);
  printf("boot: pi5v3d %s copied to scanout dst=%u,%u %ux%u "
         "scanout=%ux%u pitch=%u\r\n",
         label != nullptr ? label : "buffer",
         dst_x, dst_y, copy_width, copy_height,
         target.scanout->width, target.scanout->height,
         target.scanout->pitch);
  return true;
}

bool CopySolidTargetToScanout(const OutputTarget &target) {
  return CopyRgb565BufferToScanout(g_target_scratch, target, "solid");
}

bool RunSolidBootTest(const OutputTarget &target) {
  if (g_target_scratch.cpu == nullptr || g_target_scratch.size == 0) {
    return false;
  }

  memset(g_target_scratch.cpu, 0, g_target_scratch.size);
  CleanBufferForV3D(g_target_scratch);

  u32 rcl_start = 0;
  u32 rcl_end = 0;
  u32 generic_start = 0;
  u32 generic_end = 0;
  if (!BuildSolidRcl(&rcl_start, &rcl_end, &generic_start, &generic_end)) {
    printf("boot: pi5v3d solid RCL build failed\r\n");
    return false;
  }

  printf("boot: pi5v3d solid RCL start=0x%08x end=0x%08x bytes=%u "
         "generic=0x%08x..0x%08x target_va=0x%08x target_pitch=%u "
         "tiles=%ux%u\r\n",
         rcl_start, rcl_end, rcl_end - rcl_start,
         generic_start, generic_end, g_target_scratch.v3d_address,
         g_target_scratch.pitch, SolidTilesX(), SolidTilesY());

  if (!SubmitSolidRcl(rcl_start, rcl_end, true)) {
    return false;
  }

  InvalidateBufferFromV3D(g_target_scratch);
  if (!VerifySolidTarget()) {
    return false;
  }

  return CopySolidTargetToScanout(target);
}

u16 SourcePatternPixel(u32 x, u32 y, u32 width, u32 height) {
  if (x == 0 || y == 0 || x + 1U == width || y + 1U == height) {
    return 0xFFFF;
  }

  const u32 red = width > 1 ? (x * 255U) / (width - 1U) : 0;
  const u32 green = height > 1 ? (y * 255U) / (height - 1U) : 0;
  const bool checker = (((x / kSourcePatternTile) ^
                         (y / kSourcePatternTile)) & 1U) != 0;
  const u32 blue = checker ? 255U : 32U;
  return Rgb565(red, green, blue);
}

bool FillSourcePattern() {
  if (g_source_scratch.cpu == nullptr ||
      g_source_scratch.width == 0 || g_source_scratch.height == 0 ||
      g_source_scratch.depth != 16 ||
      g_source_scratch.pitch < g_source_scratch.width * 2U) {
    return false;
  }

  for (u32 y = 0; y < g_source_scratch.height; ++y) {
    u16 *row = (u16 *)(g_source_scratch.cpu + y * g_source_scratch.pitch);
    for (u32 x = 0; x < g_source_scratch.width; ++x) {
      row[x] = SourcePatternPixel(x, y, g_source_scratch.width,
                                  g_source_scratch.height);
    }
  }

  CleanBufferForV3D(g_source_scratch);
  return true;
}

bool VerifySourcePattern() {
  if (g_source_scratch.cpu == nullptr ||
      g_source_scratch.width == 0 || g_source_scratch.height == 0 ||
      g_source_scratch.pitch < g_source_scratch.width * 2U) {
    return false;
  }

  const u16 *first = (const u16 *)g_source_scratch.cpu;
  const u32 mid_x = g_source_scratch.width / 2U;
  const u32 mid_y = g_source_scratch.height / 2U;
  const u16 *mid = (const u16 *)(g_source_scratch.cpu +
      mid_y * g_source_scratch.pitch) + mid_x;
  const u32 last_x = g_source_scratch.width - 1U;
  const u32 last_y = g_source_scratch.height - 1U;
  const u16 *last = (const u16 *)(g_source_scratch.cpu +
      last_y * g_source_scratch.pitch) + last_x;
  const u16 expected_first =
      SourcePatternPixel(0, 0, g_source_scratch.width, g_source_scratch.height);
  const u16 expected_mid = SourcePatternPixel(mid_x, mid_y,
      g_source_scratch.width, g_source_scratch.height);
  const u16 expected_last = SourcePatternPixel(last_x, last_y,
      g_source_scratch.width, g_source_scratch.height);
  const bool ok = *first == expected_first &&
                  *mid == expected_mid &&
                  *last == expected_last;

  printf("boot: pi5v3d source pixels first=0x%04x/0x%04x "
         "mid=0x%04x/0x%04x last=0x%04x/0x%04x expected=%s\r\n",
         *first, expected_first, *mid, expected_mid, *last, expected_last,
         ok ? "yes" : "no");
  return ok;
}

bool RunSourceBootTest(const OutputTarget &target) {
  if (!FillSourcePattern()) {
    printf("boot: pi5v3d source pattern fill failed\r\n");
    return false;
  }

  printf("boot: pi5v3d source staged va=0x%08x cpu=0x%08x "
         "size=%u geom=%ux%u pitch=%u depth=%u\r\n",
         g_source_scratch.v3d_address,
         (u32)(uintptr)g_source_scratch.cpu,
         g_source_scratch.size,
         g_source_scratch.width,
         g_source_scratch.height,
         g_source_scratch.pitch,
         g_source_scratch.depth);

  if (!VerifySourcePattern()) {
    return false;
  }

  return CopyRgb565BufferToScanout(g_source_scratch, target, "source");
}

bool ResolveSourceRect(const TextureSource &source, Rect *rect) {
  if (rect == nullptr || source.pixels == nullptr ||
      source.width == 0 || source.height == 0 || source.pitch == 0) {
    return false;
  }

  Rect resolved = source.source_rect;
  if (resolved.width == 0 || resolved.height == 0) {
    resolved.x = 0;
    resolved.y = 0;
    resolved.width = source.width;
    resolved.height = source.height;
  }

  if (resolved.x >= source.width || resolved.y >= source.height ||
      resolved.width == 0 || resolved.height == 0 ||
      resolved.width > source.width - resolved.x ||
      resolved.height > source.height - resolved.y) {
    return false;
  }

  const u32 bytes_per_pixel = source.pixelmode == 1 ? 2U : 1U;
  if (source.pitch < source.width * bytes_per_pixel) {
    return false;
  }

  *rect = resolved;
  return true;
}

void LogStagedSourcePaletteIfChanged(const TextureSource &source,
                                     u32 staged_width,
                                     u32 staged_height) {
  if (source.pixelmode != 0 ||
      (g_source_palette_log_valid &&
       g_source_palette_log_generation == source.palette_generation &&
       g_source_palette_log_signature == source.palette_signature)) {
    return;
  }

  printf("boot: pi5v3d source palette generation=%u signature=0x%08x "
         "staged=%ux%u\r\n",
         source.palette_generation,
         source.palette_signature,
         staged_width,
         staged_height);
  g_source_palette_log_valid = true;
  g_source_palette_log_generation = source.palette_generation;
  g_source_palette_log_signature = source.palette_signature;
}

void LogSourceOutputResponseIfChanged(
    const TextureSource &source,
    const v3dcrt::OutputResponseParams &output_response) {
  static bool log_valid = false;
  static u32 logged_pixelmode = 0U;
  static v3dcrt::OutputResponseParams logged_params = {};
  if (log_valid && logged_pixelmode == source.pixelmode &&
      v3dcrt::OutputResponseParamsEqual(logged_params, output_response)) {
    return;
  }

  const char *path = source.pixelmode == 0 ? "indexed8-palette" :
      (output_response.enabled ? "rgb565-lut" : "rgb565-direct");
  printf("boot: pi5v3d source response=%s path=%s level_mapping=%u "
         "input_gamma_x100=%d output_gamma_x100=%d saturation_x100=%d "
         "black_x100=%d white_x100=%d\r\n",
         output_response.enabled ?
             (output_response.fast ? "fast" : "accurate") : "off",
         path, output_response.level_mapping,
         ParamX100(output_response.input_gamma),
         ParamX100(output_response.output_gamma),
         ParamX100(output_response.saturation),
         ParamX100(output_response.black_level),
         ParamX100(output_response.white_clip));
  log_valid = true;
  logged_pixelmode = source.pixelmode;
  logged_params = output_response;
}

bool StageTextureSource(
    const TextureSource &source,
    const v3dcrt::OutputResponseParams &output_response,
    u32 *staged_width,
    u32 *staged_height) {
  if (staged_width == nullptr || staged_height == nullptr ||
      g_source_scratch.cpu == nullptr ||
      g_source_scratch.depth != 16 ||
      g_source_scratch.pitch < g_source_scratch.width * 2U ||
      (source.pixelmode != 0 && source.pixelmode != 1) ||
      (source.pixelmode == 0 && source.pal565 == nullptr)) {
    return false;
  }

  Rect rect = {0, 0, 0, 0};
  if (!ResolveSourceRect(source, &rect) ||
      rect.width > g_source_scratch.width ||
      rect.height > g_source_scratch.height) {
    if (!g_frame_unsupported_logged) {
      printf("boot: pi5v3d frame unsupported source "
             "format=%u geom=%ux%u pitch=%u rect=%u,%u %ux%u "
             "scratch=%ux%u\r\n",
             source.pixelmode, source.width, source.height, source.pitch,
             source.source_rect.x, source.source_rect.y,
             source.source_rect.width, source.source_rect.height,
             g_source_scratch.width, g_source_scratch.height);
      g_frame_unsupported_logged = true;
    }
    return false;
  }

  if ((source.pixelmode == 0 &&
       !EnsureFragmentSourcePalette(source, output_response)) ||
      (source.pixelmode == 1 &&
       !EnsureFragmentSourceOutputResponseLut(output_response))) {
    return false;
  }

  for (u32 y = 0; y < rect.height; ++y) {
    u16 *dst = (u16 *)(g_source_scratch.cpu + y * g_source_scratch.pitch);
    const uint8_t *src = source.pixels + (rect.y + y) * source.pitch;
    if (source.pixelmode == 0) {
      src += rect.x;
      for (u32 x = 0; x < rect.width; ++x) {
        dst[x] = g_fragment_source_staging_cache.palette_rgb565[src[x]];
      }
    } else {
      src += rect.x * 2U;
      if (!output_response.enabled) {
        memcpy(dst, src, rect.width * 2U);
      } else {
        for (u32 x = 0; x < rect.width; ++x) {
          u16 pixel = 0U;
          memcpy(&pixel, src + x * 2U, sizeof pixel);
          dst[x] =
              g_fragment_source_staging_cache.output_response_lut[pixel];
        }
      }
    }
  }

  *staged_width = rect.width;
  *staged_height = rect.height;
  CleanBufferForV3D(g_source_scratch);
  LogStagedSourcePaletteIfChanged(source, rect.width, rect.height);
  return true;
}

bool StageFragmentTextureSource(
    const TextureSource &source,
    const v3dcrt::OutputResponseParams &output_response,
    u32 *staged_width,
    u32 *staged_height) {
  if (staged_width == nullptr || staged_height == nullptr ||
      (source.pixelmode != 0 && source.pixelmode != 1) ||
      (source.pixelmode == 0 && source.pal565 == nullptr)) {
    return false;
  }

  Rect rect = {0, 0, 0, 0};
  v3d71::Rgba8TextureLayout layout = {};
  if (!ResolveSourceRect(source, &rect) ||
      !v3d71::ComputeRgba8TextureLayout(rect.width, rect.height, &layout) ||
      !EnsureFragmentSourceBuffer(layout)) {
    if (!g_frame_unsupported_logged) {
      printf("boot: pi5v3d fragment frame unsupported source "
             "format=%u geom=%ux%u pitch=%u rect=%u,%u %ux%u "
             "scratch_bytes=%u\r\n",
             source.pixelmode, source.width, source.height, source.pitch,
             source.source_rect.x, source.source_rect.y,
             source.source_rect.width, source.source_rect.height,
             g_fragment_source_scratch.size);
      g_frame_unsupported_logged = true;
    }
    return false;
  }

  if ((source.pixelmode == 0 &&
       !EnsureFragmentSourcePalette(source, output_response)) ||
      (source.pixelmode == 1 &&
       !EnsureFragmentSourceOutputResponseLut(output_response))) {
    return false;
  }

  struct UploadContext {
    const TextureSource *source;
    Rect rect;
    const u32 *palette;
    const u16 *output_response_lut;
  } context = {
    &source,
    rect,
    g_fragment_source_staging_cache.palette,
    output_response.enabled ?
        g_fragment_source_staging_cache.output_response_lut : nullptr
  };
  const auto region_source = [](void *opaque, u32 origin_x, u32 origin_y,
                                u32 width, u32 height, u32 *rgba8,
                                u32 stride_words) -> bool {
    UploadContext *upload = static_cast<UploadContext *>(opaque);
    if (upload == nullptr || rgba8 == nullptr || stride_words < width) {
      return false;
    }
    for (u32 y = 0U; y < height; ++y) {
      const uint8_t *row = upload->source->pixels +
          (upload->rect.y + origin_y + y) * upload->source->pitch;
      if (upload->source->pixelmode == 0) {
        row += upload->rect.x + origin_x;
        for (u32 x = 0U; x < width; ++x) {
          rgba8[y * stride_words + x] = upload->palette[row[x]];
        }
      } else {
        row += (upload->rect.x + origin_x) * 2U;
        for (u32 x = 0U; x < width; ++x) {
          u16 rgb = 0U;
          memcpy(&rgb, row + x * 2U, sizeof rgb);
          if (upload->output_response_lut != nullptr) {
            rgb = upload->output_response_lut[rgb];
          }
          rgba8[y * stride_words + x] =
              v3d71::Rgba8TextureWordFromRgb565(rgb);
        }
      }
    }
    return true;
  };
  if (!v3d71::UploadRgba8TexturePhysical(
          g_fragment_source_scratch.cpu,
          g_fragment_source_scratch.size,
          layout, region_source, &context)) {
    return false;
  }

  *staged_width = rect.width;
  *staged_height = rect.height;
  CleanBufferRangeForV3D(g_fragment_source_scratch, 0, layout.size_bytes);
  LogStagedSourcePaletteIfChanged(source, rect.width, rect.height);
  return true;
}

struct Rgb565Channels {
  unsigned r;
  unsigned g;
  unsigned b;
};

unsigned LerpChannel(unsigned a, unsigned b, unsigned frac) {
  return ((a * (256U - frac)) + (b * frac) + 128U) >> 8;
}

Rgb565Channels UnpackRgb565(u16 rgb) {
  Rgb565Channels channels = {
    (unsigned)(rgb >> 11) & 0x1F,
    (unsigned)(rgb >> 5) & 0x3F,
    (unsigned)rgb & 0x1F
  };
  return channels;
}

u16 BilinearRgb565(u16 c00,
                   u16 c10,
                   u16 c01,
                   u16 c11,
                   unsigned frac_x,
                   unsigned frac_y) {
  const Rgb565Channels p00 = UnpackRgb565(c00);
  const Rgb565Channels p10 = UnpackRgb565(c10);
  const Rgb565Channels p01 = UnpackRgb565(c01);
  const Rgb565Channels p11 = UnpackRgb565(c11);

  const unsigned r0 = LerpChannel(p00.r, p10.r, frac_x);
  const unsigned g0 = LerpChannel(p00.g, p10.g, frac_x);
  const unsigned b0 = LerpChannel(p00.b, p10.b, frac_x);
  const unsigned r1 = LerpChannel(p01.r, p11.r, frac_x);
  const unsigned g1 = LerpChannel(p01.g, p11.g, frac_x);
  const unsigned b1 = LerpChannel(p01.b, p11.b, frac_x);

  return (u16)((LerpChannel(r0, r1, frac_y) << 11) |
               (LerpChannel(g0, g1, frac_y) << 5) |
               LerpChannel(b0, b1, frac_y));
}

int ParamX100(float value) {
  const float scaled = value * 100.0f;
  return (int)(scaled >= 0.0f ? scaled + 0.5f : scaled - 0.5f);
}

void ResetRenderStats() {
  memset(&g_render_stats, 0, sizeof g_render_stats);
  ResetFragmentPassStats();
  ResetBloomPassStats();
}

void ResetFragmentPassStats() {
  memset(&g_fragment_pass_stats, 0, sizeof g_fragment_pass_stats);
}

void ResetBloomPassStats() {
  memset(&g_bloom_pass_stats, 0, sizeof g_bloom_pass_stats);
}

void SelectRenderStatsScanoutMode(bool direct_scanout) {
  if (!g_render_stats_scanout_mode_valid ||
      g_render_stats_direct_scanout != direct_scanout) {
    g_render_stats_scanout_mode_valid = true;
    g_render_stats_direct_scanout = direct_scanout;
    ResetRenderStats();
  }
}

const char *RenderFallbackReasonName(RenderFallbackReason reason) {
  switch (reason) {
    case kRenderFallbackRuntimeQpuDisabled:
      return "runtime-qpu-disabled";
    case kRenderFallbackStageSource:
      return "stage-source";
    case kRenderFallbackSourceStageBlit:
      return "source-stage-blit";
    case kRenderFallbackQpuGeometry:
      return "qpu-geometry";
    case kRenderFallbackQpuCsdBuild:
      return "qpu-csd-build";
    case kRenderFallbackQpuSubmit:
      return "qpu-submit";
    case kRenderFallbackQpuVerify:
      return "qpu-verify";
    case kRenderFallbackQpuCpuScanout:
      return "qpu-cpu-scanout";
    case kRenderFallbackFragmentProbePending:
      return "fragment-probe-pending";
    case kRenderFallbackFragmentProbeDone:
      return "fragment-probe-done";
    case kRenderFallbackFragmentProbeSubmit:
      return "fragment-probe-submit";
    case kRenderFallbackRuntimeFragmentDisabled:
      return "runtime-fragment-disabled";
    case kRenderFallbackFragmentFrameState:
      return "fragment-frame-state";
    case kRenderFallbackFragmentFrameSubmit:
      return "fragment-frame-submit";
    case kRenderFallbackBloomPrepare:
      return "bloom-prepare";
    case kRenderFallbackBloomFrameState:
      return "bloom-frame-state";
    case kRenderFallbackBloomFrameSubmit:
      return "bloom-frame-submit";
    default:
      return "unknown";
  }
}

void RecordRenderFallback(RenderFallbackReason reason) {
  if (reason < 0 || reason >= kRenderFallbackCount) {
    return;
  }

  ++g_render_fallback_stats.total;
  ++g_render_fallback_stats.count[reason];
  if (!g_render_fallback_stats.logged[reason]) {
    printf("boot: pi5v3d render fallback reason=%s count=%u total=%u\r\n",
           RenderFallbackReasonName(reason),
           g_render_fallback_stats.count[reason],
           g_render_fallback_stats.total);
    g_render_fallback_stats.logged[reason] = true;
  }
}

void StartRenderStatsWindow(const char *path, const char *work_label,
                            u64 window_start_us) {
  const u32 total_frames = g_render_stats.total_frames;
  memset(&g_render_stats, 0, sizeof g_render_stats);
  g_render_stats.active = true;
  g_render_stats.shader = g_shader_preset;
  g_render_stats.path = path;
  g_render_stats.work_label = work_label;
  g_render_stats.direct_scanout = g_render_stats_direct_scanout;
  g_render_stats.window_start_us = window_start_us;
  g_render_stats.total_frames = total_frames;
}

void AccumulateRenderStat(u32 value, u32 count,
                          u32 *min_us, u32 *max_us, u64 *total_us) {
  if (count == 0 || value < *min_us) {
    *min_us = value;
  }
  if (count == 0 || value > *max_us) {
    *max_us = value;
  }
  *total_us += value;
}

u32 AverageRenderStat(u64 total_us, u32 frames) {
  return frames == 0 ? 0 : (u32)(total_us / frames);
}

u32 FpsX100(u32 frames, u64 elapsed_us) {
  if (frames == 0 || elapsed_us == 0) {
    return 0;
  }
  const u64 fps_x100 = ((u64)frames * 100U * 1000000U) / elapsed_us;
  return fps_x100 > 0xFFFFFFFFULL ? 0xFFFFFFFFU : (u32)fps_x100;
}

void LogRenderStats(u64 now_us) {
  if (!g_render_stats.active || g_render_stats.frames == 0) {
    return;
  }

  u64 elapsed_us = now_us - g_render_stats.window_start_us;
  if (elapsed_us == 0) {
    elapsed_us = 1;
  }

  printf("boot: pi5v3d stats shader=%s path=%s frames=%u total=%u "
         "scanout=%s scope=v3d-call elapsed_ms=%u fps_x100=%u "
         "stage_us=%u/%u/%u %s_us=%u/%u/%u "
         "scanout_us=%u/%u/%u total_us=%u/%u/%u\r\n",
         ShaderPresetName(g_render_stats.shader),
         g_render_stats.path,
         g_render_stats.frames,
         g_render_stats.total_frames,
         g_render_stats.direct_scanout ? "direct-hvs" : "deferred-hvs",
         (u32)(elapsed_us / 1000U),
         FpsX100(g_render_stats.frames, elapsed_us),
         g_render_stats.stage_min_us,
         AverageRenderStat(g_render_stats.stage_total_us,
                           g_render_stats.frames),
         g_render_stats.stage_max_us,
         g_render_stats.work_label,
         g_render_stats.work_min_us,
         AverageRenderStat(g_render_stats.work_total_us,
                           g_render_stats.frames),
         g_render_stats.work_max_us,
         g_render_stats.scanout_min_us,
         AverageRenderStat(g_render_stats.scanout_total_us,
                           g_render_stats.frames),
         g_render_stats.scanout_max_us,
         g_render_stats.total_min_us,
         AverageRenderStat(g_render_stats.total_total_us,
                           g_render_stats.frames),
         g_render_stats.total_max_us);
}

void LogBloomPassStats() {
  if (g_bloom_pass_stats.frames == 0) {
    return;
  }

  printf("boot: pi5v3d bloom pass stats frames=%u "
         "scanout=%s scope=v3d-call "
         "source_us=%u/%u/%u base_us=%u/%u/%u "
         "blur_h_us=%u/%u/%u blur_v_us=%u/%u/%u "
         "composite_us=%u/%u/%u\r\n",
         g_bloom_pass_stats.frames,
         g_render_stats_direct_scanout ? "direct-hvs" : "deferred-hvs",
         g_bloom_pass_stats.min_us[kBloomPassTimingSource],
         AverageRenderStat(
             g_bloom_pass_stats.total_us[kBloomPassTimingSource],
             g_bloom_pass_stats.frames),
         g_bloom_pass_stats.max_us[kBloomPassTimingSource],
         g_bloom_pass_stats.min_us[kBloomPassTimingBase],
         AverageRenderStat(
             g_bloom_pass_stats.total_us[kBloomPassTimingBase],
             g_bloom_pass_stats.frames),
         g_bloom_pass_stats.max_us[kBloomPassTimingBase],
         g_bloom_pass_stats.min_us[kBloomPassTimingHorizontal],
         AverageRenderStat(
             g_bloom_pass_stats.total_us[kBloomPassTimingHorizontal],
             g_bloom_pass_stats.frames),
         g_bloom_pass_stats.max_us[kBloomPassTimingHorizontal],
         g_bloom_pass_stats.min_us[kBloomPassTimingVertical],
         AverageRenderStat(
             g_bloom_pass_stats.total_us[kBloomPassTimingVertical],
             g_bloom_pass_stats.frames),
         g_bloom_pass_stats.max_us[kBloomPassTimingVertical],
         g_bloom_pass_stats.min_us[kBloomPassTimingComposite],
         AverageRenderStat(
             g_bloom_pass_stats.total_us[kBloomPassTimingComposite],
             g_bloom_pass_stats.frames),
         g_bloom_pass_stats.max_us[kBloomPassTimingComposite]);
}

void LogFragmentPassStats() {
  if (g_fragment_pass_stats.frames == 0) {
    return;
  }

  printf("boot: pi5v3d fragment split stats path=%s frames=%u "
         "scanout=%s scope=v3d-call "
         "stage_us=%u/%u/%u source_prepare_us=%u/%u/%u "
         "source_render_us=%u/%u/%u edge_sample_us=%u/%u/%u "
         "output_prepare_us=%u/%u/%u output_render_us=%u/%u/%u\r\n",
         SafeString(g_fragment_pass_stats.path),
         g_fragment_pass_stats.frames,
         g_render_stats_direct_scanout ? "direct-hvs" : "deferred-hvs",
         g_fragment_pass_stats.min_us[kFragmentPassTimingStage],
         AverageRenderStat(
             g_fragment_pass_stats.total_us[kFragmentPassTimingStage],
             g_fragment_pass_stats.frames),
         g_fragment_pass_stats.max_us[kFragmentPassTimingStage],
         g_fragment_pass_stats.min_us[kFragmentPassTimingSourcePrepare],
         AverageRenderStat(
             g_fragment_pass_stats.total_us[
                 kFragmentPassTimingSourcePrepare],
             g_fragment_pass_stats.frames),
         g_fragment_pass_stats.max_us[kFragmentPassTimingSourcePrepare],
         g_fragment_pass_stats.min_us[kFragmentPassTimingSourceRender],
         AverageRenderStat(
             g_fragment_pass_stats.total_us[
                 kFragmentPassTimingSourceRender],
             g_fragment_pass_stats.frames),
         g_fragment_pass_stats.max_us[kFragmentPassTimingSourceRender],
         g_fragment_pass_stats.min_us[kFragmentPassTimingEdgeSample],
         AverageRenderStat(
             g_fragment_pass_stats.total_us[
                 kFragmentPassTimingEdgeSample],
             g_fragment_pass_stats.frames),
         g_fragment_pass_stats.max_us[kFragmentPassTimingEdgeSample],
         g_fragment_pass_stats.min_us[kFragmentPassTimingOutputPrepare],
         AverageRenderStat(
             g_fragment_pass_stats.total_us[
                 kFragmentPassTimingOutputPrepare],
             g_fragment_pass_stats.frames),
         g_fragment_pass_stats.max_us[kFragmentPassTimingOutputPrepare],
         g_fragment_pass_stats.min_us[kFragmentPassTimingOutputRender],
         AverageRenderStat(
             g_fragment_pass_stats.total_us[
                 kFragmentPassTimingOutputRender],
             g_fragment_pass_stats.frames),
         g_fragment_pass_stats.max_us[kFragmentPassTimingOutputRender]);
}

void RecordFragmentPassStats(const char *path, u32 signature,
                             const FragmentPassTimings &timings) {
  const bool matching_window =
      g_fragment_pass_stats.path == path &&
      g_fragment_pass_stats.signature == signature;
  if (g_fragment_pass_stats.complete && matching_window) {
    return;
  }
  if (!g_fragment_pass_stats.active || !matching_window) {
    memset(&g_fragment_pass_stats, 0, sizeof g_fragment_pass_stats);
    g_fragment_pass_stats.active = true;
    g_fragment_pass_stats.path = path;
    g_fragment_pass_stats.signature = signature;
    return;
  }

  const u32 count = g_fragment_pass_stats.frames;
  const u32 values[kFragmentPassTimingCount] = {
    timings.stage_us,
    timings.source_prepare_us,
    timings.source_render_us,
    timings.edge_sample_us,
    timings.output_prepare_us,
    timings.output_render_us
  };
  for (u32 i = 0; i < kFragmentPassTimingCount; ++i) {
    AccumulateRenderStat(
        values[i], count, &g_fragment_pass_stats.min_us[i],
        &g_fragment_pass_stats.max_us[i],
        &g_fragment_pass_stats.total_us[i]);
  }

  ++g_fragment_pass_stats.frames;
  if (g_fragment_pass_stats.frames >= kRenderStatsLogIntervalFrames) {
    LogFragmentPassStats();
    g_fragment_pass_stats.active = false;
    g_fragment_pass_stats.complete = true;
  }
}

void RecordBloomPassStats(u32 signature,
                          const BloomPassTimings &timings) {
  const bool matching_window =
      g_bloom_pass_stats.signature == signature;
  if (g_bloom_pass_stats.complete && matching_window) {
    return;
  }
  if (!g_bloom_pass_stats.active || !matching_window) {
    memset(&g_bloom_pass_stats, 0, sizeof g_bloom_pass_stats);
    g_bloom_pass_stats.active = true;
    g_bloom_pass_stats.signature = signature;
    return;
  }

  const u32 count = g_bloom_pass_stats.frames;
  const u32 values[kBloomPassTimingCount] = {
    timings.source_us,
    timings.base_us,
    timings.horizontal_us,
    timings.vertical_us,
    timings.composite_us
  };
  for (u32 i = 0; i < kBloomPassTimingCount; ++i) {
    AccumulateRenderStat(
        values[i], count, &g_bloom_pass_stats.min_us[i],
        &g_bloom_pass_stats.max_us[i], &g_bloom_pass_stats.total_us[i]);
  }

  ++g_bloom_pass_stats.frames;
  if (g_bloom_pass_stats.frames >= kRenderStatsLogIntervalFrames) {
    LogBloomPassStats();
    g_bloom_pass_stats.active = false;
    g_bloom_pass_stats.complete = true;
  }
}

void RecordRenderStats(const char *path, const char *work_label,
                       u64 frame_start_us, u64 frame_done_us,
                       u32 stage_us, u32 work_us, u32 scanout_us) {
  const bool matching_window =
      g_render_stats.shader == g_shader_preset &&
      g_render_stats.path == path &&
      g_render_stats.work_label == work_label &&
      g_render_stats.direct_scanout == g_render_stats_direct_scanout;
  if (g_render_stats.complete && matching_window) {
    return;
  }
  if (!g_render_stats.active || !matching_window) {
    // The first frame of a new path may allocate buffers, emit diagnostics,
    // or switch HVS composition. Start the steady window after it completes.
    StartRenderStatsWindow(path, work_label, frame_done_us);
    return;
  }

  const u32 count = g_render_stats.frames;
  const u32 total_us = (u32)(frame_done_us - frame_start_us);
  AccumulateRenderStat(stage_us, count, &g_render_stats.stage_min_us,
                       &g_render_stats.stage_max_us,
                       &g_render_stats.stage_total_us);
  AccumulateRenderStat(work_us, count, &g_render_stats.work_min_us,
                       &g_render_stats.work_max_us,
                       &g_render_stats.work_total_us);
  AccumulateRenderStat(scanout_us, count, &g_render_stats.scanout_min_us,
                       &g_render_stats.scanout_max_us,
                       &g_render_stats.scanout_total_us);
  AccumulateRenderStat(total_us, count, &g_render_stats.total_min_us,
                       &g_render_stats.total_max_us,
                       &g_render_stats.total_total_us);

  ++g_render_stats.frames;
  ++g_render_stats.total_frames;
  if (g_render_stats.frames >= kRenderStatsLogIntervalFrames) {
    LogRenderStats(frame_done_us);
    // UART output is blocking and this line is longer than one PAL frame.
    // Keep the timing useful without perturbing steady-state cadence.
    g_render_stats.active = false;
    g_render_stats.complete = true;
  }
}

bool IsSourceStageProgram(const ShaderProgram &program) {
  return program.execution_mode == kShaderExecutionSourceStage ||
         program.execution_mode == kShaderExecutionSourceStageEffect;
}

bool IsRuntimeFrameProgram(const ShaderProgram &program) {
  return IsSourceStageProgram(program) ||
         program.execution_mode == kShaderExecutionQpuFrameCopy ||
         program.execution_mode == kShaderExecutionQpuFrameEffect ||
         program.execution_mode == kShaderExecutionQpuFragment ||
         program.execution_mode == kShaderExecutionQpuFragmentFrame;
}

bool SelectedProgramUsesSourceStageEffects() {
  const ShaderProgram *program = GetShaderProgram(g_shader_preset);
  return program != nullptr &&
         program->execution_mode == kShaderExecutionSourceStageEffect;
}

bool SelectedProgramUsesRuntimeQpu() {
  const ShaderProgram *program = GetShaderProgram(g_shader_preset);
  return program != nullptr &&
         (program->execution_mode == kShaderExecutionQpuFrameCopy ||
          program->execution_mode == kShaderExecutionQpuFrameEffect);
}

bool SelectedProgramUsesRuntimeFragment() {
  const ShaderProgram *program = GetShaderProgram(g_shader_preset);
  return program != nullptr &&
         (program->execution_mode == kShaderExecutionQpuFragment ||
          program->execution_mode == kShaderExecutionQpuFragmentFrame);
}

bool SelectedProgramUsesContinuousRuntimeFragment() {
  const ShaderProgram *program = GetShaderProgram(g_shader_preset);
  return program != nullptr &&
         program->execution_mode == kShaderExecutionQpuFragmentFrame;
}

void MarkRuntimeQpuFailed() {
  g_runtime_qpu_failed = true;
  ResetRenderStats();
  if (!g_runtime_qpu_failure_logged) {
    printf("boot: pi5v3d runtime qpu failed; disabling V3D renderer "
           "for existing framebuffer fallback\r\n");
    g_runtime_qpu_failure_logged = true;
  }
}

void MarkRuntimeFragmentFailed() {
  g_runtime_fragment_failed = true;
  ResetRenderStats();
  if (!g_runtime_fragment_failure_logged) {
    printf("boot: pi5v3d runtime fragment failed; disabling V3D renderer "
           "for existing framebuffer fallback\r\n");
    g_runtime_fragment_failure_logged = true;
  }
}

RenderParams ApplyPresetDefaults(RenderParams params) {
  if (g_shader_preset == kShaderCrtSoft) {
    params.enable_scanlines = true;
    params.enable_output_response = true;
    params.enable_mask = false;
    params.phosphor_mask_pattern = 0U;
    if (params.scanline_weight <= 0.0f) {
      params.scanline_weight = 3.0f;
    } else if (params.scanline_weight > 3.0f) {
      params.scanline_weight = 3.0f;
    }
    if (params.scanline_gap_brightness <= 0.0f) {
      params.scanline_gap_brightness = 0.25f;
    }
    params.mask_brightness = 1.0f;
    if (params.input_gamma <= 0.0f) {
      params.input_gamma = 2.2f;
    }
    if (params.output_gamma <= 0.0f) {
      params.output_gamma = 2.2f;
    }
  }
  return params;
}

unsigned ClampU32(unsigned value, unsigned limit) {
  return value > limit ? limit : value;
}

u16 PackRgb565(unsigned r, unsigned g, unsigned b) {
  return (u16)((ClampU32(r, 31U) << 11) |
               (ClampU32(g, 63U) << 5) |
               ClampU32(b, 31U));
}

unsigned ScaleChannel(unsigned value, unsigned scale_256, unsigned limit) {
  return ClampU32((value * scale_256 + 128U) >> 8, limit);
}

struct SourceStageEffectState {
  bool active;
  bool scanlines_enabled;
  bool mask_enabled;
  bool soften_enabled;
  unsigned gamma_scale_256;
  unsigned scanline_scale_256;
  unsigned mask_scale_256;
  unsigned soften_neighbor_weight_256;
};

SourceStageEffectState BuildSourceStageEffectState(const RenderParams &params) {
  const bool soften_enabled = g_shader_preset == kShaderCrtSoft;
  SourceStageEffectState state = {
    false,
    params.enable_scanlines,
    params.enable_mask,
    soften_enabled,
    256U,
    256U,
    256U,
    soften_enabled ? 24U : 0U
  };

  if (params.enable_output_response) {
    const int input = ParamX100(params.input_gamma);
    const int output = ParamX100(params.output_gamma);
    if (input > 0 && output > 0) {
      state.gamma_scale_256 =
          (unsigned)((input * 256 + output / 2) / output);
      if (state.gamma_scale_256 > 384U) {
        state.gamma_scale_256 = 384U;
      }
    }
  }

  if (params.enable_scanlines) {
    unsigned darken = (unsigned)(ParamX100(params.scanline_weight) / 5);
    if (darken > 160U) {
      darken = 160U;
    }
    state.scanline_scale_256 = 256U - darken;
  }

  if (params.enable_mask) {
    int mask = ParamX100(params.mask_brightness);
    if (mask <= 0) {
      mask = 25;
    }
    unsigned darken = (unsigned)mask;
    if (darken > 160U) {
      darken = 160U;
    }
    state.mask_scale_256 = 256U - darken;
  }

  state.active = params.enable_output_response ||
                 (params.enable_scanlines &&
                  state.scanline_scale_256 != 256U) ||
                 (params.enable_mask && state.mask_scale_256 != 256U) ||
                 state.soften_enabled;
  return state;
}

u16 BlendSoftRgb565(u16 center,
                    u16 left,
                    u16 right,
                    unsigned neighbor_weight_256) {
  if (neighbor_weight_256 == 0U) {
    return center;
  }

  if (neighbor_weight_256 > 96U) {
    neighbor_weight_256 = 96U;
  }
  const unsigned center_weight_256 = 256U - 2U * neighbor_weight_256;
  const Rgb565Channels c = UnpackRgb565(center);
  const Rgb565Channels l = UnpackRgb565(left);
  const Rgb565Channels r = UnpackRgb565(right);

  return PackRgb565(
      (c.r * center_weight_256 + (l.r + r.r) * neighbor_weight_256 + 128U) >> 8,
      (c.g * center_weight_256 + (l.g + r.g) * neighbor_weight_256 + 128U) >> 8,
      (c.b * center_weight_256 + (l.b + r.b) * neighbor_weight_256 + 128U) >> 8);
}

u16 ApplySourceStageEffects(u16 rgb,
                            u32 dst_x,
                            unsigned row_scale_256,
                            const SourceStageEffectState &state);

bool BuildProcessedSoftSourceInScratch(u32 staged_width,
                                       u32 staged_height,
                                       const SourceStageEffectState &state,
                                       uint8_t **source_base) {
  if (source_base == nullptr || !state.soften_enabled ||
      state.soften_neighbor_weight_256 == 0U ||
      g_source_scratch.cpu == nullptr ||
      staged_width == 0 || staged_height == 0 ||
      staged_width > g_source_scratch.width ||
      staged_height > kSoftenedSourceYOffset ||
      kSoftenedSourceYOffset + staged_height > g_source_scratch.height) {
    return false;
  }

  uint8_t *soft_base =
      g_source_scratch.cpu + kSoftenedSourceYOffset * g_source_scratch.pitch;
  SourceStageEffectState source_effect_state = state;
  source_effect_state.soften_enabled = false;
  for (u32 y = 0; y < staged_height; ++y) {
    const u16 *src =
        (const u16 *)(g_source_scratch.cpu + y * g_source_scratch.pitch);
    u16 *dst = (u16 *)(soft_base + y * g_source_scratch.pitch);
    unsigned row_scale_256 = source_effect_state.gamma_scale_256;
    if (source_effect_state.scanlines_enabled && (y & 1U) != 0) {
      row_scale_256 =
          (row_scale_256 * source_effect_state.scanline_scale_256 + 128U) >> 8;
    }

    for (u32 x = 0; x < staged_width; ++x) {
      const u16 center = src[x];
      const u16 left = src[x > 0 ? x - 1U : x];
      const u16 right = src[x + 1U < staged_width ? x + 1U : x];
      u16 pixel = BlendSoftRgb565(center, left, right,
                                  state.soften_neighbor_weight_256);
      pixel = ApplySourceStageEffects(pixel, x, row_scale_256,
                                      source_effect_state);
      dst[x] = pixel;
    }
  }

  *source_base = soft_base;
  return true;
}

u16 ApplySourceStageEffects(u16 rgb,
                            u32 dst_x,
                            unsigned row_scale_256,
                            const SourceStageEffectState &state) {
  const Rgb565Channels channels = UnpackRgb565(rgb);
  unsigned r = channels.r;
  unsigned g = channels.g;
  unsigned b = channels.b;

  if (row_scale_256 != 256U) {
    r = ScaleChannel(r, row_scale_256, 31U);
    g = ScaleChannel(g, row_scale_256, 63U);
    b = ScaleChannel(b, row_scale_256, 31U);
  }

  if (state.mask_enabled) {
    switch (dst_x % 3U) {
      case 0:
        g = ScaleChannel(g, state.mask_scale_256, 63U);
        b = ScaleChannel(b, state.mask_scale_256, 31U);
        break;
      case 1:
        r = ScaleChannel(r, state.mask_scale_256, 31U);
        b = ScaleChannel(b, state.mask_scale_256, 31U);
        break;
      default:
        r = ScaleChannel(r, state.mask_scale_256, 31U);
        g = ScaleChannel(g, state.mask_scale_256, 63U);
        break;
    }
  }

  return PackRgb565(r, g, b);
}

void ClearScanoutOutsideDestination(pi5kms::Framebuffer *scanout,
                                    const Rect &destination) {
  if (scanout == nullptr || scanout->pixels == nullptr ||
      scanout->depth != 16 || scanout->pitch < scanout->width * 2U) {
    return;
  }

  const u32 row_bytes = scanout->width * 2U;
  const u32 left_bytes = destination.x * 2U;
  const u32 right_x = destination.x + destination.width;
  const u32 right_bytes =
      right_x < scanout->width ? (scanout->width - right_x) * 2U : 0U;
  const u32 bottom_y = destination.y + destination.height;

  for (u32 y = 0; y < destination.y; ++y) {
    memset(scanout->pixels + y * scanout->pitch, 0, row_bytes);
  }

  for (u32 y = destination.y; y < bottom_y; ++y) {
    uint8_t *row = scanout->pixels + y * scanout->pitch;
    if (left_bytes != 0) {
      memset(row, 0, left_bytes);
    }
    if (right_bytes != 0) {
      memset(row + right_x * 2U, 0, right_bytes);
    }
  }

  for (u32 y = bottom_y; y < scanout->height; ++y) {
    memset(scanout->pixels + y * scanout->pitch, 0, row_bytes);
  }
}

bool BlitRgb565BufferNearestToScanout(const Buffer &buffer,
                                      u32 source_width,
                                      u32 source_height,
                                      const OutputTarget &target) {
  if (target.scanout == nullptr || target.scanout->pixels == nullptr ||
      target.scanout->depth != 16 || buffer.cpu == nullptr ||
      buffer.depth != 16 || buffer.pitch < source_width * 2U ||
      target.scanout->pitch < target.scanout->width * 2U ||
      source_width == 0 || source_height == 0 ||
      source_width > buffer.width ||
      source_height > buffer.height ||
      target.destination_rect.width == 0 ||
      target.destination_rect.height == 0 ||
      target.destination_rect.x >= target.scanout->width ||
      target.destination_rect.y >= target.scanout->height ||
      target.destination_rect.width >
          target.scanout->width - target.destination_rect.x ||
      target.destination_rect.height >
          target.scanout->height - target.destination_rect.y) {
    return false;
  }

  ClearScanoutOutsideDestination(target.scanout, target.destination_rect);
  const u64 x_step = ((u64)source_width << 16) /
                     target.destination_rect.width;
  const u64 y_step = ((u64)source_height << 16) /
                     target.destination_rect.height;
  const u32 max_src_x = source_width - 1U;
  const u32 max_src_y = source_height - 1U;

  u64 y_acc = 0;
  for (u32 y = 0; y < target.destination_rect.height; ++y) {
    u32 src_y = (u32)(y_acc >> 16);
    if (src_y > max_src_y) {
      src_y = max_src_y;
    }
    const u16 *src =
        (const u16 *)(buffer.cpu + src_y * buffer.pitch);
    u16 *dst = (u16 *)(target.scanout->pixels +
        (target.destination_rect.y + y) * target.scanout->pitch) +
        target.destination_rect.x;

    u64 x_acc = 0;
    for (u32 x = 0; x < target.destination_rect.width; ++x) {
      u32 src_x = (u32)(x_acc >> 16);
      if (src_x > max_src_x) {
        src_x = max_src_x;
      }
      dst[x] = src[src_x];
      x_acc += x_step;
    }

    y_acc += y_step;
  }

  pi5kms::FlushFramebuffer(*target.scanout);
  return true;
}

bool BlitStagedSourceToScanout(const OutputTarget &target,
                               u32 staged_width,
                               u32 staged_height,
                               const RenderParams &params) {
  if (target.scanout == nullptr || target.scanout->pixels == nullptr ||
      target.scanout->depth != 16 || g_source_scratch.cpu == nullptr ||
      target.scanout->pitch < target.scanout->width * 2U ||
      staged_width == 0 || staged_height == 0 ||
      staged_width > g_source_scratch.width ||
      staged_height > g_source_scratch.height ||
      target.destination_rect.width == 0 ||
      target.destination_rect.height == 0 ||
      target.destination_rect.x >= target.scanout->width ||
      target.destination_rect.y >= target.scanout->height ||
      target.destination_rect.width >
          target.scanout->width - target.destination_rect.x ||
      target.destination_rect.height >
          target.scanout->height - target.destination_rect.y) {
    return false;
  }

  ClearScanoutOutsideDestination(target.scanout, target.destination_rect);
  const bool use_linear = params.enable_interpolation &&
      (staged_width != target.destination_rect.width ||
       staged_height != target.destination_rect.height);
  const SourceStageEffectState effect_state =
      BuildSourceStageEffectState(params);
  const bool use_effects = SelectedProgramUsesSourceStageEffects() &&
                           effect_state.active;
  uint8_t *source_base = g_source_scratch.cpu;
  SourceStageEffectState per_pixel_effect_state = effect_state;
  if (use_effects &&
      BuildProcessedSoftSourceInScratch(staged_width, staged_height,
                                        effect_state, &source_base)) {
    per_pixel_effect_state.active = false;
    per_pixel_effect_state.soften_enabled = false;
  }
  const bool use_per_pixel_effects = use_effects &&
                                     per_pixel_effect_state.active;
  const u64 x_step = ((u64)staged_width << 16) /
                     target.destination_rect.width;
  const u64 y_step = ((u64)staged_height << 16) /
                     target.destination_rect.height;
  const u32 max_src_x = staged_width - 1U;
  const u32 max_src_y = staged_height - 1U;

  u64 y_acc = 0;
  for (u32 y = 0; y < target.destination_rect.height; ++y) {
    u16 *dst = (u16 *)(target.scanout->pixels +
        (target.destination_rect.y + y) * target.scanout->pitch) +
        target.destination_rect.x;
    u32 src_y0 = (u32)(y_acc >> 16);
    if (src_y0 > max_src_y) {
      src_y0 = max_src_y;
    }
    const u32 src_y1 = src_y0 < max_src_y ? src_y0 + 1U : src_y0;
    const unsigned frac_y = use_linear ? (unsigned)((y_acc >> 8) & 0xFFU)
                                       : 0U;
    const u16 *src0 =
        (const u16 *)(source_base + src_y0 * g_source_scratch.pitch);
    const u16 *src1 =
        (const u16 *)(source_base + src_y1 * g_source_scratch.pitch);

    unsigned row_scale_256 = per_pixel_effect_state.gamma_scale_256;
    if (use_per_pixel_effects && per_pixel_effect_state.scanlines_enabled &&
        (src_y0 & 1U) != 0) {
      row_scale_256 =
          (row_scale_256 * per_pixel_effect_state.scanline_scale_256 + 128U) >>
          8;
    }

    u64 x_acc = 0;
    for (u32 x = 0; x < target.destination_rect.width; ++x) {
      u32 src_x0 = (u32)(x_acc >> 16);
      if (src_x0 > max_src_x) {
        src_x0 = max_src_x;
      }

      u16 pixel = 0;
      if (!use_linear) {
        pixel = src0[src_x0];
      } else {
        const u32 src_x1 = src_x0 < max_src_x ? src_x0 + 1U : src_x0;
        const unsigned frac_x = (unsigned)((x_acc >> 8) & 0xFFU);
        pixel = BilinearRgb565(src0[src_x0], src0[src_x1],
                               src1[src_x0], src1[src_x1],
                               frac_x, frac_y);
      }
      if (use_per_pixel_effects) {
        pixel = ApplySourceStageEffects(pixel,
                                        target.destination_rect.x + x,
                                        row_scale_256,
                                        per_pixel_effect_state);
      }
      dst[x] = pixel;
      x_acc += x_step;
    }

    y_acc += y_step;
  }

  pi5kms::FlushFramebuffer(*target.scanout);
  return true;
}

void LogFirstFrameStage(const TextureSource &source,
                        const OutputTarget &target,
                        u32 staged_width,
                        u32 staged_height,
                        const RenderParams &params,
                        u32 stage_us,
                        u32 blit_us) {
  if (g_frame_path_logged || g_source_scratch.cpu == nullptr ||
      staged_width == 0 || staged_height == 0) {
    return;
  }

  const u16 first = *(const u16 *)g_source_scratch.cpu;
  const u32 mid_x = staged_width / 2U;
  const u32 mid_y = staged_height / 2U;
  const u16 mid = *((const u16 *)(g_source_scratch.cpu +
      mid_y * g_source_scratch.pitch) + mid_x);
  const u16 last = *((const u16 *)(g_source_scratch.cpu +
      (staged_height - 1U) * g_source_scratch.pitch) +
      (staged_width - 1U));

  printf("boot: pi5v3d frame staged shader=%s filter=%s format=%u "
         "source=%ux%u pitch=%u rect=%u,%u %ux%u "
         "staged=%ux%u va=0x%08x samples=0x%04x,0x%04x,0x%04x "
         "dst=%u,%u %ux%u scanout=%ux%u "
         "effects=scanlines:%u,mask:%u,gamma:%u,curvature:%u,soft:%u "
         "weights_x100=%d,%d gap_x100=%d gamma_x100=%d,%d "
         "timing_us=stage:%u,blit:%u\r\n",
         ShaderPresetName(g_shader_preset),
         params.enable_interpolation ? "linear" : "nearest",
         source.pixelmode,
         source.width, source.height, source.pitch,
         source.source_rect.x, source.source_rect.y,
         source.source_rect.width, source.source_rect.height,
         staged_width, staged_height, g_source_scratch.v3d_address,
         first, mid, last,
         target.destination_rect.x, target.destination_rect.y,
         target.destination_rect.width, target.destination_rect.height,
         target.scanout != nullptr ? target.scanout->width : 0,
         target.scanout != nullptr ? target.scanout->height : 0,
         params.enable_scanlines ? 1U : 0U,
         params.enable_mask ? 1U : 0U,
         params.enable_output_response ? 1U : 0U,
         params.enable_geometry ? 1U : 0U,
         g_shader_preset == kShaderCrtSoft ? 1U : 0U,
         ParamX100(params.scanline_weight),
         ParamX100(params.mask_brightness),
         ParamX100(params.scanline_gap_brightness),
         ParamX100(params.input_gamma),
         ParamX100(params.output_gamma),
         stage_us,
         blit_us);
  g_frame_path_logged = true;
}

void LogFirstFrameQpuFrame(const TextureSource &source,
                           const OutputTarget &target,
                           const Buffer &target_buffer,
                           QpuFrameProgram program,
                           const QpuFrameJob &job,
                           const char *scanout_mode,
                           const char *filter_name,
                           u32 staged_width,
                           u32 staged_height,
                           u32 stage_us,
                           u32 qpu_us,
                           u32 scanout_us) {
  if (g_frame_path_logged) {
    return;
  }

  printf("boot: pi5v3d frame %s shader=%s source=%ux%u pitch=%u "
         "rect=%u,%u %ux%u staged=%ux%u copy=%ux%u "
         "target_va=0x%08x target_pitch=%u dst=%u,%u %ux%u "
         "target_index=%u scanout=%ux%u scanout_mode=%s "
         "filter=%s timing_us=stage:%u,qpu:%u,scanout:%u\r\n",
         QpuFrameProgramLogName(program),
         ShaderPresetName(g_shader_preset),
         source.width, source.height, source.pitch,
         source.source_rect.x, source.source_rect.y,
         source.source_rect.width, source.source_rect.height,
         staged_width, staged_height,
         job.width, job.height,
         target_buffer.v3d_address, target_buffer.pitch,
         target.destination_rect.x, target.destination_rect.y,
         target.destination_rect.width, target.destination_rect.height,
         QpuTargetBufferIndex(target_buffer),
         target.scanout != nullptr ? target.scanout->width : 0,
         target.scanout != nullptr ? target.scanout->height : 0,
         scanout_mode != nullptr ? scanout_mode : "unknown",
         filter_name != nullptr ? filter_name : "unknown",
         stage_us, qpu_us, scanout_us);
  g_frame_path_logged = true;
}

bool BuildQpuFrameTargetPlane(const OutputTarget &target,
                              const Buffer &target_buffer,
                              const QpuFrameJob &job,
                              pi5kms::ScaleFilter filter,
                              pi5kms::Plane *plane) {
  if (plane == nullptr ||
      target_buffer.hvs_bus_address == 0 ||
      target_buffer.depth != 16 ||
      target_buffer.width < job.width ||
      target_buffer.height < job.height ||
      target_buffer.pitch < job.row_bytes ||
      job.width == 0 ||
      job.height == 0 ||
      target.display_width == 0 || target.display_height == 0 ||
      target.destination_rect.width == 0 ||
      target.destination_rect.height == 0) {
    return false;
  }

  *plane = {
    target_buffer.hvs_bus_address,
    target_buffer.pitch,
    target_buffer.width,
    target_buffer.height,
    target_buffer.depth,
    pi5kms::kPixelFormatRgb565,
    filter,
    {0, 0, job.width, job.height},
    {(s32)target.destination_rect.x,
     (s32)target.destination_rect.y,
     target.destination_rect.width,
     target.destination_rect.height}
  };
  return true;
}

bool PresentQpuFrameTargetDirect(const OutputTarget &target,
                                 const Buffer &target_buffer,
                                 const QpuFrameJob &job,
                                 pi5kms::ScaleFilter filter,
                                 u64 *present_done_us) {
  if (!target.allow_direct_scanout) {
    return false;
  }
  if (g_direct_scanout_failed) {
    return false;
  }

  pi5kms::Plane plane;
  if (!BuildQpuFrameTargetPlane(target, target_buffer, job, filter, &plane)) {
    g_direct_scanout_failed = true;
    if (!g_direct_scanout_failure_logged) {
      printf("boot: pi5v3d direct scanout unavailable; falling back to "
             "CPU scanout copy\r\n");
      g_direct_scanout_failure_logged = true;
    }
    return false;
  }

  if (!pi5kms::PresentScanout(&plane, 1, target.display_width,
                              target.display_height,
                              target.wait_for_vblank)) {
    g_direct_scanout_failed = true;
    if (!g_direct_scanout_failure_logged) {
      printf("boot: pi5v3d direct scanout failed; falling back to "
             "CPU scanout copy\r\n");
      g_direct_scanout_failure_logged = true;
    }
    return false;
  }
  if (present_done_us != nullptr) {
    *present_done_us = CTimer::GetClockTicks64();
  }

  if (target.presented != nullptr) {
    *target.presented = true;
  }
  if (!g_direct_scanout_logged) {
    printf("boot: pi5v3d direct scanout target_index=%u "
           "hvs=0x%08x pitch=%u "
           "src=0,0 %ux%u dst=%u,%u %ux%u display=%ux%u "
           "filter=%s wait_vblank=%u\r\n",
           QpuTargetBufferIndex(target_buffer),
           target_buffer.hvs_bus_address,
           target_buffer.pitch,
           job.width, job.height,
           target.destination_rect.x, target.destination_rect.y,
           target.destination_rect.width, target.destination_rect.height,
           target.display_width, target.display_height,
           HvsScaleFilterName(filter),
           target.wait_for_vblank ? 1U : 0U);
    g_direct_scanout_logged = true;
  }
  return true;
}

bool WarmRuntimeQpuFrame(QpuFrameProgram program,
                         u32 staged_width,
                         u32 staged_height,
                         const RenderParams &params,
                         u64 *done_us) {
  if (done_us != nullptr) {
    *done_us = CTimer::GetClockTicks64();
  }
  if (g_qpu_warmup_attempted) {
    return g_qpu_warmup_done;
  }

  g_qpu_warmup_attempted = true;
  const u64 start_us = CTimer::GetClockTicks64();
  QpuFrameJob job;
  u32 cfg[7];
  const bool built = BuildQpuFrameJob(program, staged_width, staged_height,
                                      g_target_scratch, params, &job) &&
                     BuildQpuFrameCsd(program, job, g_target_scratch, cfg);
  bool submitted = false;
  bool verified = false;
  if (built) {
    submitted = SubmitQpuCsd(cfg, false);
    if (submitted) {
      verified = VerifyQpuFrameTarget(program, job, g_target_scratch, false);
    }
  }
  const u64 finish_us = CTimer::GetClockTicks64();
  if (done_us != nullptr) {
    *done_us = finish_us;
  }
  g_qpu_warmup_done = built && submitted && verified;

  printf("boot: pi5v3d runtime qpu warmup program=%s target_index=%u "
         "build=%s submit=%s verify=%s copy=%ux%u groups_per_row=%u "
         "scanline_x16=%u dark_masks=0x%08x,0x%08x,0x%08x,0x%08x "
         "timing_us=%u\r\n",
         QpuFrameProgramLogName(program),
         QpuTargetBufferIndex(g_target_scratch),
         built ? "ok" : "failed",
         submitted ? "ok" : "failed",
         verified ? "ok" : "failed",
         built ? job.width : 0,
         built ? job.height : 0,
         built ? job.groups_per_row : 0,
         built && program == kQpuFrameProgramScanlines ?
             job.scanline_scale_x16 : 0U,
         built && program == kQpuFrameProgramScanlines ?
             job.scanline_half_mask : 0U,
         built && program == kQpuFrameProgramScanlines ?
             job.scanline_quarter_mask : 0U,
         built && program == kQpuFrameProgramScanlines ?
             job.scanline_eighth_mask : 0U,
         built && program == kQpuFrameProgramScanlines ?
             job.scanline_sixteenth_mask : 0U,
         (u32)(finish_us - start_us));
  return g_qpu_warmup_done;
}

bool RenderQpuFrameToScanout(const TextureSource &source,
                             const OutputTarget &target,
                             QpuFrameProgram program,
                             u32 staged_width,
                             u32 staged_height,
                             u64 start_us,
                             u64 staged_us,
                             const RenderParams &params) {
  const bool first_frame_log = !g_frame_path_logged;
  Buffer &target_buffer = SelectNextQpuTargetBuffer();
  if (g_qpu_target_select_log_count < 4) {
    printf("boot: pi5v3d qpu target select index=%u "
           "v3d_va=0x%08x hvs=0x%08x pitch=%u\r\n",
           QpuTargetBufferIndex(target_buffer),
           target_buffer.v3d_address,
           target_buffer.hvs_bus_address,
           target_buffer.pitch);
    ++g_qpu_target_select_log_count;
  }

  QpuFrameJob job;
  u32 cfg[7];
  if (!BuildQpuFrameJob(program, staged_width, staged_height,
                        target_buffer, params, &job) ||
      !BuildQpuFrameCsd(program, job, target_buffer, cfg)) {
    RecordRenderFallback(kRenderFallbackQpuCsdBuild);
    return false;
  }
  LogQpuFrameProgramIfChanged(program, job);
  LogQpuScanlineParamsIfChanged(program, job);

  if (first_frame_log) {
    printf("boot: pi5v3d frame %s CSD code=0x%08x words=%u "
           "uniforms=0x%08x uniform_words=%u src=0x%08x dst=0x%08x "
           "target_index=%u rows=%u groups_per_row=%u "
           "src_skip=%u dst_skip=%u scanline_weight_x100=%d "
           "gap_x100=%d scanline_x16=%u "
           "dark_masks=0x%08x,0x%08x,0x%08x,0x%08x\r\n",
           QpuFrameProgramLogName(program),
           g_fragment_output.control_scratch.v3d_address + kQpuTestCodeOffset,
           QpuFrameProgramCodeWords(program),
           cfg[6],
           QpuFrameProgramUniformWords(program),
           g_source_scratch.v3d_address,
           target_buffer.v3d_address,
           QpuTargetBufferIndex(target_buffer),
           job.height,
           job.groups_per_row,
           job.source_row_skip,
           job.target_row_skip,
           job.scanline_weight_x100,
           job.scanline_gap_x100,
           program == kQpuFrameProgramScanlines ?
               job.scanline_scale_x16 : 0U,
           program == kQpuFrameProgramScanlines ?
               job.scanline_half_mask : 0U,
           program == kQpuFrameProgramScanlines ?
               job.scanline_quarter_mask : 0U,
           program == kQpuFrameProgramScanlines ?
               job.scanline_eighth_mask : 0U,
           program == kQpuFrameProgramScanlines ?
               job.scanline_sixteenth_mask : 0U);
  }

  const u64 qpu_start_us = CTimer::GetClockTicks64();
  if (!SubmitQpuCsd(cfg, false)) {
    RecordRenderFallback(kRenderFallbackQpuSubmit);
    return false;
  }
  const u64 qpu_done_us = CTimer::GetClockTicks64();

  if (first_frame_log &&
      !VerifyQpuFrameTarget(program, job, target_buffer, true)) {
    RecordRenderFallback(kRenderFallbackQpuVerify);
    return false;
  }
  if (!ProbeQpuFrameEffectUntilFound(program, job, target_buffer)) {
    RecordRenderFallback(kRenderFallbackQpuVerify);
    return false;
  }

  const pi5kms::ScaleFilter direct_filter =
      params.enable_interpolation ? pi5kms::kScaleFilterMitchell
                                  : pi5kms::kScaleFilterNearest;
  const u64 scanout_start_us = CTimer::GetClockTicks64();
  u64 scanout_done_us = 0;
  if (target.rendered_plane != nullptr) {
    BuildQpuFrameTargetPlane(target, target_buffer, job, direct_filter,
                             target.rendered_plane);
  }
  if (!target.allow_direct_scanout && target.rendered_plane != nullptr &&
      target.rendered_plane->framebuffer_bus_address != 0) {
    scanout_done_us = CTimer::GetClockTicks64();
    LogFirstFrameQpuFrame(source, target, target_buffer, program,
                          job, "deferred-hvs",
                          HvsScaleFilterName(direct_filter),
                          staged_width, staged_height,
                          (u32)(staged_us - start_us),
                          (u32)(qpu_done_us - qpu_start_us),
                          (u32)(scanout_done_us - scanout_start_us));
    if (!first_frame_log) {
      RecordRenderStats(QpuFrameStatsPath(program, "deferred-hvs"), "qpu",
                        start_us, scanout_done_us,
                        (u32)(staged_us - start_us),
                        (u32)(qpu_done_us - qpu_start_us),
                        (u32)(scanout_done_us - scanout_start_us));
    }
    return true;
  }

  bool direct_scanout = PresentQpuFrameTargetDirect(target, target_buffer,
                                                    job,
                                                    direct_filter,
                                                    &scanout_done_us);
  if (!direct_scanout) {
    if (target.presented != nullptr) {
      *target.presented = false;
    }
    if (!BlitRgb565BufferNearestToScanout(target_buffer, job.width,
                                          job.height, target)) {
      RecordRenderFallback(kRenderFallbackQpuCpuScanout);
      return false;
    }
    scanout_done_us = CTimer::GetClockTicks64();
  }
  const u64 done_us = scanout_done_us != 0 ? scanout_done_us
                                           : CTimer::GetClockTicks64();

  const char *scanout_mode = direct_scanout ? "direct-hvs" : "nearest-cpu";
  LogFirstFrameQpuFrame(source, target, target_buffer, program,
                        job,
                        scanout_mode,
                        direct_scanout ? HvsScaleFilterName(direct_filter)
                                       : "nearest",
                        staged_width, staged_height,
                        (u32)(staged_us - start_us),
                        (u32)(qpu_done_us - qpu_start_us),
                        (u32)(done_us - scanout_start_us));
  if (!first_frame_log) {
    RecordRenderStats(QpuFrameStatsPath(program, scanout_mode), "qpu",
                      start_us, done_us,
                      (u32)(staged_us - start_us),
                      (u32)(qpu_done_us - qpu_start_us),
                      (u32)(done_us - scanout_start_us));
  }
  return true;
}

const char *QpuFrameStatsPath(QpuFrameProgram program,
                              const char *scanout_mode) {
  if (program == kQpuFrameProgramScanlines) {
    if (strcmp(scanout_mode, "deferred-hvs") == 0) {
      return "qpu-scanlines-deferred-hvs";
    }
    if (strcmp(scanout_mode, "direct-hvs") == 0) {
      return "qpu-scanlines-hvs";
    }
    return "qpu-scanlines-copy";
  }

  if (strcmp(scanout_mode, "deferred-hvs") == 0) {
    return "qpu-copy-deferred-hvs";
  }
  if (strcmp(scanout_mode, "direct-hvs") == 0) {
    return "qpu-copy-hvs";
  }
  return "qpu-copy";
}

bool AllocateV3dAddressRange(u32 size, u32 alignment, u32 *address) {
  if (address == nullptr || size == 0 || !g_mmu_ready ||
      alignment == 0 || !IsPowerOfTwo(alignment)) {
    return false;
  }

  const u32 page_aligned_size = AlignUp32(size, kV3dMmuPageSize);
  const u32 aligned =
      AlignUp32(g_next_v3d_address, alignment > kV3dMmuPageSize
                                    ? alignment
                                    : kV3dMmuPageSize);
  if (aligned < kV3dFirstAddress ||
      aligned > 0xFFFFFFFFU - page_aligned_size ||
      (aligned + page_aligned_size) / kV3dMmuPageSize >
          kV3dPageTableEntries) {
    return false;
  }

  *address = aligned;
  g_next_v3d_address = aligned + page_aligned_size;
  return true;
}

bool MapBufferForV3D(Buffer *buffer) {
  if (buffer == nullptr || buffer->cpu == nullptr ||
      buffer->size == 0 || g_page_table == nullptr || !g_mmu_ready) {
    return false;
  }

  u32 v3d_address = 0;
  if (!AllocateV3dAddressRange(buffer->size, buffer->alignment,
                               &v3d_address)) {
    printf("boot: pi5v3d V3D VA allocation failed for %s size %u\r\n",
           BufferUsageName(buffer->usage), buffer->size);
    return false;
  }

  const u32 first_page = v3d_address >> kV3dMmuPageShift;
  const u32 page_count = AlignUp32(buffer->size, kV3dMmuPageSize) >>
                         kV3dMmuPageShift;
  const u64 bus = CpuToAxiBusAddress(buffer->cpu);
  if ((bus & (kV3dMmuPageSize - 1U)) != 0 ||
      (bus >> kV3dMmuPageShift) > 0x0FFFFFFFU ||
      first_page + page_count > kV3dPageTableEntries) {
    printf("boot: pi5v3d invalid V3D BO map bus=0x%08x%08x "
           "va=0x%08x pages=%u\r\n",
           (u32)(bus >> 32), (u32)bus, v3d_address, page_count);
    return false;
  }

  for (u32 i = 0; i < page_count; ++i) {
    g_page_table[first_page + i] =
        kV3dPteValid | kV3dPteWriteable |
        (u32)((bus >> kV3dMmuPageShift) + i);
  }

  CleanDataCacheRange((u64)(uintptr)&g_page_table[first_page],
                      page_count * sizeof(u32));
  if (!FlushV3dMmu()) {
    memset(&g_page_table[first_page], 0, page_count * sizeof(u32));
    CleanDataCacheRange((u64)(uintptr)&g_page_table[first_page],
                        page_count * sizeof(u32));
    FlushV3dMmu();
    return false;
  }

  buffer->axi_bus_address = bus;
  buffer->v3d_address = v3d_address;
  return true;
}

void UnmapBufferFromV3D(Buffer *buffer) {
  if (buffer == nullptr || buffer->v3d_address == 0 ||
      g_page_table == nullptr) {
    return;
  }

  const u32 first_page = buffer->v3d_address >> kV3dMmuPageShift;
  const u32 page_count = AlignUp32(buffer->size, kV3dMmuPageSize) >>
                         kV3dMmuPageShift;
  if (first_page + page_count <= kV3dPageTableEntries) {
    memset(&g_page_table[first_page], 0, page_count * sizeof(u32));
    CleanDataCacheRange((u64)(uintptr)&g_page_table[first_page],
                        page_count * sizeof(u32));
    FlushV3dMmu();
  }
  buffer->v3d_address = 0;
}

const char *BufferUsageName(BufferUsage usage) {
  switch (usage) {
    case kBufferUsageSource:
      return "source";
    case kBufferUsageRenderTarget:
      return "target";
    case kBufferUsageControl:
      return "control";
    default:
      return "unknown";
  }
}

void LogBuffer(const Buffer &buffer) {
  printf("boot: pi5v3d buffer %s cpu=0x%08x ",
         BufferUsageName(buffer.usage), (u32)(uintptr)buffer.cpu);
  PrintAddress64("axi", buffer.axi_bus_address);
  printf("v3d_va=0x%08x hvs=0x%08x legacy_bus=0x%08x "
         "size=%u alloc=%u align=%u "
         "geom=%ux%u pitch=%u depth=%u\r\n",
         buffer.v3d_address, buffer.hvs_bus_address,
         buffer.legacy_gpu_bus_address,
         buffer.size, buffer.allocation_size, buffer.alignment,
         buffer.width, buffer.height, buffer.pitch, buffer.depth);
}

bool InitializeMmu() {
  if (g_mmu_ready) {
    return true;
  }

  uint8_t *page_table_cpu = nullptr;
  if (!AllocateAlignedDma(kV3dPageTableBytes, kV3dMmuPageSize,
                          &g_page_table_allocation,
                          &page_table_cpu,
                          &g_page_table_allocation_size) ||
      !AllocateAlignedDma(kMmuScratchBytes, kV3dMmuPageSize,
                          &g_mmu_scratch_allocation,
                          &g_mmu_scratch,
                          &g_mmu_scratch_allocation_size)) {
    printf("boot: pi5v3d MMU allocation failed\r\n");
    delete[] g_page_table_allocation;
    delete[] g_mmu_scratch_allocation;
    g_page_table_allocation = nullptr;
    g_page_table = nullptr;
    g_mmu_scratch_allocation = nullptr;
    g_mmu_scratch = nullptr;
    g_page_table_allocation_size = 0;
    g_mmu_scratch_allocation_size = 0;
    return false;
  }

  g_page_table = (u32 *)page_table_cpu;
  g_page_table_bus_address = CpuToAxiBusAddress(g_page_table);
  g_mmu_scratch_bus_address = CpuToAxiBusAddress(g_mmu_scratch);
  g_next_v3d_address = kV3dFirstAddress;

  InitializeCoreState();
  CleanDataCacheRange((u64)(uintptr)g_page_table, kV3dPageTableBytes);
  CleanDataCacheRange((u64)(uintptr)g_mmu_scratch, kMmuScratchBytes);

  WriteReg(kV3dHubBase, kV3dMmuPtPaBase,
           (u32)(g_page_table_bus_address >> kV3dMmuPageShift));
  WriteReg(kV3dHubBase, kV3dMmuIllegalAddr,
           (u32)(g_mmu_scratch_bus_address >> kV3dMmuPageShift) |
           kV3dMmuIllegalAddrEnable);
  WriteReg(kV3dHubBase, kV3dMmuCtl,
           kV3dMmuCtlEnable |
           kV3dMmuCtlPtInvalidEnable |
           kV3dMmuCtlPtInvalidAbort |
           kV3dMmuCtlPtInvalidInt |
           kV3dMmuCtlWriteViolationAbort |
           kV3dMmuCtlWriteViolationInt |
           kV3dMmuCtlCapExceededAbort |
           kV3dMmuCtlCapExceededInt);
  WriteReg(kV3dHubBase, kV3dMmuControl, kV3dMmuControlEnable);

  g_mmu_ready = FlushV3dMmu();
  if (!g_mmu_logged) {
    printf("boot: pi5v3d mmu pt_cpu=0x%08x ",
           (u32)(uintptr)g_page_table);
    PrintAddress64("pt_axi", g_page_table_bus_address);
    printf("scratch_cpu=0x%08x ",
           (u32)(uintptr)g_mmu_scratch);
    PrintAddress64("scratch_axi", g_mmu_scratch_bus_address);
    printf("pt_bytes=%u scratch_bytes=%u pt_reg=0x%08x "
           "ctl=0x%08x mmuc=0x%08x ready=%s\r\n",
           kV3dPageTableBytes, kMmuScratchBytes,
           ReadReg(kV3dHubBase, kV3dMmuPtPaBase),
           ReadReg(kV3dHubBase, kV3dMmuCtl),
           ReadReg(kV3dHubBase, kV3dMmuControl),
           g_mmu_ready ? "yes" : "no");
    g_mmu_logged = true;
  }

  if (!g_mmu_ready) {
    return false;
  }

  return true;
}

void FreeMmu() {
  const bool had_mmu_state = g_page_table != nullptr || g_mmu_ready;
  g_mmu_ready = false;
  g_next_v3d_address = 0;
  if (g_page_table != nullptr) {
    memset(g_page_table, 0, kV3dPageTableBytes);
    CleanDataCacheRange((u64)(uintptr)g_page_table, kV3dPageTableBytes);
  }
  if (had_mmu_state) {
    WriteReg(kV3dHubBase, kV3dMmuControl, 0);
    WriteReg(kV3dHubBase, kV3dMmuCtl, 0);
    WriteReg(kV3dHubBase, kV3dMmuIllegalAddr, 0);
    WriteReg(kV3dHubBase, kV3dMmuPtPaBase, 0);
  }

  delete[] g_page_table_allocation;
  delete[] g_mmu_scratch_allocation;
  g_page_table_allocation = nullptr;
  g_page_table = nullptr;
  g_mmu_scratch_allocation = nullptr;
  g_mmu_scratch = nullptr;
  g_page_table_bus_address = 0;
  g_mmu_scratch_bus_address = 0;
  g_page_table_allocation_size = 0;
  g_mmu_scratch_allocation_size = 0;
  g_mmu_logged = false;
}

void SetBufferGeometry(Buffer *buffer, u32 width, u32 height,
                       u32 pitch, u32 depth) {
  if (buffer == nullptr) {
    return;
  }
  buffer->width = width;
  buffer->height = height;
  buffer->pitch = pitch;
  buffer->depth = depth;
}

bool ComputeFragmentTargetLayout(u32 width, u32 height,
                                 u32 *pitch, u32 *size) {
  if (pitch == nullptr || size == nullptr || width == 0 || height == 0 ||
      width > kOutputTargetMaxWidth || height > kOutputTargetMaxHeight) {
    return false;
  }

  u32 row_bytes = 0;
  u32 aligned_pitch = 0;
  u32 target_size = 0;
  if (!CheckedMul32(width, kScratchTargetDepth / 8U, &row_bytes) ||
      !CheckedAlignUp32(row_bytes, 64U, &aligned_pitch) ||
      !CheckedMul32(aligned_pitch, height, &target_size)) {
    return false;
  }

  *pitch = aligned_pitch;
  *size = target_size;
  return true;
}

bool FragmentTargetHasCapacity(const Buffer &buffer,
                               u32 width, u32 height, u32 pitch) {
  return buffer.cpu != nullptr && buffer.v3d_address != 0 &&
         buffer.hvs_bus_address != 0 && buffer.depth == kScratchTargetDepth &&
         buffer.width >= width && buffer.height >= height &&
         buffer.pitch >= pitch && buffer.size >= buffer.pitch * buffer.height;
}

bool Rgba8TextureLayoutsEqual(
    const v3d71::Rgba8TextureLayout &a,
    const v3d71::Rgba8TextureLayout &b) {
  return a.width == b.width && a.height == b.height &&
         a.padded_width == b.padded_width &&
         a.padded_height == b.padded_height &&
         a.padded_row_bytes == b.padded_row_bytes &&
         a.size_bytes == b.size_bytes &&
         a.array_stride_64_bytes == b.array_stride_64_bytes &&
         a.level_0_ub_pad == b.level_0_ub_pad && a.tiling == b.tiling;
}

bool EnsureFragmentReplayControlForContext(
    const FragmentReplayContext &context) {
  if (context.control_scratch == nullptr || context.prepared == nullptr) {
    return false;
  }
  if (context.control_scratch->cpu != nullptr) {
    return true;
  }

  Buffer replacement = {};
  if (!AllocateBuffer(kBufferUsageControl, kScratchControlBytes,
                      kBufferAlignment, &replacement)) {
    printf("boot: pi5v3d fragment context control allocation failed "
           "context=%s bytes=%u\r\n",
           SafeString(context.name), kScratchControlBytes);
    return false;
  }
  *context.control_scratch = replacement;
  ResetFragmentReplayPreparedState(context.prepared);
  LogBuffer(*context.control_scratch);
  return true;
}

bool EnsureRgba8FragmentTarget(
    Buffer *buffer,
    v3d71::Rgba8TextureLayout *layout,
    const FragmentReplayGeometry &geometry,
    const char *label,
    bool *changed) {
  if (buffer == nullptr || layout == nullptr || changed == nullptr) {
    return false;
  }
  *changed = false;

  v3d71::Rgba8TextureLayout required = {};
  v3d71::Rgba8RenderTargetStoreConfig store = {};
  if (!v3d71::ComputeRgba8TextureLayout(
          geometry.width, geometry.height, &required) ||
      !v3d71::GetRgba8RenderTargetStoreConfig(required, &store) ||
      (required.tiling != v3d71::kRgba8TextureUifNoXor &&
       required.tiling != v3d71::kRgba8TextureUifXor)) {
    printf("boot: pi5v3d rgba8 target layout unsupported label=%s "
           "requested=%ux%u\r\n",
           SafeString(label), geometry.width, geometry.height);
    return false;
  }

  const bool ready = buffer->cpu != nullptr &&
      buffer->v3d_address != 0 && buffer->depth == 32 &&
      buffer->width == required.width &&
      buffer->height == required.height &&
      buffer->pitch == required.padded_row_bytes &&
      buffer->size >= required.size_bytes &&
      Rgba8TextureLayoutsEqual(*layout, required);
  if (ready) {
    return true;
  }

  Buffer replacement = {};
  if (!AllocateBuffer(kBufferUsageRenderTarget, required.size_bytes,
                      kBufferAlignment, &replacement)) {
    printf("boot: pi5v3d rgba8 target allocation failed label=%s "
           "requested=%ux%u bytes=%u\r\n",
           SafeString(label), geometry.width, geometry.height,
           required.size_bytes);
    return false;
  }
  SetBufferGeometry(&replacement, required.width, required.height,
                    required.padded_row_bytes, 32);
  memset(replacement.cpu, 0, replacement.size);
  CleanBufferForV3D(replacement);

  FreeBuffer(buffer);
  *buffer = replacement;
  *layout = required;
  *changed = true;
  printf("boot: pi5v3d rgba8 target ready label=%s size=%ux%u "
         "layout=%s padded=%ux%u pitch=%u bytes=%u store_format=%u "
         "store_height_ub=%u va=0x%08x\r\n",
         SafeString(label), required.width, required.height,
         v3d71::Rgba8TextureTilingName(required.tiling),
         required.padded_width, required.padded_height,
         required.padded_row_bytes, required.size_bytes,
         store.memory_format, store.height_in_ub_or_stride,
         buffer->v3d_address);
  LogBuffer(*buffer);
  return true;
}

bool EnsureFragmentSourceBuffer(
    const v3d71::Rgba8TextureLayout &required) {
  if (required.width == 0 || required.height == 0 ||
      required.padded_row_bytes == 0 || required.size_bytes == 0) {
    return false;
  }

  const bool layout_changed =
      !Rgba8TextureLayoutsEqual(g_fragment_source_layout, required);
  const bool has_capacity =
      g_fragment_source_scratch.cpu != nullptr &&
      g_fragment_source_scratch.v3d_address != 0 &&
      g_fragment_source_scratch.depth == 32 &&
      g_fragment_source_scratch.size >= required.size_bytes;
  if (has_capacity) {
    if (layout_changed) {
      SetBufferGeometry(&g_fragment_source_scratch,
                        required.width, required.height,
                        required.padded_row_bytes, 32);
      g_fragment_source_layout = required;
      ResetFragmentReplayPreparedState(&g_fragment_source.prepared);
      g_fragment_frame_state_log_valid = false;
      g_frame_path_logged = false;
      ResetRenderStats();
      printf("boot: pi5v3d fragment source reconfigured size=%ux%u "
             "layout=%s padded=%ux%u pitch=%u bytes=%u capacity=%u "
             "va=0x%08x\r\n",
             required.width, required.height,
             v3d71::Rgba8TextureTilingName(required.tiling),
             required.padded_width, required.padded_height,
             required.padded_row_bytes, required.size_bytes,
             g_fragment_source_scratch.size,
             g_fragment_source_scratch.v3d_address);
    }
    return true;
  }

  Buffer replacement = {};
  if (!AllocateBuffer(kBufferUsageSource, required.size_bytes,
                      kBufferAlignment, &replacement)) {
    printf("boot: pi5v3d fragment source allocation failed "
           "requested=%ux%u bytes=%u\r\n",
           required.width, required.height, required.size_bytes);
    return false;
  }
  SetBufferGeometry(&replacement, required.width, required.height,
                    required.padded_row_bytes, 32);
  memset(replacement.cpu, 0, replacement.size);
  CleanBufferForV3D(replacement);

  const u32 old_capacity = g_fragment_source_scratch.size;
  FreeBuffer(&g_fragment_source_scratch);
  g_fragment_source_scratch = replacement;
  g_fragment_source_layout = required;
  ResetFragmentReplayPreparedState(&g_fragment_source.prepared);
  g_fragment_frame_state_log_valid = false;
  g_frame_path_logged = false;
  ResetRenderStats();
  printf("boot: pi5v3d fragment source resized old_capacity=%u "
         "size=%ux%u layout=%s padded=%ux%u pitch=%u bytes=%u "
         "va=0x%08x\r\n",
         old_capacity, required.width, required.height,
         v3d71::Rgba8TextureTilingName(required.tiling),
         required.padded_width, required.padded_height,
         required.padded_row_bytes, required.size_bytes,
         g_fragment_source_scratch.v3d_address);
  LogBuffer(g_fragment_source_scratch);
  return true;
}

bool EnsureFragmentIntermediateTarget(
    const FragmentReplayGeometry &geometry) {
  v3d71::Rgba8TextureLayout required = {};
  v3d71::Rgba8RenderTargetStoreConfig store = {};
  if (!v3d71::ComputeRgba8TextureLayout(
          geometry.width, geometry.height, &required) ||
      !v3d71::GetRgba8RenderTargetStoreConfig(required, &store) ||
      (required.tiling != v3d71::kRgba8TextureUifNoXor &&
       required.tiling != v3d71::kRgba8TextureUifXor)) {
    printf("boot: pi5v3d fragment intermediate layout unsupported "
           "mode=%s requested=%ux%u\r\n",
           SafeString(geometry.log_name), geometry.width, geometry.height);
    return false;
  }

  if (g_fragment_source.control_scratch.cpu == nullptr) {
    Buffer replacement_control = {};
    if (!AllocateBuffer(kBufferUsageControl, kScratchControlBytes,
                        kBufferAlignment, &replacement_control)) {
      printf("boot: pi5v3d fragment source-pass control allocation failed "
             "bytes=%u\r\n", kScratchControlBytes);
      return false;
    }
    g_fragment_source.control_scratch = replacement_control;
    ResetFragmentReplayPreparedState(&g_fragment_source.prepared);
    LogBuffer(g_fragment_source.control_scratch);
  }

  const bool target_ready =
      g_fragment_intermediate_scratch.cpu != nullptr &&
      g_fragment_intermediate_scratch.v3d_address != 0 &&
      g_fragment_intermediate_scratch.depth == 32 &&
      g_fragment_intermediate_scratch.width == required.width &&
      g_fragment_intermediate_scratch.height == required.height &&
      g_fragment_intermediate_scratch.pitch == required.padded_row_bytes &&
      g_fragment_intermediate_scratch.size >= required.size_bytes &&
      Rgba8TextureLayoutsEqual(g_fragment_intermediate_layout, required);
  if (target_ready) {
    return true;
  }

  Buffer replacement = {};
  if (!AllocateBuffer(kBufferUsageRenderTarget, required.size_bytes,
                      kBufferAlignment, &replacement)) {
    printf("boot: pi5v3d fragment intermediate allocation failed "
           "mode=%s requested=%ux%u bytes=%u\r\n",
           SafeString(geometry.log_name), geometry.width, geometry.height,
           required.size_bytes);
    return false;
  }
  SetBufferGeometry(&replacement, required.width, required.height,
                    required.padded_row_bytes, 32);
  memset(replacement.cpu, 0, replacement.size);
  CleanBufferForV3D(replacement);

  FreeBuffer(&g_fragment_intermediate_scratch);
  g_fragment_intermediate_scratch = replacement;
  g_fragment_intermediate_layout = required;
  ResetFragmentReplayPreparedState(&g_fragment_source.prepared);
  g_fragment_frame_state_log_valid = false;
  g_frame_path_logged = false;
  printf("boot: pi5v3d fragment intermediate ready size=%ux%u "
         "layout=%s padded=%ux%u pitch=%u bytes=%u store_format=%u "
         "store_height_ub=%u va=0x%08x\r\n",
         required.width, required.height,
         v3d71::Rgba8TextureTilingName(required.tiling),
         required.padded_width, required.padded_height,
         required.padded_row_bytes, required.size_bytes,
         store.memory_format, store.height_in_ub_or_stride,
         g_fragment_intermediate_scratch.v3d_address);
  LogBuffer(g_fragment_intermediate_scratch);
  return true;
}

bool EnsureFragmentRenderTargets(const FragmentReplayGeometry &geometry) {
  u32 target_pitch = 0;
  u32 target_size = 0;
  if (!ComputeFragmentTargetLayout(geometry.width, geometry.height,
                                   &target_pitch, &target_size)) {
    printf("boot: pi5v3d fragment target geometry unsupported "
           "mode=%s requested=%ux%u max=%ux%u\r\n",
           SafeString(geometry.log_name), geometry.width, geometry.height,
           kOutputTargetMaxWidth, kOutputTargetMaxHeight);
    return false;
  }

  if (FragmentTargetHasCapacity(g_target_scratch, geometry.width,
                                geometry.height, target_pitch) &&
      FragmentTargetHasCapacity(g_target_scratch_alt, geometry.width,
                                geometry.height, target_pitch)) {
    return true;
  }

  Buffer replacement = {};
  Buffer replacement_alt = {};
  if (!AllocateBuffer(kBufferUsageRenderTarget, target_size,
                      kBufferAlignment, &replacement) ||
      !AllocateBuffer(kBufferUsageRenderTarget, target_size,
                      kBufferAlignment, &replacement_alt)) {
    printf("boot: pi5v3d fragment target allocation failed "
           "mode=%s requested=%ux%u pitch=%u bytes_each=%u; "
           "keeping=%ux%u\r\n",
           SafeString(geometry.log_name), geometry.width, geometry.height,
           target_pitch, target_size,
           g_target_scratch.width, g_target_scratch.height);
    FreeBuffer(&replacement);
    FreeBuffer(&replacement_alt);
    return false;
  }

  SetBufferGeometry(&replacement, geometry.width, geometry.height,
                    target_pitch, kScratchTargetDepth);
  SetBufferGeometry(&replacement_alt, geometry.width, geometry.height,
                    target_pitch, kScratchTargetDepth);
  memset(replacement.cpu, 0, replacement.size);
  memset(replacement_alt.cpu, 0, replacement_alt.size);
  CleanBufferForV3D(replacement);
  CleanBufferForV3D(replacement_alt);

  const u32 old_width = g_target_scratch.width;
  const u32 old_height = g_target_scratch.height;
  ResetFragmentReplayPreparedState();
  ResetFragmentReplayPreparedState(&g_fragment_bloom_composite.prepared);
  FreeBuffer(&g_target_scratch);
  FreeBuffer(&g_target_scratch_alt);
  g_target_scratch = replacement;
  g_target_scratch_alt = replacement_alt;
  g_qpu_target_buffer_index = 0;
  g_qpu_target_select_log_count = 0;
  g_fragment_frame_state_log_valid = false;
  g_frame_path_logged = false;
  ResetRenderStats();

  printf("boot: pi5v3d fragment targets resized old=%ux%u "
         "new=%ux%u pitch=%u bytes_each=%u total=%u\r\n",
         old_width, old_height, geometry.width, geometry.height,
         target_pitch, target_size, target_size * 2U);
  LogBuffer(g_target_scratch);
  LogBuffer(g_target_scratch_alt);
  return true;
}

bool InitializeScratchBuffers() {
  if (g_buffers_ready) {
    return true;
  }

  memset(&g_source_scratch, 0, sizeof g_source_scratch);
  memset(&g_fragment_source_scratch, 0,
         sizeof g_fragment_source_scratch);
  memset(&g_target_scratch, 0, sizeof g_target_scratch);
  memset(&g_target_scratch_alt, 0, sizeof g_target_scratch_alt);
  memset(&g_fragment_intermediate_scratch, 0,
         sizeof g_fragment_intermediate_scratch);
  memset(&g_fragment_bloom_base_scratch, 0,
         sizeof g_fragment_bloom_base_scratch);
  memset(&g_fragment_bloom_horizontal_scratch, 0,
         sizeof g_fragment_bloom_horizontal_scratch);
  memset(&g_fragment_bloom_vertical_scratch, 0,
         sizeof g_fragment_bloom_vertical_scratch);
  memset(g_fragment_pass_resources, 0,
         sizeof g_fragment_pass_resources);
  memset(&g_fragment_intermediate_layout, 0,
         sizeof g_fragment_intermediate_layout);
  memset(&g_fragment_source_layout, 0,
         sizeof g_fragment_source_layout);
  memset(&g_fragment_bloom_base_layout, 0,
         sizeof g_fragment_bloom_base_layout);
  memset(&g_fragment_bloom_horizontal_layout, 0,
         sizeof g_fragment_bloom_horizontal_layout);
  memset(&g_fragment_bloom_vertical_layout, 0,
         sizeof g_fragment_bloom_vertical_layout);
  g_qpu_target_buffer_index = 0;
  g_qpu_target_select_log_count = 0;
  ResetQpuScanlineParamLog();

  const u32 target_pitch =
      AlignUp32(kScratchTargetWidth * (kScratchTargetDepth / 8U), 64);
  const u32 target_size = target_pitch * kScratchTargetHeight;

  if (!AllocateBuffer(kBufferUsageSource, kScratchSourceBytes,
                      kBufferAlignment, &g_source_scratch) ||
      !AllocateBuffer(kBufferUsageRenderTarget, target_size,
                      kBufferAlignment, &g_target_scratch) ||
      !AllocateBuffer(kBufferUsageRenderTarget, target_size,
                      kBufferAlignment, &g_target_scratch_alt) ||
      !AllocateBuffer(kBufferUsageControl, kScratchControlBytes,
                      kBufferAlignment, &g_fragment_output.control_scratch)) {
    printf("boot: pi5v3d scratch allocation failed\r\n");
    FreeBuffer(&g_source_scratch);
    FreeBuffer(&g_target_scratch);
    FreeBuffer(&g_target_scratch_alt);
    FreeBuffer(&g_fragment_output.control_scratch);
    return false;
  }

  SetBufferGeometry(&g_source_scratch, 512, 512, 512 * 2, 16);
  SetBufferGeometry(&g_target_scratch, kScratchTargetWidth,
                    kScratchTargetHeight, target_pitch, kScratchTargetDepth);
  SetBufferGeometry(&g_target_scratch_alt, kScratchTargetWidth,
                    kScratchTargetHeight, target_pitch, kScratchTargetDepth);

  g_buffers_ready = true;
  if (!g_buffers_logged) {
    LogBuffer(g_source_scratch);
    LogBuffer(g_target_scratch);
    LogBuffer(g_target_scratch_alt);
    LogBuffer(g_fragment_output.control_scratch);
    g_buffers_logged = true;
  }
  return true;
}

bool ProbeHardware() {
  u32 clock_rate = 0;
  if (GetV3dClockRate(&clock_rate)) {
    printf("boot: pi5v3d clock %u Hz\r\n", clock_rate);
  } else {
    printf("boot: pi5v3d clock query failed\r\n");
  }

  printf("boot: pi5v3d regs ");
  PrintAddress("hub", kV3dHubBase);
  PrintAddress("core0", kV3dCore0Base);
  PrintAddress("sms", kV3dSmsBase);
  printf("\r\n");

  const u32 sms_tee_cs = ReadReg(kV3dSmsBase, kV3dSmsTeeCs);
  const u32 sms_state = sms_tee_cs & kV3dSmsStateMask;
  printf("boot: pi5v3d sms tee_cs=0x%08x state=0x%x\r\n",
         sms_tee_cs, sms_state);
  if (sms_state == kV3dSmsPowerOffState) {
    printf("boot: pi5v3d SMS reports power-off; power/reset bring-up required\r\n");
    return false;
  }

  const u32 hub_ident0 = ReadReg(kV3dHubBase, kV3dHubIdent0);
  const u32 hub_ident1 = ReadReg(kV3dHubBase, kV3dHubIdent1);
  const u32 hub_ident2 = ReadReg(kV3dHubBase, kV3dHubIdent2);
  const u32 hub_ident3 = ReadReg(kV3dHubBase, kV3dHubIdent3);
  const u32 mmu_debug = ReadReg(kV3dHubBase, kV3dMmuDebugInfo);

  if (hub_ident1 == 0 || hub_ident1 == 0xFFFFFFFFU) {
    printf("boot: pi5v3d invalid hub ident1=0x%08x\r\n", hub_ident1);
    return false;
  }

  const unsigned hub_version =
      Field(hub_ident1, kV3dHubIdent1TverMask, kV3dHubIdent1TverShift) * 10 +
      Field(hub_ident1, kV3dHubIdent1RevMask, kV3dHubIdent1RevShift);
  const unsigned cores =
      Field(hub_ident1, kV3dHubIdent1NCoresMask, kV3dHubIdent1NCoresShift);
  const unsigned iprev =
      Field(hub_ident3, kV3dHubIdent3IprevMask, kV3dHubIdent3IprevShift);
  const bool has_mmu = (hub_ident2 & kV3dHubIdent2WithMmu) != 0;
  const unsigned pa_width =
      30 + Field(mmu_debug, kV3dMmuDebugInfoPaWidthMask,
                 kV3dMmuDebugInfoPaWidthShift);
  const unsigned va_width =
      30 + Field(mmu_debug, kV3dMmuDebugInfoVaWidthMask,
                 kV3dMmuDebugInfoVaWidthShift);
  const unsigned mmu_version =
      Field(mmu_debug, kV3dMmuDebugInfoVersionMask,
            kV3dMmuDebugInfoVersionShift);

  printf("boot: pi5v3d hub ident0=0x%08x ident1=0x%08x ident2=0x%08x "
         "ident3=0x%08x ver=%u cores=%u iprev=%u mmu=%u\r\n",
         hub_ident0, hub_ident1, hub_ident2, hub_ident3, hub_version, cores,
         iprev, has_mmu ? 1U : 0U);
  printf("boot: pi5v3d mmu debug=0x%08x pa_width=%u va_width=%u version=%u\r\n",
         mmu_debug, pa_width, va_width, mmu_version);

  const u32 core_ident0 = ReadReg(kV3dCore0Base, kV3dCoreIdent0);
  const u32 core_ident1 = ReadReg(kV3dCore0Base, kV3dCoreIdent1);
  const u32 core_ident2 = ReadReg(kV3dCore0Base, kV3dCoreIdent2);
  const u32 core_int_status = ReadReg(kV3dCore0Base, kV3dCoreIntStatus);
  const u32 csd_status = ReadReg(kV3dCore0Base, kV3dCsdStatus);
  const unsigned core_version =
      Field(core_ident0, kV3dCoreIdent0VerMask, kV3dCoreIdent0VerShift);

  printf("boot: pi5v3d core ident0=0x%08x ident1=0x%08x ident2=0x%08x "
         "ver=%u int=0x%08x csd=0x%08x\r\n",
         core_ident0, core_ident1, core_ident2, core_version,
         core_int_status, csd_status);

  if (hub_version != kExpectedV3dVersion) {
    printf("boot: pi5v3d unsupported hub version %u, expected %u\r\n",
           hub_version, kExpectedV3dVersion);
    return false;
  }

  return true;
}

bool LogShaderProgram(ShaderPreset preset, u32 v3d_version) {
  const ShaderProgram *program = GetShaderProgram(preset);
  if (program == nullptr) {
    printf("boot: pi5v3d shader program preset=%s status=off\r\n",
           ShaderPresetName(preset));
    return false;
  }

  const char *reason = nullptr;
  const bool valid = ValidateShaderProgram(*program, v3d_version, &reason);
  printf("boot: pi5v3d shader program preset=%s name=%s mode=%s "
         "input=%s output=%s qpu_words=%u uniforms=%u "
         "required_v3d=%u status=%s\r\n",
         ShaderPresetName(preset),
         program->name,
         ShaderExecutionModeName(program->execution_mode),
         program->input_format,
         program->output_format,
         program->qpu_code_words,
         program->uniform_bytes,
         program->required_v3d_version,
         valid ? "ok" : (reason != nullptr ? reason : "invalid"));
  return valid;
}

void LogFragmentShaderPackageStatus(
    const char *role,
    const shader_artifacts::PreparedFragmentShaderPackage &prepared,
    bool ready,
    const char *reason) {
  const v3dcrt::shaders::ShaderPackage *package =
      shader_artifacts::GetPreparedFragmentShaderPackage(&prepared);
  const shader_artifacts::ShaderArtifact *artifact =
      shader_artifacts::GetPreparedFragmentShaderArtifact(&prepared);
  printf("boot: pi5v3d shader package role=%s id=%s artifact=%s "
         "target=%s version=%u.%u hash=%s status=%s\r\n",
         SafeString(role),
         package != nullptr ? package->id : "(none)",
         artifact != nullptr ? artifact->name : "(none)",
         package != nullptr ? package->target_profile : "(none)",
         package != nullptr ? package->v3d_version : 0U,
         package != nullptr ? package->v3d_revision : 0U,
         package != nullptr ? package->content_sha256 : "(none)",
         ready ? "ok" : (reason != nullptr ? reason : "invalid"));
}

void CleanBufferRangeForV3D(const Buffer &buffer, u32 offset, u32 size) {
  if (buffer.cpu == nullptr || size == 0 ||
      offset > buffer.size || size > buffer.size - offset) {
    return;
  }

  CleanDataCacheRange(
      (u64)(uintptr)(buffer.cpu + offset), size);
}

}  // namespace

const char *ShaderPresetName(ShaderPreset preset) {
  switch (preset) {
    case kShaderSharp:
      return "sharp";
    case kShaderCrt:
      return "crt";
    case kShaderCrtSoft:
      return "crt_soft";
    case kShaderFrameCopy:
      return "frame_copy";
    case kShaderScanlines:
      return "scanlines";
    case kShaderFragmentProbe:
      return "fragment_probe";
    case kShaderOff:
    default:
      return "off";
  }
}

const char *BootTestModeName(BootTestMode mode) {
  switch (mode) {
    case kBootTestMmu:
      return "mmu";
    case kBootTestSolid:
      return "solid";
    case kBootTestSource:
      return "source";
    case kBootTestQpu:
      return "qpu";
    case kBootTestQpuFill:
      return "qpu_fill";
    case kBootTestFragmentArtifact:
      return "fragment_artifact";
    case kBootTestFragmentReplay:
      return "fragment_replay";
    case kBootTestFragmentScanout:
      return "fragment_scanout";
    case kBootTestFragmentFullscreen:
      return "fragment_fullscreen";
    case kBootTestFragmentSource:
      return "fragment_source";
    case kBootTestOff:
    default:
      return "off";
  }
}

void Configure(bool requested, bool kms_active, ShaderPreset preset,
               BootTestMode boot_test_mode,
               bool fragment_probe_wait_for_vblank,
               FragmentPackageMode fragment_package_mode,
               RenderResolution render_resolution) {
  g_requested = requested;
  g_kms_active = kms_active;
  g_shader_preset = preset;
  g_boot_test_mode = boot_test_mode;
  g_fragment_probe_wait_for_vblank = fragment_probe_wait_for_vblank;
  g_fragment_package_mode = fragment_package_mode;
  g_render_resolution = render_resolution;
  g_runtime_qpu_failed = false;
  g_runtime_qpu_failure_logged = false;
  g_runtime_fragment_failed = false;
  g_runtime_fragment_failure_logged = false;
  g_direct_scanout_failed = false;
  g_direct_scanout_logged = false;
  g_direct_scanout_failure_logged = false;
  g_qpu_warmup_done = false;
  g_qpu_warmup_attempted = false;
  g_qpu_geometry_unsupported_logged = false;
  g_output_resolution_fallback_logged = false;
  g_qpu_target_buffer_index = 0;
  g_qpu_target_select_log_count = 0;
  g_source_palette_log_valid = false;
  g_source_palette_log_generation = 0;
  g_source_palette_log_signature = 0;
  g_fragment_probe_done = false;
  g_fragment_package_ready = false;
  g_fragment_bloom_packages_ready = false;
  g_fragment_fast_cubic_package_ready = false;
  g_fragment_fast_cubic_selected = false;
  g_fragment_fast_cubic_runtime_failed = false;
  g_runtime_bloom_failed = false;
  g_runtime_bloom_failure_logged = false;
  g_fragment_bloom_path_log_valid = false;
  g_fragment_bloom_path_active = false;
  g_fragment_probe_attempts = 0;
  g_fragment_temporal_frame = 0;
  v3dcrt::ResetEdgeGlowTemporalFilter(&g_edge_glow_temporal_filter);
  g_fragment_frame_state_log_valid = false;
  g_fragment_frame_log_width = 0;
  g_fragment_frame_log_height = 0;
  g_fragment_frame_log_linear = false;
  g_fragment_frame_log_gap_bits = 0;
  g_fragment_frame_log_weight_bits = 0;
  g_fragment_frame_log_mask_signature = 0;
  g_fragment_frame_log_response_signature = 0;
  g_fragment_frame_log_effect_signature = 0;
  g_fragment_frame_log_bloom_enabled = false;
  g_fragment_frame_log_bloom_factor_bits = 0;
  g_fragment_source_effect_log_valid = false;
  g_fragment_source_filter_log_enabled = false;
  g_fragment_source_filter_log_sigma_bits = 0;
  g_fragment_source_convergence_log_signature = 0;
  g_fragment_source_composite_log_signature = 0;
  g_fragment_source_jitter_log_signature = 0;
  g_fragment_source_noise_log_signature = 0;
  g_fragment_source_sampler_log_linear = false;
  ResetFragmentReplayContexts();
  ResetFragmentPassPackages();
  ResetQpuScanlineParamLog();
  g_qpu_effect_probe_done = false;
  g_qpu_effect_probe_attempts = 0;
  memset(&g_render_fallback_stats, 0, sizeof g_render_fallback_stats);
  g_render_stats_scanout_mode_valid = false;
  g_render_stats_direct_scanout = false;
  ResetRenderStats();
  ClearCompletedRenderTarget();
}

bool Initialize() {
  if (g_initialized) {
    return g_available;
  }

  g_initialized = true;
  g_available = false;

  if (!g_requested) {
    return false;
  }

  if (!g_kms_active) {
    printf("boot: pi5v3d requested but pi5kms is inactive; using existing framebuffer path\r\n");
    g_unavailable_logged = true;
    return false;
  }

  g_hardware_visible = ProbeHardware();
  const bool mmu_ready = g_hardware_visible && InitializeMmu();
  const bool buffers_ready = mmu_ready && InitializeScratchBuffers();
  shader_artifacts::FragmentShaderPackageKind package_kind =
      shader_artifacts::kFragmentShaderPackageScanlineProbe;
  if (g_fragment_package_mode == kFragmentPackageCoreDiagnostic) {
    package_kind = shader_artifacts::kFragmentShaderPackageCrtCoreProbe;
  } else if (g_fragment_package_mode ==
             kFragmentPackageConvergenceDiagnostic) {
    package_kind = shader_artifacts::kFragmentShaderPackageCrtConvergenceProbe;
  } else if (g_fragment_package_mode ==
             kFragmentPackageEdgeBlurDiagnostic) {
    package_kind = shader_artifacts::kFragmentShaderPackageCrtEdgeBlurProbe;
  } else if (g_fragment_package_mode ==
             kFragmentPackageEdgeGlowDiagnostic) {
    package_kind = shader_artifacts::kFragmentShaderPackageCrtEdgeGlowProbe;
  } else if (g_fragment_package_mode ==
             kFragmentPackageSurfaceResponseDiagnostic) {
    package_kind =
        shader_artifacts::kFragmentShaderPackageCrtSurfaceResponseProbe;
  } else if (g_fragment_package_mode ==
             kFragmentPackageMaskVignetteDiagnostic) {
    package_kind =
        shader_artifacts::kFragmentShaderPackageCrtMaskVignetteProbe;
  } else if (g_fragment_package_mode ==
             kFragmentPackageIlluminationJitterDiagnostic) {
    package_kind =
        shader_artifacts::kFragmentShaderPackageCrtIlluminationJitterProbe;
  } else if (g_fragment_package_mode == kFragmentPackageCompositeDiagnostic) {
    package_kind = shader_artifacts::kFragmentShaderPackageCrtCompositeProbe;
  } else if (g_fragment_package_mode == kFragmentPackageNoiseDiagnostic) {
    package_kind = shader_artifacts::kFragmentShaderPackageCrtNoiseProbe;
  } else if (g_fragment_package_mode == kFragmentPackageDefault &&
             g_boot_test_mode == kBootTestOff &&
             (g_shader_preset == kShaderCrt ||
              (g_shader_preset == kShaderSharp &&
               SelectedOutputResolutionPath() !=
                   kOutputResolutionPathPassthrough))) {
    package_kind = shader_artifacts::kFragmentShaderPackageCrtSinglePass;
  }
  const bool split_packages =
      SelectedOutputResolutionPath() == kOutputResolutionPathSplitGeometry;
  shader_artifacts::FragmentShaderPackageKind output_package_kind =
      package_kind;
  if (split_packages) {
    output_package_kind =
        shader_artifacts::
            kFragmentShaderPackageCrtOutputResponse;
  }

  ResetFragmentPassPackages();
  const shader_artifacts::FragmentShaderPackageKind
      pass_package_kinds[kFragmentPassCount] = {
    output_package_kind,
    shader_artifacts::kFragmentShaderPackageCrtSourceNoise,
    shader_artifacts::kFragmentShaderPackageCrtOutputResponse,
    shader_artifacts::kFragmentShaderPackageCrtBloomBlurHorizontal,
    shader_artifacts::kFragmentShaderPackageCrtBloomBlurVertical,
    shader_artifacts::kFragmentShaderPackageCrtBloomComposite,
  };
  bool pass_package_ready[kFragmentPassCount] = {};
  const char *pass_package_reasons[kFragmentPassCount] = {};
  for (u32 pass = 0; pass < kFragmentPassCount; ++pass) {
    if (pass == kFragmentPassOutput || split_packages) {
      pass_package_ready[pass] =
          shader_artifacts::PrepareFragmentShaderPackage(
              &g_fragment_pass_resources[pass].package,
              pass_package_kinds[pass], &pass_package_reasons[pass]);
    } else {
      pass_package_ready[pass] = pass == kFragmentPassSource;
    }
  }
  const char *fast_cubic_package_reason = nullptr;
  g_fragment_fast_cubic_package_ready = split_packages &&
      shader_artifacts::PrepareFragmentShaderPackage(
          &g_fragment_output_fast_cubic_package,
          shader_artifacts::
              kFragmentShaderPackageCrtOutputResponseFastCubic,
          &fast_cubic_package_reason);
  g_fragment_bloom_packages_ready = split_packages;
  for (u32 pass = kFragmentPassBloomBase;
       pass <= kFragmentPassBloomComposite; ++pass) {
    g_fragment_bloom_packages_ready &= pass_package_ready[pass];
  }
  g_fragment_package_ready =
      pass_package_ready[kFragmentPassOutput] &&
      pass_package_ready[kFragmentPassSource];
  const char *package_reason =
      !pass_package_ready[kFragmentPassOutput] ?
          pass_package_reasons[kFragmentPassOutput] :
      !pass_package_ready[kFragmentPassSource] ?
          pass_package_reasons[kFragmentPassSource] : "ok";
  LogFragmentShaderPackageStatus(
      kFragmentPassNames[kFragmentPassOutput], g_fragment_output.package,
      pass_package_ready[kFragmentPassOutput],
      pass_package_reasons[kFragmentPassOutput]);
  if (split_packages) {
    LogFragmentShaderPackageStatus(
        "output-fast-cubic", g_fragment_output_fast_cubic_package,
        g_fragment_fast_cubic_package_ready, fast_cubic_package_reason);
    for (u32 pass = kFragmentPassSource;
         pass < kFragmentPassCount; ++pass) {
      LogFragmentShaderPackageStatus(
          kFragmentPassNames[pass],
          g_fragment_pass_resources[pass].package,
          pass_package_ready[pass], pass_package_reasons[pass]);
    }
    printf("boot: pi5v3d bloom package set status=%s "
           "fallback=two-pass\r\n",
           g_fragment_bloom_packages_ready ? "ok" : "unavailable");
  }
  const bool shader_ready = g_hardware_visible &&
                            LogShaderProgram(g_shader_preset,
                                             kExpectedV3dVersion);
  const ShaderProgram *program = GetShaderProgram(g_shader_preset);
  bool fragment_ready = true;
  const char *fragment_status = "n/a";
  const bool fragment_program = program != nullptr &&
      (program->execution_mode == kShaderExecutionQpuFragment ||
       program->execution_mode == kShaderExecutionQpuFragmentFrame);
  if (buffers_ready && shader_ready && fragment_program) {
    if (!g_fragment_package_ready) {
      fragment_ready = false;
      fragment_status = package_reason != nullptr ? package_reason :
                                                   "package-invalid";
    } else if (SelectedOutputResolutionPath() !=
                   kOutputResolutionPathDisabled &&
               program->execution_mode == kShaderExecutionQpuFragmentFrame) {
      fragment_status = "deferred-output";
    } else if (g_boot_test_mode == kBootTestOff) {
      const FragmentReplayGeometry geometry = {
        kFragmentFullscreenWidth,
        kFragmentFullscreenHeight,
        program->execution_mode == kShaderExecutionQpuFragmentFrame ?
            "fragment_frame" : "fragment_probe"
      };
      fragment_ready = PrepareFragmentReplayRuntimeState(geometry);
      fragment_status = fragment_ready ? "prepared" : "unavailable";
    } else {
      fragment_status = "boot-test-skip";
    }
  }
  g_available = buffers_ready && shader_ready &&
                fragment_ready &&
                program != nullptr && IsRuntimeFrameProgram(*program);
  const char *renderer = "not implemented yet; falling back";
  if (g_available && program != nullptr) {
    renderer = ShaderExecutionModeName(program->execution_mode);
  }
  printf("boot: pi5v3d requested shader=%s test=%s resolution=%s "
         "output_scope=%s, "
         "probe=%s, mmu=%s, "
         "buffers=%s, shader=%s, fragment=%s, renderer=%s\r\n",
         ShaderPresetName(g_shader_preset),
         BootTestModeName(g_boot_test_mode),
         RenderResolutionName(g_render_resolution),
         OutputResolutionPathName(SelectedOutputResolutionPath()),
         g_hardware_visible ? "ok" : "unavailable",
         mmu_ready ? "ready" : "unavailable",
         buffers_ready ? "ready" : "unavailable",
         shader_ready ? "ready" : "unavailable",
         fragment_status,
         renderer);
  g_unavailable_logged = true;
  return g_available;
}

bool Requested() {
  return g_requested;
}

bool IsAvailable() {
  return g_available;
}

bool AllocateBuffer(BufferUsage usage, u32 size, u32 alignment, Buffer *buffer) {
  if (buffer == nullptr || size == 0 || alignment == 0 ||
      !IsPowerOfTwo(alignment)) {
    return false;
  }

  memset(buffer, 0, sizeof *buffer);
  uint8_t *allocation = nullptr;
  uint8_t *cpu = nullptr;
  u32 alloc_size = 0;
  if (!AllocateAlignedDma(size, alignment, &allocation, &cpu, &alloc_size)) {
    return false;
  }

  buffer->allocation = allocation;
  buffer->cpu = cpu;
  buffer->axi_bus_address = CpuToAxiBusAddress(cpu);
  buffer->v3d_address = 0;
  buffer->hvs_bus_address = (u32)(uintptr)cpu;
  buffer->legacy_gpu_bus_address = BUS_ADDRESS((uintptr)cpu);
  buffer->allocation_size = alloc_size;
  buffer->size = size;
  buffer->alignment = alignment;
  buffer->usage = usage;

  if (g_mmu_ready && !MapBufferForV3D(buffer)) {
    delete[] buffer->allocation;
    memset(buffer, 0, sizeof *buffer);
    return false;
  }

  CleanBufferForV3D(*buffer);
  return true;
}

void FreeBuffer(Buffer *buffer) {
  if (buffer == nullptr) {
    return;
  }

  UnmapBufferFromV3D(buffer);
  delete[] buffer->allocation;
  memset(buffer, 0, sizeof *buffer);
}

void CleanBufferForV3D(const Buffer &buffer) {
  CleanBufferRangeForV3D(buffer, 0, buffer.size);
}

void InvalidateBufferFromV3D(const Buffer &buffer) {
  if (buffer.cpu == nullptr || buffer.size == 0) {
    return;
  }

  InvalidateDataCacheRange((u64)(uintptr)buffer.cpu, buffer.size);
}

bool RenderFullscreen(const TextureSource &source,
                      const OutputTarget &target,
                      const RenderParams &params) {
  ClearCompletedRenderTarget();
  if (target.presented != nullptr) {
    *target.presented = false;
  }

  if (g_requested && g_available) {
    SelectRenderStatsScanoutMode(target.allow_direct_scanout);
    const bool runtime_qpu = SelectedProgramUsesRuntimeQpu();
    const bool runtime_fragment = SelectedProgramUsesRuntimeFragment();
    const bool continuous_fragment =
        SelectedProgramUsesContinuousRuntimeFragment();
    if (runtime_qpu && g_runtime_qpu_failed) {
      RecordRenderFallback(kRenderFallbackRuntimeQpuDisabled);
      return false;
    }
    if (runtime_fragment && g_runtime_fragment_failed) {
      RecordRenderFallback(kRenderFallbackRuntimeFragmentDisabled);
      return false;
    }
    if (runtime_fragment && !continuous_fragment && g_fragment_probe_done) {
      RecordRenderFallback(kRenderFallbackFragmentProbeDone);
      return false;
    }

    RenderParams effective_params = ApplyPresetDefaults(params);
    effective_params.temporal_frame =
        (float)(g_fragment_temporal_frame & 0x3FFU);
    const RenderParams source_effect_scope =
        ResolveFragmentEffectScope(effective_params);
    const v3dcrt::OutputResponseParams output_response =
        v3dcrt::ResolveOutputResponseParams(
            source_effect_scope.enable_output_response,
            source_effect_scope.fast_output_response,
            source_effect_scope.output_level_mapping,
            source_effect_scope.input_gamma,
            source_effect_scope.output_gamma,
            source_effect_scope.output_saturation,
            source_effect_scope.black_level,
            source_effect_scope.white_clip);
    u32 staged_width = 0;
    u32 staged_height = 0;
    const u64 start_us = CTimer::GetClockTicks64();
    const bool staged = continuous_fragment ?
        StageFragmentTextureSource(
            source, output_response, &staged_width, &staged_height) :
        StageTextureSource(
            source, output_response, &staged_width, &staged_height);
    if (staged) {
      LogSourceOutputResponseIfChanged(source, output_response);
      // Output Response is a source-image transfer function. The complete
      // emulator viewport, including its border, has already been transformed
      // above; keep all later CRT passes and BMX overlays outside that curve.
      effective_params.enable_output_response = false;
      const u64 staged_us = CTimer::GetClockTicks64();
      if (runtime_qpu) {
        const QpuFrameProgram qpu_program =
            SelectedQpuFrameProgram(effective_params);
        const char *qpu_geometry_reason = nullptr;
        if (!RuntimeQpuFrameGeometrySupported(qpu_program,
                                              staged_width, staged_height,
                                              &qpu_geometry_reason)) {
          LogRuntimeQpuGeometryUnsupportedIfNeeded(qpu_program,
                                                   staged_width, staged_height,
                                                   qpu_geometry_reason);
          RecordRenderFallback(kRenderFallbackQpuGeometry);
          return false;
        }
        u64 render_start_us = start_us;
        u64 render_staged_us = staged_us;
        if (!g_qpu_warmup_attempted) {
          u64 warmup_done_us = staged_us;
          if (WarmRuntimeQpuFrame(qpu_program,
                                  staged_width, staged_height,
                                  effective_params,
                                  &warmup_done_us)) {
            const u64 stage_us = staged_us - start_us;
            render_staged_us = warmup_done_us;
            render_start_us = warmup_done_us - stage_us;
          }
        }
        if (RenderQpuFrameToScanout(source, target,
                                    qpu_program,
                                    staged_width, staged_height,
                                    render_start_us, render_staged_us,
                                    effective_params)) {
          return true;
        }
        MarkRuntimeQpuFailed();
        return false;
      }
      if (runtime_fragment) {
        if (continuous_fragment) {
          if (RenderFragmentFrameToScanout(
                  source, target, staged_width, staged_height,
                  start_us, staged_us, effective_params)) {
            RecordCompletedRenderTarget(target);
            ++g_fragment_temporal_frame;
            return true;
          }
          MarkRuntimeFragmentFailed();
          return false;
        }
        const FragmentProbeFrameResult fragment_result =
            RenderFragmentProbeFrameToScanout(target,
                                             staged_width, staged_height,
                                             start_us, staged_us);
        switch (fragment_result) {
          case kFragmentProbeFramePresented:
            return true;
          case kFragmentProbeFrameWaiting:
            RecordRenderFallback(kRenderFallbackFragmentProbePending);
            return false;
          case kFragmentProbeFrameDone:
            RecordRenderFallback(kRenderFallbackFragmentProbeDone);
            return false;
          case kFragmentProbeFrameFailed:
          default:
            RecordRenderFallback(kRenderFallbackFragmentProbeSubmit);
            return false;
        }
      }
      if (BlitStagedSourceToScanout(target, staged_width, staged_height,
                                    effective_params)) {
        const u64 done_us = CTimer::GetClockTicks64();
        LogFirstFrameStage(source, target, staged_width, staged_height,
                           effective_params,
                           (u32)(staged_us - start_us),
                           (u32)(done_us - staged_us));
        RecordRenderStats("source-stage", "blit",
                          start_us, done_us,
                          (u32)(staged_us - start_us),
                          (u32)(done_us - staged_us),
                          0);
        return true;
      }
      RecordRenderFallback(kRenderFallbackSourceStageBlit);
    } else {
      RecordRenderFallback(kRenderFallbackStageSource);
    }
  }

  if (g_requested && !g_available && !g_unavailable_logged) {
    printf("boot: pi5v3d unavailable; using existing present path\r\n");
    g_unavailable_logged = true;
  }
  return false;
}

bool ReadCompletedRenderTarget(RenderReadback *readback) {
  if (readback == nullptr || g_last_completed_target == nullptr) {
    return false;
  }

  Buffer &buffer = *g_last_completed_target;
  if (buffer.cpu == nullptr || buffer.depth != 16 ||
      g_last_completed_width == 0 || g_last_completed_height == 0 ||
      g_last_completed_width > buffer.width ||
      g_last_completed_height > buffer.height ||
      buffer.pitch < g_last_completed_width * 2U) {
    return false;
  }

  InvalidateBufferFromV3D(buffer);
  DataSyncBarrier();
  *readback = {
    buffer.cpu,
    g_last_completed_width,
    g_last_completed_height,
    buffer.pitch,
    buffer.depth,
    buffer.size,
    QpuTargetBufferIndex(buffer)
  };
  return true;
}

bool GetDiagnosticStatus(DiagnosticStatus *status) {
  if (status == nullptr || !g_hardware_visible) {
    return false;
  }

  const u32 hub_interrupt_status = ReadReg(kV3dHubBase, kV3dHubIntStatus);
  const u32 mmu_faults =
      hub_interrupt_status & (kV3dHubIntMmuWriteViolation |
                              kV3dHubIntMmuPtInvalid |
                              kV3dHubIntMmuCapExceeded);
  *status = {
    hub_interrupt_status,
    ReadReg(kV3dCore0Base, kV3dCoreIntStatus),
    mmu_faults,
    ReadReg(kV3dHubBase, kV3dMmuVioId),
    ReadReg(kV3dHubBase, kV3dMmuVioAddr),
    g_runtime_qpu_failed || g_runtime_fragment_failed ||
        g_runtime_bloom_failed
  };
  return true;
}

void ResetDiagnosticFrameState() {
  g_fragment_temporal_frame = 0;
  v3dcrt::ResetEdgeGlowTemporalFilter(&g_edge_glow_temporal_filter);
  ClearCompletedRenderTarget();
}

bool RunBootTest(const OutputTarget &target) {
  if (!g_requested || g_boot_test_mode == kBootTestOff) {
    return false;
  }

  if (target.scanout == nullptr) {
    printf("boot: pi5v3d test=%s unavailable; no scanout target\r\n",
           BootTestModeName(g_boot_test_mode));
    return false;
  }

  if (!g_initialized) {
    Initialize();
  }

  printf("boot: pi5v3d test=%s target=%ux%u display=%ux%u "
         "scanout=0x%08x probe=%s mmu=%s buffers=%s\r\n",
         BootTestModeName(g_boot_test_mode),
         target.scanout->width, target.scanout->height,
         target.display_width, target.display_height,
         (u32)(uintptr)target.scanout->pixels,
         g_hardware_visible ? "ok" : "unavailable",
         g_mmu_ready ? "ready" : "unavailable",
         g_buffers_ready ? "ready" : "unavailable");

  if (!g_hardware_visible || !g_mmu_ready || !g_buffers_ready) {
    printf("boot: pi5v3d test=%s blocked before submit; "
           "hardware/MMU/buffers not ready\r\n",
           BootTestModeName(g_boot_test_mode));
    return false;
  }

  InvalidateV3dCaches();

  const u32 hub_int = ReadReg(kV3dHubBase, kV3dHubIntStatus);
  const u32 core_int = ReadReg(kV3dCore0Base, kV3dCoreIntStatus);
  const u32 mmu_ctl = ReadReg(kV3dHubBase, kV3dMmuCtl);
  const u32 vio_id = ReadReg(kV3dHubBase, kV3dMmuVioId);
  const u32 vio_addr = ReadReg(kV3dHubBase, kV3dMmuVioAddr);
  const u32 mmu_faults = hub_int & (kV3dHubIntMmuWriteViolation |
                                    kV3dHubIntMmuPtInvalid |
                                    kV3dHubIntMmuCapExceeded);
  printf("boot: pi5v3d test=%s status hub_int=0x%08x core_int=0x%08x "
         "mmu_faults=0x%08x mmu_ctl=0x%08x vio_id=0x%08x "
         "vio_addr=0x%08x\r\n",
         BootTestModeName(g_boot_test_mode), hub_int, core_int, mmu_faults,
         mmu_ctl, vio_id, vio_addr);

  if (g_boot_test_mode == kBootTestMmu) {
    printf("boot: pi5v3d test=mmu complete; V3D VA setup is ready for first submit\r\n");
    return false;
  }

  if (g_boot_test_mode == kBootTestSource) {
    return RunSourceBootTest(target);
  }

  if (g_boot_test_mode == kBootTestQpu) {
    return RunQpuBootTest(target);
  }

  if (g_boot_test_mode == kBootTestQpuFill) {
    return RunQpuFillBootTest(target);
  }

  if (g_boot_test_mode == kBootTestFragmentArtifact) {
    if (!g_fragment_package_ready) {
      printf("boot: pi5v3d test=fragment_artifact blocked; "
             "shader package invalid\r\n");
      return false;
    }
    return RunFragmentArtifactBootTest();
  }

  if (g_boot_test_mode == kBootTestFragmentReplay ||
      g_boot_test_mode == kBootTestFragmentScanout ||
      g_boot_test_mode == kBootTestFragmentFullscreen ||
      g_boot_test_mode == kBootTestFragmentSource) {
    if (!g_fragment_package_ready) {
      printf("boot: pi5v3d test=%s blocked; shader package invalid\r\n",
             BootTestModeName(g_boot_test_mode));
      return false;
    }
    const bool fullscreen =
        g_boot_test_mode == kBootTestFragmentFullscreen ||
        g_boot_test_mode == kBootTestFragmentSource;
    const FragmentReplayGeometry geometry = {
      fullscreen ? kFragmentFullscreenWidth : kFragmentReplayWidth,
      fullscreen ? kFragmentFullscreenHeight : kFragmentReplayHeight,
      BootTestModeName(g_boot_test_mode)
    };
    return RunFragmentReplayBootTest(
        target, geometry,
        g_boot_test_mode == kBootTestFragmentScanout || fullscreen,
        g_boot_test_mode == kBootTestFragmentSource);
  }

  return RunSolidBootTest(target);
}

void Shutdown() {
  ResetFragmentSourceStagingCache();
  FreeBuffer(&g_source_scratch);
  FreeBuffer(&g_fragment_source_scratch);
  FreeBuffer(&g_target_scratch);
  FreeBuffer(&g_target_scratch_alt);
  FreeBuffer(&g_fragment_intermediate_scratch);
  FreeBuffer(&g_fragment_bloom_base_scratch);
  FreeBuffer(&g_fragment_bloom_horizontal_scratch);
  FreeBuffer(&g_fragment_bloom_vertical_scratch);
  for (u32 pass = 0; pass < kFragmentPassCount; ++pass) {
    FreeBuffer(&g_fragment_pass_resources[pass].control_scratch);
    FreeBuffer(&g_fragment_pass_resources[pass].tile_scratch);
  }
  FreeMmu();
  g_initialized = false;
  g_available = false;
  g_hardware_visible = false;
  g_unavailable_logged = false;
  g_buffers_ready = false;
  g_buffers_logged = false;
  g_frame_path_logged = false;
  g_frame_unsupported_logged = false;
  g_runtime_qpu_failed = false;
  g_runtime_qpu_failure_logged = false;
  g_runtime_fragment_failed = false;
  g_runtime_fragment_failure_logged = false;
  g_direct_scanout_failed = false;
  g_direct_scanout_logged = false;
  g_direct_scanout_failure_logged = false;
  g_qpu_warmup_done = false;
  g_qpu_warmup_attempted = false;
  g_qpu_geometry_unsupported_logged = false;
  g_output_resolution_fallback_logged = false;
  g_qpu_target_buffer_index = 0;
  g_qpu_target_select_log_count = 0;
  g_source_palette_log_valid = false;
  g_source_palette_log_generation = 0;
  g_source_palette_log_signature = 0;
  g_fragment_probe_done = false;
  g_fragment_package_ready = false;
  g_fragment_bloom_packages_ready = false;
  g_fragment_fast_cubic_package_ready = false;
  g_fragment_fast_cubic_selected = false;
  g_fragment_fast_cubic_runtime_failed = false;
  g_runtime_bloom_failed = false;
  g_runtime_bloom_failure_logged = false;
  g_fragment_bloom_path_log_valid = false;
  g_fragment_bloom_path_active = false;
  g_fragment_probe_attempts = 0;
  g_fragment_temporal_frame = 0;
  v3dcrt::ResetEdgeGlowTemporalFilter(&g_edge_glow_temporal_filter);
  g_fragment_frame_state_log_valid = false;
  g_fragment_frame_log_width = 0;
  g_fragment_frame_log_height = 0;
  g_fragment_frame_log_linear = false;
  g_fragment_frame_log_gap_bits = 0;
  g_fragment_frame_log_weight_bits = 0;
  g_fragment_frame_log_mask_signature = 0;
  g_fragment_frame_log_response_signature = 0;
  g_fragment_frame_log_effect_signature = 0;
  g_fragment_frame_log_bloom_enabled = false;
  g_fragment_frame_log_bloom_factor_bits = 0;
  g_fragment_source_effect_log_valid = false;
  g_fragment_source_filter_log_enabled = false;
  g_fragment_source_filter_log_sigma_bits = 0;
  g_fragment_source_convergence_log_signature = 0;
  g_fragment_source_composite_log_signature = 0;
  g_fragment_source_jitter_log_signature = 0;
  g_fragment_source_noise_log_signature = 0;
  g_fragment_source_sampler_log_linear = false;
  ResetFragmentReplayContexts();
  ResetFragmentPassPackages();
  ResetQpuScanlineParamLog();
  g_qpu_effect_probe_done = false;
  g_qpu_effect_probe_attempts = 0;
  for (u32 pass = 0; pass < kFragmentPassCount; ++pass) {
    memset(&g_fragment_pass_resources[pass].tile_layout, 0,
           sizeof g_fragment_pass_resources[pass].tile_layout);
  }
  memset(&g_fragment_intermediate_layout, 0,
         sizeof g_fragment_intermediate_layout);
  memset(&g_fragment_bloom_base_layout, 0,
         sizeof g_fragment_bloom_base_layout);
  memset(&g_fragment_bloom_horizontal_layout, 0,
         sizeof g_fragment_bloom_horizontal_layout);
  memset(&g_fragment_bloom_vertical_layout, 0,
         sizeof g_fragment_bloom_vertical_layout);
  memset(&g_render_fallback_stats, 0, sizeof g_render_fallback_stats);
  g_render_stats_scanout_mode_valid = false;
  g_render_stats_direct_scanout = false;
  ResetRenderStats();
  ClearCompletedRenderTarget();
  g_boot_test_mode = kBootTestOff;
  g_render_resolution = kRenderResolutionSource;
}

}  // namespace pi5v3d
