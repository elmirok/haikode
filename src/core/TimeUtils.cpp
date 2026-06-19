#include "core/TimeUtils.h"

#include <ctime>
#include <iomanip>
#include <sstream>

namespace TimeUtils {

std::string
CompactTimestamp()
{
	time_t now = time(nullptr);
	struct tm tmValue;
#if defined(_WIN32)
	localtime_s(&tmValue, &now);
#else
	localtime_r(&now, &tmValue);
#endif
	std::ostringstream out;
	out << std::put_time(&tmValue, "%Y%m%d-%H%M%S");
	return out.str();
}

std::string
IsoTimestamp()
{
	time_t now = time(nullptr);
	struct tm tmValue;
#if defined(_WIN32)
	gmtime_s(&tmValue, &now);
#else
	gmtime_r(&now, &tmValue);
#endif
	std::ostringstream out;
	out << std::put_time(&tmValue, "%Y-%m-%dT%H:%M:%SZ");
	return out.str();
}

}
