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

#ifndef CELEX5_H
#define CELEX5_H

#include <stdint.h>
#include <vector>
#include <map>
#include <list>
#include <fstream>
#include <opencv2/opencv.hpp>
#include "../celextypes.h"

#define _ARM_ZYNQ_

#ifdef _WIN32
#ifdef CELEX_API_EXPORTS
#define CELEX_EXPORTS __declspec(dllexport)
#else
#define CELEX_EXPORTS __declspec(dllimport)
#endif
#else
#if defined(CELEX_LIBRARY)
#define CELEX_EXPORTS
#else
#define CELEX_EXPORTS
#endif
#endif

using namespace std;

class FrontPanel;
class CeleDriver;
class HHSequenceMgr;
class DataProcessThreadEx;
class DataReaderThread;
class MIPIDataReaderThread;
class CX5SensorDataServer;
class CommandBase;
class DataRecorder;
class CELEX_EXPORTS CeleX5
{
public:
	enum DeviceType {
		Unknown_Devive = 0,
		CeleX5_MIPI = 1,
		CeleX5_OpalKelly = 2,
		CeleX5_ZYNQ = 3
	};

	enum CeleX5Mode {
		Unknown_Mode = -1,
		Event_Address_Only_Mode = 0,
		Event_Optical_Flow_Mode = 1,
		Event_Intensity_Mode = 2,
		Full_Picture_Mode = 3,
		Full_Optical_Flow_S_Mode = 4,
		Full_Optical_Flow_Test_Mode = 5,
		Full_Optical_Flow_M_Mode = 6
	};

	enum emEventPicType {
		EventBinaryPic = 0,
		EventAccumulatedPic = 1,
		EventGrayPic = 2,
		EventCountPic = 3
	};

	enum emFullPicType {
		Full_Optical_Flow_Pic = 0,
		Full_Optical_Flow_Speed_Pic = 1,
		Full_Optical_Flow_Direction_Pic = 2
	};

	typedef struct CfgInfo
	{
		std::string name;
		uint32_t    min;
		uint32_t    max;
		uint32_t    value;
		uint32_t    step;
		int16_t     high_addr;
		int16_t     middle_addr;
		int16_t     low_addr;
	} CfgInfo;

	typedef struct BinFileAttributes
	{
		uint8_t    bLoopMode; //0: fixed mode; 1: loop mode
		uint8_t    loopA_mode;
		uint8_t    loopB_mode;
		uint8_t    loopC_mode;
		uint8_t    event_data_format;
		uint8_t    hour;
		uint8_t    minute;
		uint8_t    second;
		uint32_t   package_count;
	} BinFileAttributes;

	CeleX5();
	~CeleX5();

	bool openSensor(DeviceType type, CeleX5Mode sensorMode);
	bool isSensorReady();
	void pipeOutFPGAData();
	void getMIPIData(vector<uint8_t> &buffer);
	void pipeOutMIPIData();
	bool getSensorData(unsigned char* buffer);
	int  getbufpos();
	uint8_t* getImagedata(int pos);
	int      setfcntl();

	DeviceType getDeviceType();

	bool setFpnFile(const std::string& fpnFile);
	void generateFPN(std::string fpnFile);

	void setSensorFixedMode(CeleX5Mode mode);
	CeleX5Mode getSensorFixedMode();

	void setSensorLoopMode(CeleX5Mode mode, int loopNum); //LopNum = 1/2/3
	CeleX5Mode getSensorLoopMode(int loopNum); //LopNum = 1/2/3
	void setLoopModeEnabled(bool enable);
	bool isLoopModeEnabled();

	//------- for fixed mode -------
	uint32_t getFullPicFrameTime(); //related to fps (range: 20 ~ 100), hardware parameter, unit: millisecond
	void setOpticalFlowFrameTime(uint32_t value); //hardware parameter, unit: millisecond
	uint32_t getOpticalFlowFrameTime();
	void setEventFrameTime(uint32_t value); //software parameter, unit: millisecond
	uint32_t getEventFrameTime();
	void setEventFrameOverlappedTime(uint32_t msec); //software parameter, unit: millisecond
	uint32_t getEventFrameOverlappedTime();
	void setEventFrameParameters(uint32_t frameTime, uint32_t intervalTime); //software parameter, unit: millisecond

	//------- for loop mode -------
	void setEventDuration(uint32_t value);
	void setPictureNumber(uint32_t num, CeleX5Mode mode);

	//------- sensor control interfaces -------
	void setThreshold(uint32_t value);
	uint32_t getThreshold();
	void setContrast(uint32_t value);
	uint32_t getContrast();
	void setISO(uint32_t value);
	uint32_t getISO();
	void setBrightness(uint32_t value);
	uint32_t getBrightness();
	uint32_t getClockRate(); //unit: MHz
	void setClockRate(uint32_t value); //unit: MHz

	void setEventDataFormat(int format); //0: format 0; 1: format 1; 2: format 2
	int getEventDataFormat();

	//------- get image interfaces -------
	void getFullPicBuffer(unsigned char* buffer);
	cv::Mat getFullPicMat();
	void getEventPicBuffer(unsigned char* buffer, emEventPicType type = EventBinaryPic);
	cv::Mat getEventPicMat(emEventPicType type);

	void getOpticalFlowPicBuffer(unsigned char* buffer,emFullPicType type = Full_Optical_Flow_Pic);
	cv::Mat getOpticalFlowPicMat(emFullPicType type);

	//------- get event data vector -------
	bool getEventDataVector(std::vector<EventData> &vector);
	bool getEventDataVector(std::vector<EventData> &vector, uint64_t& frameNo);

	//------- record raw data interfaces -------
	void startRecording(std::string filePath);
	void stopRecording();

	CX5SensorDataServer* getSensorDataServer();

	//--- Playback Interfaces ---
	bool openBinFile(std::string filePath);
	bool readPlayBackData(long length = 1968644);
	bool readBinFileData();
	uint32_t getTotalPackageCount();
	uint32_t getCurrentPackageNo();
	void setCurrentPackageNo(uint32_t value);
	BinFileAttributes getBinFileAttributes();

	void reset();

	uint32_t getFullFrameFPS();

	//----- for test -----
	void setAntiFlashlightEnabled(bool enabled);
	void enterTemperature(string strT);
	void setAutoISPEnabled(bool enable);
	bool isAutoISPEnabled();
	void setISPThreshold(uint32_t value, int num);
	void setISPBrightness(uint32_t value, int num);
	//----- for test -----
	void beginSaveFullPic(std::string filename);
	void stopSaveFullPic();
	void setRowCycleCount(int count);

	map<string, vector<CfgInfo>> getCeleX5Cfg();
	map<string, vector<CfgInfo>> getCeleX5CfgModified();
	void writeRegister(int16_t addressH, int16_t addressM, int16_t addressL, uint32_t value);
	CfgInfo getCfgInfoByName(string csrType, string name, bool bDefault);
	void writeCSRDefaults(string csrType);
	void modifyCSRParameter(string csrType, string cmdName, uint32_t value);
	bool saveXMLFile();

	void wireIn(uint32_t address, uint32_t value, uint32_t mask);

private:
	bool initializeFPGA();
	bool configureSettings(DeviceType type);
	bool resetConfigureSettings(DeviceType type);
	//for write register
	//void wireIn(uint32_t address, uint32_t value, uint32_t mask);
	void writeRegister(CfgInfo cfgInfo);
	void resetPin(bool bReset);
	void setALSEnabled(bool enable);
	void enterCFGMode();
	void enterStartMode();
	void disableMIPI();
	void enableMIPI();
	void clearData();

private:
	FrontPanel*                    m_pFrontPanel;
#ifndef _ARM_ZYNQ_
	CeleDriver*                    m_pCeleDriver;
#endif // _ARM_ZYNQ_
	HHSequenceMgr*                 m_pSequenceMgr;
	DataProcessThreadEx*           m_pDataProcessThread;
	DataReaderThread*              m_pFPGAReaderThread;
	MIPIDataReaderThread*          m_pMIPIReaderThread;
	DataRecorder*                  m_pDataRecorder;

	map<string, vector<CfgInfo>>   m_mapCfgDefaults;
	map<string, vector<CfgInfo>>   m_mapCfgModified;

	unsigned char*                 m_pReadBuffer;
	uint8_t*                       m_pDataToRead;

	bool                           m_bLoopModeEnabled;
	uint32_t                       m_uiContrast;
	uint32_t                       m_uiBrightness;
	uint32_t                       m_uiThreshold;
	uint32_t                       m_uiClockRate;
	uint32_t                       m_uiOpticalFlowTime;
	int                            m_iEventDataFormat;

	//playback
	std::ifstream                  m_ifstreamPlayback;
	uint32_t                       m_uiPackageCount;
	uint32_t                       m_uiPackageCountList[1000];
	bool                           m_bFirstReadFinished;
	BinFileAttributes              m_stBinFileHeader;
	DeviceType                     m_emDeviceType;
	uint32_t                       m_uiPackageCounter;
	uint32_t                       m_uiPackageCountPS;
	uint32_t                       m_uiPackageTDiff;
	uint32_t                       m_uiPackageBeginT;

	bool                           m_bAutoISPEnabled;
	uint32_t                       m_arrayISPThreshold[3];
	uint32_t                       m_arrayBrightness[4];
	uint32_t                       m_uiAutoISPRefreshTime;
	//
	int                            m_iFileDescriptor;
	uint32_t                       m_uiDMABufferSize;
};

#endif // CELEX5_H
