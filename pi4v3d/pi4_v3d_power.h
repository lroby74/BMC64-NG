#ifndef PI4V3D_PI4_V3D_POWER_H
#define PI4V3D_PI4_V3D_POWER_H

#include <stdint.h>

namespace pi4v3d {

struct PowerSnapshot {
  uint32_t pm_grafx;
  uint32_t asb_bridge_id;
  uint32_t asb_slave;
  uint32_t asb_master;
  bool clock_state_valid;
  uint32_t clock_state;
  bool measured_clock_valid;
  uint32_t measured_clock_hz;
};

struct PowerInfo {
  bool controls_readable;
  bool bridge_id_valid;
  bool domain_enabled;
  bool reset_released;
  bool slave_active;
  bool master_active;
  bool slave_stopped;
  bool master_stopped;
  bool clock_exists;
  bool clock_on;
  bool clock_running;
  bool safe_to_probe;
  bool power_on_eligible;
};

PowerInfo AnalyzePowerSnapshot(const PowerSnapshot &snapshot);

}  // namespace pi4v3d

#endif  // PI4V3D_PI4_V3D_POWER_H
