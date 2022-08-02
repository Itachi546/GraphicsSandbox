#include "Utils.h"
#include "Platform.h"

#include <assert.h>

UINT GetMessageIconType(MessageType messageType)
{
	switch (messageType)
	{
	case MessageType::Info:
		return MB_ICONINFORMATION;
	case MessageType::Error:
		return MB_ICONERROR;
	case MessageType::Warning:
		return MB_ICONWARNING;
	case MessageType::Question:
		return MB_ICONQUESTION;
	default:
		return MB_ICONINFORMATION;
	}
}

void Utils::ShowMessageBox(const std::string& message, const std::string& caption, MessageType messageType)
{
#ifdef PLATFORM_WINDOW
	MessageBoxA(GetActiveWindow(), message.c_str(), caption.c_str(), GetMessageIconType(messageType));
#endif
}

char* Utils::ReadFile(const char* path, uint32_t* fileSizeInByte)
{
	FILE* file = nullptr;
	fopen_s(&file, path, "rb");
	if (!file)
		return nullptr;

	fseek(file, 0, SEEK_END);
	long length = ftell(file);
	assert(length >= 0);
	fseek(file, 0, SEEK_SET);
	char* buffer = new char[length];
	assert(buffer);
	size_t rc = fread(buffer, 1, length, file);
	assert(rc == size_t(length));
	fclose(file);

	*fileSizeInByte = length;
	return buffer;
}
