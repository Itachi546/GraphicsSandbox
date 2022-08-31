#pragma once

#include <string>

enum class MessageType
{
	Error = 0,
	Warning,
	Info,
};

namespace Utils
{
	void ShowMessageBox(const std::string& message, const std::string& caption, MessageType type = MessageType::Info);

	char* ReadFile(const char* filename, uint32_t* fileSizeInByte);

	constexpr std::size_t StringHash(const char* input)
	{
		size_t hash = sizeof(size_t) == 8 ? 0xcbf29ce484222325 : 0x811c9dc5;
		const size_t prime = sizeof(size_t) == 8 ? 0x00000100000001b3 : 0x01000193;

		while (*input) {
			hash ^= static_cast<size_t>(*input);
			hash *= prime;
			++input;
		}
		return hash;
	}
};