#ifndef BMX_PLATFORM_HARDWARE_IDENTITY_H
#define BMX_PLATFORM_HARDWARE_IDENTITY_H
#if defined(RASPI_COMPILE)
#include <circle/bcmpropertytags.h>
#include <stdio.h>
#endif
namespace bmx { namespace platform {

inline bool ReadBoardSerial(char serial[17]) {
#if defined(RASPI_COMPILE)
    if (!serial) return false;
    TPropertyTagSerial tag = {};
    CBcmPropertyTags tags;
    if (!tags.GetTag(PROPTAG_GET_BOARD_SERIAL, &tag, sizeof tag) ||
        (!tag.Serial[0] && !tag.Serial[1])) return false;
    snprintf(serial, 17, "%08x%08x", tag.Serial[1], tag.Serial[0]);
    return true;
#else
    (void)serial; return false;
#endif
}
} }
#endif
