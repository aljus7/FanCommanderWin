#include "readJson.h"
#include <iostream>
#include <nlohmann/json.hpp>
#include <fstream>
#include <stdexcept>
#include <utility>
using namespace std;
using json = nlohmann::json;

JsonConfigReader::JsonConfigReader(string configPath) {
    this->configPath = configPath;
}

void JsonConfigReader::readJsonConfig() {
    string jsonConfig;
    ifstream jsonConf(this->configPath);
    if (jsonConf.is_open()) {
        string line;
        while (getline (jsonConf, line)) {
            jsonConfig += line;
        }
        jsonConf.close();
    }
    json conf;
    try {
        conf = json::parse(jsonConfig);
    } catch (const json::parse_error& e) {
        throw std::invalid_argument("Config json is invalid: \n" + std::string(e.what()));
    }
    
    if(conf.contains("settings")) {
        if (conf["settings"].contains("refreshInterval"))
            this -> refresh_interval = conf["settings"]["refreshInterval"].get<int>();
        else
            this -> refresh_interval = 2;
        if (conf["settings"].contains("oneSensorReadPerCycle"))
            this->oneSenseReadPc = conf["settings"]["oneSensorReadPerCycle"].get<bool>();
        else
            this->oneSenseReadPc = false;
    } else {
        throw invalid_argument("'Settings' object in config should exist.");
    }

    if (conf.contains("tempSensors") && conf["tempSensors"].is_array()) {
        for (const auto &sensor : conf["tempSensors"]) {
            this->name.push_back(sensor["sensor"].get<string>());
            this->tempPath.push_back(sensor["path"].get<string>());
            this->tempNames.push_back(sensor["deviceName"].get<string>());
            vector<pair<int, int>> temps;
            for (const auto &graph : sensor["graph"]) {
                temps.push_back(make_pair(graph["temp"].get<int>(), graph["pwm"].get<int>()));
            }
            this->tempRpmGraph.push_back(temps);
        }
    } else {
        throw invalid_argument("'tempSensors' array sould exist in config.");
    }

    if (conf.contains("fans") && conf["fans"].is_array()) {
        for (const auto& fan : conf["fans"]) {
            this->fanControlPath.push_back(fan["fanControlPath"].get<string>());
            this->fanRpmPath.push_back(fan["fanRpmPath"].get<string>());
            this->fanControlerNames.push_back(fan["deviceName"].get<string>());
            vector<string> sensors;
            for (const auto &sensor : fan["sensors"]) {
                sensors.push_back(sensor.get<string>());
            }
            this->sensors.push_back(sensors);
            this->sensorFunc.push_back(fan["sensorFunction"].get<string>());
            this->avgTimes.push_back(fan["averageSampleSize"].get<int>());
            this->minPwm.push_back(fan["minPwm"].get<int>());
            this->startPwm.push_back(fan["startPwm"].get<int>());
            this->maxPwm.push_back(fan["maxPwm"].get<int>());
            this->overrideMax.push_back(fan["overrideMax"].get<bool>());
            this->proportionalFactor.push_back(fan["proportionalFactor"].get<double>());
            this->hysteresis.push_back(fan["hysteresis"].get<double>());
        }
    } else {
        throw invalid_argument("'fans' array sould exist in config.");
    }
}

void JsonConfigReader::returnJsonConfig(FanControlParam* fanControlParam, SoftwareParam* softwareParam) {
    softwareParam->refreshInterval = this->refresh_interval;
    softwareParam->oneSenseReadPc = this->oneSenseReadPc;

    if (this->name.empty()) {
        throw std::invalid_argument("There are no sensor names set!");
    }
    for (const auto& nameOne : this->name) {
        int same = 0;
        for(const auto& nameTwo : this->name) {
            if (nameOne == nameTwo) {
                ++same;
            }
        }
        if (same > 1) {
            cerr << "Duplicate " << nameOne << " sensor name found!" << endl;
            throw std::invalid_argument("Duplicate sensor name found: " + nameOne 
                + " Names of the sensors should be unique.");
        }
    }
    fanControlParam->sensorNames = this->name;
    fanControlParam->tempPaths = this->tempPath;
    fanControlParam->tempNames = this->tempNames;
    fanControlParam->tempRpmGraphs = this->tempRpmGraph;

    fanControlParam->fanControlPaths = this->fanControlPath;
    fanControlParam->fanRpmPaths = this->fanRpmPath;
    fanControlParam->fanControlerNames = this->fanControlerNames;
    fanControlParam->sensors = this->sensors;
    fanControlParam->sensorFunctions = this->sensorFunc;
    fanControlParam->avgTimes = this->avgTimes;
    fanControlParam->minPwms = this->minPwm;
    fanControlParam->startPwms = this->startPwm;
    fanControlParam->maxPwms = this->maxPwm;
    fanControlParam->overrideMax = this->overrideMax;
    fanControlParam->proportionalFactor = this->proportionalFactor;
    for (const auto& hyst : this->hysteresis) {
        if (hyst < 0 && hyst > 0.3) {
            throw std::invalid_argument("Hysteresis can be set between value 0 - 0.3");
        }
    }
    fanControlParam->hysteresis = this->hysteresis;
}

void JsonConfigReader::printParsedJsonInStdout(FanControlParam* fcp, SoftwareParam* sp) {

    cout << endl << "-------- Found settings: --------" << endl;
    cout << "Refresh interval: " << sp->refreshInterval << endl;
    cout << "One Sensor Read Per Cycle optimization: " << (sp->refreshInterval ? "ON" : "OFF");
    cout << endl << "Sensors:";
    for (int i = 0; i < fcp->sensorNames.size(); i++) {
        cout << endl << "\tSensor: " << fcp->sensorNames[i] << endl;
        cout << "\tSensor path: " << fcp->tempPaths[i] << endl;
        cout << "\tDevice name: " << fcp->tempNames[i] << endl;
        cout << "\tTemp / Rpm graph:" << endl;
        vector<pair<int, int>> vals = fcp->tempRpmGraphs[i];
        for (const auto &pair : vals) {
            cout << "\t\ttemp: " << pair.first << " pwm: " << pair.second << endl;
        }
    }
    cout << endl << "Fans:";
    for (int i = 0; i < fcp->fanControlPaths.size(); i++) {
        cout << endl << "Fan" << i << ":" << endl;
        cout << "\tFan control path: " << fcp->fanControlPaths[i] << endl;
        cout << "\tFan rpm path: " << fcp->fanRpmPaths[i] << endl;
        cout << "\tFan device name: " << fcp->fanControlerNames[i] << endl;
        cout << "\tFan uses sensors: ";
        vector sensorss = fcp->sensors[i];
        for(string sensor : sensorss) {
            cout << sensor << ", ";
        }
        cout << endl;
        cout << "\tSensor function: " << fcp->sensorFunctions[i] << endl;
        cout << "\tAveraging over: " << fcp->avgTimes[i] << " times" << endl;
        cout << "\tMin PWM: " << fcp->minPwms[i] << endl;
        cout << "\tStart PWM: " << fcp->startPwms[i] << endl;
        cout << "\tMax PWM: " << fcp->maxPwms[i] << endl;
        cout << "\tOverride max value: " << ((fcp->overrideMax[i] == true) ? "ON" : "OFF") << endl;
        cout << "\tProportional fan control state (proportionalFactor>0 - ON; proportionalFactor<0 - OFF): " << ((fcp->proportionalFactor[i] > 0) ? "ON" : "OFF") << endl;
        cout << "\tProportional factor value: " << ((fcp->proportionalFactor[i] == 0) ? "OFF" :  to_string(fcp->proportionalFactor[i])) << endl;
        cout << "\tHysteresis percentage: " << fcp->hysteresis[i]*100 << "%" << endl;
    }
    cout << endl << endl;

}    