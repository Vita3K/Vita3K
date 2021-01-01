#pragma once

#include <ctime>

#ifdef WIN32
#define SAFE_GMTIME(time, result) gmtime_s(result, time)
#define SAFE_LOCALTIME(time, result) localtime_s(result, time)
#else
#define SAFE_GMTIME(time, result) gmtime_r(time, result)
#define SAFE_LOCALTIME(time, result) localtime_r(time, result)
#endif
