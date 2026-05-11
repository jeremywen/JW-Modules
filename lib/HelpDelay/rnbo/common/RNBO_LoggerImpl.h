//
//  RNBO_Logger.h
//  baremetal
//
//  Created by Stefan Brunner on 18.04.24.
//

#ifndef RNBO_LoggerImpl_h
#define RNBO_LoggerImpl_h

#include "RNBO_Logger.h"
#include "RNBO_String.h"

namespace RNBO {

class LoggerImpl : public Logger
{
public:
	LoggerImpl() {}

	void log(LogLevel level, const char* message) override
	{
		if (_outputCallback) {
			_outputCallback(level, message);
		}
		else {
			const static char* levelStr[] = { "[INFO]", "[WARNING]", "[ERROR]" };
			String formattedMessage = levelStr[level];
			formattedMessage += "\t";
			formattedMessage += message;
			formattedMessage += "\n";
			Platform::printMessage(formattedMessage.c_str());
		}
	}

	void setLoggerOutputCallback(OutputCallback* callback) override {
		_outputCallback = callback;
	}

private:

	OutputCallback* _outputCallback = nullptr;
};

extern "C" void SetLogger(Logger* logger);

static LoggerImpl internalLogger;
static Logger* console = &internalLogger;

} // RNBO

#endif /* RNBO_LoggerImpl_h */
