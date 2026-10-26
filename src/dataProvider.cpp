#include "dataProvider.h"

// Create shared memory for fan data, allocated memory size is size of fanData struct * number of fans
fanDataProvider::fanDataProvider(vector<SetFans*> setFans) : sharedMemory(SharedMemoryName, sizeof(fanData) * setFans.size()), setFans(setFans) {
	fanDataList = this->sharedMemory.GetData<fanData>();
}

fanDataProvider::~fanDataProvider() {
	delete[] this->fanDataList;
}	

void fanDataProvider::readFansData() {
	for (int i = 0; i < this->setFans.size(); ++i) {
		this->fanDataList[i].fanPwmUnfiltered = this->setFans[i]->dataProvider_unfilteredFanPwm;
		this->fanDataList[i].fanPwmFiltered = this->setFans[i]->dataProvider_filteredFanPwm;
		this->fanDataList[i].fanRpm = this->setFans[i]->dataProvider_fanRpm;
		this->fanDataList[i].fanCount = this->setFans.size();
	}
}
