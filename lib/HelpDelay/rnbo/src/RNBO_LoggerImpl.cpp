//
//  RNBO_LoggerImpl.cpp
//  RNBO
//
//  Created by Stefan Brunner on 30.04.24.
//

#include "RNBO_LoggerImpl.h"

namespace RNBO {

	static LoggerImpl s_console;
	Logger* logger = &s_console;

	Logger& Logger::getInstance()
	{
		return *logger;
	}
}
