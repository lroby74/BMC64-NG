

















#include <circle/devicenameservice.h>
#include <circle/usb/usbxum1541.h>
#include <circle/usb/usbhostcontroller.h>
#include <circle/logger.h>
#include <circle/memorymap.h>
#include <string.h>

extern "C" {
#include "third_party/common/circle.h"
}














#define XUM_NO_DEADLINE USB_TIMEOUT_NONE

static CUSBXum1541Device *s_pXum = 0;

static CUSBXum1541Device *trova(void) {
  return (CUSBXum1541Device *)
      CDeviceNameService::Get()->GetDevice("uxum", 1, FALSE);
}

extern "C" {

int circle_xum1541_present(void) { return trova() != 0 ? 1 : 0; }

int circle_xum1541_open(void) {
  s_pXum = trova();
  if (s_pXum == 0) {
    CLogger::Get()->Write("uxum", LogWarning, "no xum1541 plugged in");
    return -1;
  }
  return 0;
}

void circle_xum1541_close(void) { s_pXum = 0; }




















#define RIMBALZO_ALLINEA        64
#define RIMBALZO_MINIMO         256
#define RIMBALZO_MASSIMO        (64 * 1024)

static u8 *s_pRimbalzoGrezzo = 0;
static u8 *s_pRimbalzo = 0;
static unsigned s_nRimbalzo = 0;

static u8 *rimbalzo(unsigned nLen) {
  if (s_pRimbalzo != 0 && s_nRimbalzo >= nLen) {
    return s_pRimbalzo;
  }
  if (nLen > RIMBALZO_MASSIMO) {
    return 0;
  }

  unsigned nNuova = RIMBALZO_MINIMO;
  while (nNuova < nLen) {
    nNuova *= 2;
  }

  u8 *p = new u8[nNuova + RIMBALZO_ALLINEA];
  if (p == 0) {
    return 0;
  }

  delete[] s_pRimbalzoGrezzo;
  s_pRimbalzoGrezzo = p;
  s_pRimbalzo = (u8 *)(((uintptr)p + (RIMBALZO_ALLINEA - 1))
                       & ~(uintptr)(RIMBALZO_ALLINEA - 1));
  s_nRimbalzo = nNuova;



  if ((uintptr)s_pRimbalzo <= MEM_KERNEL_END) {
    return 0;
  }
  return s_pRimbalzo;
}

int circle_xum1541_control_in(int request, unsigned char *buf, int len) {
  if (s_pXum == 0) {
    return -1;
  }
  if (buf == 0 || len <= 0 || (uintptr)buf > MEM_KERNEL_END) {
    return s_pXum->ControlIn((unsigned char)request, buf, (unsigned short)len);
  }

  u8 *p = rimbalzo((unsigned)len);
  if (p == 0) {
    return -1;
  }
  int n = s_pXum->ControlIn((unsigned char)request, p, (unsigned short)len);
  if (n > 0) {
    memcpy(buf, p, (size_t)(n > len ? len : n));
  }
  return n;
}

int circle_xum1541_control_out(int request) {
  if (s_pXum == 0) {
    return -1;
  }
  return s_pXum->ControlOut((unsigned char)request);
}

int circle_xum1541_bulk_out(const unsigned char *buf, int len) {
  if (s_pXum == 0 || len <= 0) {
    return -1;
  }
  if ((uintptr)buf > MEM_KERNEL_END) {
    return s_pXum->BulkOut(buf, (unsigned)len, XUM_NO_DEADLINE);
  }

  u8 *p = rimbalzo((unsigned)len);
  if (p == 0) {
    return -1;
  }
  memcpy(p, buf, (size_t)len);
  return s_pXum->BulkOut(p, (unsigned)len, XUM_NO_DEADLINE);
}

int circle_xum1541_bulk_in(unsigned char *buf, int len) {
  if (s_pXum == 0 || len <= 0) {
    return -1;
  }
  if ((uintptr)buf > MEM_KERNEL_END) {
    return s_pXum->BulkIn(buf, (unsigned)len, XUM_NO_DEADLINE);
  }

  u8 *p = rimbalzo((unsigned)len);
  if (p == 0) {
    return -1;
  }
  int n = s_pXum->BulkIn(p, (unsigned)len, XUM_NO_DEADLINE);
  if (n > 0) {
    memcpy(buf, p, (size_t)(n > len ? len : n));
  }
  return n;
}

}


extern "C" void circle_xum1541_set_imod(int imodi) {
  if (s_pXum != 0) {
    s_pXum->SetIrqDelay((unsigned)imodi);
  }
}

extern "C" int circle_xum1541_get_imod(void) {
  return s_pXum != 0 ? (int)s_pXum->GetIrqDelay() : -1;
}

void circle_xum1541_clear_halts (void) {
  if (s_pXum != nullptr) {
    s_pXum->ClearHalts ();
  }
}
