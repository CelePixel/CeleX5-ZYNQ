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

#ifndef CELEX5DATAPROCESSOR_H
#define CELEX5DATAPROCESSOR_H

#include "../include/celex5/celex5.h"
#include "../include/celex5/celex5processeddata.h"
#include "../include/celex5/celex5datamanager.h"
#include "../eventproc/celex5datareader.h"

class CeleX5DataProcessor
{
public:
	enum emEventShowMethod {
		ShowByTime = 0,
		ShowByEventCount = 1,
		ShowByRow = 2
	};

	CeleX5DataProcessor();
	~CeleX5DataProcessor();

	void getFullPicBuffer(unsigned char* buffer);
	void getEventPicBuffer(unsigned char* buffer, CeleX5::emEventPicType type);
	void getOpticalFlowPicBuffer(unsigned char* buffer, CeleX5::emFullPicType type);

	bool getEventDataVector(std::vector<EventData> &vector);
	bool getEventDataVector(std::vector<EventData> &vector, uint64_t& frameNo);

	void processData(unsigned char* data, long length);
	bool processEvent(unsigned char* data); // return true for normal event, false for special event

	CX5SensorDataServer *getSensorDataServer();
	CeleX5ProcessedData *getProcessedData();

	bool setFpnFile(const std::string& fpnFile);
	void generateFPN(std::string filePath);

	CeleX5::CeleX5Mode getSensorFixedMode();
	CeleX5::CeleX5Mode getSensorLoopMode(int loopNum);
	void setSensorFixedMode(CeleX5::CeleX5Mode mode);
	void setSensorLoopMode(CeleX5::CeleX5Mode mode, int loopNum);
	void setLoopModeEnabled(bool enable);
	void setISOLevel(uint32_t value);

	void setEventCountForShow(uint32_t value); //software parameter, unit: millisecond
	void setEventFrameTime(uint32_t value);
	uint32_t getEventFrameTime();

	void setRowCycleCount(int count);
	void setRotateType(int type);
	void setClockRate(int value);
	uint32_t getFullFrameFPS();
	int getIMUData(std::vector<IMUData>& data);

private:
	//----- for Parallel data -----
	bool processFPGAColEvent(unsigned int row, unsigned int col, unsigned int adc, unsigned int time_stamp);

	void processEventRow(int row);

	bool createImage();
	unsigned int normalizeADC(unsigned int adc);
	void generateFPNimpl();
	int calculateDenoiseScore(unsigned char* pBuffer, unsigned int pos);
	int calMean(unsigned char* pBuffer, unsigned int pos);

	void calDirectionAndSpeed(int i, int j, unsigned char* pBuffer, unsigned char* &speedBuffer, unsigned char* &dirBuffer);

	int getCurrentIndex(int initIndex);

private:
	CeleX5DataReader*        m_pFPGADataReader;
	CeleX5ProcessedData*     m_pCX5ProcessedData;
	CX5SensorDataServer*     m_pCX5Server;
	//
	unsigned char*           m_pEventCountBuffer;
	unsigned char*           m_pEventADCBuffer;
	unsigned char*           m_pLastADC;
	//for fpn
	long*                    m_pFpnGenerationBuffer;
	int*                     m_pFpnBuffer;
	//
	CeleX5::CeleX5Mode       m_emCurrentSensorMode;
	CeleX5::CeleX5Mode       m_emSensorFixedMode;
	CeleX5::CeleX5Mode       m_emSensorLoopAMode;
	CeleX5::CeleX5Mode       m_emSensorLoopBMode;
	CeleX5::CeleX5Mode       m_emSensorLoopCMode;
	//
	string                   m_strFpnFilePath;
	//
	uint32_t                 m_uiPixelCount;
	uint32_t                 m_uiEventTCounter;

	int32_t                  m_iLastRowTimeStamp;
	int32_t                  m_iRowTimeStamp;

	int32_t                  m_iLastRow;
	int32_t                  m_iCurrentRow;
	uint32_t                 m_uiRowCount;
	uint32_t                 m_uiEventCountForShow;

	int                      m_iFpnCalculationTimes;
	uint32_t                 m_uiEventFrameTime;
	uint32_t                 m_uiClockRate;

	bool                     m_bIsGeneratingFPN;
	bool                     m_bLoopModeEnabled;
	bool                     m_bShowEventImage;

	int                      m_iCycleCount;
	emEventShowMethod        m_emEventShowMethod;

	vector<EventData>        m_vecEventData;
	int                      m_iRowCycleCount;
	int						 m_iRotateType;	//rotate param
	uint32_t                 m_uiISOLevel; //range: 1 ~ 4

	uint32_t                 m_uiFrameCounter;
	uint32_t                 m_uiFrameCountPS;
	clock_t                  m_cltFrameTDiff;
	clock_t                  m_cltFrameBeginT;

	bool				     m_bIsIMUSpecialStart;
	vector<IMUData>			 m_vecIMUData;
	unsigned int		     m_uiIMUTimeStamp;
	uint64_t                 m_uiEventFrameNo;
};

#endif // CELEX5DATAPROCESSOR_H
