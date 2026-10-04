#ifndef BMX_KMS_KMS_MODE_H
#define BMX_KMS_KMS_MODE_H

#include "kms/kms_types.h"

namespace bmxkms {

enum TimingParsePolicy {
  kTimingParsePermissive = 0,
  kTimingParseProgressiveStrict,
};

bool ResolveBmcMode(unsigned hdmi_group, unsigned hdmi_mode,
                    const char *hdmi_timings, const char *named_mode,
                    TimingParsePolicy timing_policy, Mode *mode);

}  // namespace bmxkms

#endif  // BMX_KMS_KMS_MODE_H
