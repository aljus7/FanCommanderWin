#ifndef READJSON_H
#define READJSON_H
#include <string>
#include <vector>
#include <map>

using namespace std;

struct SoftwareParam {
    int refreshInterval;
    bool oneSenseReadPc;
};

struct FanControlParam {
    // Sensor data
    vector<string> sensorNames;
    vector<string> tempPaths;
    vector<string> tempNames;
    vector<vector<pair<int, int>>> tempRpmGraphs;
    // Fan data
    vector<string> fanControlPaths;
    vector<string> fanRpmPaths;
    vector<string> fanControlerNames;
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
        vector<string> tempPath;
        vector<string> tempNames;
        vector<vector<pair<int, int>>> tempRpmGraph;

        vector<string> fanControlPath;
        vector<string> fanRpmPath;
        vector<string> fanControlerNames;
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