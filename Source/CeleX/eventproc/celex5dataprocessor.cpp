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

#include "celex5dataprocessor.h"
#include "../base/xbase.h"
#include <iostream>
#include <sstream>

#define EVENT_T_COUNTER  500
#define FORMAT2_T_MAX    4096

#define MAX_FRAME_COUNT  2

using namespace std;

unsigned char* g_pFrameBuffer_Full;
unsigned char* g_pFrameBuffer_Event1;
unsigned char* g_pFrameBuffer_Event2;
unsigned char* g_pFrameBuffer_Event3;
unsigned char* g_pFrameBuffer_OpticalFlow1;
unsigned char* g_pFrameBuffer_OpticalFlow2;
unsigned char* g_pFrameBuffer_OpticalFlow3;

vector<EventData> g_vecEventData;

CeleX5DataProcessor::CeleX5DataProcessor()
	: m_uiPixelCount(0)
	, m_uiEventTCounter(0)
	, m_iLastRow(-1)
	, m_iCurrentRow(-1)
	, m_uiRowCount(0)
	, m_bIsGeneratingFPN(false)
	, m_iFpnCalculationTimes(-1)
	, m_bLoopModeEnabled(false)
	, m_uiISOLevel(2)
	, m_bShowEventImage(false)
	, m_iLastRowTimeStamp(-1)
	, m_iRowTimeStamp(-1)
	, m_uiEventCountForShow(100000)
	, m_emCurrentSensorMode(CeleX5::Event_Address_Only_Mode)
	, m_emSensorFixedMode(CeleX5::Event_Address_Only_Mode)
	, m_emSensorLoopAMode(CeleX5::Event_Address_Only_Mode)
	, m_emSensorLoopBMode(CeleX5::Full_Picture_Mode)
	, m_emSensorLoopCMode(CeleX5::Full_Optical_Flow_S_Mode)
	, m_uiEventFrameTime(30)
	, m_iCycleCount(0)
	, m_uiClockRate(40)
	, m_emEventShowMethod(CeleX5DataProcessor::ShowByTime)
	, m_iRowCycleCount(6)
	, m_uiFrameCounter(0)
	, m_uiFrameCountPS(0)
	, m_cltFrameTDiff(0)
	, m_cltFrameBeginT(0)
	, m_iRotateType(0)
{
	m_pFPGADataReader = new CeleX5DataReader;
	m_pEventADCBuffer = new unsigned char[CELEX5_PIXELS_NUMBER];
	m_pEventCountBuffer = new unsigned char[CELEX5_PIXELS_NUMBER];
	m_pLastADC = new unsigned char[CELEX5_PIXELS_NUMBER];

	g_pFrameBuffer_Full = new unsigned char[CELEX5_PIXELS_NUMBER];
	g_pFrameBuffer_Event1 = new unsigned char[CELEX5_PIXELS_NUMBER];
	g_pFrameBuffer_Event2 = new unsigned char[CELEX5_PIXELS_NUMBER];
	g_pFrameBuffer_Event3 = new unsigned char[CELEX5_PIXELS_NUMBER];
	g_pFrameBuffer_OpticalFlow1 = new unsigned char[CELEX5_PIXELS_NUMBER];
	g_pFrameBuffer_OpticalFlow2 = new unsigned char[CELEX5_PIXELS_NUMBER];
	g_pFrameBuffer_OpticalFlow3 = new unsigned char[CELEX5_PIXELS_NUMBER];

	m_pFpnGenerationBuffer = new long[CELEX5_PIXELS_NUMBER];
	m_pFpnBuffer = new int[CELEX5_PIXELS_NUMBER];

	for (int i = 0; i < CELEX5_PIXELS_NUMBER; i++)
	{
		m_pEventADCBuffer[i] = 0;
		m_pEventCountBuffer[i] = 0;
		m_pLastADC[i] = 0;
		//
		m_pFpnGenerationBuffer[i] = 0;
		m_pFpnBuffer[i] = 0;
	}

	m_pCX5ProcessedData = new CeleX5ProcessedData;
	m_pCX5Server = new CX5SensorDataServer;
	m_pCX5Server->setCX5SensorData(m_pCX5ProcessedData);

	XBase base;
	std::string filePath = base.getApplicationDirPath();
	filePath += "\\FPN_2.txt";
	setFpnFile(filePath);
}

CeleX5DataProcessor::~CeleX5DataProcessor()
{
	if (m_pFPGADataReader) delete m_pFPGADataReader;
	if (m_pCX5ProcessedData) delete m_pCX5ProcessedData;
	if (m_pCX5Server) delete m_pCX5Server;

	if (m_pEventCountBuffer) delete[] m_pEventCountBuffer;
	if (m_pEventADCBuffer) delete[] m_pEventADCBuffer;
	if (m_pFpnGenerationBuffer) delete[] m_pFpnGenerationBuffer;
	if (m_pFpnBuffer) delete[] m_pFpnBuffer;
}

void CeleX5DataProcessor::getFullPicBuffer(unsigned char* buffer)
{
	memcpy(buffer, g_pFrameBuffer_Full, CELEX5_PIXELS_NUMBER);
}

void CeleX5DataProcessor::getEventPicBuffer(unsigned char* buffer, CeleX5::emEventPicType type)
{
	if (type == CeleX5::EventBinaryPic)
		memcpy(buffer, g_pFrameBuffer_Event1, CELEX5_PIXELS_NUMBER);
	else if (type == CeleX5::EventGrayPic)
		memcpy(buffer, g_pFrameBuffer_Event2, CELEX5_PIXELS_NUMBER);
	else if (type == CeleX5::EventAccumulatedPic)
		memcpy(buffer, g_pFrameBuffer_Event3, CELEX5_PIXELS_NUMBER);
}

void CeleX5DataProcessor::getOpticalFlowPicBuffer(unsigned char* buffer, CeleX5::emFullPicType type)
{
	if (type == CeleX5::Full_Optical_Flow_Pic)
		memcpy(buffer, g_pFrameBuffer_OpticalFlow1, CELEX5_PIXELS_NUMBER);
	else if (type == CeleX5::Full_Optical_Flow_Speed_Pic)
		memcpy(buffer, g_pFrameBuffer_OpticalFlow2, CELEX5_PIXELS_NUMBER);
	else if (type == CeleX5::Full_Optical_Flow_Direction_Pic)
		memcpy(buffer, g_pFrameBuffer_OpticalFlow3, CELEX5_PIXELS_NUMBER);
}

bool CeleX5DataProcessor::getEventDataVector(std::vector<EventData> &vector)
{
	vector = g_vecEventData;
	return true;
}

bool CeleX5DataProcessor::getEventDataVector(std::vector<EventData> &vector, uint64_t& frameNo)
{
	vector = g_vecEventData;
	return true;
}

void CeleX5DataProcessor::processData(unsigned char* data, long length)
{
	if (!data)
	{
		return;
	}
	for (long i = 0; i + EVENT_SIZE <= length; i = i + EVENT_SIZE)
	{
		bool isSpecialEvent = !(processEvent(&data[i]));
		if (isSpecialEvent)
		{
			if (m_bIsGeneratingFPN)
			{
				generateFPNimpl();
			}
			else
			{
				createImage();
				m_pCX5Server->notify(CeleX5DataManager::CeleX_Frame_Data);
			}
		}
	}
}

bool CeleX5DataProcessor::processEvent(unsigned char* data)
{
	if (m_pFPGADataReader->isSpecialEvent(data))
	{
		//cout << "CeleX5DataProcessor::isSpecialEvent£º m_uiPixelCount = " << m_uiPixelCount << endl;
		if (m_bLoopModeEnabled)
		{
			m_emCurrentSensorMode = (CeleX5::CeleX5Mode)m_pFPGADataReader->getSensorMode();
			//cout << "Loop: m_emCurrentSensorMode = " << m_pFPGADataReader->getSensorMode() << ", SpecialEvent: m_uiPixelCount = " << m_uiPixelCount << endl;
			if (m_uiPixelCount > 0)
			{
				m_uiPixelCount = 0;
				return false;
			}
		}
		else
		{
			if (m_emCurrentSensorMode == CeleX5::Full_Picture_Mode ||
				m_emCurrentSensorMode == CeleX5::Full_Optical_Flow_S_Mode ||
				m_emCurrentSensorMode == CeleX5::Full_Optical_Flow_Test_Mode ||
				m_emCurrentSensorMode == CeleX5::Full_Optical_Flow_M_Mode)
			{
				//cout << "SpecialEvent: m_uiPixelCount = " << m_uiPixelCount << endl;
				m_uiPixelCount = 0;
#ifdef _WIN32
				clock_t t2 = clock();
#else
				clock_t t2 = clock()/1000;
#endif // _WIN32
				m_cltFrameTDiff += (t2 - m_cltFrameBeginT);
				m_cltFrameBeginT = t2;
				m_uiFrameCounter++;
				if (m_cltFrameTDiff > 1000)
				{
					m_uiFrameCountPS = m_uiFrameCounter;
					m_cltFrameTDiff = 0;
					m_uiFrameCounter = 0;
				}
				return false;
			}
		}
	}
	else
	{
		/*if (m_emSensorFixedMode == CeleX5::Event_Address_Only_Mode ||
			m_emSensorFixedMode == CeleX5::Event_Optical_Flow_Mode ||
			m_emSensorFixedMode == CeleX5::Event_Intensity_Mode)
		{
			if (m_pFPGADataReader->isRowEvent(data))
			{
				return processRowEvent(data);
			}
			else if (m_pFPGADataReader->isColumnEvent(data))
			{
				return processColEvent(data);
			}
		}*/
		if (m_emCurrentSensorMode == CeleX5::Event_Address_Only_Mode ||
			m_emCurrentSensorMode == CeleX5::Event_Optical_Flow_Mode ||
			m_emCurrentSensorMode == CeleX5::Event_Intensity_Mode)
		{
			unsigned int col1 = 2000;
			unsigned int col2 = 2000;
			if (m_emCurrentSensorMode == CeleX5::Event_Address_Only_Mode)
			{
				bool bRowEvent = m_pFPGADataReader->parseFPGAEvent(data, &col1, &col2);
				unsigned int row = m_pFPGADataReader->row();
				if (bRowEvent)
					processEventRow(row);
				unsigned int t = m_pFPGADataReader->getTFromFPGA();
				processFPGAColEvent(row, col1, 0, t);
				processFPGAColEvent(row, col2, 0, t);
			}
			else
			{
				unsigned int adc1 = 0;
				unsigned int adc2 = 0;
				bool bRowEvent = m_pFPGADataReader->parseFPGAEventEx(data, &col1, &adc1, &col2, &adc2);
				unsigned int row = m_pFPGADataReader->row();
				if (bRowEvent)
					processEventRow(row);
				unsigned int t = m_pFPGADataReader->getTFromFPGA();
				processFPGAColEvent(row, col1, adc1, t);
				processFPGAColEvent(row, col2, adc2, t);
			}
			if (!m_bLoopModeEnabled)
			{
				if (m_emEventShowMethod == CeleX5DataProcessor::ShowByEventCount)
				{
					if (m_uiPixelCount >= m_uiEventCountForShow)
					{
						//cout << "------------------ m_uiPixelCount = " << m_uiPixelCount << endl;
						m_uiPixelCount = 0;
						return false;
					}
				}
				else if (m_emEventShowMethod == CeleX5DataProcessor::ShowByRow)
				{
					if (m_iCycleCount >= m_iRowCycleCount && m_uiPixelCount > 10000)
					{
						//cout << "-------------- m_uiPixelCount = " << m_uiPixelCount << ", m_uiRowCount = " << m_uiRowCount << endl;
						m_uiPixelCount = 0;
						m_uiRowCount = 0;
						m_iCycleCount = 0;
						return false;
					}
				}
				else if (m_emEventShowMethod == CeleX5DataProcessor::ShowByTime)
				{
					if (m_uiEventTCounter >= m_uiEventFrameTime * 1000 * m_uiClockRate / 128)
					{
						m_uiPixelCount = 0;
						m_uiRowCount = 0;
						m_uiEventTCounter = 0;

						//cout << __FUNCTION__ << ": m_vecEventData.size() = " << m_vecEventData.size() << endl;
						g_vecEventData.clear();
						g_vecEventData = m_vecEventData;
						m_vecEventData.clear();
						return false;
					}
				}
			}
		}
		else
		{
			if (m_uiPixelCount + 3 < CELEX5_PIXELS_NUMBER)
			{
				m_pEventADCBuffer[m_uiPixelCount] = 255 - (int)data[0];
				m_pEventADCBuffer[m_uiPixelCount + 1] = 255 - (int)data[1];
				m_pEventADCBuffer[m_uiPixelCount + 2] = 255 - (int)data[2];
				m_pEventADCBuffer[m_uiPixelCount + 3] = 255 - (int)data[3];
			}
			m_uiPixelCount += 4;
			//cout << (int)m_pEventADCBuffer[m_uiPixelCount] << " ";
		}
	}
	return true;
}


CX5SensorDataServer *CeleX5DataProcessor::getSensorDataServer()
{
	return m_pCX5Server;
}

CeleX5ProcessedData *CeleX5DataProcessor::getProcessedData()
{
	return m_pCX5ProcessedData;
}


bool CeleX5DataProcessor::processFPGAColEvent(unsigned int row, unsigned int col, unsigned int adc, unsigned int time_stamp)
{
	EventData eventData;
	if (row < CELEX5_ROW && col < CELEX5_COL)
	{
		// normalize to 0~255
		//adc = normalizeADC(adc);
		adc = (adc >> 2);
		int index = row * CELEX5_COL + col;
		if (m_emCurrentSensorMode == CeleX5::Event_Address_Only_Mode) //000
		{
			eventData.row = row;
			eventData.col = col;
			eventData.brightness = adc;
			eventData.t = time_stamp;

			m_pEventCountBuffer[index] += 1;
		}
		else if (m_emCurrentSensorMode == CeleX5::Event_Optical_Flow_Mode) //001
		{
			eventData.row = row;
			eventData.col = col;
			eventData.brightness = adc;
			eventData.t = time_stamp;
			m_pEventADCBuffer[index] = 255 - adc;
		}
		else if (m_emCurrentSensorMode == CeleX5::Event_Intensity_Mode) //010
		{
			eventData.row = row;
			eventData.col = col;
			eventData.brightness = adc;
			eventData.t = time_stamp;

			if (adc > m_pLastADC[index])
				eventData.polarity = 1;
			else if (adc < m_pLastADC[index])
				eventData.polarity = -1;
			else
				eventData.polarity = 0;
			m_pLastADC[index] = adc;

			m_pEventADCBuffer[index] = 255 - adc;
			m_pEventCountBuffer[index] += 1;
		}
		else if (m_emCurrentSensorMode == CeleX5::Full_Picture_Mode ||
			     m_emCurrentSensorMode == CeleX5::Full_Optical_Flow_S_Mode ||
			     m_emCurrentSensorMode == CeleX5::Full_Optical_Flow_M_Mode)
		{
			m_pEventADCBuffer[index] = 255 - adc;
		}
		m_vecEventData.push_back(eventData);
		m_uiPixelCount++;
	}
	return true;
}

bool CeleX5DataProcessor::createImage()
{
	unsigned char* pFullPic = m_pCX5ProcessedData->getFullPicBuffer();
	unsigned char* pOpticalFlowPic = m_pCX5ProcessedData->getOpticalFlowPicBuffer(CeleX5::Full_Optical_Flow_Pic);
	unsigned char* pOpticalFlowSpeedPic = m_pCX5ProcessedData->getOpticalFlowPicBuffer(CeleX5::Full_Optical_Flow_Speed_Pic);
	unsigned char* pOpticalFlowDirectionPic = m_pCX5ProcessedData->getOpticalFlowPicBuffer(CeleX5::Full_Optical_Flow_Direction_Pic);

	unsigned char* pEventBinaryPic = m_pCX5ProcessedData->getEventPicBuffer(CeleX5::EventBinaryPic);
	unsigned char* pEventGrayPic = m_pCX5ProcessedData->getEventPicBuffer(CeleX5::EventGrayPic);
	unsigned char* pEventAccumulatedPic = m_pCX5ProcessedData->getEventPicBuffer(CeleX5::EventAccumulatedPic);

	m_pCX5ProcessedData->setSensorMode(m_emCurrentSensorMode);

	if (m_bLoopModeEnabled)
	{
		if (m_emCurrentSensorMode == m_emSensorLoopAMode)
		{
			m_pCX5ProcessedData->setLoopNum(1);
		}
		else if (m_emCurrentSensorMode == m_emSensorLoopBMode)
		{
			m_pCX5ProcessedData->setLoopNum(2);
		}
		else if (m_emCurrentSensorMode == m_emSensorLoopCMode)
		{
			m_pCX5ProcessedData->setLoopNum(3);
		}
	}
	else
	{
		m_pCX5ProcessedData->setLoopNum(-1);
	}
	int value = 0, value1 = 0, index = 0;
	if (m_emCurrentSensorMode == CeleX5::Full_Picture_Mode)
	{
		for (int i = 0; i < CELEX5_PIXELS_NUMBER; i++)
		{
			index = getCurrentIndex(i);

			if (m_pEventADCBuffer[i] != 255)
			{
				value = m_pEventADCBuffer[i] - m_pFpnBuffer[i]; //Subtract FPN
			}
			else
			{
				value = m_pEventADCBuffer[i] - (m_pFpnBuffer[i] >> 2);
			}
			if (value < 0)
				value = 0;
			if (value > 255)
				value = 255;

			pFullPic[index] = value;
		}
		memcpy(g_pFrameBuffer_Full, pFullPic, CELEX5_PIXELS_NUMBER);
	}
	else if (m_emCurrentSensorMode == CeleX5::Event_Address_Only_Mode)
	{
		for (int i = 0; i < CELEX5_PIXELS_NUMBER; i++)
		{
			index = getCurrentIndex(i);

			if (m_pEventCountBuffer[i] > 0)
			{
				pEventBinaryPic[index] = 255;
				m_pEventCountBuffer[i] = 0;
			}
			else
			{
				pEventBinaryPic[index] = 0;
			}
		}
		memcpy(g_pFrameBuffer_Event1, pEventBinaryPic, CELEX5_PIXELS_NUMBER);
	}
	else if (m_emCurrentSensorMode == CeleX5::Event_Optical_Flow_Mode)
	{
		for (int i = 0; i < CELEX5_PIXELS_NUMBER; i++)
		{
			index = getCurrentIndex(i);

			if (m_pEventADCBuffer[i] > 0)
			{
				pOpticalFlowPic[index] = m_pEventADCBuffer[i];
				m_pEventADCBuffer[i] = 0;
			}
			else
			{
				pOpticalFlowPic[index] = 0;
			}
		}
		memcpy(g_pFrameBuffer_OpticalFlow1, pOpticalFlowPic, CELEX5_PIXELS_NUMBER);
	}
	else if (m_emCurrentSensorMode == CeleX5::Event_Intensity_Mode)
	{
		for (int i = 0; i < CELEX5_PIXELS_NUMBER; i++)
		{
			index = getCurrentIndex(i);

			if (m_pEventADCBuffer[i] > 0)
			{
				value = m_pEventADCBuffer[i] - m_pFpnBuffer[i];
				if (value < 0)
					value = 0;
				if (value > 255)
					value = 255;
				pEventBinaryPic[index] = 255;
				pEventGrayPic[index] = value;
				pEventAccumulatedPic[index] = value; //don't need to clear
				m_pEventADCBuffer[i] = 0;
			}
			else
			{
				pEventBinaryPic[index] = 0;
				pEventGrayPic[index] = 0;
			}
		}
		memcpy(g_pFrameBuffer_Event1, pEventBinaryPic, CELEX5_PIXELS_NUMBER);
		memcpy(g_pFrameBuffer_Event2, pEventGrayPic, CELEX5_PIXELS_NUMBER);
		memcpy(g_pFrameBuffer_Event3, pEventAccumulatedPic, CELEX5_PIXELS_NUMBER);
	}
	else if (m_emCurrentSensorMode == CeleX5::Full_Optical_Flow_S_Mode ||
		m_emCurrentSensorMode == CeleX5::Full_Optical_Flow_M_Mode)
	{
		for (int i = 0; i < CELEX5_PIXELS_NUMBER; i++)
		{
			index = getCurrentIndex(i);

			int value = m_pEventADCBuffer[i];
			//cout << "m_pEventADCBuffer: " << value << endl;
			if (value == 255)
				value = 0;
			pOpticalFlowPic[index] = value;

			calDirectionAndSpeed(i, index, m_pEventADCBuffer, pOpticalFlowSpeedPic, pOpticalFlowDirectionPic);
		}
		memcpy(g_pFrameBuffer_OpticalFlow1, pOpticalFlowPic, CELEX5_PIXELS_NUMBER);
		memcpy(g_pFrameBuffer_OpticalFlow2, pOpticalFlowSpeedPic, CELEX5_PIXELS_NUMBER);
		memcpy(g_pFrameBuffer_OpticalFlow3, pOpticalFlowDirectionPic, CELEX5_PIXELS_NUMBER);

	}
	else if (m_emCurrentSensorMode == CeleX5::Full_Optical_Flow_Test_Mode)
	{
		for (int i = 0; i < CELEX5_PIXELS_NUMBER; i++)
		{
			int value = m_pEventADCBuffer[i];
			pOpticalFlowPic[CELEX5_PIXELS_NUMBER - i - 1] = value;
		}
		memcpy(g_pFrameBuffer_OpticalFlow1, pOpticalFlowPic, CELEX5_PIXELS_NUMBER);
	}
	return true;
}

// normalize to 0~255
unsigned int CeleX5DataProcessor::normalizeADC(unsigned int adc)
{
	int brightness = adc;
	if (adc < 0)
		brightness = 0;
	else if (adc > 4096)
		brightness = 255;
	else
		brightness = 255 * adc / 4096 - 0;
	return brightness;
}

void CeleX5DataProcessor::generateFPNimpl()
{
	for (int i = 0; i < CELEX5_PIXELS_NUMBER; ++i)
	{
		if (m_iFpnCalculationTimes == FPN_CALCULATION_TIMES)
		{
			m_pFpnGenerationBuffer[i] = m_pEventADCBuffer[i];
		}
		else
		{
			m_pFpnGenerationBuffer[i] += m_pEventADCBuffer[i];
		}
	}
	--m_iFpnCalculationTimes;

	if (m_iFpnCalculationTimes <= 0)
	{
		m_bIsGeneratingFPN = false;
		std::ofstream ff;
		if (m_strFpnFilePath.empty())
		{
			XBase base;
			std::string filePath = base.getApplicationDirPath();
#ifdef _WIN32
			filePath += "/";
#endif 
			if (1 == m_uiISOLevel)
				filePath += "FPN_1.txt";
			else if (2 == m_uiISOLevel)
				filePath += "FPN_2.txt";
			else if (3 == m_uiISOLevel)
				filePath += "FPN_3.txt";
			else if (4 == m_uiISOLevel)
				filePath += "FPN_4.txt";
			else
				filePath += "FPN.txt";
			// output the FPN file now
			ff.open(filePath.c_str());
		}
		else
		{
			ff.open(m_strFpnFilePath.c_str());
		}
		if (!ff)
			return;
		long total = 0;
		for (int i = 0; i < CELEX5_PIXELS_NUMBER; ++i)
		{
			m_pFpnGenerationBuffer[i] = m_pFpnGenerationBuffer[i] / FPN_CALCULATION_TIMES;
			total += m_pFpnGenerationBuffer[i];
		}
		int avg = total / CELEX5_PIXELS_NUMBER;
		for (int i = 0; i < CELEX5_PIXELS_NUMBER; ++i)
		{
			int d = m_pFpnGenerationBuffer[i] - avg;
			ff << d << "  ";
			if ((i + 1) % 1280 == 0)
				ff << "\n";
		}
		ff.close();
	}
}

bool CeleX5DataProcessor::setFpnFile(const std::string& fpnFile)
{
	int index = 0;
	std::ifstream in;
	in.open(fpnFile.c_str());
	if (!in.is_open())
	{
		return false;
	}
	std::string line;
	while (!in.eof() && index < CELEX5_PIXELS_NUMBER)
	{
		in >> line;
		m_pFpnBuffer[index] = atoi(line.c_str());
		//cout << index << ", " << m_pFpnBuffer[index] << endl;
		index++;
	}
	if (index != CELEX5_PIXELS_NUMBER)
		return false;

	cout << "fpn count = " << index << endl;

	in.close();
	return true;
}

void CeleX5DataProcessor::generateFPN(std::string filePath)
{
	m_bIsGeneratingFPN = true;
	m_iFpnCalculationTimes = FPN_CALCULATION_TIMES;
	m_strFpnFilePath = filePath;
}

CeleX5::CeleX5Mode CeleX5DataProcessor::getSensorFixedMode()
{
	return m_emSensorFixedMode;
}

CeleX5::CeleX5Mode CeleX5DataProcessor::getSensorLoopMode(int loopNum)
{
	if (1 == loopNum)
		return m_emSensorLoopAMode;
	else if (2 == loopNum)
		return m_emSensorLoopBMode;
	else if (3 == loopNum)
		return m_emSensorLoopCMode;
	else
		return CeleX5::Unknown_Mode;
}

void CeleX5DataProcessor::setSensorFixedMode(CeleX5::CeleX5Mode mode)
{
	m_emCurrentSensorMode = m_emSensorFixedMode = mode;
}

void CeleX5DataProcessor::setSensorLoopMode(CeleX5::CeleX5Mode mode, int loopNum)
{
	if (1 == loopNum)
		m_emSensorLoopAMode = mode;
	else if (2 == loopNum)
		m_emSensorLoopBMode = mode;
	else if (3 == loopNum)
		m_emSensorLoopCMode = mode;
}

void CeleX5DataProcessor::setLoopModeEnabled(bool enable)
{
	m_bLoopModeEnabled = enable;
	m_uiEventTCounter = 0;
	if (!enable)
		m_emCurrentSensorMode = m_emSensorFixedMode = CeleX5::Event_Address_Only_Mode;
}

void CeleX5DataProcessor::setISOLevel(uint32_t value)
{
	m_uiISOLevel = value;
}

void CeleX5DataProcessor::setEventCountForShow(uint32_t value)
{
	m_uiEventCountForShow = value;
}

void CeleX5DataProcessor::setEventFrameTime(uint32_t value)
{
	m_uiEventFrameTime = value;
}
uint32_t CeleX5DataProcessor::getEventFrameTime()
{
	return m_uiEventFrameTime;
}

int CeleX5DataProcessor::calculateDenoiseScore(unsigned char* pBuffer, unsigned int pos)
{
	if (NULL == pBuffer)
	{
		return 255;
	}
	int row = pos / CELEX5_COL;
	int col = pos % CELEX5_COL;

	int count1 = 0;
	int count2 = 0;
	for (int i = row - 1; i < row + 2; ++i) //8 points
	{
		for (int j = col - 1; j < col + 2; ++j)
		{
			int index = i * CELEX5_COL + j;
			if (index < 0 || index == pos || index >= CELEX5_PIXELS_NUMBER)
				continue;
			if (pBuffer[index] > 0)
				++count1;
			else
				++count2;
		}
	}
	if (count1 >= count2)
		return 255;
	else
		return 0;
}

void CeleX5DataProcessor::calDirectionAndSpeed(int i, int j, unsigned char* pBuffer, unsigned char* &speedBuffer, unsigned char* &dirBuffer)
{
	int row = i / CELEX5_COL;
	int col = i % CELEX5_COL;
	int Gx = 0, Gy = 0;

	if (col == 0 || col == CELEX5_COL - 1)
		Gx = 0;
	else
		Gx = pBuffer[i + 1] - pBuffer[i - 1];

	if (row == 0 || row == CELEX5_ROW - 1)
		Gy = 0;
	else
		Gy = pBuffer[i + CELEX5_COL] - pBuffer[i - CELEX5_COL];

	int theta = 0;
	if (Gx == 0 && Gy == 0)
	{
		theta = 0;
	}
	else
	{
		if (Gx == 0)
		{
			if (Gy > 0)
				theta = 90;
			else
				theta = 270;
		}
		else
		{
			theta = atan2(Gy, Gx) * 180 / CV_PI;
		}
	}
	if (theta < 0)
		theta += 360;
	dirBuffer[j] = theta * 255 / 360;

	int value1 = sqrt(Gx*Gx + Gy*Gy);
	if (value1 > 255)
		value1 = 255;
	speedBuffer[j] = value1;
}

int CeleX5DataProcessor::getCurrentIndex(int initIndex)
{
	switch (m_iRotateType)
	{
	case 0:
		return CELEX5_PIXELS_NUMBER - initIndex - 1;
		break;
	case 1:
		return (initIndex / CELEX5_COL * CELEX5_COL + (CELEX5_COL - initIndex % CELEX5_COL - 1));
		break;
	case 2:
		return ((CELEX5_ROW - initIndex / CELEX5_COL - 1)* CELEX5_COL + initIndex % CELEX5_COL);
		break;
	case 3:
		return (initIndex / CELEX5_COL * CELEX5_COL + initIndex % CELEX5_COL);
		break;
	}
}

void CeleX5DataProcessor::setRotateType(int type)
{
	m_iRotateType = type;
}

void CeleX5DataProcessor::setClockRate(int value)
{
	m_uiClockRate = value;
}

uint32_t CeleX5DataProcessor::getFullFrameFPS()
{
	return m_uiFrameCountPS;
}

int CeleX5DataProcessor::calMean(unsigned char* pBuffer, unsigned int pos)
{
	if (NULL == pBuffer)
	{
		return 255;
	}
	int row = pos / CELEX5_COL;
	int col = pos % CELEX5_COL;
	//cout << "--- " << row << ", col" << col << endl;
	int index = 0;
	int count1 = 0;

	int index1 = (row - 1)*PIXELS_PER_COL + col;
	int index2 = row*PIXELS_PER_COL + col - 1;
	int index3 = row*PIXELS_PER_COL + col + 1;
	int index4 = (row + 1)*PIXELS_PER_COL + col;
	int aa[4] = { index1, index2, index3, index4 };
	int value = 255;
	for (int i = 0; i < 4; ++i)
	{
		if (aa[i] < 0 || aa[i] >= PIXELS_NUMBER)
			continue;
		if (pBuffer[aa[i]] > 250)
		{
			++count1;
			value = pBuffer[aa[i]];
		}

	}
	if (count1 > 0)
	{
		//cout << "----------------- " << count1 << endl;
		return 255;
	}
	else
		return value;
}

void CeleX5DataProcessor::processEventRow(int row)
{
	m_iLastRow = m_iCurrentRow;
	m_iCurrentRow = row;
	m_uiRowCount++;
	if (m_iCurrentRow < m_iLastRow)
	{
		//cout << "currentRow = " << m_iCurrentRow << ", lastRow = " << m_iLastRow << endl;
		m_iCycleCount++;
	}
	unsigned int t = m_pFPGADataReader->getTFromFPGA();
	unsigned int tLast = m_pFPGADataReader->getLastTFromFPGA();
	int diffT = t - tLast;
	if (diffT < 0)
	{
		//cout << "t = " << t << ", tLast = " << tLast << endl;
		if (m_emCurrentSensorMode == CeleX5::Event_Address_Only_Mode)
			diffT = diffT + 32;
		else
			diffT = diffT + 2048;
	}
	m_uiEventTCounter += diffT;
}

void CeleX5DataProcessor::setRowCycleCount(int count)
{
	m_iRowCycleCount = count;
}