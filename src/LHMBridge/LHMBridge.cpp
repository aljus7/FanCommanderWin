#define LHM_BRIDGE_EXPORTS
#include "LHMBridge.h"

#include <iostream>
#include <msclr/marshal_cppstd.h>

#using "LibreHardwareMonitorLib.dll"

using namespace System;
using namespace LibreHardwareMonitor::Hardware;
using namespace msclr::interop;
using namespace std;

ref class LHMHost
{
public:
    static Computer^ computer = nullptr;

    static void EnsureInitialized()
    {
        if (computer != nullptr)
            return;

        computer = gcnew Computer();
        computer->IsCpuEnabled = true;
        computer->IsGpuEnabled = true;
        computer->Open();
    }
};

extern "C" __declspec(dllexport) int GetDeviceTemp(enum DeviceType deviceType, const wchar_t* sensorName, const int deviceOrder)
{
    LHMHost::EnsureInitialized();
    System::String^ managedSensorName = gcnew System::String(sensorName);

    for each(IHardware ^ hw in LHMHost::computer->Hardware)
    {
        // CPU
        if (deviceType == CPU && hw->HardwareType == HardwareType::Cpu)
        {
            hw->Update();
			int i = 0;
            for each(ISensor ^ sensor in hw->Sensors)
            {
                if (sensor->SensorType == SensorType::Temperature &&
                    sensor->Name->Contains(managedSensorName))
                {
                    if (i == deviceOrder) {
                        return sensor->Value.HasValue ? (int)sensor->Value.Value : -1;
                    }
                    i++;
                }
            }
        }
        // GPU
        else if (deviceType == GPU && 
            (hw->HardwareType == HardwareType::GpuNvidia ||
             hw->HardwareType == HardwareType::GpuAmd ||
             hw->HardwareType == HardwareType::GpuIntel))
        {
            hw->Update();
			int i = 0;
            for each(ISensor ^ sensor in hw->Sensors)
            {
                if (sensor->SensorType == SensorType::Temperature &&
                    sensor->Name->Contains(managedSensorName))
                {
                    if (i == deviceOrder) {
                        return sensor->Value.HasValue ? (int)sensor->Value.Value : -1;
                    }
                    i++;
                }
            }
        }
    }

    return -1;
}

extern "C" __declspec(dllexport) void ListAllDevices()
{
    LHMHost::EnsureInitialized();

    for each(IHardware ^ hw in LHMHost::computer->Hardware) {
        if (hw->HardwareType == HardwareType::Cpu)
        {
            cout << "CPU: " << marshal_as<string>(hw->Name) << endl;
            hw->Update();
            for each(ISensor ^ sensor in hw->Sensors)
            {
                if (sensor->SensorType == SensorType::Temperature)
                {
                    cout << "  Sensor: " << marshal_as<string>(sensor->Name)
                        << " - Value: "
                        << (sensor->Value.HasValue ? to_string(sensor->Value.Value) : "N/A")
                        << endl;
                }
            }
        }
        else if (hw->HardwareType == HardwareType::GpuNvidia ||
                hw->HardwareType == HardwareType::GpuAmd ||
                hw->HardwareType == HardwareType::GpuIntel)
        {
            cout << "GPU: " << marshal_as<string>(hw->Name) << endl;
            hw->Update();
            for each(ISensor ^ sensor in hw->Sensors)
            {
                if (sensor->SensorType == SensorType::Temperature)
                {
                    cout << "  Sensor: " << marshal_as<string>(sensor->Name)
                        << " - Value: "
                        << (sensor->Value.HasValue ? to_string(sensor->Value.Value) : "N/A")
                        << endl;
                }
            }
        }
    }
}