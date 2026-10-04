// vice_network.cpp
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

#include "viceapp.h"
#include "vice_network.h"
#include "network_time_sync.h"
#include "third_party/common/circle.h"
#include "fbl.h"

#include <stddef.h>

static CNetSubSystem *network_subsystem;
static ViceStdioApp *stdio_app;
static circle_network_status_changed_handler_t *network_status_changed_handler;

static const int acia_network_addresses[] = CIRCLE_ACIA_NETWORK_ADDRESS_VALUES;

static int IsAciaNetworkAddress(int address) {
  for (int index = 0;
       index < (int)(sizeof(acia_network_addresses) /
                     sizeof(acia_network_addresses[0])); index++) {
    if (acia_network_addresses[index] == address) {
      return 1;
    }
  }
  return 0;
}

static int HasOnboardWLAN(TMachineModel machine_model) {
  switch (machine_model) {
  case MachineModelZeroW:
  case MachineModelZero2W:
  case MachineModel3B:
  case MachineModel3APlus:
  case MachineModel3BPlus:
  case MachineModel4B:
  case MachineModel400:
  case MachineModelCM4:
  case MachineModelCM4S:
  case MachineModel5:
  case MachineModel500:
  case MachineModelCM5:
    return 1;
  default:
    return 0;
  }
}

static int HasOnboardEthernet(TMachineModel machine_model) {
  switch (machine_model) {
  case MachineModel2B:
  case MachineModel3B:
  case MachineModel3BPlus:
  case MachineModel4B:
  case MachineModel400:
  case MachineModel5:
  case MachineModel500:
  case MachineModelCM5:
    return 1;
  default:
    return 0;
  }
}

static bool HasWifiFirmwareFile(const char *firmware_path,
                                const char *filename) {
  CString path;
  path.Format("%s%s", firmware_path, filename);

  FIL file;
  if (f_open(&file, (const char *)path, FA_READ) != FR_OK) {
    return false;
  }
  bool has_contents = f_size(&file) > 0;
  f_close(&file);
  return has_contents;
}

static bool HasWifiFirmware(const char *firmware_path) {
  const char *firmware_files[] = {
#if RASPPI >= 5
      "brcmfmac43455-sdio.raspberrypi,5-model-b.bin",
      "brcmfmac43455-sdio.raspberrypi,5-model-b.txt",
      "brcmfmac43455-sdio.raspberrypi,5-model-b.clm_blob",
#else
      "brcmfmac43430-sdio.bin",
      "brcmfmac43430-sdio.txt",
      "brcmfmac43430-sdio.clm_blob",
      "brcmfmac43436-sdio.bin",
      "brcmfmac43436-sdio.txt",
      "brcmfmac43436-sdio.clm_blob",
      "brcmfmac43436s-sdio.bin",
      "brcmfmac43436s-sdio.txt",
      "brcmfmac43455-sdio.bin",
      "brcmfmac43455-sdio.clm_blob",
      "brcmfmac43455-sdio.txt",
#endif
  };

  for (unsigned int index = 0;
       index < sizeof(firmware_files) / sizeof(firmware_files[0]); index++) {
    if (!HasWifiFirmwareFile(firmware_path, firmware_files[index])) {
      return false;
    }
  }
  return true;
}

struct wifi_bss_info {
  uint32_t version;
  uint32_t length;
  uint8_t bssid[6];
  uint16_t beacon_period;
  uint16_t capability;
  uint8_t ssid_length;
  uint8_t ssid[32];
  struct {
    uint32_t count;
    uint8_t rates[16];
  } rateset;
  uint16_t chanspec;
  uint16_t atim_window;
  uint8_t dtim_period;
  int16_t rssi;
  int8_t phy_noise;
  uint8_t n_cap;
  uint32_t nbss_cap;
  uint8_t ctl_ch;
  uint32_t reserved32[1];
  uint8_t flags;
  uint8_t reserved[3];
  uint8_t basic_mcs[16];
  uint16_t ie_offset;
  uint32_t ie_length;
  uint16_t snr;
};

struct wifi_scan_result {
  uint32_t buffer_length;
  uint32_t version;
  uint16_t sync_id;
  uint16_t bss_count;
  struct wifi_bss_info bss;
};

static const uint16_t WIFI_CAPABILITY_PRIVACY = 0x0010;

static unsigned int ParseWifiScanResult(
    const uint8_t *buffer, unsigned int result_length,
    struct wifi_access_point *access_points, unsigned int max_access_points,
    unsigned int count) {
  const unsigned int minimum_bss_length =
      offsetof(struct wifi_bss_info, rssi) + sizeof(int16_t);

  if (result_length < sizeof(struct wifi_scan_result)) {
    return count;
  }

  const struct wifi_scan_result *scan =
      (const struct wifi_scan_result *)buffer;
  const uint8_t *record = (const uint8_t *)&scan->bss;
  const uint8_t *end = buffer + result_length;
  for (unsigned int index = 0; index < scan->bss_count &&
       record + minimum_bss_length <= end; index++) {
    const struct wifi_bss_info *bss = (const struct wifi_bss_info *)record;
    if (bss->length < minimum_bss_length || record + bss->length > end) {
      break;
    }

    unsigned int ssid_length = bss->ssid_length;
    if (ssid_length > sizeof(bss->ssid)) {
      ssid_length = sizeof(bss->ssid);
    }
    if (ssid_length <= bss->length - offsetof(struct wifi_bss_info, ssid) &&
        ssid_length > 0) {
      unsigned int channel = bss->chanspec & 0xff;
      if (bss->length > offsetof(struct wifi_bss_info, ctl_ch) &&
          bss->ctl_ch != 0) {
        channel = bss->ctl_ch;
      }
      int mhz = 0;
      if (channel >= 1 && channel <= 13) {
        mhz = 2407 + 5 * (int)channel;
      } else if (channel == 14) {
        mhz = 2484;
      } else if (channel >= 32 && channel <= 177) {
        mhz = 5000 + 5 * (int)channel;
      }
      bool duplicate = false;
      for (unsigned int existing = 0; existing < count; existing++) {
        if (strlen(access_points[existing].ssid) == ssid_length &&
            memcmp(access_points[existing].ssid, bss->ssid,
                   ssid_length) == 0 &&
            access_points[existing].mhz == mhz) {
          duplicate = true;
          if (bss->rssi > access_points[existing].signal) {
            access_points[existing].signal = bss->rssi;
            access_points[existing].channel = (int)channel;
            access_points[existing].secure =
                (bss->capability & WIFI_CAPABILITY_PRIVACY) != 0;
          }
          break;
        }
      }
      if (!duplicate && count < max_access_points) {
        memcpy(access_points[count].ssid, bss->ssid, ssid_length);
        access_points[count].ssid[ssid_length] = '\0';
        access_points[count].signal = bss->rssi;
        access_points[count].secure =
            (bss->capability & WIFI_CAPABILITY_PRIVACY) != 0;
        access_points[count].mhz = mhz;
        access_points[count].channel = (int)channel;
        count++;
      }
    }
    record += bss->length;
  }
  return count;
}

static unsigned int CollectWifiScanResults(
    CBcm4343Device *wlan, struct wifi_access_point *access_points,
    unsigned int max_access_points, unsigned int count,
    unsigned int *result_messages) {
  uint8_t buffer[FRAME_BUFFER_SIZE];
  unsigned int result_length;
  while (wlan->ReceiveScanResult(buffer, &result_length)) {
    (*result_messages)++;
    count = ParseWifiScanResult(buffer, result_length, access_points,
                                max_access_points, count);
  }
  return count;
}


#if defined(RASPI_C64) || defined(RASPI_C128)
extern "C" {
#include "third_party/vice-3.3/src/resources.h"
}

extern "C" int circle_get_acia_network_enabled(void) {
  int enabled = 0;
  resources_get_int("Acia1Enable", &enabled);
  return enabled;
}

extern "C" int circle_get_acia_network_address(void) {
  int address = 0;
  resources_get_int("Acia1Base", &address);
  return address;
}

extern "C" int circle_set_acia_network_address(int address) {
  if (!IsAciaNetworkAddress(address)) {
    return 0;
  }
  return resources_set_int("Acia1Mode", 1) == 0 &&
         resources_set_int("Acia1Base", address) == 0;
}

extern "C" int circle_set_acia_network_enabled(int enabled) {
  if (enabled) {
    if (resources_set_int("Acia1Mode", 1) < 0) {
      return 0;
    }
  }
  return resources_set_int("Acia1Enable", enabled) == 0;
}
#else
extern "C" int circle_get_acia_network_enabled(void) {
  return 0;
}

extern "C" int circle_get_acia_network_address(void) {
  return 0;
}

extern "C" int circle_set_acia_network_address(int address) {
  (void) address;
  return 0;
}

extern "C" int circle_set_acia_network_enabled(int enabled) {
  (void) enabled;
  return 0;
}
#endif

extern "C" int circle_get_network_ip_address(char *address,
                                              unsigned int address_size) {
  if (address == nullptr || address_size == 0 || network_subsystem == nullptr ||
      !network_subsystem->IsRunning()) {
    return 0;
  }

  CString formatted_address;
  network_subsystem->GetConfig()->GetIPAddress()->Format(&formatted_address);
  if (strcmp(static_cast<const char *>(formatted_address), "0.0.0.0") == 0) {
    return 0;
  }
  strncpy(address, static_cast<const char *>(formatted_address), address_size - 1);
  address[address_size - 1] = '\0';
  return 1;
}

extern "C" int circle_get_network_config(int which, char *out,
                                          unsigned int size) {
  if (out == nullptr || size == 0 || network_subsystem == nullptr ||
      !network_subsystem->IsRunning()) {
    return 0;
  }
  CNetConfig *config = network_subsystem->GetConfig();
  CIPAddress address;
  switch (which) {
  case 0: address.Set(*config->GetIPAddress()); break;
  case 1: address.Set(config->GetNetMask()); break;
  case 2: address.Set(*config->GetDefaultGateway()); break;
  case 3: address.Set(*config->GetDNSServer()); break;
  default: return 0;
  }
  CString text;
  address.Format(&text);
  if (strcmp(static_cast<const char *>(text), "0.0.0.0") == 0) {
    return 0;
  }
  strncpy(out, static_cast<const char *>(text), size - 1);
  out[size - 1] = '\0';
  return 1;
}

extern "C" int circle_get_network_status(void) {
  if (stdio_app != nullptr) {
    return stdio_app->GetNetworkStatus();
  }
  return CIRCLE_NETWORK_DISABLED;
}

extern "C" void circle_set_network_status_changed_handler(
    circle_network_status_changed_handler_t *handler) {
  network_status_changed_handler = handler;
}

extern "C" int circle_scan_wifi_access_points(
    struct wifi_access_point *access_points, unsigned int max_access_points) {
  if (stdio_app == nullptr || access_points == nullptr ||
      max_access_points == 0) {
    return 0;
  }
  return stdio_app->ScanWifiAccessPoints(access_points, max_access_points);
}

extern "C" int circle_has_onboard_wifi(void) {
  return stdio_app != nullptr &&
         HasOnboardWLAN(CMachineInfo::Get()->GetMachineModel());
}

extern "C" int circle_has_onboard_ethernet(void) {
  return stdio_app != nullptr &&
         HasOnboardEthernet(CMachineInfo::Get()->GetMachineModel());
}

extern "C" int circle_wifi_is_running(void) {
  return stdio_app != nullptr && stdio_app->WifiIsRunning();
}

extern "C" int circle_connect_wifi(void) {
  return stdio_app != nullptr && stdio_app->ConnectWifi();
}

int ViceNetworkHasOnboardWifi(TMachineModel machine_model) {
  return HasOnboardWLAN(machine_model);
}

int ViceNetworkHasOnboardEthernet(TMachineModel machine_model) {
  return HasOnboardEthernet(machine_model);
}

bool ViceNetworkHasWifiFirmware(const char *firmware_path) {
  return HasWifiFirmware(firmware_path);
}

unsigned int ViceNetworkCollectWifiScanResults(
    CBcm4343Device *wlan, struct wifi_access_point *access_points,
    unsigned int max_access_points, unsigned int count,
    unsigned int *result_messages) {
  return CollectWifiScanResults(wlan, access_points, max_access_points, count,
                                result_messages);
}










namespace _CircleStdlib {
extern CNetSubSystem *pCNet;
}

void ViceNetworkSetSubsystem(CNetSubSystem *subsystem) {
  network_subsystem = subsystem;
  _CircleStdlib::pCNet = subsystem;
}

CNetSubSystem *ViceNetworkGetSubsystem(void) {
  return network_subsystem;
}

void ViceNetworkSetStdioApp(ViceStdioApp *app) {
  stdio_app = app;
}

void ViceNetworkNotifyStatusChanged(void) {
  if (network_status_changed_handler != nullptr) {
    network_status_changed_handler();
  }
}
