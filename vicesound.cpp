//
// vicesound.cpp
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "vicesound.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

extern "C" {
#include <strings.h>
}

#include <circle/sched/scheduler.h>
#include <circle/timer.h>


extern "C" unsigned long raspi_vchiq_byte;
extern "C" unsigned raspi_vchiq_coda;
extern "C" unsigned raspi_vchiq_inizio;
extern "C" unsigned long raspi_vchiq_attese;

ViceSound::ViceSound(CVCHIQDevice *pVCHIQDevice,
                     TVCHIQSoundDestination Destination)
    : ViceSoundBaseDevice(pVCHIQDevice, SAMPLE_RATE, CHUNK_SIZE, Destination) {
  src_pos = 0;
  src_size = 0;
  src_buffer = 0;
  bytes_buffered = 0;
  num_channels = 1;
}

ViceSound::~ViceSound(void) {}











boolean ViceSound::Playback(int volume, int channels) {
  assert(!IsActive());
  num_channels = channels == 2 ? 2 : 1;
  SetVolume(volume);
  SetChannels(VCHIQ_CANALI);
  raspi_vchiq_byte = 0;
  raspi_vchiq_attese = 0;
  raspi_vchiq_inizio = CTimer::GetClockTicks();


  printf("[AUDIO] VCHIQ a due canali, il VICE ne manda %u\n", num_channels);
  return Start();
}

boolean ViceSound::PlaybackActive(void) const { return IsActive(); }

void ViceSound::CancelPlayback(void) { Cancel(); }

void ViceSound::SetControl(int nVolume, TVCHIQSoundDestination Destination) {
  ViceSoundBaseDevice::SetControl(nVolume, Destination);
}

unsigned ViceSound::AddChunk(s16 *pBuffer, unsigned nChunkSize) {
  if (num_channels == 2) {

    AggiungiPezzi(pBuffer, nChunkSize & ~1u);
  } else {

    while (nChunkSize > 0) {
      unsigned quadri = nChunkSize;
      if (quadri > CHUNK_SIZE / 2) {
        quadri = CHUNK_SIZE / 2;
      }
      for (unsigned i = 0; i < quadri; i++) {
        stereo_buffer[2 * i] = pBuffer[i];
        stereo_buffer[2 * i + 1] = pBuffer[i];
      }
      AggiungiPezzi(stereo_buffer, 2 * quadri);
      pBuffer += quadri;
      nChunkSize -= quadri;
    }
  }


  src_buffer = 0;
  src_size = 0;
  return 0;
}

void ViceSound::AggiungiPezzi(s16 *pBuffer, unsigned nChunkSize) {
  // nChunkSize is vice's sample count, not necessarily equal
  // to VC4's chunk size.  Copy what we're given into our intermediate
  // buffer and flush CHUNK_SIZE packets out of it for VC4.
  src_pos = 0;
  src_buffer = pBuffer;

  while (nChunkSize > 0) {
    // This triggers the GetChunk call below. Yeild not required.
    src_size = nChunkSize;
    if (src_size > CHUNK_SIZE)
      src_size = CHUNK_SIZE;

    WriteChunk();
    src_pos += src_size;
    nChunkSize -= src_size;
  }
}

unsigned ViceSound::GetChunk(s16 *pBuffer, unsigned nChunkSize) {

  assert(pBuffer != 0);
  assert(nChunkSize > 0);
  assert((nChunkSize & 1) == 0);




  if (bytes_buffered >=
      FRAG_SIZE * NUM_FRAGS * BYTES_PER_SAMPLE * VCHIQ_CANALI) {
    raspi_vchiq_attese++;
    while (bytes_buffered >=
           FRAG_SIZE * NUM_FRAGS * BYTES_PER_SAMPLE * VCHIQ_CANALI) {
      CScheduler::Get()->Yield();
    }
  }

  if (src_buffer == 0 || src_size == 0) {

    memset(pBuffer, 0, FRAG_SIZE * BYTES_PER_SAMPLE);
    src_size = FRAG_SIZE;
  } else {
    // We always give VC4 nChunkSize packets.
    memcpy(pBuffer, src_buffer + src_pos, src_size * BYTES_PER_SAMPLE);
  }
  raspi_vchiq_byte += src_size * BYTES_PER_SAMPLE;
  return src_size;
}

// Callback from VC to let us know how much is currently buffered.
void ViceSound::AmountBufferedBytes(unsigned nBytes) {
  bytes_buffered = nBytes;
  raspi_vchiq_coda = nBytes;
}

// Call from vice to ask us how much space is left in our buffer.


unsigned ViceSound::BufferSpaceSamples() {
  int left = FRAG_SIZE * NUM_FRAGS -
             bytes_buffered / (BYTES_PER_SAMPLE * VCHIQ_CANALI);
  return left < 0 ? 0 : left;
}
