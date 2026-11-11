#ifndef BMX_PI5_V3D71_SHADER_STATE_H
#define BMX_PI5_V3D71_SHADER_STATE_H

#include <stdint.h>

namespace pi5v3d {





inline uint32_t FragmentShaderStateFlags(unsigned ip_revision) {
  const bool draw_index_record = ip_revision >= 10U;
  return (1U << 1) |
         (1U << (draw_index_record ? 15U : 12U)) |
         (1U << (draw_index_record ? 21U : 18U));
}

}

#endif
