/*
* Copyright (c) 2017-2018  CelePixel Technology Co. Ltd.  All rights reserved.
*
* Licensed under the Apache License, Version 2.0 (the "License");
* you may not use this file except in compliance with the License.
* You may obtain a copy of the License at
*
*      http://www.apache.org/licenses/LICENSE-2.0
*
* Unless required by applicable law or agreed to in writing, software
* distributed under the License is distributed on an "AS IS" BASIS,
* WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
* See the License for the specific language governing permissions and
* limitations under the License.
*/

#include "dataprocessthread.h"
#include <iostream>

using namespace std;

DataProcessThread::DataProcessThread(const std::string &name)
	: XThread(name)
	, m_emPlaybackState(CeleX4::NoBinPlaying)
{
	//m_queueDataEx.allocMemory(1024000 * 100);
	//m_pData = new unsigned char[2048000*10];
}

DataProcessThread::~DataProcessThread()
{
	delete[] m_pData;
	m_pData = NULL;
}

void DataProcessThread::addData(unsigned char *data, long length)
{
    m_queueData.push(data, length);
	//m_queueDataEx.push(data, length);
}

void DataProcessThread::clearData()
{
    m_queueData.clear();
}

void DataProcessThread::setSensorMode(emSensorMode mode)
{
    m_queueData.clear();
    m_dataProcessor.setSensorMode(mode);
}

uint32_t DataProcessThread::queueSize()
{
    return m_queueData.size();
}

FPGADataProcessor *DataProcessThread::getDataProcessor()
{
	return &m_dataProcessor;
}

CeleX4::PlaybackState DataProcessThread::getPlaybackState()
{
	return m_emPlaybackState;
}

void DataProcessThread::setPlaybackState(CeleX4::PlaybackState state)
{
	m_emPlaybackState = state;
	m_dataProcessor.setPlaybackState(state);
}

void DataProcessThread::run()
{
    while (m_bRun)
    {
        //cout << "---------- DataProcessThread::run ----------" << endl;
#ifndef _WIN32
        pthread_mutex_lock(&m_mutex);
        while (m_bSuspended)
        {
            pthread_cond_wait(&m_cond, &m_mutex);
        }
        pthread_mutex_unlock(&m_mutex);
#endif
		if (m_queueData.size() > 0)
		{
			unsigned char* data = 0;
			long dataLen = 0;
			m_queueData.pop(data, &dataLen);
			if (dataLen > 0)
			{
				m_dataProcessor.processData(data, dataLen);
				delete[] data;
			}
		}
		else
		{
			if (m_emPlaybackState == CeleX4::BinReadFinished)
			{
				m_emPlaybackState = CeleX4::PlayFinished;
				m_dataProcessor.setPlaybackState(CeleX4::PlayFinished);
			}
		}
		/*if (m_queueDataEx.size() > 0)
		{
		uint32_t dataLen = 0;
		m_queueDataEx.pop(m_pData, &dataLen);
		if (dataLen > 0)
		{
		cout << "dataLen = " << dataLen << endl;
		m_dataProcessor.processData(m_pData, dataLen);
		}
		}
		else
		{
		if (m_emPlaybackState == CeleX4::BinReadFinished)
		{
		m_emPlaybackState = CeleX4::PlayFinished;
		m_dataProcessor.setPlaybackState(CeleX4::PlayFinished);
		}
		}*/
    }
}


DataProcessThreadEx::DataProcessThreadEx(const std::string &name)
	: XThread(name)
	, m_uiPackageNo(0)
	, m_emDeviceType(CeleX5::Unknown_Devive)
	, m_bPlaybackBinFile(false)
{
    m_pData = new unsigned char[655360];
}

DataProcessThreadEx::~DataProcessThreadEx()
{
	delete[] m_pData;
	m_pData = NULL;
}

void DataProcessThreadEx::addData(unsigned char *data, long length)
{
	m_queueData.push(data, length);
}

void DataProcessThreadEx::addData(vector<uint8_t> vecData)
{
	//cout << "size = " << vecData.size() << endl;
	if (vecData.size() > 0)
	{
		vector<uint8_t> vec;
		vec.swap(vecData);
		//cout << "DataProcessThread::addData = " << vec.size() << endl;
		m_queueVecData.push(vec);
	}
	//cout << m_queueVecData.size() << endl;
}

void DataProcessThreadEx::clearData()
{
	m_queueData.clear();
	m_uiPackageNo = 0;
}

uint32_t DataProcessThreadEx::queueSize()
{
	return m_queueData.size();
}

uint32_t DataProcessThreadEx::packageNo()
{
	return m_uiPackageNo;
}

void DataProcessThreadEx::setPackageNo(uint32_t no)
{
	m_uiPackageNo = no;
}

CeleX5DataProcessor *DataProcessThreadEx::getDataProcessor5()
{
	return &m_dataProcessor5;
}

void DataProcessThreadEx::setDeviceType(CeleX5::DeviceType type)
{
	m_emDeviceType = type;
}

void DataProcessThreadEx::setCeleX(CeleX5* pCeleX5)
{
	m_pCeleX5 = pCeleX5;
}

void DataProcessThreadEx::setPlayback()
{
	m_bPlaybackBinFile = true;
}

void DataProcessThreadEx::run()
{
	while (m_bRun)
	{
		//cout << "---------- DataProcessThread::run ----------" << endl;
#ifndef _WIN32
		pthread_mutex_lock(&m_mutex);
		while (m_bSuspended)
		{
			pthread_cond_wait(&m_cond, &m_mutex);
		}
		pthread_mutex_unlock(&m_mutex);
#endif
		if (CeleX5::CeleX5_MIPI == m_emDeviceType)
		{
			//--- for real display ---
			//m_pCeleX5->getMIPIData(m_vecMIPIPackage);
			//cout << "------------------" << m_vecMIPIPackage.size() << endl;
			if (m_bPlaybackBinFile)
			{
				//for playback
				if (m_queueData.size() > 0)
				{
					uint8_t* data = 0;
					long dataLen = 0;
					m_queueData.pop(data, &dataLen);
					if (dataLen > 0)
					{
						m_dataProcessor5.processMIPIData(vector<uint8_t>(), data, dataLen, true);
						m_uiPackageNo++;
						delete[] data;
					}
				}
				//m_vecMIPIPackage.clear();
			}
			else
			{
				/*if (m_vecMIPIPackage.size() > 0)
				{
					m_dataProcessor5.processMIPIData(m_vecMIPIPackage, NULL, 0, false);
					m_vecMIPIPackage.clear();
				}*/
				if (m_queueVecData.size() > 0)
				{
					vector<uint8_t> vecData;
					long dataLen = 0;
					vecData = m_queueVecData.front();
					m_queueVecData.pop();
					if (vecData.size() > 0)
					{
						m_dataProcessor5.processMIPIData(vecData, NULL, 0, false);
					}
				}
			}
		}
		//if (CeleX5::CeleX5_OpalKelly == m_emDeviceType)
		else
		{
			if (m_queueData.size() > 0)
			{
				long dataLen = 0;
				m_queueData.pop(m_pData, &dataLen);
				if (dataLen > 0)
				{
					m_dataProcessor5.processData(m_pData, dataLen);
				}
				//cout << "dataLen = " << dataLen << endl;
			}
		}
	}
}
