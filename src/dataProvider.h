#pragma once
#ifndef DATAPROVIDER_H
#define DATAPROVIDER_H
#include"fanControl.h"

struct fanData {
	int fanPwmUnfiltered;
	int fanPwmFiltered;
	int fanRpm;
};

class fanDataProvider {
	private:
		SetFans* setFans;
		vector<fanData*> fanDataList;
	public:
		fanDataProvider(SetFans* setFans);
		~fanDataProvider();
};

#endif