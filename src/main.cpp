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

    vector<string> sensorFixedPaths;
    vector<string> fanFixedControl = fanControlParam->fanControlPaths;
    vector<string> fanFixedRpm = fanControlParam->fanRpmPaths;

    ListAllDevices();

    OneSenseReadPerCycle *oneRead = new OneSenseReadPerCycle();

    for (int i = 0; i < fanControlParam->fanControlPaths.size(); i++) {
        
        vector<string> sensorNames;
        vector<string> sensorNamesDevices;
		vector<int> deviceIndexes;
        vector<vector<pair<int, int>>> buildTempTempRpmGraphs;

        for (int j = 0; j < fanControlParam->sensors[i].size(); j++) {
            string sensorName = fanControlParam->sensors[i][j];
            for (int k = 0; k < fanControlParam->sensorNames.size(); k++) {
                if (sensorName == fanControlParam->sensorNames[k]) {
					sensorNames.push_back(fanControlParam->sensorNames[k]);
                    sensorNamesDevices.push_back(fanControlParam->sensorNamesDevice[k]);
                    deviceIndexes.push_back(fanControlParam->deviceIndexes[k]);
                    buildTempTempRpmGraphs.push_back(fanControlParam->tempRpmGraphs[k]);
                }
            }
        }

        setFans.push_back(new SetFans(sensorNames, sensorNamesDevices, deviceIndexes, buildTempTempRpmGraphs, fanControlParam->sensorFunctions[i], fanFixedControl[i], fanFixedRpm[i],
        fanControlParam->minPwms[i], fanControlParam->maxPwms[i], fanControlParam->startPwms[i], fanControlParam->avgTimes[i], fanControlParam->overrideMax[i], fanControlParam->proportionalFactor[i], fanControlParam->hysteresis[i], oneRead, softwareParam->oneSenseReadPc, fanControlParam->fanControlPaths[i]));
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

    // deleting objects
    for (auto* fan : setFans) {
        delete fan;
    }
    delete jsonConfigReader;
    delete fanControlParam;
    delete softwareParam;
    delete oneRead;
    
    return 0;

}