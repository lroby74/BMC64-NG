#ifndef PI5V3D_SHADERS_SHADER_ARTIFACT_MATERIALIZER_H
#define PI5V3D_SHADERS_SHADER_ARTIFACT_MATERIALIZER_H

#include "pi5v3d/shaders/shader_artifact.h"

#include <stdint.h>

namespace pi5v3d {
namespace shader_artifacts {

enum ShaderArtifactMaterializeStatus {
  kShaderArtifactMaterializeOk = 0,
  kShaderArtifactMaterializeInvalidArgument,
  kShaderArtifactMaterializeMissingBinding,
  kShaderArtifactMaterializeBufferOverflow,
  kShaderArtifactMaterializeOverlappingLayout,
  kShaderArtifactMaterializePatchOverflow,
  kShaderArtifactMaterializeUnsupportedPatch
};

struct ShaderArtifactBufferBinding {
  const char *name;
  const char *kind;
  uint8_t *cpu;
  uint32_t size;
  uint32_t v3d_address;
};

struct ShaderArtifactResolvedPatch {
  ShaderArtifactPatchKind kind;
  ShaderArtifactStage stage;
  uint32_t index;
  uint32_t word_index;
  const char *buffer_name;
  const char *buffer_kind;
  uint32_t offset;
  ShaderArtifactAddressEncoding encoding;
  uint32_t value;
  bool applied;
};

struct ShaderArtifactMaterializeResult {
  ShaderArtifactMaterializeStatus status;
  uint32_t stages_copied;
  uint32_t uniforms_copied;
  uint32_t samplers_copied;
  uint32_t data_blocks_copied;
  uint32_t resolved_patch_points;
  uint32_t applied_patch_words;
  uint32_t applied_address_word_patches;
  uint32_t required_resource_bytes;
  uint32_t required_cl_bytes;
  uint32_t required_sampler_bytes;
};

const char *ShaderArtifactMaterializeStatusName(
    ShaderArtifactMaterializeStatus status);

bool MaterializeShaderArtifact(
    const ShaderArtifact &artifact,
    const ShaderArtifactBufferBinding *bindings,
    uint32_t binding_count,
    ShaderArtifactResolvedPatch *resolved_patches,
    uint32_t resolved_patch_capacity,
    ShaderArtifactMaterializeResult *result,
    const char **reason);

}  // namespace shader_artifacts
}  // namespace pi5v3d

#endif  // PI5V3D_SHADERS_SHADER_ARTIFACT_MATERIALIZER_H
