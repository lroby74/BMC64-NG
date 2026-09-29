//
// viceemulatorcore.cpp
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

#include <unistd.h>
#include "viceemulatorcore.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "defs.h"

extern "C" {
#include "third_party/vice-3.3/src/main.h"
#include "third_party/common/semaphore.h"

extern void circle_kernel_core_init_complete(int core);
void passo_nota(const char *cosa);


extern "C" {
  extern uint32_t fbl_job;
  extern uint32_t fbl_done;
  void fbl_worker_ready(void);
  void fbl_worker_run(void);
}

}

#include "third_party/vice-3.3/src/sid/sid.h"
#include "third_party/vice-3.3/src/resid/sid.h"
#include "third_party/vice-3.3/src/resid/filter.h"
#include "sidworker.h"

ViceEmulatorCore::ViceEmulatorCore(CMemorySystem *pMemorySystem,
                                   int cyclesPerSecond) :
#ifdef ARM_ALLOW_MULTI_CORE
       CMultiCoreSupport(pMemorySystem),
#endif
       launch_(false), cyclesPerSecond_(cyclesPerSecond) {








  passBandFreq_ = 0;
}

ViceEmulatorCore::~ViceEmulatorCore(void) {}

void ViceEmulatorCore::RunMainVice(bool wait) {
  if (wait) {
     printf("Core waiting for launch\n");
     bool waiting = true;
     while (waiting) {
       m_Lock.Acquire();
       if (launch_)
         waiting = false;
       m_Lock.Release();
     }
  }

  // Call Vice's main_program










  unlink("/vice-prima.log");
  link("/vice.log", "/vice-prima.log");
  unlink("/BLOCCO-PRIMA.TXT");
  link("/BLOCCO.TXT", "/BLOCCO-PRIMA.TXT");



  passo_nota("il VICE parte");
  printf("Starting emulator main loop\n");

#if defined(RASPI_SCPU64)




  int argc = 13;
  char *argv[] = {
      (char *)"vice", timing_option_, (char *)"-sounddev", (char *)"raspi",
      // Unless we disable the video cache, vsync is messed up
      (char *)"+VICIIvcache",
      (char *)"+drive8truedrive", (char *)"+drive9truedrive",
      (char *)"+drive10truedrive", (char *)"+drive11truedrive",
      (char *)"-trapdevice8", (char *)"-trapdevice9",
      (char *)"-trapdevice10", (char *)"-trapdevice11",
  };
#elif defined(RASPI_C64)
  int argc = 5;
  char *argv[] = {
      (char *)"vice", timing_option_, (char *)"-sounddev", (char *)"raspi",
      // Unless we disable the video cache, vsync is messed up
      (char *)"+VICIIvcache",
  };
#elif defined(RASPI_C128)
  int argc = 8;
  char *argv[] = {
      (char *)"vice", timing_option_, (char *)"-sounddev", (char *)"raspi",
      (char *)"-soundoutput", (char *)"1",
      // Unless we disable the video cache, vsync is messed up
      (char *)"+VICIIvcache",
      (char *)"+VDCvcache",
  };
#elif defined(RASPI_VIC20)
  int argc = 7;
  char *argv[] = {
      (char *)"vice", timing_option_, (char *)"-sounddev", (char *)"raspi",
      (char *)"-soundoutput", (char *)"1",
      // Unless we disable the video cache, vsync is messed up
      (char *)"+VICvcache",
  };
#elif defined(RASPI_PLUS4)
  int argc = 7;
  char *argv[] = {
      (char *)"vice", timing_option_, (char *)"-sounddev", (char *)"raspi",
      (char *)"-soundoutput", (char *)"1",
      // Unless we disable the video cache, vsync is messed up
      (char *)"+TEDvcache",
  };
#elif defined(RASPI_PET)
  int argc = 7;
  char *argv[] = {
      (char *)"vice", timing_option_, (char *)"-sounddev", (char *)"raspi",
      (char *)"-soundoutput", (char *)"1",
      (char *)"+CRTCvcache",
  };
#else
#error "RASPI_[model] NOT DEFINED"
#endif
  emu_machine_init(m_options->GetRasterSkip(), m_options->GetRasterSkip2());
  main_program(argc, argv);
  emu_exit();
}

// Initializing the filters for each SID model takes quite a bit.
// This method instantiates a modified Filter object in ReSid.  The
// modified version lets us initialize both SIDs in parallel on seperate
// cores.  This saves a lot of boot time since when VICE eventually gets
// around to initializing the filters, they are already done and opening
// ReSid is very quick at that point.  See resid/filter.cc for
// modifications done for BMC64.






void ViceEmulatorCore::ComputeResidFilter(int model) {
  if (model != 0) {
    return;
  }
  reSID::Filter f;
}

// In addition to initializing the filters in parellel during boot, we
// compute the resampling tables for the two resampling methods.
void ViceEmulatorCore::Run(unsigned nCore) {
  assert(nCore > 0);
  switch (nCore) {
  case 1:
    RunMainVice(true);
    break;
  case 2:
    // Core 2 will initialize 6581 filter data. Then partition 1
    // of the resampling tables. Then sleep.
#ifdef ARM_ALLOW_MULTI_CORE
    ComputeResidFilter(0);
    circle_kernel_core_init_complete(2);
#endif
    break;
  case 3:
    // Core 3 will initialize 8580 filter data. Then partition 2
    // of the resampling tables. Then sleep.
#ifdef ARM_ALLOW_MULTI_CORE
    ComputeResidFilter(1);
    circle_kernel_core_init_complete(3);
#endif
    break;
  }

  if (nCore == 2) {








     if (m_options == NULL || m_options->SidCore2()) {
        bmx_sid_worker_run();
     } else {
        printf("Core 2: sid_core2=0, i SID restano sul core 1\n");
     }
  }

  if (nCore == 3) {




     fbl_worker_ready();
     while (true) {
        sem_dec(&fbl_job);
        fbl_worker_run();
        sem_inc(&fbl_done);
        bmc_giri[3]++;
     }
  }

#ifdef ARM_ALLOW_MULTI_CORE
  printf("Core %d idle\n", nCore);


#if defined(__aarch64__)
  asm("dsb sy\n\t"
      "1: wfi\n\t"
      "b 1b\n\t");
#else
  asm("dsb\n\t"
      "1: wfi\n\t"
      "b 1b\n\t");
#endif
#endif
}

bool ViceEmulatorCore::Init(ViceOptions* options) {
  m_options = options;
  return Initialize();
}

void ViceEmulatorCore::LaunchEmulator(char *timing_option) {
  strncpy(timing_option_, timing_option, sizeof(timing_option_) - 1);
  timing_option_[sizeof(timing_option_) - 1] = '\0';
#ifdef ARM_ALLOW_MULTI_CORE
  m_Lock.Acquire();
  launch_ = true;
  m_Lock.Release();
#else
  RunMainVice(false);
#endif
}
