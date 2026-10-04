


// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at

//     https://www.apache.org/licenses/LICENSE-2.0


// distributed under the License is distributed on an "AS IS" BASIS,

// See the License for the specific language governing permissions and
// limitations under the License.

#include "vicesoundusb.h"
#include "usblog.h"

#include <circle/usb/usbaudiostreaming.h>
#include <circle/devicenameservice.h>

#include <stdio.h>
#include <assert.h>
#include <math.h>
#include <string.h>

#define SCALE_WORDS 2048



extern "C" unsigned long raspi_snd_scartati;




extern "C" unsigned long raspi_snd_tagliati;

namespace {


bool sample_rate_supported(
    const CUSBAudioStreamingDevice::TDeviceInfo &info,
    unsigned sample_rate) {
  for (unsigned i = 0; i < info.SampleRateRanges; i++) {
    const unsigned min_rate = info.SampleRateRange[i].Min;
    const unsigned max_rate = info.SampleRateRange[i].Max;
    const unsigned resolution = info.SampleRateRange[i].Resolution;
    if (min_rate <= sample_rate && sample_rate <= max_rate &&
        (!resolution || (sample_rate - min_rate) % resolution == 0)) {
      return true;
    }
  }

  return false;
}

CUSBAudioStreamingDevice *find_usb_output_device(void) {
  for (unsigned subdevice = 1; ; subdevice++) {
    char device_name[16];
    snprintf(device_name, sizeof(device_name), "uaudio1-%u", subdevice);

    CDevice *device =
        CDeviceNameService::Get()->GetDevice(device_name, FALSE);
    if (!device) {
      break;
    }

    CUSBAudioStreamingDevice *streaming_device =
        static_cast<CUSBAudioStreamingDevice *>(device);
    if (streaming_device->GetDeviceInfo().IsOutput) {
      return streaming_device;
    }
  }

  return nullptr;
}

}

boolean ViceSoundUSB::UscitaPresente(void) {
  return find_usb_output_device() != nullptr;
}


unsigned ViceSoundUSB::ScegliFrequenza(void) {
  CUSBAudioStreamingDevice *streaming_device = find_usb_output_device();
  if (streaming_device) {
    CUSBAudioStreamingDevice::TDeviceInfo info =
        streaming_device->GetDeviceInfo();

    static const unsigned preferred_rates[] = {SAMPLE_RATE, 48000};
    for (unsigned i = 0; i < sizeof preferred_rates / sizeof preferred_rates[0];
         i++) {
      if (sample_rate_supported(info, preferred_rates[i])) {
        return preferred_rates[i];
      }
    }

    for (unsigned i = 0; i < info.SampleRateRanges; i++) {
      if (info.SampleRateRange[i].Min) {
        return info.SampleRateRange[i].Min;
      }
    }
  }

  return SAMPLE_RATE;
}

ViceSoundUSB::ViceSoundUSB(unsigned frequenza)
    : CUSBSoundBaseDevice(frequenza, CUSBSoundBaseDevice::DeviceModeTXOnly, 0),
      m_nChannels(1), m_nGain(256), m_bQueueAllocated(FALSE), m_pScaled(0),
      m_nFrequenza(frequenza) {}

ViceSoundUSB::~ViceSoundUSB(void) {
  delete[] m_pScaled;
  m_pScaled = 0;
}

int ViceSoundUSB::VolumeToGain(int nVolume) {
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

boolean ViceSoundUSB::Playback(int volume, int channels) {
  assert(channels == 1 || channels == 2);

  m_nChannels = (unsigned)channels;
  m_nGain = VolumeToGain(volume);

  if (!m_bQueueAllocated) {
    if (!AllocateQueueFrames(USB_QUEUE_FRAMES)) {
      return FALSE;
    }
    if (m_pScaled == 0) {
      m_pScaled = new s16[SCALE_WORDS];
    }
    m_bQueueAllocated = TRUE;
  }

  SetWriteFormat(SoundFormatSigned16, m_nChannels);





  {


    CUSBAudioStreamingDevice *apparato = 0;
    for (unsigned d = 1; d <= 2 && apparato == 0; d++) {
      for (unsigned s = 1; s <= 4 && apparato == 0; s++) {
        char nome[16];
        snprintf(nome, sizeof(nome), "uaudio%u-%u", d, s);
        CDevice *trovato = CDeviceNameService::Get()->GetDevice(nome, FALSE);
        if (trovato == 0) {
          continue;
        }
        CUSBAudioStreamingDevice *a = (CUSBAudioStreamingDevice *)trovato;
        if (a->GetDeviceInfo().IsOutput) {
          usblog_riga("audio USB: apparato %s", nome);
          apparato = a;
        }
      }
    }
    if (apparato == 0) {
      usblog_riga("audio USB: nessun apparato di uscita trovato");
    } else {
      CUSBAudioStreamingDevice::TDeviceInfo info = apparato->GetDeviceInfo();
      usblog_riga("audio USB: %u canali, %u gamme di frequenza, chiesti %u Hz",
                  info.NumChannels, info.SampleRateRanges,
                  m_nFrequenza);
      for (unsigned i = 0; i < info.SampleRateRanges; i++) {
        usblog_riga("   gamma %u: da %u a %u Hz (passo %u)", i,
                    info.SampleRateRange[i].Min, info.SampleRateRange[i].Max,
                    info.SampleRateRange[i].Resolution);
      }
    }
  }



  boolean partito = Start();
  usblog_riga("audio USB: avvio %s", partito ? "riuscito" : "FALLITO");
  return partito;
}

boolean ViceSoundUSB::PlaybackActive(void) const { return IsActive(); }

void ViceSoundUSB::CancelPlayback(void) { Cancel(); }

void ViceSoundUSB::SetControl(int nVolume, TVCHIQSoundDestination Destination) {
  (void)Destination;
  m_nGain = VolumeToGain(nVolume);
}







boolean ViceSoundUSB::CambiaCanali(int channels) {
  if (!m_bQueueAllocated || (channels != 1 && channels != 2)) {
    return FALSE;
  }
  m_nChannels = (unsigned)channels;
  SetWriteFormat(SoundFormatSigned16, m_nChannels);
  usblog_riga("audio USB: ora %u canali, senza riaprire", m_nChannels);
  return TRUE;
}

unsigned ViceSoundUSB::AddChunk(s16 *pBuffer, unsigned nChunkSize) {
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

unsigned ViceSoundUSB::BufferSpaceSamples(void) {
  if (!m_bQueueAllocated) {
    return 0;
  }
  unsigned size = GetQueueSizeFrames();
  unsigned used = GetQueueFramesAvail();
  return used >= size ? 0 : size - used;
}
