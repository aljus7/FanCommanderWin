#include "FanCommanderApi.h"

#include <Windows.h>
#include <wchar.h>

static HANDLE g_hMap = nullptr;
static fanData* g_fanData = nullptr;

bool Connect() {
    OutputDebugStringW(L"FanCommanderApi: Connect() called!\n");

    if (g_hMap)
        return true;

    g_hMap = OpenFileMappingW(
        FILE_MAP_READ,
        FALSE,
        SharedMemoryName
    );

    if (!g_hMap) {
        // Get the exact Windows error code
        DWORD err = GetLastError();
        WCHAR logMsg[256];
        swprintf_s(logMsg, 256, L"FanCommanderApi: OpenFileMappingW failed with error code %lu\n", err);

        // Output to the Windows debugger (Visible in Visual Studio Output window or DebugView)
        OutputDebugStringW(logMsg);
        return false;
    }

    g_fanData = static_cast<fanData*>(
        MapViewOfFile(g_hMap, FILE_MAP_READ, 0, 0, 0)
        );

    if (!g_fanData) {
        CloseHandle(g_hMap);
        g_hMap = nullptr;
        return false;
    }

    return true;
}

void Disconnect() {
    if (g_fanData) {
        UnmapViewOfFile(g_fanData);

        g_fanData = nullptr;
    }

    if (g_hMap) {
        CloseHandle(g_hMap);

        g_hMap = nullptr;
    }
}

int GetFanCount() {
    if (!g_fanData)
        return -1;
    return g_fanData[0].fanCount;
}

int GetFanRpm(int fanIndex) {
    if (!g_fanData)
        return -1;

    return g_fanData[fanIndex].fanRpm;
}

int GetFilteredPwm(int fanIndex) {
    if (!g_fanData)
        return -1;

    return g_fanData[fanIndex].fanPwmFiltered;
}

int GetUnfilteredPwm(int fanIndex) {
    if (!g_fanData)
        return -1;

    return g_fanData[fanIndex].fanPwmUnfiltered;
}

const fanData* GetFanData(
    int fanIndex) {
    if (!g_fanData)
        return nullptr;

    return &g_fanData[fanIndex];
}