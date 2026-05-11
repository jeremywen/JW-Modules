//
//  RNBO_Logger.h
//  RNBO
//
//  Created by Stefan Brunner on 18.04.24.
//

#ifndef RNBO_Logger_h
#define RNBO_Logger_h

#include "RNBO_Types.h"
#include "RNBO_String.h"
#include "RNBO_List.h"

namespace RNBO {

/**
 * @private
 */
class Logger {
public:
	virtual ~Logger() {}
	virtual void log(LogLevel level, const char* message) = 0;

	// can send as many args to log() as you like, similar to javascript
	template <typename...ARGS>
	void log(ARGS...args) {
		String message;

		// template magic to handle arbitrary args
		appendArgsToString(message, args...);

		log(Info, message.c_str());
	}

	template <typename...ARGS>
	void log(LogLevel level, ARGS...args) {
		String message;

		// template magic to handle arbitrary args
		appendArgsToString(message, args...);

		log(level, message.c_str());
	}

	// clients can set callback to take over handling of log messages
	using OutputCallback = void(LogLevel level, const char* message);
	virtual void setLoggerOutputCallback(OutputCallback*) {}
	
	static Logger& getInstance();

private:
	// implementation details below
	// necessary in header for templates to work

	void appendArgument(String& message, const char *str)
	{
		message += str;
	}

	template <class T, int N=256>
	void appendArgument(String& message, T val)
	{
		char buff[N];
		Platform::toString(buff, N, val);
		message += buff;
	}

	void appendArgument(String& message, const list& l)
	{
		for (size_t i = 0; i < l.length; i++) {
			if (i > 0) {
				message += " ";
			}
			appendArgument(message, l[i]);
		}
	}

    template<typename T, size_t N> void appendArgument(String& message, const listbase<T, N>& l)
    {
        for (size_t i = 0; i < l.length; i++) {
            if (i > 0) {
                message += " ";
            }
            appendArgument(message, l[i]);
        }
    }

	/** empty argument handling (required for recursive variadic templates)
	 */
	void appendArgsToString(String& message)
	{
		RNBO_UNUSED(message);
	}

	/** handle N arguments of any type by recursively working through them
	 and matching them to the type-matched routine above.
	 */
	template <typename FIRST_ARG, typename ...REMAINING_ARGS>
	void appendArgsToString(String& message, FIRST_ARG const& first, REMAINING_ARGS const& ...args)
	{
		appendArgument(message, first);
		bool hasArgs = sizeof...(args) > 0 ? true : false;
		if (hasArgs) {
			message += " ";
			appendArgsToString(message, args...);	// recurse
		}
	}
};


/**
	this function will be provided by the generated code
 */

using SetLoggerFunctionPtr = void(*)(Logger*);

} // RNBO

#endif /* RNBO_Logger_h */
