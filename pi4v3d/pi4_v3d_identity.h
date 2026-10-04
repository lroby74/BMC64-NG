#ifndef PI4V3D_PI4_V3D_IDENTITY_H
#define PI4V3D_PI4_V3D_IDENTITY_H

#include <stdint.h>

namespace pi4v3d {

struct IdentityRegisters {
  uint32_t hub_ident0;
  uint32_t hub_ident1;
  uint32_t hub_ident2;
  uint32_t hub_ident3;
  uint32_t core_ident0;
  uint32_t core_ident1;
  uint32_t core_ident2;
  uint32_t mmu_debug;
};

struct IdentityInfo {
  bool accessible;
  bool supported;
  bool reference_profile;
  uint32_t version;
  uint32_t cores;
  uint32_t ip_revision;
  bool has_mmu;
  uint32_t core_version;
  uint32_t physical_address_width;
  uint32_t virtual_address_width;
  uint32_t mmu_version;
};

IdentityInfo DecodeIdentity(const IdentityRegisters &registers);

}  // namespace pi4v3d

#endif  // PI4V3D_PI4_V3D_IDENTITY_H
