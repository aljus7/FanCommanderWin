#pragma once
#include <windows.h>
#include <string>
#include <map>

// DECLARATION ONLY — no definition here
extern std::map<std::string, std::string> loggingAreas;

// Default argument ONLY here
void logToEventLog(const std::string& msg, WORD type = EVENTLOG_INFORMATION_TYPE);

void addLoggingAreaMessage(const std::string& area, const std::string& message);
void logLoggingArea(const std::string& area);
void errorLog(const std::string& msg);
