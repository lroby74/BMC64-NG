#include "pi4v3d/pi4_v3d_identity.h"

namespace pi4v3d {

namespace {

const uint32_t kInvalidRegister = 0xffffffffU;

bool RegisterAccessible(uint32_t value) {
  return value != 0U && value != kInvalidRegister;
}

bool AllRegistersReadable(const IdentityRegisters &registers) {
  return registers.hub_ident0 != kInvalidRegister &&
         registers.hub_ident1 != kInvalidRegister &&
         registers.hub_ident2 != kInvalidRegister &&
         registers.hub_ident3 != kInvalidRegister &&
         registers.core_ident0 != kInvalidRegister &&
         registers.core_ident1 != kInvalidRegister &&
         registers.core_ident2 != kInvalidRegister &&
         registers.mmu_debug != kInvalidRegister;
}

}  // namespace

IdentityInfo DecodeIdentity(const IdentityRegisters &registers) {
  IdentityInfo info = {};
  info.accessible = AllRegistersReadable(registers) &&
                    RegisterAccessible(registers.hub_ident1) &&
                    RegisterAccessible(registers.core_ident0);
  info.version = (registers.hub_ident1 & 0xfU) * 10U +
                 ((registers.hub_ident1 >> 4) & 0xfU);
  info.cores = (registers.hub_ident1 >> 8) & 0xfU;
  info.ip_revision = (registers.hub_ident3 >> 8) & 0xffU;
  info.has_mmu = (registers.hub_ident2 & (1U << 8)) != 0U;
  info.core_version = (registers.core_ident0 >> 24) & 0xffU;
  info.physical_address_width = 30U +
      ((registers.mmu_debug >> 8) & 0xfU);
  info.virtual_address_width = 30U +
      ((registers.mmu_debug >> 4) & 0xfU);
  info.mmu_version = registers.mmu_debug & 0xfU;

  info.supported = info.accessible && info.version == 42U &&
                   info.cores == 1U && info.has_mmu &&
                   info.core_version == 4U;
  info.reference_profile = info.accessible &&
      registers.hub_ident1 == 0x000e1124U &&
      registers.hub_ident2 == 0x00000100U &&
      registers.hub_ident3 == 0x00000e00U &&
      registers.core_ident0 == 0x04443356U &&
      registers.core_ident1 == 0x81001422U &&
      registers.core_ident2 == 0x40078121U;
  return info;
}

}  // namespace pi4v3d
