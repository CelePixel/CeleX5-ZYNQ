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

unsigned char* g_pFrameBuffer_Full[MAX_FRAME_COUNT];
unsigned char* g_pFrameBuffer_Event1[MAX_FRAME_COUNT];
unsigned char* g_pFrameBuffer_Event2[MAX_FRAME_COUNT];
unsigned char* g_pFrameBuffer_Event3[MAX_FRAME_COUNT];
unsigned char* g_pFrameBuffer_OpticalFlow1[MAX_FRAME_COUNT];
unsigned char* g_pFrameBuffer_OpticalFlow2[MAX_FRAME_COUNT];
unsigned char* g_pFrameBuffer_OpticalFlow3[MAX_FRAME_COUNT];
static int s_write_index = 0;

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
	, m_bShowEventImage(false)
	, m_uiColGainValue(2)
	, m_iLastRowTimeStamp(-1)
	, m_iRowTimeStamp(-1)
	, m_iMIPIDataFormat(2)
	, m_uiEventCountForShow(100000)
	, m_emSensorFixedMode(CeleX5::Event_Address_Only_Mode)
	, m_emSensorLoopAMode(CeleX5::Event_Address_Only_Mode)
	, m_emSensorLoopBMode(CeleX5::Full_Picture_Mode)
	, m_emSensorLoopCMode(CeleX5::Full_Optical_Flow_S_Mode)
	, m_bTFinished(false)
	, m_uiEventFrameTime(30)
	, m_uiEventOverlappedTime(0)
    , m_iCycleCount(0)
	, m_bBeginSaveFullPic(false)
	, m_emEventShowMethod(CeleX5DataProcessor::ShowByRow)
	, m_iRowCycleCount(6)
{
	m_pFPGADataReader = new CeleX5DataReader;
	m_pEventADCBuffer = new unsigned char[CELEX5_PIXELS_NUMBER];
	m_pEventCountBuffer = new unsigned char[CELEX5_PIXELS_NUMBER];

	for (int i = 0; i < MAX_FRAME_COUNT; i++)
	{
        g_pFrameBuffer_Full[i] = new unsigned char[CELEX5_PIXELS_NUMBER];
        g_pFrameBuffer_Event1[i] = new unsigned char[CELEX5_PIXELS_NUMBER];
        g_pFrameBuffer_Event2[i] = new unsigned char[CELEX5_PIXELS_NUMBER];
        g_pFrameBuffer_Event3[i] = new unsigned char[CELEX5_PIXELS_NUMBER];
        g_pFrameBuffer_OpticalFlow1[i] = new unsigned char[CELEX5_PIXELS_NUMBER];
        g_pFrameBuffer_OpticalFlow2[i] = new unsigned char[CELEX5_PIXELS_NUMBER];
        g_pFrameBuffer_OpticalFlow3[i] = new unsigned char[CELEX5_PIXELS_NUMBER];
	}

	m_pFpnGenerationBuffer = new long[CELEX5_PIXELS_NUMBER];
	m_pFpnBuffer = new int[CELEX5_PIXELS_NUMBER];

	for (int i = 0; i < CELEX5_PIXELS_NUMBER; i++)
	{
		m_pEventADCBuffer[i] = 0;
		m_pEventCountBuffer[i] = 0;
		//
		m_pFpnGenerationBuffer[i] = 0;
		m_pFpnBuffer[i] = 0;
	}

	m_pCX5ProcessedData = new CeleX5ProcessedData;
	m_pCX5Server = new CX5SensorDataServer;
	m_pCX5Server->setCX5SensorData(m_pCX5ProcessedData);

	//XBase base;
	//std::string filePath = base.getApplicationDirPath();
	std::string filePath = "/opt/FPN.txt";
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
    if (s_write_index == 0)
        memcpy(buffer, g_pFrameBuffer_Full[1], CELEX5_PIXELS_NUMBER);
    else
        memcpy(buffer, g_pFrameBuffer_Full[0], CELEX5_PIXELS_NUMBER);
}

void CeleX5DataProcessor::getEventPicBuffer(unsigned char* buffer, CeleX5::emEventPicType type)
{
    int index = 0;
    if (s_write_index == 0)
        index = 1;
    else
        index = 0;

	if (type == CeleX5::EventBinaryPic)
		memcpy(buffer, g_pFrameBuffer_Event1[index], CELEX5_PIXELS_NUMBER);
	else if (type == CeleX5::EventGrayPic)
		memcpy(buffer, g_pFrameBuffer_Event2[index], CELEX5_PIXELS_NUMBER);
	else if (type == CeleX5::EventAccumulatedPic)
		memcpy(buffer, g_pFrameBuffer_Event3[index], CELEX5_PIXELS_NUMBER);
}

void CeleX5DataProcessor::getOpticalFlowPicBuffer(unsigned char* buffer, CeleX5::emFullPicType type)
{
    int index = 0;
    if (s_write_index == 0)
        index = 1;
    else
        index = 0;

	if (type == CeleX5::Full_Optical_Flow_Pic)
		memcpy(buffer, g_pFrameBuffer_OpticalFlow1[index], CELEX5_PIXELS_NUMBER);
	else if (type == CeleX5::Full_Optical_Flow_Speed_Pic)
		memcpy(buffer, g_pFrameBuffer_OpticalFlow2[index], CELEX5_PIXELS_NUMBER);
	else if (type == CeleX5::Full_Optical_Flow_Direction_Pic)
		memcpy(buffer, g_pFrameBuffer_OpticalFlow3[index], CELEX5_PIXELS_NUMBER);
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
				//m_pCX5Server->notify(CeleX5DataManager::CeleX_Frame_Data);
			}
		}
	}
}

bool CeleX5DataProcessor::processEvent(unsigned char* data)
{
	if (m_pFPGADataReader->isSpecialEvent(data))
	{
        //cout << "CeleX5DataProcessor::isSpecialEvent" << endl;
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
                    ;
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

//vecData[size -1]: data mode = 1: event / 0: fullpic
void CeleX5DataProcessor::processMIPIData(vector<uint8_t> vecData, uint8_t* pData, int size, bool bPlayback)
{
	int dataSize = (bPlayback == true ? size : vecData.size());
	//cout << "CeleX5DataProcessor::processMIPIData: vecData size = " << dataSize << endl;

	if (dataSize < 0)
		return;

	uint8_t dataType = 0;
	if (bPlayback)
	{
		dataType = *(pData + dataSize - 1);
	}
	else
	{
		dataType = vecData[dataSize - 1];
	}
	//cout << "CeleX5DataProcessor::processMIPIData: data mode = " << (int)dataType << endl;
	//if (dataType == 0/* || dataSize > 1000000*/) //full pic mode: package size = 1536001 = 1280 * 800 * 1.5 + 1
	if (dataType == 0/* || dataSize > 500000*/) //full pic mode: package size = 1536001 = 1280 * 800 * 1.5 + 1
	{
		uint8_t mode;
		if (bPlayback)
			mode = 0xFF & *pData;
		else
			mode = 0xFF & vecData.at(0);
		if (mode == 0x60 || mode == 0x80 || mode == 0xA0 || mode == 0xC0 || mode == 0xE0)
		{
			if (m_bLoopModeEnabled || bPlayback)
				m_emCurrentSensorMode = CeleX5::CeleX5Mode((0xE0 & mode) >> 5);
			//cout << "CeleX5DataProcessor::processMIPIData: m_emCurrentSensorMode = " << (int)m_emCurrentSensorMode << endl;
			processFullPicData(vecData, pData, size, bPlayback);
		}
	}
	else
	{
		if (m_iMIPIDataFormat == 0)// Format0 (CSR_73 = 0), Package Size: 1024*200*1.5 = 307200 Byte
		{
			parseEventDataFormat0(vecData, pData, size, bPlayback);
		}
		else if (m_iMIPIDataFormat == 1)// Format1 (CSR_73 = 1), Package Size: 1024*200*1.75 = 358400 Byte
		{
			parseEventDataFormat1(vecData, pData, size, bPlayback);
		}
		else if (m_iMIPIDataFormat == 2)// Format2 (CSR_73 = 2), Package Size: 1024*200*1.75 = 358400 Byte
		{
			parseEventDataFormat2(vecData, pData, size, bPlayback);
		}
	}
}

CX5SensorDataServer *CeleX5DataProcessor::getSensorDataServer()
{
	return m_pCX5Server;
}

CeleX5ProcessedData *CeleX5DataProcessor::getProcessedData()
{
	return m_pCX5ProcessedData;
}

bool CeleX5DataProcessor::processRowEvent(unsigned char* data)
{
	m_pFPGADataReader->parseRowEvent(data);
	unsigned int t = m_pFPGADataReader->getTFromFPGA();
	//cout << "row = " << m_pFPGADataReader->row() << ",  = " << t << endl

	if (!m_bLoopModeEnabled) //fixed mode
	{
		if (m_emCurrentSensorMode == CeleX5::Event_Address_Only_Mode ||
			m_emCurrentSensorMode == CeleX5::Event_Optical_Flow_Mode ||
			m_emCurrentSensorMode == CeleX5::Event_Intensity_Mode)
		{
			if (m_emEventShowMethod == CeleX5DataProcessor::ShowByRow)
			{
				m_iLastRow = m_iCurrentRow;
				m_iCurrentRow = m_pFPGADataReader->row();
				m_uiRowCount++;
				if (m_iCurrentRow < m_iLastRow)
				{
					m_iCycleCount++;
				}
				if (m_iCycleCount >= m_iRowCycleCount)
				{
					m_uiRowCount = 0;
					m_iCycleCount = 0;
					return false;
				}
			}
			else if (m_emEventShowMethod == CeleX5DataProcessor::ShowByTime)
			{
				unsigned int t = m_pFPGADataReader->getTFromFPGA();
				unsigned int tLast = m_pFPGADataReader->getLastTFromFPGA();
				int diffT = t - tLast;
				if (diffT < 0)
				{
					//cout << "t = " << t << ", tLast = " << tLast << endl;
					diffT = diffT + 262144;
				}
				m_uiEventTCounter += diffT;
				if (m_uiEventTCounter > 25000 * m_uiEventFrameTime)
				{
					//cout << "m_uiEventTCounter = " << m_uiEventTCounter << endl;
					m_uiEventTCounter = 0;
					return false;
				}
			}
		}
	}
	return true;
}

bool CeleX5DataProcessor::processColEvent(unsigned char* data)
{
	//m_pFPGADataReader->parseColumnEvent(data);
	//
	unsigned int row = m_pFPGADataReader->row();
	unsigned int col = m_pFPGADataReader->column();
	unsigned int adc = m_pFPGADataReader->adc();
	unsigned int t = m_pFPGADataReader->getTFromFPGA();

	//cout << "row = " << row << ", col = " << col << endl;
	//cout << "adc = " << adc << endl;
	if (row < CELEX5_ROW && col < CELEX5_COL)
	{
		// normalize to 0~255
		//adc = normalizeADC(adc);
		adc = (adc >> 2);
		int index = row * CELEX5_COL + col;
		unsigned int type = m_pFPGADataReader->getEventType();
		//m_emCurrentSensorMode = CeleX5::CeleX5Mode(type);
		//cout << "type = " << type << endl;
		if (m_emCurrentSensorMode == CeleX5::Event_Address_Only_Mode) //000
		{
			m_pEventCountBuffer[index] += 1;
		}
		else if (m_emCurrentSensorMode == CeleX5::Event_Optical_Flow_Mode) //001
		{
			m_pEventADCBuffer[index] = 255 - adc;
		}
		else if (m_emCurrentSensorMode == CeleX5::Event_Intensity_Mode) //010
		{
			m_pEventADCBuffer[index] = 255 - adc;
			m_pEventCountBuffer[index] += 1;
		}
		else if (m_emCurrentSensorMode == CeleX5::Full_Picture_Mode ||
			m_emCurrentSensorMode == CeleX5::Full_Optical_Flow_S_Mode ||
			m_emCurrentSensorMode == CeleX5::Full_Optical_Flow_M_Mode)
		{
			m_pEventADCBuffer[index] = 255 - adc;
		}
		m_uiPixelCount++;

		if (m_emEventShowMethod == CeleX5DataProcessor::ShowByEventCount)
		{
			if (m_uiPixelCount > 100000)
			{
				cout << "------------------ m_uiPixelCount = " << m_uiPixelCount << endl;
				m_uiPixelCount = 0;
				return false;
			}
		}
	}
	return true;
}

bool CeleX5DataProcessor::processFPGAColEvent(unsigned int row, unsigned int col, unsigned int adc, unsigned int time_stamp)
{
	if (row < CELEX5_ROW && col < CELEX5_COL)
	{
		// normalize to 0~255
		//adc = normalizeADC(adc);
		adc = (adc >> 2);
		int index = row * CELEX5_COL + col;
		if (m_emCurrentSensorMode == CeleX5::Event_Address_Only_Mode) //000
		{
			m_pEventCountBuffer[index] += 1;
		}
		else if (m_emCurrentSensorMode == CeleX5::Event_Optical_Flow_Mode) //001
		{
			m_pEventADCBuffer[index] = 255 - adc;
		}
		else if (m_emCurrentSensorMode == CeleX5::Event_Intensity_Mode) //010
		{
			m_pEventADCBuffer[index] = 255 - adc;
			m_pEventCountBuffer[index] += 1;
		}
		else if (m_emCurrentSensorMode == CeleX5::Full_Picture_Mode ||
			m_emCurrentSensorMode == CeleX5::Full_Optical_Flow_S_Mode ||
			m_emCurrentSensorMode == CeleX5::Full_Optical_Flow_M_Mode)
		{
			m_pEventADCBuffer[index] = 255 - adc;
		}
		m_uiPixelCount++;
	}
	return true;
}

void CeleX5DataProcessor::processFullPicData(vector<uint8_t> vecData, uint8_t* pData, int size, bool bPlayback)
{
	if (!bPlayback)
	{
		//---------- for test ----------
		if (m_bBeginSaveFullPic)
		{
			saveOpticalFlowData(vecData);
		}
		if (m_bIsGeneratingFPN)
		{
			generateOpticalFlowFPN(vecData);
		}
		//------------------------------
	}
	m_uiEventTCounter = 0;
	m_iLastRowTimeStamp = -1;
	m_iRowTimeStamp = -1;

	int dataSize = bPlayback == true ? size : vecData.size();
	if (dataSize != 1536001)
	{
		cout << "CeleX5DataProcessor::processFullPicData: Not a full package!" << endl;
		//return;
	}
	int index = 0;
	for (int i = 0; i < dataSize - 2; i += 3)
	{
		uint8_t value1, value2, value3;
		if (bPlayback)
		{
			value1 = *(pData + i);
			value2 = *(pData + i + 1);
			value3 = *(pData + i + 2);
		}
		else
		{
			value1 = vecData.at(i);
			value2 = vecData.at(i + 1);
			value3 = vecData.at(i + 2);
		}
		uint8_t adc1 = value1;
		uint8_t adc2 = value2;

		if (m_emCurrentSensorMode == CeleX5::Full_Picture_Mode ||
			m_emCurrentSensorMode == CeleX5::Full_Optical_Flow_S_Mode ||
			m_emCurrentSensorMode == CeleX5::Full_Optical_Flow_M_Mode ||
			m_emCurrentSensorMode == CeleX5::Full_Optical_Flow_Test_Mode)
		{
			m_pEventADCBuffer[index] = 255 - adc1;
			m_pEventADCBuffer[index + 1] = 255 - adc2;
		}
		index += 2;
		if (index == CELEX5_PIXELS_NUMBER)
		{
			break;
		}
	}
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

void CeleX5DataProcessor::parseEventDataFormat0(vector<uint8_t> vecData, uint8_t* pData, int size, bool bPlayback)
{
	int dataSize = vecData.size();
	if (dataSize != 307201)
	{
		cout << "CeleX5DataProcessor::parseEventDataFormat0: Not a full package!" << endl;
	}
	int index = 0;
	int row = 0;
	int col = 0;
	int adc = 0;
	int temperature = 0;
	uint32_t row_time_stamp = 0;
	for (int i = 0; i < dataSize - 2; i += 3)
	{
		uint8_t value1 = vecData.at(i);
		uint8_t value2 = vecData.at(i + 1);
		uint8_t value3 = vecData.at(i + 2);

		//dataID[1:0] = data[1:0] = value3[1:0]
		uint8_t dataID = (0x03 & value3);

		if (dataID == 2) //b2'10: row_addr[9:0] & row_time_stamp[11:0]
		{
			//row[9:0] = data[23:14] = value2[7:0] + value3[7:6]
			row = (value2 << 2) + ((0xC0 & value3) >> 6);

			//row_time_stamp[11:0] = data[13:2] = value3[5:4] + value1[7:0] + value3[3:2]
			row_time_stamp = ((0x30 & value3) << 6) + (value1 << 2) + ((0x0C & value3) >> 2);
			//cout << "Format 0: " << "row = " << row << ", row_time_stamp = " << row_time_stamp << endl;
		}
		else if (dataID == 1) //b2'01: col_addr[10:0] & adc[7:0]
		{
			//adc[7:0] = data[23:16] = value2[7:0]
			adc = value2;

			//col[10:0] = data[15:5] = value3[7:4] + value1[7:1]
			col = ((0xF0 & value3) << 3) + ((0xFE & value1) >> 1);

			//packet_format[2:0] = data[4:2] = value1[0] + value3[3:2]
			int packet_format = ((0x01 & value1) << 2) + ((0x0C & value3) >> 2);
			//cout << "format 0: " << "adc = " << adc << ", col = " << col << ", packet_format = " << packet_format << endl;

			index = row * CELEX5_COL + col;
			m_pEventCountBuffer[index] += 1;
			m_pEventADCBuffer[index] = adc;
		}
		else if (dataID == 0)
		{
			//mark[6:0] = data[23:17] = value2[7:1] = 2b'0000000
			int mark = (0xFE & value2) >> 1;

			//op_mode[2:0] = data[16:14] = value2[0] + value3[7:6]
			int op_mode = ((0x01 & value2) << 2) + ((0xC0 & value3) >> 6);
			m_emCurrentSensorMode = (CeleX5::CeleX5Mode)op_mode;

			//temperature[11:0] = data[13:2] = value3[5:4] + value1[7:0] + value3[3:2]
			temperature = ((0x30 & value3) << 6) + (value1 << 2) + ((0x0C & value3) >> 2);

			//cout << "Format 0:  mark = " << mark << ", op_mode = " << op_mode << ", temperature = " << temperature << endl;
		}
		//if (m_uiRowCount > 800 * 5)
		//{
		//	m_uiRowCount = 0;
		//	//cout << "-------------- m_uiPixelCount = " << m_uiPixelCount << endl;
		//	createImage();
		//	m_pCX5Server->notify(CeleX5DataManager::CeleX_Frame_Data);
		//}
	}
	createImage();
	m_pCX5Server->notify(CeleX5DataManager::CeleX_Frame_Data);
}

void CeleX5DataProcessor::parseEventDataFormat1(vector<uint8_t> vecData, uint8_t* pData, int size, bool bPlayback)
{
	int dataSize = (bPlayback == true ? size : vecData.size());
	if (dataSize != 357001) //358401
	{
		if (!m_bLoopModeEnabled)
		{
			cout << "CeleX5DataProcessor::parseEventDataFormat1: Not a full package = " << dataSize << endl;
		}
	}
	int index = 0;
	int row = 0;
	int col = 0;
	int adc = 0;
	int temperature = 0;
	uint32_t row_time_stamp = 0;
	for (int i = 0; i < dataSize - 6; i += 7)
	{
		uint8_t value1, value2, value3, value4, value5, value6, value7;
		if (bPlayback)
		{
			value1 = *(pData + i);
			value2 = *(pData + i + 1);
			value3 = *(pData + i + 2);
			value4 = *(pData + i + 3);
			value5 = *(pData + i + 4);
			value6 = *(pData + i + 5);
			value7 = *(pData + i + 6);
		}
		else
		{
			value1 = vecData.at(i);
			value2 = vecData.at(i + 1);
			value3 = vecData.at(i + 2);
			value4 = vecData.at(i + 3);
			value5 = vecData.at(i + 4);
			value6 = vecData.at(i + 5);
			value7 = vecData.at(i + 6);
		}
		//------------------------- Package A -------------------------
		//---------- dataID1[1:0] = data[1:0] = value5[1:0] ----------
		uint8_t dataID1 = 0x03 & value5;
		if (dataID1 == 2) //2b'10
		{
			//row_addr[9:0] = data_28_1[27:18] = value2[7:0] + value6[3:2]
			row = (value2 << 2) + ((0x0C & value6) >> 2);
			processEventRow(row);

			//row_time_stamp[15:0] = data_28_1[17:2] = value6[1:0] + value5[7:6] + value1[7:0] + value5[5:2]
			row_time_stamp = ((0x03 & value6) << 14) + ((0xC0 & value5) << 6) + (value1 << 4) + ((0x3C & value5) >> 2);
			//cout << "Format 1 (Package_A): " << "row = " << row << ", row_time_stamp = " << row_time_stamp << endl;
			processMIPIEventTimeStamp(row_time_stamp);
		}
		else if (dataID1 == 1) //2b'01
		{
			//adc[11:0] = data_28_1[27:16] = value2[7:0] + value6[3:0]
			adc = (value2 << 4) + (0x0F & value6);
			//col[10:0] = data_28_1[15:5] = value5[7:6] + value1[7:0] + value5[5]
			col = ((0xC0 & value5) << 3) + (value1 << 1) + ((0x20 & value5) >> 5);
			processMIPIEventCol(col, adc, true);
		}
		else if (dataID1 == 0) //2b'00
		{
			//mark[10:0] = data_28_1[27:17] = value2[7:0] + value6[3:1] = 2b'0000 0000 000
			int mark = (value2 << 3) + ((0x0E & value6) >> 1);

			//op_mode[2:0] = data_28_1[16:14] = value6[0] + value5[7:6]
			int op_mode = ((0x01 & value6) << 2) + ((0xC0 & value5) >> 6);
			if (m_bLoopModeEnabled)
				m_emCurrentSensorMode = CeleX5::Event_Address_Only_Mode;
			//m_emCurrentSensorMode = (CeleX5::CeleX5Mode)op_mode;

			//temperature[11:0] = data_28_1[13:2] = value1[7:0] + value5[5:2]
			temperature = (value1 << 4) + ((0x3C & value5) >> 2);
			//cout << "Format 1 (Package_A): mark = " << mark << ", op_mode = " << op_mode << ", temperature = " << temperature << endl;
			m_pCX5ProcessedData->setTemperature(temperature);
		}

		//------------------------- Package B -------------------------
		//---------- dataID2[1:0] = data[1:0] = value6[5:4] ----------
		uint8_t dataID2 = (0x30 & value6) >> 4;
		if (dataID2 == 2) //2b'10
		{
			//row_addr[9:0] = data_28_2[27:18] = (value4[7:0] << 2) + (value7[7:6] >> 6)
			row = (value4 << 2) + ((0xC0 & value7) >> 6);
			processEventRow(row);

			//row_time_stamp[15:0] = data_28_1[17:2] = value7[5:2] + value3[7:0] + value7[1:0] + value6[7:6]
			row_time_stamp = ((0x3C & value7) << 10) + (value3 << 4) + ((0x03 & value7) << 2) + ((0xC0 & value6) >> 6);
			//cout << "Format 1 (Package_B): " << "row = " << row << ", row_time_stamp = " << row_time_stamp << endl;
			processMIPIEventTimeStamp(row_time_stamp);
		}
		else if (dataID2 == 1) //2b'01
		{
			//adc[11:0] = data_28_2[27:16] = value4[7:0] + value7[7:4]
			adc = (value4 << 4) + ((0xF0 & value7) >> 4);
			//col[10:0] = data_28_2[15:5] = value7[3:2] + value3[7:0]  + value7[1]
			col = ((0x0C & value7) << 7) + (value3 << 1) + ((0x02 & value7) >> 1);
			processMIPIEventCol(col, adc, true);
		}
		else if (dataID2 == 0) //2b'00
		{
			//mark[10:0] = data_28_2[27:17] = value4[7:0] + value7[7:5] = 2b'0000 0000 000
			int mark = (value4 << 3) + ((0xE0 & value7) >> 5);

			//op_mode[2:0] = data_28_2[16:14] = value7[4:2]
			int op_mode = ((0x1C & value7) >> 2);
			if (m_bLoopModeEnabled)
				m_emCurrentSensorMode = CeleX5::Event_Address_Only_Mode;
			//m_emCurrentSensorMode = (CeleX5::CeleX5Mode)op_mode;

			//temperature[11:0] = data[13:2] = value3[7:0] + value7[1:0] + value6[7:6]
			temperature = (value3 << 4) + ((0x03 & value7) << 2) + ((0xC0 & value6) >> 6);
			//cout << "Format 1 (Package_B): mark = " << mark << ", op_mode = " << op_mode << ", temperature = " << temperature << endl;
			m_pCX5ProcessedData->setTemperature(temperature);
		}

		//---------- show event image ----------
		showMIPIEventImage();
	}
	if (m_bLoopModeEnabled)
	{
		m_uiEventTCounter = 0;
		createImage();
		m_pCX5Server->notify(CeleX5DataManager::CeleX_Frame_Data);
		//cout << "m_uiEventTCounter = " << m_uiEventTCounter << endl;

		cout << "parseEventDataFormat1: m_vecEventData.size() = " << m_vecEventData.size() << endl;
		m_vecEventData.clear();
	}
}

void CeleX5DataProcessor::parseEventDataFormat2(vector<uint8_t> vecData, uint8_t* pData, int size, bool bPlayback)
{
	int dataSize = (bPlayback == true ? size : vecData.size());
	if (dataSize != 357001) //358401
	{
		if (!m_bLoopModeEnabled)
		{
			cout << "CeleX5DataProcessor::parseEventDataFormat2: Not a full package = " << dataSize << endl;
			//return;
		}
	}
	int row = 0;
	int col = 0;
	int adc = 0;
	int temperature = 0;
	int row_time_stamp = 0;
	if (m_bLoopModeEnabled)
		m_emCurrentSensorMode = CeleX5::Event_Address_Only_Mode;
	for (int i = 0; i < dataSize - 6; i += 7)
	{
		uint8_t value1, value2, value3, value4, value5, value6, value7;
		if (bPlayback)
		{
			value1 = *(pData + i);
			value2 = *(pData + i + 1);
			value3 = *(pData + i + 2);
			value4 = *(pData + i + 3);
			value5 = *(pData + i + 4);
			value6 = *(pData + i + 5);
			value7 = *(pData + i + 6);
		}
		else
		{
			value1 = vecData.at(i);
			value2 = vecData.at(i + 1);
			value3 = vecData.at(i + 2);
			value4 = vecData.at(i + 3);
			value5 = vecData.at(i + 4);
			value6 = vecData.at(i + 5);
			value7 = vecData.at(i + 6);
		}
		//--------------- Parse Package A ---------------
		//--- dataID1[1:0] = data[1:0] = value5[1:0] ---
		uint8_t dataID1 = 0x03 & value5;
		if (dataID1 == 0x02)
		{
			//row_addr[9:0] = data_14_1[13:4] = value1[7:0] + value5[5:4]
			row = (value1 << 2) + ((0x30 & value5) >> 4);
			//cout << "Package A: " << "row = " << m_iCurrentRow << endl;
			processEventRow(row);
		}
		else if (dataID1 == 0x01)
		{
			//col_addr[10:0] = data_14_1[13:3] = value1[7:0] + value5[5:3]
			col = (value1 << 3) + ((0x38 & value5) >> 3);
			processMIPIEventCol(col, 0, false);
		}
		else if (dataID1 == 0x00)
		{
			//temperature[11:0] = data_14_1[13:2] =  value1[7:0] + value5[5:2]
			temperature = (value1 << 4) + ((0x3C & value5) >> 2);
			//cout << "Format 2 time full (Package_A): temperature = " << temperature << endl;
			m_pCX5ProcessedData->setTemperature(temperature);
		}
		else if (dataID1 == 0x03)
		{
			//row_time_stamp[11:0] = data_14_1[13:2] =  value1[7:0] + value5[5:2]
			row_time_stamp = (value1 << 4) + ((0x3C & value5) >> 2);
			//cout << "Format 2 (Package_A): row_time_stamp = " << m_iRowTimeStamp << endl;
			processMIPIEventTimeStamp(row_time_stamp);
		}

		//--------------- Parse Package B ---------------
		//--- dataID2[1:0] = data[1:0] = value5[7:6] ---
		uint8_t dataID2 = 0xC0 & value5;
		if (dataID2 == 0x80)
		{
			//row_addr[9:0] = data_14_2[13:4] = value2[7:0] + value6[3:2]
			row = (value2 << 2) + ((0x0C & value6) >> 2);
			//cout << "Package B: " << "row = " << m_iCurrentRow << endl;
			processEventRow(row);
		}
		else if (dataID2 == 0x40)
		{
			//col_addr[10:0] = data_14_2[13:3] = value2[7:0] + value6[3:1]
			col = (value2 << 3) + ((0x0E & value6) >> 1);
			processMIPIEventCol(col, 0, false);
		}
		else if (dataID2 == 0x00)
		{
			//temperature[11:0] = data_14_2[13:2] = value2[7:0] + value6[3:0]
			temperature = (value2 << 4) + (0x0F & value6);
			//cout << "Format 2 time full (Package_B): temperature = " << temperature << endl;
			m_pCX5ProcessedData->setTemperature(temperature);
		}
		else if (dataID2 == 0xC0)
		{
			//row_time_stamp[11:0] = data_14_2[13:2] = value2[7:0] + value6[3:0]
			row_time_stamp = (value2 << 4) + (0x0F & value6);
			//cout << "Format 2 (Package_B): row_time_stamp = " << m_iRowTimeStamp << endl;
			processMIPIEventTimeStamp(row_time_stamp);
		}

		//--------------- Parse Package C ---------------
		//--- dataID3[1:0] = data[1:0] = value6[5:4] ---
		uint8_t dataID3 = 0x30 & value6;
		if (dataID3 == 0x20)
		{
			//row_addr[9:0] = data_14_3[13:4] = value3[7:0] + value7[1:0]
			row = (value3 << 2) + (0x03 & value7);
			//cout << "Package C: " << "row = " << m_iCurrentRow << endl;
			processEventRow(row);
		}
		else if (dataID3 == 0x10)
		{
			//col_addr[10:0] = data_14_3[13:3] = value3[7:0] + value7[1:0] + value6[7]
			col = (value3 << 3) + ((0x03 & value7) << 1) + ((0x80 & value6) >> 7);
			processMIPIEventCol(col, 0, false);
		}
		else if (dataID3 == 0x00)
		{
			//temperature[11:0] = data_14_3[13:2] = value3[7:0]+ value7[1:0] + value6[7:6]
			temperature = (value3 << 4) + ((0x03 & value7) << 2) + ((0xC0 & value6) >> 6);
			//cout << "Format 2 time full (Package_C): temperature = " << temperature << endl;
			m_pCX5ProcessedData->setTemperature(temperature);
		}
		else if (dataID3 == 0x30)
		{
			//row_time_stamp[11:0] = data_14_3[13:2] = value3[7:0]+ value7[1:0] + value6[7:6]
			row_time_stamp = (value3 << 4) + ((0x03 & value7) << 2) + ((0xC0 & value6) >> 6);
			//cout << "Format 2 (Package_C): row_time_stamp = " << m_iRowTimeStamp << endl;
			processMIPIEventTimeStamp(row_time_stamp);
		}

		//--------------- Parse Package D ---------------
		//--- dataID4[1:0] = data[1:0] = value7[3:2] ---
		uint8_t dataID4 = 0x0C & value7;
		if (dataID4 == 0x08)
		{
			//row_addr[9:0] = data_14_4[13:4] = value4[7:0] + value7[7:6]
			row = (value4 << 2) + ((0xC0 & value7) >> 6);
			//cout << "Package D: " << "row = " << m_iCurrentRow << endl;
			processEventRow(row);
		}
		else if (dataID4 == 0x04)
		{
			//col_addr[10:0] = data_14_4[13:3] = value4[7:0] + value7[7:5]
			col = (value4 << 3) + ((0xE0 & value7) >> 5);
			processMIPIEventCol(col, 0, false);
		}
		else if (dataID4 == 0x00)
		{
			//temperature[11:0] = data_14_4[13:3] = value4[7:0] + value7[7:4]
			temperature = (value4 << 4) + ((0xF0 & value7) >> 4);
			//cout << "Format 2 time full (Package_D): temperature = " << temperature << endl;
			m_pCX5ProcessedData->setTemperature(temperature);
		}
		else if (dataID4 == 0x0C)
		{
			//row_time_stamp[11:0] = data_14_4[13:3] = value4[7:0] + value7[7:4]
			row_time_stamp = (value4 << 4) + ((0xF0 & value7) >> 4);
			//cout << "Format 2 (Package_D): row_time_stamp = " << m_iRowTimeStamp << endl;
			processMIPIEventTimeStamp(row_time_stamp);
		}
		if (!m_bLoopModeEnabled)
		{
			showMIPIEventImage();
		}
	}
	if (m_bLoopModeEnabled)
	{
		m_uiEventTCounter = 0;
		createImage();
		m_pCX5Server->notify(CeleX5DataManager::CeleX_Frame_Data);
		//cout << "m_uiEventTCounter = " << m_uiEventTCounter << endl;
		//cout << "parseEventDataFormat2: m_vecEventData.size() = " << m_vecEventData.size() << endl;
		m_vecEventData.clear();
	}
}

bool CeleX5DataProcessor::createImage()
{
	/*
	unsigned char* pFullPic = pFrameBuffer1[0];
	unsigned char* pOpticalFlowPic = m_pCX5ProcessedData->getOpticalFlowPicBuffer(CeleX5::Full_Optical_Flow_Pic);
	unsigned char* pOpticalFlowSpeedPic = m_pCX5ProcessedData->getOpticalFlowPicBuffer(CeleX5::Full_Optical_Flow_Speed_Pic);
	unsigned char* pOpticalFlowDirectionPic = m_pCX5ProcessedData->getOpticalFlowPicBuffer(CeleX5::Full_Optical_Flow_Direction_Pic);

	unsigned char* pEventBinaryPic = m_pCX5ProcessedData->getEventPicBuffer(CeleX5::EventBinaryPic);
	unsigned char* pEventGrayPic = m_pCX5ProcessedData->getEventPicBuffer(CeleX5::EventGrayPic);
	unsigned char* pEventAccumulatedPic = m_pCX5ProcessedData->getEventPicBuffer(CeleX5::EventAccumulatedPic);
	*/

	//m_pCX5ProcessedData->setSensorMode(m_emCurrentSensorMode);

	//cout << "---------------------------------------------------------- mode = " << int(m_emCurrentSensorMode) << endl;;
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

	if (m_emCurrentSensorMode == CeleX5::Full_Picture_Mode)
	{
        unsigned char* pFullPic = g_pFrameBuffer_Full[s_write_index];
        for (int i = 0; i < CELEX5_PIXELS_NUMBER; i++)
        {
            int value = 0;
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

			pFullPic[CELEX5_PIXELS_NUMBER - i - 1] = value;
        }
	}
	else if (m_emCurrentSensorMode == CeleX5::Event_Address_Only_Mode)
	{
        unsigned char* pEventBinaryPic = g_pFrameBuffer_Event1[s_write_index];
        for (int i = 0; i < CELEX5_PIXELS_NUMBER; i++)
        {
            if (m_pEventCountBuffer[i] > 0)
            {
                pEventBinaryPic[CELEX5_PIXELS_NUMBER - i - 1] = 255;
                m_pEventCountBuffer[i] = 0;
            }
            else
            {
                pEventBinaryPic[CELEX5_PIXELS_NUMBER - i - 1] = 0;
            }
        }
	}
	else if (m_emCurrentSensorMode == CeleX5::Event_Optical_Flow_Mode)
	{
        unsigned char* pOpticalFlowPic = g_pFrameBuffer_OpticalFlow1[s_write_index];
        for (int i = 0; i < CELEX5_PIXELS_NUMBER; i++)
        {
            if (m_pEventADCBuffer[i] > 0)
            {
                pOpticalFlowPic[CELEX5_PIXELS_NUMBER - i - 1] = m_pEventADCBuffer[i];
                m_pEventADCBuffer[i] = 0;
            }
            else
            {
                pOpticalFlowPic[CELEX5_PIXELS_NUMBER - i - 1] = 0;
            }
        }
	}
	else if (m_emCurrentSensorMode == CeleX5::Event_Intensity_Mode)
	{
        unsigned char* pEventBinaryPic = g_pFrameBuffer_Event1[s_write_index];
        unsigned char* pEventGrayPic = g_pFrameBuffer_Event2[s_write_index];
        unsigned char* pEventAccumulatedPic = g_pFrameBuffer_Event3[s_write_index];

        for (int i = 0; i < CELEX5_PIXELS_NUMBER; i++)
        {
            int value = 0;
            if (m_pEventADCBuffer[i] > 0)
			{
				value = m_pEventADCBuffer[i] - m_pFpnBuffer[i];
				if (value < 0)
					value = 0;
				if (value > 255)
					value = 255;
				pEventBinaryPic[CELEX5_PIXELS_NUMBER - i - 1] = 255;
				pEventGrayPic[CELEX5_PIXELS_NUMBER - i - 1] = value;
				pEventAccumulatedPic[CELEX5_PIXELS_NUMBER - i - 1] = value; //don't need to clear
				m_pEventADCBuffer[i] = 0;
			}
			else
			{
				pEventBinaryPic[CELEX5_PIXELS_NUMBER - i - 1] = 0;
				pEventGrayPic[CELEX5_PIXELS_NUMBER - i - 1] = 0;
			}
        }
	}
	else if (m_emCurrentSensorMode == CeleX5::Full_Optical_Flow_S_Mode ||
	         m_emCurrentSensorMode == CeleX5::Full_Optical_Flow_M_Mode)
	{
        unsigned char* pOpticalFlowPic = g_pFrameBuffer_OpticalFlow1[s_write_index];
        for (int i = 0; i < CELEX5_PIXELS_NUMBER; i++)
        {
            int value = m_pEventADCBuffer[i];
			//cout << "m_pEventADCBuffer: " << value << endl;
			if (value == 255)
				value = 0;
			pOpticalFlowPic[CELEX5_PIXELS_NUMBER - i - 1] = value;

			//calDirectionAndSpeed(i, m_pEventADCBuffer, pOpticalFlowSpeedPic, pOpticalFlowDirectionPic);
        }
	}
	else if (m_emCurrentSensorMode == CeleX5::Full_Optical_Flow_Test_Mode)
	{
        unsigned char* pOpticalFlowPic = g_pFrameBuffer_OpticalFlow1[s_write_index];
        for (int i = 0; i < CELEX5_PIXELS_NUMBER; i++)
        {
            int value = m_pEventADCBuffer[i];
			pOpticalFlowPic[CELEX5_PIXELS_NUMBER - i - 1] = value;
        }
	}
	if (s_write_index == 0)
            s_write_index = 1;
        else
            s_write_index = 0;
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
			if (0 == m_uiColGainValue)
				filePath += "\\FPN_Gain0.txt";
			else if (1 == m_uiColGainValue)
				filePath += "\\FPN_Gain1.txt";
			else if (2 == m_uiColGainValue)
				filePath += "\\FPN_Gain2.txt";
			else if (3 == m_uiColGainValue)
				filePath += "\\FPN_Gain3.txt";
			else
				filePath += "\\FPN.txt";
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

void CeleX5DataProcessor::setColGainValue(uint32_t value)
{
	m_uiColGainValue = value;
}

void CeleX5DataProcessor::setMIPIDataFormat(int format)
{
	m_iMIPIDataFormat = format;
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

void CeleX5DataProcessor::setEventFrameOverlappedTime(uint32_t msec)
{
	m_uiEventOverlappedTime = msec;
}
uint32_t CeleX5DataProcessor::getEventFrameOverlappedTime()
{
	return m_uiEventOverlappedTime;
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

void CeleX5DataProcessor::calDirectionAndSpeed(int i, unsigned char* pBuffer, unsigned char* &speedBuffer, unsigned char* &dirBuffer)
{
	int row = i / CELEX5_COL;
	int col = i % CELEX5_COL;
	int Gx = 0, Gy = 0;

	if (col == 0 || col == 1279)
		Gx = 0;
	else
		Gx = pBuffer[i + 1] - pBuffer[i - 1];

	if (row == 0 || row == 799)
		Gy = 0;
	else
		Gy = pBuffer[i + 1280] - pBuffer[i - 1280];

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
	dirBuffer[CELEX5_PIXELS_NUMBER - i - 1] = theta * 255 / 360;

	int value1 = sqrt(Gx*Gx + Gy*Gy);
	if (value1 > 255)
		value1 = 255;
	speedBuffer[CELEX5_PIXELS_NUMBER - i - 1] = value1;
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
}

void CeleX5DataProcessor::processMIPIEventCol(int col, int adc, bool bADCEnabled)
{
	if (m_iCurrentRow < 0)
	{
		return;
	}
	if (col < CELEX5_COL && m_iCurrentRow < CELEX5_ROW)
	{
		int index = m_iCurrentRow * CELEX5_COL + col;
		m_pEventCountBuffer[index] += 1;
		if (bADCEnabled)
			m_pEventADCBuffer[index] = adc >> 4;

		EventData eventData;
		eventData.row = m_iCurrentRow;
		eventData.col = col;
		eventData.brightness = adc;
		eventData.t = m_iRowTimeStamp;
		m_vecEventData.push_back(eventData);
		m_uiPixelCount++;
	}
	else
	{
		//cout << "row = " << m_iCurrentRow << ", col = " << col << endl;
	}
}

void CeleX5DataProcessor::processMIPIEventTimeStamp(int timestamp)
{
	m_iLastRowTimeStamp = m_iRowTimeStamp;
	m_iRowTimeStamp = timestamp;
	int diffT = m_iRowTimeStamp - m_iLastRowTimeStamp;
	if (diffT < 0)
		diffT += FORMAT2_T_MAX;
	if (m_iLastRowTimeStamp != -1)
		m_uiEventTCounter += diffT;

	if (m_uiEventTCounter > EVENT_T_COUNTER)
	{
		//cout << "isEventTCounterFull: m_uiEventTCounter = " << m_uiEventTCounter << endl;
		m_bShowEventImage = true;
		m_uiEventTCounter = 0;
	}
}

void CeleX5DataProcessor::showMIPIEventImage()
{
	if (m_bLoopModeEnabled)
	{
		return;
	}
	if (m_emEventShowMethod == CeleX5DataProcessor::ShowByTime)
	{
		if (m_bShowEventImage)
		{
			m_bShowEventImage = false;
			m_uiPixelCount = 0;
			createImage();
			m_pCX5Server->notify(CeleX5DataManager::CeleX_Frame_Data);

			//cout << "showMIPIEventImage: m_vecEventData.size() = " << m_vecEventData.size() << endl;
			g_vecEventData.clear();
			g_vecEventData = m_vecEventData;
			m_vecEventData.clear();
		}
	}
	else if (m_emEventShowMethod == CeleX5DataProcessor::ShowByRow)
	{
		if (m_iCycleCount >= m_iRowCycleCount && m_uiPixelCount > 10000)
		//if (m_iCycleCount > m_iRowCycleCount && m_uiRowCount > 4000)
		{
			//cout << "-------------- m_uiPixelCount = " << m_uiPixelCount << ", m_uiRowCount = " << m_uiRowCount << endl;
			m_uiPixelCount = 0;
			m_uiRowCount = 0;
			m_iCycleCount = 0;
			createImage();
			m_pCX5Server->notify(CeleX5DataManager::CeleX_Frame_Data);

			//cout << "showMIPIEventImage: m_vecEventData.size() = " << m_vecEventData.size() << endl;
			g_vecEventData.clear();
			g_vecEventData = m_vecEventData;
			m_vecEventData.clear();
		}
	}
	else if (m_emEventShowMethod == CeleX5DataProcessor::ShowByEventCount)
	{
		if (m_uiPixelCount >= m_uiEventCountForShow)
		{
			m_uiPixelCount = 0;
			m_uiRowCount = 0;
			m_iCycleCount = 0;
			createImage();
			m_pCX5Server->notify(CeleX5DataManager::CeleX_Frame_Data);

			g_vecEventData.clear();
			g_vecEventData = m_vecEventData;
			m_vecEventData.clear();
		}
	}
}

void CeleX5DataProcessor::setRowCycleCount(int count)
{
	m_iRowCycleCount = count;
}

void CeleX5DataProcessor::enterTemperature(string strT)
{
	m_strTemperature = strT;
	if (m_ofTemperature.is_open())
		m_ofTemperature.close();
	XBase base;
	std::string filePath = base.getApplicationDirPath();
	filePath += "\\";
	filePath += m_strTemperature;
	filePath += "\\Temperature_";
	filePath += strT;
	filePath += "_Brightness_";
	if (m_strBrightness.empty())
		filePath += "150";
	else
		filePath += m_strBrightness;
	filePath += ".txt";
	m_ofTemperature.open(filePath);
	m_bTFinished = false;
}

void CeleX5DataProcessor::setBrightness(uint32_t value)
{
	std::stringstream ss;
	ss << value;
	m_strBrightness = ss.str();
}

void CeleX5DataProcessor::beginSaveFullPic(std::string filename)
{
	m_bBeginSaveFullPic = true;
}

void CeleX5DataProcessor::stopSaveFullPic()
{
	m_bBeginSaveFullPic = false;
	if (m_ofFullPic.is_open())
		m_ofFullPic.close();
}

void CeleX5DataProcessor::saveOpticalFlowData(vector<uint8_t> vecData)
{
	int dataSize = vecData.size();

	if (!m_ofFullPic.is_open())
		m_ofFullPic.open("test_full_pic.txt");

	/*int index1 = 200 * CELEX5_COL + 320;
	int index2 = 200 * CELEX5_COL + 960;
	int index3 = 600 * CELEX5_COL + 320;
	int index4 = 600 * CELEX5_COL + 960;*/
	int index = 0;
	for (int i = 0; i < dataSize - 2; i += 3)
	{
		uint8_t value1 = vecData.at(i);
		uint8_t value2 = vecData.at(i + 1);
		uint8_t value3 = vecData.at(i + 2);

		uint8_t adc1 = value1;
		uint8_t adc2 = value2;

		//200*320, 200*960, 600*320, 200*960
		/*if (index == index1 || index == index2 || index == index3 || index == index4)
		{
			adc11 = (value1 << 4) + (0x0F & value3);
			uint16_t adc22 = (value2 << 4) + ((0xF0 & value3) >> 4);
			m_ofFullPic << adc11 << " ";
		}*/
		uint16_t adc11 = (value1 << 4) + (0x0F & value3);
		uint16_t adc22 = (value2 << 4) + ((0xF0 & value3) >> 4);
		//m_ofFullPic << adc11 << "  " << adc22 << "  ";
		m_ofFullPic << 255 - (int)value1 << "  " << 255 - (int)value2 << "  ";
		index += 2;
		if (index % 1280 == 0)
			m_ofFullPic << endl;
	}
	m_bBeginSaveFullPic = false;
	m_ofFullPic.close();
}

void CeleX5DataProcessor::generateOpticalFlowFPN(vector<uint8_t> vecData)
{
	return;
	int dataSize = vecData.size();
	ofstream ff;
	ff.open("test_fpn.txt");
	int index = 0;
	for (int i = 0; i < dataSize - 2; i += 3)
	{
		uint8_t value1 = vecData.at(i);
		uint8_t value2 = vecData.at(i + 1);
		uint8_t value3 = vecData.at(i + 2);

		uint16_t adc11 = (value1 << 4) + (0x0F & value3);
		uint16_t adc22 = (value2 << 4) + ((0xF0 & value3) >> 4);
		ff << adc11 << "  " << adc22 << "  ";
		index += 2;
		if (index % 1280 == 0)
			ff << endl;
	}
	m_bIsGeneratingFPN = false;
	ff.close();
}

bool bHaHa = false;
int badCol = -1;
void CeleX5DataProcessor::testBadSensor(vector<uint8_t> vecData)
{
	int dataSize = vecData.size();
	if (dataSize != 1536001)
	{
		cout << "CeleX5DataProcessor::testBadSensor: Not a full package!" << endl;
		return;
	}
	if (bHaHa)
	{
		bHaHa = false;
		int averValue[1280] = { 0 };
		int hahaIndex = 0;
		for (int i = 0; i < dataSize - 2; i += 3)
		{
			uint8_t value1 = vecData.at(i);
			uint8_t value2 = vecData.at(i + 1);
			uint8_t value3 = vecData.at(i + 2);

			uint8_t adc1 = value1;
			uint8_t adc2 = value2;

			averValue[hahaIndex % 1280] += value1;
			averValue[(hahaIndex + 1) % 1280] += value2;

			hahaIndex += 2;
		}
		for (int j = 0; j < 1280; j++)
		{
			int hahaValue = averValue[j] / 800;
			cout << "-----------" << j << ": " << hahaValue << endl;
			if (hahaValue == 255 || hahaValue == 127 || hahaValue == 0)
			{
				ofstream hahaOF;
				hahaOF.open("hahaValue.txt");
				hahaOF << j << "  " << hahaValue;
				hahaOF.close();
				badCol = j;
			}
		}
	}
}
