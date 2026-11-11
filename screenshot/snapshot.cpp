#include "snapshot.h"
extern "C" {
#include "third_party/common/circle.h"
}
#include <stdlib.h>
#include <string.h>
#include <limits.h>
namespace bmx { namespace screenshot {
namespace {
uint32_t Expand(uint16_t p) {
    unsigned r = p >> 11, g = (p >> 5) & 63, b = p & 31;
    return ((r * 8 + r / 4) << 16) | ((g * 4 + g / 16) << 8) | (b * 8 + b / 4);
}
uint16_t Pack(uint32_t p) { return ((p >> 8) & 0xf800) | ((p >> 5) & 0x7e0) | ((p >> 3) & 31); }
int Clamp(int x, int lo, int hi) { return x < lo ? lo : x > hi ? hi : x; }
unsigned Lerp(unsigned a, unsigned b, unsigned w) { return (a * (256 - w) + b * w + 128) >> 8; }
uint32_t Filter(uint32_t a, uint32_t b, uint32_t c, uint32_t d, unsigned wx, unsigned wy,
                bool local, bool packed) {
    uint32_t result = 0;
    for (int channel = 0; channel < (packed ? 3 : 4); ++channel) {
        int shift = packed ? (channel == 0 ? 0 : channel == 1 ? 5 : 11) : channel * 8;
        unsigned mask = packed ? (channel == 1 ? 63 : 31) : 255;
        unsigned aa = (a >> shift) & mask, bb = (b >> shift) & mask;
        unsigned cc = (c >> shift) & mask, dd = (d >> shift) & mask;
        uint64_t v;
        if (local) {
            uint64_t top = aa * (65536U - wx) + bb * wx;
            uint64_t bottom = cc * (65536U - wx) + dd * wx;
            v = (top * (65536U - wy) + bottom * wy) >> 32;
        } else v = Lerp(Lerp(aa, bb, wx), Lerp(cc, dd, wx), wy);
        result |= (uint32_t)v << shift;
    }
    return result;
}
}
Snapshot::Snapshot() : copy_us(0), cache_us(0), arena_(nullptr), row_(nullptr), maps_(nullptr), capacity_(0), used_(0),
    maximum_width_(0), stage_(0), width_(0), height_(0), screen_width_(0), screen_height_(0),
    row_number_(-1), count_(0) {}
Snapshot::~Snapshot() { free(arena_); free(row_); free(maps_); }
bool Snapshot::Reserve(size_t bytes, int width) {
    if (width <= 0 || width > INT_MAX / 3 || bytes > SIZE_MAX - 16) return false;
    if (bytes > capacity_) {
        void *p = realloc(arena_, bytes);
        if (!p) return false;
        arena_ = static_cast<uint8_t *>(p); capacity_ = bytes;
    }
    if (width > maximum_width_) {
        Sample *m = static_cast<Sample *>(malloc((size_t)width * MaxLayers * sizeof(Sample)));
        uint8_t *r = static_cast<uint8_t *>(malloc((size_t)width * 3));
        if (!m || !r) { free(m); free(r); return false; }
        free(maps_); free(row_); maps_ = m; row_ = r; maximum_width_ = width;
    }
    return true;
}
void Snapshot::Reset(int stage, int width, int height, int sw, int sh) {
    copy_us = cache_us = 0;
    used_ = count_ = 0; stage_ = stage; width_ = width; height_ = height;
    screen_width_ = sw; screen_height_ = sh; row_number_ = -1;
}
bool Snapshot::Add(const SnapshotLayer &l) {
    if (!l.pixels || count_ == MaxLayers || l.width <= 0 || l.height <= 0 ||
        (l.bpp != 1 && l.bpp != 2) || l.width > INT_MAX / l.bpp ||
        l.pitch < l.width * l.bpp || l.sw <= 0 || l.sh <= 0 || l.dw <= 0 || l.dh <= 0 ||
        l.sx < 0 || l.sy < 0 || l.sw > l.width - l.sx || l.sh > l.height - l.sy) return false;
    size_t pitch = (size_t)l.width * l.bpp;
    if ((size_t)l.height > SIZE_MAX / pitch) return false;
    size_t bytes = pitch * l.height, offset = (used_ + 1) & ~size_t(1);
    if (offset > capacity_ || bytes > capacity_ - offset) return false;
    SnapshotLayer &copy = layers_[count_++]; copy = l;
    copy.pixels = arena_ + offset; copy.pitch = (int)pitch;
    uint64_t start = circle_get_ticks64();
    for (int y = 0; y < l.height; ++y)
        memcpy(arena_ + offset + (size_t)y * pitch, l.pixels + (size_t)y * l.pitch, pitch);
    copy_us += circle_get_ticks64() - start;
    used_ = offset + bytes;
    return true;
}
Snapshot::Sample Snapshot::Coordinate(const SnapshotLayer &l, int output, bool horizontal) const {
    int size = horizontal ? width_ : height_;
    int source = horizontal ? l.sw : l.sh, start = horizontal ? l.sx : l.sy;
    int destination = horizontal ? l.dw : l.dh;
    int native = output;
    int64_t f;
    if (stage_ == SCREENSHOT_SCREEN) {
        native = (int)((int64_t)output * (horizontal ? screen_width_ : screen_height_) / size);
        native -= horizontal ? l.dx : l.dy;
        if (native < 0 || native >= destination) return {0, 0, 0, false};
        f = l.processed ? ((int64_t)native * source << 16) / destination
                        : (int64_t)native * (((int64_t)source << 16) / destination);
    } else {
        if (stage_ != SCREENSHOT_EMULATOR) native = (int)((int64_t)output * destination / size);
        else destination = size;
        f = ((int64_t)native * source << 16) / destination;
    }
    int first = Clamp(start + (int)(f >> 16), start, start + source - 1);
    return {first, first + 1 < start + source ? first + 1 : first,
            (unsigned)(stage_ == SCREENSHOT_SCREEN ? (f >> 8) & 255 : f & 65535), true};
}
bool Snapshot::Prepare() {
    if (width_ <= 0 || height_ <= 0 || width_ > maximum_width_ ||
        screen_width_ <= 0 || screen_height_ <= 0) return false;
    for (unsigned i = 0; i < count_; ++i) {
        for (unsigned c = 0; c < 256; ++c) layers_[i].rgb[c] = Expand(layers_[i].palette[c]);
        for (int x = 0; x < width_; ++x) maps_[i * maximum_width_ + x] = Coordinate(layers_[i], x, true);
    }
    return stage_ == SCREENSHOT_SCREEN || count_ == 1;
}
uint32_t Snapshot::Pixel(const SnapshotLayer &l, int x, int y, bool alpha) const {
    const uint8_t *row = l.pixels + (size_t)y * l.pitch;
    if (l.bpp == 2) {
        uint16_t p = reinterpret_cast<const uint16_t *>(row)[x];
        return stage_ == SCREENSHOT_SCREEN ? p : Expand(p);
    }
    unsigned index = row[x];
    return alpha ? l.argb[index] : stage_ == SCREENSHOT_SCREEN ? l.palette[index] : l.rgb[index];
}
void Snapshot::Row(int y) {
    if (stage_ != SCREENSHOT_SCREEN && count_ == 1) {
        const SnapshotLayer &l = layers_[0];
        if (l.sw == width_ && l.sh == height_ &&
            (stage_ == SCREENSHOT_EMULATOR || (l.dw == width_ && l.dh == height_))) {
            const uint8_t *source = l.pixels + (size_t)(l.sy + y) * l.pitch;
            for (int x = 0; x < width_; ++x) {
                uint32_t p = l.bpp == 1 ? l.rgb[source[l.sx + x]]
                    : Expand(reinterpret_cast<const uint16_t *>(source)[l.sx + x]);
                row_[x * 3] = p >> 16; row_[x * 3 + 1] = p >> 8; row_[x * 3 + 2] = p;
            }
            row_number_ = y;
            return;
        }
    }
    memset(row_, 0, (size_t)width_ * 3);
    for (unsigned i = 0; i < count_; ++i) {
        const SnapshotLayer &l = layers_[i];
        Sample sy = Coordinate(l, y, false);
        if (!sy.visible) continue;
        bool alpha = stage_ == SCREENSHOT_SCREEN && l.bpp == 1 && l.transparent;
        for (int x = 0; x < width_; ++x) {
            const Sample &sx = maps_[i * maximum_width_ + x];
            if (!sx.visible) continue;
            uint32_t p = Pixel(l, sx.first, sy.first, alpha);
            if (l.interpolate && (sx.weight || sy.weight)) p = Filter(p, Pixel(l, sx.second, sy.first, alpha),
                Pixel(l, sx.first, sy.second, alpha), Pixel(l, sx.second, sy.second, alpha),
                sx.weight, sy.weight, stage_ != SCREENSHOT_SCREEN, stage_ == SCREENSHOT_SCREEN && !alpha);
            uint8_t *out = row_ + x * 3;
            if (stage_ == SCREENSHOT_SCREEN) {
                if (alpha) {
                    unsigned a = p >> 24;
                    if (!a) continue;
                    if (l.interpolate && a != 255) {
                        uint32_t blend = 0;
                        for (int c = 0; c < 3; ++c)
                            blend |= ((((p >> (16 - 8 * c)) & 255) * a + out[c] * (255 - a) + 127) / 255) << (16 - 8 * c);
                        p = blend;
                    }
                    p = Pack(p);
                }
                p = Expand((uint16_t)p);
            }
            out[0] = p >> 16; out[1] = p >> 8; out[2] = p;
        }
    }
    row_number_ = y;
}
bool Snapshot::Read(size_t offset, uint8_t *output, size_t bytes) {
    size_t pitch = (size_t)width_ * 3, size = pitch * height_;
    if (!pitch || offset > size || bytes > size - offset) return false;
    while (bytes) {
        int y = (int)(offset / pitch);
        size_t x = offset % pitch, n = pitch - x;
        if (n > bytes) n = bytes;
        if (row_number_ != y) Row(y);
        memcpy(output, row_ + x, n); output += n; offset += n; bytes -= n;
    }
    return true;
}
} }
