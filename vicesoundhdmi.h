














// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at

//     https://www.apache.org/licenses/LICENSE-2.0


// distributed under the License is distributed on an "AS IS" BASIS,

// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef _vice_sound_hdmi_h
#define _vice_sound_hdmi_h

#include "defs.h"
#include "soundtypes.h"
#include "vicesoundout.h"
#include <circle/interrupt.h>
#include <circle/sound/hdmisoundbasedevice.h>
#include <circle/types.h>



#define FRAG_SIZE 256
#define NUM_FRAGS 16
#define BYTES_PER_SAMPLE 2



#define HDMI_QUEUE_FRAMES 4096


#define HDMI_CHUNK_WORDS (384 * 4)

class ViceSoundHDMI : public ViceSoundOut, private CHDMISoundBaseDevice {
public:

  ViceSoundHDMI(CInterruptSystem *pInterrupt, unsigned nPorta = 0);
  ~ViceSoundHDMI(void);

  boolean Playback(int volume, int channels) override;
  boolean PlaybackActive(void) const override;
  void CancelPlayback(void) override;



  void SetControl(int nVolume, TVCHIQSoundDestination Destination) override;

  unsigned AddChunk(s16 *pBuffer, unsigned nChunkSize) override;
  unsigned BufferSpaceSamples(void) override;

private:

  static int VolumeToGain(int nVolume);

  unsigned m_nChannels;
  int m_nGain;
  boolean m_bQueueAllocated;
  s16 *m_pScaled;
};

#endif
