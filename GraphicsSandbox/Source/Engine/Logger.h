#pragma once

#include "Platform.h"
#include <string>

namespace Logger
{

	void Initialize();

	void Debug(const std::string& str);

	void Info(const std::string& str);

	void Error(const std::string& str);
	
	void Warn(const std::string& str);
}