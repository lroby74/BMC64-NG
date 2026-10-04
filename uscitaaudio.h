
#ifndef _vice_uscita_audio_h
#define _vice_uscita_audio_h

#include "soundtypes.h"
#ifndef USCITA_AUDIO_BANCO
#include <circle/machineinfo.h>
#endif












static inline bool uscita_audio_ha_il_jack(TMachineModel modello) {
#if RASPPI >= 5
  (void) modello;
  return false;
#else
  return modello != MachineModel400 && modello != MachineModelCM4;
#endif
}

static inline TVCHIQSoundDestination uscita_audio_vera(
    TVCHIQSoundDestination chiesta, TMachineModel modello) {
  if (chiesta == VCHIQSoundDestinationHeadphones &&
      !uscita_audio_ha_il_jack(modello)) {
    return VCHIQSoundDestinationAuto;
  }
  return chiesta;
}

#endif
