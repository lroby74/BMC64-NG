#ifndef BMX_SCREENSHOT_SNAPSHOT_H
#define BMX_SCREENSHOT_SNAPSHOT_H
#include <stddef.h>
#include <stdint.h>
namespace bmx { namespace screenshot {

struct SnapshotLayer {
    const uint8_t *pixels;
    int width, height, pitch, bpp;
    int sx, sy, sw, sh, dx, dy, dw, dh;
    bool processed, interpolate, transparent;
    uint16_t palette[256];
    uint32_t argb[256], rgb[256];
};
class Snapshot {
public:
    static const unsigned MaxLayers = 8;
    Snapshot();
    ~Snapshot();
    bool Reserve(size_t bytes, int maximum_width);
    void Reset(int stage, int width, int height, int screen_width, int screen_height);
    bool Add(const SnapshotLayer &layer);
    bool Prepare();
    bool Read(size_t offset, uint8_t *output, size_t bytes);
    uint64_t copy_us, cache_us;
    unsigned Count() const { return count_; }
private:
    Snapshot(const Snapshot &) = delete;
    Snapshot &operator=(const Snapshot &) = delete;
    struct Sample { int first, second; unsigned weight; bool visible; };
    Sample Coordinate(const SnapshotLayer &, int output, bool horizontal) const;
    uint32_t Pixel(const SnapshotLayer &, int x, int y, bool alpha) const;
    void Row(int y);
    SnapshotLayer layers_[MaxLayers];
    uint8_t *arena_, *row_;
    Sample *maps_;
    size_t capacity_, used_;
    int maximum_width_, stage_, width_, height_, screen_width_, screen_height_, row_number_;
    unsigned count_;
};
} }
#endif
