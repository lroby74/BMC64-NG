





// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or


#include <mbedtls/platform_time.h>
#include <circle/timer.h>

extern "C" mbedtls_ms_time_t mbedtls_ms_time (void)
{
	return static_cast<mbedtls_ms_time_t> (CTimer::GetClockTicks64 ()
						 / (CLOCKHZ / 1000));
}
