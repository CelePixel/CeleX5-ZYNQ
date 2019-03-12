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

#include <iostream>
#include <cstring>
#include <thread>

#ifndef _WIN32
#include <unistd.h>
#endif

#include "../include/celex5/celex5.h"
#include "../socket/celextcpsocket.h"
#include "../base/xbase.h"
#include "../eventproc/dataprocessthread.h"
#include "../eventproc/datareaderthread.h"
#include "datarecorder.h"

#ifdef _WIN32
static HANDLE s_hEventHandle = nullptr;
#endif

#define CSR_COL_GAIN          45

#define CSR_BIAS_ADVL_I_H     30
#define CSR_BIAS_ADVL_I_L     31
#define CSR_BIAS_ADVH_I_H     26
#define CSR_BIAS_ADVH_I_L     27

#define CSR_BIAS_ADVCL_I_H    38
#define CSR_BIAS_ADVCL_I_L    39
#define CSR_BIAS_ADVCH_I_H    34
#define CSR_BIAS_ADVCH_I_L    35

#define DATA_BUFFER_SIZE      655360

CeleX5::CeleX5()
	: m_bLoopModeEnabled(false)
	, m_uiClockRate(40)
	, m_iEventDataFormat(2)
	, m_pDataToRead(NULL)
	, m_uiPackageCount(0)
	, m_bFirstReadFinished(false)
	, m_pReadBuffer(NULL)
	, m_emDeviceType(CeleX5::Unknown_Devive)
	, m_uiBrightness(120)
	, m_uiPackageCounter(0)
	, m_uiPackageTDiff(0)
	, m_uiPackageBeginT(0)
	, m_pFPGAReaderThread(NULL)
	, m_iRotateType(0)
	, m_uiISOLevel(2)
	, m_bPlayBackState(false)
	, m_uiPackageSize(0)
{
	//create data process thread
	m_pDataProcessThread = new DataProcessThreadEx("CeleX5Thread");
	//m_pDataProcessThread->setDeviceType(CeleX5::CeleX5_MIPI);
	m_pDataProcessThread->setCeleX(this);

	m_pDataRecorder = new DataRecorder;
	//auto n = thread::hardware_concurrency();//cpu core count = 8

	m_pTcpSocket = new CeleXTcpSocket;
}

CeleX5::~CeleX5()
{
	if (m_pTcpSocket)
	{
		delete m_pTcpSocket;
		m_pTcpSocket = NULL;
	}
	if (m_pDataProcessThread)
	{
		delete m_pDataProcessThread;
		m_pDataProcessThread = NULL;
	}
	if (m_pDataRecorder)
	{
		delete m_pDataRecorder;
		m_pDataRecorder;
	}
	if (m_pReadBuffer)
	{
		delete[] m_pReadBuffer;
		m_pReadBuffer = NULL;
	}
}

bool CeleX5::openSensor(DeviceType type)
{
	m_emDeviceType = type;
	m_pDataProcessThread->setDeviceType(type);
	if (m_pFPGAReaderThread == NULL)
	{
		m_pFPGAReaderThread = new DataReaderThread(this);
		m_pFPGAReaderThread->startReadData(true);
		m_pFPGAReaderThread->start();

		m_pDataProcessThread->start();
	}
	return true;
}

bool CeleX5::isSensorReady()
{
	return true;
}

unsigned char package_buffer[DATA_BUFFER_SIZE * 2];
int CeleX5::getSensorRawData(unsigned char* buffer, int length)
{
	if (NULL == buffer || 0 == length)
		return 0;

	int bufferLen = m_pTcpSocket->recvData((char*)buffer, length);
	if (bufferLen < 0)
		return bufferLen;
	//cout << "receive buffer size =  " << bufferLen << endl;

	memcpy(&package_buffer[m_uiPackageSize], buffer, bufferLen);
	m_uiPackageSize += bufferLen;
	if (m_uiPackageSize >= DATA_BUFFER_SIZE)
	{
		if (!m_bPlayBackState)
		{
			m_pDataProcessThread->addData(&package_buffer[0], DATA_BUFFER_SIZE);
		}
		//record sensor data
		if (m_pDataRecorder->isRecording())
		{
			m_pDataRecorder->writeData(&package_buffer[0], DATA_BUFFER_SIZE);
		}
		if (m_uiPackageSize > DATA_BUFFER_SIZE)
		{
			//cout << "package_size = " << package_size << endl;
			int nextLen = m_uiPackageSize - DATA_BUFFER_SIZE;
			memcpy(&package_buffer[0], &buffer[bufferLen - nextLen], nextLen);
			m_uiPackageSize = nextLen;
		}
		else
		{
			m_uiPackageSize = 0;
		}
#ifdef _WIN32
		m_uiPackageCounter++;
#endif
	}
#ifdef _WIN32
	uint32_t t2 = GetTickCount();
	m_uiPackageTDiff += (t2 - m_uiPackageBeginT);
	m_uiPackageBeginT = t2;
	if (m_uiPackageTDiff > 1000)
	{
		//cout << "----------------- package count = " << m_uiPackageCounter << endl;
		m_uiPackageTDiff = 0;
		m_uiPackageCounter = 0;
	}
#endif // _WIN32
	return bufferLen;
}

CeleX5::DeviceType CeleX5::getDeviceType()
{
	return m_emDeviceType;
}

bool CeleX5::setFpnFile(const std::string& fpnFile)
{
	return m_pDataProcessThread->getDataProcessor5()->setFpnFile(fpnFile);
}

void CeleX5::generateFPN(std::string fpnFile)
{
	m_pDataProcessThread->getDataProcessor5()->generateFPN(fpnFile);
}

void CeleX5::reset()
{
	TCP_MSG_PACK cmd;
	cmd.cmd = CMD_RESET;
	cmd.value1 = 0;
	cmd.value2 = 0;
	m_pTcpSocket->sendCmd(cmd);
	clearData();
}

uint32_t CeleX5::getFullFrameFPS()
{
	return m_pDataProcessThread->getDataProcessor5()->getFullFrameFPS();
}

// Set the Sensor operation mode in fixed mode
// address = 53, width = [2:0]
void CeleX5::setSensorFixedMode(CeleX5Mode mode)
{
	if (CeleX5::CeleX5_ZYNQ == m_emDeviceType)
	{
		TCP_MSG_PACK cmd;
		cmd.cmd = CMD_SET_FIXED_MODE;
		cmd.value1 = mode;
		cmd.value2 = 0;
		m_pTcpSocket->sendCmd(cmd);
		m_pDataProcessThread->getDataProcessor5()->setSensorFixedMode(mode);
	}
}

// Set the Sensor operation mode in loop mode
// loop = 1: the first operation mode in loop mode, address = 53, width = [2:0]
// loop = 2: the second operation mode in loop mode, address = 54, width = [2:0]
// loop = 3: the third operation mode in loop mode, address = 55, width = [2:0]
void CeleX5::setSensorLoopMode(CeleX5Mode mode, int loopNum)
{
	if (loopNum < 1 || loopNum > 3)
	{
		cout << "CeleX5::setSensorMode: wrong loop number!";
		return;
	}
	clearData();
	if (CeleX5::CeleX5_ZYNQ == m_emDeviceType)
	{
		TCP_MSG_PACK cmd;
		cmd.cmd = CMD_SET_LOOP_MODE;
		cmd.value1 = mode;
		cmd.value2 = loopNum;
		m_pTcpSocket->sendCmd(cmd);
	}
	m_pDataProcessThread->getDataProcessor5()->setSensorLoopMode(mode, loopNum);
}

CeleX5::CeleX5Mode CeleX5::getSensorFixedMode()
{
	return m_pDataProcessThread->getDataProcessor5()->getSensorFixedMode();
}

CeleX5::CeleX5Mode CeleX5::getSensorLoopMode(int loopNum)
{
	return m_pDataProcessThread->getDataProcessor5()->getSensorLoopMode(loopNum);
}

// enable or disable loop mode
// address = 64, width = [0], 0: fixed mode / 1: loop mode
void CeleX5::setLoopModeEnabled(bool enable)
{
	m_bLoopModeEnabled = enable;
	if (CeleX5::CeleX5_ZYNQ == m_emDeviceType)
	{
		TCP_MSG_PACK cmd;
		cmd.cmd = CMD_ENABLE_LOOP_MODE;
		cmd.value1 = enable;
		cmd.value2 = 0;
		m_pTcpSocket->sendCmd(cmd);
	}
	m_pDataProcessThread->getDataProcessor5()->setLoopModeEnabled(enable);
}

bool CeleX5::isLoopModeEnabled()
{
	return m_bLoopModeEnabled;
}

// Set the duration of event mode (Mode_A/B/C) when sensor operates in loop mode
// low byte address = 57, width = [7:0]
// high byte address = 58, width = [1:0]
void CeleX5::setEventDuration(uint32_t value)
{
	if (CeleX5::CeleX5_ZYNQ == m_emDeviceType)
	{
		TCP_MSG_PACK cmd;
		cmd.cmd = CMD_SET_LOOP_EVENT_DURATION;
		cmd.value1 = value;
		cmd.value2 = 0;
		m_pTcpSocket->sendCmd(cmd);
	}
}

// Set the mumber of pictures to acquire in Mode_D/E/F/G/H
// Mode_D: address = 59, width = [7:0]
// Mode_E: address = 60, width = [7:0]
// Mode_F: address = 61, width = [7:0]
// Mode_G: address = 62, width = [7:0]
// Mode_H: address = 63, width = [7:0]
void CeleX5::setPictureNumber(uint32_t num, CeleX5Mode mode)
{
	if (CeleX5::CeleX5_ZYNQ == m_emDeviceType)
	{
		TCP_MSG_PACK cmd;
		if(mode == CeleX5::Full_Picture_Mode)
			cmd.cmd = CMD_SET_LOOP_FULL_PIC_NUM;
		else if(mode == CeleX5::Full_Optical_Flow_S_Mode)
			cmd.cmd = CMD_SET_LOOP_S_FO_PIC_NUM;
		else
			cmd.cmd = CMD_SET_LOOP_M_FO_PIC_NUM;
		cmd.value1 = num;
		cmd.value2 = (int)mode;
		m_pTcpSocket->sendCmd(cmd);
	}
}

// related to fps (main clock frequency), hardware parameter
uint32_t CeleX5::getFullPicFrameTime()
{
	return 1000 / m_uiClockRate;
}

//software parameter
void CeleX5::setEventFrameTime(uint32_t value)
{
	m_pDataProcessThread->getDataProcessor5()->setEventFrameTime(value);
}
uint32_t CeleX5::getEventFrameTime()
{
	return m_pDataProcessThread->getDataProcessor5()->getEventFrameTime();
}

//hardware parameter
void CeleX5::setOpticalFlowFrameTime(uint32_t value)
{
	;
}
uint32_t CeleX5::getOpticalFlowFrameTime()
{
	return 0;
}

// BIAS_EVT_VL : 341 address(2/3)
// BIAS_EVT_DC : 512 address(4/5)
// BIAS_EVT_VH : 683 address(6/7)
void CeleX5::setThreshold(uint32_t value)
{
	m_uiThreshold = value;

	if (CeleX5::CeleX5_ZYNQ == m_emDeviceType)
	{
		TCP_MSG_PACK cmd;
		cmd.cmd = CMD_SET_THRESHOLD;
		cmd.value1 = value;
		cmd.value2 = 0;
		m_pTcpSocket->sendCmd(cmd);
	}
}

uint32_t CeleX5::getThreshold()
{
	return m_uiThreshold;
}

void CeleX5::setISO(uint32_t value)
{
	int index = value - 1;
	if (index < 0)
		index = 0;
	if (index > 5)
		index = 5;

	m_uiISOLevel = index + 1;
	m_pDataProcessThread->getDataProcessor5()->setISOLevel(m_uiISOLevel);
	if (CeleX5::CeleX5_ZYNQ == m_emDeviceType)
	{
		TCP_MSG_PACK cmd;
		cmd.cmd = CMD_SET_ISO;
		cmd.value1 = value;
		cmd.value2 = 0;
		m_pTcpSocket->sendCmd(cmd);
	}
}

uint32_t CeleX5::getISO()
{
	return m_uiISOLevel;
}

// Set brightness
// <BIAS_BRT_I>
// high byte address = 22, width = [1:0]
// low byte address = 23, width = [7:0]
void CeleX5::setBrightness(uint32_t value)
{
	m_uiBrightness = value;

	if (CeleX5::CeleX5_ZYNQ == m_emDeviceType)
	{
		TCP_MSG_PACK cmd;
		cmd.cmd = CMD_SET_BRIGHTNESS;
		cmd.value1 = value;
		cmd.value2 = 0;
		m_pTcpSocket->sendCmd(cmd);
	}
}

uint32_t CeleX5::getBrightness()
{
	return m_uiBrightness;
}

uint32_t CeleX5::getClockRate()
{
	return m_uiClockRate;
}

void CeleX5::setClockRate(uint32_t value)
{
	if (CeleX5::CeleX5_ZYNQ == m_emDeviceType)
	{
		if (value > 160 || value < 20)
			return;
		m_uiClockRate = value;
		m_pDataProcessThread->getDataProcessor5()->setClockRate(m_uiClockRate);
		TCP_MSG_PACK cmd;
		cmd.cmd = CMD_SET_CLOCK;
		cmd.value1 = value;
		cmd.value2 = 0;
		m_pTcpSocket->sendCmd(cmd);
	}
}

void CeleX5::setRotateType(int type)
{
	m_iRotateType += type;
	m_pDataProcessThread->getDataProcessor5()->setRotateType(m_iRotateType);
}

int CeleX5::getRotateType()
{
	return m_iRotateType;
}

void CeleX5::getFullPicBuffer(unsigned char* buffer)
{
	m_pDataProcessThread->getDataProcessor5()->getFullPicBuffer(buffer);
}

cv::Mat CeleX5::getFullPicMat()
{
	CeleX5ProcessedData* pSensorData = m_pDataProcessThread->getDataProcessor5()->getProcessedData();
	if (pSensorData)
	{
		return cv::Mat(cv::Size(CELEX5_COL, CELEX5_ROW), CV_8UC1, pSensorData->getFullPicBuffer());
	}
	return cv::Mat();
}

void CeleX5::getEventPicBuffer(unsigned char* buffer, emEventPicType type)
{
	m_pDataProcessThread->getDataProcessor5()->getEventPicBuffer(buffer, type);
}

cv::Mat CeleX5::getEventPicMat(emEventPicType type)
{
	CeleX5ProcessedData* pSensorData = m_pDataProcessThread->getDataProcessor5()->getProcessedData();
	if (pSensorData)
	{
		return cv::Mat(cv::Size(CELEX5_COL, CELEX5_ROW), CV_8UC1, pSensorData->getEventPicBuffer(type));
	}
	return cv::Mat();
}

void CeleX5::getOpticalFlowPicBuffer(unsigned char* buffer, emFullPicType type)
{
	m_pDataProcessThread->getDataProcessor5()->getOpticalFlowPicBuffer(buffer,type);
}

cv::Mat CeleX5::getOpticalFlowPicMat(emFullPicType type)
{
	CeleX5ProcessedData* pSensorData = m_pDataProcessThread->getDataProcessor5()->getProcessedData();
	if (pSensorData)
	{
		return cv::Mat(cv::Size(CELEX5_COL, CELEX5_ROW), CV_8UC1, pSensorData->getOpticalFlowPicBuffer(type));
	}
	return cv::Mat();
}

bool CeleX5::getEventDataVector(std::vector<EventData> &vector)
{
	return m_pDataProcessThread->getDataProcessor5()->getEventDataVector(vector);
}

bool CeleX5::getEventDataVector(std::vector<EventData> &vector, uint64_t& frameNo)
{
	return m_pDataProcessThread->getDataProcessor5()->getEventDataVector(vector, frameNo);
}

void CeleX5::startRecording(std::string filePath)
{
	m_pDataRecorder->startRecording(filePath);
}

void CeleX5::stopRecording()
{
	BinFileAttributes header;
	if (isLoopModeEnabled())
	{
		header.bLoopMode = 1;
		header.loopA_mode = m_pDataProcessThread->getDataProcessor5()->getSensorLoopMode(1);
		header.loopB_mode = m_pDataProcessThread->getDataProcessor5()->getSensorLoopMode(2);
		header.loopC_mode = m_pDataProcessThread->getDataProcessor5()->getSensorLoopMode(3);
	}
	else
	{
		header.bLoopMode = 0;
		header.loopA_mode = m_pDataProcessThread->getDataProcessor5()->getSensorFixedMode();
		header.loopB_mode = 255;
		header.loopC_mode = 255;
	}
	header.event_data_format = 2;
	m_pDataRecorder->stopRecording(&header);
}

CX5SensorDataServer* CeleX5::getSensorDataServer()
{
	return m_pDataProcessThread->getDataProcessor5()->getSensorDataServer();
}

int CeleX5::connectToZYNQServer()
{
	return m_pTcpSocket->connectToZYNQServer();
}

void CeleX5::writeRegister(CfgInfo cfgInfo)
{
}

void CeleX5::resetPin(bool bReset)
{
	uint32_t address = 0x00;
	uint32_t value = 0;
	uint32_t mask = 0x08;
	if (bReset)
	{
		value = 0x08;
	}
	cout << "CelexSensorDLL::resetPin: address = " << address << ", value = " << value << ", mask = " << mask << endl;
}

bool CeleX5::openBinFile(std::string filePath)
{
	m_uiPackageCount = 0;
	m_bFirstReadFinished = false;
	if (m_ifstreamPlayback.is_open())
	{
		m_ifstreamPlayback.close();
	}
	m_pDataProcessThread->clearData();
	m_ifstreamPlayback.open(filePath.c_str(), std::ios::binary);
	if (!m_ifstreamPlayback.good())
	{
		cout << "Can't Open File: " << filePath.c_str();
		return false;
	}
	m_pDataProcessThread->setPlayback(true);
	// read header
	m_ifstreamPlayback.read((char*)&m_stBinFileHeader, sizeof(BinFileAttributes));
	cout << "isLoopMode = " << (int)m_stBinFileHeader.bLoopMode
		 << ", loopA_mode = " << (int)m_stBinFileHeader.loopA_mode << ", loopB_mode = " << (int)m_stBinFileHeader.loopB_mode << ", loopC_mode = " << (int)m_stBinFileHeader.loopC_mode
		 << ", event_data_format = " << (int)m_stBinFileHeader.event_data_format
		 << ", hour = " << (int)m_stBinFileHeader.hour << ", minute = " << (int)m_stBinFileHeader.minute << ", second = " << (int)m_stBinFileHeader.second
		 << ", package_count = " << (int)m_stBinFileHeader.package_count << endl;

	m_pDataProcessThread->start();

	if (m_stBinFileHeader.bLoopMode)
	{
		;
	}
	else
	{
		m_pDataProcessThread->getDataProcessor5()->setSensorFixedMode(CeleX5::CeleX5Mode(m_stBinFileHeader.loopA_mode));
	}
	return true;
}

bool CeleX5::readBinFileData()
{
	//cout << __FUNCTION__ << endl;
	bool eof = false;
	if (NULL == m_pReadBuffer)
		m_pReadBuffer = new unsigned char[DATA_BUFFER_SIZE];
	//cout << "--------------" << m_pDataProcessThread->queueSize() << endl;
	while (true && m_pDataProcessThread->queueSize() < 20)
	{
		m_ifstreamPlayback.read((char*)m_pReadBuffer, DATA_BUFFER_SIZE);
		int dataLen = m_ifstreamPlayback.gcount();
		if (dataLen > 0)
			m_pDataProcessThread->addData(m_pReadBuffer, dataLen);

		if (m_ifstreamPlayback.eof())
		{
			eof = true;
			//m_ifstreamPlayback.close();
			cout << "Read Playback file Finished!" << endl;
			break;
		}
	}
	return eof;
}

void CeleX5::setPlayBackState(bool state)
{
	m_bPlayBackState = state;
	m_pDataProcessThread->setPlayback(state);
}

bool CeleX5::getPlayBackState()
{
	return m_bPlayBackState;
}

uint32_t CeleX5::getTotalPackageCount()
{
	return m_uiPackageCount;
}

uint32_t CeleX5::getCurrentPackageNo()
{
	return m_pDataProcessThread->packageNo();
}

void CeleX5::setCurrentPackageNo(uint32_t value)
{
	m_ifstreamPlayback.clear();
	m_ifstreamPlayback.seekg(m_uiPackageCountList[value], ios::beg);
	m_pDataProcessThread->setPackageNo(value);
}

CeleX5::BinFileAttributes CeleX5::getBinFileAttributes()
{
	return m_stBinFileHeader;
}

// FLICKER_DETECT_EN: CSR_183
// Flicker detection enable select: 1:enable / 0:disable
void CeleX5::setAntiFlashlightEnabled(bool enabled)
{
}

void CeleX5::setRowCycleCount(int count)
{
	m_pDataProcessThread->getDataProcessor5()->setRowCycleCount(count);
}

void CeleX5::clearData()
{
    m_pDataProcessThread->clearData();
}
