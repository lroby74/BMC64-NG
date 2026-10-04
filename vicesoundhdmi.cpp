


// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at

//     https://www.apache.org/licenses/LICENSE-2.0


// distributed under the License is distributed on an "AS IS" BASIS,

// See the License for the specific language governing permissions and
// limitations under the License.

#include "vicesoundhdmi.h"
#include <assert.h>
#include <math.h>
#include <string.h>



#define SCALE_WORDS 2048



extern "C" unsigned long raspi_snd_scartati;




extern "C" unsigned long raspi_snd_tagliati;

ViceSoundHDMI::ViceSoundHDMI(CInterruptSystem *pInterrupt, unsigned nPorta)
    : CHDMISoundBaseDevice(pInterrupt, SAMPLE_RATE, HDMI_CHUNK_WORDS, nPorta),
      m_nChannels(1), m_nGain(256), m_bQueueAllocated(FALSE),
      m_pScaled(0) {}

ViceSoundHDMI::~ViceSoundHDMI(void) {
  delete[] m_pScaled;
  m_pScaled = 0;
}


int ViceSoundHDMI::VolumeToGain(int nVolume) {
  if (nVolume <= VCHIQ_SOUND_VOLUME_MIN) {
    return 0;
  }
  if (nVolume > VCHIQ_SOUND_VOLUME_MAX) {
    nVolume = VCHIQ_SOUND_VOLUME_MAX;
  }
  double gain = pow(10.0, (double)nVolume / 2000.0) * 256.0;
  if (gain < 0.0) {
    gain = 0.0;
  }
  if (gain > 512.0) {
    gain = 512.0;
  }
  return (int)(gain + 0.5);
}

boolean ViceSoundHDMI::Playback(int volume, int channels) {
  assert(!IsActive());
  assert(channels == 1 || channels == 2);

  m_nChannels = (unsigned)channels;
  m_nGain = VolumeToGain(volume);

  if (!m_bQueueAllocated) {
    if (!AllocateQueueFrames(HDMI_QUEUE_FRAMES)) {
      return FALSE;
    }
    if (m_pScaled == 0) {
      m_pScaled = new s16[SCALE_WORDS];
    }
    m_bQueueAllocated = TRUE;
  }



  SetWriteFormat(SoundFormatSigned16, m_nChannels);

  return Start();
}

boolean ViceSoundHDMI::PlaybackActive(void) const { return IsActive(); }

void ViceSoundHDMI::CancelPlayback(void) { Cancel(); }

void ViceSoundHDMI::SetControl(int nVolume, TVCHIQSoundDestination Destination) {

  (void)Destination;
  m_nGain = VolumeToGain(nVolume);
}



unsigned ViceSoundHDMI::AddChunk(s16 *pBuffer, unsigned nChunkSize) {
  if (!m_bQueueAllocated || pBuffer == 0 || nChunkSize == 0) {
    return 0;
  }

  const unsigned frame_words = m_nChannels;
  unsigned pos = 0;

  while (pos < nChunkSize) {
    unsigned words = nChunkSize - pos;
    const s16 *src;

    if (m_nGain == 256) {
      src = pBuffer + pos;
    } else {
      if (words > SCALE_WORDS) {
        words = SCALE_WORDS;
      }
      for (unsigned i = 0; i < words; i++) {
        int v = ((int)pBuffer[pos + i] * m_nGain) >> 8;
        if (v > 32767) {
          v = 32767;
          raspi_snd_tagliati++;
        } else if (v < -32768) {
          v = -32768;
          raspi_snd_tagliati++;
        }
        m_pScaled[i] = (s16)v;
      }
      src = m_pScaled;
    }


    words -= words % frame_words;
    if (words == 0) {
      break;
    }




    unsigned off = 0;
    int tries = 0;
    while (off < words && tries < 1000) {
      int done = Write((const unsigned char *)src + off * sizeof(s16),
                       (words - off) * sizeof(s16));
      if (done <= 0) {
        tries++;
        continue;
      }
      off += (unsigned)done / sizeof(s16);
      tries = 0;
    }
    if (off < words) {
      raspi_snd_scartati += words - off;
    }

    pos += words;
  }

  return 0;
}


unsigned ViceSoundHDMI::BufferSpaceSamples(void) {
  if (!m_bQueueAllocated) {
    return 0;
  }
  unsigned size = GetQueueSizeFrames();
  unsigned used = GetQueueFramesAvail();
  return used >= size ? 0 : size - used;
}
