#ifndef BMX_PI5KMS_SCALING_ORDER_H
#define BMX_PI5KMS_SCALING_ORDER_H

namespace pi5kms {

enum HvsScaling {
  kHvsScalingNone = 0,
  kHvsScalingPpf,
  kHvsScalingTpz,
};

enum HvsScalingParameter {
  kHvsHorizontalPpf = 0,
  kHvsVerticalPpf,
  kHvsHorizontalTpz,
  kHvsVerticalTpz,
};

inline unsigned BuildHvsScalingParameterOrder(
    HvsScaling horizontal, HvsScaling vertical,
    HvsScalingParameter order[4]) {
  unsigned count = 0;

  // VC6 display lists group parameters by scaler type, not by axis.
  if (horizontal == kHvsScalingPpf) order[count++] = kHvsHorizontalPpf;
  if (vertical == kHvsScalingPpf) order[count++] = kHvsVerticalPpf;
  if (horizontal == kHvsScalingTpz) order[count++] = kHvsHorizontalTpz;
  if (vertical == kHvsScalingTpz) order[count++] = kHvsVerticalTpz;

  return count;
}

}  // namespace pi5kms

#endif
