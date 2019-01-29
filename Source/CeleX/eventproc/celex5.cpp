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

#define _ARM_ZYNQ_

#include <iostream>
#include <cstring>
#include <thread>

#ifndef _WIN32
#include <unistd.h>
#endif

#include "../include/celex5/celex5.h"
#include "../frontpanel/frontpanel.h"

#ifdef _ARM_ZYNQ_
#include <string.h>
#include <unistd.h>
#include <stdio.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/ioctl.h>
#include "../driver/axidma_ioctl.h"
#include <fstream>
#else
#include "../driver/CeleDriver.h"
#endif

#include "../configproc/hhsequencemgr.h"
#include "../configproc/hhwireincommand.h"
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


CeleX5::CeleX5()
	: m_bLoopModeEnabled(false)
	, m_uiClockRate(100)
	, m_iEventDataFormat(2)
	, m_pDataToRead(NULL)
	, m_uiPackageCount(0)
	, m_bFirstReadFinished(false)
	, m_pReadBuffer(NULL)
	, m_emDeviceType(CeleX5::Unknown_Devive)
	, m_pFrontPanel(NULL)
#ifndef _ARM_ZYNQ_
	, m_pCeleDriver(NULL)
#endif
	, m_uiBrightness(120)
	, m_uiPackageCounter(0)
	, m_uiPackageTDiff(0)
	, m_uiPackageBeginT(0)
	, m_bAutoISPEnabled(false)
	, m_arrayISPThreshold{60, 500, 2500}
	, m_arrayBrightness{100, 130, 150, 175}
	, m_uiAutoISPRefreshTime(80)
	, m_pFPGAReaderThread(NULL)
	, m_pMIPIReaderThread(NULL)
	, m_iFileDescriptor(-1)
	, m_uiDMABufferSize(0)
{
	m_pSequenceMgr = new HHSequenceMgr;
	//create data process thread
	m_pDataProcessThread = new DataProcessThreadEx("CeleX5Thread");
	//m_pDataProcessThread->setDeviceType(CeleX5::CeleX5_MIPI);
	m_pDataProcessThread->setCeleX(this);

	m_pDataRecorder = new DataRecorder;
	//auto n = thread::hardware_concurrency();//cpu core count = 8
}

CeleX5::~CeleX5()
{
	if (m_pFrontPanel)
	{
		delete m_pFrontPanel;
		m_pFrontPanel = NULL;
	}
#ifndef _ARM_ZYNQ_
	if (m_pCeleDriver)
	{
		m_pCeleDriver->clearData();
		m_pCeleDriver->Close();
		delete m_pCeleDriver;
		m_pCeleDriver = NULL;
	}
#endif // _ARM_ZYNQ_
	if (m_pSequenceMgr)
	{
		delete m_pSequenceMgr;
		m_pSequenceMgr = NULL;
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

bool CeleX5::openSensor(DeviceType type, CeleX5Mode sensorMode)
{
	m_emDeviceType = type;
	m_pDataProcessThread->setDeviceType(type);
	if (CeleX5::CeleX5_MIPI == type)
	{
#ifndef _ARM_ZYNQ_
		m_pSequenceMgr->parseCeleX5Cfg(FILE_CELEX5_CFG_MIPI);
		m_mapCfgModified = m_mapCfgDefaults = getCeleX5Cfg();
		if (NULL == m_pCeleDriver)
		{
			m_pCeleDriver = new CeleDriver;
			//if (!m_pCeleDriver->Open())
			if (!m_pCeleDriver->openUSB())
				return false;

			////m_pCeleDriver->writeSerialNumber("CX5-MP-0001-HXQ");
			//Sleep(1000);
			//cout << "------------- Serial Number: " << m_pCeleDriver->getSerialNumber() << endl;
			//cout << "------------- Firmware Version: " << m_pCeleDriver->getFirmwareVersion() << endl;
			//cout << "------------- Firmware Date: " << m_pCeleDriver->getFirmwareDate() << endl;
		}
		if (!configureSettings(type))
			return false;
		if (m_pMIPIReaderThread == NULL)
		{
			m_pMIPIReaderThread = new MIPIDataReaderThread(this);
			m_pMIPIReaderThread->startReadData(true);
			m_pMIPIReaderThread->start();
		}
		m_pDataProcessThread->start();
#endif // _ARM_ZYNQ_
	}
	else if (CeleX5::CeleX5_OpalKelly == type)
	{
		m_pSequenceMgr->parseCeleX5Cfg(FILE_CELEX5_CFG_PA);
		m_mapCfgModified = m_mapCfgDefaults = getCeleX5Cfg();
		m_pFrontPanel = FrontPanel::getInstance();
		initializeFPGA();
		if (isSensorReady())
		{
			if (!configureSettings(type))
				return false;
			/*if (m_pFPGAReaderThread == NULL)
			{
				m_pFPGAReaderThread = new DataReaderThread(this);
				m_pFPGAReaderThread->startReadData(true);
				m_pFPGAReaderThread->start();
			}*/
			m_pDataProcessThread->start();
		}
	}
	else if (CeleX5::CeleX5_ZYNQ == type)
	{
		m_iFileDescriptor = open("/dev/axidma", O_RDWR|O_EXCL);
        if (m_iFileDescriptor > 0)
        {
            /*if ((int)sensorMode <= 2) //event mode
            {
                m_uiDMABufferSize = 1280 * 32 * 4;
            }
            else //full-frame mode
            {
                //m_ImageBuffersize =  (PIXELS_NUMBER + 4)<<1;
                m_uiDMABufferSize = 1280 * 128 * 4;
            }*/
            m_uiDMABufferSize = 1280 * 128 * 4;
            //m_uiDMABufferSize = 4096000;
            printf("-------------- m_uiDMABufferSize = %d\n", m_uiDMABufferSize);

            m_pDataToRead = (uint8_t*)mmap(nullptr, m_uiDMABufferSize<<1, PROT_READ|PROT_WRITE, MAP_SHARED, m_iFileDescriptor, 0);
            if (m_pDataToRead != nullptr)
            {
                m_pDataProcessThread->start();
                return true;
            }
            /*if (m_pDataToRead != nullptr)
            {
                munmap(m_pDataToRead, m_uiDMABufferSize<<1);
                m_pDataToRead = nullptr;
            }*/
        }
        close(m_iFileDescriptor);
        return false;
	}

	return true;
}

bool CeleX5::isSensorReady()
{
	return true;
}

void CeleX5::pipeOutFPGAData()
{
	if (CeleX5::CeleX5_OpalKelly != m_emDeviceType)
	{
		return;
	}
	if (!isSensorReady())
	{
		return;
	}
	uint32_t pageCount;
	m_pFrontPanel->wireOut(0x21, 0x1FFFFF, &pageCount);
	//cout << "-------------- pageCount = " << pageCount << endl;
	if (pageCount > 10)
	{
		if (pageCount > MAX_PAGE_COUNT)
			pageCount = MAX_PAGE_COUNT;
		int blockSize = 128;
		long length = (long)(pageCount * blockSize);
		if (NULL == m_pReadBuffer)
			m_pReadBuffer = new unsigned char[128 * MAX_PAGE_COUNT];
		//unsigned char* pData = new unsigned char[length];
		//Return the number of bytes read or ErrorCode (<0) if the read failed.
		long dataLen = m_pFrontPanel->blockPipeOut(0xa0, blockSize, length, m_pReadBuffer);
		if (dataLen > 0)
		{
			//record sensor data
			if (m_pDataRecorder->isRecording())
			{
				m_pDataRecorder->writeData(m_pReadBuffer, dataLen);
			}
			m_pDataProcessThread->addData(m_pReadBuffer, dataLen);
		}
		else if (dataLen < 0) //read failed
		{
			switch (dataLen)
			{
			case okCFrontPanel::InvalidBlockSize:
				cout << "Block Size Not Supported" << endl;
				break;

			case okCFrontPanel::UnsupportedFeature:
				cout << "Unsupported Feature" << endl;
				break;

			default:
				cout << "Transfer Failed with error: " << dataLen << endl;
				break;
			}
			cout << "pageCount = " << pageCount << ", blockSize = " << blockSize << ", length = " << length << endl;
		}
	}
}

void CeleX5::getMIPIData(vector<uint8_t> &buffer)
{
#ifndef _ARM_ZYNQ_
	if (CeleX5::CeleX5_MIPI != m_emDeviceType)
	{
		return;
	}
	if (m_pCeleDriver->getimage(buffer))
	{
		//record sensor data
		if (m_pDataRecorder->isRecording())
		{
			m_pDataRecorder->writeData(buffer);
		}
		//cout << "image buffer size = " << buffer.size() << endl;

#ifdef _WIN32
		m_uiPackageCounter++;
		uint32_t t2 = GetTickCount();
		m_uiPackageTDiff += (t2 - m_uiPackageBeginT);
		m_uiPackageBeginT = t2;
		if (m_uiPackageTDiff > 1000)
		{
			//cout << "--- package count = " << counter << endl;
			m_pDataProcessThread->getDataProcessor5()->getProcessedData()->setFullFrameFPS(m_uiPackageCountPS);
			m_uiPackageTDiff = 0;
			m_uiPackageCountPS = m_uiPackageCounter;
			m_uiPackageCounter = 0;
		}
#endif
	}
#endif
}

void CeleX5::pipeOutMIPIData()
{
#ifndef _ARM_ZYNQ_
	if (CeleX5::CeleX5_MIPI != m_emDeviceType)
	{
		return;
	}
	vector<uint8_t> buffer;
	if (m_pCeleDriver->getimage(buffer))
	{
		//record sensor data
		if (m_pDataRecorder->isRecording())
		{
			m_pDataRecorder->writeData(buffer);
		}
		//cout << "image buffer size = " << buffer.size() << endl;
		m_pDataProcessThread->addData(buffer);

#ifdef _WIN32
		m_uiPackageCounter++;
		uint32_t t2 = GetTickCount();
		m_uiPackageTDiff += (t2 - m_uiPackageBeginT);
		m_uiPackageBeginT = t2;
		if (m_uiPackageTDiff > 1000)
		{
			//cout << "--- package count = " << counter << endl;
			m_pDataProcessThread->getDataProcessor5()->getProcessedData()->setFullFrameFPS(m_uiPackageCountPS);
			m_uiPackageTDiff = 0;
			m_uiPackageCountPS = m_uiPackageCounter;
			m_uiPackageCounter = 0;
		}
#endif
	}
#endif // _ARM_ZYNQ_
}

bool CeleX5::getSensorData(unsigned char* buffer)
{
#ifdef _ARM_ZYNQ_
    struct axidma_transaction trans;
    trans.timeout = 2000;      // wait time out;
    trans.channel_id = 1;      // The id of the DMA channel to use
    trans.buf = m_pDataToRead; // The buffer used for the transaction
    trans.buf_len = m_uiDMABufferSize << 1; // The length of the buffer
    trans.period_len = m_uiDMABufferSize;
   // setBrightness(260);
    //usleep(100000);
    if (ioctl(m_iFileDescriptor, AXIDMA_DMA_READ, &trans) == 0)
    {
        //cout << "--- CeleX5::getSensorData: Begin" << endl;
        //m_pDataProcessThread->addData(m_pDataToRead, m_uiDMABufferSize);
        //cout << "--- CeleX5::getSensorData: buffer size = " << m_uiDMABufferSize << endl;
        return true;
    }
    return false;
#endif // _ARM_ZYNQ_
}

int testvalue;
int CeleX5::getbufpos()
{
#ifdef _ARM_ZYNQ_
    //int value;
    if (ioctl(m_iFileDescriptor, CAMERA_GET_BUFPOS, &testvalue) !=  0)
    {
        printf("get bufpos failure!\n");
        return -1;
    }
    return testvalue;
#endif // _ARM_ZYNQ_
}

uint8_t* CeleX5::getImagedata(int pos)
{
    //cout << "--- CeleX5::getImagedata: pos = " << pos << endl;
    //struct timeval tv;
    //struct timezone tz;

    //char filename[30] = {0};
    //char filedata[60] = {0};
    //static int i = 0;
    //i++;
    //gettimeofday(&tv,&tz);
    if (pos==0)
    {
        //sprintf(filedata,"pos=0,begin=0x%x,second=0x%x, end=0x%x\n",*(int*)(&m_fullPicBuffer[0]),*(int*)(&m_fullPicBuffer[4]),*(int*)(&m_fullPicBuffer[4096000-4]));
        //printf("filedata=%s, sec=%d, usec=%d\n",filedata,tv.tv_sec, tv.tv_usec);

        //*(m_pDataToRead)=0x11;

        m_pDataProcessThread->addData(m_pDataToRead, m_uiDMABufferSize);
        //m_pDataProcessThread->getDataProcessor5()->processData(m_pDataToRead, m_uiDMABufferSize);

        //cout << "--- CeleX5::getSensorData: buffer size = " << m_uiDMABufferSize << endl;
        return m_pDataToRead;
    }
    if (pos==1)
    {
        //sprintf(filedata,"pos=1,begin=0x%x,second=0x%x, end=0x%x\n",*(int*)(&m_fullPicBuffer[4096000]),*(int*)(&m_fullPicBuffer[4096000+4]),*(int*)(&m_fullPicBuffer[4096000*2-4]));
        //printf("filedata=%s, sec=%d, usec=%d\n",filedata,tv.tv_sec, tv.tv_usec);

        //*(m_pDataToRead+m_uiDMABufferSize)=0x11;
        m_pDataProcessThread->addData(m_pDataToRead+m_uiDMABufferSize, m_uiDMABufferSize);
        //m_pDataProcessThread->getDataProcessor5()->processData(m_pDataToRead+m_uiDMABufferSize, m_uiDMABufferSize);
        //cout << "--- CeleX5::getSensorData: buffer size = " << m_uiDMABufferSize << endl;
        return (m_pDataToRead+m_uiDMABufferSize);
    }
}

int CeleX5::setfcntl()
{
    int flags;
    printf("DMS 1===============\n");
    fcntl(m_iFileDescriptor, F_SETOWN, getpid());
    printf("DMS 2===============\n");
    flags = fcntl(m_iFileDescriptor, F_GETFL);
    printf("DMS 3===============\n");
    fcntl(m_iFileDescriptor, F_SETFL, flags | FASYNC);
    printf("DMS 4===============\n");
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
	if (m_emDeviceType == CeleX5::CeleX5_OpalKelly)
	{
		resetPin(true);
		resetPin(false);
		resetConfigureSettings(m_emDeviceType);
	}
	else if (m_emDeviceType == CeleX5::CeleX5_MIPI)
	{
		enterCFGMode();

		//m_pDataProcessThread->suspend();
		////m_pCeleDriver->closeStream();
		//
		////m_pCeleDriver->openStream();
		//m_pDataProcessThread->resume();
		/*m_pCeleDriver->i2c_set(254, 10);
		Sleep(100);
		configureSettings(CeleX5::CeleX5_MIPI);*/

		enterStartMode();
	}
	else if (m_emDeviceType == CeleX5::CeleX5_ZYNQ)
	{
		;
	}
}

uint32_t CeleX5::getFullFrameFPS()
{
	return m_uiPackageCountPS;
}

// Set the Sensor operation mode in fixed mode
// address = 53, width = [2:0]
void CeleX5::setSensorFixedMode(CeleX5Mode mode)
{
    clearData();
    //Disable ALS read and write, must be the first operation
    setALSEnabled(false);

	enterCFGMode();
	//Write Fixed Sensor Mode
	wireIn(53, static_cast<uint32_t>(mode), 0xFF);
	if (CeleX5::CeleX5_MIPI == m_emDeviceType)
	{
		//Disable brightness adjustment (auto isp), always load sensor core parameters from profile0
		wireIn(221, 0, 0xFF); //AUTOISP_BRT_EN
		wireIn(223, 0, 0xFF); //AUTOISP_TRIGGER
		wireIn(220, 0, 0xFF); //AUTOISP_PROFILE_ADDR, Write core parameters to profile0
		writeRegister(233, -1, 232, 1500); //AUTOISP_BRT_VALUE, Set initial brightness value 1500
		writeRegister(22, -1, 23, m_uiBrightness); //BIAS_BRT_I, Override the brightness value in profile0, avoid conflict with AUTOISP profile0
	}
	else if (CeleX5::CeleX5_ZYNQ == m_emDeviceType)
	{
        /*if (m_pDataToRead != nullptr)
        {
            munmap(m_pDataToRead, m_uiDMABufferSize<<1);
            m_pDataToRead = nullptr;
        }
        if ((int)mode <= 2) //event mode
        {
            m_uiDMABufferSize = 1280 * 32 * 4;
        }
        else //full-frame mode
        {
            m_uiDMABufferSize = 1280 * 128 * 4;
        }
        m_pDataToRead = (uint8_t*)mmap(nullptr, m_uiDMABufferSize<<1, PROT_READ|PROT_WRITE, MAP_SHARED, m_iFileDescriptor, 0);
        if (m_pDataToRead != nullptr)
        {
            printf("-------------- m_uiDMABufferSize = %d\n", m_uiDMABufferSize);
        }*/
	}
	enterStartMode();
	m_pDataProcessThread->getDataProcessor5()->setSensorFixedMode(mode);
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
	enterCFGMode();
	wireIn(52 + loopNum, static_cast<uint32_t>(mode), 0xFF);
	enterStartMode();
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

	if (CeleX5::CeleX5_MIPI == m_emDeviceType)
	{
		if (m_bAutoISPEnabled)
			setALSEnabled(false);
	}
	enterCFGMode();
	if (enable)
	{
		wireIn(64, 1, 0xFF);

		if (CeleX5::CeleX5_MIPI == m_emDeviceType)
		{
			//Disable brightness adjustment (auto isp), always load sensor core parameters from profile0
			wireIn(221, 0, 0xFF); //AUTOISP_BRT_EN, disable auto isp
			wireIn(223, 1, 0xFF); //AUTOISP_TRIGGER
			wireIn(220, 0, 0xFF); //AUTOISP_PROFILE_ADDR, Write core parameters to profile0
			writeRegister(233, -1, 232, 1500); //AUTOISP_BRT_VALUE, Set initial brightness value 1500
			writeRegister(22, -1, 23, m_uiBrightness); //BIAS_BRT_I, Override the brightness value in profile0, avoid conflict with AUTOISP profile0
		}
	}
	else
	{
		wireIn(64, 0, 0xFF);
	}
	enterStartMode();
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
	enterCFGMode();
	uint32_t valueH = value >> 8;
	uint32_t valueL = 0xFF & value;
	wireIn(58, valueH, 0xFF);
	wireIn(57, valueL, 0xFF);
	enterStartMode();
}

// Set the mumber of pictures to acquire in Mode_D/E/F/G/H
// Mode_D: address = 59, width = [7:0]
// Mode_E: address = 60, width = [7:0]
// Mode_F: address = 61, width = [7:0]
// Mode_G: address = 62, width = [7:0]
// Mode_H: address = 63, width = [7:0]
void CeleX5::setPictureNumber(uint32_t num, CeleX5Mode mode)
{
	enterCFGMode();

	if (Full_Picture_Mode == mode)
		wireIn(59, num, 0xFF);
	else if (Full_Optical_Flow_S_Mode == mode)
		wireIn(60, num, 0xFF);
	else if (Full_Optical_Flow_M_Mode == mode)
		wireIn(62, num, 0xFF);

	enterStartMode();
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

//software parameter
void CeleX5::setEventFrameOverlappedTime(uint32_t msec)
{
	m_pDataProcessThread->getDataProcessor5()->setEventFrameOverlappedTime(msec);
}
uint32_t CeleX5::getEventFrameOverlappedTime()
{
	return m_pDataProcessThread->getDataProcessor5()->getEventFrameOverlappedTime();
}

void CeleX5::setEventFrameParameters(uint32_t frameTime, uint32_t intervalTime)
{
	;
}

// BIAS_EVT_VL : 341 address(2/3)
// BIAS_EVT_DC : 512 address(4/5)
// BIAS_EVT_VH : 683 address(6/7)
void CeleX5::setThreshold(uint32_t value)
{
	m_uiThreshold = value;

	enterCFGMode();

	int EVT_VL = 512 - value;
	if (EVT_VL < 0)
		EVT_VL = 0;
	writeRegister(2, -1, 3, EVT_VL);

	int EVT_VH = 512 + value;
	if (EVT_VH > 1023)
		EVT_VH = 1023;
	writeRegister(6, -1, 7, EVT_VH);

	enterStartMode();
}

uint32_t CeleX5::getThreshold()
{
	return m_uiThreshold;
}

// Set Contrast
// COL_GAIN: address = 45, width = [1:0]
void CeleX5::setContrast(uint32_t value)
{
	m_uiContrast = value;
	if (value < 1)
		m_uiContrast = 1;
	else if (value > 3)
		m_uiContrast = 3;

	enterCFGMode();
	writeRegister(45, -1, -1, m_uiContrast);
	enterStartMode();
	m_pDataProcessThread->getDataProcessor5()->setColGainValue(m_uiContrast);
}

uint32_t CeleX5::getContrast()
{
	return m_uiContrast;
}

void CeleX5::setISO(uint32_t value)
{
	int index = value - 1;
	if (index < 0)
		index = 0;
	if (index > 5)
		index = 5;
	int col_gain[6] = { 2, 2, 2, 2, 1, 1 };

	int bias_advl_i[6] = { 470, 410, 350, 290, 410, 380 };
	int bias_advh_i[6] = { 710, 770, 830, 890, 770, 800 };

	int bias_advcl_i[6] = { 560, 545, 530, 515, 545, 540 };
	int bias_advch_i[6] = { 620, 635, 650, 665, 635, 640 };


	int bias_vcm_i[6] = { 590, 590, 590, 590, 590, 590 };

	writeRegister(CSR_COL_GAIN, -1, -1, col_gain[index]);
	writeRegister(CSR_BIAS_ADVL_I_H, -1, CSR_BIAS_ADVL_I_L, bias_advl_i[index]);
	writeRegister(CSR_BIAS_ADVH_I_H, -1, CSR_BIAS_ADVH_I_L, bias_advh_i[index]);
	writeRegister(CSR_BIAS_ADVCL_I_H, -1, CSR_BIAS_ADVCL_I_L, bias_advcl_i[index]);
	writeRegister(CSR_BIAS_ADVCH_I_H, -1, CSR_BIAS_ADVCH_I_L, bias_advch_i[index]);

	writeRegister(42, -1, 43, bias_vcm_i[index]);
}

uint32_t CeleX5::getISO()
{
	return 0;
}

// Set brightness
// <BIAS_BRT_I>
// high byte address = 22, width = [1:0]
// low byte address = 23, width = [7:0]
void CeleX5::setBrightness(uint32_t value)
{
	m_uiBrightness = value;

	enterCFGMode();
	writeRegister(22, -1, 23, value);
	enterStartMode();

	m_pDataProcessThread->getDataProcessor5()->setBrightness(value);
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
	if (value > 160 || value < 20)
		return;
	m_uiClockRate = value;

	if (CeleX5::CeleX5_MIPI == m_emDeviceType)
	{
		enterCFGMode();

		// Disable PLL
		wireIn(150, 0, 0xFF);
		int clock[15] = { 20,  30,  40,  50,  60,  70,  80,  90, 100, 110, 120, 130, 140, 150, 160 };

		int PLL_DIV_N[15] = { 12,  18,  12,  15,  18,  21,  12,  18,  15,  22,  18,  26,  21,  30,  24 };
		int PLL_DIV_L[15] = { 2,   3,   2,   2,   2,   2,   2,   3,   2,   3,   2,   3,   2,   3,   2 };
		int PLL_FOUT_DIV1[15] = { 3,   2,   1,   1,   1,   1,   0,   0,   0,   0,   0,   0,   0,   0,   0 };
		int PLL_FOUT_DIV2[15] = { 3,   2,   3,   3,   3,   3,   3,   2,   3,   3,   3,   3,   3,   3,   3 };

		int MIPI_PLL_DIV_I[15] = { 3,   2,   3,   3,   2,   2,   3,   2,   3,   2,   2,   2,   2,   2,   1 };
		int MIPI_PLL_DIV_N[15] = { 120, 120, 120, 96, 120, 102, 120, 120, 96, 130, 120, 110, 102, 96, 120 };

		int index = value / 10 - 2;

		cout << "CeleX5::setClockRate: " << clock[index] << " MHz" << endl;

		// Write PLL Clock Parameter
		writeRegister(159, -1, -1, PLL_DIV_N[index]);
		writeRegister(160, -1, -1, PLL_DIV_L[index]);
		writeRegister(151, -1, -1, PLL_FOUT_DIV1[index]);
		writeRegister(152, -1, -1, PLL_FOUT_DIV2[index]);

		// Enable PLL
		wireIn(150, 1, 0xFF);

		disableMIPI();
		writeRegister(113, -1, -1, MIPI_PLL_DIV_I[index]);
		writeRegister(115, -1, 114, MIPI_PLL_DIV_N[index]);
		enableMIPI();

		enterStartMode();
	}
	else
	{
		enterCFGMode();

		// Disable PLL
		wireIn(150, 0, 0xFF);
		// Write PLL Clock Parameter
		wireIn(159, value * 3 / 5, 0xFF);
		// Enable PLL
		wireIn(150, 1, 0xFF);

		enterStartMode();
	}
}

//0: format 0; 1: format 1; 2: format 2
void CeleX5::setEventDataFormat(int format)
{
	m_iEventDataFormat = format;
}

int CeleX5::getEventDataFormat()
{
	return m_iEventDataFormat;
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

map<string, vector<CeleX5::CfgInfo>> CeleX5::getCeleX5Cfg()
{
	map<string, vector<HHCommandBase*>> mapCfg = m_pSequenceMgr->getCeleX5Cfg();
	//
	map<string, vector<CeleX5::CfgInfo>> mapCfg1;
	for (auto itr = mapCfg.begin(); itr != mapCfg.end(); itr++)
	{
		//cout << "CelexSensorDLL::getCeleX5Cfg: " << itr->first << endl;
		vector<HHCommandBase*> pCmdList = itr->second;
		vector<CeleX5::CfgInfo> vecCfg;
		for (auto itr1 = pCmdList.begin(); itr1 != pCmdList.end(); itr1++)
		{
			WireinCommandEx* pCmd = (WireinCommandEx*)(*itr1);
			//cout << "----- Register Name: " << pCmd->name() << endl;
			CeleX5::CfgInfo cfgInfo;
			cfgInfo.name = pCmd->name();
			cfgInfo.min = pCmd->minValue();
			cfgInfo.max = pCmd->maxValue();
			cfgInfo.value = pCmd->value();
			cfgInfo.high_addr = pCmd->highAddr();
			cfgInfo.middle_addr = pCmd->middleAddr();
			cfgInfo.low_addr = pCmd->lowAddr();
			vecCfg.push_back(cfgInfo);
		}
		mapCfg1[itr->first] = vecCfg;
	}
	return mapCfg1;
}

map<string, vector<CeleX5::CfgInfo>> CeleX5::getCeleX5CfgModified()
{
	return m_mapCfgModified;
}

void CeleX5::writeRegister(int16_t addressH, int16_t addressM, int16_t addressL, uint32_t value)
{
	if (addressL == -1)
	{
		wireIn(addressH, value, 0xFF);
	}
	else
	{
		if (addressM == -1)
		{
			uint32_t valueH = value >> 8;
			uint32_t valueL = 0xFF & value;
			wireIn(addressH, valueH, 0xFF);
			wireIn(addressL, valueL, 0xFF);
		}
	}
}

CeleX5::CfgInfo CeleX5::getCfgInfoByName(string csrType, string name, bool bDefault)
{
	map<string, vector<CfgInfo>> mapCfg;
	if (bDefault)
		mapCfg = m_mapCfgDefaults;
	else
		mapCfg = m_mapCfgModified;
	CeleX5::CfgInfo cfgInfo;
	for (auto itr = mapCfg.begin(); itr != mapCfg.end(); itr++)
	{
		string tapName = itr->first;
		if (csrType == tapName)
		{
			vector<CfgInfo> vecCfg = itr->second;
			int index = 0;
			for (auto itr1 = vecCfg.begin(); itr1 != vecCfg.end(); itr1++)
			{
				if ((*itr1).name == name)
				{
					cfgInfo = (*itr1);
					return cfgInfo;
				}
				index++;
			}
			break;
		}
	}
	return cfgInfo;
}

void CeleX5::writeCSRDefaults(string csrType)
{
	//cout << "CeleX5::writeCSRDefaults: " << csrType << endl;
	m_mapCfgModified[csrType] = m_mapCfgDefaults[csrType];
	for (auto itr = m_mapCfgDefaults.begin(); itr != m_mapCfgDefaults.end(); itr++)
	{
		//cout << "group name: " << itr->first << endl;
		string tapName = itr->first;
		if (csrType == tapName)
		{
			vector<CeleX5::CfgInfo> vecCfg = itr->second;
			for (auto itr1 = vecCfg.begin(); itr1 != vecCfg.end(); itr1++)
			{
				CeleX5::CfgInfo cfgInfo = (*itr1);
				writeRegister(cfgInfo);
			}
			break;
		}
	}
}

void CeleX5::modifyCSRParameter(string csrType, string cmdName, uint32_t value)
{
	CeleX5::CfgInfo cfgInfo;
	for (auto itr = m_mapCfgModified.begin(); itr != m_mapCfgModified.end(); itr++)
	{
		string tapName = itr->first;
		if (csrType.empty())
		{
			vector<CfgInfo> vecCfg = itr->second;
			int index = 0;
			for (auto itr1 = vecCfg.begin(); itr1 != vecCfg.end(); itr1++)
			{
				if ((*itr1).name == cmdName)
				{
					cfgInfo = (*itr1);
					cout << "CeleX5::modifyCSRParameter: Old value = " << cfgInfo.value << endl;
					//modify the value in m_pMapCfgModified
					cfgInfo.value = value;
					vecCfg[index] = cfgInfo;
					m_mapCfgModified[tapName] = vecCfg;
					cout << "CeleX5::modifyCSRParameter: New value = " << cfgInfo.value << endl;
					break;
				}
				index++;
			}
		}
		else
		{
			if (csrType == tapName)
			{
				vector<CfgInfo> vecCfg = itr->second;
				int index = 0;
				for (auto itr1 = vecCfg.begin(); itr1 != vecCfg.end(); itr1++)
				{
					if ((*itr1).name == cmdName)
					{
						cfgInfo = (*itr1);
						cout << "CeleX5::modifyCSRParameter: Old value = " << cfgInfo.value << endl;
						//modify the value in m_pMapCfgModified
						cfgInfo.value = value;
						vecCfg[index] = cfgInfo;
						m_mapCfgModified[tapName] = vecCfg;
						cout << "CeleX5::modifyCSRParameter: New value = " << cfgInfo.value << endl;
						break;
					}
					index++;
				}
				break;
			}
		}
	}
}

bool CeleX5::saveXMLFile()
{
	cout << "CeleX5::saveXMLFile" << endl;
	return m_pSequenceMgr->saveCeleX5XML(m_mapCfgModified);
}

bool CeleX5::initializeFPGA()
{
#ifdef _ARM_ZYNQ_
	return true;
#else
    return m_pFrontPanel->initializeFPGA("celex5_top.bit");
#endif
}

bool CeleX5::configureSettings(CeleX5::DeviceType type)
{
	if (CeleX5::CeleX5_OpalKelly == type)
	{
		//--------------- Step1 ---------------
		wireIn(94, 1, 0xFF);

		//--------------- Step2 ---------------
		//Disable PLL
		wireIn(150, 0, 0xFF);
		//Load PLL Parameters
		writeCSRDefaults("PLL_Parameters");
		//Enable PLL
		wireIn(150, 1, 0xFF);

		//--------------- Step3 ---------------
		enterCFGMode();

		//Load Other Parameters
		wireIn(92, 12, 0xFF); //

		//Load Sensor Core Parameters
		wireIn(220, 0, 0xFF); //AUTOISP_PROFILE_ADDR
		writeCSRDefaults("Sensor_Core_Parameters");

		//wireIn(91, 11, 0xFF);
		//wireIn(18, 151, 0xFF); //23

		//Load Sensor_Operation_Mode_Control_Parameters
		wireIn(53, 0, 0xFF); //SENSOR_MODE_1
		wireIn(54, 3, 0xFF); //SENSOR_MODE_2
		wireIn(55, 4, 0xFF); //SENSOR_MODE_3

		wireIn(57, 60, 0xFF); //EVENT_DURATION, low byte
		wireIn(58, 0, 0xFF); //EVENT_DURATION, high byte

		wireIn(64, 0, 0xFF); //SENSOR_MODE_SELECT, fixed mode

		enterStartMode();
	}
	else if (CeleX5::CeleX5_MIPI == type)
	{
		setALSEnabled(false);
#ifndef _ARM_ZYNQ_
		if (m_pCeleDriver)
			m_pCeleDriver->openStream();
#endif
		//--------------- Step1 ---------------
		wireIn(94, 0, 0xFF); //PADDR_EN

		//--------------- Step2: Load PLL Parameters ---------------
		//Disable PLL
		cout << "--- Disable PLL ---" << endl;
		wireIn(150, 0, 0xFF); //PLL_PD_B
		//Load PLL Parameters
		cout << endl << "--- Load PLL Parameters ---" << endl;
		writeCSRDefaults("PLL_Parameters");
		//Enable PLL
		cout << "--- Enable PLL ---" << endl;
		wireIn(150, 1, 0xFF); //PLL_PD_B

		//--------------- Step3: Load MIPI Parameters ---------------
		cout << endl << "--- Disable MIPI ---" << endl;
		disableMIPI();

		cout << endl << "--- Load MIPI Parameters ---" << endl;
		writeCSRDefaults("MIPI_Parameters");
		writeRegister(115, -1, 114, 120); //MIPI_PLL_DIV_N

		//Enable MIPI
		cout << endl << "--- Enable MIPI ---" << endl;
		enableMIPI();

		//--------------- Step4: ---------------
		cout << endl << "--- Enter CFG Mode ---" << endl;
		enterCFGMode();

		//----- Load Sensor Core Parameters -----
		wireIn(220, 0, 0xFF); //AUTOISP_PROFILE_ADDR
		writeCSRDefaults("Sensor_Core_Parameters");
		//if (m_bAutoISPEnabled)
		{
			//--------------- for auto isp ---------------
			wireIn(220, 3, 0xFF); //AUTOISP_PROFILE_ADDR
			//Load Sensor Core Parameters
			writeCSRDefaults("Sensor_Core_Parameters");
			writeRegister(22, -1, 23, m_arrayBrightness[3]);

			wireIn(220, 2, 0xFF); //AUTOISP_PROFILE_ADDR
			 //Load Sensor Core Parameters
			writeCSRDefaults("Sensor_Core_Parameters");
			writeRegister(22, -1, 23, m_arrayBrightness[2]);

			wireIn(220, 1, 0xFF); //AUTOISP_PROFILE_ADDR
			//Load Sensor Core Parameters
			writeCSRDefaults("Sensor_Core_Parameters");
			writeRegister(22, -1, 23, m_arrayBrightness[1]);

			wireIn(220, 0, 0xFF); //AUTOISP_PROFILE_ADDR
			//Load Sensor Core Parameters
			writeCSRDefaults("Sensor_Core_Parameters");
			writeRegister(22, -1, 23, 120);

			wireIn(221, 0, 0xFF); //AUTOISP_BRT_EN, disable auto ISP
			wireIn(222, 0, 0xFF); //AUTOISP_TEM_EN
			wireIn(223, 0, 0xFF); //AUTOISP_TRIGGER

			writeRegister(225, -1, 224, m_uiAutoISPRefreshTime); //AUTOISP_REFRESH_TIME

			writeRegister(235, -1, 234, m_arrayISPThreshold[0]); //AUTOISP_BRT_THRES1
			writeRegister(237, -1, 236, m_arrayISPThreshold[1]); //AUTOISP_BRT_THRES2
			writeRegister(239, -1, 238, m_arrayISPThreshold[2]); //AUTOISP_BRT_THRES3

			writeRegister(233, -1, 232, 1500); //AUTOISP_BRT_VALUE
		}
		writeCSRDefaults("Sensor_Operation_Mode_Control_Parameters");

		writeCSRDefaults("Sensor_Data_Transfer_Parameters");
		wireIn(73, m_iEventDataFormat, 0xFF); //EVENT_PACKET_SELECT
		m_pDataProcessThread->getDataProcessor5()->setMIPIDataFormat(m_iEventDataFormat);

		cout << endl << "--- Enter Start Mode ---" << endl;
		enterStartMode();
	}
	return true;
}

bool CeleX5::resetConfigureSettings(DeviceType type)
{
	if (CeleX5::CeleX5_OpalKelly == type)
	{
		//--------------- Step1 ---------------
		wireIn(94, 1, 0xFF);

		//--------------- Step2 ---------------
		//Disable PLL
		wireIn(150, 0, 0xFF);
		//Load PLL Parameters
		writeCSRDefaults("PLL_Parameters");
		//Enable PLL
		wireIn(150, 1, 0xFF);

		//--------------- Step3 ---------------
		enterCFGMode();

		//Load Other Parameters
		wireIn(92, 12, 0xFF);
		wireIn(220, 0, 0xFF);
		//Load Sensor Core Parameters
		writeCSRDefaults("Sensor_Core_Parameters");

		wireIn(91, 11, 0xFF);
		wireIn(18, 151, 0xFF);

		wireIn(64, 0, 0xFF);
		//sensor mode
		wireIn(53, 2, 0xFF);
		wireIn(54, 2, 0xFF);
		wireIn(55, 2, 0xFF);

		wireIn(57, 60, 0xFF);
		wireIn(58, 0, 0xFF);

		enterStartMode();
	}
	else if (CeleX5::CeleX5_MIPI == type)
	{
		//cout << endl << "--- Enter CFG Mode ---" << endl;
		enterCFGMode();
#ifdef _WIN32
		Sleep(10);
#else
		usleep(1000 * 10);
#endif
		//Enter Start Mode
		//cout << endl << "--- Enter Start Mode ---" << endl;
		enterStartMode();
	}
	return true;
}

void CeleX5::wireIn(uint32_t address, uint32_t value, uint32_t mask)
{
	if (CeleX5::CeleX5_MIPI == m_emDeviceType)
	{
#ifndef _ARM_ZYNQ_
		if (m_pCeleDriver)
		{
			if (isAutoISPEnabled())
			{
				setALSEnabled(false);
#ifdef _WIN32
				Sleep(2);
#else
				usleep(1000 * 2);
#endif
			}
			if (m_pCeleDriver->i2c_set(address, value))
			{
				//cout << "CeleX5::wireIn(i2c_set): address = " << address << ", value = " << value << endl;
			}
			if (isAutoISPEnabled())
			{
				setALSEnabled(true);
			}
		}
#endif
	}
	else if (CeleX5::CeleX5_OpalKelly == m_emDeviceType)
	{
		//DAC_APPLY_OFF
		m_pFrontPanel->wireIn(0x05, 0x00, 0x02);
		//<command name = "set SPI_XXX to #value#">
		m_pFrontPanel->wireIn(0x04, value, 0x00FF); //0x03FF
													//<command name="set DAC_CHANNEL to #fixed value#">
		m_pFrontPanel->wireIn(0x04, (0x800 + address) << 8, 0xFFF00); //0xF000
		cout << "CeleX5::wireIn: address = " << address << ", value = " << value << endl;

		//DAC_APPLY_ON
		m_pFrontPanel->wireIn(0x05, 0x02, 0x02);

		//wire out the  data that have written
		//uint32_t valueOut;
		//m_pFrontPanel->wireOut(0x20, 0xFFFF0000, &valueOut);
		//cout << hex << "wireout value = " << valueOut << endl;
	}
	else if (CeleX5::CeleX5_ZYNQ == m_emDeviceType)
	{
#ifdef _ARM_ZYNQ_
        camera_register_buffer buffer;
        buffer.address = address;
        buffer.value   = value;
        buffer.mask    = mask;
        if (ioctl(m_iFileDescriptor, CAMERA_SET_REGISTER, &buffer) == 0)
        {
            cout << "CeleX5::wireIn: address = " << address << ", value = " << value << endl;
        }
#endif // _ARM_ZYNQ_
    }
}

void CeleX5::writeRegister(CfgInfo cfgInfo)
{
	if (cfgInfo.low_addr == -1)
	{
		wireIn(cfgInfo.high_addr, cfgInfo.value, 0xFF);
	}
	else
	{
		if (cfgInfo.middle_addr == -1)
		{
			uint32_t valueH = cfgInfo.value >> 8;
			uint32_t valueL = 0xFF & cfgInfo.value;
			wireIn(cfgInfo.high_addr, valueH, 0xFF);
			wireIn(cfgInfo.low_addr, valueL, 0xFF);
		}
	}
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
#ifdef _ARM_ZYNQ_

#else
	m_pFrontPanel->wireIn(address, value, mask);
#endif
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
	m_pDataProcessThread->setPlayback();
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

bool CeleX5::readPlayBackData(long length)
{
	//cout << __FUNCTION__ << endl;
	bool eof = false;
	int maxLen = 128 * MAX_PAGE_COUNT;
	int lenToRead = length > maxLen ? maxLen : length;

	if (NULL == m_pReadBuffer)
		m_pReadBuffer = new unsigned char[maxLen];

	while (true && m_pDataProcessThread->queueSize() < 1000000)
	{
		m_ifstreamPlayback.read((char*)m_pReadBuffer, lenToRead);

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

bool CeleX5::readBinFileData()
{
	//cout << __FUNCTION__ << endl;
	bool eof = false;
	uint32_t lenToRead = 0;
	m_ifstreamPlayback.read((char*)&lenToRead, 4);
	if (NULL == m_pDataToRead)
		m_pDataToRead = new uint8_t[2048000];
	m_ifstreamPlayback.read((char*)m_pDataToRead, lenToRead);
	//cout << "lenToRead = " << lenToRead << endl;
	//
	int dataLen = m_ifstreamPlayback.gcount();
	if (dataLen > 0)
	{
		uint8_t* dataIn = new uint8_t[dataLen];
		memcpy(dataIn, (uint8_t*)m_pDataToRead, dataLen);
		m_pDataProcessThread->addData(dataIn, dataLen);
		if (!m_bFirstReadFinished)
		{
			m_uiPackageCount += 1;
			if (m_uiPackageCount == 1)
			{
				m_uiPackageCountList[0] = sizeof(BinFileAttributes);
				m_uiPackageCountList[1] = m_uiPackageCountList[0] + lenToRead + 4;
				//cout << "--------------- 0" << m_uiPackageCountList[0] << endl;
				//cout << "--------------- 1" << m_uiPackageCountList[1] << endl;
			}
			else
			{
				m_uiPackageCountList[m_uiPackageCount] = m_uiPackageCountList[m_uiPackageCount-1] + lenToRead + 4;
				//cout << "--------------- i" << m_uiPackageCountList[m_uiPackageCount] << endl;
			}
		}
		cout << "package_count = " << m_uiPackageCount << endl;
	}
	if (m_ifstreamPlayback.eof())
	{
		eof = true;
		m_bFirstReadFinished = true;
		//m_ifstreamPlayback.close();
		cout << "Read Playback file Finished!" << endl;
	}
	return eof;
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
	enterCFGMode();
	if (enabled)
		writeRegister(183, -1, -1, 1);
	else
		writeRegister(183, -1, -1, 0);
	enterStartMode();
}

void CeleX5::enterTemperature(string strT)
{
	m_pDataProcessThread->getDataProcessor5()->enterTemperature(strT);
}

void CeleX5::setAutoISPEnabled(bool enable)
{
	m_bAutoISPEnabled = enable;
	if (enable)
	{
		enterCFGMode();

		wireIn(221, 1, 0xFF); //AUTOISP_BRT_EN, enable auto ISP
		if (isLoopModeEnabled())
			wireIn(223, 1, 0xFF); //AUTOISP_TRIGGER
		else
			wireIn(223, 0, 0xFF); //AUTOISP_TRIGGER

		wireIn(220, 0, 0xFF); //AUTOISP_PROFILE_ADDR, Write core parameters to profile0
		writeRegister(233, -1, 232, 1500); //AUTOISP_BRT_VALUE, Set initial brightness value 1500
		writeRegister(22, -1, 23, 80); //BIAS_BRT_I, Override the brightness value in profile0, avoid conflict with AUTOISP profile0

		enterStartMode();

		setALSEnabled(true);
	}
	else
	{
		setALSEnabled(false); //Disable ALS read and write

		enterCFGMode();

		//Disable brightness adjustment (auto isp), always load sensor core parameters from profile0
		wireIn(221, 0, 0xFF); //AUTOISP_BRT_EN, disable auto ISP
		if (isLoopModeEnabled())
			wireIn(223, 1, 0xFF); //AUTOISP_TRIGGER
		else
			wireIn(223, 0, 0xFF); //AUTOISP_TRIGGER

		wireIn(220, 0, 0xFF); //AUTOISP_PROFILE_ADDR, Write core parameters to profile0
		writeRegister(233, -1, 232, 1500); //AUTOISP_BRT_VALUE, Set initial brightness value 1500
		writeRegister(22, -1, 23, 140); //BIAS_BRT_I, Override the brightness value in profile0, avoid conflict with AUTOISP profile0
		enterStartMode();
	}
}

// This function is only for MIPI
bool CeleX5::isAutoISPEnabled()
{
	return m_bAutoISPEnabled;
}

// This function is only for MIPI
void CeleX5::setALSEnabled(bool enable)
{
    if (CeleX5::CeleX5_MIPI == m_emDeviceType)
	{
#ifndef _ARM_ZYNQ_
	if (enable)
		m_pCeleDriver->i2c_set(254, 0);
	else
		m_pCeleDriver->i2c_set(254, 2);
#endif
	}
}

void CeleX5::setISPThreshold(uint32_t value, int num)
{
	m_arrayISPThreshold[num - 1] = value;
	if (num == 1)
		writeRegister(235, -1, 234, m_arrayISPThreshold[0]); //AUTOISP_BRT_THRES1
	else if (num == 2)
		writeRegister(237, -1, 236, m_arrayISPThreshold[1]); //AUTOISP_BRT_THRES2
	else if (num == 3)
		writeRegister(239, -1, 238, m_arrayISPThreshold[2]); //AUTOISP_BRT_THRES3
}

void CeleX5::setISPBrightness(uint32_t value, int num)
{
	m_arrayBrightness[num - 1] = value;
	wireIn(220, num - 1, 0xFF); //AUTOISP_PROFILE_ADDR
	writeRegister(22, -1, 23, m_arrayBrightness[num - 1]);
}

void CeleX5::beginSaveFullPic(std::string filename)
{
	m_pDataProcessThread->getDataProcessor5()->beginSaveFullPic(filename);
}

void CeleX5::setRowCycleCount(int count)
{
	m_pDataProcessThread->getDataProcessor5()->setRowCycleCount(count);
}

//Enter CFG Mode
void CeleX5::enterCFGMode()
{
	wireIn(93, 0, 0xFF);
	wireIn(90, 1, 0xFF);
}

//Enter Start Mode
void CeleX5::enterStartMode()
{
	wireIn(90, 0, 0xFF);
	wireIn(93, 1, 0xFF);
}

void CeleX5::disableMIPI()
{
	wireIn(139, 0, 0xFF);
	wireIn(140, 0, 0xFF);
	wireIn(141, 0, 0xFF);
	wireIn(142, 0, 0xFF);
	wireIn(143, 0, 0xFF);
	wireIn(144, 0, 0xFF);
}

void CeleX5::enableMIPI()
{
	wireIn(139, 1, 0xFF);
	wireIn(140, 1, 0xFF);
	wireIn(141, 1, 0xFF);
	wireIn(142, 1, 0xFF);
	wireIn(143, 1, 0xFF);
	wireIn(144, 1, 0xFF);
}

void CeleX5::clearData()
{
    m_pDataProcessThread->clearData();
	if (CeleX5::CeleX5_MIPI == m_emDeviceType)
	{
#ifndef _ARM_ZYNQ_
		m_pCeleDriver->clearData();
#endif
	}
}
