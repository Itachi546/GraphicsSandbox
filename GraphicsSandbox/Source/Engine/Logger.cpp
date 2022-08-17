#include "Logger.h"
#include <deque>

#include <assert.h>
#include <fstream>

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

static void Print(std::string_view msg)
{
	fprintf(stdout, "%s\n", msg.data());
}

void Logger::Initialize()
{
	Debug("Initializing Logger");
}

void Logger::Debug(const std::string& str)
{
	AddToLogHistory({ LogType::Debug, str });
	Print("[DEBUG]: " +  str);
}

void Logger::Info(const std::string& str)
{
	AddToLogHistory({ LogType::Info, str });
	Print("[INFO]: " +  str);
}

void Logger::Error(const std::string& str)
{
	AddToLogHistory({ LogType::Error, str });
	Print("[ERROR]: " + str);
	assert(0);
}

void Logger::Warn(const std::string& str)
{
	AddToLogHistory({ LogType::Warn, str });
	Print("[WARN]: " + str);
}
