#pragma once
#ifndef READJSON_H
#define READJSON_H
#include <string>
#include <vector>
#include <map>
#include <eventLogger.h>

#define LOG_AREA_JSON "Initialization"

using namespace std;

struct SoftwareParam {
    int refreshInterval;
    bool oneSenseReadPc;
};

struct FanControlParam {
    // Sensor data
    vector<string> sensorNames;
    vector<string> sensorNamesDevice;
    vector<int> deviceIndexes;
    vector<vector<pair<int, int>>> tempRpmGraphs;
    // Fan data
    vector<int> fanControlIndexs;
    vector<int> fanRpmIndexs;
    vector<vector<string>> sensors;
    vector<string> sensorFunctions;
    vector<int> avgTimes;
    vector<int> minPwms;
    vector<int> startPwms;
    vector<int> maxPwms;
    vector<bool> overrideMax;
    vector<double> proportionalFactor;
    vector<double> hysteresis;
};

class JsonConfigReader {
    private:
        string configPath;

        int refresh_interval;
        bool oneSenseReadPc;

        vector<string> name;
        vector<string> sensorName;
        vector<int> deviceIndex;
        vector<vector<pair<int, int>>> tempRpmGraph;

        vector<int> fanControlIndex;
        vector<int> fanRpmIndex;
        vector<vector<string>> sensors;
        vector<string> sensorFunc;
        vector<int> avgTimes;
        vector<int> minPwm;
        vector<int> startPwm;
        vector<int> maxPwm;
        vector<bool> overrideMax;
        vector<double> proportionalFactor;
        vector<double> hysteresis;
    public:
        JsonConfigReader(string configPath);
        void readJsonConfig();
        void returnJsonConfig(FanControlParam* fanControlParam, SoftwareParam* softwareParam);
        void printParsedJsonInStdout(FanControlParam* fanControlParam, SoftwareParam* softwareParam);
};

#endif