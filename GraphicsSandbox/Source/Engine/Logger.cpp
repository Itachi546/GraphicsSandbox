#include "Logger.h"
#include <deque>

#include <assert.h>
#include <fstream>

constexpr const char* DEBUG_COLOR = "\u001b[37m";
constexpr const char* ERROR_COLOR = "\u001b[31m";
constexpr const char* WARN_COLOR  = "\u001b[33m";
constexpr const char* INFO_COLOR  = "\u001b[36m";

enum class LogType
{
	Debug,
	Warn,
	Info,
	Error
};

struct LogEntry
{
	LogType type;
	std::string log;
};

std::deque<LogEntry> gHistory;
const char* logFile = "log.txt";
int maxLines = 500;

static void AddToLogHistory(const LogEntry& entry)
{
	if (gHistory.size() > maxLines)
	{
		gHistory.pop_front();
	}
	gHistory.push_back(entry);
}

static void Print(std::string_view msg, const char* colorCode)
{
	fprintf(stdout, "%s%s\n", colorCode, msg.data());
#ifdef _MSC_VER
	OutputDebugStringA(msg.data());
	OutputDebugStringA("\n");
#endif
}

void Logger::Initialize()
{
	Debug("Initializing Logger");
}

void Logger::Debug(const std::string& str)
{
	std::string message = "[DEBUG]: " + str;
	AddToLogHistory({ LogType::Debug, message});
	Print(message, DEBUG_COLOR);
}

void Logger::Info(const std::string& str)
{
	std::string message = "[INFO]: " + str;
	AddToLogHistory({ LogType::Info, message });
	Print(message, INFO_COLOR);
}

void Logger::Error(const std::string& str)
{
	std::string message = "[ERROR]: " + str;
	AddToLogHistory({ LogType::Error, message });
	Print(message, ERROR_COLOR);
	assert(0);
}

void Logger::Warn(const std::string& str)
{
	std::string message = "[WARN]: " + str;
	AddToLogHistory({ LogType::Warn, message });
	Print(message, WARN_COLOR);
}
