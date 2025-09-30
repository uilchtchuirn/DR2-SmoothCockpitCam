////////////////////////////////////////////////////////////////////////////////////////////////////////
// Part of Injectable Generic Camera System
// Copyright(c) 2020, Frans Bouma
// All rights reserved.
// https://github.com/FransBouma/InjectableGenericCameraSystem
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met :
//
//  * Redistributions of source code must retain the above copyright notice, this
//	  list of conditions and the following disclaimer.
//
//  * Redistributions in binary form must reproduce the above copyright notice,
//    this list of conditions and the following disclaimer in the documentation
//    and / or other materials provided with the distribution.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
// AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
// DISCLAIMED.IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
// FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
// DAMAGES(INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
// SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
// CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
// OR TORT(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
////////////////////////////////////////////////////////////////////////////////////////////////////////

#include "stdafx.h"
#include "MessageHandler.h"
#include "NamedPipeManager.h"
#include "Utils.h"
#include <chrono>
#include <iomanip>
#include <sstream>
#include <ctime>
#include "Console.h"
#include "Config.h"


namespace IGCS::MessageHandler
{
	
    const bool consoleEnabled = IGCS::Config::get().ConsoleEnabled;

	std::string currentTimeString() {
		using namespace std::chrono;
		auto now = system_clock::now();
		std::time_t now_c = system_clock::to_time_t(now);
		std::tm tm = *std::localtime(&now_c);
		std::ostringstream oss;
		oss << std::put_time(&tm, "%H:%M:%S");
		return oss.str();
	}

    void logDebug(const char* fmt, ...)
    {
#ifdef _DEBUG
        if (!consoleEnabled) return;
        va_list args;
        va_start(args, fmt);
        const std::string formattedArgs = Utils::formatStringVa(fmt, args);
        va_end(args);

        IGCS::Console::WriteLine(formattedArgs);
#endif
    }

    void logError(const char* fmt, ...)
    {
        if (!consoleEnabled) return;
        va_list args;
        va_start(args, fmt);
        const std::string formattedArgs = Utils::formatStringVa(fmt, args);
        va_end(args);

        IGCS::Console::WriteError(formattedArgs);
    }

    void logLine(const char* fmt, ...)
    {
        if (!consoleEnabled) return;
        va_list args;
        va_start(args, fmt);

        // Format the variable argument string
        const std::string formattedArgs = Utils::formatStringVa(fmt, args);
        va_end(args);

        // Prepend timestamp to the log message (kept as before)
        std::string logMessage = "[" + currentTimeString() + "] " + formattedArgs;

        IGCS::Console::WriteLine(logMessage);
    }

}
