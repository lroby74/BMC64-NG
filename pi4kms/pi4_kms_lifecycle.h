#ifndef PI4KMS_PI4_KMS_LIFECYCLE_H
#define PI4KMS_PI4_KMS_LIFECYCLE_H

namespace pi4kms {

// Tracks the one-way ownership transition from the firmware display service
// to native ARM scanout.  Firmware recovery is deliberately bounded: it is
// available only until the first native display list has reached the active
// HVS slot.
class RecoveryLifecycle {
public:
  RecoveryLifecycle() : firmware_display_claimed_(false),
                        native_scanout_committed_(false) {}

  void Reset() {
    firmware_display_claimed_ = false;
    native_scanout_committed_ = false;
  }

  void MarkFirmwareDisplayClaimed() {
    firmware_display_claimed_ = true;
  }

  // Returns true exactly once, when the first verified native present commits
  // the ownership transition.  A commit is invalid before firmware handover.
  bool CommitNativePresent() {
    if (!firmware_display_claimed_ || native_scanout_committed_) {
      return false;
    }
    native_scanout_committed_ = true;
    return true;
  }

  bool FirmwareDisplayClaimed() const {
    return firmware_display_claimed_;
  }

  bool NativeScanoutCommitted() const {
    return native_scanout_committed_;
  }

  bool FirmwareRestorePermitted() const {
    return firmware_display_claimed_ && !native_scanout_committed_;
  }

private:
  bool firmware_display_claimed_;
  bool native_scanout_committed_;
};

}  // namespace pi4kms

#endif  // PI4KMS_PI4_KMS_LIFECYCLE_H
