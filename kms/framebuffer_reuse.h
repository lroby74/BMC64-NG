#ifndef BMX_KMS_FRAMEBUFFER_REUSE_H
#define BMX_KMS_FRAMEBUFFER_REUSE_H

namespace bmxkms {

inline bool CanReuseFramebuffer(unsigned current_width,
                                unsigned current_height,
                                unsigned current_depth,
                                unsigned requested_width,
                                unsigned requested_height,
                                unsigned requested_depth) {
  return current_depth == requested_depth &&
         current_width >= requested_width &&
         current_height >= requested_height;
}

inline unsigned ExpandedFramebufferDimension(unsigned current,
                                              unsigned requested) {
  return current > requested ? current : requested;
}

}  // namespace bmxkms

#endif  // BMX_KMS_FRAMEBUFFER_REUSE_H
