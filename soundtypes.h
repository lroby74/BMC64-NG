







// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at

//     https://www.apache.org/licenses/LICENSE-2.0


// distributed under the License is distributed on an "AS IS" BASIS,

// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef _vice_sound_types_h
#define _vice_sound_types_h




#if RASPPI >= 5 || defined(BMC64_HDMI_SOUND)
#define BMC64_USE_HDMI_SOUND 1
#endif



#if RASPPI >= 4
#define BMC64_HAVE_USB_SOUND 1
#endif




#if RASPPI >= 4 || defined(BMC64_HDMI_SOUND)
#define BMC64_HAVE_HDMI_SOUND 1
#endif

#define VCHIQ_SOUND_VOLUME_MIN -10000
#define VCHIQ_SOUND_VOLUME_DEFAULT 0
#define VCHIQ_SOUND_VOLUME_MAX 400

enum TVCHIQSoundDestination {
  VCHIQSoundDestinationAuto,
  VCHIQSoundDestinationHeadphones,
  VCHIQSoundDestinationHDMI,

  VCHIQSoundDestinationUSB,
  VCHIQSoundDestinationUnknown
};

#endif
