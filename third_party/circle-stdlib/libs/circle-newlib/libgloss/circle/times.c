/*
 * Stub version of times.
 */

#include "config.h"
#include <_ansi.h>
#include <_syslist.h>
#include <sys/times.h>
#include <sys/timespec.h>
#include <errno.h>
#include "warning.h"

clock_t
_times(struct tms *buf)
{
  errno = ENOSYS;
  return -1;
}

int
timespec_get (struct timespec *ts, int base)
{
  return 0;
}

// stub_warning(_times)
