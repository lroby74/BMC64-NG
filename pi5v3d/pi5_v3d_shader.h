#ifndef PI5V3D_PI5_V3D_SHADER_H
#define PI5V3D_PI5_V3D_SHADER_H

#include "pi5v3d/pi5_v3d.h"

#include <circle/types.h>

namespace pi5v3d {

enum ShaderExecutionMode {
  kShaderExecutionSourceStage = 0,
  kShaderExecutionSourceStageEffect,
  kShaderExecutionQpuFrameCopy,
  kShaderExecutionQpuFrameEffect,
  kShaderExecutionQpuFragment,
  kShaderExecutionQpuFragmentFrame
};

struct ShaderProgram {
  ShaderPreset preset;
  const char *name;
  ShaderExecutionMode execution_mode;
  const u32 *qpu_code;
  u32 qpu_code_words;
  u32 required_v3d_version;
  const char *input_format;
  const char *output_format;
  u32 uniform_bytes;
  const char *provenance;
};

const ShaderProgram *GetShaderProgram(ShaderPreset preset);
const char *ShaderExecutionModeName(ShaderExecutionMode mode);
bool ValidateShaderProgram(const ShaderProgram &program,
                           u32 v3d_version,
                           const char **reason);

}  // namespace pi5v3d

#endif  // PI5V3D_PI5_V3D_SHADER_H
