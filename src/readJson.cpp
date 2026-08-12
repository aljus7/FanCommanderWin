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
		errorLog("Config json is invalid: \n" + std::string(e.what()));
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
        errorLog("'Settings' object in config should exist.");
        throw invalid_argument("'Settings' object in config should exist.");
    }

    if (conf.contains("tempSensors") && conf["tempSensors"].is_array()) {
        for (const auto &sensor : conf["tempSensors"]) {
            this->name.push_back(sensor["sensor"].get<string>());
            this->sensorName.push_back(sensor["sensorName"].get<string>());
            this->deviceIndex.push_back(sensor["deviceIndex"].get<int>());
            vector<pair<int, int>> temps;
            for (const auto &graph : sensor["graph"]) {
                temps.push_back(make_pair(graph["temp"].get<int>(), graph["pwm"].get<int>()));
            }
            this->tempRpmGraph.push_back(temps);
        }
    } else {
        errorLog("'tempSensors' array sould exist in config.");
        throw invalid_argument("'tempSensors' array sould exist in config.");
    }

    if (conf.contains("fans") && conf["fans"].is_array()) {
        for (const auto& fan : conf["fans"]) {
            this->fanControlIndex.push_back(fan["fanControlIndex"].get<int>());
            this->fanRpmIndex.push_back(fan["fanRpmIndex"].get<int>());
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
            
            vector<tuple<int, int, int>> spinUpDelays;
            if (fan.contains("spinUpDelay") && fan["spinUpDelay"].is_array()) {
                for (const auto &delay : fan["spinUpDelay"]) {
                    spinUpDelays.push_back(make_tuple(delay["fromPwm"].get<int>(), delay["toPwm"].get<int>(), delay["duration"].get<int>()));
                }
            } else {
				spinUpDelays = vector<tuple<int, int, int>>(); // Empty vector if spinUpDelay is not present
            }
            this->spinUpDelays.push_back(spinUpDelays);

            vector<tuple<int, int, int>> spinDownDelays;
            if (fan.contains("spinDownDelay") && fan["spinDownDelay"].is_array()) {
                for (const auto& delay : fan["spinDownDelay"]) {
                    spinDownDelays.push_back(make_tuple(delay["fromPwm"].get<int>(), delay["toPwm"].get<int>(), delay["duration"].get<int>()));
                }
            }
            else {
                spinDownDelays = vector<tuple<int, int, int>>(); // Empty vector if spinDownDelay is not present
            }
            this->spinDownDelays.push_back(spinDownDelays);

        }
    } else {
        errorLog("'fans' array sould exist in config.");
        throw invalid_argument("'fans' array sould exist in config.");
    }
}

void JsonConfigReader::returnJsonConfig(FanControlParam* fanControlParam, SoftwareParam* softwareParam) {
    softwareParam->refreshInterval = this->refresh_interval;
    softwareParam->oneSenseReadPc = this->oneSenseReadPc;

    if (this->name.empty()) {
		errorLog("There are no sensor names set!");
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
            errorLog("Duplicate " + nameOne + " sensor name found! Names of the sensors should be unique.");
            cerr << "Duplicate " << nameOne << " sensor name found!" << endl;
            throw std::invalid_argument("Duplicate sensor name found: " + nameOne 
                + " Names of the sensors should be unique.");
        }
    }
    fanControlParam->sensorNames = this->name;
    fanControlParam->sensorNamesDevice = this->sensorName;
    fanControlParam->deviceIndexes = this->deviceIndex;
    fanControlParam->tempRpmGraphs = this->tempRpmGraph;

    fanControlParam->fanControlIndexs = this->fanControlIndex;
    fanControlParam->fanRpmIndexs = this->fanRpmIndex;
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
			errorLog("Hysteresis can be set between value 0 - 0.3");
            throw std::invalid_argument("Hysteresis can be set between value 0 - 0.3");
        }
    }
    fanControlParam->hysteresis = this->hysteresis;

    for (auto& spinUpDelaysPerFan : this->spinUpDelays) {
        map<int, int> spinUpDelaysMap;
        if (!spinUpDelaysPerFan.empty()) {
            for (int i = 0; i <= 255; i++) {
                bool isInRange = false;
                int valueToSet = 0;
                for (const auto& delay : spinUpDelaysPerFan) {
                    if (i >= get<0>(delay) && i <= get<1>(delay)) {
                        isInRange = true;
                        valueToSet = get<2>(delay);
                        valueToSet = (valueToSet < 0) ? 0 : valueToSet;
                        valueToSet = valueToSet / this->refresh_interval;
                        valueToSet = (valueToSet < 1) ? 1 : valueToSet;
                        break;
                    }
                }
                if (isInRange) {
                    spinUpDelaysMap[i] = valueToSet;
                }
                else {
                    spinUpDelaysMap[i] = 0;
                }
            }
        }
        else {
            spinUpDelaysMap = map<int, int>();
        }
        fanControlParam->spinUpDelays.push_back(spinUpDelaysMap);
    }

    for (auto& spinDownDelaysPerFan : this->spinDownDelays) {
        map<int, int> spinDownDelaysMap;
        if (!spinDownDelaysPerFan.empty()) {
            for (int i = 255; i >= 0; i--) {
                bool isInRange = false;
                int valueToSet = 0;
                for (const auto& delay : spinDownDelaysPerFan) {
                    if (i >= get<0>(delay) && i <= get<1>(delay)) {
                        isInRange = true;
                        valueToSet = get<2>(delay);
                        valueToSet = (valueToSet < 0) ? 0 : valueToSet;
                        valueToSet = valueToSet / this->refresh_interval;
                        valueToSet = (valueToSet < 1) ? 1 : valueToSet;
                        break;
                    }
                }
                if (isInRange) {
                    spinDownDelaysMap[i] = valueToSet;
                }
                else {
                    spinDownDelaysMap[i] = 0;
                }
            }
        }
        else {
            spinDownDelaysMap = map<int, int>();
        }
        fanControlParam->spinDownDelays.push_back(spinDownDelaysMap);
    }
}

void JsonConfigReader::printParsedJsonInStdout(FanControlParam* fcp, SoftwareParam* sp) {
	string area = LOG_AREA_JSON;

    cout << endl << "-------- Found settings: --------" << endl;
    addLoggingAreaMessage(area, "-------- Found settings: --------");
    cout << "Refresh interval: " << sp->refreshInterval << endl;
	addLoggingAreaMessage(area, "Refresh interval: " + to_string(sp->refreshInterval));
    cout << "One Sensor Read Per Cycle optimization: " << (sp->refreshInterval ? "ON" : "OFF");
	addLoggingAreaMessage(area, "One Sensor Read Per Cycle optimization: " + string((sp->refreshInterval ? "ON" : "OFF")));
    cout << endl << "Sensors:";
	addLoggingAreaMessage(area, "Sensors:");
    for (int i = 0; i < fcp->sensorNames.size(); i++) {
        cout << endl << "\tSensor: " << fcp->sensorNames[i] << endl;
		addLoggingAreaMessage(area, "\tSensor: " + fcp->sensorNames[i]);
        cout << "\tSensor hardware name: " << fcp->sensorNamesDevice[i] << endl;
		addLoggingAreaMessage(area, "\tSensor hardware name: " + fcp->sensorNamesDevice[i]);
        cout << "\tDevice index: " << fcp->deviceIndexes[i] << endl;
		addLoggingAreaMessage(area, "\tDevice index: " + to_string(fcp->deviceIndexes[i]));
        cout << "\tTemp / Rpm graph:" << endl;
		addLoggingAreaMessage(area, "\tTemp / Rpm graph:");
        vector<pair<int, int>> vals = fcp->tempRpmGraphs[i];
        for (const auto &pair : vals) {
			addLoggingAreaMessage(area, "\t\ttemp: " + to_string(pair.first) + " pwm: " + to_string(pair.second));
            cout << "\t\ttemp: " << pair.first << " pwm: " << pair.second << endl;
        }
    }
    cout << endl << "Fans:";
	addLoggingAreaMessage(area, "Fans:");
    for (int i = 0; i < fcp->fanControlIndexs.size(); i++) {
        cout << endl << "Fan" << i << ":" << endl;
		addLoggingAreaMessage(area, "Fan" + to_string(i) + ":");
        cout << "\tFan control path: " << fcp->fanControlIndexs[i] << endl;
		addLoggingAreaMessage(area, "\tFan control path: " + to_string(fcp->fanControlIndexs[i]));
        cout << "\tFan rpm path: " << fcp->fanRpmIndexs[i] << endl;
		addLoggingAreaMessage(area, "\tFan rpm path: " + to_string(fcp->fanRpmIndexs[i]));
        cout << "\tFan uses sensors: ";
		addLoggingAreaMessage(area, "\tFan uses sensors: ");
        vector<string> sensorss = fcp->sensors[i];
        for(const string &sensor : sensorss) {
			addLoggingAreaMessage(area, "\t\t" + sensor);
            cout << sensor << ", ";
        }
        cout << endl;
        cout << "\tSensor function: " << fcp->sensorFunctions[i] << endl;
		addLoggingAreaMessage(area, "\tSensor function: " + fcp->sensorFunctions[i]);
        cout << "\tAveraging over: " << fcp->avgTimes[i] << " times" << endl;
		addLoggingAreaMessage(area, "\tAveraging over: " + to_string(fcp->avgTimes[i]) + " times");
        cout << "\tMin PWM: " << fcp->minPwms[i] << endl;
		addLoggingAreaMessage(area, "\tMin PWM: " + to_string(fcp->minPwms[i]));
        cout << "\tStart PWM: " << fcp->startPwms[i] << endl;
		addLoggingAreaMessage(area, "\tStart PWM: " + to_string(fcp->startPwms[i]));
        cout << "\tMax PWM: " << fcp->maxPwms[i] << endl;
		addLoggingAreaMessage(area, "\tMax PWM: " + to_string(fcp->maxPwms[i]));
        cout << "\tOverride max value: " << ((fcp->overrideMax[i] == true) ? "ON" : "OFF") << endl;
		addLoggingAreaMessage(area, "\tOverride max value: " + string((fcp->overrideMax[i] == true) ? "ON" : "OFF"));
        cout << "\tProportional fan control state (proportionalFactor>0 - ON; proportionalFactor<0 - OFF): " << ((fcp->proportionalFactor[i] > 0) ? "ON" : "OFF") << endl;
		addLoggingAreaMessage(area, "\tProportional fan control state (proportionalFactor>0 - ON; proportionalFactor<0 - OFF): " + string((fcp->proportionalFactor[i] > 0) ? "ON" : "OFF"));
        cout << "\tProportional factor value: " << ((fcp->proportionalFactor[i] == 0) ? "OFF" :  to_string(fcp->proportionalFactor[i])) << endl;
		addLoggingAreaMessage(area, "\tProportional factor value: " + ((fcp->proportionalFactor[i] == 0) ? "OFF" : to_string(fcp->proportionalFactor[i])));
        cout << "\tHysteresis percentage: " << fcp->hysteresis[i]*100 << "%" << endl;
		addLoggingAreaMessage(area, "\tHysteresis percentage: " + to_string(fcp->hysteresis[i] * 100) + "%");
		cout << "\tSpin up delays: " << endl;
		addLoggingAreaMessage(area, "\tSpin delays: ");
		map<int, int> spinUpDelaysMap = fcp->spinUpDelays[i];
        if (spinUpDelaysMap.empty()) {
			cout << "\t\tNo spin up delays defined." << endl;
            addLoggingAreaMessage(area, "\t\tNo spin up delays defined.");
        }
        else {
            cout << "\t\tSpin up delays defined." << endl;
            addLoggingAreaMessage(area, "\t\tSpin up delays defined.");
        }
        map<int, int> spinDownDelaysMap = fcp->spinDownDelays[i];
        if (spinDownDelaysMap.empty()) {
            cout << "\t\tNo spin down delays defined." << endl;
            addLoggingAreaMessage(area, "\t\tNo spin down delays defined.");
        }
        else {
            cout << "\t\tSpin down delays defined." << endl;
            addLoggingAreaMessage(area, "\t\tSpin down delays defined.");
        }
    }
    cout << endl << endl;

}    