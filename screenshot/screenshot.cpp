





#include "screenshot.h"
#include "snapshot.h"
#include "storage.h"
#include "ppm.h"
#include "fbl.h"
#include <stdio.h>
#include <stdint.h>
extern "C" {
#include "third_party/common/circle.h"
}
namespace bmx { namespace screenshot {
namespace {
Writer writer;
Snapshot snapshot;
bool Read(void *context, size_t offset, uint8_t *out, size_t bytes) {
    return static_cast<Snapshot *>(context)->Read(offset, out, bytes);
}
}
int Take(FrameBufferLayer &layer, int stage, int maximum_width,
         int x, int y, int width, int height, int format, char *name, size_t name_size) {
    if (name && name_size) name[0] = 0;
    if (stage < SCREENSHOT_EMULATOR || stage > SCREENSHOT_SCREEN || maximum_width < 0) return 0;
    if (format != SCREENSHOT_FORMAT_PPM && format != SCREENSHOT_FORMAT_PNG) return 0;
    const uint64_t started = circle_get_ticks64();
    int hdmi_width, hdmi_height, source_width, source_height, output_width, output_height;
    if (!FrameBufferLayer::CaptureDimensions(&hdmi_width, &hdmi_height)) return 0;
    if (stage == SCREENSHOT_SCREEN) {
        source_width = hdmi_width;
        source_height = hdmi_height;
    } else if (!layer.CaptureLocalDimensions(stage, width, height,
                                             &source_width, &source_height)) return 0;
    int limit = maximum_width > 0 && maximum_width < hdmi_width ? maximum_width : hdmi_width;
    if (maximum_width > 0) {

        source_height = (int)((int64_t)source_height * limit / source_width);
        if (source_height < 1) source_height = 1;
        source_width = limit;
    }
    if (!Fit(source_width, source_height, limit, hdmi_height, &output_width, &output_height) ||
        (size_t)output_width > SIZE_MAX / 3U / (size_t)output_height) return 0;


    if (!snapshot.Reserve(FrameBufferLayer::CaptureSnapshotBytes(), hdmi_width)) return 0;
    snapshot.Reset(stage, output_width, output_height, hdmi_width, hdmi_height);
    bool ok = stage == SCREENSHOT_SCREEN
        ? FrameBufferLayer::CaptureSnapshotScreen(snapshot)
        : layer.CaptureSnapshotLayer(snapshot, stage, x, y, width, height);
    ok = ok && snapshot.Prepare() &&
         writer.Write("SD:/screenshots", "/screenshots", Read, &snapshot, output_width, output_height,
                      format, name, name_size);
    printf("screenshot: stage=%d %dx%d format=%s ok=%d file=%s layers=%u copy_us=%llu cache_us=%llu "
           "scan_us=%llu render_us=%llu write_us=%llu total_us=%llu\n",
           stage, output_width, output_height, format == SCREENSHOT_FORMAT_PNG ? "png" : "ppm",
           ok ? 1 : 0, name && name[0] ? name : "-", snapshot.Count(),
           (unsigned long long)snapshot.copy_us, (unsigned long long)snapshot.cache_us,
           (unsigned long long)writer.scan_us, (unsigned long long)writer.render_us,
           (unsigned long long)writer.write_us,
           (unsigned long long)(circle_get_ticks64() - started));
    return ok ? 1 : 0;
}
} }
