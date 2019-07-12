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

#include "datarecorder.h"
#include "../base/xbase.h"

#ifdef _WIN32
#include <Windows.h>
CRITICAL_SECTION    g_csDataRecorder;
#else
#endif

uint8_t* pData;
DataRecorder::DataRecorder()
	: m_bRecording(false)
	, m_iTimeStampStart(0)
{
	pData = new uint8_t[1536001];

#ifdef _WIN32
	InitializeCriticalSection(&g_csDataRecorder);
#else
#endif
}

DataRecorder::~DataRecorder()
{
}

bool DataRecorder::isRecording()
{
	return m_bRecording;
}

void DataRecorder::startRecording(std::string filePath)
{
	XBase base;
	m_iTimeStampStart = base.getTimeStamp();

	m_ofstreamRecord.open(filePath.c_str(), std::ios::binary);
	if (!m_ofstreamRecord.is_open())
		cout << "Can't open recording file." << endl;

	CeleX5::BinFileAttributes header;
	m_ofstreamRecord.write((char*)&header, sizeof(CeleX5::BinFileAttributes));

	m_bRecording = true;
}

void DataRecorder::stopRecording(CeleX5::BinFileAttributes* header)
{
#ifdef _WIN32
	EnterCriticalSection(&g_csDataRecorder);
#endif

	XBase base;

	int iTimeRecorded = base.getTimeStamp() - m_iTimeStampStart;

	int hour = iTimeRecorded / 3600;
	int minute = (iTimeRecorded % 3600) / 60;
	int second = (iTimeRecorded % 3600) % 60;

	cout << iTimeRecorded << endl;

	// write a header
	header->hour = hour;
	header->minute = minute;
	header->second = second;

	m_ofstreamRecord.seekp(0, ios::beg);
	m_ofstreamRecord.write((char*)header, sizeof(CeleX5::BinFileAttributes));

	//m_ofstreamRecord.flush();
	m_bRecording = false;
	m_ofstreamRecord.close();

#ifdef _WIN32
	LeaveCriticalSection(&g_csDataRecorder);
#endif
}

void DataRecorder::writeData(unsigned char* pData, long length)
{
#ifdef _WIN32
	EnterCriticalSection(&g_csDataRecorder);
#endif

	m_ofstreamRecord.write((char*)pData, length);

#ifdef _WIN32
	LeaveCriticalSection(&g_csDataRecorder);
#endif
}