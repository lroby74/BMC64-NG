#include "png.h"
#define MINIZ_NO_ZLIB_COMPATIBLE_NAMES
#include "third_party/miniz/miniz.h"
#include <stdlib.h>
#include <string.h>

namespace bmx { namespace screenshot {
namespace {
void BigEndian(uint8_t *p, uint32_t n) {
    p[0] = n >> 24; p[1] = n >> 16; p[2] = n >> 8; p[3] = n;
}

size_t Chunk(uint8_t *out, const char *type, size_t bytes) {
    BigEndian(out, bytes);
    memcpy(out + 4, type, 4);
    BigEndian(out + 8 + bytes, mz_crc32(0, out + 4, bytes + 4));
    return bytes + 12;
}
}
Png::Png() : compressor_(nullptr), row_(nullptr), row_bytes_(0), row_offset_(0),
    width_(0), height_(0), y_(0), phase_(0) {}
Png::~Png() { Reset(); }
void Png::Reset() {
    free(compressor_); compressor_ = nullptr;
    free(row_); row_ = nullptr;
}
bool Png::Begin(int width, int height) {
    Reset();
    if (width <= 0 || height <= 0 || (size_t)width > (SIZE_MAX - 1) / 3 ||
        (size_t)width > SIZE_MAX / 3 / (size_t)height) return false;
    width_ = width; height_ = height; y_ = phase_ = 0;
    row_bytes_ = (size_t)width * 3 + 1; row_offset_ = row_bytes_;
    compressor_ = malloc(sizeof(tdefl_compressor));
    row_ = static_cast<uint8_t *>(malloc(row_bytes_));

    if (!compressor_ || !row_ || tdefl_init(static_cast<tdefl_compressor *>(compressor_),
        nullptr, nullptr, 1 | TDEFL_GREEDY_PARSING_FLAG | TDEFL_WRITE_ZLIB_HEADER) != TDEFL_STATUS_OKAY) {
        Reset(); return false;
    }
    return true;
}
bool Png::Next(Reader reader, void *context, uint8_t *out, size_t capacity,
               size_t &bytes, bool &done) {
    bytes = 0; done = false;
    if (!compressor_ || !reader || !out || capacity < 33) return false;
    if (phase_ == 0) {
        static const uint8_t signature[] = {137,80,78,71,13,10,26,10};
        memcpy(out, signature, 8);
        BigEndian(out + 16, width_); BigEndian(out + 20, height_);
        out[24] = 8; out[25] = 2; out[26] = out[27] = out[28] = 0;
        bytes = 8 + Chunk(out + 8, "IHDR", 13); phase_ = 1;
        return true;
    }
    if (phase_ == 2) {
        bytes = Chunk(out, "IEND", 0); done = true; phase_ = 3;
        return true;
    }
    if (phase_ != 1) return false;
    if (row_offset_ == row_bytes_ && y_ < height_) {
        if (!reader(context, (size_t)y_ * (row_bytes_ - 1), row_ + 1, row_bytes_ - 1)) return false;

        for (size_t i = row_bytes_ - 1; i > 3; --i) row_[i] -= row_[i - 3];
        row_[0] = 1; row_offset_ = 0; ++y_;
    }
    size_t input = row_bytes_ - row_offset_, output = capacity - 12;
    tdefl_status status = tdefl_compress(static_cast<tdefl_compressor *>(compressor_),
        row_ + row_offset_, &input, out + 8, &output,
        y_ == height_ ? TDEFL_FINISH : TDEFL_NO_FLUSH);
    if (status < 0) return false;
    row_offset_ += input;
    if (output) bytes = Chunk(out, "IDAT", output);
    if (status == TDEFL_STATUS_DONE) phase_ = 2;
    return true;
}
} }
