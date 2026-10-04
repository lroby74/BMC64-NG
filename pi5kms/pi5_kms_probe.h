#ifndef _pi5_kms_probe_h
#define _pi5_kms_probe_h

#include <circle/logger.h>
#include <circle/types.h>

namespace pi5kms {

class Probe {
public:
  void Dump(CLogger &logger) const;
  void RunModeSweep(CLogger &logger, unsigned hold_ms) const;

private:
  static u32 ReadReg(uintptr base, u32 offset);
  static void WriteReg(uintptr base, u32 offset, u32 value);
  static bool GetClockRate(unsigned clock_id, u32 *rate);
  static bool SetClockRate(unsigned clock_id, u32 rate);
  static bool GetDisplayDimensions(u32 *width, u32 *height);
  static void DumpClock(CLogger &logger, const char *name, unsigned clock_id);
  static void DumpReg(CLogger &logger, const char *block, uintptr base,
                      const char *name, u32 offset);
  static void SetClockAndLog(CLogger &logger, const char *name,
                             unsigned clock_id, u32 rate);
  static void InitVc6Phy(CLogger &logger, u32 tmds_rate);
  static void SetMode(CLogger &logger, unsigned mode_index);
};

}  // namespace pi5kms

#endif
