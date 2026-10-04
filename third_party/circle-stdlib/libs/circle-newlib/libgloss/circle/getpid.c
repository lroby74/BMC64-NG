/*
 * Stub version of getpid.
 */

#include "config.h"
#include <_ansi.h>
#include <_syslist.h>
#include <errno.h>
#include "warning.h"

int
_getpid(void)
{
  errno = ENOSYS;
  return -1;
}

// stub_warning(_getpid)
