#pragma once

#ifdef LHM_BRIDGE_EXPORTS
    #define LHM_BRIDGE_API __declspec(dllexport)
#else
    #define LHM_BRIDGE_API __declspec(dllimport)
#endif

#include <string>
#include <src/eventLogger.h>

#define LOG_AREA_LHM "Initialization"

enum DeviceType
{
    CPU = 0,
    GPU = 1
};

extern "C" {
    LHM_BRIDGE_API int __stdcall GetDeviceTemp(DeviceType deviceType, const wchar_t* sensorName, int deviceOrder);
    LHM_BRIDGE_API bool SetFanPwm(int fanIndex, unsigned char pwmValue);
	LHM_BRIDGE_API float ReadFanRpm(int fanIndex);
    LHM_BRIDGE_API void ListAllDevices();
    LHM_BRIDGE_API void TestAllFansSequence();
    LHM_BRIDGE_API void logLhmArea();
}