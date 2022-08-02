#pragma once

#include <string>
#include "Enums.h"

namespace Utils
{
	void ShowMessageBox(const std::string& message, const std::string& caption, MessageType type = MessageType::Info);

	char* ReadFile(const char* filename, uint32_t* fileSizeInByte);
};