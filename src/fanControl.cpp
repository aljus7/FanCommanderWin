#include"fanControl.h"
#include <functional>
#include <ostream>
#include <regex>
#include <string>
#include <thread>
#include <utility>
#include "LHMBridge/LHMBridge.h"
using json = nlohmann::json;

GetTemperature::GetTemperature(vector<string> tempSensorDevice, vector<string> tempSensorNames ,vector<int> tempSenseIndex, vector<vector<pair<int, int>>> tempRpmGraph, string function, int maxPwm, int avgTimes, OneSenseReadPerCycle* oneSensePc, bool osrpcState) {
    if (tempSensorDevice.size() == tempRpmGraph.size()) {
        for (int i = 0; i < tempSensorDevice.size(); i++) {
            if (!tempSensorDevice[i].empty() && !tempSensorNames[i].empty() && tempSenseIndex[i] >= 0) {
				cerr << "Temp sensor device path, name or index is invalid!" << endl;
            }
            this->tempSensorDevice[i] = tempSensorDevice[i];
			this->tempSensorNames[i] = tempSensorNames[i];
            this->tempSensorIndexes[i] = tempSenseIndex[i];
        }

        this->osrpc = oneSensePc;
        this->osrpcState = osrpcState;

        if (!tempRpmGraph.empty()) {
            for (int i = 0; i < tempRpmGraph.size(); i++) {
                if (tempRpmGraph[i].size() >= 3) {
                    this->tempRpmGraph = tempRpmGraph;
                    cout << "Sucessfull tempRpmGraph size validation" << endl;
                } else {
                    cerr << "Only min 3 temp-rpm points permited!" << endl;
                }
            }
        }
        
    } else {
        cerr << "for every temp sensor device needs to be one tempRpm graph, max 4 sensors per fan permited!" << endl;
    }

    if(function == "max" || function == "min" || function == "avg") {
        this->function = function;
    } else {
        cerr << "function can be only of 'min', 'max', 'avg' value (defaulting to max)" << endl;
        this->function = "max";
    }

    if(maxPwm <= 255 && maxPwm > 0) {
        this->maxPwm = maxPwm;
    } else {
        cerr << "maxPwm was set over 255 value or under 1 value. (defaulting to 255)" << endl;
        this->maxPwm = 255;
    }

    if (this->tempSensorDevice.size() != this->tempRpmGraph.size()) {
        cerr << "Size mismatch: tempSensorDevice.size() != tempRpmGraph.size()" << std::endl;
        throw std::invalid_argument("Size mismatch: tempSensorDevice.size() != tempRpmGraph.size()");
    }

    this->avgTimes = avgTimes;

    this->rpms.resize(tempSensorDevice.size());

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
        std::cerr << "Averaging error: array is not populated yet." << std::endl;
        return 255;
    }
}

void GetTemperature::getRpm() {
    vector<int> temps(this->tempSensorDevice.size());
    string tempStr;

    if (this->osrpcState) {
        for(int i = 0; i < this->tempSensorDevice.size(); i++) {
            if (!osrpc->isValueSet(this->tempSensorNames[i])) {
                temps[i] = GetDeviceTemp(this->tempSensorDevice[i], this->tempSensorNames[i], this->tempSensorIndexes[i]);
                osrpc->setValue(this->tempSensorNames[i], temps[i]);
            } else {
                temps[i] = osrpc->getSetValue();
                //cout << "Setting saved value " << temps[i] << endl;
            }
        }
    } else {
        for(int i = 0; i < this->tempSensorDevice.size(); i++) {
            temps[i] = GetDeviceTemp(this->tempSensorDevice[i], this->tempSensorNames[i], this->tempSensorIndexes[i]);    
    }
    }


    for (size_t j = 0; j < temps.size(); ++j) {
        if (j >= tempRpmGraph.size()) {
            std::cerr << "Index " << j << " out of bounds for tempRpmGraph" << std::endl;
            continue;
        }
        const int temp = temps[j];
        const auto& currGraph = tempRpmGraph[j];
        int& rpm = this->rpms[j];

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

}

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
            cerr << "function value needs to be either 'max', 'min' or 'avg'" << endl;
            return 255;
        }
        
    } else {
        return averaging(this->rpms[0]);
    }
}

FanControl::FanControl(string fanPath, string fanNamePathOriginal, string rpmPath, int minPwm, int maxPwm, int startPwm, bool overrideMax, double propFactor, double hysteresis) {

    if (!fanPath.empty() && !rpmPath.empty()) {
        this->fanNamePath = fanPath;
        this->fanNamePathOriginal = fanNamePathOriginal;
        if (fanNamePathOriginal.empty()) {
            throw std::invalid_argument("Fan control path is empty, make shure to provide valid fan path.");
        }
        this->fanControl.open(fanPath);
        if(fanControl.is_open()) {
            cout << "Fan control: " << fanPath << " successfully open!" << endl;
        } else {
            throw std::runtime_error("Failed to open fan control file.");
        }
        this->rpmSensor.open(rpmPath);
        if (rpmSensor.is_open()) {
            this->rpmPath = rpmPath;
            cout << "Fan sensor: " << rpmPath << " sucessfully open!" << endl;
        } else {
            throw std::runtime_error("Failed to open rpm sensor file.");
        }
    }

    if (hysteresis < 1 && hysteresis >= 0) {
        this->hysteresisGood = hysteresis*255;
    } else {
        cerr << "Hysteresis is invalid, defaulting to 0" << endl;
        this->hysteresisGood = 0;
    }

    if (minPwm >= 0 && minPwm <= 255 && startPwm >= 0 && startPwm <= 255 && maxPwm <= 255 && maxPwm >= 0 && maxPwm != minPwm && startPwm != maxPwm) {
        this->minPwm = minPwm;
        this->startPwm = startPwm;
        this->maxPwmGood = maxPwm;
    } else {
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

    this->propFactor = propFactor;

    regex nonDigit("[^0-9]+");
    string autoGenFileName = this->stateFilesPath + regex_replace(this->fanNamePathOriginal, nonDigit, "") + this->autoGenFileAppend;

    if (autoGenFileName.empty()) {
        throw std::invalid_argument("Fan control file doesent contain any digits to create unique auto gen file name. Feel free to MR this issue with better solution.");
    }

    std::ifstream checkFile(autoGenFileName);
    bool fileExists = checkFile.good();
    checkFile.close();

    // Open with trunc if file doesn't exist, else open normally
    std::ios_base::openmode mode = std::ios::in | std::ios::out;
    if (!fileExists) {
        mode |= std::ios::trunc; // Create new file
    }

    fanSettingsAutoGenFile.open(autoGenFileName, mode);

    if (!fanSettingsAutoGenFile.is_open()) {
        std::cerr << "Failed to open file: " << autoGenFileName << std::endl;
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
        if (savedVal["overrideMax"] != this->overrideMax || this->propFactor == 0 && savedVal["proportionalFactor"] > 0) {
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
        std::cerr << "File is not open!" << std::endl;
        throw std::runtime_error("Failed to open saved values file.");
    }
}

void FanControl::writeMinStartPwm(fstream &file) {
    
    // calculating min pwm
    this->fanControl.seekp(0);
    this->fanControl << 255 << endl;
    waitForFanRpmToStabilize();
    for (int i = 255; i >= 0; i--) {
        this->fanControl.seekp(0);
        this->fanControl << i << endl;
        waitForFanRpmToStabilize();
        string rpm;
        this->rpmSensor.seekg(0);
        if (getline(this->rpmSensor, rpm)) {
            if (stod(rpm) == 0) {
                this->minPwmGood = i;
                break;
            } else if (i == 0) {
                this->minPwmGood = i;
                break;
            }
        } else {
            cerr << "Failed to read RPM sensor, when probing Min PWM." << endl;
        }
    }

    // calculationg start pwm and make rpm / pwm coorelation graph
    bool startFound = false;
    for (int i = 0; i <= 255; i++) {
        this->fanControl.seekp(0);
        this->fanControl << i << endl;
        waitForFanRpmToStabilize();
        string rpm;
        this->rpmSensor.seekg(0);
        if (getline(this->rpmSensor, rpm)) {
            if (stod(rpm) > 0 && !startFound) {
                this->startPwmGood = i;
                startFound = true;
            }
            if (startFound && stod(rpm) == 0) {
                startFound = false;
            }
            this->rpmPwmCoorelation[i] = stod(rpm);
        } else {
            cerr << "Failed to read RPM sensor, when probing Start PWM and making rpm - pwm coorelationgraph." << endl;
        }
    }

    // calculating max pwm, if not oveririden
    if (!overrideMax || this->propFactor > 0) {
        int prevRpm = 0;
        bool quitOuter = false;
        int lastInc = 0;
        for (int i = this->startPwmGood; i<=255; i++) {
            if (i >= lastInc) {
                this->fanControl.seekp(0);
                this->fanControl << i << endl;
                if (i == this->startPwmGood) {
                    this_thread::sleep_for(std::chrono::milliseconds(5000));
                }
                waitForFanRpmToStabilize();
                string rpm;
                this->rpmSensor.seekg(0);
                if (getline(this->rpmSensor, rpm)) {
                    if (i == this->startPwmGood)
                        prevRpm = stod(rpm);
                    else {
                        if (!(prevRpm < stod(rpm))) {
                            for(int j = i+1; j <= 255; j++) {
                                this->fanControl.seekp(0);
                                this->fanControl << j << endl;
                                waitForFanRpmToStabilize();
                                string rpmSan;
                                this->rpmSensor.seekg(0);
                                if (getline(this->rpmSensor, rpmSan)) {
                                    if(prevRpm < stod(rpmSan)) {
                                        //if (j > 0)
                                        if (i < j)    
                                            lastInc = j;
                                        prevRpm = stod(rpmSan);
                                        break;
                                    } else if (j == 255) {
                                        this->maxRpm = stod(rpmSan);
                                        this->maxPwmGood = i;
                                        quitOuter = true;
                                        break;
                                    }
                                } else {
                                    cerr << "Failed to read RPM sensor, when probing Max PWM value." << endl;
                                }
                            }
                        }
                        if (quitOuter) {
                            break;
                        } else if (i==255) {
                            this->maxPwmGood = 255;
                            this->maxRpm = stod(rpm);
                        }
                        prevRpm = stod(rpm);
                    }

                } else {
                    cerr << "Failed to read RPM sensor, when probing Max PWM value." << endl;
                }
            }
        }
    }

    if (this->startPwm > this->startPwmGood) {
        cout << "Custom StartPwm value set that is higher than real one, using custiom value" << endl;
        this->startPwmGood = this->startPwm;
    }

    if (this->minPwm > this->minPwmGood) {
        cout << "Custom MinPwm value set that is higher than real one, using custiom value" << endl;
        this->minPwmGood = this->minPwm;
    }

    if(file.is_open()) {
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
        file << jObject.dump(4);
    } else {
        std::cerr << "File is not open!" << std::endl;
        throw std::runtime_error("Failed to open saved values file.");
    }
        
}

void FanControl::waitForFanRpmToStabilize() {
    string fanRpmStr;
    int prevRpm = 256;
    int diff;
    int i = 0;
    do {
        this->rpmSensor.seekg(0);
        if (getline(this->rpmSensor, fanRpmStr)) {
            int fanRpm = stoi(fanRpmStr);
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
        }
    } while(diff > 20 && i < 20);
}

void FanControl::getFeedbackRpm() {
    string rpmString;
    int fanRpm;

    if (this->rpmSensor.is_open()) {
        this->rpmSensor.seekg(0);
        if (getline(this->rpmSensor, rpmString)) {
            fanRpm = stod(rpmString);
        } else {
            this->rpmSensor.close();
            this->rpmSensor.open(this->rpmPath);
            if (!this->rpmSensor.is_open()) {
                throw std::runtime_error("Failed to reopen fan rpm feedback file.");
            }
            if (getline(this->rpmSensor, rpmString)) {
                fanRpm = stod(rpmString);
            } else {
                throw std::runtime_error("Failed to read line from reopened fan rpm feedback file.");
            }
            cerr << "couldnt getline from rpm fan feedback file, auto fixing was done by reopening file." << endl;
        }
    } else {
        throw std::runtime_error("Failed to open fan rpm feedback file.");
    }

    this->feedBackRpm = fanRpm;
}

void FanControl::setFanSpeed(int pwm) {
    getFeedbackRpm();
    int &prevPwm = this->prevSetPwm;
    bool &needChange = this->needsChange;

    if (fanControl.is_open()) {
        if (pwm <= 255 && pwm >= 0) {
            if (pwm >= this->minPwmGood && this->feedBackRpm == 0) {
                if (255 - this->startPwmGood > 10)  {   
                    fanControl.seekp(0);
                    fanControl << this->startPwmGood + 10 << endl;
                } else {
                    fanControl.seekp(0);
                    fanControl << this->startPwmGood << endl;
                    cout << "ATENTION: Critical system failure, fan is likely dead!" << endl;
                }
            }

            if (this->hysteresisGood > 0) {
                if (abs(prevPwm-pwm) < this->hysteresisGood) {
                    pwm = prevPwm;
                    if (this->propFactor == 0) {
                        needChange = false;
                    } 
                } else {
                    needChange = true;
                }
                prevPwm = pwm;
            }

            if (this->propFactor > 0 && pwm > this->minPwmGood) {
                
                if (pwm > maxPwmGood) {
                    pwm = maxPwmGood;
                }

                pwm = this->propFactor * (this->rpmPwmCoorelation[pwm] - this->feedBackRpm) + pwm;

            }

            if (needChange) {

                if (pwm > this->maxPwmGood) {
                    pwm = maxPwmGood;
                }

                if (pwm >= this->minPwmGood) {    
                    fanControl.seekp(0);
                    fanControl << pwm << endl;
                } else {
                    fanControl.seekp(0);
                    fanControl << this->minPwmGood << endl;
                }
                
            }

        } else {
            cerr << "PWM value must be between 0 and 255." << endl;
            throw std::out_of_range("PWM value must be between 0 and 255.");
        }
    } else {
        cerr << "Fancontrol cant set fanspeed, fanRpmPath file not open anymore." << endl;
        throw std::runtime_error("Failed to open fan control file.");
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
    for (auto &sensor : this->tempSensor) {
        if(sensor.get().is_open()) {
            sensor.get().close();
        }
    }
}

FanControl::~FanControl() {

    if (this->fanControl.is_open())
        this->fanControl.close();

    if (this->rpmSensor.is_open())
        this->rpmSensor.close();

    if (this->fanSettingsAutoGenFile.is_open())
        this->fanSettingsAutoGenFile.close();

}


TempSensorServer::TempSensorServer(vector<string> tempPaths, vector<string> sensorName) {

    if (tempPaths.size() > 0 && tempPaths.size() == sensorName.size()) {
        for (int i = 0; i < tempPaths.size(); i++) {
            this->tempSensorNamePathCoor.push_back(make_pair(sensorName[i], tempPaths[i]));
        }

        for (int i = 0; i < this->tempSensorNamePathCoor.size(); i++) {
            const string& path = this->tempSensorNamePathCoor[i].second;
            bool match = false;
            if (this->tempSensor.size() == 0) {
                this->tempSensorStreams.emplace_back(path, ios::in);
                if (!this->tempSensorStreams.back().is_open()) {
                    tempSensorStreams.pop_back();
                    throw std::runtime_error("Failed to open file: " + path);
                }
                this->tempSensor.emplace(path, ref(tempSensorStreams.back()));
            } else {
                auto val = this->tempSensor.find(path);
                if (val != this->tempSensor.end()) {
                    match = true;
                }
                if (!match) {
                    this->tempSensorStreams.emplace_back(path, ios::in);
                    if (!this->tempSensorStreams.back().is_open()) {
                        tempSensorStreams.pop_back();
                        throw std::runtime_error("Failed to open file: " + path);
                    }
                    this->tempSensor.emplace(path, ref(this->tempSensorStreams.back()));
                }
            }
        }
    } else {
        throw std::invalid_argument("tempPaths and sensorName must have the same size and should not be empty");
    }

}

ifstream& TempSensorServer::getTempSenseIfstream(const string &sensorPath) {
    auto it = this->tempSensor.find(sensorPath);
    if (it == this->tempSensor.end()) {
        throw std::runtime_error("Sensor path not found: " + sensorPath);
    }

    ifstream& stream = it->second.get();
    if (!stream.is_open()) {
        throw std::runtime_error("Sensor stream for path " + sensorPath + " is not open!");
    }

    return stream;
}

string TempSensorServer::getTempSenseName(const string &sensorPath) {
    for (auto& tempSensPair : this->tempSensor) {
        if (tempSensPair.first == sensorPath) {
            return tempSensPair.first;
        }
    }
    throw std::runtime_error("Sensor path not found: " + sensorPath);
}

bool OneSenseReadPerCycle::isValueSet(string& senseName) {
    auto it = savedValues.find(senseName);
    if (it != savedValues.end()) {
        nextReturnValue = it->second;
        return true;
    }
    return false;
}

int& OneSenseReadPerCycle::getSetValue() {
    return this->nextReturnValue;
}

void OneSenseReadPerCycle::setValue(string& senseName, int val) {
    this->savedValues[senseName] = val;
}

void OneSenseReadPerCycle::resetAllSavedValues() {
    this->savedValues.clear();
}