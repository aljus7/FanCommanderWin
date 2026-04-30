#pragma once

enum DeviceType
{
    CPU,
    GPU
};

extern "C" {
    __declspec(dllimport) int GetDeviceTemp(enum DeviceType deviceType, const wchar_t* sensorName, const int deviceOrder);
    __declspec(dllimport) void ListAllDevices();
}