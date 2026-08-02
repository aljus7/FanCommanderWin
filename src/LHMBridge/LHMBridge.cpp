#define LHM_BRIDGE_EXPORTS

#include "LHMBridge.h"

#include <iostream>
#include <msclr/marshal_cppstd.h>
#include <thread>
#include <chrono>

#using "LibreHardwareMonitorLib.dll"

using namespace System;
using namespace msclr::interop;
using namespace std;
using namespace System::Collections::Generic;
using namespace LibreHardwareMonitor::Hardware;

public enum class ManagedDeviceType
{
    CPU = 0,
    GPU = 1
};

ref class LHMHost
{
public:
    static Computer^ computer = nullptr;

    // caches moved into managed ref class to avoid file-scope managed statics
    static System::Collections::Generic::List<LibreHardwareMonitor::Hardware::IControl^>^ cachedControls = nullptr;
    static System::Collections::Generic::List<LibreHardwareMonitor::Hardware::ISensor^>^ cachedFanSensors = nullptr;
    static bool controlsInitialized = false;

    
// temperature sensor caches (per device type)
    static Dictionary<
        System::ValueTuple<ManagedDeviceType, System::String^, int>,
        ISensor^
    >^ tempSensors = nullptr;

    static Dictionary<
        System::ValueTuple<ManagedDeviceType, System::String^, int>,
        IHardware^
    >^ tempHardware = nullptr;
    static bool sensorsInitialized = false;

    static void EnsureInitialized()
    {
        if (computer != nullptr)
            return;

        computer = gcnew Computer();
        // Enable hardware groups needed for SuperIO and sensors
        computer->IsCpuEnabled = true;
        computer->IsGpuEnabled = true;
        computer->IsMotherboardEnabled = true;    // discover SuperIO / motherboard sensors
        computer->IsControllerEnabled = true;     // discover external controllers (pump, RGB, etc.)
        computer->Open();
    }
};

// Helper: build superIO cache
void InitializeSuperIOCache()
{
    if (LHMHost::controlsInitialized) return;

    LHMHost::cachedControls = gcnew System::Collections::Generic::List<LibreHardwareMonitor::Hardware::IControl^>();
    LHMHost::cachedFanSensors = gcnew System::Collections::Generic::List<LibreHardwareMonitor::Hardware::ISensor^>();
    LHMHost::EnsureInitialized();

    for each(IHardware ^ hw in LHMHost::computer->Hardware)
    {
        if (hw->HardwareType != HardwareType::Motherboard) continue;

        // for each sub-hardware (SuperIO or EmbeddedController), collect control and fan sensors by index
        for each(IHardware ^ sub in hw->SubHardware)
        {
            // ensure sub-hardware has fresh readings
            try { sub->Update(); } catch (...) {}

            // build maps index -> control, index -> fan
            System::Collections::Generic::Dictionary<int, LibreHardwareMonitor::Hardware::IControl^>^ controlByIdx =
                gcnew System::Collections::Generic::Dictionary<int, LibreHardwareMonitor::Hardware::IControl^>();
            System::Collections::Generic::Dictionary<int, LibreHardwareMonitor::Hardware::ISensor^>^ fanByIdx =
                gcnew System::Collections::Generic::Dictionary<int, LibreHardwareMonitor::Hardware::ISensor^>();

            for each(LibreHardwareMonitor::Hardware::ISensor ^ s in sub->Sensors)
            {
                if (s->SensorType == SensorType::Control && s->Control != nullptr)
                    controlByIdx[s->Index] = s->Control;
                else if (s->SensorType == SensorType::Fan)
                    fanByIdx[s->Index] = s;
            }

            // determine max index
            int maxIdx = -1;
            for each(int k in controlByIdx->Keys)
                if (k > maxIdx) maxIdx = k;
            for each(int k in fanByIdx->Keys)
                if (k > maxIdx) maxIdx = k;

            for (int idx = 0; idx <= maxIdx; idx++)
            {
                LibreHardwareMonitor::Hardware::IControl^ c = nullptr;
                LibreHardwareMonitor::Hardware::ISensor^ f = nullptr;
                if (controlByIdx->ContainsKey(idx)) c = controlByIdx[idx];
                if (fanByIdx->ContainsKey(idx)) f = fanByIdx[idx];

                LHMHost::cachedControls->Add(c);
                LHMHost::cachedFanSensors->Add(f);
            }
        }
    }

    LHMHost::controlsInitialized = true;
}

// Exports

extern "C" __declspec(dllexport) bool SetFanPwm(int fanIndex, unsigned char pwmValue)
{
    InitializeSuperIOCache();

    auto controls = LHMHost::cachedControls;
    if (controls != nullptr && fanIndex >= 0 && fanIndex < controls->Count && controls[fanIndex] != nullptr)
    {
        auto ctrl = controls[fanIndex];
        try
        {
            // determine target in control units using control's software range
            float minV = ctrl->MinSoftwareValue;
            float maxV = ctrl->MaxSoftwareValue;
            if (maxV <= minV) { minV = 0.0f; maxV = 100.0f; } // guard
            float target = minV + ((float)pwmValue / 255.0f) * (maxV - minV);

            // optional: clamp
            if (target < minV) target = minV;
            if (target > maxV) target = maxV;

            // ensure control supports software mode (best-effort)
            // don't assume, but SetSoftware should handle it or throw
            ctrl->SetSoftware(target);

            // update owning hardware so subsequent reads reflect the change
            try { if (ctrl->Sensor != nullptr && ctrl->Sensor->Hardware != nullptr) ctrl->Sensor->Hardware->Update(); }
            catch (...) {}

            return true;
        }
        catch (...) {}
    }

    // reinit and retry (post-sleep)
    LHMHost::controlsInitialized = false;
    InitializeSuperIOCache();

    controls = LHMHost::cachedControls;
    if (controls != nullptr && fanIndex >= 0 && fanIndex < controls->Count && controls[fanIndex] != nullptr)
    {
        auto ctrl = controls[fanIndex];
        try
        {
            float minV = ctrl->MinSoftwareValue;
            float maxV = ctrl->MaxSoftwareValue;
            if (maxV <= minV) { minV = 0.0f; maxV = 100.0f; }
            float target = minV + ((float)pwmValue / 255.0f) * (maxV - minV);
            if (target < minV) target = minV;
            if (target > maxV) target = maxV;

            ctrl->SetSoftware(target);
            try { if (ctrl->Sensor != nullptr && ctrl->Sensor->Hardware != nullptr) ctrl->Sensor->Hardware->Update(); }
            catch (...) {}
            return true;
        }
        catch (...) {}
    }

    return false;
}

extern "C" __declspec(dllexport) float ReadFanRpm(int fanIndex)
{
    InitializeSuperIOCache();

    auto fans = LHMHost::cachedFanSensors;
    if (fans != nullptr && fanIndex >= 0 && fanIndex < fans->Count && fans[fanIndex] != nullptr)
    {
        // update owning hardware to refresh sensor values
        try { if (fans[fanIndex]->Hardware != nullptr) fans[fanIndex]->Hardware->Update(); } catch (...) {}
        System::Nullable<float> v = fans[fanIndex]->Value;
        if (v.HasValue) return v.Value;
    }

    // reinit once in case of stale caches after sleep
    LHMHost::controlsInitialized = false;
    InitializeSuperIOCache();

    fans = LHMHost::cachedFanSensors;
    if (fans != nullptr && fanIndex >= 0 && fanIndex < fans->Count && fans[fanIndex] != nullptr)
    {
        try { if (fans[fanIndex]->Hardware != nullptr) fans[fanIndex]->Hardware->Update(); } catch (...) {}
        System::Nullable<float> v = fans[fanIndex]->Value;
        return v.HasValue ? v.Value : -1.0f;
    }

    return -1.0f;
}

void initializeTempSensors() {

    if (LHMHost::sensorsInitialized)
    {
        return;
    }

    LHMHost::EnsureInitialized();

    if (!LHMHost::tempSensors)
    {
        LHMHost::tempSensors = gcnew Dictionary<
            System::ValueTuple<ManagedDeviceType, System::String^, int>,
            ISensor^
        >();

        LHMHost::tempHardware = gcnew Dictionary<
            System::ValueTuple<ManagedDeviceType, System::String^, int>,
            IHardware^
        >();
    }

    int CPUIndex = 0;
    int GPUIndex = 0;
    for each(IHardware ^ hw in LHMHost::computer->Hardware) {
        hw->Update();

        if (hw->HardwareType == HardwareType::Cpu)
        {
            for each(ISensor ^ sensor in hw->Sensors)
            {
                if (sensor->SensorType == SensorType::Temperature)
                {
                    auto key = System::ValueTuple<ManagedDeviceType, System::String^, int>(
                        ManagedDeviceType::CPU,
                        sensor->Name,
                        CPUIndex
                    );
                    LHMHost::tempSensors->Add(key, sensor);
                    LHMHost::tempHardware->Add(key, hw);
                }
            }
            CPUIndex++;
        }

        else if (hw->HardwareType == HardwareType::GpuNvidia ||
            hw->HardwareType == HardwareType::GpuAmd ||
            hw->HardwareType == HardwareType::GpuIntel)
        {
            for each(ISensor ^ sensor in hw->Sensors)
            {
                if (sensor->SensorType == SensorType::Temperature)
                {
                    auto key = System::ValueTuple<ManagedDeviceType, System::String^, int>(
                        ManagedDeviceType::GPU,
                        sensor->Name,
                        GPUIndex
                    );
                    LHMHost::tempSensors->Add(key, sensor);
                    LHMHost::tempHardware->Add(key, hw);
                }
            }
            GPUIndex++;
        }
    }

    LHMHost::sensorsInitialized = true; 

}

extern "C" __declspec(dllexport) int __stdcall GetDeviceTemp(DeviceType deviceType, const wchar_t* sensorName, int deviceOrder)
{
    ManagedDeviceType mdt = static_cast<ManagedDeviceType>(deviceType);
    System::String^ managedSensorName = gcnew System::String(sensorName);
	initializeTempSensors();

    auto key = System::ValueTuple<ManagedDeviceType, System::String^, int>(
        mdt,
        managedSensorName,
        deviceOrder
    );

    IHardware^ hw;
    if (LHMHost::tempHardware->TryGetValue(key, hw))
    {
        hw->Update();
    }

    ISensor^ sensor;
    if (LHMHost::tempSensors->TryGetValue(key, sensor))
    {
        return sensor->Value.HasValue ? (int)sensor->Value.Value : -1;
    }

    return -1;
}

/*extern "C" __declspec(dllexport) int GetDeviceTemp(DeviceType deviceType, const wchar_t* sensorName, int deviceOrder)
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
}*/

extern "C" __declspec(dllexport) void ListAllDevices()
{
    LHMHost::EnsureInitialized();

    cout << "--------------- SENDORS: ---------------" << endl;

    for each(IHardware ^ hw in LHMHost::computer->Hardware) {
        if (hw->HardwareType == HardwareType::Cpu)
        {
            cout << "CPU: " << marshal_as<string>(hw->Name) << endl;
            hw->Update();
            for each(ISensor ^ sensor in hw->Sensors)
            {
                if (sensor->SensorType == SensorType::Temperature)
                {
                    cout << " Sensor: " << marshal_as<string>(sensor->Name)
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
                    cout << " Sensor: " << marshal_as<string>(sensor->Name)
                        << " - Value: "
                        << (sensor->Value.HasValue ? to_string(sensor->Value.Value) : "N/A")
                        << endl;
                }
            }
        }
    }

    cout << "---------------- FANS: ----------------" << endl;

    InitializeSuperIOCache();
    auto controls = LHMHost::cachedControls;

    cout << "Found " << controls->Count << " fan/s" << endl;

    auto rpmSensors = LHMHost::cachedFanSensors;

    if (controls->Count != rpmSensors->Count) {
        std::cout << "Number of fan controls (" << controls->Count << ") and number of sensors (" << rpmSensors->Count << ") doesn't match!\n";
    }

    int count = System::Math::Min(controls->Count, rpmSensors->Count);
    for (int i = 0; i < count; ++i) {
        auto sensor = rpmSensors[i];
        if (sensor == nullptr || !sensor->Value.HasValue) {
            std::cout << "control: " << i << " rpm: N/A\n";
        }
        else {
            float f = sensor->Value.Value;
            int rpm = (int)System::Math::Round(f); // round to nearest int
            std::cout << "control: " << i << " rpm: " << rpm << "\n";
        }
    }

}

//For testing purposes only
extern "C" __declspec(dllexport) void TestAllFansSequence()
{
    InitializeSuperIOCache();
    auto controls = LHMHost::cachedControls;
    if (controls == nullptr) { 
        cout << "pwmControls not initialized" << endl;
        return; 
    }

    auto rpmSensors = LHMHost::cachedFanSensors;
    if (rpmSensors == nullptr) { 
        cout << "rpmSensors not initialized" << endl;
        return; 
    }

    int steps[3] = { 220, 150, 220 };

    std::cout << endl << endl << "TestAllFansSequence: found " << controls->Count << " controls\n";

    if (controls->Count != rpmSensors->Count) {
        std::cout << "Number of fan controls (" << controls->Count << ") and number of sensors (" << rpmSensors->Count << ") doesn't match!\n";
    }

    cout << endl;

    int countCtrl = controls->Count;
    int countRpm = rpmSensors->Count;
    for (int i = 0; i < countCtrl; i++) {
        cout << "Testing control index " << i;
        float value = 0.0;
        bool reinitK = true;
        bool skipCtrl = false;
        for (int k = 0; k < countRpm; k++) {
            if (skipCtrl) {
                cout << "Skipping that fan" << endl << endl;
                break;
            }
            cout << " against feedback index " << (reinitK?i:k) << endl;
            if (k == 0) { 
                if (i < countRpm) {
                    cout << "Trying coresponding sensor" << endl;
                    k = i;
                }
            } 
            else if (reinitK) {
                k = 0;
                reinitK = false;
            }
            int rpmIndex = k;
            bool match = false;
            for (int j = 0; j < sizeof(steps); j++) {
                int set = steps[j];
                SetFanPwm(i, set);
                std::this_thread::sleep_for(std::chrono::milliseconds(1700));
                if (j == 0) {
                    value = ReadFanRpm(rpmIndex);
                    if (value == 0.0) {
                        skipCtrl = true;
                        break;
                    }
                }
                else {
                    if (steps[j - 1] > steps[j]) {
                        float newValue = ReadFanRpm(rpmIndex);
                        if (newValue < value) {
                            match = true;
                        }
                        else {
                            match = false;
                        }
                        value = newValue;
                    }
                    else {
                        float newValue = ReadFanRpm(rpmIndex);
                        if (newValue > value) {
                            match = true;
                        }
                        else {
                            match = false;
                        }
                        value = newValue;
                    }
                }
            }
            if (match) {
                cout << "#### FAN " << i << " matches RPM SENSOR " << k << " ####" << endl << endl;
                break;
            }
        }
    }

    std::cout << "TestAllFansSequence: done\n";
}