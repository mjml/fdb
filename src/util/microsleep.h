#pragma once
#include <time.h>

inline int microsleep (long sec, long us)
{
	struct timespec ts;
	ts.tv_sec = sec;
	ts.tv_nsec = 1000 * us;
	return nanosleep(&ts, nullptr);
}
