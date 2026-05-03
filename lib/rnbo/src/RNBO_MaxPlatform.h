//
//  RNBO_Platform.h
//  baremetal
//
//  Created by Stefan Brunner on 03.04.24.
//

#ifndef RNBO_MaxPlatform_h
#define RNBO_MaxPlatform_h

#include "RNBO_Types.h"
#include "RNBO_PlatformInterface.h"

namespace RNBO {

extern "C" PlatformInterface* getPlatformInstance();

struct ErrorReportingInfo {
	bool mAssertWarned = false;
	bool mErrorWarned = false;
};

#pragma GCC diagnostic error "-Wreturn-type"

namespace Platform {

static void error(RuntimeError e, const char* msg);
static void assertTrue(bool v, const char* msg);
static void printMessage(const char* message);
static void printErrorMessage(const char* message);

static ErrorReportingInfo s_ErrorReportingInfo;

static bool once(bool& v) {
	bool have = v;
	v = true;
	return !have;
}

template <typename T>
T errorOrDefault(RuntimeError e, const char* str, T def) {
	RNBO_UNUSED(e);
	//warn
	if (once(s_ErrorReportingInfo.mErrorWarned)) {
		printErrorMessage(str);
	}
	return def;
}


template <typename T>
T assertTrueOrDefault(bool v, const char* str, T def) {
	//warn
	if (!v && once(s_ErrorReportingInfo.mAssertWarned)) {
		printErrorMessage(str);
	}
	return def;
}

static void error(RuntimeError e, const char* msg) {
	getPlatformInstance()->error(e, msg);
}

static void assertTrue(bool v, const char* msg) {
	getPlatformInstance()->assertTrue(v, msg);
}

static void printMessage(const char* message) {
	getPlatformInstance()->printMessage(message);
}

static void printErrorMessage(const char* message) {
	printMessage(message);
}

// string formatting
template <typename T> static void toString(char* str, size_t maxlen, T val)
{
	getPlatformInstance()->toString(str, maxlen, val);
}

static void free(void* ptr) {
	getPlatformInstance()->free(ptr);
}

static void* malloc(size_t bytes) {
	return getPlatformInstance()->malloc(bytes);
}

static void* calloc(size_t num, size_t size) {
	return getPlatformInstance()->calloc(num, size);
}

static void* realloc(void* ptr, size_t bytes) {
	return getPlatformInstance()->realloc(ptr, bytes);
}

static void* memmove(void* dest, const void* src, size_t n) {
	return getPlatformInstance()->memmove(dest, src, n);
}

static void* memcpy(void* dest, const void* src, size_t n) {
	return getPlatformInstance()->memcpy(dest, src, n);
}

static int strcmp(const char *s1, const char *s2) {
	return getPlatformInstance()->strcmp(s1, s2);
}

static size_t strlen(const char *s) {
	return getPlatformInstance()->strlen(s);
}

static char* strcpy(char* dest, const char* src) {
	return getPlatformInstance()->strcpy(dest, src);
}

static void abort() {
	return getPlatformInstance()->abort();
}

static void* memset(void *dest, int value, size_t n) {
	return getPlatformInstance()->memset(dest, value, n);
}


} // namespace Platform

} // namespace RNBO

#endif /* RNBO_MaxPlatform_h */
