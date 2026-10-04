//
// viceoptions.cpp
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

#include "viceoptions.h"

#include <stdlib.h>
#include <string.h>
#include <algorithm>

#include <circle/logger.h>
#include <circle/machineinfo.h>
#include <circle/sysconfig.h>
#include <circle/util.h>

extern "C" {
#include "third_party/common/circle.h"
}

#define INVALID_VALUE ((unsigned)-1)

ViceOptions *ViceOptions::s_pThis = 0;

ViceOptions::ViceOptions(void)
    : m_nMachineTiming(MACHINE_TIMING_PAL_HDMI),
      m_bDemoEnabled(false), m_bSerialEnabled(false),
      m_bGPIOOutputsEnabled(false), m_nCyclesPerSecond(0),
      m_audioOut(VCHIQSoundDestinationAuto), m_bForceVCHIQ(false),
      m_bDPIEnabled(false),
      m_nFramebufferWidth(0), m_nFramebufferHeight(0),
      m_nFramebufferDepth(16),
      m_bPi4KmsEnabled(false), m_bPi5KmsEnabled(false),
      m_bV3DCrtEnabled(false),
      m_bV3DCrtCore3(true),
      m_bSidCore2(true),
      m_bSidCore3(true),
      m_bV3DCrtFragmentProbeWaitVblank(true),
      m_bV3DCrtScanlineWeightOverride(false),
      m_bV3DCrtScanlineGapBrightnessOverride(false),
      m_v3dCrtScanlineWeight(0.0f), m_v3dCrtScanlineGapBrightness(0.0f),
      m_nHdmiGroup(0), m_nHdmiMode(0),
      m_scaling_param_fbw{0,0}, m_scaling_param_fbh{0,0},
      m_scaling_param_sx{0,0}, m_scaling_param_sy{0,0},
      m_raster_skip(false), m_raster_skip2(false),
      m_fb_size_w(0), m_fb_size_h(0), m_fb_display(0), m_fb_pages(2), m_bDoppioHdmi(false), m_bViceDopoUsb(false),
      m_bComposito(false), m_bCompositoHvs(true) {
  s_pThis = this;


  m_pi5KmsTimings[0] = '\0';
  m_pi5KmsMode[0] = '\0';
  strcpy(m_v3dCrtShader, "off");
  strcpy(m_v3dCrtTest, "off");
  strcpy(m_v3dCrtFragmentPackage, "default");
  strcpy(m_v3dCrtRenderResolution, "source");

  CBcmPropertyTags Tags;
  if (!Tags.GetTag(PROPTAG_GET_COMMAND_LINE, &m_TagCommandLine,
                   sizeof m_TagCommandLine)) {
    return;
  }

  if (m_TagCommandLine.Tag.nValueLength >= sizeof m_TagCommandLine.String) {
    return;
  }
  m_TagCommandLine.String[m_TagCommandLine.Tag.nValueLength] = '\0';

  m_pOptions = (char *)m_TagCommandLine.String;

  // Set the default volume we mount for fatfs
  m_disk_partition = 0; // this tells fatfs 'auto'
  strcpy(m_disk_volume, "SD");

  char *pOption;
  while ((pOption = GetToken()) != 0) {
    char *pValue = GetOptionValue(pOption);

    if (!pValue) continue;

    if (strcmp(pOption, "machine_timing") == 0) {
      if (strcmp(pValue, "ntsc") == 0 || strcmp(pValue, "ntsc-hdmi") == 0) {
        m_nMachineTiming = MACHINE_TIMING_NTSC_HDMI;
      } else if (strcmp(pValue, "ntsc-dpi") == 0) {
        m_nMachineTiming = MACHINE_TIMING_NTSC_DPI;
      } else if (strcmp(pValue, "ntsc-composite") == 0) {
        m_nMachineTiming = MACHINE_TIMING_NTSC_COMPOSITE;
      } else if (strcmp(pValue, "ntsc-custom") == 0) {
        m_nMachineTiming = MACHINE_TIMING_NTSC_CUSTOM_HDMI;
      } else if (strcmp(pValue, "pal") == 0 || strcmp(pValue, "pal-hdmi") == 0) {
        m_nMachineTiming = MACHINE_TIMING_PAL_HDMI;
      } else if (strcmp(pValue, "pal-dpi") == 0) {
        m_nMachineTiming = MACHINE_TIMING_PAL_DPI;
      } else if (strcmp(pValue, "pal-composite") == 0) {
        m_nMachineTiming = MACHINE_TIMING_PAL_COMPOSITE;
      } else if (strcmp(pValue, "pal-custom") == 0) {
        m_nMachineTiming = MACHINE_TIMING_PAL_CUSTOM_HDMI;
      }
    } else if (strcmp(pOption, "enable_demo") == 0) {
      if (strcmp(pValue,"true") == 0 || strcmp(pValue, "1") == 0) {
        m_bDemoEnabled = true;
      } else {
        m_bDemoEnabled = false;
      }
    } else if (strcmp(pOption, "enable_serial") == 0) {
      if (strcmp(pValue,"true") == 0 || strcmp(pValue, "1") == 0) {
        m_bSerialEnabled = true;
      } else {
        m_bSerialEnabled = false;
      }
    } else if (strcmp(pOption, "enable_gpio_outputs") == 0) {
      // Unless this is true, OUTPUT HIGH should not be allowed on any pin.
      if (strcmp(pValue,"true") == 0 || strcmp(pValue, "1") == 0) {
        m_bGPIOOutputsEnabled = true;
      } else {
        m_bGPIOOutputsEnabled = false;
      }
    } else if (strcmp(pOption, "disk_partition") == 0) {
      m_disk_partition = atoi(pValue);
      if (m_disk_partition < 0)
        m_disk_partition = 0;
    } else if (strcmp(pOption, "cycles_per_refresh") == 0 || strcmp(pOption, "cycles_per_second") == 0) {
      // This was named incorrectly in earlier versions. Keeping the old bad name working.
      m_nCyclesPerSecond = atol(pValue);
    } else if (strcmp(pOption, "audio_out") == 0) {
      if (strcmp(pValue, "hdmi") == 0 || strcmp(pValue, "ntsc-hdmi") == 0) {
        m_audioOut = VCHIQSoundDestinationHDMI;
      } else if (strcmp(pValue, "analog") == 0) {
        m_audioOut = VCHIQSoundDestinationHeadphones;
      } else if (strcmp(pValue, "auto") == 0) {
        m_audioOut = VCHIQSoundDestinationAuto;
      } else if (strcmp(pValue, "vchiq") == 0) {

        m_audioOut = VCHIQSoundDestinationAuto;
        m_bForceVCHIQ = true;
      } else if (strcmp(pValue, "usb") == 0) {


        m_audioOut = VCHIQSoundDestinationUSB;
      }
    } else if (strcmp(pOption, "audio_vchiq") == 0) {



      m_bForceVCHIQ = strcmp(pValue, "1") == 0 || strcmp(pValue, "true") == 0;
    } else if (strcmp(pOption, "fb_size") == 0) {


      int w = 0, h = 0;
      const char *p = pValue;
      while (*p >= '0' && *p <= '9') { w = w * 10 + (*p++ - '0'); }
      if (*p == 'x' || *p == 'X') {
        p++;
        while (*p >= '0' && *p <= '9') { h = h * 10 + (*p++ - '0'); }
      }
      if (w >= 64 && w <= 2048 && h >= 64 && h <= 2048) {
        m_fb_size_w = w & ~1;
        m_fb_size_h = h & ~1;
      }
    } else if (strcmp(pOption, "fb_display") == 0) {
      int d = atoi(pValue);
      if (d >= 0 && d < 4) {
        m_fb_display = d;
      }
    } else if (strcmp(pOption, "fb_pages") == 0) {
      m_fb_pages = atoi(pValue) == 1 ? 1 : 2;
    } else if (strcmp(pOption, "enable_dpi") == 0) {
      if (strcmp(pValue, "true") == 0 || strcmp(pValue, "1") == 0) {
        m_bDPIEnabled = true;
      } else {
        m_bDPIEnabled = false;
      }
    } else if (strcmp(pOption, "vice_dopo_usb") == 0) {



      m_bViceDopoUsb = strcmp(pValue, "1") == 0 || strcmp(pValue, "true") == 0;
    } else if (strcmp(pOption, "doppio_hdmi") == 0) {

      m_bDoppioHdmi = strcmp(pValue, "1") == 0 || strcmp(pValue, "true") == 0;
    } else if (strcmp(pOption, "composito") == 0) {

      m_bComposito = strcmp(pValue, "1") == 0 || strcmp(pValue, "true") == 0;
    } else if (strcmp(pOption, "composito_hvs") == 0) {

      m_bCompositoHvs = !(strcmp(pValue, "0") == 0 ||
                          strcmp(pValue, "false") == 0);


    } else if (strcmp(pOption, "pi4kms") == 0) {
      if (strcmp(pValue, "true") == 0 || strcmp(pValue, "1") == 0) {
        m_bPi4KmsEnabled = true;
      } else {
        m_bPi4KmsEnabled = false;
      }
    } else if (strcmp(pOption, "pi5kms") == 0) {
      if (strcmp(pValue, "true") == 0 || strcmp(pValue, "1") == 0) {
        m_bPi5KmsEnabled = true;
      } else {
        m_bPi5KmsEnabled = false;
      }
    } else if (strcmp(pOption, "v3dcrt") == 0 ||
               strcmp(pOption, "pi5v3d") == 0) {
      if (strcmp(pValue, "true") == 0 || strcmp(pValue, "1") == 0) {
        m_bV3DCrtEnabled = true;
      } else {
        m_bV3DCrtEnabled = false;
      }
    } else if (strcmp(pOption, "v3dcrt_core3") == 0) {


      m_bV3DCrtCore3 = !(strcmp(pValue, "false") == 0 ||
                         strcmp(pValue, "0") == 0);
    } else if (strcmp(pOption, "sid_core2") == 0) {



      m_bSidCore2 = !(strcmp(pValue, "false") == 0 ||
                      strcmp(pValue, "0") == 0);
    } else if (strcmp(pOption, "sid_core3") == 0) {


      m_bSidCore3 = !(strcmp(pValue, "false") == 0 ||
                      strcmp(pValue, "0") == 0);
    } else if (strcmp(pOption, "v3dcrt_shader") == 0 ||
               strcmp(pOption, "pi5v3d_shader") == 0) {
      strncpy(m_v3dCrtShader, pValue, sizeof m_v3dCrtShader - 1);
      m_v3dCrtShader[sizeof m_v3dCrtShader - 1] = '\0';
    } else if (strcmp(pOption, "v3dcrt_test") == 0 ||
               strcmp(pOption, "pi5v3d_test") == 0) {
      strncpy(m_v3dCrtTest, pValue, sizeof m_v3dCrtTest - 1);
      m_v3dCrtTest[sizeof m_v3dCrtTest - 1] = '\0';
#ifdef BMC64_DEBUG_PROFILE
    } else if (strcmp(pOption, "v3dcrt_fragment_package") == 0 ||
               strcmp(pOption, "pi5v3d_fragment_package") == 0) {
      strncpy(m_v3dCrtFragmentPackage, pValue,
              sizeof m_v3dCrtFragmentPackage - 1);
      m_v3dCrtFragmentPackage[sizeof m_v3dCrtFragmentPackage - 1] = '\0';
#endif
    } else if (strcmp(pOption, "v3dcrt_render_resolution") == 0 ||
               strcmp(pOption, "pi5v3d_render_resolution") == 0) {
      strncpy(m_v3dCrtRenderResolution, pValue,
              sizeof m_v3dCrtRenderResolution - 1);
      m_v3dCrtRenderResolution[sizeof m_v3dCrtRenderResolution - 1] = '\0';
    } else if (strcmp(pOption, "v3dcrt_fragment_probe_wait_vblank") == 0 ||
               strcmp(pOption, "v3dcrt_fragment_probe_vblank") == 0 ||
               strcmp(pOption, "pi5v3d_fragment_probe_wait_vblank") == 0 ||
               strcmp(pOption, "pi5v3d_fragment_probe_vblank") == 0) {
      if (strcmp(pValue, "true") == 0 || strcmp(pValue, "1") == 0) {
        m_bV3DCrtFragmentProbeWaitVblank = true;
      } else {
        m_bV3DCrtFragmentProbeWaitVblank = false;
      }
    } else if (strcmp(pOption, "v3dcrt_scanline_weight") == 0 ||
               strcmp(pOption, "pi5v3d_scanline_weight") == 0) {
      float value;
      if (GetFloat(pValue, &value) && value >= 0.0f && value <= 15.0f) {
        m_bV3DCrtScanlineWeightOverride = true;
        m_v3dCrtScanlineWeight = value;
      }
    } else if (strcmp(pOption, "v3dcrt_scanline_gap_brightness") == 0 ||
               strcmp(pOption, "v3dcrt_scanline_gap") == 0 ||
               strcmp(pOption, "pi5v3d_scanline_gap_brightness") == 0 ||
               strcmp(pOption, "pi5v3d_scanline_gap") == 0) {
      float value;
      if (GetFloat(pValue, &value) && value >= 0.0f && value <= 1.0f) {
        m_bV3DCrtScanlineGapBrightnessOverride = true;
        m_v3dCrtScanlineGapBrightness = value;
      }
    } else if (strcmp(pOption, "hdmi_group") == 0) {
      unsigned value = GetDecimal(pValue);
      if (value != INVALID_VALUE) {
        m_nHdmiGroup = value;
      }
    } else if (strcmp(pOption, "hdmi_mode") == 0) {
      unsigned value = GetDecimal(pValue);
      if (value != INVALID_VALUE) {
        m_nHdmiMode = value;
      }
    } else if (strcmp(pOption, "pi5kms_timings") == 0) {
      strncpy(m_pi5KmsTimings, pValue, sizeof m_pi5KmsTimings - 1);
      m_pi5KmsTimings[sizeof m_pi5KmsTimings - 1] = '\0';
    } else if (strcmp(pOption, "pi5kms_mode") == 0) {
      strncpy(m_pi5KmsMode, pValue, sizeof m_pi5KmsMode - 1);
      m_pi5KmsMode[sizeof m_pi5KmsMode - 1] = '\0';
    } else if (strcmp(pOption, "framebuffer_width") == 0 ||
               strcmp(pOption, "pi5_framebuffer_width") == 0) {
      unsigned value = GetDecimal(pValue);
      if (value != INVALID_VALUE) {
        m_nFramebufferWidth = value;
      }
    } else if (strcmp(pOption, "framebuffer_height") == 0 ||
               strcmp(pOption, "pi5_framebuffer_height") == 0) {
      unsigned value = GetDecimal(pValue);
      if (value != INVALID_VALUE) {
        m_nFramebufferHeight = value;
      }
    } else if (strcmp(pOption, "framebuffer_depth") == 0 ||
               strcmp(pOption, "pi5_framebuffer_depth") == 0) {
      unsigned value = GetDecimal(pValue);
      if (value == 16 || value == 32) {
        m_nFramebufferDepth = value;
      }
    } else if (strcmp(pOption, "scaling_params") == 0 ||
               strcmp(pOption, "scaling_params2") == 0) {
      int num = 0;
      if (strcmp(pOption, "scaling_params2") == 0) {
         num = 1;
      }
      char* fbw_s = strtok(pValue, ",");
      if (!fbw_s) continue;
      char* fbh_s = strtok(NULL, ",");
      if (!fbh_s) continue;
      char* sx_s = strtok(NULL, ",");
      if (!sx_s) continue;
      char* sy_s = strtok(NULL, ",");
      if (!sy_s) continue;

      m_scaling_param_fbw[num] = atoi(fbw_s);
      m_scaling_param_fbh[num] = atoi(fbh_s);
      m_scaling_param_sx[num] = atoi(sx_s);
      m_scaling_param_sy[num] = atoi(sy_s);
    } else if (strcmp(pOption, "raster_skip") == 0) {
      if (strcmp(pValue, "true") == 0 || strcmp(pValue, "1") == 0) {
        m_raster_skip = true;
      } else {
        m_raster_skip = false;
      }
    } else if (strcmp(pOption, "raster_skip2") == 0) {
      if (strcmp(pValue, "true") == 0 || strcmp(pValue, "1") == 0) {
        m_raster_skip2 = true;
      } else {
        m_raster_skip2 = false;
      }
    }
  }

  // When DPI is enabled, use the DPI versions of constants. Behavior
  // is identical. It's just used for display purposes.
  if (m_nMachineTiming == MACHINE_TIMING_PAL_CUSTOM_HDMI &&
      m_bDPIEnabled) {
     m_nMachineTiming = MACHINE_TIMING_PAL_CUSTOM_DPI;
  } else if (m_nMachineTiming == MACHINE_TIMING_NTSC_CUSTOM_HDMI &&
      m_bDPIEnabled) {
     m_nMachineTiming = MACHINE_TIMING_NTSC_CUSTOM_DPI;
  }

  if (m_nMachineTiming == MACHINE_TIMING_PAL_CUSTOM_HDMI &&
      m_nCyclesPerSecond == 0) {
    m_nMachineTiming = MACHINE_TIMING_PAL_HDMI;
  } else if (m_nMachineTiming == MACHINE_TIMING_NTSC_CUSTOM_HDMI &&
             m_nCyclesPerSecond == 0) {
    m_nMachineTiming = MACHINE_TIMING_NTSC_HDMI;
  } else if (m_nMachineTiming == MACHINE_TIMING_NTSC_CUSTOM_DPI &&
             m_nCyclesPerSecond == 0) {
    m_nMachineTiming = MACHINE_TIMING_NTSC_DPI;
  } else if (m_nMachineTiming == MACHINE_TIMING_NTSC_CUSTOM_DPI &&
             m_nCyclesPerSecond == 0) {
    m_nMachineTiming = MACHINE_TIMING_NTSC_DPI;
  }

  if (m_bDPIEnabled) {
     m_bSerialEnabled = false;
  }
}

ViceOptions::~ViceOptions(void) { s_pThis = 0; }

unsigned ViceOptions::GetMachineTiming(void) const { return m_nMachineTiming; }

bool ViceOptions::DemoEnabled(void) const { return m_bDemoEnabled; }

bool ViceOptions::SerialEnabled(void) const { return m_bSerialEnabled; }

bool ViceOptions::GPIOOutputsEnabled(void) const { return m_bGPIOOutputsEnabled; }

bool ViceOptions::DPIEnabled(void) const { return m_bDPIEnabled; }

int ViceOptions::GetDiskPartition(void) const { return m_disk_partition; }

void ViceOptions::GetScalingParams(int display, int *fbw, int *fbh, int *sx, int *sy) const {
  if (display >=0 && display < 2) {
     *fbw = m_scaling_param_fbw[display];
     *fbh = m_scaling_param_fbh[display];
     *sx = m_scaling_param_sx[display];
     *sy = m_scaling_param_sy[display];
  }
}

void ViceOptions::GetFBSize(int *w, int *h) const {
  *w = m_fb_size_w;
  *h = m_fb_size_h;
}

bool ViceOptions::GetRasterSkip(void) const { return m_raster_skip; }
bool ViceOptions::GetRasterSkip2(void) const { return m_raster_skip2; }

const char *ViceOptions::GetDiskVolume(void) const { return m_disk_volume; }

unsigned long ViceOptions::GetCyclesPerSecond(void) const {
  return m_nCyclesPerSecond;
}



extern "C" int circle_uscita_audio(void) {
  ViceOptions *o = ViceOptions::Get();
  if (o == nullptr) {
    return 0;
  }
  switch (o->GetAudioOut()) {
    case VCHIQSoundDestinationHeadphones: return 1;
    case VCHIQSoundDestinationHDMI:       return 2;
    case VCHIQSoundDestinationUSB:        return 3;
    default:                              return 0;
  }
}


unsigned ViceOptions::GetFramebufferWidth(void) const {
  return m_nFramebufferWidth;
}

unsigned ViceOptions::GetFramebufferHeight(void) const {
  return m_nFramebufferHeight;
}

unsigned ViceOptions::GetFramebufferDepth(void) const {
  return m_nFramebufferDepth;
}

bool ViceOptions::Pi4KmsEnabled(void) const { return m_bPi4KmsEnabled; }

bool ViceOptions::Pi5KmsEnabled(void) const { return m_bPi5KmsEnabled; }

bool ViceOptions::DoppioHdmi(void) const { return m_bDoppioHdmi; }

bool ViceOptions::Composito(void) const {
  return m_bComposito && CMachineInfo::Get() != nullptr &&
         CMachineInfo::Get()->GetMachineModel() == MachineModel4B;
}
bool ViceOptions::CompositoHvs(void) const { return m_bCompositoHvs; }
bool ViceOptions::ViceDopoUsb(void) const { return m_bViceDopoUsb; }

bool ViceOptions::V3DCrtEnabled(void) const { return m_bV3DCrtEnabled; }

bool ViceOptions::V3DCrtCore3(void) const { return m_bV3DCrtCore3; }

bool ViceOptions::SidCore2(void) const { return m_bSidCore2; }

bool ViceOptions::SidCore3(void) const { return m_bSidCore3; }

const char *ViceOptions::GetV3DCrtShader(void) const {
  return m_v3dCrtShader;
}

const char *ViceOptions::GetV3DCrtTest(void) const {
  return m_v3dCrtTest;
}

const char *ViceOptions::GetV3DCrtFragmentPackage(void) const {
  return m_v3dCrtFragmentPackage;
}

const char *ViceOptions::GetV3DCrtRenderResolution(void) const {
  return m_v3dCrtRenderResolution;
}

bool ViceOptions::GetV3DCrtFragmentProbeWaitVblank(void) const {
  return m_bV3DCrtFragmentProbeWaitVblank;
}

bool ViceOptions::GetV3DCrtScanlineWeight(float *value) const {
  if (value != 0) {
    *value = m_v3dCrtScanlineWeight;
  }
  return m_bV3DCrtScanlineWeightOverride;
}

bool ViceOptions::GetV3DCrtScanlineGapBrightness(float *value) const {
  if (value != 0) {
    *value = m_v3dCrtScanlineGapBrightness;
  }
  return m_bV3DCrtScanlineGapBrightnessOverride;
}

bool ViceOptions::Pi5V3DEnabled(void) const { return V3DCrtEnabled(); }

const char *ViceOptions::GetPi5V3DShader(void) const {
  return GetV3DCrtShader();
}

const char *ViceOptions::GetPi5V3DTest(void) const {
  return GetV3DCrtTest();
}

unsigned ViceOptions::GetHdmiGroup(void) const { return m_nHdmiGroup; }

unsigned ViceOptions::GetHdmiMode(void) const { return m_nHdmiMode; }

const char *ViceOptions::GetPi5KmsTimings(void) const {
  return m_pi5KmsTimings;
}

const char *ViceOptions::GetPi5KmsMode(void) const {
  return m_pi5KmsMode;
}

TVCHIQSoundDestination ViceOptions::GetAudioOut(void) const {


  if (Composito() && (m_audioOut == VCHIQSoundDestinationHDMI ||
                      m_audioOut == VCHIQSoundDestinationAuto)) {
    return VCHIQSoundDestinationHeadphones;
  }
  return m_audioOut;
}

bool ViceOptions::AudioForceVCHIQ(void) const { return m_bForceVCHIQ; }

int ViceOptions::GetFBDisplay(void) const { return m_fb_display; }
int ViceOptions::GetFBPages(void) const { return m_fb_pages; }

ViceOptions *ViceOptions::Get(void) { return s_pThis; }

char *ViceOptions::GetToken(void) {
  while (*m_pOptions != '\0') {
    if (*m_pOptions != ' ') {
      break;
    }

    m_pOptions++;
  }

  if (*m_pOptions == '\0') {
    return 0;
  }

  char *pToken = m_pOptions;

  while (*m_pOptions != '\0') {
    if (*m_pOptions == ' ') {
      *m_pOptions++ = '\0';

      break;
    }

    m_pOptions++;
  }

  return pToken;
}

char *ViceOptions::GetOptionValue(char *pOption) {
  while (*pOption != '\0') {
    if (*pOption == '=') {
      break;
    }

    pOption++;
  }

  if (*pOption == '\0') {
    return 0;
  }

  *pOption++ = '\0';

  return pOption;
}

unsigned ViceOptions::GetDecimal(char *pString) {
  if (pString == 0 || *pString == '\0') {
    return INVALID_VALUE;
  }

  unsigned nResult = 0;

  char chChar;
  while ((chChar = *pString++) != '\0') {
    if (!('0' <= chChar && chChar <= '9')) {
      return INVALID_VALUE;
    }

    unsigned nPrevResult = nResult;

    nResult = nResult * 10 + (chChar - '0');
    if (nResult < nPrevResult || nResult == INVALID_VALUE) {
      return INVALID_VALUE;
    }
  }

  return nResult;
}


bool ViceOptions::GetFloat(char *pString, float *pValue) {
  if (pString == 0 || pValue == 0 || *pString == '\0') {
    return false;
  }

  unsigned whole = 0;
  unsigned fraction = 0;
  unsigned divisor = 1;
  bool after_decimal = false;
  bool seen_digit = false;

  char chChar;
  while ((chChar = *pString++) != '\0') {
    if (chChar == '.') {
      if (after_decimal) {
        return false;
      }
      after_decimal = true;
      continue;
    }

    if (!('0' <= chChar && chChar <= '9')) {
      return false;
    }

    seen_digit = true;
    const unsigned digit = (unsigned)(chChar - '0');
    if (after_decimal) {
      if (divisor <= 1000000U) {
        fraction = fraction * 10 + digit;
        divisor *= 10;
      }
    } else {
      if (whole > 1000000U) {
        return false;
      }
      whole = whole * 10 + digit;
    }
  }

  if (!seen_digit) {
    return false;
  }

  *pValue = (float)whole;
  if (divisor > 1) {
    *pValue += (float)fraction / (float)divisor;
  }
  return true;
}
