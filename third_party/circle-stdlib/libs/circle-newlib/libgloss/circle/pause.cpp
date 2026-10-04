#include "circle_macros.h"

#include <unistd.h>
#include <errno.h>
#include <circle/sched/scheduler.h>

extern "C"
int pause (void)
{
	while (1)
	{
		CScheduler::Get ()->Sleep (20);
	}

	errno = EINTR;

	return -1;
}
