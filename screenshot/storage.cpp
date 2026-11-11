






#include "storage.h"
#include "ppm.h"
extern "C" {
#include "third_party/common/circle.h"
}
#include <dirent.h>
#include <limits.h>
#include <string.h>
#include <strings.h>
#include "third_party/circle-stdlib/include/wrap_fatfs.h"

namespace bmx { namespace screenshot {
namespace {
const char kPrefix[] = "bmc64ng-";
}
Writer::Writer() : render_us(0), scan_us(0), write_us(0), maximum_(0) {}
bool Writer::Number(const char *name) {
    const size_t prefix = sizeof kPrefix - 1;
    if (strncasecmp(name, kPrefix, prefix) || name[prefix] < '0' || name[prefix] > '9') return true;
    const char *p = name + prefix;
    unsigned long n = 0;
    bool overflow = false;
    while (*p >= '0' && *p <= '9') {
        unsigned d = *p++ - '0';
        if (n > (ULONG_MAX - d) / 10) overflow = true;
        if (!overflow) n = n * 10 + d;
    }
    if (strcasecmp(p, ".ppm") && strcasecmp(p, ".png")) return true;
    if (overflow || n == ULONG_MAX) return false;
    if (n > maximum_) maximum_ = n;
    return true;
}
bool Writer::WritePng(FILE *fp, Reader reader, void *context, int width, int height) {
    if (!png_.Begin(width, height)) return false;
    bool ok = true, done = false;
    while (ok && !done) {
        size_t bytes = 0;
        uint64_t start = circle_get_ticks64();
        ok = png_.Next(reader, context, buffer_, sizeof buffer_, bytes, done);
        render_us += circle_get_ticks64() - start;
        bmc_giri[1]++;
        if (ok && bytes && fwrite(buffer_, 1, bytes, fp) != bytes) ok = false;
    }
    png_.Reset();
    return ok;
}
bool Writer::WritePpm(FILE *fp, Reader reader, void *context, int width, int height) {
    char header[64];
    int header_size = PpmHeader(header, sizeof header, width, height);
    if (header_size <= 0 || (size_t)header_size >= sizeof header ||
        fwrite(header, 1, (size_t)header_size, fp) != (size_t)header_size) return false;
    const size_t size = (size_t)width * height * 3U;
    for (size_t offset = 0; offset < size;) {
        size_t count = size - offset;
        if (count > sizeof buffer_) count = sizeof buffer_;
        uint64_t start = circle_get_ticks64();
        bool ok = reader(context, offset, buffer_, count);
        render_us += circle_get_ticks64() - start;
        bmc_giri[1]++;
        if (!ok || fwrite(buffer_, 1, count, fp) != count) return false;
        offset += count;
    }
    return true;
}
bool Writer::Write(const char *volume_directory, const char *directory, Reader reader, void *context,
                   int width, int height, int format, char *name, size_t name_size) {
    render_us = scan_us = write_us = 0;
    maximum_ = 0;
    if (!volume_directory || !directory || !reader || width <= 0 || height <= 0 ||
        (format != SCREENSHOT_FORMAT_PPM && format != SCREENSHOT_FORMAT_PNG) ||
        (size_t)width > SIZE_MAX / 3U / (size_t)height) return false;
    uint64_t start = circle_get_ticks64();
    DIR *dir = opendir(directory);
    if (dir == nullptr) {
        FRESULT r = f_mkdir(volume_directory);
        if (r != FR_OK && r != FR_EXIST) return false;
        dir = opendir(directory);
        if (dir == nullptr) return false;
    }
    bool ok = true;
    for (struct dirent *entry = readdir(dir); entry != nullptr; entry = readdir(dir)) {
        if (!Number(entry->d_name)) { ok = false; break; }
    }
    if (closedir(dir) != 0) ok = false;
    scan_us = circle_get_ticks64() - start;
    if (!ok || maximum_ == ULONG_MAX) return false;
    char path[192];
    int count = snprintf(path, sizeof path, "%s/%s%04lu.%s", directory, kPrefix, maximum_ + 1,
                         format == SCREENSHOT_FORMAT_PNG ? "png" : "ppm");
    if (count < 0 || (size_t)count >= sizeof path) return false;
    start = circle_get_ticks64();
    FILE *fp = fopen(path, "wb");
    if (fp == nullptr) return false;
    ok = format == SCREENSHOT_FORMAT_PNG ? WritePng(fp, reader, context, width, height)
                                         : WritePpm(fp, reader, context, width, height);
    if (fclose(fp) != 0) ok = false;
    write_us = circle_get_ticks64() - start - render_us;
    if (!ok) {
        remove(path);
        return false;
    }
    if (name && name_size) snprintf(name, name_size, "%s", path + strlen(directory) + 1);
    return true;
}
} }
