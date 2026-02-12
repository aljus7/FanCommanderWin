#include "readJson.h"
#include "fanControl.h"
#include <chrono>
#include <thread>
#include <atomic>
#include <csignal>
#include <regex>
#include <filesystem>
#include <fstream>
#include <iostream>
using namespace std;
//const string jsonConfigLocation = "config.json";
const string jsonConfigLocation = "/etc/fanCommander/config.json";

atomic<bool> keepRunning(true);

void signalHandler(int signum) {
    cout << "\nInterrupt signal (" << signum << ") received.\n";
    keepRunning = false;
}

#include <string>
#include <cctype>

pair<string, string> split_at_first_digit(const string& s) {
    for (size_t i = 0; i < s.size(); i++) {
        if (isdigit(s[i])) {
            return { s.substr(0, i), s.substr(i) };
        }
    }
    return { s, "" };
}

string find_hwmon(const string& target_name, const string& targetPath) {
    
    string basePath;

    if (target_name.empty()) {
        cout << "Info: Empty target name provided to find_hwmon(). Returning provided path as fixed: " << targetPath << endl;
        return targetPath;
    }

    if (targetPath.empty()) {
        cout << "Info: Empty target path provided to find_hwmon(). Target name: " << target_name << endl;
        throw std::invalid_argument("Empty target path provided to find_hwmon().");
    }

    if (regex_match(targetPath, regex(".*hwmon[0-9].*"))) {
        basePath = split_at_first_digit(targetPath).first;
        size_t pos = basePath.find("/hwmon");
        if (pos != string::npos) {
            basePath.replace(pos, 6, ""); // remove "/hwmon"
        } else {
            throw std::invalid_argument("Provided path \"" + targetPath + "\" is not a valid hwmon path.");
        }
    } else {
        cout << "Info: Provided path \"" << targetPath << "\" is not a hwmon path." << endl;
        return targetPath;
    }

    for (const auto& entry : filesystem::directory_iterator(basePath)) {
        string name_path = entry.path().string() + "/name";

        ifstream name_file(name_path);
        if (!name_file.is_open())
            continue;

        string name;
        getline(name_file, name);

        if (name == target_name) {
            // Find the suffix after hwmonX
            size_t hwmon_pos = targetPath.find("/hwmon");
            if (hwmon_pos != string::npos) {
                size_t digit_pos = targetPath.find_first_of("0123456789", hwmon_pos + 6);
                if (digit_pos != string::npos) {
                    size_t suffix_pos = targetPath.find_first_not_of("0123456789", digit_pos);
                    string suffix = (suffix_pos != string::npos) ? targetPath.substr(suffix_pos) : "";
                    return entry.path().string() + suffix;
                }
            }
            return entry.path().string(); // fallback
        }
    }

    cout << "Mild error: Could not find hwmon directory for sensor name \"" << target_name << "\". Using provided path instead: " << targetPath << endl;
    return targetPath;
}

int main() {
    signal(SIGINT, signalHandler);  // Handle Ctrl+C
    signal(SIGTERM, signalHandler); // Handle kill/pkill

    SoftwareParam *softwareParam = new SoftwareParam();
    FanControlParam *fanControlParam = new FanControlParam();

    JsonConfigReader *jsonConfigReader = new JsonConfigReader(jsonConfigLocation);
    jsonConfigReader->readJsonConfig();
    jsonConfigReader->returnJsonConfig(fanControlParam, softwareParam);
    jsonConfigReader->printParsedJsonInStdout(fanControlParam, softwareParam);
    
    vector<SetFans*> setFans;
    vector<string> fanModePaths;

    vector<string> sensorFixedPaths;
    vector<string> fanFixedControl;
    vector<string> fanFixedRpm;

    for (const auto& sensorName : fanControlParam->tempNames) {
        size_t index = &sensorName - &fanControlParam->tempNames[0];
        string fixedPath = find_hwmon(sensorName, fanControlParam->tempPaths[index]);
        sensorFixedPaths.push_back(fixedPath);
    }

    for (const auto& fanControlPath : fanControlParam->fanControlPaths) {
        size_t index = &fanControlPath - &fanControlParam->fanControlPaths[0];
        string fixedPath = find_hwmon(fanControlParam->fanControlerNames[index], fanControlPath);
        fanFixedControl.push_back(fixedPath);
    }

    for (const auto& fanRpmPath : fanControlParam->fanRpmPaths) {
        size_t index = &fanRpmPath - &fanControlParam->fanRpmPaths[0];
        string fixedPath = find_hwmon(fanControlParam->fanControlerNames[index], fanRpmPath);
        fanFixedRpm.push_back(fixedPath);
    }

    TempSensorServer *senServ = new TempSensorServer(sensorFixedPaths, fanControlParam->sensorNames);
    OneSenseReadPerCycle *oneRead = new OneSenseReadPerCycle();

    for (int i = 0; i < fanControlParam->fanControlPaths.size(); i++) {
        
        vector<string> buildTempTempPaths;
        vector<vector<pair<int, int>>> buildTempTempRpmGraphs;

        fanModePaths.push_back(fanFixedControl[i] + "_enable");
        ofstream modeFile(fanFixedControl[i] + "_enable");

        if (modeFile.is_open()) {
            modeFile.seekp(0);
            modeFile << 1 << endl;
            modeFile.close();
        } else {
            throw std::runtime_error("Failed to open fan mode file: \"" + fanFixedControl[i] + "_enable\"");
        }

        for (int j = 0; j < fanControlParam->sensors[i].size(); j++) {
            string sensorName = fanControlParam->sensors[i][j];
            for (int k = 0; k < fanControlParam->sensorNames.size(); k++) {
                if (sensorName == fanControlParam->sensorNames[k]) {
                    buildTempTempPaths.push_back(sensorFixedPaths[k]);
                    buildTempTempRpmGraphs.push_back(fanControlParam->tempRpmGraphs[k]);
                }
            }
        }

        setFans.push_back(new SetFans(buildTempTempPaths, buildTempTempRpmGraphs, fanControlParam->sensorFunctions[i], fanFixedControl[i], fanFixedRpm[i], 
        fanControlParam->minPwms[i], fanControlParam->maxPwms[i], fanControlParam->startPwms[i], fanControlParam->avgTimes[i], senServ, fanControlParam->overrideMax[i], fanControlParam->proportionalFactor[i], fanControlParam->hysteresis[i], oneRead, softwareParam->oneSenseReadPc, fanControlParam->fanControlPaths[i]));
    }

    int balancedRefreshTime = 0;
    if (setFans.size() > 0) {
        balancedRefreshTime = softwareParam->refreshInterval / setFans.size();
    } else {
        throw std::runtime_error("No fans found!");
    }

    while (keepRunning) {
        for (auto &fan : setFans) {
            fan->declareFanRpmFromTempGraph();
            fan->setFanSpeedFromDeclaredRpm();
            this_thread::sleep_for(std::chrono::milliseconds(balancedRefreshTime));
        }
        if (softwareParam->oneSenseReadPc) {
            oneRead->resetAllSavedValues();
        }
    }

    std::cout << "Exiting...\n";
    
    // set fan mode to 2 which means automatic bios control
    for (const auto &fanModePath : fanModePaths) {

        ofstream modeFile(fanModePath);

        if (modeFile.is_open()) {
            modeFile.seekp(0);
            modeFile << 2 << endl;
            modeFile.close();
        } else {
            throw std::runtime_error("Failed to open fan mode file.");
        }

    }

    // deleting objects
    for (auto* fan : setFans) {
        delete fan;
    }
    delete senServ;
    delete jsonConfigReader;
    delete fanControlParam;
    delete softwareParam;
    delete oneRead;
    
    return 0;

}