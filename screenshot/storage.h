#ifndef BMX_SCREENSHOT_STORAGE_H
#define BMX_SCREENSHOT_STORAGE_H
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include "png.h"


namespace bmx { namespace screenshot {
class Writer {
public:
    typedef bool (*Reader)(void *, size_t, uint8_t *, size_t);
    Writer();

    bool Write(const char *volume_directory, const char *directory, Reader reader, void *context,
               int width, int height, int format, char *name, size_t name_size);
    uint64_t render_us, scan_us, write_us;
private:
    Writer(const Writer &) = delete;
    Writer &operator=(const Writer &) = delete;
    bool Number(const char *name);
    bool WritePng(FILE *fp, Reader reader, void *context, int width, int height);
    bool WritePpm(FILE *fp, Reader reader, void *context, int width, int height);
    Png png_;
    unsigned long maximum_;
#ifndef BMX_SCREENSHOT_WRITE_CHUNK
#define BMX_SCREENSHOT_WRITE_CHUNK 32768
#endif
    uint8_t buffer_[BMX_SCREENSHOT_WRITE_CHUNK];
};
} }
#endif
