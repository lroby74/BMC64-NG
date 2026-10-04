#ifndef PI5V3D_SHADERS_SHADER_ARTIFACT_H
#define PI5V3D_SHADERS_SHADER_ARTIFACT_H

#include <stdint.h>

namespace pi5v3d {
namespace shader_artifacts {

constexpr uint32_t kInvalidArtifactIndex = 0xffffffffU;

enum ShaderArtifactStage {
  kArtifactStageNone = 0,
  kArtifactStageFragment,
  kArtifactStageVertex,
  kArtifactStageCoordinate
};

enum ShaderArtifactAddressEncoding {
  kArtifactAddressDirect = 0,
  kArtifactAddressWordShiftedRight4
};

enum ShaderArtifactPatchKind {
  kArtifactPatchGlShaderStateRecord = 0,
  kArtifactPatchShaderCode,
  kArtifactPatchShaderUniform,
  kArtifactPatchUniformWordAddressCandidate,
  kArtifactPatchSamplerWordAddressCandidate,
  kArtifactPatchShaderAttribute
};

struct ShaderArtifactStageCode {
  ShaderArtifactStage stage;
  const char *mesa_stage;
  const char *program;
  const char *buffer_name;
  uint32_t code_offset;
  const uint64_t *qpu_words;
  uint32_t qpu_word_count;
};

struct ShaderArtifactUniformBlock {
  ShaderArtifactStage stage;
  const char *buffer_name;
  uint32_t offset;
  const uint32_t *words;
  uint32_t word_count;
};

struct ShaderArtifactSamplerBlock {
  const char *name;
  uint32_t offset;
  const uint32_t *words;
  uint32_t word_count;
  bool referenced_by_uniforms;
};

struct ShaderArtifactDataBlock {
  const char *buffer_name;
  const char *buffer_kind;
  uint32_t offset;
  const uint32_t *words;
  uint32_t word_count;
};

struct ShaderArtifactAddressWordPatch {
  const char *write_buffer_name;
  const char *write_buffer_kind;
  uint32_t write_offset;
  const char *target_buffer_name;
  const char *target_buffer_kind;
  uint32_t target_offset;
  ShaderArtifactAddressEncoding encoding;
};

struct ShaderArtifactPatchPoint {
  ShaderArtifactPatchKind kind;
  ShaderArtifactStage stage;
  uint32_t index;
  uint32_t word_index;
  const char *buffer_name;
  const char *buffer_kind;
  uint32_t offset;
  ShaderArtifactAddressEncoding encoding;
};

struct ShaderArtifact {
  const char *name;
  const char *schema;
  const char *provenance;
  uint32_t shader_record_index;
  uint32_t fragment_varyings;
  const ShaderArtifactStageCode *stages;
  uint32_t stage_count;
  const ShaderArtifactUniformBlock *uniforms;
  uint32_t uniform_count;
  const ShaderArtifactSamplerBlock *samplers;
  uint32_t sampler_count;
  const ShaderArtifactDataBlock *data_blocks;
  uint32_t data_block_count;
  const ShaderArtifactAddressWordPatch *address_word_patches;
  uint32_t address_word_patch_count;
  const ShaderArtifactPatchPoint *patch_points;
  uint32_t patch_point_count;
  bool comparison_compatible;
};

}  // namespace shader_artifacts
}  // namespace pi5v3d

#endif  // PI5V3D_SHADERS_SHADER_ARTIFACT_H
