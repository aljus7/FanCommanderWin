#pragma once
#ifndef FANDATA_H
#define FANDATA_H

struct fanData {
	int fanPwmUnfiltered = 0;
	int fanPwmFiltered = 0;
	int fanRpm = 0;
	int fanCount = 0;
};

constexpr wchar_t SharedMemoryName[] =
L"Global\\fanDataSharedMemory";

#endif