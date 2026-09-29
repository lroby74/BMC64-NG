#ifndef V3DCRT_SHADERS_SHADER_PACKAGE_H
#define V3DCRT_SHADERS_SHADER_PACKAGE_H

#include <stdint.h>

namespace v3dcrt {
namespace shaders {

enum ShaderStageKind {
  kShaderStageCoordinate = 0,
  kShaderStageVertex,
  kShaderStageFragment
};

struct ShaderUniformSpec {
  const char *kind;
  uint32_t data;
  const char *semantic;
};

struct ShaderStageRequirements {
  uint32_t threads;
  bool single_segment;
  uint32_t spill_size;
  uint32_t tmu_count;
  uint32_t vpm_input_size;
  uint32_t vpm_output_size;
  uint32_t varying_count;
};

struct ShaderStageProgram {
  ShaderStageKind stage;
  const uint64_t *qpu_words;
  uint32_t qpu_word_count;
  const ShaderUniformSpec *uniforms;
  uint32_t uniform_count;
  ShaderStageRequirements requirements;
};

struct ShaderPackage {
  const char *schema;
  const char *id;
  const char *target_profile;
  uint32_t v3d_version;
  uint32_t v3d_revision;
  const char *content_sha256;
  const char *provenance;
  const ShaderStageProgram *stages;
  uint32_t stage_count;
};

struct ShaderBackendCapabilities {
  const char *target_profile;
  uint32_t v3d_version;
  uint32_t v3d_revision;
  uint32_t max_threads;
  uint32_t max_tmu_count;
  uint32_t max_vpm_io_size;
  bool supports_spilling;
};

const ShaderStageProgram *FindShaderStage(const ShaderPackage &package,
                                          ShaderStageKind stage);

bool ValidateShaderPackage(const ShaderPackage &package,
                           const ShaderBackendCapabilities &capabilities,
                           const char **reason);

}  // namespace shaders
}  // namespace v3dcrt

#endif  // V3DCRT_SHADERS_SHADER_PACKAGE_H
