#ifndef BMX_SCREENSHOT_PNG_H
#define BMX_SCREENSHOT_PNG_H
#include <stddef.h>
#include <stdint.h>
namespace bmx { namespace screenshot {


class Png {
public:
    typedef bool (*Reader)(void *, size_t, uint8_t *, size_t);
    Png();
    ~Png();
    bool Begin(int width, int height);
    void Reset();
    bool Next(Reader reader, void *context, uint8_t *out, size_t capacity,
              size_t &bytes, bool &done);
private:
    Png(const Png &) = delete;
    Png &operator=(const Png &) = delete;
    void *compressor_;
    uint8_t *row_;
    size_t row_bytes_, row_offset_;
    int width_, height_, y_, phase_;
};
} }
#endif
