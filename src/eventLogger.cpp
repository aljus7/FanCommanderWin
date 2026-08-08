#include "eventLogger.h"

// DEFINITION — only one place in the whole program
std::map<std::string, std::string> loggingAreas;

// NO default argument here
void logToEventLog(const std::string& msg, WORD type)
{
    HANDLE hEventLog = RegisterEventSourceA(NULL, "fanCommander");

    if (hEventLog) {
        LPCSTR strings[1] = { msg.c_str() };

        ReportEventA(
            hEventLog,
            type,
            0,
            0x1000,
            NULL,
            1,
            0,
            strings,
            NULL
        );

        DeregisterEventSource(hEventLog);
    }
}

void addLoggingAreaMessage(const std::string& area, const std::string& message) {
    loggingAreas[area] += message + "\n";
}

void logLoggingArea(const std::string& area) {
    auto it = loggingAreas.find(area);
    if (it != loggingAreas.end()) {
        logToEventLog(it->second);
    }
}

void errorLog(const std::string& msg) {
    logToEventLog(msg, EVENTLOG_ERROR_TYPE);
}
