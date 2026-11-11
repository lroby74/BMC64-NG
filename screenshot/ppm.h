#ifndef BMX_SCREENSHOT_PPM_H
#define BMX_SCREENSHOT_PPM_H
#include <stddef.h>
#include <stdio.h>
namespace bmx { namespace screenshot {
inline int PpmHeader(char *out, size_t capacity, int width, int height) {
    return snprintf(out, capacity, "P6\n%d %d\n255\n", width, height);
}

inline bool Fit(int source_width, int source_height, int limit_width,
                int limit_height, int *width, int *height) {
    if (source_width <= 0 || source_height <= 0 || limit_width <= 0 || limit_height <= 0)
        return false;
    *width = source_width;
    *height = source_height;
    if (*width > limit_width) {
        *height = (int)((long long)source_height * limit_width / source_width);
        *width = limit_width;
    }
    if (*height > limit_height) {
        *width = (int)((long long)source_width * limit_height / source_height);
        *height = limit_height;
    }
    if (*width < 1) *width = 1;
    if (*height < 1) *height = 1;
    return true;
}
} }
#endif
