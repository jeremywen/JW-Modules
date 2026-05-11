//
//  RNBO_Platform.h
//  baremetal
//
//  Created by Stefan Brunner on 03.04.24.
//

#ifndef RNBO_Platform_h
#define RNBO_Platform_h

#ifndef RNBO_USECUSTOMPLATFORM

#include "RNBO_Debug.h"
#include "RNBO_Types.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <stdexcept>

namespace RNBO {

struct ErrorReportingInfo {

#if RNBO_PLATFORM_ASSERT_WARN==1
		bool mAssertWarned = false;
#endif
#if RNBO_PLATFORM_ERROR_WARN==1
		bool mErrorWarned = false;
#endif
};

namespace Platform {

static void printMessage(const char* message);
static void printErrorMessage(const char* message);

static void error(RuntimeError e, const char* msg);
static void assertTrue(bool v, const char* msg);

static void toString(char* str, size_t maxlen, number val);
static void toString(char* str, size_t maxlen, int val);
static void toString(char* str, size_t maxlen, unsigned int val);
static void toString(char* str, size_t maxlen, long val);
static void toString(char* str, size_t maxlen, long long val);
static void toString(char* str, size_t maxlen, unsigned long val);
static void toString(char* str, size_t maxlen, unsigned long long val);
static void toString(char* str, size_t maxlen, void* val);

static ErrorReportingInfo s_ErrorReportingInfo;

static bool once(bool& v) {
	bool have = v;
	v = true;
	return !have;
}

ATTRIBUTE_UNUSED
static void resetWarnings() {
#if RNBO_PLATFORM_ERROR_WARN==1
			s_ErrorReportingInfo.mErrorWarned = false;
#endif
#if RNBO_PLATFORM_ASSERT_WARN==1
			s_ErrorReportingInfo.mAssertWarned = false;
#endif
}

template <typename T>
T errorOrDefault(RuntimeError e, const char* str, T def) {
#if RNBO_PLATFORM_ERROR_WARN==0
	error(e, str);
#else
	RNBO_UNUSED(e);
	//warn
	if (once(s_ErrorReportingInfo.mErrorWarned)) {
		printErrorMessage(str);
	}
#endif
	return def;
}


template <typename T>
T assertTrueOrDefault(bool v, const char* str, T def) {
#if RNBO_PLATFORM_ASSERT_WARN==0
	assertTrue(v, str);
#else
	//warn
	if (!v && once(s_ErrorReportingInfo.mAssertWarned)) {
		printErrorMessage(str);
	}
#endif
	return def;
}

ATTRIBUTE_UNUSED
static void error(RuntimeError e, const char* msg) {
	switch (e) {
#if defined(RNBO_NOSTL) || defined(RNBO_NOTHROW)
		default:
			printErrorMessage(msg);
#else
		case RuntimeError::OutOfRange:
			throw std::out_of_range(msg);
        case RuntimeError::QueueOverflow:
            throw std::overflow_error(msg);
		default:
			throw std::runtime_error(msg);
#endif
	}
}

ATTRIBUTE_UNUSED
static void assertTrue(bool v, const char* msg) {
	if (!v) {
#if defined(RNBO_NOSTL) || defined(RNBO_NOTHROW)
		printErrorMessage(msg);
#else
		throw std::runtime_error(msg);
#endif
	}
}


#ifndef RNBO_USECUSTOMPLATFORMPRINT

/**
 * @brief Print a message to stdout or some kind of log
 */
static void printMessage(const char* message) {
	printf("%s\n", message);
}

/**
 * @brief Prints an error message
 *
 * Defaults to printMessage.
 */
static void printErrorMessage(const char* message) {
	printMessage(message);
}

#endif // RNBO_USECUSTOMPLATFORMPRINT

// string formatting
ATTRIBUTE_UNUSED static void toString(char* str, size_t maxlen, number val)
{
	snprintf(str, maxlen, "%f", double(val));
}

ATTRIBUTE_UNUSED
static void toString(char* str, size_t maxlen, int val)
{
	snprintf(str, maxlen, "%d", val);
}

ATTRIBUTE_UNUSED
static void toString(char* str, size_t maxlen, unsigned int val)
{
	snprintf(str, maxlen, "%u", val);
}

ATTRIBUTE_UNUSED
static void toString(char* str, size_t maxlen, long val)
{
	snprintf(str, maxlen, "%ld", val);
}

ATTRIBUTE_UNUSED
static void toString(char* str, size_t maxlen, long long val)
{
	snprintf(str, maxlen, "%lld", val);
}

ATTRIBUTE_UNUSED
static void toString(char* str, size_t maxlen, unsigned long val)
{
	snprintf(str, maxlen, "%lu", val);
}

ATTRIBUTE_UNUSED
static void toString(char* str, size_t maxlen, unsigned long long val)
{
	snprintf(str, maxlen, "%llu", val);
}

ATTRIBUTE_UNUSED
static void toString(char* str, size_t maxlen, void* val)
{
	snprintf(str, maxlen, "%p", val);
}

#ifndef RNBO_USECUSTOMALLOCATOR

using ::free;
using ::malloc;
using ::calloc;
using ::realloc;

#else

// if you use custom allocation you have to implement these
void *malloc(size_t size);
void  free(void * ptr);
void *realloc(void * ptr, size_t size);
void *calloc(size_t count, size_t size);

#endif // RNBO_USECUSTOMALLOCATOR

using ::memmove;
using ::memcpy;
using ::strcmp;
using ::strlen;
using ::strcpy;
using ::abort;
using ::memset;


} // namespace Platform

} // namespace RNBO

#endif // RNBO_USECUSTOMPLATFORM

#endif /* RNBO_Platform_h */
