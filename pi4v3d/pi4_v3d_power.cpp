#include "pi4v3d/pi4_v3d_power.h"

namespace pi4v3d {

namespace {

const uint32_t kInvalidRegister = 0xffffffffU;
const uint32_t kAsbBridgeId = 0x62726467U;
const uint32_t kPmEnable = 1U << 12;
const uint32_t kPmV3dResetNot = 1U << 6;
const uint32_t kAsbReqStop = 1U << 0;
const uint32_t kAsbAck = 1U << 1;
const uint32_t kClockOn = 1U << 0;
const uint32_t kClockDoesNotExist = 1U << 1;

bool RegisterReadable(uint32_t value) {
  return value != kInvalidRegister;
}

bool BridgeActive(uint32_t value) {
  return (value & (kAsbReqStop | kAsbAck)) == 0U;
}

bool BridgeStopped(uint32_t value) {
  return (value & (kAsbReqStop | kAsbAck)) ==
         (kAsbReqStop | kAsbAck);
}

}  // namespace

PowerInfo AnalyzePowerSnapshot(const PowerSnapshot &snapshot) {
  PowerInfo info = {};
  info.controls_readable = RegisterReadable(snapshot.pm_grafx) &&
                           RegisterReadable(snapshot.asb_bridge_id) &&
                           RegisterReadable(snapshot.asb_slave) &&
                           RegisterReadable(snapshot.asb_master);
  info.bridge_id_valid =
      info.controls_readable && snapshot.asb_bridge_id == kAsbBridgeId;
  info.domain_enabled = (snapshot.pm_grafx & kPmEnable) != 0U;
  info.reset_released = (snapshot.pm_grafx & kPmV3dResetNot) != 0U;
  info.slave_active = BridgeActive(snapshot.asb_slave);
  info.master_active = BridgeActive(snapshot.asb_master);
  info.slave_stopped = BridgeStopped(snapshot.asb_slave);
  info.master_stopped = BridgeStopped(snapshot.asb_master);
  info.clock_exists = snapshot.clock_state_valid &&
                      (snapshot.clock_state & kClockDoesNotExist) == 0U;
  info.clock_on = info.clock_exists &&
                  (snapshot.clock_state & kClockOn) != 0U;
  info.clock_running = snapshot.measured_clock_valid &&
                       snapshot.measured_clock_hz != 0U;
  info.safe_to_probe = info.controls_readable && info.bridge_id_valid &&
                       info.domain_enabled && info.reset_released &&
                       info.slave_active && info.master_active &&
                       info.clock_running;
  info.power_on_eligible = info.controls_readable && info.bridge_id_valid &&
                           info.domain_enabled && !info.reset_released &&
                           info.slave_stopped && info.master_stopped &&
                           info.clock_exists && info.clock_running;
  return info;
}

}  // namespace pi4v3d
