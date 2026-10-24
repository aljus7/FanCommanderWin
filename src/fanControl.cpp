#include"fanControl.h"
#include <functional>
#include <ostream>
#include <regex>
#include <string>
#include <thread>
#include <utility>
#include <regex>
#include <locale>
#include <codecvt>
#include <filesystem>
#include <eventLogger.h>
using json = nlohmann::json;

DeviceType getDeviceTypeFromString(const std::string& str) {
    if (str == "CPU") return DeviceType::CPU;
    if (str == "GPU") return DeviceType::GPU;
    throw std::invalid_argument("Unknown device type: " + str);
}

GetTemperature::GetTemperature(vector<string> tempSensorDevice, vector<string> tempSensorNames ,vector<int> tempSenseIndex, vector<vector<pair<int, int>>> tempRpmGraph, string function, int maxPwm, int avgTimes, OneSenseReadPerCycle* oneSensePc, bool osrpcState) {
	cout << "Initializing GetTemperature with " << tempSensorDevice.size() << " temp sensor devices and " << tempRpmGraph.size() << " temp-rpm graphs." << endl;

    if (tempSensorDevice.size() == tempRpmGraph.size()) {
        this->tempSensorDevice.resize(tempSensorDevice.size());
        this->tempSensorDeviceNoNumbers.resize(tempSensorDevice.size());
        this->tempSensorDeviceType.resize(tempSensorDevice.size());
        this->tempSensorNames.resize(tempSensorDevice.size());
        this->tempSensorNamesVarchar.resize(tempSensorDevice.size());
        this->tempSensorIndexes.resize(tempSensorDevice.size());
        this->uniqueSensorNames.resize(tempSensorDevice.size());
        
        for (int i = 0; i < tempSensorDevice.size(); i++) {
            if (tempSensorDevice[i].empty() || tempSensorNames[i].empty() || tempSenseIndex[i] < 0) {
				errorLog("Temp sensor device path, name or index is invalid!");
				throw std::invalid_argument("Invalid temp sensor device path, name or index!");
            }
            this->tempSensorDevice[i] = tempSensorDevice[i];
			this->tempSensorDeviceNoNumbers[i] = regex_replace(regex_replace(tempSensorDevice[i], regex("[0-9]+"), ""), regex("^\\s+|\\s+$"), "");
			this->tempSensorDeviceType[i] = getDeviceTypeFromString(this->tempSensorDeviceNoNumbers[i]);
			this->tempSensorNames[i] = tempSensorNames[i];
            wstring wstr = std::wstring_convert<std::codecvt_utf8<wchar_t>>().from_bytes(this->tempSensorNames[i]);
            this->tempSensorNamesVarchar[i] = wstr;

            this->tempSensorIndexes[i] = tempSenseIndex[i];
			this->uniqueSensorNames[i] = this->tempSensorDevice[i] + "_" + this->tempSensorNames[i] + "_" + to_string(this->tempSensorIndexes[i]);
        }

        this->osrpc = oneSensePc;
        this->osrpcState = osrpcState;

        if (!tempRpmGraph.empty()) {
            for (int i = 0; i < tempRpmGraph.size(); i++) {
                if (tempRpmGraph[i].size() >= 3) {
                    this->tempRpmGraph = tempRpmGraph;
					addLoggingAreaMessage(LOG_AREA_FANCONTROL, "Sucessfull tempRpmGraph size validation");
                    cout << "Sucessfull tempRpmGraph size validation" << endl;
                } else {
                    errorLog("Only min 3 temp-rpm points permited!");
                }
            }
        }
        
    } else {
        errorLog("for every temp sensor device needs to be one tempRpm graph, max 4 sensors per fan permited!");
    }

    if(function == "max" || function == "min" || function == "avg") {
        this->function = function;
    } else {
        errorLog("function can be only of 'min', 'max', 'avg' value (defaulting to max)");
        this->function = "max";
    }

    if(maxPwm <= 255 && maxPwm > 0) {
        this->maxPwm = maxPwm;
    } else {
        errorLog("maxPwm was set over 255 value or under 1 value. (defaulting to 255)");
        this->maxPwm = 255;
    }

    if (this->tempSensorDevice.size() != this->tempRpmGraph.size()) {
        errorLog("Size mismatch: tempSensorDevice.size() != tempRpmGraph.size()");
        throw std::invalid_argument("Size mismatch: tempSensorDevice.size() != tempRpmGraph.size()");
    }

    this->avgTimes = avgTimes;

    this->rpms.resize(tempSensorDevice.size());

    for (int i = 0; i < this->tempSensorDevice.size(); i++) {
        int tempValue = GetDeviceTemp(this->tempSensorDeviceType[i], this->tempSensorNamesVarchar[i].c_str(), this->tempSensorIndexes[i]);
        if (tempValue < 0) {
            errorLog("Failed to read temperature from sensor: " + this->uniqueSensorNames[i]);
            throw std::runtime_error("Failed to read temperature from sensor: " + this->uniqueSensorNames[i]);
        }
    }

}

int GetTemperature::averaging(int pwm) {
    int sum = 0;

    // simplification using <deque>
    this->lastPwmValues.push_back(pwm);
    if (this->lastPwmValues.size() > this->avgTimes) {
        this->lastPwmValues.pop_front();
    } 

    // Compute average if we have at least one value
    if (!this->lastPwmValues.empty()) {
        for (int value : this->lastPwmValues) {
            sum += value;
        }
        return sum / this->lastPwmValues.size();
    } else {   
		errorLog("Averaging error: array is not populated yet.");
        std::cerr << "Averaging error: array is not populated yet." << std::endl;
        return 255;
    }
}

// what i really meant is get Pwm, haha
void GetTemperature::getRpm() {
    vector<int> temps(this->tempSensorDevice.size());
    string tempStr;
	bool sensorInvalidRead = false;

    if (this->osrpcState) {
        for(int i = 0; i < this->tempSensorDevice.size(); i++) {
                temps[i] = osrpc->getSetValue(this->uniqueSensorNames[i]);
                if (temps[i] < 0) {
                    temps[i] = GetDeviceTemp(this->tempSensorDeviceType[i], this->tempSensorNamesVarchar[i].c_str(), this->tempSensorIndexes[i]);
                    osrpc->setValue(this->uniqueSensorNames[i], temps[i]);
                }
                //cout << "Setting saved value " << temps[i] << endl;
        }
    } else {
        for(int i = 0; i < this->tempSensorDevice.size(); i++) {
            temps[i] = GetDeviceTemp(this->tempSensorDeviceType[i], this->tempSensorNamesVarchar[i].c_str(), this->tempSensorIndexes[i]);
        }
    }


    for (size_t j = 0; j < temps.size(); ++j) {
        if (j >= tempRpmGraph.size()) {
			errorLog("Index " + to_string(j) + " out of bounds for tempRpmGraph");
            std::cerr << "Index " << j << " out of bounds for tempRpmGraph" << std::endl;
            continue;
        }
        const int temp = temps[j];
        const auto& currGraph = tempRpmGraph[j];
        int& rpm = this->rpms[j];

        if (temp == -100) {
            sensorInvalidRead = true;
            rpm = 255;
            continue;
        }

        for (size_t i = 0; i < currGraph.size(); ++i) {
            if (temp <= currGraph[i].first) {
                if (i == 0) {
                    rpm = currGraph[0].second;
                } else {
                    const auto& prev = currGraph[i - 1];
                    const auto& curr = currGraph[i];
                    rpm = prev.second + ((temp - prev.first) * (curr.second - prev.second)) / (curr.first - prev.first);
                }
                break;
            }
        }

        if (temp > currGraph.back().first) {
            rpm = currGraph.back().second;
        }
    }

    if (sensorInvalidRead && !this->sensorReadErrorMessageShown) {
		string sensorList;
		for (const auto& name : this->uniqueSensorNames) {
            			sensorList += name + ", ";
		}
        errorLog("One or more temperature sensors returned invalid values. (this can heppen while driver updates are in progress)\n"
            + sensorList);
        this->sensorReadErrorMessageShown = true;
    } else if (this->sensorReadErrorMessageShown) {
		this->sensorReadErrorMessageShown = false;
    }

}

// pwm haha
int GetTemperature::getFanRpm() {

    if(this->rpms.size() > 1) { 

        if (this->function == "max" && !this->rpms.empty()) {
            int max = rpms[0];
            for (int i = 1; i < this->rpms.size(); i++) {
                    max = max > rpms[i] ? max : rpms[i];
            }
            return averaging(max);
        }

        else if (this->function == "min" && !this->rpms.empty()) {
            int min = rpms[0];
            for (int i = 1; i < this->rpms.size(); i++) {
                    min = min < rpms[i] ? min : rpms[i];
            }
            return averaging(min);
        }

        else if (this->function == "avg" && !this->rpms.empty()) {
            int avg = rpms[0];
            int sum = 0;
            for (int i = 0; i < this->rpms.size(); i++) {
                sum += rpms[i];
            }
            avg = sum/rpms.size();
            return averaging(avg);
        }

        else {
			errorLog("function value needs to be either 'max', 'min' or 'avg'");
            cerr << "function value needs to be either 'max', 'min' or 'avg'" << endl;
            return 255;
        }
        
    } else {
        return averaging(this->rpms[0]);
    }
}

FanControl::FanControl(int logicPwmTolerances, int fanIndex, string fanNamePathUnique, int rpmIndex, int minPwm, int maxPwm, int startPwm, bool overrideMax, double propFactor, double hysteresis, 
    map<int, int> spinUpDelays, map<int, int> spinDownDelays, int* accGraph, int* deaccGraph, int softAccelerationTimeframe, int softAccelerationSamplingInterval, int softAccelerationInitTreshold) {

    if (fanIndex < 0 && rpmIndex < 0) {
        errorLog("Fan and rpm index should both be positive integers");
        throw std::invalid_argument("Fan and rpm index should both be positive integers");
    }

    this->fanPwmIndex = fanIndex;
    this->fanRpmIndex = rpmIndex;
    this->logicPwmTolerances = logicPwmTolerances;

    if (hysteresis < 1 && hysteresis >= 0) {
        this->hysteresisGood = hysteresis*255;
    } else {
        errorLog("Hysteresis is invalid, defaulting to 0");
        cerr << "Hysteresis is invalid, defaulting to 0" << endl;
        this->hysteresisGood = 0;
    }

    if (minPwm >= 0 && minPwm <= 255 && startPwm >= 0 && startPwm <= 255 && maxPwm <= 255 && maxPwm >= 0 && maxPwm != minPwm && startPwm != maxPwm) {
        this->minPwm = minPwm;
        this->startPwm = startPwm;
        this->maxPwmGood = maxPwm;
    } else {
		addLoggingAreaMessage(LOG_AREA_FANCONTROL, "ATENTION: Some of the values of min/start/max pwm do not match requirements! Default sane values used. Some of the values will be calculated.");
        cout << "ATENTION: Some of the values of min/start/max pwm do not match requirements! Default sane values used. Some of the values will be calculated." << endl;
        this->minPwm = 0;
        this->startPwm = 0;
        this->maxPwmGood = 255;
    }

    if (overrideMax) {
        this->overrideMax = true;
    } else {
        this->overrideMax = false;
    }

    if (accGraph == nullptr || propFactor > 0) {
        if (propFactor > 0 && accGraph != nullptr) {
            cout << "Note: Soft fan acceleration cant be enabled if proportional factor is more that 0" << endl;
			addLoggingAreaMessage(LOG_AREA_FANCONTROL, "Note: Soft fan acceleration cant be enabled if proportional factor is more that 0");
        }
        this->softAccState = false;
        this->softAccGraph = nullptr;
		this->softDeaccGraph = nullptr;
		this->softFanAccThreadRunState = false;
		this->softAccelerationTimeframe = 0;
		this->softAccelerationSamplingInterval = 0;
		this->softAccelerationInitTreshold = 0;
    } else {
        this->softAccState = true;
        this->softAccGraph = accGraph;
        if (deaccGraph != nullptr) {
			this->softDeaccState = true;
        } else {
			this->softDeaccState = false;
        }
		this->softDeaccGraph = deaccGraph;
		this->softAccelerationTimeframe = softAccelerationTimeframe;
		this->softAccelerationSamplingInterval = softAccelerationSamplingInterval;
		this->softAccelerationInitTreshold = softAccelerationInitTreshold;
		int stepSize = 255 / (softAccelerationTimeframe / softAccelerationSamplingInterval);
		bool deaccSoftState = this->softDeaccState;
        this->softFanAccThread = thread([this, accGraph, deaccGraph, stepSize, softAccelerationSamplingInterval, deaccSoftState, softAccelerationInitTreshold]() {
			auto& prevValue = this->softFanAccPrevValue;
			auto& currValue = this->softFanAccCurrValue;
			auto& softFanAccEmidiateSet = this->softFanAccEmidiateSet;
			auto& renewSoftFanAccThreadCycle = this->renewSoftFanAccThreadCycle;
			auto& softFanAccThreadRunState = this->softFanAccThreadRunState;
			auto& turnMutex = this->turnMutex;
			auto& turnCV = this->turnCV;
			auto& fanPwmIndex = this->fanPwmIndex;
			auto& accGraph = this->softAccGraph;
			auto& deaccGraph = this->softDeaccGraph;
            int deaccSize = 40;
            int trijectory = 0;
			int currValueWork = 0;
			auto& needsChange = this->needsChange;
			bool nudge = false;
			bool& softDeaccState = this->softDeaccState;
            while (softFanAccThreadRunState) {
                unique_lock<mutex> lk(turnMutex);
                turnCV.wait(lk, [&] { return !softFanAccThreadRunState || renewSoftFanAccThreadCycle; });
                renewSoftFanAccThreadCycle = false;
                lk.unlock();

                if (!softFanAccThreadRunState)
                    break;

                if (!softFanAccEmidiateSet && abs(prevValue - currValue) > softAccelerationInitTreshold) {
					nudge = true;
                    currValueWork = currValue;
                    trijectory = currValue;
                    if (currValue > prevValue) {
                        if (!deaccSoftState || abs(currValue - prevValue) > 40) {
                            for (int i = 0; i < 255; i += stepSize) {
                                trijectory += accGraph[i];
                                if (trijectory < 255 && accGraph[i] != -1) {
                                    SetFanPwm(fanPwmIndex, trijectory);
                                    lk.lock();
                                    turnCV.wait_for(lk, std::chrono::milliseconds(softAccelerationSamplingInterval), [&] { return !softFanAccThreadRunState || renewSoftFanAccThreadCycle; });
                                    lk.unlock();
                                    if (currValue <= trijectory || softFanAccEmidiateSet) {
                                        break;
                                    }
                                }
                                else {
                                    break;
                                }
                            }
                        }
                        else {
                            for (int i = 0; i <= deaccSize; i += stepSize) {
                                trijectory += deaccGraph[i];
                                if (trijectory < 255 && deaccGraph[i] != -1) {
                                    SetFanPwm(fanPwmIndex, trijectory);
                                    lk.lock();
                                    turnCV.wait_for(lk, std::chrono::milliseconds(softAccelerationSamplingInterval), [&] { return !softFanAccThreadRunState || renewSoftFanAccThreadCycle; });
                                    lk.unlock();
                                    if (currValue <= trijectory || softFanAccEmidiateSet) {
                                        break;
                                    }
                                }
                                else {
                                    break;
                                }
                            }
                        }
                    }
                    else {
                        if (!deaccSoftState || abs(currValue - prevValue) > 40) {
                            for (int i = 0; i < 255; i += stepSize) {
                                trijectory -= accGraph[i];
                                if (trijectory > 0 && accGraph[i] != -1) {
                                    SetFanPwm(fanPwmIndex, trijectory);
                                    lk.lock();
                                    turnCV.wait_for(lk, std::chrono::milliseconds(softAccelerationSamplingInterval), [&] { return !softFanAccThreadRunState || renewSoftFanAccThreadCycle; });
                                    lk.unlock();
                                    if (currValue >= trijectory || softFanAccEmidiateSet) {
                                        break;
                                    }
                                }
                                else {
                                    break;
                                }
                            }
                        }
                        else {
                            for (int i = deaccSize; i >= 0; i -= stepSize) {
                                trijectory -= deaccGraph[i];
                                if (trijectory > 0 && deaccGraph[i] != -1) {
                                    SetFanPwm(fanPwmIndex, trijectory);
                                    lk.lock();
                                    turnCV.wait_for(lk, std::chrono::milliseconds(softAccelerationSamplingInterval), [&] { return !softFanAccThreadRunState || renewSoftFanAccThreadCycle; });
                                    lk.unlock();
                                    if (currValue >= trijectory || softFanAccEmidiateSet) {
                                        break;
                                    }
                                }
                                else {
                                    break;
                                }
                            }
                        }
                    }
                } else if (needsChange.load() || softFanAccEmidiateSet || nudge) {
                    if (nudge)
						nudge = false;
                    SetFanPwm(fanPwmIndex, currValue);
                }
                prevValue.store(currValue.load());
            }
        });
    }

    this->propFactor = propFactor;

    this->spinUpDelays = spinUpDelays;
	this->spinUpDelaysState = !spinUpDelays.empty();

    this->spinDownDelays = spinDownDelays;
	this->spinDownDelaysState = !spinDownDelays.empty();

    filesystem::create_directories(this->stateFilesPath);

    regex nonDigit("[^0-9]+");
    this->autoGenFileName = this->stateFilesPath + regex_replace(fanNamePathUnique, nonDigit, "") + this->autoGenFileAppend;

    if (this->autoGenFileName.empty()) {
		errorLog("Fan control file doesn't contain any digits to create unique auto gen file name. Feel free to MR this issue with better solution.");
        throw std::invalid_argument("Fan control file doesent contain any digits to create unique auto gen file name. Feel free to MR this issue with better solution.");
    }

    std::ifstream checkFile(this->autoGenFileName);
    bool fileExists = checkFile.good();
    checkFile.close();

    // Open with trunc if file doesn't exist, else open normally
    std::ios_base::openmode mode = std::ios::out | std::ios::in;
    if (!fileExists) {
        mode = std::ios::out | std::ios::trunc | std::ios::in;
    }

    fanSettingsAutoGenFile.open(this->autoGenFileName, mode);

    if (!fanSettingsAutoGenFile.is_open()) {
		errorLog("Failed to open autoGen file: " + this->autoGenFileName);
        std::cerr << "Failed to open autoGen file: " << this->autoGenFileName << std::endl;
        return;
    }

    if (fileExists) {
        getMinStartPwm(fanSettingsAutoGenFile);
    } else {
        writeMinStartPwm(fanSettingsAutoGenFile);
    }

    fanSettingsAutoGenFile.close();

}

void FanControl::getMinStartPwm(fstream &file) {
    string savedSettingsJson;
    if (file.is_open()) {
        string line;
        while (getline (file, line)) {
            savedSettingsJson += line;
        }
        json savedVal = json::parse(savedSettingsJson);
        if (savedVal["overrideMax"].get<bool>() != this->overrideMax || 
            (savedVal["proportionalFactor"].get<double>() > 0 && this->propFactor == 0) || 
            (savedVal["proportionalFactor"].get<double>() == 0 && this->propFactor > 0)) {
            writeMinStartPwm(file);
            return;
        }
        this->minPwmGood = savedVal["minPwm"].get<int>();
        this->startPwmGood = savedVal["startPwm"].get<int>();
        this->maxPwmGood = savedVal["maxPwm"].get<int>();

        if (this->propFactor > 0) {
            if (savedVal["pwmRpmData"].is_array()) {
                for (const auto &data : savedVal["pwmRpmData"]) {
                    this->rpmPwmCoorelation[data["pwm"].get<int>()] = data["rpm"].get<int>();
                }
            }
        }
    } else {
		errorLog("File is not open!");
        std::cerr << "File is not open!" << std::endl;
        throw std::runtime_error("Failed to open saved values file.");
    }
}

void FanControl::writeMinStartPwm(fstream &file) {
    
	addLoggingAreaMessage(LOG_AREA_FANCONTROL, "Probing PWM values. Please Wait (that is only done first time program is launched)");
    cout << "Probing PWM values. Please Wait (that is only done first time program is launched)" << endl;

    // calculating min pwm
    SetFanPwm(this->fanPwmIndex, 255);
    waitForFanRpmToStabilize();
    for (int i = 255; i >= 0; i--) {
        SetFanPwm(this->fanPwmIndex, i);
        waitForFanRpmToStabilize();
        int rpm = ReadFanRpm(this->fanRpmIndex);
        if (rpm == 0) {
            this->minPwmGood = i;
            break;
        } else if (i == 0) {
            this->minPwmGood = i;
            break;
        }
    }

    // calculationg start pwm and make rpm / pwm coorelation graph
    bool startFound = false;
    for (int i = 0; i <= 255; i++) {
        SetFanPwm(this->fanPwmIndex, i);
        waitForFanRpmToStabilize();
        int rpm = ReadFanRpm(this->fanRpmIndex);
        if (rpm > 0 && !startFound) {
            this->startPwmGood = i;
            startFound = true;
        }
        if (startFound && rpm == 0) {
            startFound = false;
        }
        this->rpmPwmCoorelation[i] = rpm;
    }

    // calculating max pwm, if not oveririden
    if (!overrideMax || this->propFactor > 0) {
        int prevRpm = 0;
        bool quitOuter = false;
        int lastInc = 0;
        for (int i = this->startPwmGood; i<=255; i++) {
            if (i >= lastInc) {
                SetFanPwm(this->fanPwmIndex, i);
                if (i == this->startPwmGood) {
                    this_thread::sleep_for(std::chrono::milliseconds(5000));
                }
                waitForFanRpmToStabilize();
                int rpm = ReadFanRpm(this->fanRpmIndex);
                if (i == this->startPwmGood)
                    prevRpm = rpm;
                else {
                    if (!(prevRpm < rpm)) {
                        for(int j = i+1; j <= 255; j++) {
                            SetFanPwm(this->fanPwmIndex, j);
                            waitForFanRpmToStabilize();
                            int rpmSan = ReadFanRpm(this->fanRpmIndex);
                            if(prevRpm < rpmSan) {
                                //if (j > 0)
                                if (i < j)    
                                    lastInc = j;
                                prevRpm = rpmSan;
                                break;
                            } else if (j == 255) {
                                this->maxRpm = rpmSan;
                                this->maxPwmGood = i;
                                quitOuter = true;
                                break;
                            }
                        }
                    }
                    if (quitOuter) {
                        break;
                    } else if (i==255) {
                        this->maxPwmGood = 255;
                        this->maxRpm = rpm;
                    }
                    prevRpm = rpm;
                }
            }
        }
    }

    if (this->startPwm > this->startPwmGood) {
		addLoggingAreaMessage(LOG_AREA_FANCONTROL, "Custom StartPwm value set that is higher than real one, using custiom value");
        cout << "Custom StartPwm value set that is higher than real one, using custiom value" << endl;
        this->startPwmGood = this->startPwm;
    }

    if (this->minPwm > this->minPwmGood) {
		addLoggingAreaMessage(LOG_AREA_FANCONTROL, "Custom MinPwm value set that is higher than real one, using custiom value");
        cout << "Custom MinPwm value set that is higher than real one, using custiom value" << endl;
        this->minPwmGood = this->minPwm;
    }

    json jObject;
    jObject["minPwm"] = this->minPwmGood;
    jObject["startPwm"] = this->startPwmGood;
    jObject["maxPwm"] = this->maxPwmGood;
    jObject["overrideMax"] = this->overrideMax;
    jObject["proportionalFactor"] = this->propFactor;
    jObject["pwmRpmData"] = json::array();
    for (int i = 0; i <= 255; i++) {
        nlohmann::json entry;
        entry["pwm"] = i;
        entry["rpm"] = this->rpmPwmCoorelation[i];
        jObject["pwmRpmData"].push_back(entry);
    }
    if (file.is_open()) {
        file.close();
    }
    file.open(this->autoGenFileName, std::ios::out | std::ios::trunc | std::ios::in);
    if (file.is_open()) {
        file << jObject.dump(4);
    } else {
        errorLog("Internal settings file is not open!");
        throw std::runtime_error("Failed to open saved values file.");
    }
        
}

void FanControl::waitForFanRpmToStabilize() {
    int fanRpmStr = 0;
    int prevRpm = 256;
    int diff;
    int i = 0;
    do {
        fanRpmStr = ReadFanRpm(this->fanRpmIndex);
        int fanRpm = fanRpmStr;
        if (prevRpm == 256) {    
            prevRpm = fanRpm;
            this_thread::sleep_for(std::chrono::milliseconds(1000));
            diff = abs(prevRpm-fanRpm);
        } else {
            this_thread::sleep_for(std::chrono::milliseconds(1000));
            diff = abs(prevRpm-fanRpm);
            prevRpm = fanRpm;
        }
        ++i;
    } while(diff > 20 && i < 20);
}

void FanControl::getFeedbackRpm() {
    int fanRpm = ReadFanRpm(this->fanRpmIndex);
    
    this->feedBackRpm = fanRpm;
}

void FanControl::softFanAcceleration(int fanPwmIndex, int targetPwm, bool state, bool emidiate) {
    if (state) {
        auto& softFanAccEmidiateSet = this->softFanAccEmidiateSet;
        auto& renewSoftFanAccThreadCycle = this->renewSoftFanAccThreadCycle;
        auto& currValue = this->softFanAccCurrValue;

        {
            lock_guard<mutex> lk(this->turnMutex);
            softFanAccEmidiateSet = emidiate;
            currValue = targetPwm;
            renewSoftFanAccThreadCycle = true;
        }
		this->turnCV.notify_one();
    }
    else {
        SetFanPwm(this->fanPwmIndex, targetPwm);
    }
}

void FanControl::setFanSpeed(int pwm) {
    getFeedbackRpm();
	int& logicPwmTolerances = this->logicPwmTolerances;
    int& prevPwm = this->prevSetPwm;
    auto& needChange = this->needsChange;

	bool& spinDownDelaysState = this->spinDownDelaysState;
	bool& spinUpDelaysState = this->spinUpDelaysState;
	int &spinUpDelayLastPwm = this->spinUpDelayLastPwm;
	int &spinUpDelayLastDelay = this->spinUpDelayLastDelay;
	int& spinUpDelayCountdown = this->spinUpDelayCountdown;
	int &spinDownDelayLastPwm = this->spinDownDelayLastPwm;
	int &spinDownDelayLastDelay = this->spinDownDelayLastDelay;
	int& spinDownDelayCountdown = this->spinDownDelayCountdown;

	bool& softAccState = this->softAccState;

    if (pwm <= 255 && pwm >= 0) {
        if (pwm >= this->minPwmGood && this->feedBackRpm == 0) {
            if (255 - this->startPwmGood > 10)  { 
                softFanAcceleration(this->fanPwmIndex, this->startPwmGood + 10, softAccState, true);
            } else {
                softFanAcceleration(this->fanPwmIndex, this->startPwmGood, softAccState, true);
				errorLog("ATENTION: Critical system failure, fan is likely dead!");
                cout << "ATENTION: Critical system failure, fan is likely dead!" << endl;
            }
        }

        if (spinUpDelaysState) {
            auto it = this->spinUpDelays.find(pwm);
            if (it != this->spinUpDelays.end() && it->second > 0) {
                if (spinUpDelayLastDelay != it->second) {
                    spinUpDelayLastDelay = 0;
                }
                if (spinUpDelayCountdown <= 0 && pwm > prevPwm + logicPwmTolerances) {
                    spinUpDelayCountdown = it->second;
                    spinUpDelayLastPwm = pwm;
                }
            }
            else {
                spinUpDelayCountdown = 0;
            }

            if (spinUpDelayCountdown > 0) {
                if (pwm >= spinUpDelayLastPwm - logicPwmTolerances) {
                    spinUpDelayCountdown--;
                    pwm = spinUpDelayLastPwm;
                } else {
					spinUpDelayCountdown = 0;
                }
            }
			spinUpDelayLastDelay = it->second;
		}

        if (spinDownDelaysState) {
            auto it = this->spinDownDelays.find(pwm);
            if (it != this->spinDownDelays.end() && it->second > 0) {
                if (spinDownDelayCountdown <= 0 && pwm < prevPwm - logicPwmTolerances) {
                    spinDownDelayCountdown = it->second;
                    spinDownDelayLastPwm = pwm;
                }
            }
            else {
                spinDownDelayCountdown = 0;
            }

            if (spinDownDelayCountdown > 0) {
                if (pwm <= spinDownDelayLastPwm + logicPwmTolerances) {
                    spinDownDelayCountdown--;
                    pwm = spinDownDelayLastPwm;
                } else {
					spinDownDelayCountdown = 0;
                }
            }
		}

        if (this->hysteresisGood > 0) {
            if (abs(prevPwm-pwm) < this->hysteresisGood) {
                pwm = prevPwm;
                if (this->propFactor == 0) {
                    needChange.store(false);
                } 
            } else {
                needChange.store(true);
            }
        }

        // here uis prev pwm save for next cycle
        if (this->hysteresisGood > 0 || this->spinUpDelaysState) {
			prevPwm = pwm;
        }

        if (this->propFactor > 0 && pwm > this->minPwmGood) {
                
            if (pwm > maxPwmGood) {
                pwm = maxPwmGood;
            }

            pwm = this->propFactor * (this->rpmPwmCoorelation[pwm] - this->feedBackRpm) + pwm;

        }

        if (needChange.load()) {

            if (pwm > this->maxPwmGood) {
                pwm = maxPwmGood;
            }

            if (pwm >= this->minPwmGood) {    
                softFanAcceleration(this->fanPwmIndex, pwm, softAccState, false);
				//cout << "Setting PWM to " << pwm << " for " << this->fanPwmIndex << endl;
            } else {
                softFanAcceleration(this->fanPwmIndex, this->minPwmGood, softAccState, true);
            }
                
        }

    } else {
		errorLog("PWM value must be between 0 and 255.");
        cerr << "PWM value must be between 0 and 255." << endl;
        throw std::out_of_range("PWM value must be between 0 and 255.");
    }
}



void SetFans::declareFanRpmFromTempGraph() {
    getRpm();
    this->fanRpm = getFanRpm();
}

void SetFans::setFanSpeedFromDeclaredRpm() {
    setFanSpeed(this->fanRpm);
}



GetTemperature::~GetTemperature() {
    
}

FanControl::~FanControl() {

    if (this->fanSettingsAutoGenFile.is_open())
        this->fanSettingsAutoGenFile.close();

    if (this->softAccState) {
        this->softFanAccThreadRunState = false;
        this->renewSoftFanAccThreadCycle = true;
        this->turnCV.notify_one();
        if (this->softFanAccThread.joinable()) {
            this->softFanAccThread.join();
        }
    }

}

bool OneSenseReadPerCycle::isValueSet(string& senseName) {
    lock_guard<mutex> lk(this->oneReadPerCycleMutex);
    auto it = savedValues.find(senseName);
    if (it != savedValues.end()) {
        return true;
    }
    return false;
}

int OneSenseReadPerCycle::getSetValue(string& senseName) {
    lock_guard<mutex> lk(this->oneReadPerCycleMutex);
    auto it = savedValues.find(senseName);
    if (it != savedValues.end()) {
        return it->second;
    }
    return -1;
}

void OneSenseReadPerCycle::setValue(string& senseName, int val) {
    lock_guard<mutex> lk(this->oneReadPerCycleMutex);
    this->savedValues[senseName] = val;
}

void OneSenseReadPerCycle::resetAllSavedValues() {
    lock_guard<mutex> lk(this->oneReadPerCycleMutex);
    this->savedValues.clear();
}