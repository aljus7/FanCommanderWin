#ifndef FANCONTROL_H
#define FANCONTROL_H
#include <list>
#include <string>
#include <map>
#include <fstream>
#include <vector>
#include <iostream>
#include <utility>
#include <nlohmann/json.hpp>
#include <cmath>
#include <deque>
#include <windows.h>
#include <mutex>
#include <thread>
#include "LHMBridge/LHMBridge.h"

#define LOG_AREA_FANCONTROL "Initialization"

using namespace std;

enum accelerationType {
    LINEAR,
    EXPONENTIAL
};

    DeviceType getDeviceTypeFromString(const std::string& str);

    class OneSenseReadPerCycle {
        private:
            mutex oneReadPerCycleMutex;
            unordered_map<string, int> savedValues;
        public:
            bool isValueSet(string& senseName);
            int getSetValue(string& senseName);
            void setValue(string& senseName, int val);
            void resetAllSavedValues();
    };

    class GetTemperature {
        private:
            vector<int> fanRpm;
            vector<string> tempSensorDevice; //vector<reference_wrapper<ifstream>> tempSensor;
            vector<string> tempSensorDeviceNoNumbers;
			vector<DeviceType> tempSensorDeviceType;
            vector<string> tempSensorNames;
			vector<wstring> tempSensorNamesVarchar;
			vector<string> uniqueSensorNames;
            vector<int> tempSensorIndexes; //vector<string> tempSensorPaths;
            vector<vector<pair<int, int>>> tempRpmGraph;
            string function;
            vector<int> rpms;
            int maxPwm;
            int avgTimes;
            deque<int> lastPwmValues;
            int averaging(int pwm);
			OneSenseReadPerCycle* osrpc;
			bool osrpcState;

			bool sensorReadErrorMessageShown = false;
        protected:
            void getRpm();
            int getFanRpm();
        public: 
            GetTemperature(vector<string> tempSensorDevice, vector<string> tempSensorNames, vector<int> tempSenseIndex, vector<vector<pair<int, int>>> tempRpmGraph, string function, int maxPwm, int avgTimes, OneSenseReadPerCycle* oneSensePc, bool osrpcState);
            ~GetTemperature();
    };

    class FanControl {
        private:
			int logicPwmTolerances;
            string const autoGenFileAppend = "_fanSettings";
            int fanPwmIndex;
            int fanRpmIndex;
            const string stateFilesPath = "C:\\ProgramData\\fanCommander\\data\\";
            string autoGenFileName;

            string rpmIndex;

            fstream fanSettingsAutoGenFile;
            int minPwmGood;
            int minPwm;
            int startPwmGood;
            int startPwm;
            int maxPwmGood;
            void writeMinStartPwm(fstream &file);
            void getMinStartPwm(fstream &file);
            void waitForFanRpmToStabilize();
            bool overrideMax;
            double propFactor;
            int maxRpm;
            int rpmPwmCoorelation[256];
            int hysteresisGood;

            int prevSetPwm = 0;
            atomic<bool> needsChange = true;

            int feedBackRpm;

			int spinUpDelayCountdown = 0;
			bool spinUpDelaysState = false;
			map<int, int> spinUpDelays;
			int spinUpDelayLastPwm = 0;
            int spinUpDelayLastDelay = 0;
            
			int spinDownDelayCountdown = 0;
			bool spinDownDelaysState = false;
			map<int, int> spinDownDelays;
			int spinDownDelayLastPwm = 0;
			int spinDownDelayLastDelay = 0;

			bool softAccState = false;
			bool softDeaccState = false;
			int* softAccGraph;
			int* softDeaccGraph;
            mutex turnMutex;
            condition_variable turnCV;
            atomic<int> softFanAccPrevValue = 0;
            atomic<int> softFanAccCurrValue = 0;
			atomic<bool> softFanAccThreadRunState = true;
			atomic<bool> renewSoftFanAccThreadCycle = false;
			atomic<bool> softFanAccEmidiateSet = false;
			thread softFanAccThread;
			int softAccelerationTimeframe;
			int softAccelerationSamplingInterval;
			int softAccelerationInitTreshold;
        public: 
            FanControl(int logicPwmTolerances, int fanPath, string fanNamePathOriginal, int rpmPath, int minPwm, int maxPwm, int startPwm, bool overrideMax, double propFactor, double hysteresis, 
                map<int, int> spinUpDelays, map<int, int> spinDownDelays, int* accGraph, int* deaccGraph, int softAccelerationTimeframe, int softAccelerationSamplingInterval, int softAccelerationInitTreshold);
            void setFanSpeed(int pwm);
            void softFanAcceleration(int fanPwmIndex, int targetPwm, bool state, bool emidiate);
            void getFeedbackRpm();
            ~FanControl();
    };

    class SetFans : protected FanControl, protected GetTemperature {
        private:
            int fanRpm;
        protected:

        public:
            SetFans(int logicPwmTolerances, vector<string> tempSensorDevice, vector<string> tempSensorNames, vector<int> tempSenseIndex, vector<vector<pair<int, int>>> tempRpmGraph, 
                string function, int fanPath, int rmpPath, int minPwm, int maxPwm, int startPwm, int avgTimes, bool overrideMax, double propFactor, double hysteresis, OneSenseReadPerCycle* oneSensePc, 
                bool osrpcState, string fanNamePathOriginal, map<int, int> spinUpDelays, map<int, int> spinDownDelays, int* accGraph, int* deaccGraph, int softAccelerationTimeframe, int softAccelerationSamplingInterval, int softAccelerationInitTreshold) :
            FanControl(logicPwmTolerances, fanPath, fanNamePathOriginal, rmpPath, minPwm, maxPwm, startPwm, overrideMax, propFactor, hysteresis, spinUpDelays, spinDownDelays, accGraph, deaccGraph, softAccelerationTimeframe, softAccelerationSamplingInterval, softAccelerationInitTreshold), 
            GetTemperature(tempSensorDevice, tempSensorNames, tempSenseIndex, tempRpmGraph, function, maxPwm, avgTimes, oneSensePc, osrpcState) {

            };
            void declareFanRpmFromTempGraph();
            void setFanSpeedFromDeclaredRpm();
    };

#endif