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
#include "LHMBridge/LHMBridge.h"
using namespace std;
//const string jsonConfigLocation = "config.json";
const string jsonConfigLocation = "config.json";

atomic<bool> keepRunning(true);

void signalHandler(int signum) {
    cout << "\nInterrupt signal (" << signum << ") received.\n";
    keepRunning = false;
}

#include <string>
#include <cctype>}

int main() {
    //signal(SIGINT, signalHandler);  // Handle Ctrl+C
    //signal(SIGTERM, signalHandler); // Handle kill/pkill

    std::cout << "START\n" << std::flush;

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

    ListAllDevices();
    
    return 0;

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