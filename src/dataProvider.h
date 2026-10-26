#pragma once
#ifndef DATAPROVIDER_H
#define DATAPROVIDER_H
#include"fanControl.h"
#include"fanData.h"
#include <vector>
#include <Windows.h>
#include <stdexcept>

class SharedMemory {
    private:
        HANDLE m_hMap = nullptr;
        void* m_data = nullptr;
        size_t m_size = 0;

    public:
        SharedMemory(
            const wchar_t* name,
            size_t size) {
            m_size = size;

            m_hMap = CreateFileMappingW(
                INVALID_HANDLE_VALUE,
                nullptr,
                PAGE_READWRITE,
                0,
                static_cast<DWORD>(size),
                name
            );

            if (!m_hMap) {
                throw std::runtime_error(
                    "CreateFileMapping failed"
                );
            }

            m_data = MapViewOfFile(
                m_hMap,
                FILE_MAP_ALL_ACCESS,
                0,
                0,
                size
            );

            if (!m_data) {
                CloseHandle(m_hMap);

                throw std::runtime_error(
                    "MapViewOfFile failed"
                );
            }
        }

        ~SharedMemory() {
            if (m_data) {
                UnmapViewOfFile(m_data);
            }

            if (m_hMap) {
                CloseHandle(m_hMap);
            }
        }

        template<typename T> T* GetData() {
            return static_cast<T*>(m_data);
        }
};

class fanDataProvider {
	private:
		vector<SetFans*> setFans;
		SharedMemory sharedMemory;
		fanData* fanDataList;
	public:
		fanDataProvider(vector<SetFans*>);
		void readFansData();
		~fanDataProvider();
};

#endif