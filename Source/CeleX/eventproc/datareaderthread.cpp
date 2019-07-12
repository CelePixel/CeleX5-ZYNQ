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

#include "datareaderthread.h"
#include <iostream>
//#include <Windows.h>
#include "../include/celex5/celex5.h"

using namespace std;

DataReaderThread::DataReaderThread(CeleX5* pSensor, const std::string &name)
	: XThread(name)
	, m_bPipeoutAllowed(false)
	, m_pCeleX5(pSensor)
{

}

DataReaderThread::~DataReaderThread()
{

}

void DataReaderThread::startReadData(bool bRead)
{
    m_bPipeoutAllowed = bRead;
}

void DataReaderThread::run()
{
    while (m_bRun)
    {
		m_pCeleX5->getSensorRawData(&m_bufferRecv[0], 4096);
    }
}