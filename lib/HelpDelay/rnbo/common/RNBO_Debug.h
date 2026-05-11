#ifndef _RNBO_DEBUG_H_
#define _RNBO_DEBUG_H_

// See https://github.com/scottt/debugbreak/blob/master/debugbreak.h for inspiration

namespace RNBO {

#if !defined(RNBO_PLATFORM_ASSERT_WARN)
    #ifdef RNBO_DEBUG
        #define RNBO_PLATFORM_ASSERT_WARN 0
    #else
        #define RNBO_PLATFORM_ASSERT_WARN 1
    #endif
#endif

#if !defined(RNBO_PLATFORM_ERROR_WARN)
    #ifdef RNBO_DEBUG
        #define RNBO_PLATFORM_ERROR_WARN 0
    #else
        #define RNBO_PLATFORM_ERROR_WARN 1
    #endif
#endif

#ifndef RNBO_ASSERT

# if defined(__APPLE__)
	
#if defined(__thumb__)
#define RNBODebugBreak __asm__ volatile(".inst 0xde01")
#elif defined(__arm__)
#define RNBODebugBreak __asm__ volatile(".inst 0xe7f001f0")
#elif defined(__aarch64__)
#define RNBODebugBreak __builtin_trap()
#else
#define RNBODebugBreak asm("int3")
#endif
	
#elif defined(_MSC_VER)
#define RNBODebugBreak __debugbreak()
#else
#define RNBODebugBreak (0)
#endif

#define RNBO_STR_HELPER(x) #x
#define RNBO_STR(x) RNBO_STR_HELPER(x)

#if defined(_DEBUG) || defined(DEBUG)

namespace Platform {
    static void printErrorMessage(const char* message);
}

#define RNBO_ASSERT(condition) \
if (!(condition)) { Platform::printErrorMessage("ASSERTION - failed condition: " RNBO_STR(condition) "\n"); RNBODebugBreak; }

#define RNBO_DEBUG 1

#else
#define RNBO_ASSERT(condition)
#endif

#endif  // #ifndef RNBO_ASSERT

} // namespace RNBO

#endif // #ifndef _RNBO_DEBUG_H_
