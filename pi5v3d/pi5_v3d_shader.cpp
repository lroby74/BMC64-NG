#include "pi5v3d/pi5_v3d_shader.h"

namespace pi5v3d {

namespace {

constexpr u32 kV3d71 = 71;

const ShaderProgram kSharpFragmentProgram = {
  kShaderSharp,
  "sharp-crt-single-pass-frame",
  kShaderExecutionQpuFragmentFrame,
  nullptr,
  0,
  kV3d71,
  "rgba8-live-frame",
  "rgb565-target",
  18 * sizeof(u32),
  "continuous V3D fragment pass using the generated CRT single-pass package with effects disabled"
};

const ShaderProgram kCrtProgram = {
  kShaderCrt,
  "crt-single-pass-fragment-frame",
  kShaderExecutionQpuFragmentFrame,
  nullptr,
  0,
  kV3d71,
  "rgba8-live-frame",
  "rgb565-target",
  18 * sizeof(u32),
  "continuous V3D fragment scanline and output-response effect using the generated CRT single-pass package"
};

const ShaderProgram kCrtSoftProgram = {
  kShaderCrtSoft,
  "crt-soft-source-stage-preview",
  kShaderExecutionSourceStageEffect,
  nullptr,
  0,
  kV3d71,
  "rgb565-linear",
  "rgb565-scanout",
  0,
  "diagnostic source-stage effect preview; no QPU blob required"
};

const ShaderProgram kFrameCopyProgram = {
  kShaderFrameCopy,
  "frame-copy-qpu-diagnostic",
  kShaderExecutionQpuFrameCopy,
  nullptr,
  0,
  kV3d71,
  "rgb565-linear",
  "rgb565-target",
  6 * sizeof(u32),
  "diagnostic runtime frame-copy path; QPU blob is embedded in the Pi5 backend"
};

const ShaderProgram kScanlinesProgram = {
  kShaderScanlines,
  "scanlines-qpu-runtime",
  kShaderExecutionQpuFrameEffect,
  nullptr,
  0,
  kV3d71,
  "rgb565-linear",
  "rgb565-target",
  10 * sizeof(u32),
  "runtime QPU scanline effect with quantized gap brightness; QPU blob is embedded in the Pi5 backend"
};

const ShaderProgram kFragmentProbeProgram = {
  kShaderFragmentProbe,
  "fragment-probe-runtime",
  kShaderExecutionQpuFragment,
  nullptr,
  0,
  kV3d71,
  "rgba8-4x4-live-sample",
  "rgb565-target",
  0,
  "diagnostic runtime fragment replay path; Mesa-derived artifact is embedded separately"
};

}  // namespace

const ShaderProgram *GetShaderProgram(ShaderPreset preset) {
  switch (preset) {
    case kShaderSharp:
      return &kSharpFragmentProgram;
    case kShaderCrt:
      return &kCrtProgram;
    case kShaderCrtSoft:
      return &kCrtSoftProgram;
    case kShaderFrameCopy:
      return &kFrameCopyProgram;
    case kShaderScanlines:
      return &kScanlinesProgram;
    case kShaderFragmentProbe:
      return &kFragmentProbeProgram;
    case kShaderOff:
    default:
      return nullptr;
  }
}

const char *ShaderExecutionModeName(ShaderExecutionMode mode) {
  switch (mode) {
    case kShaderExecutionSourceStage:
      return "source-stage";
    case kShaderExecutionSourceStageEffect:
      return "source-stage-effect";
    case kShaderExecutionQpuFrameCopy:
      return "qpu-frame-copy";
    case kShaderExecutionQpuFrameEffect:
      return "qpu-frame-effect";
    case kShaderExecutionQpuFragment:
      return "qpu-fragment";
    case kShaderExecutionQpuFragmentFrame:
      return "qpu-fragment-frame";
    default:
      return "unknown";
  }
}

bool ValidateShaderProgram(const ShaderProgram &program,
                           u32 v3d_version,
                           const char **reason) {
  if (program.required_v3d_version != v3d_version) {
    if (reason != nullptr) {
      *reason = "v3d-version-mismatch";
    }
    return false;
  }

  if (program.execution_mode == kShaderExecutionSourceStage ||
      program.execution_mode == kShaderExecutionSourceStageEffect ||
      program.execution_mode == kShaderExecutionQpuFrameCopy ||
      program.execution_mode == kShaderExecutionQpuFrameEffect) {
    if (reason != nullptr) {
      *reason = "ok";
    }
    return true;
  }

  if (program.preset == kShaderFragmentProbe &&
      program.execution_mode == kShaderExecutionQpuFragment) {
    if (reason != nullptr) {
      *reason = "ok";
    }
    return true;
  }

  if ((program.preset == kShaderSharp || program.preset == kShaderCrt) &&
      program.execution_mode == kShaderExecutionQpuFragmentFrame) {
    if (reason != nullptr) {
      *reason = "ok";
    }
    return true;
  }

  if (program.qpu_code == nullptr || program.qpu_code_words == 0) {
    if (reason != nullptr) {
      *reason = "missing-qpu-blob";
    }
    return false;
  }

  if (reason != nullptr) {
    *reason = "ok";
  }
  return true;
}

}  // namespace pi5v3d
