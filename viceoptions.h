//
// viceoptions.h
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

#ifndef _vice_options_h
#define _vice_options_h

#include "soundtypes.h"
#include <circle/bcmpropertytags.h>
#include <circle/cputhrottle.h>

#define VOLUME_NAME_LEN 16

#define PI5KMS_MODE_LEN 32
#define PI5KMS_TIMINGS_LEN 192
#define V3DCRT_SHADER_LEN 32
#define V3DCRT_TEST_LEN 32
#define V3DCRT_FRAGMENT_PACKAGE_LEN 32
#define V3DCRT_RENDER_RESOLUTION_LEN 16

class ViceOptions {
public:
  ViceOptions(void);
  ~ViceOptions(void);

  unsigned GetMachineTiming(void) const;
  bool DemoEnabled(void) const;
  bool SerialEnabled(void) const;
  bool GPIOOutputsEnabled(void) const;
  int GetDiskPartition(void) const;
  const char *GetDiskVolume(void) const;
  unsigned long GetCyclesPerSecond(void) const;
  TVCHIQSoundDestination GetAudioOut(void) const;



  bool AudioForceVCHIQ(void) const;
  bool DPIEnabled(void) const;


  unsigned GetFramebufferWidth(void) const;
  unsigned GetFramebufferHeight(void) const;
  unsigned GetFramebufferDepth(void) const;
  bool Pi4KmsEnabled(void) const;
  bool Pi5KmsEnabled(void) const;
  bool V3DCrtEnabled(void) const;
  bool V3DCrtCore3(void) const;
  bool SidCore2(void) const;
  const char *GetV3DCrtShader(void) const;
  const char *GetV3DCrtTest(void) const;
  // Diagnostic package overrides are parsed only by debug builds.
  const char *GetV3DCrtFragmentPackage(void) const;
  const char *GetV3DCrtRenderResolution(void) const;
  bool GetV3DCrtFragmentProbeWaitVblank(void) const;
  bool GetV3DCrtScanlineWeight(float *value) const;
  bool GetV3DCrtScanlineGapBrightness(float *value) const;
  bool Pi5V3DEnabled(void) const;
  const char *GetPi5V3DShader(void) const;
  const char *GetPi5V3DTest(void) const;
  unsigned GetHdmiGroup(void) const;
  unsigned GetHdmiMode(void) const;
  const char *GetPi5KmsTimings(void) const;
  const char *GetPi5KmsMode(void) const;
  void GetScalingParams(int display, int *fbw, int *fbh, int *sx, int *sy) const;

  void GetFBSize(int *w, int *h) const;
  int GetFBDisplay(void) const;
  int GetFBPages(void) const;




  bool DoppioHdmi(void) const;




  bool ViceDopoUsb(void) const;
  bool GetRasterSkip(void) const;
  bool GetRasterSkip2(void) const;

  static ViceOptions *Get(void);

private:
  char *
  GetToken(void); // returns next "option=value" pair, 0 if nothing follows

  static char *GetOptionValue(
      char *pOption); // returns value and terminates option with '\0'

  static unsigned
  GetDecimal(char *pString); // returns decimal value, -1 on error

  static bool GetFloat(char *pString, float *pValue);

private:
  TPropertyTagCommandLine m_TagCommandLine;
  char *m_pOptions;

  unsigned m_nMachineTiming;
  bool m_bDemoEnabled;
  bool m_bSerialEnabled;
  bool m_bGPIOOutputsEnabled;
  int m_disk_partition;
  char m_disk_volume[VOLUME_NAME_LEN];
  unsigned long m_nCyclesPerSecond;
  TVCHIQSoundDestination m_audioOut;
  bool m_bForceVCHIQ;
  bool m_bDPIEnabled;
  unsigned m_nFramebufferWidth;
  unsigned m_nFramebufferHeight;
  unsigned m_nFramebufferDepth;
  bool m_bPi4KmsEnabled;
  bool m_bPi5KmsEnabled;
  bool m_bV3DCrtEnabled;
  bool m_bV3DCrtCore3;
  bool m_bSidCore2;
  char m_v3dCrtShader[V3DCRT_SHADER_LEN];
  char m_v3dCrtTest[V3DCRT_TEST_LEN];
  char m_v3dCrtFragmentPackage[V3DCRT_FRAGMENT_PACKAGE_LEN];
  char m_v3dCrtRenderResolution[V3DCRT_RENDER_RESOLUTION_LEN];
  bool m_bV3DCrtFragmentProbeWaitVblank;
  bool m_bV3DCrtScanlineWeightOverride;
  bool m_bV3DCrtScanlineGapBrightnessOverride;
  float m_v3dCrtScanlineWeight;
  float m_v3dCrtScanlineGapBrightness;
  unsigned m_nHdmiGroup;
  unsigned m_nHdmiMode;
  char m_pi5KmsTimings[PI5KMS_TIMINGS_LEN];
  char m_pi5KmsMode[PI5KMS_MODE_LEN];
  int m_scaling_param_fbw[2];
  int m_scaling_param_fbh[2];
  int m_scaling_param_sx[2];
  int m_scaling_param_sy[2];
  bool m_raster_skip;
  bool m_raster_skip2; // for VDC
  int m_fb_size_w;
  int m_fb_size_h;
  int m_fb_display;
  int m_fb_pages;
  bool m_bDoppioHdmi;
  bool m_bViceDopoUsb;

  static ViceOptions *s_pThis;
};

#endif
