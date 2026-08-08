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
#include <string>
#include <cctype>
#include "eventLogger.h"

#define LOG_AREA_INIT "Initialization"

using namespace std;
const string jsonConfigLocation = "C:\\ProgramData\\fanCommander\\config.json";

atomic<bool> keepRunning(true);

BOOL WINAPI ConsoleHandler(DWORD event) {
    switch (event) {
    case CTRL_C_EVENT:
        std::cout << "\nCtrl+C received\n";
        keepRunning = false;
        return TRUE;

    case CTRL_BREAK_EVENT:
        std::cout << "\nCtrl+Break received\n";
        keepRunning = false;
        return TRUE;

    case CTRL_CLOSE_EVENT:
        std::cout << "\nConsole is closing\n";
        keepRunning = false;
        return TRUE;

    case CTRL_LOGOFF_EVENT:
    case CTRL_SHUTDOWN_EVENT:
        std::cout << "\nSystem is shutting down/logging off\n";
        keepRunning = false;
        return TRUE;
    }
    return FALSE;
}

int main(int argc, char** argv) {
    SetConsoleCtrlHandler(ConsoleHandler, TRUE);

    bool testFans = false;
    bool continueAfterTest = false;

    for (int i = 1; i < argc; i++) {
        string a = argv[i];
        if (a == "--testFans" || a == "-tf") {
            testFans = true;
        }
        if (a == "--testFansContinue" || a == "-tfc") {
            testFans = true;
            continueAfterTest = true;
        }
    }

    ListAllDevices();
    if (testFans) {
        TestAllFansSequence();
        if (!continueAfterTest) {
            return 0;
        }
    }
    logLhmArea();

    SoftwareParam *softwareParam = new SoftwareParam();
    FanControlParam *fanControlParam = new FanControlParam();

    JsonConfigReader *jsonConfigReader = new JsonConfigReader(jsonConfigLocation);
    jsonConfigReader->readJsonConfig();
    jsonConfigReader->returnJsonConfig(fanControlParam, softwareParam);
    jsonConfigReader->printParsedJsonInStdout(fanControlParam, softwareParam);
    
    vector<SetFans*> setFans;

    vector<int> fanControl = fanControlParam->fanControlIndexs;
    vector<int> fanRpm = fanControlParam->fanRpmIndexs;

    OneSenseReadPerCycle *oneRead = new OneSenseReadPerCycle();

    for (int i = 0; i < fanControlParam->fanControlIndexs.size(); i++) {
        
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

        string uniqueFanCtrlName = to_string(fanControlParam->fanControlIndexs[i]) + "_" + to_string(fanControlParam->fanRpmIndexs[i]);

        setFans.push_back(new SetFans(sensorNames, sensorNamesDevices, deviceIndexes, buildTempTempRpmGraphs, fanControlParam->sensorFunctions[i], fanControl[i], fanRpm[i],
        fanControlParam->minPwms[i], fanControlParam->maxPwms[i], fanControlParam->startPwms[i], fanControlParam->avgTimes[i], fanControlParam->overrideMax[i], fanControlParam->proportionalFactor[i], fanControlParam->hysteresis[i], oneRead, softwareParam->oneSenseReadPc, uniqueFanCtrlName));
    }

    int balancedRefreshTime = 0;
    if (setFans.size() > 0) {
        balancedRefreshTime = softwareParam->refreshInterval / setFans.size();
    } else {
		errorLog("No fans found!");
        throw std::runtime_error("No fans found!");
    }

    logLoggingArea(LOG_AREA_INIT);

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