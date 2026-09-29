#ifndef BMX_PI4V3D_EFFECT_POLICY_H
#define BMX_PI4V3D_EFFECT_POLICY_H

#include <string.h>

namespace pi4v3d {

struct FramePresetPolicy {
  bool production;
  bool runtime;
  bool menu_effects;
  bool force_scanlines;
};

inline FramePresetPolicy ResolveFramePresetPolicy(const char *name) {
  FramePresetPolicy policy = {};
  if (name == nullptr) {
    return policy;
  }
  if (strcmp(name, "frame_copy") == 0) {
    policy.runtime = true;
    return policy;
  }
  if (strcmp(name, "sharp") != 0 &&
      strcmp(name, "scanlines") != 0 &&
      strcmp(name, "crt") != 0 &&
      strcmp(name, "crt_soft") != 0) {
    return policy;
  }
  policy.production = true;
  policy.runtime = true;
  policy.menu_effects = true;
  policy.force_scanlines = strcmp(name, "scanlines") == 0;
  return policy;
}

}  // namespace pi4v3d

#endif
