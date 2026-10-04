#include "v3dcrt/shaders/shader_package.h"

#include <string.h>

namespace v3dcrt {
namespace shaders {

namespace {

bool StringEquals(const char *left, const char *right) {
  return left != nullptr && right != nullptr && strcmp(left, right) == 0;
}

bool Fail(const char *text, const char **reason) {
  if (reason != nullptr) {
    *reason = text;
  }
  return false;
}

}  // namespace

const ShaderStageProgram *FindShaderStage(const ShaderPackage &package,
                                          ShaderStageKind stage) {
  if (package.stages == nullptr) {
    return nullptr;
  }
  for (uint32_t i = 0; i < package.stage_count; ++i) {
    if (package.stages[i].stage == stage) {
      return &package.stages[i];
    }
  }
  return nullptr;
}

bool ValidateShaderPackage(const ShaderPackage &package,
                           const ShaderBackendCapabilities &capabilities,
                           const char **reason) {
  if (reason != nullptr) {
    *reason = nullptr;
  }
  if (!StringEquals(package.schema, "bmx.v3d_shader_package.v2")) {
    return Fail("unsupported-schema", reason);
  }
  if (package.id == nullptr || package.id[0] == '\0' ||
      package.content_sha256 == nullptr ||
      strlen(package.content_sha256) != 64 ||
      package.provenance == nullptr || package.provenance[0] == '\0') {
    return Fail("missing-provenance", reason);
  }
  if (!StringEquals(package.target_profile, capabilities.target_profile)) {
    return Fail("target-profile-mismatch", reason);
  }
  if (package.v3d_version != capabilities.v3d_version ||
      package.v3d_revision != capabilities.v3d_revision) {
    return Fail("target-version-mismatch", reason);
  }
  if (package.stages == nullptr || package.stage_count != 3) {
    return Fail("stage-count-mismatch", reason);
  }

  bool seen[3] = {false, false, false};
  for (uint32_t i = 0; i < package.stage_count; ++i) {
    const ShaderStageProgram &stage = package.stages[i];
    const uint32_t stage_index = static_cast<uint32_t>(stage.stage);
    if (stage_index >= 3 || seen[stage_index]) {
      return Fail("duplicate-or-invalid-stage", reason);
    }
    seen[stage_index] = true;
    if (stage.qpu_words == nullptr || stage.qpu_word_count == 0) {
      return Fail("missing-qpu-code", reason);
    }
    if (stage.uniform_count != 0 && stage.uniforms == nullptr) {
      return Fail("missing-uniform-contract", reason);
    }
    for (uint32_t uniform = 0; uniform < stage.uniform_count; ++uniform) {
      if (stage.uniforms[uniform].kind == nullptr ||
          stage.uniforms[uniform].kind[0] == '\0' ||
          stage.uniforms[uniform].semantic == nullptr) {
        return Fail("invalid-uniform-contract", reason);
      }
    }
    if (stage.requirements.threads == 0 ||
        stage.requirements.threads > capabilities.max_threads) {
      return Fail("unsupported-thread-count", reason);
    }
    if (stage.requirements.tmu_count > capabilities.max_tmu_count) {
      return Fail("unsupported-tmu-count", reason);
    }
    if (stage.requirements.vpm_input_size > capabilities.max_vpm_io_size ||
        stage.requirements.vpm_output_size > capabilities.max_vpm_io_size) {
      return Fail("unsupported-vpm-size", reason);
    }
    if (stage.requirements.spill_size != 0 &&
        !capabilities.supports_spilling) {
      return Fail("unsupported-spill", reason);
    }
  }
  return true;
}

}  // namespace shaders
}  // namespace v3dcrt
