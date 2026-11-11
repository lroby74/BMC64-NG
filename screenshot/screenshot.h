#ifndef BMX_SCREENSHOT_H
#define BMX_SCREENSHOT_H
#include <stddef.h>



class FrameBufferLayer;
namespace bmx { namespace screenshot {
int Take(FrameBufferLayer &layer, int stage, int maximum_width,
         int x, int y, int width, int height, int format, char *name, size_t name_size);
} }
#endif
