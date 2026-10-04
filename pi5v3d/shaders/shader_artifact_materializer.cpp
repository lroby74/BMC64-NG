#include "pi5v3d/shaders/shader_artifact_materializer.h"

#include <string.h>

namespace pi5v3d {
namespace shader_artifacts {

namespace {

bool StringEquals(const char *a, const char *b) {
  if (a == nullptr || b == nullptr) {
    return false;
  }
  return strcmp(a, b) == 0;
}

void SetFailure(ShaderArtifactMaterializeStatus status,
                const char *text,
                ShaderArtifactMaterializeResult *result,
                const char **reason) {
  if (result != nullptr) {
    result->status = status;
  }
  if (reason != nullptr) {
    *reason = text;
  }
}

uint32_t Max32(uint32_t a, uint32_t b) {
  return a > b ? a : b;
}

bool AddBytes(uint32_t offset, uint32_t bytes, uint32_t *end) {
  if (end == nullptr || offset > 0xFFFFFFFFU - bytes) {
    return false;
  }
  *end = offset + bytes;
  return true;
}

bool ByteCount(uint32_t count, uint32_t word_size, uint32_t *bytes) {
  if (bytes == nullptr || word_size == 0 ||
      count > 0xFFFFFFFFU / word_size) {
    return false;
  }
  *bytes = count * word_size;
  return true;
}

void WriteLe32(uint8_t *dst, uint32_t value) {
  dst[0] = (uint8_t)value;
  dst[1] = (uint8_t)(value >> 8);
  dst[2] = (uint8_t)(value >> 16);
  dst[3] = (uint8_t)(value >> 24);
}

void WriteLe64(uint8_t *dst, uint64_t value) {
  WriteLe32(dst, (uint32_t)value);
  WriteLe32(dst + 4, (uint32_t)(value >> 32));
}

const ShaderArtifactBufferBinding *FindBinding(
    const ShaderArtifactBufferBinding *bindings,
    uint32_t binding_count,
    const char *name,
    const char *kind) {
  if (bindings == nullptr) {
    return nullptr;
  }

  for (uint32_t i = 0; i < binding_count; ++i) {
    if (StringEquals(bindings[i].name, name)) {
      return &bindings[i];
    }
  }

  for (uint32_t i = 0; i < binding_count; ++i) {
    if (StringEquals(bindings[i].kind, kind)) {
      return &bindings[i];
    }
  }

  return nullptr;
}

bool BindingCanWrite(const ShaderArtifactBufferBinding &binding,
                     uint32_t offset,
                     uint32_t bytes) {
  uint32_t end = 0;
  return binding.cpu != nullptr &&
         AddBytes(offset, bytes, &end) &&
         end <= binding.size;
}

struct ArtifactRegion {
  const ShaderArtifactBufferBinding *binding;
  uint32_t offset;
  uint32_t bytes;
};

constexpr uint32_t kMaxArtifactRegions = 64U;

bool AddArtifactRegion(
    const ShaderArtifactBufferBinding *binding,
    uint32_t offset,
    uint32_t bytes,
    ArtifactRegion *regions,
    uint32_t *region_count,
    ShaderArtifactMaterializeResult *result,
    const char **reason) {
  if (binding == nullptr || regions == nullptr || region_count == nullptr ||
      *region_count >= kMaxArtifactRegions ||
      !BindingCanWrite(*binding, offset, bytes)) {
    SetFailure(kShaderArtifactMaterializeBufferOverflow,
               "artifact-layout-overflow", result, reason);
    return false;
  }

  const uint32_t end = offset + bytes;
  for (uint32_t i = 0; i < *region_count; ++i) {
    if (regions[i].binding != binding) {
      continue;
    }
    const uint32_t other_end = regions[i].offset + regions[i].bytes;
    if (offset < other_end && regions[i].offset < end) {
      SetFailure(kShaderArtifactMaterializeOverlappingLayout,
                 "artifact-layout-overlap", result, reason);
      return false;
    }
  }

  regions[*region_count] = {binding, offset, bytes};
  ++*region_count;
  return true;
}

bool ValidateArtifactLayout(
    const ShaderArtifact &artifact,
    const ShaderArtifactBufferBinding *bindings,
    uint32_t binding_count,
    ShaderArtifactMaterializeResult *result,
    const char **reason) {
  ArtifactRegion regions[kMaxArtifactRegions];
  uint32_t region_count = 0;

  for (uint32_t i = 0; i < artifact.stage_count; ++i) {
    const ShaderArtifactStageCode &stage = artifact.stages[i];
    uint32_t bytes = 0;
    if (stage.qpu_words == nullptr ||
        !ByteCount(stage.qpu_word_count, sizeof(uint64_t), &bytes) ||
        !AddArtifactRegion(
            FindBinding(bindings, binding_count, stage.buffer_name,
                        "resource"),
            stage.code_offset, bytes, regions, &region_count,
            result, reason)) {
      return false;
    }
  }

  for (uint32_t i = 0; i < artifact.uniform_count; ++i) {
    const ShaderArtifactUniformBlock &uniform = artifact.uniforms[i];
    uint32_t bytes = 0;
    if (uniform.words == nullptr ||
        !ByteCount(uniform.word_count, sizeof(uint32_t), &bytes) ||
        !AddArtifactRegion(
            FindBinding(bindings, binding_count, uniform.buffer_name, "CL"),
            uniform.offset, bytes, regions, &region_count,
            result, reason)) {
      return false;
    }
  }

  for (uint32_t i = 0; i < artifact.sampler_count; ++i) {
    const ShaderArtifactSamplerBlock &sampler = artifact.samplers[i];
    uint32_t bytes = 0;
    if (sampler.words == nullptr ||
        !ByteCount(sampler.word_count, sizeof(uint32_t), &bytes) ||
        !AddArtifactRegion(
            FindBinding(bindings, binding_count, sampler.name, "sampler"),
            sampler.offset, bytes, regions, &region_count,
            result, reason)) {
      return false;
    }
  }

  for (uint32_t i = 0; i < artifact.data_block_count; ++i) {
    const ShaderArtifactDataBlock &block = artifact.data_blocks[i];
    uint32_t bytes = 0;
    if (block.words == nullptr ||
        !ByteCount(block.word_count, sizeof(uint32_t), &bytes) ||
        !AddArtifactRegion(
            FindBinding(bindings, binding_count, block.buffer_name,
                        block.buffer_kind),
            block.offset, bytes, regions, &region_count,
            result, reason)) {
      return false;
    }
  }

  return true;
}

bool ResolveAddress(const ShaderArtifactBufferBinding &binding,
                    uint32_t offset,
                    ShaderArtifactAddressEncoding encoding,
                    uint32_t *value) {
  if (value == nullptr || binding.v3d_address > 0xFFFFFFFFU - offset) {
    return false;
  }

  const uint32_t address = binding.v3d_address + offset;
  if (encoding == kArtifactAddressDirect) {
    *value = address;
    return true;
  }

  if (encoding == kArtifactAddressWordShiftedRight4) {
    if (address > 0x0FFFFFFFU) {
      return false;
    }
    *value = address << 4;
    return true;
  }

  return false;
}

const ShaderArtifactUniformBlock *FindUniformBlock(
    const ShaderArtifact &artifact,
    ShaderArtifactStage stage) {
  for (uint32_t i = 0; i < artifact.uniform_count; ++i) {
    if (artifact.uniforms[i].stage == stage) {
      return &artifact.uniforms[i];
    }
  }
  return nullptr;
}

const ShaderArtifactSamplerBlock *FindSamplerBlock(
    const ShaderArtifact &artifact,
    uint32_t index) {
  if (index >= artifact.sampler_count) {
    return nullptr;
  }
  return &artifact.samplers[index];
}

void UpdateRequiredBytes(const char *kind,
                         uint32_t end,
                         ShaderArtifactMaterializeResult *result) {
  if (result == nullptr) {
    return;
  }
  if (StringEquals(kind, "resource")) {
    result->required_resource_bytes =
        Max32(result->required_resource_bytes, end);
  } else if (StringEquals(kind, "CL")) {
    result->required_cl_bytes = Max32(result->required_cl_bytes, end);
  } else if (StringEquals(kind, "sampler")) {
    result->required_sampler_bytes =
        Max32(result->required_sampler_bytes, end);
  }
}

bool CopyStageCode(const ShaderArtifactStageCode &stage,
                   const ShaderArtifactBufferBinding *bindings,
                   uint32_t binding_count,
                   ShaderArtifactMaterializeResult *result,
                   const char **reason) {
  const ShaderArtifactBufferBinding *binding =
      FindBinding(bindings, binding_count, stage.buffer_name, "resource");
  if (binding == nullptr) {
    SetFailure(kShaderArtifactMaterializeMissingBinding,
               "missing-stage-code-binding", result, reason);
    return false;
  }

  uint32_t bytes = 0;
  uint32_t end = 0;
  if (!ByteCount(stage.qpu_word_count, sizeof(uint64_t), &bytes) ||
      !AddBytes(stage.code_offset, bytes, &end) ||
      !BindingCanWrite(*binding, stage.code_offset, bytes)) {
    SetFailure(kShaderArtifactMaterializeBufferOverflow,
               "stage-code-overflow", result, reason);
    return false;
  }

  uint8_t *dst = binding->cpu + stage.code_offset;
  for (uint32_t i = 0; i < stage.qpu_word_count; ++i) {
    WriteLe64(dst + i * sizeof(uint64_t), stage.qpu_words[i]);
  }
  if (result != nullptr) {
    ++result->stages_copied;
  }
  UpdateRequiredBytes(binding->kind, end, result);
  return true;
}

bool CopyUniformBlock(const ShaderArtifactUniformBlock &uniforms,
                      const ShaderArtifactBufferBinding *bindings,
                      uint32_t binding_count,
                      ShaderArtifactMaterializeResult *result,
                      const char **reason) {
  const ShaderArtifactBufferBinding *binding =
      FindBinding(bindings, binding_count, uniforms.buffer_name, "CL");
  if (binding == nullptr) {
    SetFailure(kShaderArtifactMaterializeMissingBinding,
               "missing-uniform-binding", result, reason);
    return false;
  }

  uint32_t bytes = 0;
  uint32_t end = 0;
  if (!ByteCount(uniforms.word_count, sizeof(uint32_t), &bytes) ||
      !AddBytes(uniforms.offset, bytes, &end) ||
      !BindingCanWrite(*binding, uniforms.offset, bytes)) {
    SetFailure(kShaderArtifactMaterializeBufferOverflow,
               "uniform-overflow", result, reason);
    return false;
  }

  uint8_t *dst = binding->cpu + uniforms.offset;
  for (uint32_t i = 0; i < uniforms.word_count; ++i) {
    WriteLe32(dst + i * sizeof(uint32_t), uniforms.words[i]);
  }
  if (result != nullptr) {
    ++result->uniforms_copied;
  }
  UpdateRequiredBytes(binding->kind, end, result);
  return true;
}

bool CopySamplerBlock(const ShaderArtifactSamplerBlock &sampler,
                      const ShaderArtifactBufferBinding *bindings,
                      uint32_t binding_count,
                      ShaderArtifactMaterializeResult *result,
                      const char **reason) {
  const ShaderArtifactBufferBinding *binding =
      FindBinding(bindings, binding_count, sampler.name, "sampler");
  if (binding == nullptr) {
    SetFailure(kShaderArtifactMaterializeMissingBinding,
               "missing-sampler-binding", result, reason);
    return false;
  }

  uint32_t bytes = 0;
  uint32_t end = 0;
  if (!ByteCount(sampler.word_count, sizeof(uint32_t), &bytes) ||
      !AddBytes(sampler.offset, bytes, &end) ||
      !BindingCanWrite(*binding, sampler.offset, bytes)) {
    SetFailure(kShaderArtifactMaterializeBufferOverflow,
               "sampler-overflow", result, reason);
    return false;
  }

  uint8_t *dst = binding->cpu + sampler.offset;
  for (uint32_t i = 0; i < sampler.word_count; ++i) {
    WriteLe32(dst + i * sizeof(uint32_t), sampler.words[i]);
  }
  if (result != nullptr) {
    ++result->samplers_copied;
  }
  UpdateRequiredBytes(binding->kind, end, result);
  return true;
}

bool CopyDataBlock(const ShaderArtifactDataBlock &block,
                   const ShaderArtifactBufferBinding *bindings,
                   uint32_t binding_count,
                   ShaderArtifactMaterializeResult *result,
                   const char **reason) {
  const ShaderArtifactBufferBinding *binding =
      FindBinding(bindings, binding_count, block.buffer_name,
                  block.buffer_kind);
  if (binding == nullptr) {
    SetFailure(kShaderArtifactMaterializeMissingBinding,
               "missing-data-block-binding", result, reason);
    return false;
  }

  uint32_t bytes = 0;
  uint32_t end = 0;
  if (block.words == nullptr ||
      !ByteCount(block.word_count, sizeof(uint32_t), &bytes) ||
      !AddBytes(block.offset, bytes, &end) ||
      !BindingCanWrite(*binding, block.offset, bytes)) {
    SetFailure(kShaderArtifactMaterializeBufferOverflow,
               "data-block-overflow", result, reason);
    return false;
  }

  uint8_t *dst = binding->cpu + block.offset;
  for (uint32_t i = 0; i < block.word_count; ++i) {
    WriteLe32(dst + i * sizeof(uint32_t), block.words[i]);
  }
  if (result != nullptr) {
    ++result->data_blocks_copied;
  }
  UpdateRequiredBytes(binding->kind, end, result);
  return true;
}

bool ResolvePatch(const ShaderArtifactPatchPoint &patch,
                  const ShaderArtifactBufferBinding *bindings,
                  uint32_t binding_count,
                  uint32_t *value,
                  ShaderArtifactMaterializeResult *result,
                  const char **reason) {
  const ShaderArtifactBufferBinding *target =
      FindBinding(bindings, binding_count, patch.buffer_name,
                  patch.buffer_kind);
  if (target == nullptr) {
    SetFailure(kShaderArtifactMaterializeMissingBinding,
               "missing-patch-target-binding", result, reason);
    return false;
  }

  if (!ResolveAddress(*target, patch.offset, patch.encoding, value)) {
    SetFailure(kShaderArtifactMaterializePatchOverflow,
               "patch-address-overflow", result, reason);
    return false;
  }
  return true;
}

bool ApplyPatchWord(const ShaderArtifact &artifact,
                    const ShaderArtifactPatchPoint &patch,
                    uint32_t value,
                    const ShaderArtifactBufferBinding *bindings,
                    uint32_t binding_count,
                    ShaderArtifactMaterializeResult *result,
                    const char **reason) {
  const ShaderArtifactBufferBinding *binding = nullptr;
  uint32_t offset = 0;

  if (patch.kind == kArtifactPatchUniformWordAddressCandidate) {
    const ShaderArtifactUniformBlock *uniforms =
        FindUniformBlock(artifact, patch.stage);
    if (uniforms == nullptr ||
        patch.word_index >= uniforms->word_count) {
      SetFailure(kShaderArtifactMaterializeInvalidArgument,
                 "invalid-uniform-patch", result, reason);
      return false;
    }
    binding = FindBinding(bindings, binding_count, uniforms->buffer_name, "CL");
    offset = uniforms->offset + patch.word_index * sizeof(uint32_t);
  } else if (patch.kind == kArtifactPatchSamplerWordAddressCandidate) {
    const ShaderArtifactSamplerBlock *sampler =
        FindSamplerBlock(artifact, patch.index);
    if (sampler == nullptr || patch.word_index >= sampler->word_count) {
      SetFailure(kShaderArtifactMaterializeInvalidArgument,
                 "invalid-sampler-patch", result, reason);
      return false;
    }
    binding = FindBinding(bindings, binding_count, sampler->name, "sampler");
    offset = sampler->offset + patch.word_index * sizeof(uint32_t);
  } else {
    return true;
  }

  if (binding == nullptr) {
    SetFailure(kShaderArtifactMaterializeMissingBinding,
               "missing-patch-source-binding", result, reason);
    return false;
  }
  if (!BindingCanWrite(*binding, offset, sizeof(uint32_t))) {
    SetFailure(kShaderArtifactMaterializeBufferOverflow,
               "patch-source-overflow", result, reason);
    return false;
  }

  WriteLe32(binding->cpu + offset, value);
  if (result != nullptr) {
    ++result->applied_patch_words;
  }
  return true;
}

bool ApplyAddressWordPatch(const ShaderArtifactAddressWordPatch &patch,
                           const ShaderArtifactBufferBinding *bindings,
                           uint32_t binding_count,
                           ShaderArtifactMaterializeResult *result,
                           const char **reason) {
  const ShaderArtifactBufferBinding *target =
      FindBinding(bindings, binding_count, patch.target_buffer_name,
                  patch.target_buffer_kind);
  if (target == nullptr) {
    SetFailure(kShaderArtifactMaterializeMissingBinding,
               "missing-address-patch-target-binding", result, reason);
    return false;
  }

  uint32_t value = 0;
  if (!ResolveAddress(*target, patch.target_offset, patch.encoding, &value)) {
    SetFailure(kShaderArtifactMaterializePatchOverflow,
               "address-patch-target-overflow", result, reason);
    return false;
  }

  const ShaderArtifactBufferBinding *write =
      FindBinding(bindings, binding_count, patch.write_buffer_name,
                  patch.write_buffer_kind);
  if (write == nullptr) {
    SetFailure(kShaderArtifactMaterializeMissingBinding,
               "missing-address-patch-write-binding", result, reason);
    return false;
  }

  if (!BindingCanWrite(*write, patch.write_offset, sizeof(uint32_t))) {
    SetFailure(kShaderArtifactMaterializeBufferOverflow,
               "address-patch-write-overflow", result, reason);
    return false;
  }

  WriteLe32(write->cpu + patch.write_offset, value);
  if (result != nullptr) {
    ++result->applied_address_word_patches;
  }
  return true;
}

}  // namespace

const char *ShaderArtifactMaterializeStatusName(
    ShaderArtifactMaterializeStatus status) {
  switch (status) {
    case kShaderArtifactMaterializeOk:
      return "ok";
    case kShaderArtifactMaterializeInvalidArgument:
      return "invalid-argument";
    case kShaderArtifactMaterializeMissingBinding:
      return "missing-binding";
    case kShaderArtifactMaterializeBufferOverflow:
      return "buffer-overflow";
    case kShaderArtifactMaterializeOverlappingLayout:
      return "overlapping-layout";
    case kShaderArtifactMaterializePatchOverflow:
      return "patch-overflow";
    case kShaderArtifactMaterializeUnsupportedPatch:
      return "unsupported-patch";
    default:
      return "unknown";
  }
}

bool MaterializeShaderArtifact(
    const ShaderArtifact &artifact,
    const ShaderArtifactBufferBinding *bindings,
    uint32_t binding_count,
    ShaderArtifactResolvedPatch *resolved_patches,
    uint32_t resolved_patch_capacity,
    ShaderArtifactMaterializeResult *result,
    const char **reason) {
  if (result != nullptr) {
    memset(result, 0, sizeof *result);
    result->status = kShaderArtifactMaterializeInvalidArgument;
  }
  if (reason != nullptr) {
    *reason = nullptr;
  }

  if (artifact.stages == nullptr || artifact.uniforms == nullptr ||
      artifact.patch_points == nullptr || bindings == nullptr ||
      binding_count == 0) {
    SetFailure(kShaderArtifactMaterializeInvalidArgument,
               "invalid-materialize-arguments", result, reason);
    return false;
  }
  if ((artifact.data_block_count != 0 && artifact.data_blocks == nullptr) ||
      (artifact.address_word_patch_count != 0 &&
       artifact.address_word_patches == nullptr)) {
    SetFailure(kShaderArtifactMaterializeInvalidArgument,
               "invalid-materialize-data-arguments", result, reason);
    return false;
  }

  if (resolved_patches != nullptr && resolved_patch_capacity > 0) {
    memset(resolved_patches, 0,
           resolved_patch_capacity * sizeof resolved_patches[0]);
  }

  if (!ValidateArtifactLayout(artifact, bindings, binding_count,
                              result, reason)) {
    return false;
  }

  for (uint32_t i = 0; i < artifact.stage_count; ++i) {
    if (!CopyStageCode(artifact.stages[i], bindings, binding_count,
                       result, reason)) {
      return false;
    }
  }
  for (uint32_t i = 0; i < artifact.uniform_count; ++i) {
    if (!CopyUniformBlock(artifact.uniforms[i], bindings, binding_count,
                          result, reason)) {
      return false;
    }
  }
  for (uint32_t i = 0; i < artifact.sampler_count; ++i) {
    if (!CopySamplerBlock(artifact.samplers[i], bindings, binding_count,
                          result, reason)) {
      return false;
    }
  }
  for (uint32_t i = 0; i < artifact.data_block_count; ++i) {
    if (!CopyDataBlock(artifact.data_blocks[i], bindings, binding_count,
                       result, reason)) {
      return false;
    }
  }
  for (uint32_t i = 0; i < artifact.address_word_patch_count; ++i) {
    if (!ApplyAddressWordPatch(artifact.address_word_patches[i],
                               bindings, binding_count, result, reason)) {
      return false;
    }
  }

  for (uint32_t i = 0; i < artifact.patch_point_count; ++i) {
    const ShaderArtifactPatchPoint &patch = artifact.patch_points[i];
    uint32_t value = 0;
    if (!ResolvePatch(patch, bindings, binding_count, &value,
                      result, reason)) {
      return false;
    }

    bool applied = false;
    if (patch.kind == kArtifactPatchUniformWordAddressCandidate ||
        patch.kind == kArtifactPatchSamplerWordAddressCandidate) {
      if (!ApplyPatchWord(artifact, patch, value, bindings, binding_count,
                          result, reason)) {
        return false;
      }
      applied = true;
    }

    if (resolved_patches != nullptr) {
      if (i >= resolved_patch_capacity) {
        SetFailure(kShaderArtifactMaterializePatchOverflow,
                   "resolved-patch-capacity", result, reason);
        return false;
      }
      resolved_patches[i] = {
        patch.kind,
        patch.stage,
        patch.index,
        patch.word_index,
        patch.buffer_name,
        patch.buffer_kind,
        patch.offset,
        patch.encoding,
        value,
        applied
      };
    }
    if (result != nullptr) {
      ++result->resolved_patch_points;
    }
  }

  if (result != nullptr) {
    result->status = kShaderArtifactMaterializeOk;
  }
  if (reason != nullptr) {
    *reason = "ok";
  }
  return true;
}

}  // namespace shader_artifacts
}  // namespace pi5v3d
