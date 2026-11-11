

#include "fbl.h"
extern "C" {
#include "third_party/common/circle.h"
}
#include <stddef.h>

bool FrameBufferLayer::CaptureLocalDimensions(int stage, int raw_width, int raw_height,
                                               int *width, int *height) {
    if (!allocated_ || !pixels_ || !width || !height) return false;
    *width = stage == SCREENSHOT_EMULATOR ? raw_width : dst_w_;
    *height = stage == SCREENSHOT_EMULATOR ? raw_height : dst_h_;
    return *width > 0 && *height > 0;
}

#include "snapshot.h"
#include <string.h>
bool FrameBufferLayer::CaptureSnapshotLayer(bmx::screenshot::Snapshot &snapshot, int stage,
        int x, int y, int width, int height, bool screen) {
    if (!allocated_ || !pixels_) return false;
    bmx::screenshot::SnapshotLayer l = {};
    l.pixels = pixels_; l.width = fb_width_; l.height = fb_height_;
    l.pitch = fb_pitch_; l.bpp = bytes_per_pixel_;
    l.sx = stage == SCREENSHOT_EMULATOR ? x : src_x_;
    l.sy = stage == SCREENSHOT_EMULATOR ? y : src_y_;
    l.sw = stage == SCREENSHOT_EMULATOR ? width : src_w_;
    l.sh = stage == SCREENSHOT_EMULATOR ? height : src_h_;
    l.dx = dst_x_; l.dy = dst_y_; l.dw = dst_w_; l.dh = dst_h_;
    l.transparent = transparency_;
    memcpy(l.palette, pal_565_, sizeof l.palette);
    memcpy(l.argb, pal_argb_, sizeof l.argb);
    if (stage == SCREENSHOT_CRT || screen) {
        const uint64_t cache_start = circle_get_ticks64();
        unsigned rw, rh, rp;
        const uint8_t *rendered = nullptr;
        if (CaptureProcessedSource(&rendered, &rw, &rh, &rp)) {
            l.pixels = rendered; l.pitch = rp; l.bpp = 2;
            l.sx = l.sy = 0; l.width = l.sw = rw; l.height = l.sh = rh;
            l.processed = true; l.transparent = false;
        } else if (!screen && uses_shader_) return false;
        snapshot.cache_us += circle_get_ticks64() - cache_start;
    }
    l.interpolate = stage != SCREENSHOT_EMULATOR && !l.processed && CaptureInterpolation();
    if (screen) l.interpolate = l.interpolate && (l.sw != l.dw || l.sh != l.dh) &&
                                (!l.transparent || l.bpp == 1);
    return snapshot.Add(l);
}
