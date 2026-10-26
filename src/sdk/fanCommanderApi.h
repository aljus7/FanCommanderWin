#pragma once

#include "../fanData.h"

#ifdef FANCOMMANDERAPI_EXPORTS
#define FAN_API __declspec(dllexport)
#else
#define FAN_API __declspec(dllimport)
#endif

extern "C"
{
    FAN_API bool Connect();

    FAN_API void Disconnect();

    FAN_API int GetFanCount();

    FAN_API int GetFanRpm(int fanIndex);

    FAN_API int GetFilteredPwm(int fanIndex);

    FAN_API int GetUnfilteredPwm(int fanIndex);

    FAN_API const fanData* GetFanData(int fanIndex);
}