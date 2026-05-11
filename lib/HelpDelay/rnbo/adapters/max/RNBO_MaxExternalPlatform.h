#ifndef RNBO_MAX_MAXEXTERNALPLATFORM
#define RNBO_MAX_MAXEXTERNALPLATFORM

#include <c74_min.h>

namespace RNBO {

namespace Platform {

	void printMessage(const char* message)
	{
		c74::max::object_post(nullptr, message);
	}

	void printErrorMessage(const char* message)
	{
		c74::max::object_error(nullptr, message);
	}
} // Platform

} // RNBO

#endif // RNBO_MAX_MAXEXTERNALPLATFORM
