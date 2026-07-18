#pragma once

#ifdef LHM_BRIDGE_EXPORTS
    #define LHM_BRIDGE_API __declspec(dllexport)
#else
    #define LHM_BRIDGE_API __declspec(dllimport)
#endif

#include <string>

enum DeviceType
{
    CPU,
    GPU
};

extern "C" {
    LHM_BRIDGE_API int GetDeviceTemp(enum DeviceType deviceType, const wchar_t* sensorName, const int deviceOrder);
    LHM_BRIDGE_API void ListAllDevices();
}