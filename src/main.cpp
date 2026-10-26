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
#include <mutex>
#include <atomic>
#include <thread>
#include <cmath>
#include "eventLogger.h"
#include "dataProvider.h"
//#include <crtdbg.h>

#define LOG_AREA_INIT "Initialization"

using namespace std;
const string jsonConfigLocation = "C:\\ProgramData\\fanCommander\\config.json";
//const string jsonConfigLocation = "config.json";

int* createSoftAccelerationGraph(string, double, double);
string plotSoftAccelerationGraph(int*);
void helpText();
void enableConsole();
int pressReturnToClose();

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
    /*_CrtSetDbgFlag(
        _CRTDBG_ALLOC_MEM_DF |
        _CRTDBG_CHECK_ALWAYS_DF
    );*/

    SetConsoleCtrlHandler(ConsoleHandler, TRUE);

    bool testFans = false;
    bool continueAfterTest = false;
	bool accelerationGraphs = false;
	bool showConsoleOutput = false;
	bool showHelp = false;

    for (int i = 1; i < argc; i++) {
        string a = argv[i];
        if (a == "--testFans" || a == "-tf") {
            testFans = true;
            showConsoleOutput = true;
        }
        if (a == "--testFansContinue" || a == "-tfc") {
            testFans = true;
            continueAfterTest = true;
        }
        if (a == "--accelerationGraphs" || a == "-ag") {
            accelerationGraphs = true;
            showConsoleOutput = true;
        }
        if (a == "--enableConsole" || a == "-ec") {
            showConsoleOutput = true;
        }
        if (a == "--help" || a == "-h") {
            showConsoleOutput = true;
			showHelp = true;
        }
    }

    if (showConsoleOutput)
		enableConsole();

    if (showHelp) {
        helpText();
        return pressReturnToClose();
    }

    ListAllDevices();
    if (testFans) {
        TestAllFansSequence();
        if (!continueAfterTest) {
            return pressReturnToClose();
        }
    }
    logLhmArea();

    SoftwareParam *softwareParam = new SoftwareParam();
    FanControlParam *fanControlParam = new FanControlParam();

    JsonConfigReader *jsonConfigReader = new JsonConfigReader(jsonConfigLocation);
    jsonConfigReader->readJsonConfig();
    jsonConfigReader->returnJsonConfig(fanControlParam, softwareParam);
    jsonConfigReader->printParsedJsonInStdout(fanControlParam, softwareParam);

    vector<int*> softAccelerationGraph;
	vector<int*> softDeaccelerationGraph;
    vector<string> softAccelerationGraphPlot;
	int deaccelerationK = 0.2;
	int deaccelerationA = 13;

    for (int i = 0; i < fanControlParam->fanControlIndexs.size(); i++) {
        if (fanControlParam->softAccelerationTypes[i] != "null" && fanControlParam->softAccelerationK[i] != 0) {
            softAccelerationGraph.push_back(createSoftAccelerationGraph(fanControlParam->softAccelerationTypes[i], fanControlParam->softAccelerationK[i], fanControlParam->softAccelerationA[i]));
            if (fanControlParam->softAccelerationDeaccelerations[i])
                softDeaccelerationGraph.push_back(createSoftAccelerationGraph("EXPONENTIAL", deaccelerationK, deaccelerationA));
            else
				softDeaccelerationGraph.push_back(nullptr);
            if (accelerationGraphs)
                softAccelerationGraphPlot.push_back(plotSoftAccelerationGraph(softAccelerationGraph[i]));
        }
        else {
			softDeaccelerationGraph.push_back(nullptr);
			softAccelerationGraph.push_back(nullptr);
			if (accelerationGraphs)
				softAccelerationGraphPlot.push_back("No soft acceleration graph defined."); 
        }
    }

    if (accelerationGraphs) {
        for (int i = 0; i < softAccelerationGraphPlot.size(); i++) {
            cout << endl << "Fan " << i << " soft acceleration graph: " << endl;
            addLoggingAreaMessage("GRAPH_PLOT", "Fan " + to_string(i) + " soft acceleration graph: ");
            cout << softAccelerationGraphPlot[i] << endl;
            addLoggingAreaMessage("GRAPH_PLOT", softAccelerationGraphPlot[i]);
        }
        logLoggingArea("GRAPH_PLOT");
        return pressReturnToClose();
    }

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

        setFans.push_back(new SetFans(softwareParam->logicPwmTolerances, sensorNames, sensorNamesDevices, deviceIndexes, buildTempTempRpmGraphs, fanControlParam->sensorFunctions[i], fanControl[i], fanRpm[i],
        fanControlParam->minPwms[i], fanControlParam->maxPwms[i], fanControlParam->startPwms[i], fanControlParam->avgTimes[i], fanControlParam->overrideMax[i], fanControlParam->proportionalFactor[i], fanControlParam->hysteresis[i], 
            oneRead, softwareParam->oneSenseReadPc, uniqueFanCtrlName, fanControlParam->spinUpDelays[i], fanControlParam->spinDownDelays[i], 
            softAccelerationGraph[i], softDeaccelerationGraph[i], fanControlParam->softAccelerationTimeframes[i], fanControlParam->softAccelerationSamplingIntervals[i], fanControlParam->softAccelerationInitTresholds[i]));
    }
    
    fanDataProvider* dataProvider = new fanDataProvider(setFans);

    int balancedRefreshTime = 0;
    if (setFans.size() > 0) {
        balancedRefreshTime = softwareParam->refreshInterval / setFans.size();
    } else {
		errorLog("No fans found!");
        throw std::runtime_error("No fans found!");
    }

    logLoggingArea(LOG_AREA_INIT);

    vector<thread> fanThreads;

    mutex turnMutex;
    condition_variable turnCV;
    atomic<int> turn = 0;

    for (int i = 0; i < setFans.size(); i++) {
        auto* fan = setFans[i];

        fanThreads.emplace_back([&, fan, i]() {
                while (keepRunning) {

                    unique_lock<mutex> lk(turnMutex);
                    turnCV.wait(lk, [&] { return !keepRunning || turn == i; });

                    if (!keepRunning) break;

                    turn = -1;

                    lk.unlock();

                    //cout << "Thread " << i << " is setting fan speed..." << endl;
                    fan->declareFanRpmFromTempGraph();
                    fan->setFanSpeedFromDeclaredRpm();

                }
            });
    }

    while (keepRunning) {
        dataProvider->readFansData();

        for (int j = 0; j < setFans.size(); j++) {
            if (turn.load() == 0) {

                if (softwareParam->oneSenseReadPc) {
                    oneRead->resetAllSavedValues();
                }

                this_thread::sleep_for(chrono::milliseconds(1));
            }

            this_thread::sleep_for(chrono::milliseconds(balancedRefreshTime));

            {
                lock_guard<mutex> lk(turnMutex);
                turn = j % setFans.size();
            }

            turnCV.notify_all();
        }
    }

    cout << "Exiting...\n";

    turn = 0;
    for (auto& t : fanThreads) {
        if (t.joinable())
            t.join();
    }

    // deleting objects
    for (auto* fan : setFans) {
        delete fan;
    }
    delete jsonConfigReader;
    delete fanControlParam;
    delete softwareParam;
    delete oneRead;
	delete dataProvider;
    
    return 0;

}

int* createSoftAccelerationGraph(string type, double k, double a) {
    int* graph = new int[255]{};
    if ((type == "EXPONENTIAL" || type == "exponential") && a == 0)
        a = 1;
    for (int i = 0; i < 255; i++) {
        if (type == "LINEAR" || type == "linear") {
            graph[i] = static_cast<int> (round((a / 8) * i));
        }
        else if (type == "EXPONENTIAL" || type == "exponential") {
            double y = a * (pow(2.71828, k * i) - 1);
            if (!std::isfinite(y) || y > 255)
                graph[i] = -1;
            else {
				graph[i] = static_cast<int>(round(y));
            }
        } else if (type == "LOGARITHMIC" || type == "logarithmic") {
            double y = a * log(k * i + 1);
            if (!std::isfinite(y) || y > 255)
                graph[i] = -1;
            else {
                graph[i] = static_cast<int>(round(y));
            }
        }
    }
    return graph;
}

string plotSoftAccelerationGraph(int* graph) {
    const int scale = 7;
    string plot;
	bool hasStar = false;

    for (int y = 255; y >= 0; y -= scale) {
        if (y > 0 && graph[y-1] == -1)
            continue;
        for (int bucket = 0; bucket < 255; bucket += scale) {
            int sum = 0;
            int count = 0;

            for (int x = bucket; x < min(bucket + scale, 255); x++) {
                sum += graph[x];
                count++;
            }

            int avgY = sum / count;

            if (abs(avgY - y) <= scale / 2) {
                plot += '*';
				hasStar = true;
            }
            else
                plot += ' ';
        }
        if (hasStar)
            plot += '\n';
    }

    return plot;
}

void enableConsole() {
    AllocConsole();

    FILE* fp;

    freopen_s(&fp, "CONOUT$", "w", stdout);
    freopen_s(&fp, "CONOUT$", "w", stderr);
    freopen_s(&fp, "CONIN$", "r", stdin);

    cout.clear();
    cerr.clear();
    cin.clear();
}

int pressReturnToClose() {
    std::cout << "Press Enter to close...";
    std::cin.get();
    return 0;
}

void helpText() {
        cout <<
            R"(fanCommander - Fan Control Utility

        Usage:
          fanCommander.exe [options]

        Options:
          -h,  --help
              Display this help message.

          -ec, --enableConsole
              Enable console output when running as a Windows application.

          -tf, --testFans
              Run a fan test sequence and exit.

          -tfc, --testFansContinue
              Run a fan test sequence and then continue normal operation.

          -ag, --accelerationGraphs
              Generate and display configured soft acceleration graphs.

        Examples:
          fanCommander.exe --testFans
          fanCommander.exe --testFansContinue
          fanCommander.exe --accelerationGraphs
          fanCommander.exe --enableConsole
          fanCommander.exe --enableConsole --accelerationGraphs

        Notes:
          Configuration is loaded from:
              config.json

          When built as a Windows subsystem application,
          console output is only visible when enabled
          using --enableConsole or another option that
          automatically enables console output.

        )";
}