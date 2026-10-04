







// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at

//     https://www.apache.org/licenses/LICENSE-2.0


// distributed under the License is distributed on an "AS IS" BASIS,

// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef _vice_sound_out_h
#define _vice_sound_out_h

#include "soundtypes.h"
#include <circle/types.h>

class ViceSoundOut {
public:
  virtual ~ViceSoundOut(void) {}

  virtual boolean Playback(int volume, int channels) = 0;
  virtual boolean PlaybackActive(void) const = 0;
  virtual void CancelPlayback(void) = 0;
  virtual void SetControl(int nVolume, TVCHIQSoundDestination Destination) = 0;
  virtual unsigned AddChunk(s16 *pBuffer, unsigned nChunkSize) = 0;
  virtual unsigned BufferSpaceSamples(void) = 0;



  virtual boolean CambiaCanali(int channels) {
    (void)channels;
    return FALSE;
  }
};

#endif
