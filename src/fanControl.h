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

using namespace std;

    class OneSenseReadPerCycle {
        private:
            unordered_map<string, int> savedValues;
            int nextReturnValue;
        public:
            bool isValueSet(string& senseName);
            int& getSetValue();
            void setValue(string& senseName, int val);
            void resetAllSavedValues();
    };

    class TempSensorServer {
        private:
            list<ifstream> tempSensorStreams;
            unordered_map<string, reference_wrapper<ifstream>> tempSensor;
            vector<pair<string, string>> tempSensorNamePathCoor;
        protected:

        public:
            TempSensorServer(vector<string> tempPaths, vector<string> sensorName);
            ifstream& getTempSenseIfstream(const string &sensorPath);
            string getTempSenseName(const string &sensorPath);
    };

    class GetTemperature {
        private:
            vector<int> fanRpm;
            vector<reference_wrapper<ifstream>> tempSensor;
            vector<string> tempSensorNames;
            vector<string> tempSensorPaths;
            vector<vector<pair<int, int>>> tempRpmGraph;
            string function;
            vector<int> rpms;
            int maxPwm;
            int avgTimes;
            deque<int> lastPwmValues;
            int averaging(int pwm);
            OneSenseReadPerCycle* osrpc;
            bool osrpcState;
        protected:
            void getRpm();
            int getFanRpm();
        public: 
            GetTemperature(vector<string> tempPath, vector<vector<pair<int, int>>> tempRpmGraph, string function, int maxPwm, int avgTimes, TempSensorServer* tmpSrv, OneSenseReadPerCycle* oneSensePc, bool osrpcState); 
            ~GetTemperature();
    };

    class FanControl {
        private:
            string const autoGenFileAppend = "_fanSettings";
            string fanNamePath;
            string fanNamePathOriginal;
            const string stateFilesPath = "/var/lib/fanCommander/";

            string rpmPath;
 
            ofstream fanControl;
            ifstream rpmSensor;
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
            bool needsChange = true;

            int feedBackRpm;
        public: 
            FanControl(string fanPath, string fanNamePathOriginal, string rpmPath, int minPwm, int maxPwm, int startPwm, bool overrideMax, double propFactor, double hysteresis);
            void setFanSpeed(int pwm);
            void getFeedbackRpm();
            ~FanControl();
    };

    class SetFans : protected FanControl, protected GetTemperature {
        private:
            int fanRpm;
        protected:

        public:
            SetFans(vector<string> tempPath, vector<vector<pair<int, int>>> tempRpmGraph, string function, string fanPath, string rmpPath, int minPwm, int maxPwm, int startPwm, int avgTimes, TempSensorServer *tmpSrv, bool overrideMax, double propFactor, double hysteresis, OneSenseReadPerCycle* osrpc, bool osrpcState, string fanNamePathOriginal) : 
            FanControl(fanPath, fanNamePathOriginal, rmpPath, minPwm, maxPwm, startPwm, overrideMax, propFactor, hysteresis), GetTemperature(tempPath, tempRpmGraph, function, maxPwm, avgTimes, tmpSrv, osrpc, osrpcState) {

            };
            void declareFanRpmFromTempGraph();
            void setFanSpeedFromDeclaredRpm();
    };

#endif